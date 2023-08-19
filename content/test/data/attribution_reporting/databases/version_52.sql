PRAGMA foreign_keys=OFF;

BEGIN TRANSACTION;

CREATE TABLE sources(source_id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,source_event_id INTEGER NOT NULL,source_origin TEXT NOT NULL,reporting_origin TEXT NOT NULL,source_time INTEGER NOT NULL,expiry_time INTEGER NOT NULL,event_report_window_time INTEGER NOT NULL,aggregatable_report_window_time INTEGER NOT NULL,num_attributions INTEGER NOT NULL,event_level_active INTEGER NOT NULL,aggregatable_active INTEGER NOT NULL,source_type INTEGER NOT NULL,attribution_logic INTEGER NOT NULL,priority INTEGER NOT NULL,source_site TEXT NOT NULL,debug_key INTEGER,aggregatable_budget_consumed INTEGER NOT NULL,aggregatable_source BLOB NOT NULL,filter_data BLOB NOT NULL);

CREATE TABLE reports(report_id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,source_id INTEGER NOT NULL,trigger_time INTEGER NOT NULL,report_time INTEGER NOT NULL,initial_report_time INTEGER NOT NULL,failed_send_attempts INTEGER NOT NULL,external_report_id TEXT NOT NULL,debug_key INTEGER,context_origin TEXT NOT NULL,reporting_origin TEXT NOT NULL,report_type INTEGER NOT NULL,metadata BLOB NOT NULL);

CREATE TABLE rate_limits(id INTEGER PRIMARY KEY NOT NULL,scope INTEGER NOT NULL,source_id INTEGER NOT NULL,source_site TEXT NOT NULL,destination_site TEXT NOT NULL,context_origin TEXT NOT NULL,reporting_origin TEXT NOT NULL,time INTEGER NOT NULL,source_expiry_or_attribution_time INTEGER NOT NULL);

CREATE TABLE dedup_keys(source_id INTEGER NOT NULL,report_type INTEGER NOT NULL,dedup_key INTEGER NOT NULL,PRIMARY KEY(source_id,report_type,dedup_key))WITHOUT ROWID;

CREATE TABLE source_destinations(source_id INTEGER NOT NULL,destination_site TEXT NOT NULL,PRIMARY KEY(source_id,destination_site))WITHOUT ROWID;

CREATE TABLE meta(key LONGVARCHAR NOT NULL UNIQUE PRIMARY KEY, value LONGVARCHAR);

INSERT INTO meta VALUES('mmap_status','-1');
INSERT INTO meta VALUES('version','52');
INSERT INTO meta VALUES('last_compatible_version','52');

CREATE INDEX sources_by_active_reporting_origin ON sources(event_level_active,aggregatable_active,reporting_origin);

CREATE INDEX sources_by_expiry_time ON sources(expiry_time);

CREATE INDEX active_sources_by_source_origin ON sources(source_origin)WHERE event_level_active=1 OR aggregatable_active=1;

CREATE INDEX sources_by_source_time ON sources(source_time);

CREATE INDEX sources_by_destination_site ON source_destinations(destination_site);

CREATE INDEX reports_by_report_time ON reports(report_time);

CREATE INDEX reports_by_source_id_report_type ON reports(source_id,report_type);

CREATE INDEX reports_by_trigger_time ON reports(trigger_time);

CREATE INDEX reports_by_reporting_origin ON reports(reporting_origin)WHERE report_type=2;

CREATE INDEX rate_limit_source_site_reporting_origin_idx ON rate_limits(scope,source_site,reporting_origin);

CREATE INDEX rate_limit_reporting_origin_idx ON rate_limits(scope,destination_site,source_site);

CREATE INDEX rate_limit_time_idx ON rate_limits(time);

CREATE INDEX rate_limit_source_id_idx ON rate_limits(source_id);

INSERT INTO sources VALUES
(1,2,3,4,5,6,7,8,9,10,11,12,13,14,1,16, 0,17,18),
(2,2,3,4,5,6,7,8,9,10,11,12,13,14,0,16, 200,17,18);

COMMIT;
