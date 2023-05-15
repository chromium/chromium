PRAGMA foreign_keys=OFF;

BEGIN TRANSACTION;

CREATE TABLE quota(host TEXT NOT NULL, type INTEGER NOT NULL, quota INTEGER NOT NULL, PRIMARY KEY(host, type)) WITHOUT ROWID;

CREATE TABLE buckets(id INTEGER PRIMARY KEY, storage_key TEXT NOT NULL, host TEXT NOT NULL, type INTEGER NOT NULL, name TEXT NOT NULL, use_count INTEGER NOT NULL, last_accessed INTEGER NOT NULL, last_modified INTEGER NOT NULL, expiration INTEGER NOT NULL, quota INTEGER NOT NULL);

CREATE TABLE meta(key LONGVARCHAR NOT NULL UNIQUE PRIMARY KEY, value LONGVARCHAR);

INSERT INTO meta VALUES ('mmap_status', '-1');
INSERT INTO meta VALUES ('last_compatible_version', '8');
INSERT INTO meta VALUES ('version', '8');

INSERT INTO quota(host, type, quota)  VALUES('a.com', 0, 0);
INSERT INTO quota(host, type, quota)  VALUES('b.com', 0, 1);
INSERT INTO quota(host, type, quota)  VALUES('c.com', 1, 123);

INSERT INTO buckets(storage_key, host, type, name, use_count, last_accessed, last_modified, expiration, quota)
  VALUES('http://a/', 'http://a/', 0, 'bucket_a', 123, 13260644621105493, 13242931862595604, 9223372036854775807, 0);
INSERT INTO buckets(storage_key, host, type, name, use_count, last_accessed, last_modified, expiration, quota)
  VALUES('http://b/', 'http://b/', 0, 'bucket_b', 111, 13250042735631065, 13260999511438890, 9223372036854775807, 1000);
INSERT INTO buckets(storage_key, host, type, name, use_count, last_accessed, last_modified, expiration, quota)
  VALUES('chrome-extension://abc/', 'chrome-extension://abc/', 2, 'default', 321, 13261163582572088, 13261079941303629, 9223372036854775807, 10000);

CREATE UNIQUE INDEX buckets_by_storage_key ON buckets(storage_key, type, name);

CREATE INDEX buckets_by_host ON buckets(host, type);

CREATE INDEX buckets_by_last_accessed ON buckets(type, last_accessed);

CREATE INDEX buckets_by_last_modified ON buckets(type, last_modified);

CREATE INDEX buckets_by_expiration ON buckets(expiration);

COMMIT;
