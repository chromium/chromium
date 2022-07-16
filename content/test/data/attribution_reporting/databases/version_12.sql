PRAGMA foreign_keys=OFF;

BEGIN TRANSACTION;

CREATE TABLE impressions(impression_id INTEGER PRIMARY KEY,impression_data INTEGER NOT NULL,impression_origin TEXT NOT NULL,conversion_origin TEXT NOT NULL,reporting_origin TEXT NOT NULL,impression_time INTEGER NOT NULL,expiry_time INTEGER NOT NULL,num_conversions INTEGER DEFAULT 0,active INTEGER DEFAULT 1,conversion_destination TEXT NOT NULL,source_type INTEGER NOT NULL,attributed_truthfully INTEGER NOT NULL,priority INTEGER NOT NULL,impression_site TEXT NOT NULL);

CREATE TABLE conversions(conversion_id INTEGER PRIMARY KEY,impression_id INTEGER NOT NULL,conversion_data INTEGER NOT NULL,conversion_time INTEGER NOT NULL,report_time INTEGER NOT NULL,priority INTEGER NOT NULL);

CREATE TABLE rate_limits(rate_limit_id INTEGER PRIMARY KEY NOT NULL,attribution_type INTEGER NOT NULL,impression_id INTEGER NOT NULL,impression_site TEXT NOT NULL,impression_origin TEXT NOT NULL,conversion_destination TEXT NOT NULL,conversion_origin TEXT NOT NULL,conversion_time INTEGER NOT NULL,bucket TEXT NOT NULL,value INTEGER NOT NULL);

CREATE TABLE dedup_keys(impression_id INTEGER NOT NULL,dedup_key INTEGER NOT NULL,PRIMARY KEY(impression_id,dedup_key))WITHOUT ROWID;

CREATE TABLE meta(key LONGVARCHAR NOT NULL UNIQUE PRIMARY KEY, value LONGVARCHAR);

INSERT INTO meta VALUES('mmap_status','-1');
INSERT INTO meta VALUES('version','12');
INSERT INTO meta VALUES('last_compatible_version','12');

CREATE INDEX conversion_destination_idx ON impressions(active,conversion_destination,reporting_origin);

CREATE INDEX impression_expiry_idx ON impressions(expiry_time);

CREATE INDEX impression_origin_idx ON impressions(impression_origin);

CREATE INDEX event_source_impression_site_idx ON impressions(impression_site)WHERE active = 1 AND num_conversions = 0 AND source_type = 1;

CREATE INDEX conversion_report_idx ON conversions(report_time);

CREATE INDEX conversion_impression_id_idx ON conversions(impression_id);

CREATE INDEX rate_limit_impression_site_type_idx ON rate_limits(attribution_type,conversion_destination,impression_site,conversion_time);

CREATE INDEX rate_limit_attribution_type_conversion_time_idx ON rate_limits(attribution_type,conversion_time);

CREATE INDEX rate_limit_impression_id_idx ON rate_limits(impression_id);

INSERT INTO impressions VALUES (1,2,'a','b','c',3,4,5,6,'d',7,8,9,'e');
INSERT INTO conversions VALUES (10,11,12,13,14,15);

COMMIT;
