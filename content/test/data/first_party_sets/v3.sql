PRAGMA foreign_keys=OFF;

BEGIN TRANSACTION;

CREATE TABLE IF NOT EXISTS public_sets(
  version TEXT NOT NULL,
  site TEXT NOT NULL,
  primary_site TEXT NOT NULL,
  site_type INTEGER NOT NULL,
  PRIMARY KEY(version, site)
) WITHOUT ROWID;

CREATE TABLE IF NOT EXISTS browser_context_sets_version (
   browser_context_id TEXT PRIMARY KEY NOT NULL,
   public_sets_version TEXT NOT NULL
) WITHOUT ROWID;

CREATE TABLE IF NOT EXISTS policy_configurations (
   browser_context_id TEXT NOT NULL,
   site TEXT NOT NULL,
   primary_site TEXT, -- May be NULL if this row represents a deletion.
   PRIMARY KEY (browser_context_id, site)
) WITHOUT ROWID;

CREATE TABLE IF NOT EXISTS manual_sets (
  browser_context_id TEXT NOT NULL,
  site TEXT NOT NULL,
  primary_site TEXT NOT NULL,
  site_type INTEGER NOT NULL,
  PRIMARY KEY(browser_context_id, site)
) WITHOUT ROWID;

CREATE TABLE IF NOT EXISTS browser_context_sites_to_clear (
   browser_context_id TEXT NOT NULL,
   site TEXT NOT NULL,
   marked_at_run Integer NOT NULL,
   PRIMARY KEY (browser_context_id, site)
) WITHOUT ROWID;

CREATE INDEX idx_marked_at_run_sites ON browser_context_sites_to_clear (marked_at_run);

CREATE TABLE IF NOT EXISTS browser_contexts_cleared (
   browser_context_id TEXT PRIMARY KEY NOT NULL,
   cleared_at_run Integer NOT NULL
) WITHOUT ROWID;

CREATE INDEX idx_cleared_at_run_browser_contexts ON browser_contexts_cleared (cleared_at_run);

CREATE TABLE meta(key LONGVARCHAR NOT NULL UNIQUE PRIMARY KEY, value LONGVARCHAR);

INSERT INTO meta VALUES('version','3');
INSERT INTO meta VALUES('last_compatible_version','2');
INSERT INTO meta VALUES('run_count','1');

INSERT INTO public_sets VALUES('0.0.1', 'https://aaa.test', 'https://bbb.test', 1),
                              ('0.0.1', 'https://bbb.test', 'https://bbb.test', 0);
INSERT INTO browser_context_sets_version VALUES('b0', '0.0.1'),
                                               ('b1', '0.0.1'),
                                               ('b2', '0.0.1');

-- b0: has sites to clear and has performed the clearing.
-- b1: has sites to clear but has not performed the clearing.
-- b2: has policy modifications but has no site to clear.
INSERT INTO browser_context_sites_to_clear VALUES('b0', 'https://example.test', 1),
                                                 ('b1', 'https://example.test', 1);
INSERT INTO browser_contexts_cleared VALUES('b0', 1);
INSERT INTO policy_configurations VALUES('b2', 'https://member1.test','https://example.test'),
                                       ('b2', 'https://member2.test',NULL);
INSERT INTO manual_sets VALUES('b0', 'https://ccc.test','https://ddd.test', 1),
                              ('b0', 'https://ddd.test', 'https://ddd.test', 0);
COMMIT;