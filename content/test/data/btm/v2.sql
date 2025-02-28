PRAGMA foreign_keys = OFF;

BEGIN TRANSACTION;

CREATE TABLE IF NOT EXISTS bounces(
  site TEXT PRIMARY KEY NOT NULL,
  first_site_storage_time INTEGER DEFAULT NULL,
  last_site_storage_time INTEGER DEFAULT NULL,
  first_user_interaction_time INTEGER DEFAULT NULL,
  last_user_interaction_time INTEGER DEFAULT NULL,
  first_stateful_bounce_time INTEGER DEFAULT NULL,
  last_stateful_bounce_time INTEGER DEFAULT NULL,
  first_bounce_time INTEGER DEFAULT NULL,
  last_bounce_time INTEGER DEFAULT NULL
);

CREATE TABLE meta(
  key LONGVARCHAR NOT NULL UNIQUE PRIMARY KEY,
  value LONGVARCHAR
);

INSERT INTO
  meta
VALUES
  ('version', '2');

INSERT INTO
  meta
VALUES
  ('last_compatible_version', '2');

INSERT INTO
  meta
VALUES
  ('prepopulated', '1');

INSERT INTO
  bounces
VALUES
  ('storage.test', 1, 1, 4, 4, NULL, NULL, NULL, NULL),
  ('stateful-bounce.test', NULL, NULL, 4, 4, 1, 1, NULL, NULL),
  ('stateless-bounce.test', NULL, NULL, 4, 4, NULL, NULL, 1, 1),
  ('both-bounce-kinds.test', NULL, NULL, 4, 4, 1, 4, 2, 6);

COMMIT;
