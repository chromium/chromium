PRAGMA foreign_keys=OFF;

BEGIN TRANSACTION;

CREATE TABLE sources(source_id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,source_event_id INTEGER NOT NULL,source_origin TEXT NOT NULL,destination_origin TEXT NOT NULL,reporting_origin TEXT NOT NULL,source_time INTEGER NOT NULL,expiry_time INTEGER NOT NULL,event_report_window_time INTEGER NOT NULL,aggregatable_report_window_time INTEGER NOT NULL,num_attributions INTEGER NOT NULL,event_level_active INTEGER NOT NULL,aggregatable_active INTEGER NOT NULL,destination_site TEXT NOT NULL,source_type INTEGER NOT NULL,attribution_logic INTEGER NOT NULL,priority INTEGER NOT NULL,source_site TEXT NOT NULL,debug_key INTEGER,aggregatable_budget_consumed INTEGER NOT NULL,aggregatable_source BLOB NOT NULL,filter_data BLOB NOT NULL);

CREATE TABLE event_level_reports(report_id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,source_id INTEGER NOT NULL,trigger_data INTEGER NOT NULL,trigger_time INTEGER NOT NULL,report_time INTEGER NOT NULL,priority INTEGER NOT NULL,failed_send_attempts INTEGER NOT NULL,external_report_id TEXT NOT NULL,debug_key INTEGER);

CREATE TABLE rate_limits(id INTEGER PRIMARY KEY NOT NULL,scope INTEGER NOT NULL,source_id INTEGER NOT NULL,source_site TEXT NOT NULL,destination_site TEXT NOT NULL,context_origin TEXT NOT NULL,reporting_origin TEXT NOT NULL,time INTEGER NOT NULL,source_expiry_or_attribution_time INTEGER NOT NULL);

CREATE TABLE dedup_keys(source_id INTEGER NOT NULL,report_type INTEGER NOT NULL,dedup_key INTEGER NOT NULL,PRIMARY KEY(source_id,report_type,dedup_key))WITHOUT ROWID;

CREATE TABLE aggregatable_report_metadata(aggregation_id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,source_id INTEGER NOT NULL,trigger_time INTEGER NOT NULL,debug_key INTEGER,external_report_id TEXT NOT NULL,report_time INTEGER NOT NULL,failed_send_attempts INTEGER NOT NULL,initial_report_time INTEGER NOT NULL,aggregation_coordinator INTEGER NOT NULL,attestation_token TEXT);

CREATE TABLE aggregatable_contributions(aggregation_id INTEGER NOT NULL,contribution_id INTEGER NOT NULL,key_high_bits INTEGER NOT NULL,key_low_bits INTEGER NOT NULL,value INTEGER NOT NULL,PRIMARY KEY(aggregation_id,contribution_id))WITHOUT ROWID;

CREATE TABLE meta(key LONGVARCHAR NOT NULL UNIQUE PRIMARY KEY, value LONGVARCHAR);

INSERT INTO meta VALUES('mmap_status','-1');
INSERT INTO meta VALUES('version','43');
INSERT INTO meta VALUES('last_compatible_version','43');

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

INSERT INTO sources VALUES (2,3,4,5,6,7,8,9,10,11,12,13,'https://d.test',15,16,17,18,19,20,21,22);
INSERT INTO event_level_reports VALUES (1,2,3,4,5,6,7,8,9);
INSERT INTO aggregatable_report_metadata VALUES (1,2,3,4,5,6,7,8,9,10);

COMMIT;
