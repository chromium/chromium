PRAGMA foreign_keys=OFF;

BEGIN TRANSACTION;

-- The FirstPartySetsDatabase can make no assumptions about what tables exist,
-- *except* that the meta table exists, since any other tables may have been
-- removed in "newer" versions of the schema.

CREATE TABLE meta(key LONGVARCHAR NOT NULL UNIQUE PRIMARY KEY, value LONGVARCHAR);

INSERT INTO meta VALUES('version','30');
INSERT INTO meta VALUES('last_compatible_version','30');
INSERT INTO meta VALUES('run_count','2');

COMMIT;
