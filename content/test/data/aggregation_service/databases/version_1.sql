PRAGMA foreign_keys=OFF;

BEGIN TRANSACTION;

-- For the schemas to be identical, extra whitespace is needed to match the code
CREATE TABLE urls(    url_id INTEGER PRIMARY KEY NOT NULL,    url TEXT NOT NULL,    fetch_time INTEGER NOT NULL,    expiry_time INTEGER NOT NULL);

CREATE UNIQUE INDEX urls_by_url_idx     ON urls(url);
CREATE INDEX fetch_time_idx ON urls(fetch_time);
CREATE INDEX expiry_time_idx ON urls(expiry_time);

CREATE TABLE keys(    url_id INTEGER NOT NULL,    key_id TEXT NOT NULL,    key BLOB NOT NULL,    PRIMARY KEY(url_id, key_id)) WITHOUT ROWID;

CREATE TABLE meta(key LONGVARCHAR NOT NULL UNIQUE PRIMARY KEY, value LONGVARCHAR);

INSERT INTO meta VALUES('mmap_status','-1');
INSERT INTO meta VALUES('version','1');
INSERT INTO meta VALUES('last_compatible_version','1');

INSERT INTO urls(url, fetch_time, expiry_time) VALUES ('https://url.example/path',13306020080123456,13306624880123456);

INSERT INTO keys(url_id, key_id, key) VALUES (1,'example-key-id',x'1632cfa71abbbc7f8784371967830757005b55c4400160618a07f28f0086bd05');

COMMIT;
