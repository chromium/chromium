PRAGMA foreign_keys=OFF;

BEGIN TRANSACTION;

CREATE TABLE impressions(impression_id INTEGER PRIMARY KEY,impression_data TEXT NOT NULL,impression_origin TEXT NOT NULL,conversion_origin TEXT NOT NULL,reporting_origin TEXT NOT NULL,impression_time INTEGER NOT NULL,expiry_time INTEGER NOT NULL,num_conversions INTEGER DEFAULT 0,active INTEGER DEFAULT 1,conversion_destination TEXT NOT NULL,source_type INTEGER NOT NULL,attributed_truthfully INTEGER NOT NULL,priority INTEGER NOT NULL,impression_site TEXT NOT NULL);

CREATE TABLE conversions(conversion_id INTEGER PRIMARY KEY,impression_id INTEGER,conversion_data TEXT NOT NULL,conversion_time INTEGER NOT NULL,report_time INTEGER NOT NULL);

CREATE TABLE rate_limits(rate_limit_id INTEGER PRIMARY KEY,attribution_type INTEGER NOT NULL,impression_id INTEGER NOT NULL,impression_site TEXT NOT NULL,impression_origin TEXT NOT NULL,conversion_destination TEXT NOT NULL,conversion_origin TEXT NOT NULL,conversion_time INTEGER NOT NULL);

CREATE TABLE meta(key LONGVARCHAR NOT NULL UNIQUE PRIMARY KEY, value LONGVARCHAR);

INSERT INTO meta VALUES('mmap_status','-1');
INSERT INTO meta VALUES('version','7');
INSERT INTO meta VALUES('last_compatible_version','7');

CREATE INDEX conversion_destination_idx ON impressions(active,conversion_destination,reporting_origin);

CREATE INDEX impression_expiry_idx ON impressions(expiry_time);

CREATE INDEX impression_origin_idx ON impressions(impression_origin);

CREATE INDEX impression_site_idx ON impressions(active,impression_site,source_type);

CREATE INDEX conversion_report_idx ON conversions(report_time);

CREATE INDEX conversion_impression_id_idx ON conversions(impression_id);

CREATE INDEX rate_limit_impression_site_type_idx ON rate_limits(attribution_type,conversion_destination,impression_site,conversion_time);

CREATE INDEX rate_limit_conversion_time_idx ON rate_limits(conversion_time);

CREATE INDEX rate_limit_impression_id_idx ON rate_limits(impression_id);

INSERT INTO impressions
VALUES (1,
       '18446744073709551615',
       'https://impression.test',
       'https://conversion.test',
       'https://report.test',
       13245278349693988,
       13247870349693988,
       0,
       1,
       'https://conversion.test/',
       0,
       1,
       3,
       'https://impression.test'),
       (2,
       'invalid',
       'https://impression2.test',
       'https://conversion2.test',
       'https://report2.test',
       13245278349693988,
       13247870349693988,
       0,
       1,
       'https://conversion2.test/',
       0,
       1,
       5,
       'https://impression2.test');

INSERT INTO conversions
VALUES (0, 29, '234', 5, 6),
       (7, NULL, 'invalid', 8, 9);

COMMIT;
