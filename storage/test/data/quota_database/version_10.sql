PRAGMA foreign_keys=OFF;

BEGIN TRANSACTION;

CREATE TABLE buckets(id INTEGER PRIMARY KEY AUTOINCREMENT, storage_key TEXT NOT NULL, host TEXT NOT NULL, type INTEGER NOT NULL, name TEXT NOT NULL, use_count INTEGER NOT NULL, last_accessed INTEGER NOT NULL, last_modified INTEGER NOT NULL, expiration INTEGER NOT NULL, quota INTEGER NOT NULL, persistent INTEGER NOT NULL, durability INTEGER NOT NULL) STRICT;

CREATE TABLE meta(key LONGVARCHAR NOT NULL UNIQUE PRIMARY KEY, value LONGVARCHAR);

INSERT INTO meta VALUES ('mmap_status', '-1');
INSERT INTO meta VALUES ('last_compatible_version', '10');
INSERT INTO meta VALUES ('version', '10');

INSERT INTO buckets(storage_key, host, type, name, use_count, last_accessed, last_modified, expiration, quota, persistent, durability)
  VALUES('http://a/', 'http://a/', 0, 'bucket_a', 123, 13260644621105493, 13242931862595604, 0, 0, 0, 0);
INSERT INTO buckets(storage_key, host, type, name, use_count, last_accessed, last_modified, expiration, quota, persistent, durability)
  VALUES('http://b/', 'http://b/', 0, 'bucket_b', 111, 13250042735631065, 13260999511438890, 0, 1000, 0, 0);
INSERT INTO buckets(storage_key, host, type, name, use_count, last_accessed, last_modified, expiration, quota, persistent, durability)
  VALUES('chrome-extension://abc/', 'chrome-extension://abc/', 1, '_default', 321, 13261163582572088, 13261079941303629, 0, 10000, 0, 1);

CREATE UNIQUE INDEX buckets_by_storage_key ON buckets(storage_key, type, name);

CREATE INDEX buckets_by_host ON buckets(host, type);

CREATE INDEX buckets_by_last_accessed ON buckets(type, last_accessed);

CREATE INDEX buckets_by_last_modified ON buckets(type, last_modified);

CREATE INDEX buckets_by_expiration ON buckets(expiration);

COMMIT;
