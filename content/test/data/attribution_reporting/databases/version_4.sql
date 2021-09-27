PRAGMA foreign_keys=OFF;

BEGIN TRANSACTION;

CREATE TABLE impressions(impression_id INTEGER PRIMARY KEY,impression_data TEXT NOT NULL,impression_origin TEXT NOT NULL,conversion_origin TEXT NOT NULL,reporting_origin TEXT NOT NULL,impression_time INTEGER NOT NULL,expiry_time INTEGER NOT NULL,num_conversions INTEGER DEFAULT 0,active INTEGER DEFAULT 1,conversion_destination TEXT NOT NULL,source_type INTEGER NOT NULL,attributed_truthfully INTEGER NOT NULL);

CREATE TABLE conversions(conversion_id INTEGER PRIMARY KEY,impression_id INTEGER,conversion_data TEXT NOT NULL,conversion_time INTEGER NOT NULL,report_time INTEGER NOT NULL,attribution_credit INTEGER NOT NULL);

CREATE TABLE rate_limits(rate_limit_id INTEGER PRIMARY KEY,attribution_type INTEGER NOT NULL,impression_id INTEGER NOT NULL,impression_site TEXT NOT NULL,impression_origin TEXT NOT NULL,conversion_destination TEXT NOT NULL,conversion_origin TEXT NOT NULL,conversion_time INTEGER NOT NULL);

CREATE TABLE meta(key LONGVARCHAR NOT NULL UNIQUE PRIMARY KEY, value LONGVARCHAR);

INSERT INTO meta VALUES('mmap_status','-1');
INSERT INTO meta VALUES('version','4');
INSERT INTO meta VALUES('last_compatible_version','3');

CREATE INDEX conversion_destination_idx ON impressions(active,conversion_destination,reporting_origin);

CREATE INDEX impression_expiry_idx ON impressions(expiry_time);

CREATE INDEX impression_origin_idx ON impressions(impression_origin);

CREATE INDEX conversion_report_idx ON conversions(report_time);

CREATE INDEX conversion_impression_id_idx ON conversions(impression_id);

CREATE INDEX rate_limit_impression_site_type_idx ON rate_limits(attribution_type,conversion_destination,impression_site,conversion_time);

CREATE INDEX rate_limit_conversion_time_idx ON rate_limits(conversion_time);

CREATE INDEX rate_limit_impression_id_idx ON rate_limits(impression_id);

-- Add some conversions to test 0-credit deletion in version 5.
INSERT INTO conversions (conversion_id, conversion_data, conversion_time, report_time, attribution_credit) VALUES
  (1, 'a', 2, 3, 5),
  (2, 'b', 7, 11, 0),
  (3, 'c', 13, 17, 19);

COMMIT;
