PRAGMA foreign_keys=OFF;

BEGIN TRANSACTION;

CREATE TABLE HostQuotaTable(host TEXT NOT NULL, type INTEGER NOT NULL, quota INTEGER DEFAULT 0, UNIQUE(host, type));

CREATE TABLE OriginInfoTable(origin TEXT NOT NULL, type INTEGER NOT NULL, used_count INTEGER DEFAULT 0, last_access_time INTEGER DEFAULT 0, last_modified_time INTEGER DEFAULT 0, unique(origin, type));

CREATE TABLE EvictionInfoTable(origin TEXT NOT NULL, type INTEGER NOT NULL, last_eviction_time INTEGER DEFAULT 0, UNIQUE(origin, type));

CREATE TABLE meta(key LONGVARCHAR NOT NULL UNIQUE PRIMARY KEY, value LONGVARCHAR);

INSERT INTO meta VALUES('mmap_status', '-1');
INSERT INTO meta VALUES('last_compatible_version', '2');
INSERT INTO meta VALUES('version', '5');

INSERT INTO HostQuotaTable(host, type, quota)  VALUES('a.com', 0, 0);
INSERT INTO HostQuotaTable(host, type, quota)  VALUES('b.com', 0, 1);
INSERT INTO HostQuotaTable(host, type, quota)  VALUES('c.com', 1, 123);

INSERT INTO OriginInfoTable(origin, type, used_count, last_access_time, last_modified_time)
  VALUES('http://a/', 0, 123, 13260644621105493, 13242931862595604);
INSERT INTO OriginInfoTable(origin, type, used_count, last_access_time, last_modified_time)
  VALUES('http://b/', 0, 111, 13250042735631065, 13260999511438890);
INSERT INTO OriginInfoTable(origin, type, used_count, last_access_time, last_modified_time)
  VALUES('http://c/', 1, 321, 13261163582572088, 13261079941303629);

CREATE INDEX HostIndex ON HostQuotaTable(host);

CREATE INDEX OriginInfoIndex ON OriginInfoTable(origin);

CREATE INDEX OriginLastAccessTimeIndex ON OriginInfoTable(last_access_time);

CREATE INDEX OriginLastModifiedTimeIndex ON OriginInfoTable(last_modified_time);

COMMIT;
