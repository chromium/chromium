PRAGMA foreign_keys=OFF;

BEGIN TRANSACTION;

CREATE TABLE IF NOT EXISTS public_sets(
  site TEXT NOT NULL,
  primary_site TEXT NOT NULL,
  site_type INTEGER NOT NULL,
  PRIMARY KEY(site)
) WITHOUT ROWID;

CREATE TABLE IF NOT EXISTS policy_modifications (
   browser_context_id TEXT NOT NULL,
   site TEXT NOT NULL,
   site_owner TEXT, -- May be NULL if this row represents a deletion.
   PRIMARY KEY (browser_context_id, site)
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

INSERT INTO meta VALUES('version','2');
INSERT INTO meta VALUES('last_compatible_version','1');
INSERT INTO meta VALUES('run_count','0');

INSERT INTO public_sets VALUES('https://aaa.test', 'https://bbb.test', 1),
                              ('https://bbb.test', 'https://bbb.test', 0);

-- b0: has sites to clear and has performed the clearing.
-- b1: has sites to clear but has not performed the clearing.
-- b2: has policy modifications but has no site to clear.
INSERT INTO browser_context_sites_to_clear VALUES('b0', 'https://example.test', 1),
                                                 ('b1', 'https://example.test', 1);
INSERT INTO browser_contexts_cleared VALUES('b0', 1);
INSERT INTO policy_modifications VALUES('b2', 'https://member1.test','https://example.test'),
                                       ('b2', 'https://member2.test',NULL);

COMMIT;