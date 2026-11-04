PRAGMA foreign_keys = OFF;

BEGIN TRANSACTION;

CREATE TABLE IF NOT EXISTS bounces(
  site TEXT PRIMARY KEY NOT NULL,
  first_user_activation_time INTEGER DEFAULT NULL,
  last_user_activation_time INTEGER DEFAULT NULL,
  first_bounce_time INTEGER DEFAULT NULL,
  last_bounce_time INTEGER DEFAULT NULL,
  first_web_authn_assertion_time INTEGER DEFAULT NULL,
  last_web_authn_assertion_time INTEGER DEFAULT NULL
);

CREATE TABLE meta(
  key LONGVARCHAR NOT NULL UNIQUE PRIMARY KEY,
  value LONGVARCHAR
);

CREATE TABLE config(
  key TEXT NOT NULL,
  int_value INTEGER,
  PRIMARY KEY (key)
);

INSERT INTO
  meta
VALUES
  ('version', '12');

INSERT INTO
  meta
VALUES
  ('last_compatible_version', '12');

INSERT INTO
  bounces
VALUES
  ('storage.test', 4, 4, NULL, NULL, NULL, NULL),
  ('stateful-bounce.test', 4, 4, NULL, NULL, NULL, NULL),
  ('stateless-bounce.test', 4, 4, 1, 1, NULL, NULL),
  ('both-bounce-kinds.test', 4, 4, 2, 6, NULL, NULL);

COMMIT;
