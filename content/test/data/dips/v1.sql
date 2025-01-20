PRAGMA foreign_keys=OFF;

BEGIN TRANSACTION;

CREATE TABLE IF NOT EXISTS bounces(
  site TEXT PRIMARY KEY NOT NULL,
  first_site_storage_time INTEGER NOT NULL,
  last_site_storage_time INTEGER NOT NULL,
  first_user_interaction_time INTEGER NOT NULL,
  last_user_interaction_time INTEGER NOT NULL,
  first_stateful_bounce_time INTEGER NOT NULL,
  last_stateful_bounce_time INTEGER NOT NULL,
  first_stateless_bounce_time INTEGER NOT NULL,
  last_stateless_bounce_time INTEGER NOT NULL);

CREATE TABLE meta(key LONGVARCHAR NOT NULL UNIQUE PRIMARY KEY, value LONGVARCHAR);

INSERT INTO meta VALUES('version','1');
INSERT INTO meta VALUES('last_compatible_version','1');

INSERT INTO bounces VALUES('storage.test', 1, 1, 4, 4, 0, 0 ,0, 0),
                          ('stateful-bounce.test', 0, 0, 4, 4, 1, 1, 0, 0),
                          ('stateless-bounce.test', 0, 0, 4, 4, 0, 0 ,1, 1),
                          ('both-bounce-kinds.test', 0, 0, 4, 4, 1, 4 ,2, 6);


COMMIT;
