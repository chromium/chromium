PRAGMA foreign_keys=OFF;

BEGIN TRANSACTION;

CREATE TABLE impressions(impression_id INTEGER PRIMARY KEY,impression_data TEXT NOT NULL,impression_origin TEXT NOT NULL,conversion_origin TEXT NOT NULL,reporting_origin TEXT NOT NULL,impression_time INTEGER NOT NULL,expiry_time INTEGER NOT NULL,num_conversions INTEGER DEFAULT 0,active INTEGER DEFAULT 1,conversion_destination TEXT NOT NULL);

INSERT INTO impressions
VALUES(1,
       '9357e17751666f64',
       'https://impression.test',
       'https://sub.conversion.test',
       'https://report.test',
       13245278349693988,
       13247870349693988,
       0,
       1,
       'https://conversion.test/');

CREATE TABLE conversions(conversion_id INTEGER PRIMARY KEY,impression_id INTEGER,conversion_data TEXT NOT NULL,conversion_time INTEGER NOT NULL,report_time INTEGER NOT NULL,attribution_credit INTEGER NOT NULL);

CREATE TABLE meta(key LONGVARCHAR NOT NULL UNIQUE PRIMARY KEY, value LONGVARCHAR);

INSERT INTO meta VALUES('mmap_status', '-1');
INSERT INTO meta VALUES('last_compatible_version', '2');
INSERT INTO meta VALUES('version', '2');

CREATE INDEX impression_expiry_idx ON impressions(expiry_time);

CREATE INDEX impression_origin_idx ON impressions(impression_origin);

CREATE INDEX conversion_report_idx ON conversions(report_time);

CREATE INDEX conversion_impression_id_idx ON conversions(impression_id);

CREATE INDEX conversion_destination_idx ON impressions(active,conversion_destination,reporting_origin);

COMMIT;
