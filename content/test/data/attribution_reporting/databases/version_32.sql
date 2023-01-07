PRAGMA foreign_keys=OFF;

BEGIN TRANSACTION;

CREATE TABLE impressions(impression_id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,impression_data INTEGER NOT NULL,impression_origin TEXT NOT NULL,conversion_origin TEXT NOT NULL,reporting_origin TEXT NOT NULL,impression_time INTEGER NOT NULL,expiry_time INTEGER NOT NULL,num_conversions INTEGER NOT NULL,event_level_active INTEGER NOT NULL,aggregatable_active INTEGER NOT NULL,conversion_destination TEXT NOT NULL,source_type INTEGER NOT NULL,attributed_truthfully INTEGER NOT NULL,priority INTEGER NOT NULL,impression_site TEXT NOT NULL,debug_key INTEGER,aggregatable_budget_consumed INTEGER NOT NULL,aggregatable_source BLOB NOT NULL,filter_data BLOB NOT NULL);

CREATE TABLE conversions(conversion_id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,impression_id INTEGER NOT NULL,conversion_data INTEGER NOT NULL,conversion_time INTEGER NOT NULL,report_time INTEGER NOT NULL,priority INTEGER NOT NULL,failed_send_attempts INTEGER NOT NULL,external_report_id TEXT NOT NULL,debug_key INTEGER);

CREATE TABLE rate_limits(rate_limit_id INTEGER PRIMARY KEY NOT NULL,scope INTEGER NOT NULL,impression_id INTEGER NOT NULL,impression_site TEXT NOT NULL,impression_origin TEXT NOT NULL,conversion_destination TEXT NOT NULL,conversion_origin TEXT NOT NULL,reporting_origin TEXT NOT NULL,time INTEGER NOT NULL);

CREATE TABLE dedup_keys(impression_id INTEGER NOT NULL,dedup_key INTEGER NOT NULL,PRIMARY KEY(impression_id,dedup_key))WITHOUT ROWID;

CREATE TABLE aggregatable_report_metadata(aggregation_id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,source_id INTEGER NOT NULL,trigger_time INTEGER NOT NULL,debug_key INTEGER,external_report_id TEXT NOT NULL,report_time INTEGER NOT NULL,failed_send_attempts INTEGER NOT NULL);

CREATE TABLE aggregatable_contributions(contribution_id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,aggregation_id INTEGER NOT NULL,key_high_bits INTEGER NOT NULL,key_low_bits INTEGER NOT NULL,value INTEGER NOT NULL);

CREATE TABLE meta(key LONGVARCHAR NOT NULL UNIQUE PRIMARY KEY, value LONGVARCHAR);

INSERT INTO meta VALUES('mmap_status','-1');
INSERT INTO meta VALUES('version','32');
INSERT INTO meta VALUES('last_compatible_version','32');

CREATE INDEX conversion_destination_idx ON impressions(event_level_active,aggregatable_active,conversion_destination,reporting_origin);

CREATE INDEX impression_expiry_idx ON impressions(expiry_time);

CREATE INDEX impression_origin_idx ON impressions(impression_origin);

CREATE INDEX impression_site_reporting_origin_idx ON impressions(impression_site,reporting_origin)WHERE event_level_active=1 AND num_conversions=0 AND aggregatable_active=1 AND aggregatable_budget_consumed=0;

CREATE INDEX conversion_report_idx ON conversions(report_time);

CREATE INDEX conversion_impression_id_idx ON conversions(impression_id);

CREATE INDEX rate_limit_attribution_idx ON rate_limits(conversion_destination,impression_site,reporting_origin,time)WHERE scope=1;

CREATE INDEX rate_limit_reporting_origin_idx ON rate_limits(scope,conversion_destination,impression_site,time);

CREATE INDEX rate_limit_time_idx ON rate_limits(time);

CREATE INDEX rate_limit_impression_id_idx ON rate_limits(impression_id);

CREATE INDEX aggregate_source_id_idx ON aggregatable_report_metadata(source_id);

CREATE INDEX aggregate_trigger_time_idx ON aggregatable_report_metadata(trigger_time);

CREATE INDEX aggregate_report_time_idx ON aggregatable_report_metadata(report_time);

CREATE INDEX contribution_aggregation_id_idx ON aggregatable_contributions(aggregation_id);

INSERT INTO conversions VALUES (1,2,3,4,5,6,7,8,9);

COMMIT;
