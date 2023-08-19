PRAGMA foreign_keys=OFF;

BEGIN TRANSACTION;

CREATE TABLE browsing_topics_api_usages (
hashed_context_domain INTEGER NOT NULL,
hashed_main_frame_host INTEGER NOT NULL,
last_usage_time INTEGER NOT NULL,
PRIMARY KEY (hashed_context_domain, hashed_main_frame_host));

CREATE INDEX last_usage_time_idx ON browsing_topics_api_usages(last_usage_time);

CREATE TABLE meta(key LONGVARCHAR NOT NULL UNIQUE PRIMARY KEY, value LONGVARCHAR);

INSERT INTO meta VALUES('version','2');
INSERT INTO meta VALUES('last_compatible_version','1');

INSERT INTO browsing_topics_api_usages VALUES (111, 222, 333);

CREATE TABLE  browsing_topics_api_hashed_to_unhashed_domain(
hashed_context_domain INTEGER NOT NULL,
context_domain TEXT NOT NULL,
PRIMARY KEY (hashed_context_domain));

INSERT INTO browsing_topics_api_hashed_to_unhashed_domain VALUES (111, '111.com');

COMMIT;
