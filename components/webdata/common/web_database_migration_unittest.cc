// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/guid.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/autofill/core/browser/webdata/autofill_entry.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/search_engines/keyword_table.h"
#include "components/signin/public/webdata/token_service_table.h"
#include "components/webdata/common/web_database.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::AutofillProfile;
using autofill::AutofillTable;
using autofill::CreditCard;
using base::ASCIIToUTF16;
using base::Time;

namespace {

std::string RemoveQuotes(const std::string& has_quotes) {
  std::string no_quotes;
  // SQLite quotes: http://www.sqlite.org/lang_keywords.html
  base::RemoveChars(has_quotes, "\"[]`", &no_quotes);
  return no_quotes;
}

}  // anonymous namespace

// The WebDatabaseMigrationTest encapsulates testing of database migrations.
// Specifically, these tests are intended to exercise any schema changes in
// the WebDatabase and data migrations that occur in
// |WebDatabase::MigrateOldVersionsAsNeeded()|.
class WebDatabaseMigrationTest : public testing::Test {
 public:
  WebDatabaseMigrationTest() {}
  ~WebDatabaseMigrationTest() override {}

  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  // Load the database via the WebDatabase class and migrate the database to
  // the current version.
  void DoMigration() {
    AutofillTable autofill_table;
    KeywordTable keyword_table;
    TokenServiceTable token_service_table;

    WebDatabase db;
    db.AddTable(&autofill_table);
    db.AddTable(&keyword_table);
    db.AddTable(&token_service_table);

    // This causes the migration to occur.
    ASSERT_EQ(sql::INIT_OK, db.Init(GetDatabasePath()));
  }

 protected:
  // Current tested version number.  When adding a migration in
  // |WebDatabase::MigrateOldVersionsAsNeeded()| and changing the version number
  // |kCurrentVersionNumber| this value should change to reflect the new version
  // number and a new migration test added below.
  static const int kCurrentTestedVersionNumber;

  base::FilePath GetDatabasePath() {
    const base::FilePath::CharType kWebDatabaseFilename[] =
        FILE_PATH_LITERAL("TestWebDatabase.sqlite3");
    return temp_dir_.GetPath().Append(base::FilePath(kWebDatabaseFilename));
  }

  // The textual contents of |file| are read from
  // "components/test/data/web_database" and returned in the string |contents|.
  // Returns true if the file exists and is read successfully, false otherwise.
  bool GetWebDatabaseData(const base::FilePath& file, std::string* contents) {
    base::FilePath source_path;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &source_path);
    source_path = source_path.AppendASCII("components");
    source_path = source_path.AppendASCII("test");
    source_path = source_path.AppendASCII("data");
    source_path = source_path.AppendASCII("web_database");
    source_path = source_path.Append(file);
    return base::PathExists(source_path) &&
           base::ReadFileToString(source_path, contents);
  }

  static int VersionFromConnection(sql::Database* connection) {
    // Get version.
    sql::Statement s(connection->GetUniqueStatement(
        "SELECT value FROM meta WHERE key='version'"));
    if (!s.Step())
      return 0;
    return s.ColumnInt(0);
  }

  // The sql files located in "components/test/data/web_database" were generated
  // by launching the Chromium application prior to schema change, then using
  // the sqlite3 command-line application to dump the contents of the "Web Data"
  // database.
  // Like this:
  //   > .output version_nn.sql
  //   > .dump
  void LoadDatabase(const base::FilePath::StringType& file);

 private:
  base::ScopedTempDir temp_dir_;

  DISALLOW_COPY_AND_ASSIGN(WebDatabaseMigrationTest);
};

const int WebDatabaseMigrationTest::kCurrentTestedVersionNumber = 82;

void WebDatabaseMigrationTest::LoadDatabase(
    const base::FilePath::StringType& file) {
  std::string contents;
  ASSERT_TRUE(GetWebDatabaseData(base::FilePath(file), &contents));

  sql::Database connection;
  ASSERT_TRUE(connection.Open(GetDatabasePath()));
  ASSERT_TRUE(connection.Execute(contents.data()));
}

// Tests that migrating from the golden files version_XX.sql results in the same
// schema as migrating from an empty database.
TEST_F(WebDatabaseMigrationTest, VersionXxSqlFilesAreGolden) {
  DoMigration();
  sql::Database connection;
  ASSERT_TRUE(connection.Open(GetDatabasePath()));
  const std::string& expected_schema = RemoveQuotes(connection.GetSchema());
  for (int i = WebDatabase::kDeprecatedVersionNumber + 1;
       i < kCurrentTestedVersionNumber; ++i) {
    // We don't test version 52 because there's a slight discrepancy in the
    // initialization code and the migration code (relating to schema
    // formatting). Fixing the bug is possible, but would require updating every
    // version_nn.sql file.
    if (i == 52)
      continue;

    connection.Raze();
    const base::FilePath& file_name = base::FilePath::FromUTF8Unsafe(
        "version_" + base::NumberToString(i) + ".sql");
    ASSERT_NO_FATAL_FAILURE(LoadDatabase(file_name.value()))
        << "Failed to load " << file_name.MaybeAsASCII();
    DoMigration();

    EXPECT_EQ(expected_schema, RemoveQuotes(connection.GetSchema()))
        << "For version " << i;
  }
}

// Tests that the all migrations from an empty database succeed.
TEST_F(WebDatabaseMigrationTest, MigrateEmptyToCurrent) {
  DoMigration();

  // Verify post-conditions.  These are expectations for current version of the
  // database.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    // Check that expected tables are present.
    EXPECT_TRUE(connection.DoesTableExist("autofill"));
    // The autofill_dates table is obsolete. (It's been merged into the autofill
    // table.)
    EXPECT_FALSE(connection.DoesTableExist("autofill_dates"));
    EXPECT_TRUE(connection.DoesTableExist("autofill_profiles"));
    EXPECT_TRUE(connection.DoesTableExist("credit_cards"));
    EXPECT_TRUE(connection.DoesTableExist("keywords"));
    EXPECT_TRUE(connection.DoesTableExist("meta"));
    EXPECT_TRUE(connection.DoesTableExist("token_service"));
    // The web_apps and web_apps_icons tables are obsolete as of version 58.
    EXPECT_FALSE(connection.DoesTableExist("web_apps"));
    EXPECT_FALSE(connection.DoesTableExist("web_app_icons"));
    // The web_intents and web_intents_defaults tables are obsolete as of
    // version 58.
    EXPECT_FALSE(connection.DoesTableExist("web_intents"));
    EXPECT_FALSE(connection.DoesTableExist("web_intents_defaults"));
  }
}

// Versions below 52 are deprecated. This verifies that old databases are razed.
TEST_F(WebDatabaseMigrationTest, RazeDeprecatedVersionAndReinit) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_50.sql")));

  // Verify pre-conditions.  These are expectations for version 50 of the
  // database.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 50, 50));

    ASSERT_FALSE(connection.DoesColumnExist("keywords", "image_url"));
    ASSERT_FALSE(
        connection.DoesColumnExist("keywords", "search_url_post_params"));
    ASSERT_FALSE(
        connection.DoesColumnExist("keywords", "suggest_url_post_params"));
    ASSERT_FALSE(
        connection.DoesColumnExist("keywords", "image_url_post_params"));
  }

  DoMigration();

  // Verify post-conditions.  These are expectations for current version of the
  // database.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    // New columns should have been created.
    EXPECT_TRUE(connection.DoesColumnExist("keywords", "image_url"));
    EXPECT_TRUE(
        connection.DoesColumnExist("keywords", "search_url_post_params"));
    EXPECT_TRUE(
        connection.DoesColumnExist("keywords", "suggest_url_post_params"));
    EXPECT_TRUE(
        connection.DoesColumnExist("keywords", "image_url_post_params"));
  }
}

// Tests that the column |new_tab_url| is added to the keyword table schema for
// a version 52 database.
TEST_F(WebDatabaseMigrationTest, MigrateVersion52ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_52.sql")));

  // Verify pre-conditions.  These are expectations for version 52 of the
  // database.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 52, 52));

    ASSERT_FALSE(connection.DoesColumnExist("keywords", "new_tab_url"));
  }

  DoMigration();

  // Verify post-conditions.  These are expectations for current version of the
  // database.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    // New columns should have been created.
    EXPECT_TRUE(connection.DoesColumnExist("keywords", "new_tab_url"));
  }
}

// Tests that for a version 54 database,
//   (a) The street_address, dependent_locality, and sorting_code columns are
//       added to the autofill_profiles table schema.
//   (b) The address_line1, address_line2, and country columns are dropped from
//       the autofill_profiles table schema.
//   (c) The type column is dropped from the autofill_profile_phones schema.
TEST_F(WebDatabaseMigrationTest, MigrateVersion53ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_53.sql")));

  // Verify pre-conditions.  These are expectations for version 53 of the
  // database.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    EXPECT_TRUE(
        connection.DoesColumnExist("autofill_profiles", "address_line_1"));
    EXPECT_TRUE(
        connection.DoesColumnExist("autofill_profiles", "address_line_2"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles", "country"));
    EXPECT_FALSE(
        connection.DoesColumnExist("autofill_profiles", "street_address"));
    EXPECT_FALSE(
        connection.DoesColumnExist("autofill_profiles", "dependent_locality"));
    EXPECT_FALSE(
        connection.DoesColumnExist("autofill_profiles", "sorting_code"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profile_phones", "type"));
  }

  DoMigration();

  // Verify post-conditions.  These are expectations for current version of the
  // database.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    // Columns should have been added and removed appropriately.
    EXPECT_FALSE(
        connection.DoesColumnExist("autofill_profiles", "address_line1"));
    EXPECT_FALSE(
        connection.DoesColumnExist("autofill_profiles", "address_line2"));
    EXPECT_FALSE(connection.DoesColumnExist("autofill_profiles", "country"));
    EXPECT_TRUE(
        connection.DoesColumnExist("autofill_profiles", "street_address"));
    EXPECT_TRUE(
        connection.DoesColumnExist("autofill_profiles", "dependent_locality"));
    EXPECT_TRUE(
        connection.DoesColumnExist("autofill_profiles", "sorting_code"));
    EXPECT_FALSE(connection.DoesColumnExist("autofill_profile_phones", "type"));

    // Data should have been preserved.
    sql::Statement s_profiles(connection.GetUniqueStatement(
        "SELECT guid, company_name, street_address, dependent_locality,"
        " city, state, zipcode, sorting_code, country_code, date_modified,"
        " origin "
        "FROM autofill_profiles"));

    // Address lines 1 and 2.
    ASSERT_TRUE(s_profiles.Step());
    EXPECT_EQ("00000000-0000-0000-0000-000000000001",
              s_profiles.ColumnString(0));
    EXPECT_EQ(ASCIIToUTF16("Google, Inc."), s_profiles.ColumnString16(1));
    EXPECT_EQ(ASCIIToUTF16("1950 Charleston Rd.\n"
                           "(2nd floor)"),
              s_profiles.ColumnString16(2));
    EXPECT_EQ(base::string16(), s_profiles.ColumnString16(3));
    EXPECT_EQ(ASCIIToUTF16("Mountain View"), s_profiles.ColumnString16(4));
    EXPECT_EQ(ASCIIToUTF16("CA"), s_profiles.ColumnString16(5));
    EXPECT_EQ(ASCIIToUTF16("94043"), s_profiles.ColumnString16(6));
    EXPECT_EQ(base::string16(), s_profiles.ColumnString16(7));
    EXPECT_EQ(ASCIIToUTF16("US"), s_profiles.ColumnString16(8));
    EXPECT_EQ(1386046731, s_profiles.ColumnInt(9));
    EXPECT_EQ(ASCIIToUTF16(autofill::kSettingsOrigin),
              s_profiles.ColumnString16(10));

    // Only address line 1.
    ASSERT_TRUE(s_profiles.Step());
    EXPECT_EQ("00000000-0000-0000-0000-000000000002",
              s_profiles.ColumnString(0));
    EXPECT_EQ(ASCIIToUTF16("Google!"), s_profiles.ColumnString16(1));
    EXPECT_EQ(ASCIIToUTF16("1600 Amphitheatre Pkwy."),
              s_profiles.ColumnString16(2));
    EXPECT_EQ(base::string16(), s_profiles.ColumnString16(3));
    EXPECT_EQ(ASCIIToUTF16("Mtn. View"), s_profiles.ColumnString16(4));
    EXPECT_EQ(ASCIIToUTF16("California"), s_profiles.ColumnString16(5));
    EXPECT_EQ(ASCIIToUTF16("94043-1234"), s_profiles.ColumnString16(6));
    EXPECT_EQ(base::string16(), s_profiles.ColumnString16(7));
    EXPECT_EQ(ASCIIToUTF16("US"), s_profiles.ColumnString16(8));
    EXPECT_EQ(1386046800, s_profiles.ColumnInt(9));
    EXPECT_EQ(ASCIIToUTF16(autofill::kSettingsOrigin),
              s_profiles.ColumnString16(10));

    // Only address line 2.
    ASSERT_TRUE(s_profiles.Step());
    EXPECT_EQ("00000000-0000-0000-0000-000000000003",
              s_profiles.ColumnString(0));
    EXPECT_EQ(base::string16(), s_profiles.ColumnString16(1));
    EXPECT_EQ(ASCIIToUTF16("\nOnly line 2???"), s_profiles.ColumnString16(2));
    EXPECT_EQ(base::string16(), s_profiles.ColumnString16(3));
    EXPECT_EQ(base::string16(), s_profiles.ColumnString16(4));
    EXPECT_EQ(base::string16(), s_profiles.ColumnString16(5));
    EXPECT_EQ(base::string16(), s_profiles.ColumnString16(6));
    EXPECT_EQ(base::string16(), s_profiles.ColumnString16(7));
    EXPECT_EQ(base::string16(), s_profiles.ColumnString16(8));
    EXPECT_EQ(1386046834, s_profiles.ColumnInt(9));
    EXPECT_EQ(ASCIIToUTF16(autofill::kSettingsOrigin),
              s_profiles.ColumnString16(10));

    // No address lines.
    ASSERT_TRUE(s_profiles.Step());
    EXPECT_EQ("00000000-0000-0000-0000-000000000004",
              s_profiles.ColumnString(0));
    EXPECT_EQ(base::string16(), s_profiles.ColumnString16(1));
    EXPECT_EQ(base::string16(), s_profiles.ColumnString16(2));
    EXPECT_EQ(base::string16(), s_profiles.ColumnString16(3));
    EXPECT_EQ(base::string16(), s_profiles.ColumnString16(4));
    EXPECT_EQ(ASCIIToUTF16("Texas"), s_profiles.ColumnString16(5));
    EXPECT_EQ(base::string16(), s_profiles.ColumnString16(6));
    EXPECT_EQ(base::string16(), s_profiles.ColumnString16(7));
    EXPECT_EQ(base::string16(), s_profiles.ColumnString16(8));
    EXPECT_EQ(1386046847, s_profiles.ColumnInt(9));
    EXPECT_EQ(ASCIIToUTF16(autofill::kSettingsOrigin),
              s_profiles.ColumnString16(10));

    // That should be it.
    EXPECT_FALSE(s_profiles.Step());

    // Verify the phone number data as well.
    sql::Statement s_phones(connection.GetUniqueStatement(
        "SELECT guid, number FROM autofill_profile_phones"));

    ASSERT_TRUE(s_phones.Step());
    EXPECT_EQ("00000000-0000-0000-0000-000000000001", s_phones.ColumnString(0));
    EXPECT_EQ(ASCIIToUTF16("1.800.555.1234"), s_phones.ColumnString16(1));

    ASSERT_TRUE(s_phones.Step());
    EXPECT_EQ("00000000-0000-0000-0000-000000000001", s_phones.ColumnString(0));
    EXPECT_EQ(ASCIIToUTF16("+1 (800) 555-4321"), s_phones.ColumnString16(1));

    ASSERT_TRUE(s_phones.Step());
    EXPECT_EQ("00000000-0000-0000-0000-000000000002", s_phones.ColumnString(0));
    EXPECT_EQ(base::string16(), s_phones.ColumnString16(1));

    ASSERT_TRUE(s_phones.Step());
    EXPECT_EQ("00000000-0000-0000-0000-000000000003", s_phones.ColumnString(0));
    EXPECT_EQ(ASCIIToUTF16("6505557890"), s_phones.ColumnString16(1));

    ASSERT_TRUE(s_phones.Step());
    EXPECT_EQ("00000000-0000-0000-0000-000000000004", s_phones.ColumnString(0));
    EXPECT_EQ(base::string16(), s_phones.ColumnString16(1));

    EXPECT_FALSE(s_phones.Step());
  }
}

// Tests that migrating from version 54 to version 55 drops the autofill_dates
// table, and merges the appropriate dates into the autofill table.
TEST_F(WebDatabaseMigrationTest, MigrateVersion54ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_54.sql")));

  // Verify pre-conditions.  These are expectations for version 54 of the
  // database.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    EXPECT_TRUE(connection.DoesTableExist("autofill_dates"));
    EXPECT_FALSE(connection.DoesColumnExist("autofill", "date_created"));
    EXPECT_FALSE(connection.DoesColumnExist("autofill", "date_last_used"));

    // Verify the incoming data.
    sql::Statement s_autofill(connection.GetUniqueStatement(
        "SELECT name, value, value_lower, pair_id, count FROM autofill"));
    sql::Statement s_dates(connection.GetUniqueStatement(
        "SELECT pair_id, date_created FROM autofill_dates"));

    // An entry with one timestamp.
    ASSERT_TRUE(s_autofill.Step());
    EXPECT_EQ(ASCIIToUTF16("Name"), s_autofill.ColumnString16(0));
    EXPECT_EQ(ASCIIToUTF16("John Doe"), s_autofill.ColumnString16(1));
    EXPECT_EQ(ASCIIToUTF16("john doe"), s_autofill.ColumnString16(2));
    EXPECT_EQ(10, s_autofill.ColumnInt(3));
    EXPECT_EQ(1, s_autofill.ColumnInt(4));
    ASSERT_TRUE(s_dates.Step());
    EXPECT_EQ(10, s_dates.ColumnInt(0));
    EXPECT_EQ(1384299100, s_dates.ColumnInt64(1));

    // Another entry with one timestamp, differing from the previous one in case
    // only.
    ASSERT_TRUE(s_autofill.Step());
    EXPECT_EQ(ASCIIToUTF16("Name"), s_autofill.ColumnString16(0));
    EXPECT_EQ(ASCIIToUTF16("john doe"), s_autofill.ColumnString16(1));
    EXPECT_EQ(ASCIIToUTF16("john doe"), s_autofill.ColumnString16(2));
    EXPECT_EQ(11, s_autofill.ColumnInt(3));
    EXPECT_EQ(1, s_autofill.ColumnInt(4));
    ASSERT_TRUE(s_dates.Step());
    EXPECT_EQ(11, s_dates.ColumnInt(0));
    EXPECT_EQ(1384299200, s_dates.ColumnInt64(1));

    // An entry with two timestamps (with count > 2; this is realistic).
    ASSERT_TRUE(s_autofill.Step());
    EXPECT_EQ(ASCIIToUTF16("Email"), s_autofill.ColumnString16(0));
    EXPECT_EQ(ASCIIToUTF16("jane@example.com"), s_autofill.ColumnString16(1));
    EXPECT_EQ(ASCIIToUTF16("jane@example.com"), s_autofill.ColumnString16(2));
    EXPECT_EQ(20, s_autofill.ColumnInt(3));
    EXPECT_EQ(3, s_autofill.ColumnInt(4));
    ASSERT_TRUE(s_dates.Step());
    EXPECT_EQ(20, s_dates.ColumnInt(0));
    EXPECT_EQ(1384299300, s_dates.ColumnInt64(1));
    ASSERT_TRUE(s_dates.Step());
    EXPECT_EQ(20, s_dates.ColumnInt(0));
    EXPECT_EQ(1384299301, s_dates.ColumnInt64(1));

    // An entry with more than two timestamps, which are stored out of order.
    ASSERT_TRUE(s_autofill.Step());
    EXPECT_EQ(ASCIIToUTF16("Email"), s_autofill.ColumnString16(0));
    EXPECT_EQ(ASCIIToUTF16("jane.doe@example.org"),
              s_autofill.ColumnString16(1));
    EXPECT_EQ(ASCIIToUTF16("jane.doe@example.org"),
              s_autofill.ColumnString16(2));
    EXPECT_EQ(21, s_autofill.ColumnInt(3));
    EXPECT_EQ(4, s_autofill.ColumnInt(4));
    ASSERT_TRUE(s_dates.Step());
    EXPECT_EQ(21, s_dates.ColumnInt(0));
    EXPECT_EQ(1384299401, s_dates.ColumnInt64(1));
    ASSERT_TRUE(s_dates.Step());
    EXPECT_EQ(21, s_dates.ColumnInt(0));
    EXPECT_EQ(1384299400, s_dates.ColumnInt64(1));
    ASSERT_TRUE(s_dates.Step());
    EXPECT_EQ(21, s_dates.ColumnInt(0));
    EXPECT_EQ(1384299403, s_dates.ColumnInt64(1));
    ASSERT_TRUE(s_dates.Step());
    EXPECT_EQ(21, s_dates.ColumnInt(0));
    EXPECT_EQ(1384299402, s_dates.ColumnInt64(1));

    // No more entries expected.
    ASSERT_FALSE(s_autofill.Step());
    ASSERT_FALSE(s_dates.Step());
  }

  DoMigration();

  // Verify post-conditions.  These are expectations for current version of the
  // database.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    // The autofill_dates table should have been dropped, and its columns should
    // have been migrated to the autofill table.
    EXPECT_FALSE(connection.DoesTableExist("autofill_dates"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill", "date_created"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill", "date_last_used"));

    // Data should have been preserved.  Note that it appears out of order
    // relative to the previous table, as it's been alphabetized.  That's ok.
    sql::Statement s(connection.GetUniqueStatement(
        "SELECT name, value, value_lower, date_created, date_last_used,"
        " count "
        "FROM autofill "
        "ORDER BY name, value ASC"));

    // "jane.doe@example.org": Timestamps should be parsed correctly, and only
    // the first and last should be kept.
    ASSERT_TRUE(s.Step());
    EXPECT_EQ(ASCIIToUTF16("Email"), s.ColumnString16(0));
    EXPECT_EQ(ASCIIToUTF16("jane.doe@example.org"), s.ColumnString16(1));
    EXPECT_EQ(ASCIIToUTF16("jane.doe@example.org"), s.ColumnString16(2));
    EXPECT_EQ(1384299400, s.ColumnInt64(3));
    EXPECT_EQ(1384299403, s.ColumnInt64(4));
    EXPECT_EQ(4, s.ColumnInt(5));

    // "jane@example.com": Timestamps should be parsed correctly.
    ASSERT_TRUE(s.Step());
    EXPECT_EQ(ASCIIToUTF16("Email"), s.ColumnString16(0));
    EXPECT_EQ(ASCIIToUTF16("jane@example.com"), s.ColumnString16(1));
    EXPECT_EQ(ASCIIToUTF16("jane@example.com"), s.ColumnString16(2));
    EXPECT_EQ(1384299300, s.ColumnInt64(3));
    EXPECT_EQ(1384299301, s.ColumnInt64(4));
    EXPECT_EQ(3, s.ColumnInt(5));

    // "John Doe": The single timestamp should be assigned as both the creation
    // and the last use timestamp.
    ASSERT_TRUE(s.Step());
    EXPECT_EQ(ASCIIToUTF16("Name"), s.ColumnString16(0));
    EXPECT_EQ(ASCIIToUTF16("John Doe"), s.ColumnString16(1));
    EXPECT_EQ(ASCIIToUTF16("john doe"), s.ColumnString16(2));
    EXPECT_EQ(1384299100, s.ColumnInt64(3));
    EXPECT_EQ(1384299100, s.ColumnInt64(4));
    EXPECT_EQ(1, s.ColumnInt(5));

    // "john doe": Should not be merged with "John Doe" (case-sensitivity).
    ASSERT_TRUE(s.Step());
    EXPECT_EQ(ASCIIToUTF16("Name"), s.ColumnString16(0));
    EXPECT_EQ(ASCIIToUTF16("john doe"), s.ColumnString16(1));
    EXPECT_EQ(ASCIIToUTF16("john doe"), s.ColumnString16(2));
    EXPECT_EQ(1384299200, s.ColumnInt64(3));
    EXPECT_EQ(1384299200, s.ColumnInt64(4));
    EXPECT_EQ(1, s.ColumnInt(5));

    // No more entries expected.
    ASSERT_FALSE(s.Step());
  }
}

// Tests that migrating from version 55 to version 56 adds the language_code
// column to autofill_profiles table.
TEST_F(WebDatabaseMigrationTest, MigrateVersion55ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_55.sql")));

  // Verify pre-conditions. These are expectations for version 55 of the
  // database.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    EXPECT_FALSE(
        connection.DoesColumnExist("autofill_profiles", "language_code"));
  }

  DoMigration();

  // Verify post-conditions. These are expectations for current version of the
  // database.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    // The language_code column should have been added to autofill_profiles
    // table.
    EXPECT_TRUE(
        connection.DoesColumnExist("autofill_profiles", "language_code"));

    // Data should have been preserved. Language code should have been set to
    // empty string.
    sql::Statement s_profiles(connection.GetUniqueStatement(
        "SELECT guid, company_name, street_address, dependent_locality,"
        " city, state, zipcode, sorting_code, country_code, date_modified,"
        " origin, language_code "
        "FROM autofill_profiles"));

    ASSERT_TRUE(s_profiles.Step());
    EXPECT_EQ("00000000-0000-0000-0000-000000000001",
              s_profiles.ColumnString(0));
    EXPECT_EQ(ASCIIToUTF16("Google Inc"), s_profiles.ColumnString16(1));
    EXPECT_EQ(ASCIIToUTF16("340 Main St"), s_profiles.ColumnString16(2));
    EXPECT_EQ(base::string16(), s_profiles.ColumnString16(3));
    EXPECT_EQ(ASCIIToUTF16("Los Angeles"), s_profiles.ColumnString16(4));
    EXPECT_EQ(ASCIIToUTF16("CA"), s_profiles.ColumnString16(5));
    EXPECT_EQ(ASCIIToUTF16("90291"), s_profiles.ColumnString16(6));
    EXPECT_EQ(base::string16(), s_profiles.ColumnString16(7));
    EXPECT_EQ(ASCIIToUTF16("US"), s_profiles.ColumnString16(8));
    EXPECT_EQ(1395948829, s_profiles.ColumnInt(9));
    EXPECT_EQ(ASCIIToUTF16(autofill::kSettingsOrigin),
              s_profiles.ColumnString16(10));
    EXPECT_EQ(std::string(), s_profiles.ColumnString(11));

    // No more entries expected.
    ASSERT_FALSE(s_profiles.Step());
  }
}

// Tests that migrating from version 56 to version 57 adds the full_name
// column to autofill_profile_names table.
TEST_F(WebDatabaseMigrationTest, MigrateVersion56ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_56.sql")));

  // Verify pre-conditions. These are expectations for version 56 of the
  // database.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    EXPECT_FALSE(
        connection.DoesColumnExist("autofill_profile_names", "full_name"));

    // Verify the starting data.
    sql::Statement s_names(connection.GetUniqueStatement(
        "SELECT guid, first_name, middle_name, last_name "
        "FROM autofill_profile_names"));
    ASSERT_TRUE(s_names.Step());
    EXPECT_EQ("B41FE6E0-B13E-2A2A-BF0B-29FCE2C3ADBD", s_names.ColumnString(0));
    EXPECT_EQ(ASCIIToUTF16("Jon"), s_names.ColumnString16(1));
    EXPECT_EQ(base::string16(), s_names.ColumnString16(2));
    EXPECT_EQ(ASCIIToUTF16("Smith"), s_names.ColumnString16(3));
  }

  DoMigration();

  // Verify post-conditions. These are expectations for current version of the
  // database.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    // The full_name column should have been added to autofill_profile_names
    // table.
    EXPECT_TRUE(
        connection.DoesColumnExist("autofill_profile_names", "full_name"));

    // Data should have been preserved. Full name should have been set to the
    // empty string.
    sql::Statement s_names(connection.GetUniqueStatement(
        "SELECT guid, first_name, middle_name, last_name, full_name "
        "FROM autofill_profile_names"));

    ASSERT_TRUE(s_names.Step());
    EXPECT_EQ("B41FE6E0-B13E-2A2A-BF0B-29FCE2C3ADBD", s_names.ColumnString(0));
    EXPECT_EQ(ASCIIToUTF16("Jon"), s_names.ColumnString16(1));
    EXPECT_EQ(base::string16(), s_names.ColumnString16(2));
    EXPECT_EQ(ASCIIToUTF16("Smith"), s_names.ColumnString16(3));
    EXPECT_EQ(base::string16(), s_names.ColumnString16(4));

    // No more entries expected.
    ASSERT_FALSE(s_names.Step());
  }
}

// Tests that migrating from version 57 to version 58 drops the web_intents and
// web_apps tables.
TEST_F(WebDatabaseMigrationTest, MigrateVersion57ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_57.sql")));

  // Verify pre-conditions. These are expectations for version 57 of the
  // database.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    EXPECT_TRUE(connection.DoesTableExist("web_apps"));
    EXPECT_TRUE(connection.DoesTableExist("web_app_icons"));
    EXPECT_TRUE(connection.DoesTableExist("web_intents"));
    EXPECT_TRUE(connection.DoesTableExist("web_intents_defaults"));
  }

  DoMigration();

  // Verify post-conditions. These are expectations for current version of the
  // database.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    EXPECT_FALSE(connection.DoesTableExist("web_apps"));
    EXPECT_FALSE(connection.DoesTableExist("web_app_icons"));
    EXPECT_FALSE(connection.DoesTableExist("web_intents"));
    EXPECT_FALSE(connection.DoesTableExist("web_intents_defaults"));
  }
}

// Tests that migrating from version 58 to version 59 drops the omnibox
// extension keywords.
TEST_F(WebDatabaseMigrationTest, MigrateVersion58ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_58.sql")));

  const char query_extensions[] =
      "SELECT * FROM keywords "
      "WHERE url='chrome-extension://iphchnegaodmijmkdlbhbanjhfphhikp/"
      "?q={searchTerms}'";
  // Verify pre-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 58, 58));

    sql::Statement s(connection.GetUniqueStatement(query_extensions));
    ASSERT_TRUE(s.is_valid());
    int count = 0;
    while (s.Step()) {
      ++count;
    }
    EXPECT_EQ(1, count);
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    sql::Statement s(connection.GetUniqueStatement(query_extensions));
    ASSERT_TRUE(s.is_valid());
    int count = 0;
    while (s.Step()) {
      ++count;
    }
    EXPECT_EQ(0, count);

    s.Assign(
        connection.GetUniqueStatement("SELECT * FROM keywords "
                                      "WHERE short_name='Google'"));
    ASSERT_TRUE(s.is_valid());
    count = 0;
    while (s.Step()) {
      ++count;
    }
    EXPECT_EQ(1, count);
  }
}

// Tests creation of the server_credit_cards table.
TEST_F(WebDatabaseMigrationTest, MigrateVersion59ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_59.sql")));

  // Verify pre-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 59, 59));

    ASSERT_FALSE(connection.DoesTableExist("masked_credit_cards"));
    ASSERT_FALSE(connection.DoesTableExist("unmasked_credit_cards"));
    ASSERT_FALSE(connection.DoesTableExist("server_addresses"));
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    ASSERT_TRUE(connection.DoesTableExist("masked_credit_cards"));
    ASSERT_TRUE(connection.DoesTableExist("unmasked_credit_cards"));
    ASSERT_TRUE(connection.DoesTableExist("server_addresses"));
  }
}

// Tests addition of use_count and use_date fields to autofill profiles and
// credit cards.
TEST_F(WebDatabaseMigrationTest, MigrateVersion60ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_60.sql")));

  // Verify pre-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 60, 60));

    EXPECT_FALSE(connection.DoesColumnExist("credit_cards", "use_count"));
    EXPECT_FALSE(connection.DoesColumnExist("credit_cards", "use_date"));
    EXPECT_FALSE(connection.DoesColumnExist("autofill_profiles", "use_count"));
    EXPECT_FALSE(connection.DoesColumnExist("autofill_profiles", "use_date"));
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    EXPECT_TRUE(connection.DoesColumnExist("credit_cards", "use_count"));
    EXPECT_TRUE(connection.DoesColumnExist("credit_cards", "use_date"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles", "use_count"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles", "use_date"));
  }
}

// Tests addition of use_count and use_date fields to unmasked server cards.
TEST_F(WebDatabaseMigrationTest, MigrateVersion61ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_61.sql")));

  // Verify pre-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 61, 61));

    EXPECT_FALSE(
        connection.DoesColumnExist("unmasked_credit_cards", "use_count"));
    EXPECT_FALSE(
        connection.DoesColumnExist("unmasked_credit_cards", "use_date"));
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    EXPECT_TRUE(
        connection.DoesColumnExist("unmasked_credit_cards", "use_count"));
    EXPECT_TRUE(
        connection.DoesColumnExist("unmasked_credit_cards", "use_date"));
  }
}

// Tests addition of server metadata tables.
TEST_F(WebDatabaseMigrationTest, MigrateVersion64ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_64.sql")));

  // Verify pre-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 64, 64));

    EXPECT_FALSE(connection.DoesTableExist("server_card_metadata"));
    EXPECT_FALSE(connection.DoesTableExist("server_address_metadata"));

    // Add a server address --- make sure it gets an ID.
    sql::Statement insert_profiles(connection.GetUniqueStatement(
        "INSERT INTO server_addresses(id, postal_code) "
        "VALUES ('', 90210)"));
    insert_profiles.Run();
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    EXPECT_TRUE(connection.DoesTableExist("server_card_metadata"));
    EXPECT_TRUE(connection.DoesTableExist("server_address_metadata"));

    sql::Statement read_profiles(connection.GetUniqueStatement(
        "SELECT id, postal_code FROM server_addresses"));
    ASSERT_TRUE(read_profiles.Step());
    EXPECT_FALSE(read_profiles.ColumnString(0).empty());
    EXPECT_EQ("90210", read_profiles.ColumnString(1));
  }
}

// Tests addition of credit card billing address.
TEST_F(WebDatabaseMigrationTest, MigrateVersion65ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_65.sql")));

  // Verify pre-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 65, 65));

    EXPECT_FALSE(
        connection.DoesColumnExist("credit_cards", "billing_address_id"));

    EXPECT_TRUE(connection.Execute(
        "INSERT INTO credit_cards(guid, name_on_card) VALUES ('', 'Alice')"));
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    EXPECT_TRUE(
        connection.DoesColumnExist("credit_cards", "billing_address_id"));

    sql::Statement read_credit_cards(connection.GetUniqueStatement(
        "SELECT name_on_card, billing_address_id FROM credit_cards"));
    ASSERT_TRUE(read_credit_cards.Step());
    EXPECT_EQ("Alice", read_credit_cards.ColumnString(0));
    EXPECT_TRUE(read_credit_cards.ColumnString(1).empty());
  }
}

// Tests addition of masked server credit card billing address.
// That column was moved to server_card_metadata in version 71.
TEST_F(WebDatabaseMigrationTest, MigrateVersion66ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_66.sql")));

  // Verify pre-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 66, 66));

    EXPECT_FALSE(connection.DoesColumnExist("masked_credit_cards",
                                            "billing_address_id"));

    EXPECT_TRUE(
        connection.Execute("INSERT INTO masked_credit_cards(id, name_on_card) "
                           "VALUES ('id', 'Alice')"));
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    // The column was moved to server_card_metadata in version 71.
    EXPECT_FALSE(connection.DoesColumnExist("masked_credit_cards",
                                            "billing_address_id"));
    EXPECT_TRUE(connection.DoesColumnExist("server_card_metadata",
                                           "billing_address_id"));
  }
}

// Tests deletion of show_in_default_list column in keywords table.
TEST_F(WebDatabaseMigrationTest, MigrateVersion67ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_67.sql")));

  // Verify pre-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 67, 67));

    EXPECT_TRUE(connection.DoesColumnExist("keywords", "show_in_default_list"));
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    EXPECT_FALSE(
        connection.DoesColumnExist("keywords", "show_in_default_list"));
  }
}

// Tests addition of last_visited column in keywords table.
TEST_F(WebDatabaseMigrationTest, MigrateVersion68ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_68.sql")));

  // Verify pre-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 68, 68));

    EXPECT_FALSE(connection.DoesColumnExist("keywords", "last_visited"));
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    EXPECT_TRUE(connection.DoesColumnExist("keywords", "last_visited"));
  }
}

// Tests addition of sync metadata and model type state tables.
TEST_F(WebDatabaseMigrationTest, MigrateVersion69ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_69.sql")));

  // Verify pre-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 69, 69));

    EXPECT_FALSE(connection.DoesTableExist("autofill_sync_metadata"));
    EXPECT_FALSE(connection.DoesTableExist("autofill_model_type_state"));
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    EXPECT_TRUE(connection.DoesTableExist("autofill_sync_metadata"));
    EXPECT_TRUE(connection.DoesTableExist("autofill_model_type_state"));
  }
}

// Tests addition of billing_address_id to server_card_metadata and
// has_converted to server_profile_metadata and tests that the
// billing_address_id values were moved from the masked_credit_cards table to
// the server_card_metadata table.
TEST_F(WebDatabaseMigrationTest, MigrateVersion70ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_70.sql")));

  // Verify pre-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 70, 70));

    EXPECT_FALSE(connection.DoesColumnExist("server_card_metadata",
                                            "billing_address_id"));
    EXPECT_FALSE(
        connection.DoesColumnExist("server_address_metadata", "has_converted"));
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    // The billing_address_id column should have moved from masked_credit_cards
    // to server_card_metadata.
    EXPECT_FALSE(connection.DoesColumnExist("masked_credit_cards",
                                            "billing_address_id"));
    EXPECT_TRUE(connection.DoesColumnExist("server_card_metadata",
                                           "billing_address_id"));

    // The has_converted column should have been added in
    // server_address_metadata.
    EXPECT_TRUE(
        connection.DoesColumnExist("server_address_metadata", "has_converted"));

    // Make sure that the billing_address_id was moved from the
    // masked_credit_cards table to the server_card_metadata table. The values
    // are added to the table in version_70.sql.
    sql::Statement s_cards_metadata(connection.GetUniqueStatement(
        "SELECT id, billing_address_id FROM server_card_metadata"));
    ASSERT_TRUE(s_cards_metadata.Step());
    EXPECT_EQ("card_1", s_cards_metadata.ColumnString(0));
    EXPECT_EQ("address_1", s_cards_metadata.ColumnString(1));

    // Make sure that the has_converted column was set to false.
    sql::Statement s_addresses_metadata(connection.GetUniqueStatement(
        "SELECT id, has_converted FROM server_address_metadata"));
    ASSERT_TRUE(s_addresses_metadata.Step());
    EXPECT_EQ("address_1", s_addresses_metadata.ColumnString(0));
    EXPECT_FALSE(s_addresses_metadata.ColumnBool(1));

    // Make sure that the values in masked_credit_cards are still present except
    // for the billing_address_id. The values are added to the table in
    // version_70.sql.
    sql::Statement s_masked_cards(
        connection.GetUniqueStatement("SELECT id, status, name_on_card, "
                                      "network, last_four, exp_month, exp_year "
                                      "FROM masked_credit_cards"));
    ASSERT_TRUE(s_masked_cards.Step());
    EXPECT_EQ("card_1", s_masked_cards.ColumnString(0));
    EXPECT_EQ("status", s_masked_cards.ColumnString(1));
    EXPECT_EQ("bob", s_masked_cards.ColumnString(2));
    EXPECT_EQ("VISA", s_masked_cards.ColumnString(3));
    EXPECT_EQ("1234", s_masked_cards.ColumnString(4));
    EXPECT_EQ(12, s_masked_cards.ColumnInt(5));
    EXPECT_EQ(2050, s_masked_cards.ColumnInt(6));
  }
}

// Tests renaming "type" column into "network" for the "masked_credit_cards"
// table.
TEST_F(WebDatabaseMigrationTest, MigrateVersion71ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_71.sql")));

  // Verify pre-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 71, 71));

    EXPECT_TRUE(connection.DoesColumnExist("masked_credit_cards", "type"));
    EXPECT_FALSE(connection.DoesColumnExist("masked_credit_cards", "network"));

    EXPECT_TRUE(
        connection.Execute("INSERT INTO masked_credit_cards(id, type) "
                           "VALUES ('id', 'VISA')"));
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    // Don't check for absence of "type", because that's added in version 73
    // with a different meaning.
    EXPECT_TRUE(connection.DoesColumnExist("masked_credit_cards", "network"));

    sql::Statement s_cards_metadata(connection.GetUniqueStatement(
        "SELECT id, network FROM masked_credit_cards"));
    ASSERT_TRUE(s_cards_metadata.Step());
    EXPECT_EQ("id", s_cards_metadata.ColumnString(0));
    EXPECT_EQ("VISA", s_cards_metadata.ColumnString(1));
  }
}

// Tests addition of bank_name to masked_credit_cards
TEST_F(WebDatabaseMigrationTest, MigrateVersion72ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_72.sql")));

  // Verify pre-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 72, 72));

    EXPECT_FALSE(
        connection.DoesColumnExist("masked_credit_cards", "bank_name"));
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    // The bank_name column should exist.
    EXPECT_TRUE(connection.DoesColumnExist("masked_credit_cards", "bank_name"));

    // Make sure that the default bank name value is empty.
    sql::Statement s_masked_cards(connection.GetUniqueStatement(
        "SELECT bank_name FROM masked_credit_cards"));
    ASSERT_TRUE(s_masked_cards.Step());
    EXPECT_EQ("", s_masked_cards.ColumnString(0));
  }
}

// Tests adding "type" column for the "masked_credit_cards" table.
TEST_F(WebDatabaseMigrationTest, MigrateVersion73ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_73.sql")));

  // Verify pre-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 73, 73));

    EXPECT_FALSE(connection.DoesColumnExist("masked_credit_cards", "type"));

    EXPECT_TRUE(
        connection.Execute("INSERT INTO masked_credit_cards(id, network) "
                           "VALUES ('id', 'VISA')"));
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    EXPECT_TRUE(connection.DoesColumnExist("masked_credit_cards", "type"));

    sql::Statement cards(connection.GetUniqueStatement(
        "SELECT id, network, type FROM masked_credit_cards"));
    ASSERT_TRUE(cards.Step());
    EXPECT_EQ("id", cards.ColumnString(0));
    EXPECT_EQ("VISA", cards.ColumnString(1));
    EXPECT_EQ(CreditCard::CARD_TYPE_UNKNOWN, cards.ColumnInt(2));
  }
}

// Tests that version 73 with "type" column instead of "bank_name" column can be
// migrated to version 74 with both of these columns. This is necessary to
// verify that the version 73 collision resolves itself.
TEST_F(WebDatabaseMigrationTest, MigrateVersion73WithTypeColumnToCurrent) {
  ASSERT_NO_FATAL_FAILURE(
      LoadDatabase(FILE_PATH_LITERAL("version_73_with_type_column.sql")));

  // Verify pre-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 73, 73));

    EXPECT_FALSE(
        connection.DoesColumnExist("masked_credit_cards", "bank_name"));
    EXPECT_TRUE(connection.DoesColumnExist("masked_credit_cards", "type"));

    EXPECT_TRUE(connection.Execute(
        "INSERT INTO masked_credit_cards (type) VALUES (2)"));
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    // The bank_name column should exist.
    EXPECT_TRUE(connection.DoesColumnExist("masked_credit_cards", "bank_name"));

    // The type column should exist.
    EXPECT_TRUE(connection.DoesColumnExist("masked_credit_cards", "type"));

    // Make sure that the existing value of the type column is preserved.
    sql::Statement s_masked_cards(
        connection.GetUniqueStatement("SELECT type FROM masked_credit_cards"));
    ASSERT_TRUE(s_masked_cards.Step());
    EXPECT_EQ(2, s_masked_cards.ColumnInt(0));
  }
}

// Tests adding "validity_bitfield" column for the "autofill_profiles" table.
TEST_F(WebDatabaseMigrationTest, MigrateVersion74ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_74.sql")));

  // Verify pre-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 74, 74));

    EXPECT_FALSE(
        connection.DoesColumnExist("autofill_profiles", "validity_bitfield"));
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    EXPECT_TRUE(
        connection.DoesColumnExist("autofill_profiles", "validity_bitfield"));

    // Data should have been preserved. Validity bitfield should have been set
    // to 0.
    sql::Statement s_profiles(connection.GetUniqueStatement(
        "SELECT guid, company_name, street_address, dependent_locality,"
        " city, state, zipcode, sorting_code, country_code, date_modified,"
        " origin, language_code, validity_bitfield "
        "FROM autofill_profiles"));

    ASSERT_TRUE(s_profiles.Step());
    EXPECT_EQ("00000000-0000-0000-0000-000000000001",
              s_profiles.ColumnString(0));
    EXPECT_EQ(ASCIIToUTF16("Google Inc"), s_profiles.ColumnString16(1));
    EXPECT_EQ(ASCIIToUTF16("340 Main St"), s_profiles.ColumnString16(2));
    EXPECT_EQ(base::string16(), s_profiles.ColumnString16(3));
    EXPECT_EQ(ASCIIToUTF16("Los Angeles"), s_profiles.ColumnString16(4));
    EXPECT_EQ(ASCIIToUTF16("CA"), s_profiles.ColumnString16(5));
    EXPECT_EQ(ASCIIToUTF16("90291"), s_profiles.ColumnString16(6));
    EXPECT_EQ(base::string16(), s_profiles.ColumnString16(7));
    EXPECT_EQ(ASCIIToUTF16("US"), s_profiles.ColumnString16(8));
    EXPECT_EQ(1395948829, s_profiles.ColumnInt(9));
    EXPECT_EQ(ASCIIToUTF16(autofill::kSettingsOrigin),
              s_profiles.ColumnString16(10));
    EXPECT_EQ("en", s_profiles.ColumnString(11));
    // The new validity bitfield should have the default value of 0.
    EXPECT_EQ(0, s_profiles.ColumnInt(12));

    // No more entries expected.
    ASSERT_FALSE(s_profiles.Step());
  }
}

// Tests deletion of Instant-related columns in keywords table.
TEST_F(WebDatabaseMigrationTest, MigrateVersion75ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_75.sql")));

  // Verify pre-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 75, 75));

    EXPECT_TRUE(connection.DoesColumnExist("keywords", "instant_url"));
    EXPECT_TRUE(
        connection.DoesColumnExist("keywords", "instant_url_post_params"));
    EXPECT_TRUE(
        connection.DoesColumnExist("keywords", "search_terms_replacement_key"));
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    EXPECT_FALSE(connection.DoesColumnExist("keywords", "instant_url"));
    EXPECT_FALSE(
        connection.DoesColumnExist("keywords", "instant_url_post_params"));
    EXPECT_FALSE(
        connection.DoesColumnExist("keywords", "search_terms_replacement_key"));
  }
}

// Tests changing format of three timestamp columns inside keywords.
TEST_F(WebDatabaseMigrationTest, MigrateVersion76ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_76.sql")));

  // Verify pre-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 76, 76));

    sql::Statement s(connection.GetUniqueStatement(
        "SELECT id, date_created, last_modified, last_visited FROM keywords"));
    ASSERT_TRUE(s.Step());
    EXPECT_EQ(2, s.ColumnInt64(0));
    EXPECT_EQ(123, s.ColumnInt64(1));
    EXPECT_EQ(456, s.ColumnInt64(2));
    EXPECT_EQ(789, s.ColumnInt64(3));
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    sql::Statement s(connection.GetUniqueStatement(
        "SELECT id, date_created, last_modified, last_visited FROM keywords"));
    ASSERT_TRUE(s.Step());
    EXPECT_EQ(2, s.ColumnInt64(0));
    EXPECT_EQ(11644473723000000, s.ColumnInt64(1));
    EXPECT_EQ(11644474056000000, s.ColumnInt64(2));
    EXPECT_EQ(11644474389000000, s.ColumnInt64(3));
  }
}

// Tests adding model_type columns into autofill_sync_metadata and
// autofill_model_type_state.
TEST_F(WebDatabaseMigrationTest, MigrateVersion77ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_77.sql")));

  // Verify pre-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 77, 77));

    sql::Statement s1(connection.GetUniqueStatement(
        "SELECT storage_key, value FROM autofill_sync_metadata"));
    ASSERT_TRUE(s1.Step());
    EXPECT_EQ("storage_key1", s1.ColumnString(0));
    EXPECT_EQ("blob1", s1.ColumnString(1));
    ASSERT_TRUE(s1.Step());
    EXPECT_EQ("storage_key2", s1.ColumnString(0));
    EXPECT_EQ("blob2", s1.ColumnString(1));

    sql::Statement s2(connection.GetUniqueStatement(
        "SELECT id, value FROM autofill_model_type_state"));
    ASSERT_TRUE(s2.Step());
    EXPECT_EQ(1, s2.ColumnInt(0));
    EXPECT_EQ("state", s2.ColumnString(1));
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    // Note: The migration to version 78 (which added the model_type column)
    // used the wrong integer ID for the model_type. As a consequence, the later
    // migration to version 81 deletes all the badly-migrated data (at that
    // point, it'll most likely have been re-downloaded and stored under the
    // correct ID, so no point in trying to salvage anything). As a consequence,
    // there should now be no sync metadata in the database.
    sql::Statement s1(connection.GetUniqueStatement(
        "SELECT model_type, storage_key, value FROM autofill_sync_metadata"));
    EXPECT_FALSE(s1.Step());

    // The same applies for model type state.
    sql::Statement s2(connection.GetUniqueStatement(
        "SELECT model_type, value FROM autofill_model_type_state"));
    EXPECT_FALSE(s2.Step());
  }
}

TEST_F(WebDatabaseMigrationTest, MigrateVersion78ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_78.sql")));

  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    ASSERT_TRUE(connection.DoesTableExist("ie7_logins"));
    ASSERT_TRUE(connection.DoesTableExist("logins"));
  }

  DoMigration();

  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    ASSERT_FALSE(connection.DoesTableExist("ie7_logins"));
    ASSERT_FALSE(connection.DoesTableExist("logins"));
  }
}

// Tests adding "is_client_validity_states_updated" column for the
// "autofill_profiles" table.
TEST_F(WebDatabaseMigrationTest, MigrateVersion79ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_79.sql")));

  // Verify pre-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));
    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 79, 79));
    EXPECT_FALSE(connection.DoesColumnExist(
        "autofill_profiles", "is_client_validity_states_updated"));
  }
  DoMigration();
  // Verify post-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));
    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));
    EXPECT_TRUE(connection.DoesColumnExist(
        "autofill_profiles", "is_client_validity_states_updated"));
    // Data should have been preserved. Validity
    // is_client_validity_states_updated should have been set to false.
    sql::Statement s_profiles(connection.GetUniqueStatement(
        "SELECT guid, company_name, street_address, dependent_locality,"
        " city, state, zipcode, sorting_code, country_code, date_modified,"
        " origin, language_code, validity_bitfield, "
        " is_client_validity_states_updated "
        " FROM autofill_profiles"));
    ASSERT_TRUE(s_profiles.Step());
    EXPECT_EQ("00000000-0000-0000-0000-000000000001",
              s_profiles.ColumnString(0));
    EXPECT_EQ(ASCIIToUTF16("Google Inc"), s_profiles.ColumnString16(1));
    EXPECT_EQ(ASCIIToUTF16("340 Main St"), s_profiles.ColumnString16(2));
    EXPECT_EQ(base::string16(), s_profiles.ColumnString16(3));
    EXPECT_EQ(ASCIIToUTF16("Los Angeles"), s_profiles.ColumnString16(4));
    EXPECT_EQ(ASCIIToUTF16("CA"), s_profiles.ColumnString16(5));
    EXPECT_EQ(ASCIIToUTF16("90291"), s_profiles.ColumnString16(6));
    EXPECT_EQ(base::string16(), s_profiles.ColumnString16(7));
    EXPECT_EQ(ASCIIToUTF16("US"), s_profiles.ColumnString16(8));
    EXPECT_EQ(1395948829, s_profiles.ColumnInt(9));
    EXPECT_EQ(ASCIIToUTF16(autofill::kSettingsOrigin),
              s_profiles.ColumnString16(10));
    EXPECT_EQ("en", s_profiles.ColumnString(11));
    EXPECT_EQ(1365, s_profiles.ColumnInt(12));
    // The new is_client_validity_states_updated should have the default value
    // of FALSE.
    EXPECT_FALSE(s_profiles.ColumnBool(13));

    // No more entries expected.
    ASSERT_FALSE(s_profiles.Step());
  }
}

TEST_F(WebDatabaseMigrationTest, MigrateVersion80ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_80.sql")));

  // Verify pre-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 80, 79));

    sql::Statement s1(connection.GetUniqueStatement(
        "SELECT model_type, storage_key, value FROM autofill_sync_metadata"));
    ASSERT_TRUE(s1.Step());
    // Note: This is the *wrong* ID for AUTOFILL, simulating the botched
    // migration in version 78. See crbug.com/895826.
    ASSERT_EQ(
        static_cast<int>(syncer::ModelTypeHistogramValue(syncer::AUTOFILL)),
        s1.ColumnInt(0));
    ASSERT_EQ("storage_key1", s1.ColumnString(1));
    ASSERT_EQ("blob1", s1.ColumnString(2));

    ASSERT_TRUE(s1.Step());
    // Note: This is the *correct* ID for AUTOFILL, simulating the data that got
    // redownloaded after the bad migration, and stored under the correct ID.
    ASSERT_EQ(syncer::ModelTypeToStableIdentifier(syncer::AUTOFILL),
              s1.ColumnInt(0));
    ASSERT_EQ("storage_key2", s1.ColumnString(1));
    ASSERT_EQ("blob2", s1.ColumnString(2));

    sql::Statement s2(connection.GetUniqueStatement(
        "SELECT model_type, value FROM autofill_model_type_state"));
    ASSERT_TRUE(s2.Step());
    // Like above: Bad value.
    ASSERT_EQ(
        static_cast<int>(syncer::ModelTypeHistogramValue(syncer::AUTOFILL)),
        s2.ColumnInt(0));
    ASSERT_EQ("state1", s2.ColumnString(1));
    ASSERT_TRUE(s2.Step());
    // Good value.
    ASSERT_EQ(syncer::ModelTypeToStableIdentifier(syncer::AUTOFILL),
              s2.ColumnInt(0));
    ASSERT_EQ("state2", s2.ColumnString(1));
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    // Check that the badly-migrated AUTOFILL data from version 78 is gone, but
    // the correct redownloaded data is still here.
    sql::Statement s1(connection.GetUniqueStatement(
        "SELECT model_type, storage_key, value FROM autofill_sync_metadata"));
    ASSERT_TRUE(s1.Step());
    EXPECT_EQ(syncer::ModelTypeToStableIdentifier(syncer::AUTOFILL),
              s1.ColumnInt(0));
    EXPECT_EQ("storage_key2", s1.ColumnString(1));
    EXPECT_EQ("blob2", s1.ColumnString(2));
    EXPECT_FALSE(s1.Step());

    // Check that the badly-migrated AUTOFILL model type state from version 78
    // is gone, but the correct redownloaded state is still here.
    sql::Statement s2(connection.GetUniqueStatement(
        "SELECT model_type, value FROM autofill_model_type_state"));
    ASSERT_TRUE(s2.Step());
    EXPECT_EQ(syncer::ModelTypeToStableIdentifier(syncer::AUTOFILL),
              s2.ColumnInt(0));
    EXPECT_EQ("state2", s2.ColumnString(1));
    EXPECT_FALSE(s2.Step());
  }
}

// Tests addition of created_from_play_api column in keywords table.
TEST_F(WebDatabaseMigrationTest, MigrateVersion81ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_81.sql")));

  // Verify pre-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 81, 79));

    EXPECT_FALSE(
        connection.DoesColumnExist("keywords", "created_from_play_api"));
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    EXPECT_TRUE(
        connection.DoesColumnExist("keywords", "created_from_play_api"));
  }
}
