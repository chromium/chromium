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
  last_bounce_time INTEGER DEFAULT NULL,
  first_web_authn_assertion_time INTEGER DEFAULT NULL,
  last_web_authn_assertion_time INTEGER DEFAULT NULL
);

CREATE TABLE IF NOT EXISTS popups(
  opener_site TEXT NOT NULL,
  popup_site TEXT NOT NULL,
  access_id INTEGER DEFAULT NULL,
  last_popup_time INTEGER DEFAULT NULL,
  is_current_interaction BOOLEAN DEFAULT NULL,
  PRIMARY KEY (opener_site, popup_site)
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
  ('version', '7');

INSERT INTO
  meta
VALUES
  ('last_compatible_version', '6');

INSERT INTO
  bounces
VALUES
  ('storage.test', 1, 1, 4, 4, NULL, NULL, NULL, NULL, NULL, NULL),
  ('stateful-bounce.test', NULL, NULL, 4, 4, 1, 1, NULL, NULL, NULL, NULL),
  ('stateless-bounce.test', NULL, NULL, 4, 4, NULL, NULL, 1, 1, NULL, NULL),
  ('both-bounce-kinds.test', NULL, NULL, 4, 4, 1, 4, 2, 6, NULL, NULL);

INSERT INTO
  popups
VALUES
  ('site1.com', '3p-site.com', 123, '2023-10-01 12:00:00', NULL),
  ('site2.com', '3p-site.com', 456, '2023-10-02 12:00:00', TRUE);

COMMIT;
