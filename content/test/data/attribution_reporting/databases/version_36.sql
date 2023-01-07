PRAGMA foreign_keys=OFF;

BEGIN TRANSACTION;

CREATE TABLE sources(source_id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,source_event_id INTEGER NOT NULL,source_origin TEXT NOT NULL,destination_origin TEXT NOT NULL,reporting_origin TEXT NOT NULL,source_time INTEGER NOT NULL,expiry_time INTEGER NOT NULL,num_attributions INTEGER NOT NULL,event_level_active INTEGER NOT NULL,aggregatable_active INTEGER NOT NULL,destination_site TEXT NOT NULL,source_type INTEGER NOT NULL,attribution_logic INTEGER NOT NULL,priority INTEGER NOT NULL,source_site TEXT NOT NULL,debug_key INTEGER,aggregatable_budget_consumed INTEGER NOT NULL,aggregatable_source BLOB NOT NULL,filter_data BLOB NOT NULL);

CREATE TABLE event_level_reports(report_id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,source_id INTEGER NOT NULL,trigger_data INTEGER NOT NULL,trigger_time INTEGER NOT NULL,report_time INTEGER NOT NULL,priority INTEGER NOT NULL,failed_send_attempts INTEGER NOT NULL,external_report_id TEXT NOT NULL,debug_key INTEGER);

CREATE TABLE rate_limits(id INTEGER PRIMARY KEY NOT NULL,scope INTEGER NOT NULL,source_id INTEGER NOT NULL,source_site TEXT NOT NULL,source_origin TEXT NOT NULL,destination_site TEXT NOT NULL,destination_origin TEXT NOT NULL,reporting_origin TEXT NOT NULL,time INTEGER NOT NULL,expiry_time INTEGER NOT NULL);

CREATE TABLE dedup_keys(source_id INTEGER NOT NULL,dedup_key INTEGER NOT NULL,PRIMARY KEY(source_id,dedup_key))WITHOUT ROWID;

CREATE TABLE aggregatable_report_metadata(aggregation_id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,source_id INTEGER NOT NULL,trigger_time INTEGER NOT NULL,debug_key INTEGER,external_report_id TEXT NOT NULL,report_time INTEGER NOT NULL,failed_send_attempts INTEGER NOT NULL,initial_report_time INTEGER NOT NULL);

CREATE TABLE aggregatable_contributions(contribution_id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,aggregation_id INTEGER NOT NULL,key_high_bits INTEGER NOT NULL,key_low_bits INTEGER NOT NULL,value INTEGER NOT NULL);

CREATE TABLE meta(key LONGVARCHAR NOT NULL UNIQUE PRIMARY KEY, value LONGVARCHAR);

INSERT INTO meta VALUES('mmap_status','-1');
INSERT INTO meta VALUES('version','36');
INSERT INTO meta VALUES('last_compatible_version','36');

CREATE INDEX sources_by_active_destination_site_reporting_origin ON sources(event_level_active,aggregatable_active,destination_site,reporting_origin);

CREATE INDEX sources_by_expiry_time ON sources(expiry_time);

CREATE INDEX active_sources_by_source_origin ON sources(source_origin)WHERE event_level_active=1 OR aggregatable_active=1;

CREATE INDEX active_unattributed_sources_by_site_reporting_origin ON sources(source_site,reporting_origin)WHERE event_level_active=1 AND num_attributions=0 AND aggregatable_active=1 AND aggregatable_budget_consumed=0;

CREATE INDEX event_level_reports_by_report_time ON event_level_reports(report_time);

CREATE INDEX event_level_reports_by_source_id ON event_level_reports(source_id);

CREATE INDEX rate_limit_source_site_reporting_origin_idx ON rate_limits(scope,source_site,reporting_origin);

CREATE INDEX rate_limit_reporting_origin_idx ON rate_limits(scope,destination_site,source_site);

CREATE INDEX rate_limit_time_idx ON rate_limits(time);

CREATE INDEX rate_limit_source_id_idx ON rate_limits(source_id);

CREATE INDEX aggregate_source_id_idx ON aggregatable_report_metadata(source_id);

CREATE INDEX aggregate_trigger_time_idx ON aggregatable_report_metadata(trigger_time);

CREATE INDEX aggregate_report_time_idx ON aggregatable_report_metadata(report_time);

CREATE INDEX contribution_aggregation_id_idx ON aggregatable_contributions(aggregation_id);

INSERT INTO dedup_keys VALUES(1,2);

COMMIT;
