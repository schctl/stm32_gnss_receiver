/*
 * nmea.h
 *
 *  Created on: Apr 5, 2024
 *      Author: schct
 */

#ifndef INC_NMEA_H_
#define INC_NMEA_H_

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"

#include <string.h>
#include <stdlib.h>

#define _DL ","

typedef enum {
    North,
    South
} NSHemisphere;

int ns_hs_c(NSHemisphere hs) {
	if (hs == North) return 'N';
	else if (hs == South) return 'S';
	else return '?';
}

typedef enum {
    East,
    West
} EWHemisphere;

int es_hs_c(EWHemisphere hs) {
	if (hs == East) return 'E';
	else if (hs == West) return 'W';
	else return '?';
}

typedef struct {
    float degrees;
    NSHemisphere hs;
} Latitude;

typedef struct {
    float degrees;
    EWHemisphere hs;
} Longitude;

/// All units are in meters.
typedef struct {
    float horizontal;
    float vertical;
} DOP;

typedef enum {
    /// No fix available.
    FixNone,
    /// Non-differential 2D/3D fix available.
    FixAutonomous,
    /// Differential 2D/3D fix available.
    FixDifferential
} FixStatus;

typedef enum {
	FieldLat   = 0x01,
	FieldLon   = 0x02,
	FieldAlt   = 0x04,
	FieldHDOP  = 0x08,
	FieldVDOP  = 0x10,
	FieldSat   = 0x20,
	FieldFix   = 0x40
} NMEADataField;

typedef struct {
    Latitude lat;                   // GGA, RMC,    ,    ,    ,
    Longitude lon;                  // GGA, RMC,    ,    ,    ,
    float alt;                      // GGA,    , GNS,    ,    ,
    DOP dop;                        //    ,    , GNS, GSA,    ,
    unsigned int sat;               // GGA,    ,    , GSA, GSV,
    FixStatus fix;                  // GGA, RMC, GNS, GSA,    ,

    /* NMEADataField */ int updated;
} NMEAData;

typedef enum {
    GGA,
    RMC,
    GNS,
    GSA,
    GSV,
    NMEA_NF = -1,
} MessageType;

MessageType match_msg(char* sentence) {
    if      (strstr(sentence, "RMC")) return RMC;
    else if (strstr(sentence, "GGA")) return GGA;
    else if (strstr(sentence, "GNS")) return GNS;
    else if (strstr(sentence, "GSA")) return GSA;
    else if (strstr(sentence, "GSV")) return GSV;
    else return NMEA_NF;
}

void zeroize(void* buf, size_t size);

int _process_rmc(NMEAData* data);
int _process_gga(NMEAData* data);
int _process_gns(NMEAData* data);
int _process_gsa(NMEAData* data);
int _process_gsv(NMEAData* data);

/// Run a prepass on the NMEA sentence, handling empty fields so we can parse them
/// correctly with `strtok`.
void _preprocess_line(char* sentence, char* output) {
	int o = 0;

	for (int i = 0; i < strlen(sentence) - 1; i++) {
		output[o] = sentence[i];

		if (sentence[i] == ',')
			if (sentence[i] == sentence[i + 1])
				output[++o] = '?';

		o++;
	}
}

int nmea_process_line(char* sentence, NMEAData* data) {
	char p_sentence[192] = { '\0' };
	_preprocess_line(sentence, p_sentence);

    char* token = strtok(sentence, _DL);
    MessageType msg = match_msg(token);

    switch (msg)
    {
    case RMC: return _process_rmc(data);
    case GGA: return _process_gga(data);
    case GNS: return _process_gns(data);
    case GSA: return _process_gsa(data);
    case GSV: return _process_gsv(data);
    default : return -1;
    }
}

#define IF_TOK(name) char* name = strtok(NULL, _DL); if (name[0] != '?')

int _process_rmc(NMEAData* data) {
    char temp[16] = {0};

    char* _utc_time = strtok(NULL, _DL);
    char* _fix_qs = strtok(NULL, _DL);

    // latitude

    IF_TOK(lat) {
        strncpy(temp, lat, 2);
        data->lat.degrees = atof(temp);
        strncpy(temp, lat + 2, 5);
        data->lat.degrees += atof(temp) / 60.0;
        data->updated |= FieldLat;
    }
    IF_TOK(lat_hs) {
        data->lat.hs = lat_hs[0] == 'N' ? North : South;
    }

    // longitude
    zeroize(temp, 5);

    IF_TOK(lon) {
        strncpy(temp, lon, 3);
        data->lon.degrees = atof(temp);
        strncpy(temp, lon + 3, 5);
        data->lon.degrees += atof(temp) / 60.0;
        data->updated |= FieldLon;
    }
    IF_TOK(lon_hs) {
        data->lon.hs = lon_hs[0] == 'E' ? East : West;
    }

    return 0;
}

int _process_gga(NMEAData* data) {
    char temp[16] = {0};

    char* _utc_time = strtok(NULL, _DL);

    // latitude

    IF_TOK(lat) {
        strncpy(temp, lat, 2);
        data->lat.degrees = atof(temp);
        strncpy(temp, lat + 2, 5);
        data->lat.degrees += atof(temp) / 60.0;
        data->updated |= FieldLat;
    }
    IF_TOK(lat_hs) {
        data->lat.hs = lat_hs[0] == 'N' ? North : South;
    }

    // longitude
    zeroize(temp, 5);

    IF_TOK(lon) {
        strncpy(temp, lon, 3);
        data->lon.degrees = atof(temp);
        strncpy(temp, lon + 3, 5);
        data->lon.degrees += atof(temp) / 60.0;
        data->updated |= FieldLon;
    }
    IF_TOK(lon_hs) {
        data->lon.hs = lon_hs[0] == 'E' ? East : West;
    }

    // fix status
    char* fix = strtok(NULL, _DL);
    switch (fix[0]) {
    // case '0': default
    case '1': data->fix = FixAutonomous; break;
    case '2': data->fix = FixDifferential; break;
    default: data->fix  = FixNone;
    }
    data->updated |= FieldFix;

    IF_TOK(sat) {
    	data->sat = atoi(sat);
    	data->updated |= FieldSat;
    }
    IF_TOK(hdop) {
    	data->dop.horizontal = atof(hdop);
        data->updated |= FieldHDOP;
    }
    IF_TOK(alt) {
    	data->alt = atof(alt);
    	data-> updated |= FieldAlt;
    }

    return 0;
}

int _process_gns(NMEAData* data) {
    char temp[16] = {0};

    char* _utc_time = strtok(NULL, _DL);

    // latitude

    IF_TOK(lat) {
        strncpy(temp, lat, 2);
        data->lat.degrees = atof(temp);
        strncpy(temp, lat + 2, 5);
        data->lat.degrees += atof(temp) / 60.0;
        data->updated |= FieldLat;
    }
    IF_TOK(lat_hs) {
        data->lat.hs = lat_hs[0] == 'N' ? North : South;
    }

    // longitude
    zeroize(temp, 5);

    IF_TOK(lon) {
        strncpy(temp, lon, 3);
        data->lon.degrees = atof(temp);
        strncpy(temp, lon + 3, 5);
        data->lon.degrees += atof(temp) / 60.0;
        data->updated |= FieldLon;
    }
    IF_TOK(lon_hs) {
        data->lon.hs = lon_hs[0] == 'E' ? East : West;
    }

    char* _mode = strtok(NULL, _DL);

    IF_TOK(sat) {
        data->sat = atoi(sat);
        data->updated |= FieldSat;
    }
    IF_TOK(hdop) {
        data->dop.horizontal = atof(hdop);
        data->updated |= FieldHDOP;
    }
    IF_TOK(alt) {
        data->alt = atof(alt);
        data-> updated |= FieldAlt;
    }

    return 0;
}

int _process_gsa(NMEAData* data) {
    char* _sel_mode = strtok(NULL, _DL);

    char* mode = strtok(NULL, _DL);
    if (mode[0] == '2' || mode[0] == '3')
        data->fix = FixAutonomous;
    else
        data->fix = FixNone;
    data->updated |= FieldFix;

    // skip
    for (int _i = 0; _i < 13; _i++) {
        char* _ = strtok(NULL, _DL);
    }

    IF_TOK(hdop) {
        data->dop.horizontal = atof(hdop);
        data->updated |= FieldHDOP;
    }
    IF_TOK(vdop) {
        data->dop.vertical = atof(vdop);
        data->updated |= FieldVDOP;
    }

    return 0;
}

int _process_gsv(NMEAData* data) {
    char* _g = strtok(NULL, _DL);
    char* _n = strtok(NULL, _DL);

    IF_TOK(sat) {
        data->sat = atoi(sat);
        data->updated |= FieldSat;
    }

    return 0;
}

void zeroize(void* buf, size_t size) {
	memset(buf, 0, size);
}

void nmea_format(NMEAData* data, char* buffer, int max_offset) {
	int offset = 0;

	offset += snprintf(buffer + offset,
				max_offset - offset,
				"----------------\n");

	if ((data->updated & FieldLat) != 0)
	    offset += snprintf(buffer + offset,
	    		max_offset - offset,
	    		"Latitude: %.3f %c\n",
				data->lat.degrees,
				data->lat.hs == North ? 'N' : 'S');
	if ((data->updated & FieldLon) != 0)
		 offset += snprintf(buffer + offset,
				 max_offset - offset,
			    "Longitude: %.3f %c\n",
				data->lon.degrees,
				data->lon.hs == East ? 'E' : 'W');
	if ((data->updated & FieldAlt) != 0)
		offset += snprintf(buffer + offset,
				max_offset - offset,
				"Altitude: %.1f M\n", data->alt);
	if ((data->updated & FieldHDOP) != 0)
		offset += snprintf(buffer + offset,
				max_offset - offset,
				"HDOP: %.1f M\n", data->dop.horizontal);
	if ((data->updated & FieldVDOP) != 0)
		offset += snprintf(buffer + offset,
				max_offset - offset,
				"VDOP: %.1f M\n", data->dop.vertical);
	if ((data->updated & FieldSat) != 0)
		offset += snprintf(buffer + offset,
				max_offset - offset,
				"Satellites: %d\n", data->sat);
	if ((data->updated & FieldFix) != 0)
		offset += snprintf(buffer + offset,
				max_offset - offset,
				"Fix Status: %d\n", data->fix);
}

#pragma GCC diagnostic pop // -Wunused-variables
#endif /* INC_NMEA_H_ */
