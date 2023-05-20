# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Chromium presubmit script for the WebDatabase.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools."""

PRESUBMIT_VERSION = '2.0.0'

def CheckCurrentDBVersionUpdatedCorrectly(input_api, output_api):
  """Checks that whenever the WebDatabase::kCurrentVersionNumber is updated,
     - WebDatabaseMigrationTest::kCurrentTestedVersionNumber is updated
       accordingly.
     - A version_x.sql file for the previous version is added."""

  def FindAffectedFile(path):
    return next(iter(input_api.change.AffectedTestableFiles(
      file_filter = lambda f: f.LocalPath() == path)), None)

  # Helper functions to extract integer constants from an affected file `f` via
  # a regex `pattern`, those first capture group corresponds to the integer.
  # `new_content` indicates whether the old/new content of `f` is searched.
  def FindInt(f, pattern, new_content = True):
    content = "".join(f.NewContents() if new_content else f.OldContents())
    match = pattern.search(content)
    return int(match.group(1)) if match else None
  def FindCppInt(f, name, new_content = True):
    return FindInt(f, input_api.re.compile("%s = ([0-9]+)" % name), new_content)

  # Determine if the version changed.
  webdb_file = FindAffectedFile("components/webdata/common/web_database.cc")
  if not webdb_file:
    return []
  version_var_name = "WebDatabase::kCurrentVersionNumber"
  version = FindCppInt(webdb_file, version_var_name, new_content=True)
  if version == FindCppInt(webdb_file, version_var_name, new_content=False):
    return []

  # Find the current tested version and check that it matches `version`.
  migration_test_path = \
    "components/webdata/common/web_database_migration_unittest.cc"
  migration_test_file = FindAffectedFile(migration_test_path)
  if (not migration_test_file or version != FindCppInt(migration_test_file,
      "WebDatabaseMigrationTest::kCurrentTestedVersionNumber")):
    return [output_api.PresubmitError("""
Whenever WebDatabase::kCurrentVersionNumber is updated,
WebDatabaseMigrationTest::kCurrentTestedVersionNumber in %s
needs to be updated.""" % migration_test_path)]

  # Check that a golden file for the previous version was added, and that its
  # version is set correctly.
  golden_file_dir = "components/test/data/web_database"
  golden_file = FindAffectedFile(
    "%s/version_%d.sql" % (golden_file_dir, version-1))
  sql_version_pattern = input_api.re.compile(
      "INSERT INTO meta VALUES\('version','([0-9]+)'\)")
  if not golden_file or version-1 != FindInt(golden_file, sql_version_pattern):
    return [output_api.PresubmitError("""
A golden file for version {0} needs to be added in {1}.
There are generally two ways of doing so:
- Copy version_{2}.sql. Update the version to {0} and make any changes that were
  made in version {0} (new tables, columns, etc). You can find out what
  changed by either looking at the WebDatabaseMigrationTest, or by finding
  the relevant CL (blame on version_{2}.sql) and looking at the migration logic.
- Generate the file from scratch:
  - Launch Chrome with WebDatabase version {0}. That is, without any of the new
    changes that version {3} introduces.
    ./out/Default/chrome --user-data-dir=/tmp/sql
    No need to complete the first run - closing Chrome immediately is fine.
  - Run sqlite3 '/tmp/sql/Default/Web Data'
        .output version_{0}.sql
        .dump
        .exit
  - Remove any INSERT statements to tables other than "meta" from
    version_{0}.sql.""".format(version-1, golden_file_dir, version-2, version))]

  return []
