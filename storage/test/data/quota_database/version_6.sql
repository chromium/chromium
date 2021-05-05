PRAGMA foreign_keys=OFF;

BEGIN TRANSACTION;

CREATE TABLE quota(host TEXT NOT NULL, type INTEGER NOT NULL, quota INTEGER NOT NULL, PRIMARY KEY(host, type)) WITHOUT ROWID;

CREATE TABLE buckets(id INTEGER PRIMARY KEY, origin TEXT NOT NULL, type INTEGER NOT NULL, name TEXT NOT NULL, use_count INTEGER NOT NULL, last_accessed INTEGER NOT NULL, last_modified INTEGER NOT NULL, expiration INTEGER NOT NULL, quota INTEGER NOT NULL);

CREATE TABLE eviction_info(origin TEXT NOT NULL, type INTEGER NOT NULL, last_eviction_time INTEGER NOT NULL, PRIMARY KEY(origin, type));

CREATE TABLE meta(key LONGVARCHAR NOT NULL UNIQUE PRIMARY KEY, value LONGVARCHAR);

INSERT INTO meta VALUES ('mmap_status', '-1');
INSERT INTO meta VALUES ('last_compatible_version', '2');
INSERT INTO meta VALUES ('version', '6');

CREATE UNIQUE INDEX buckets_by_storage_key ON buckets(origin, type, name);

CREATE INDEX buckets_by_last_accessed ON buckets(type, last_accessed);

CREATE INDEX buckets_by_last_modified ON buckets(type, last_modified);

CREATE INDEX buckets_by_expiration ON buckets(expiration);

COMMIT;
