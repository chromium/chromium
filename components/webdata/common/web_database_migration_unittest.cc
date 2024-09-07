// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/webdata/addresses/address_autofill_table.h"
#include "components/autofill/core/browser/webdata/autocomplete/autocomplete_table.h"
#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/autofill/core/browser/webdata/autofill_sync_metadata_table.h"
#include "components/autofill/core/browser/webdata/payments/payments_autofill_table.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/plus_addresses/webdata/plus_address_table.h"
#include "components/search_engines/keyword_table.h"
#include "components/signin/public/webdata/token_service_table.h"
#include "components/webdata/common/web_database.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// To make the comparison with golden files less whitespace sensitive:
// - Remove SQLite quotes: http://www.sqlite.org/lang_keywords.html.
// - Collapse multiple spaces into one.
// - Ensure that there is no space before or after ',', '(' or ')'.
std::string NormalizeSchemaForComparison(const std::string& schema) {
  std::string normalized;
  normalized.reserve(schema.size());
  bool skip_following_spaces = false;
  for (char c : schema) {
    if (base::Contains("\"[]`", c))  // Quotes
      continue;
    if (c == ' ' && skip_following_spaces)
      continue;
    bool is_separator = base::Contains(",()", c);
    if (is_separator && !normalized.empty() && normalized.back() == ' ')
      normalized.pop_back();
    normalized.push_back(c);
    skip_following_spaces = c == ' ' || is_separator;
  }
  return normalized;
}

}  // anonymous namespace

// The WebDatabaseMigrationTest encapsulates testing of database migrations.
// Specifically, these tests are intended to exercise any schema changes in
// the WebDatabase and data migrations that occur in
// `WebDatabase::MigrateOldVersionsAsNeeded()` (most likely through one of the
// `WebDatabaseTable::MigrateToVersion()` overrides).
//
// When bumping `WebDatabase::kCurrentVersionNumber`, add a new
// `MigrateVersionXXToCurrent` test below and generate a new version_XX.sql
// file, following the instructions from the `VersionXxSqlFilesAreGolden` test
// description.
class WebDatabaseMigrationTest : public testing::Test {
 public:
  WebDatabaseMigrationTest() = default;

  WebDatabaseMigrationTest(const WebDatabaseMigrationTest&) = delete;
  WebDatabaseMigrationTest& operator=(const WebDatabaseMigrationTest&) = delete;

  ~WebDatabaseMigrationTest() override = default;

  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  // Load the database via the WebDatabase class and migrate the database to
  // the current version.
  void DoMigration() {
    autofill::AddressAutofillTable address_autofill_table;
    autofill::AutocompleteTable autocomplete_table;
    autofill::AutofillSyncMetadataTable autofill_sync_metadata_table;
    autofill::PaymentsAutofillTable payments_autofill_table;
    KeywordTable keyword_table;
    plus_addresses::PlusAddressTable plus_address_table;
    TokenServiceTable token_service_table;

    WebDatabase db;
    db.AddTable(&address_autofill_table);
    db.AddTable(&autocomplete_table);
    db.AddTable(&autofill_sync_metadata_table);
    db.AddTable(&payments_autofill_table);
    db.AddTable(&keyword_table);
    db.AddTable(&plus_address_table);
    db.AddTable(&token_service_table);

    // This causes the migration to occur.
    ASSERT_EQ(sql::INIT_OK, db.Init(GetDatabasePath()));
  }

 protected:
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
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_path);
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
};

void WebDatabaseMigrationTest::LoadDatabase(
    const base::FilePath::StringType& file) {
  std::string contents;
  ASSERT_TRUE(GetWebDatabaseData(base::FilePath(file), &contents));

  sql::Database connection;
  ASSERT_TRUE(connection.Open(GetDatabasePath()));
  ASSERT_TRUE(connection.Execute(contents));
}

// Tests that migrating from the golden files version_XX.sql results in the same
// schema as migrating from an empty database.
//
// Whenever `WebDatabase::kCurrentVersionNumber` is updated to X, add a new
// version_X.sql file to components/test/data/web_database/.
//
// There are generally two ways of doing so:
// - Copy version_X-1.sql. Update the version to X and make any changes that
//   were made in version X (new tables, columns, etc).
// - Generate the file from scratch:
//   1. Launch Chrome with WebDatabase version X.
//      ./out/Default/chrome --user-data-dir=/tmp/sql
//      No need to complete the first run -- closing Chrome immediately is fine.
//   2. Run sqlite3 '/tmp/sql/Default/Web Data'
//        .output version_X.sql
//        .dump
//        .exit
//   3. Remove any INSERT statements to tables other than "meta" from
//      version_X.sql.
TEST_F(WebDatabaseMigrationTest, VersionXxSqlFilesAreGolden) {
  DoMigration();

  // Initialize the database and retrieve the initial schema. The database needs
  // to be closed.
  const base::FilePath db_path = GetDatabasePath();
  std::string expected_schema;
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(db_path));
    expected_schema = connection.GetSchema();
    ASSERT_TRUE(connection.Raze());
  }

  for (int i = WebDatabase::kDeprecatedVersionNumber + 1;
       i <= WebDatabase::kCurrentVersionNumber; ++i) {
    SCOPED_TRACE(testing::Message() << "DB Version: " << i);
    const base::FilePath file_name = base::FilePath::FromUTF8Unsafe(
        "version_" + base::NumberToString(i) + ".sql");
    ASSERT_NO_FATAL_FAILURE(LoadDatabase(file_name.value()))
        << "Failed to load " << file_name.MaybeAsASCII();
    {
      // Check that the database file contains the right version.
      sql::Database connection;
      ASSERT_TRUE(connection.Open(GetDatabasePath()));
      EXPECT_EQ(i, VersionFromConnection(&connection)) << "For version " << i;
    }

    DoMigration();

    {
      sql::Database connection;
      ASSERT_TRUE(connection.Open(db_path));
      EXPECT_EQ(NormalizeSchemaForComparison(expected_schema),
                NormalizeSchemaForComparison(connection.GetSchema()))
          << "For version " << i;
      ASSERT_TRUE(connection.Raze());
    }
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
    EXPECT_EQ(WebDatabase::kCurrentVersionNumber,
              VersionFromConnection(&connection));

    // Check that expected tables are present.
    EXPECT_TRUE(connection.DoesTableExist("autofill"));
    EXPECT_TRUE(connection.DoesTableExist("addresses"));
    EXPECT_TRUE(connection.DoesTableExist("credit_cards"));
    EXPECT_TRUE(connection.DoesTableExist("local_ibans"));
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

// Versions below 83 are deprecated. This verifies that old databases are razed.
TEST_F(WebDatabaseMigrationTest, RazeDeprecatedVersionAndReinit) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_82.sql")));

  // Verify pre-conditions. These are expectations for version 82 of the
  // database.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 82, 79));

    EXPECT_TRUE(connection.DoesColumnExist("masked_credit_cards", "type"));
  }

  DoMigration();

  // Check post-conditions of version 104. This ensures that the migration has
  // happened.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(WebDatabase::kCurrentVersionNumber,
              VersionFromConnection(&connection));

    // The product_description column and should exist.
    EXPECT_TRUE(connection.DoesColumnExist("masked_credit_cards",
                                           "product_description"));
  }
}

// Tests addition of nickname column in masked_credit_cards table.
TEST_F(WebDatabaseMigrationTest, MigrateVersion83ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_83.sql")));

  // Verify pre-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 83, 79));

    EXPECT_FALSE(connection.DoesColumnExist("masked_credit_cards", "nickname"));
    ASSERT_TRUE(connection.ExecuteScriptForTesting(R"(
      INSERT INTO masked_credit_cards (id, status, name_on_card, network,
      last_four, exp_month, exp_year, bank_name)
      VALUES ('card_1', 'status', 'bob', 'VISA', '1234', 12, 2050, 'Chase');
    )"));
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(WebDatabase::kCurrentVersionNumber,
              VersionFromConnection(&connection));

    // The nickname column should exist.
    EXPECT_TRUE(connection.DoesColumnExist("masked_credit_cards", "nickname"));

    // Make sure that the default nickname value is empty.
    sql::Statement s_masked_cards(connection.GetUniqueStatement(
        "SELECT nickname FROM masked_credit_cards"));
    ASSERT_TRUE(s_masked_cards.Step());
    EXPECT_EQ("", s_masked_cards.ColumnString(0));
  }
}

// Tests addition of card_issuer column in masked_credit_cards table.
TEST_F(WebDatabaseMigrationTest, MigrateVersion84ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_84.sql")));

  // Verify pre-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 84, 79));

    EXPECT_FALSE(
        connection.DoesColumnExist("masked_credit_cards", "card_issuer"));
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(WebDatabase::kCurrentVersionNumber,
              VersionFromConnection(&connection));

    // The card_issuer column should exist.
    EXPECT_TRUE(
        connection.DoesColumnExist("masked_credit_cards", "card_issuer"));
  }
}

// Tests addition of nickname column in credit_cards table.
TEST_F(WebDatabaseMigrationTest, MigrateVersion86ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_86.sql")));

  // Verify pre-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 86, 83));

    EXPECT_FALSE(connection.DoesColumnExist("credit_cards", "nickname"));
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(WebDatabase::kCurrentVersionNumber,
              VersionFromConnection(&connection));

    // The nickname column should exist.
    EXPECT_TRUE(connection.DoesColumnExist("credit_cards", "nickname"));
  }
}

// Version 87 added new columns to the autofill_profile_names table. This table
// was since deprecated and replaced by local_profiles. The migration unit test
// to the current version thus no longer applies.

// Tests addition of instrument_id column in masked_credit_cards table.
TEST_F(WebDatabaseMigrationTest, MigrateVersion88ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_88.sql")));

  // Verify pre-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 88, 83));

    EXPECT_FALSE(
        connection.DoesColumnExist("masked_credit_cards", "instrument_id"));
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(WebDatabase::kCurrentVersionNumber,
              VersionFromConnection(&connection));

    // The instrument_id column should exist.
    EXPECT_TRUE(
        connection.DoesColumnExist("masked_credit_cards", "instrument_id"));
  }
}

// Tests addition of promo code and display strings columns in offer_data table.
TEST_F(WebDatabaseMigrationTest, MigrateVersion93ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_93.sql")));

  // Verify pre-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 93, 83));

    EXPECT_FALSE(connection.DoesColumnExist("offer_data", "promo_code"));
    EXPECT_FALSE(connection.DoesColumnExist("offer_data", "value_prop_text"));
    EXPECT_FALSE(connection.DoesColumnExist("offer_data", "see_details_text"));
    EXPECT_FALSE(
        connection.DoesColumnExist("offer_data", "usage_instructions_text"));
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(WebDatabase::kCurrentVersionNumber,
              VersionFromConnection(&connection));

    // The new offer_data columns should exist.
    EXPECT_TRUE(connection.DoesColumnExist("offer_data", "promo_code"));
    EXPECT_TRUE(connection.DoesColumnExist("offer_data", "value_prop_text"));
    EXPECT_TRUE(connection.DoesColumnExist("offer_data", "see_details_text"));
    EXPECT_TRUE(
        connection.DoesColumnExist("offer_data", "usage_instructions_text"));
  }
}

// Tests addition of virtual_card_enrollment_state and card_art_url columns in
// masked_credit_cards table.
TEST_F(WebDatabaseMigrationTest, MigrateVersion94ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_94.sql")));

  // Verify pre-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 94, 83));

    EXPECT_FALSE(connection.DoesColumnExist("masked_credit_cards",
                                            "virtual_card_enrollment_state"));
    EXPECT_FALSE(
        connection.DoesColumnExist("masked_credit_cards", "card_art_url"));
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(WebDatabase::kCurrentVersionNumber,
              VersionFromConnection(&connection));

    // The virtual_card_enrollment_state column and the card_art_url column
    // should exist.
    EXPECT_TRUE(connection.DoesColumnExist("masked_credit_cards",
                                           "virtual_card_enrollment_state"));
    EXPECT_TRUE(
        connection.DoesColumnExist("masked_credit_cards", "card_art_url"));
  }
}

// Version 95 added a new column to the autofill_profile table. This table
// was since deprecated and replaced by local_profiles. The migration unit test
// to the current version thus no longer applies.

// Tests addition of is_active column in keywords table.
TEST_F(WebDatabaseMigrationTest, MigrateVersion96ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_96.sql")));

  // Verify pre-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 96, 83));

    EXPECT_FALSE(connection.DoesColumnExist("keywords", "is_active"));
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(WebDatabase::kCurrentVersionNumber,
              VersionFromConnection(&connection));

    EXPECT_TRUE(connection.DoesColumnExist("keywords", "is_active"));
  }
}

TEST_F(WebDatabaseMigrationTest, MigrateVersion97ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_97.sql")));

  // Verify pre-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 97, 83));

    // The status column should exist.
    EXPECT_TRUE(connection.DoesColumnExist("masked_credit_cards", "status"));
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(WebDatabase::kCurrentVersionNumber,
              VersionFromConnection(&connection));

    // The status column should not exist.
    EXPECT_FALSE(connection.DoesColumnExist("masked_credit_cards", "status"));
  }
}

TEST_F(WebDatabaseMigrationTest, MigrateVersion98ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_98.sql")));

  // Verify pre-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 98, 98));

    // The autofill_profiles_trash table should exist.
    EXPECT_TRUE(connection.DoesTableExist("autofill_profiles_trash"));
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(WebDatabase::kCurrentVersionNumber,
              VersionFromConnection(&connection));
    // The autofill_profiles_trash table should not exist.
    EXPECT_FALSE(connection.DoesTableExist("autofill_profiles_trash"));
  }
}

// Version 99 removed columns from the autofill_profile_names table. This table
// was since deprecated and replaced by local_profiles. The migration unit test
// to the current version thus no longer applies.

TEST_F(WebDatabaseMigrationTest, MigrateVersion100ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_100.sql")));

  // Verify pre-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 100, 99));

    // The validity-related columns should exist.
    EXPECT_TRUE(connection.DoesTableExist("credit_card_art_images"));
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(WebDatabase::kCurrentVersionNumber,
              VersionFromConnection(&connection));

    EXPECT_FALSE(connection.DoesTableExist("credit_card_art_images"));
  }
}

// Version 101 added a new table autofill_profiles_birthdates. This table was
// since deprecated and replaced by local_profiles. The migration unit test
// to the current version thus no longer applies.

// Tests addition of starter_pack_id column in keywords table.
TEST_F(WebDatabaseMigrationTest, MigrateVersion102ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_102.sql")));

  // Verify pre-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 102, 99));

    EXPECT_FALSE(connection.DoesColumnExist("keywords", "starter_pack_id"));
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(WebDatabase::kCurrentVersionNumber,
              VersionFromConnection(&connection));

    EXPECT_TRUE(connection.DoesColumnExist("keywords", "starter_pack_id"));
  }
}

// Tests addition of product_description in masked_credit_cards table.
TEST_F(WebDatabaseMigrationTest, MigrateVersion103ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_103.sql")));

  // Verify pre-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 103, 99));

    EXPECT_FALSE(connection.DoesColumnExist("masked_credit_cards",
                                            "product_description"));
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(WebDatabase::kCurrentVersionNumber,
              VersionFromConnection(&connection));

    // The product_description column and should exist.
    EXPECT_TRUE(connection.DoesColumnExist("masked_credit_cards",
                                           "product_description"));
  }
}

// Tests addition of new table 'local_ibans'.
TEST_F(WebDatabaseMigrationTest, MigrateVersion104ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_104.sql")));

  // Verify pre-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(104, VersionFromConnection(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 104, 100));

    // The ibans table should not exist.
    EXPECT_FALSE(connection.DoesTableExist("ibans"));
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(WebDatabase::kCurrentVersionNumber,
              VersionFromConnection(&connection));

    // The local_ibans table should exist.
    EXPECT_TRUE(connection.DoesTableExist("local_ibans"));
  }
}

// Tests addition of new table 'ibans' with guid as PRIMARY KEY.
TEST_F(WebDatabaseMigrationTest, MigrateVersion105ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_105.sql")));

  // Verify pre-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(105, VersionFromConnection(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 105, 100));

    // The ibans table should exist, but should not have been created with guid
    // as PRIMARY KEY.
    ASSERT_TRUE(connection.DoesTableExist("ibans"));
    ASSERT_EQ(connection.GetSchema().find(
                  "CREATE TABLE ibans (guid VARCHAR PRIMARY KEY"),
              std::string::npos);
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(WebDatabase::kCurrentVersionNumber,
              VersionFromConnection(&connection));

    // The local_ibans table should exist with guid as primary key.
    EXPECT_TRUE(connection.DoesTableExist("local_ibans"));
    ASSERT_NE(connection.GetSchema().find(
                  "CREATE TABLE \"local_ibans\" (guid VARCHAR PRIMARY KEY"),
              std::string::npos);
  }
}

// Version 106 added new contact_info and contact_info_types tables. These
// tables were since deprecated and replaced by addresses and
// address_type_tokens. The migration unit test to the current version thus no
// longer applies.

// Tests addition of card_isser_id in masked_credit_cards table.
TEST_F(WebDatabaseMigrationTest, MigrateVersion107ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_107.sql")));

  // Verify pre-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 107, 106));

    EXPECT_FALSE(
        connection.DoesColumnExist("masked_credit_cards", "card_issuer_id"));
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(WebDatabase::kCurrentVersionNumber,
              VersionFromConnection(&connection));

    // The card_issuer_id column and should exist.
    EXPECT_TRUE(
        connection.DoesColumnExist("masked_credit_cards", "card_issuer_id"));
  }
}

// Tests verifying the Virtual Card Usage Data table is created.
TEST_F(WebDatabaseMigrationTest, MigrateVersion108ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_108.sql")));

  // Verify pre-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(108, VersionFromConnection(&connection));

    // The virtual_card_usage_data table should not exist.
    EXPECT_FALSE(connection.DoesTableExist("virtual_card_usage_data"));
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(WebDatabase::kCurrentVersionNumber,
              VersionFromConnection(&connection));

    // The virtual_card_usage_data tables should exist.
    EXPECT_TRUE(connection.DoesTableExist("virtual_card_usage_data"));
  }
}

// Version 109 added new columns to the contact_info table. This table was since
// deprecated and replaced by addresses. The migration unit test to the current
// version thus no longer applies.

// Tests that the virtual_card_enrollment_type column is added to the
// masked_credit_cards table.
TEST_F(WebDatabaseMigrationTest, MigrateVersion110ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_110.sql")));
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    EXPECT_EQ(110, VersionFromConnection(&connection));
    EXPECT_FALSE(connection.DoesColumnExist("masked_credit_cards",
                                            "virtual_card_enrollment_type"));
  }
  DoMigration();
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    EXPECT_EQ(WebDatabase::kCurrentVersionNumber,
              VersionFromConnection(&connection));
    EXPECT_TRUE(connection.DoesColumnExist("masked_credit_cards",
                                           "virtual_card_enrollment_type"));
  }
}

// Tests that the enforced_by_policy column is added to the keywords table.
TEST_F(WebDatabaseMigrationTest, MigrateVersion111ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_111.sql")));
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    EXPECT_EQ(111, VersionFromConnection(&connection));
    EXPECT_FALSE(connection.DoesColumnExist("keywords", "enforced_by_policy"));
  }
  DoMigration();
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    EXPECT_EQ(WebDatabase::kCurrentVersionNumber,
              VersionFromConnection(&connection));
    EXPECT_TRUE(connection.DoesColumnExist("keywords", "enforced_by_policy"));
  }
}

// Version 112 and 113 migrated autofill_profiles tables to local_address
// tables. Since the local_address tables have since been deprecated, the
// migration unit test to the current version no longer applies.

// Tests that the IBAN value column is encrypted in local_ibans table.
TEST_F(WebDatabaseMigrationTest, MigrateVersion114ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_114.sql")));
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    EXPECT_EQ(114, VersionFromConnection(&connection));
    EXPECT_TRUE(connection.DoesColumnExist("ibans", "value"));
  }
  DoMigration();
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    EXPECT_EQ(WebDatabase::kCurrentVersionNumber,
              VersionFromConnection(&connection));
    EXPECT_TRUE(connection.DoesColumnExist("local_ibans", "value_encrypted"));
    EXPECT_FALSE(connection.DoesColumnExist("local_ibans", "value"));
  }
}

// Tests verifying both stored_cvc tables are created.
TEST_F(WebDatabaseMigrationTest, MigrateVersion115ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_115.sql")));

  // Verify pre-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(115, VersionFromConnection(&connection));

    // The stored_cvc tables should not exist.
    EXPECT_FALSE(connection.DoesTableExist("local_stored_cvc"));
    EXPECT_FALSE(connection.DoesTableExist("server_stored_cvc"));
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(WebDatabase::kCurrentVersionNumber,
              VersionFromConnection(&connection));

    // The stored_cvc tables should exist.
    EXPECT_TRUE(connection.DoesTableExist("local_stored_cvc"));
    EXPECT_TRUE(connection.DoesTableExist("server_stored_cvc"));
  }
}

// Version 116 added new columns to the contact_info_type_tokens and
// local_addresses_type_tokens tables. These tables were since deprecated and
// replaced by address_type_tokens. The migration unit test to the current
// version thus no longer applies.

TEST_F(WebDatabaseMigrationTest, MigrateVersion117ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_117.sql")));
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    EXPECT_EQ(117, VersionFromConnection(&connection));
    EXPECT_TRUE(connection.DoesTableExist("payments_upi_vpa"));
  }
  DoMigration();
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    EXPECT_EQ(WebDatabase::kCurrentVersionNumber,
              VersionFromConnection(&connection));
    EXPECT_FALSE(connection.DoesTableExist("payments_upi_vpa"));
  }
}

// Tests addition of new tables 'masked_ibans' and `masked_iban_metadata`, also
// test that `ibans` has been renamed to `local_ibans`.
TEST_F(WebDatabaseMigrationTest, MigrateVersion118ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_118.sql")));

  // Verify pre-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));
    EXPECT_EQ(118, VersionFromConnection(&connection));

    EXPECT_FALSE(connection.DoesTableExist("masked_ibans"));
    EXPECT_FALSE(connection.DoesTableExist("masked_ibans_metadata"));
    EXPECT_TRUE(connection.DoesTableExist("ibans"));
    EXPECT_FALSE(connection.DoesTableExist("local_ibans"));
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(WebDatabase::kCurrentVersionNumber,
              VersionFromConnection(&connection));

    // The `masked_ibans` and `masked_iban_metadata` tables should exist.
    EXPECT_TRUE(connection.DoesTableExist("masked_ibans"));
    EXPECT_TRUE(connection.DoesTableExist("masked_ibans_metadata"));
    // The `ibans` table should be renamed to `local_ibans`.
    EXPECT_TRUE(connection.DoesTableExist("local_ibans"));
    EXPECT_FALSE(connection.DoesTableExist("ibans"));
  }
}

// Tests that the server_address* tables are dropped.
TEST_F(WebDatabaseMigrationTest, MigrateVersion120ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_120.sql")));
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    EXPECT_EQ(120, VersionFromConnection(&connection));
    EXPECT_TRUE(connection.DoesTableExist("server_addresses"));
    EXPECT_TRUE(connection.DoesTableExist("server_address_metadata"));
  }
  DoMigration();
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    EXPECT_EQ(WebDatabase::kCurrentVersionNumber,
              VersionFromConnection(&connection));
    EXPECT_FALSE(connection.DoesTableExist("server_addresses"));
    EXPECT_FALSE(connection.DoesTableExist("server_address_metadata"));
  }
}

// Tests that the `featured_by_policy` column is added to the keywords table.
TEST_F(WebDatabaseMigrationTest, MigrateVersion121ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_121.sql")));
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    EXPECT_EQ(121, VersionFromConnection(&connection));
    EXPECT_FALSE(connection.DoesColumnExist("keywords", "featured_by_policy"));
  }
  DoMigration();
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    EXPECT_EQ(WebDatabase::kCurrentVersionNumber,
              VersionFromConnection(&connection));
    EXPECT_TRUE(connection.DoesColumnExist("keywords", "featured_by_policy"));
  }
}

// Tests that the `product_terms_url` column is added to the
// `masked_credit_card` table, and the `masked_credit_card_benefits` and the
// `benefit_merchant_domains` tables are added.
TEST_F(WebDatabaseMigrationTest, MigrateVersion122ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_122.sql")));
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    EXPECT_EQ(122, VersionFromConnection(&connection));
    EXPECT_TRUE(connection.DoesTableExist("masked_credit_cards"));
    EXPECT_FALSE(
        connection.DoesColumnExist("masked_credit_cards", "product_terms_url"));
    EXPECT_FALSE(connection.DoesTableExist("masked_credit_card_benefits"));
    EXPECT_FALSE(connection.DoesTableExist("benefit_merchant_domains"));
  }
  DoMigration();
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    EXPECT_EQ(WebDatabase::kCurrentVersionNumber,
              VersionFromConnection(&connection));

    EXPECT_TRUE(connection.DoesTableExist("masked_credit_cards"));
    EXPECT_TRUE(
        connection.DoesColumnExist("masked_credit_cards", "product_terms_url"));

    EXPECT_TRUE(connection.DoesTableExist("masked_credit_card_benefits"));
    EXPECT_TRUE(connection.DoesColumnExist("masked_credit_card_benefits",
                                           "benefit_id"));
    EXPECT_TRUE(connection.DoesColumnExist("masked_credit_card_benefits",
                                           "instrument_id"));
    EXPECT_TRUE(connection.DoesColumnExist("masked_credit_card_benefits",
                                           "benefit_type"));
    EXPECT_TRUE(connection.DoesColumnExist("masked_credit_card_benefits",
                                           "benefit_category"));
    EXPECT_TRUE(connection.DoesColumnExist("masked_credit_card_benefits",
                                           "benefit_description"));
    EXPECT_TRUE(connection.DoesColumnExist("masked_credit_card_benefits",
                                           "start_time"));
    EXPECT_TRUE(
        connection.DoesColumnExist("masked_credit_card_benefits", "end_time"));

    EXPECT_TRUE(connection.DoesTableExist("benefit_merchant_domains"));
    EXPECT_TRUE(
        connection.DoesColumnExist("benefit_merchant_domains", "benefit_id"));
    EXPECT_TRUE(connection.DoesColumnExist("benefit_merchant_domains",
                                           "merchant_domain"));
  }
}

TEST_F(WebDatabaseMigrationTest, MigrateVersion123ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_123.sql")));
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(123, VersionFromConnection(&connection));

    EXPECT_TRUE(connection.DoesTableExist("payment_instruments"));
    EXPECT_TRUE(
        connection.DoesTableExist("payment_instrument_supported_rails"));
    EXPECT_TRUE(connection.DoesTableExist("payment_instruments_metadata"));
    EXPECT_TRUE(connection.DoesTableExist("bank_accounts"));
    EXPECT_FALSE(connection.DoesTableExist("masked_bank_accounts"));
    EXPECT_FALSE(connection.DoesTableExist("masked_bank_accounts_metadata"));
  }
  DoMigration();
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(WebDatabase::kCurrentVersionNumber,
              VersionFromConnection(&connection));

    EXPECT_FALSE(connection.DoesTableExist("payment_instruments"));
    EXPECT_FALSE(
        connection.DoesTableExist("payment_instrument_supported_rails"));
    EXPECT_FALSE(connection.DoesTableExist("payment_instruments_metadata"));
    EXPECT_FALSE(connection.DoesTableExist("bank_accounts"));
    EXPECT_TRUE(connection.DoesTableExist("masked_bank_accounts"));
    EXPECT_TRUE(connection.DoesTableExist("masked_bank_accounts_metadata"));
  }
}

TEST_F(WebDatabaseMigrationTest, MigrateVersion124ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_124.sql")));
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(124, VersionFromConnection(&connection));

    EXPECT_TRUE(connection.DoesTableExist("unmasked_credit_cards"));
  }
  DoMigration();
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(WebDatabase::kCurrentVersionNumber,
              VersionFromConnection(&connection));

    EXPECT_FALSE(connection.DoesTableExist("unmasked_credit_cards"));
  }
}

TEST_F(WebDatabaseMigrationTest, MigrateVersion125ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_125.sql")));
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    EXPECT_EQ(125, VersionFromConnection(&connection));
    EXPECT_FALSE(connection.DoesTableExist("plus_addresses"));
  }
  DoMigration();
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    EXPECT_EQ(WebDatabase::kCurrentVersionNumber,
              VersionFromConnection(&connection));
    EXPECT_TRUE(connection.DoesTableExist("plus_addresses"));
  }
}

TEST_F(WebDatabaseMigrationTest, MigrateVersion126ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_126.sql")));
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    EXPECT_EQ(126, VersionFromConnection(&connection));
    EXPECT_FALSE(connection.DoesColumnExist("plus_addresses", "profile_id"));
    EXPECT_FALSE(
        connection.DoesTableExist("plus_address_sync_model_type_state"));
    EXPECT_FALSE(
        connection.DoesTableExist("plus_address_sync_entity_metadata"));
  }
  DoMigration();
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    EXPECT_EQ(WebDatabase::kCurrentVersionNumber,
              VersionFromConnection(&connection));
    EXPECT_TRUE(connection.DoesColumnExist("plus_addresses", "profile_id"));
    EXPECT_TRUE(
        connection.DoesTableExist("plus_address_sync_model_type_state"));
    EXPECT_TRUE(connection.DoesTableExist("plus_address_sync_entity_metadata"));
  }
}

// Expect that version 128 altered the type plus_addresses' primary key column
// from INTEGER to VARCHAR.
TEST_F(WebDatabaseMigrationTest, MigrateVersion127ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_127.sql")));
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    EXPECT_EQ(127, VersionFromConnection(&connection));
    EXPECT_NE(
        connection.GetSchema().find(
            "CREATE TABLE plus_addresses (profile_id INTEGER PRIMARY KEY"),
        std::string::npos);
  }
  DoMigration();
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    EXPECT_EQ(WebDatabase::kCurrentVersionNumber,
              VersionFromConnection(&connection));
    EXPECT_NE(
        connection.GetSchema().find(
            "CREATE TABLE plus_addresses (profile_id VARCHAR PRIMARY KEY"),
        std::string::npos);
  }
}

TEST_F(WebDatabaseMigrationTest, MigrateVersion128ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_128.sql")));
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    EXPECT_EQ(128, VersionFromConnection(&connection));
    EXPECT_FALSE(connection.DoesTableExist("generic_payment_instruments"));
  }
  DoMigration();
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    EXPECT_EQ(WebDatabase::kCurrentVersionNumber,
              VersionFromConnection(&connection));
    EXPECT_TRUE(connection.DoesTableExist("generic_payment_instruments"));
  }
}

TEST_F(WebDatabaseMigrationTest, MigrateVersion129ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_129.sql")));
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    EXPECT_EQ(129, VersionFromConnection(&connection));
    EXPECT_FALSE(connection.DoesColumnExist("token_service", "binding_key"));
  }
  DoMigration();
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    EXPECT_EQ(WebDatabase::kCurrentVersionNumber,
              VersionFromConnection(&connection));
    EXPECT_TRUE(connection.DoesColumnExist("token_service", "binding_key"));
  }
}

TEST_F(WebDatabaseMigrationTest, MigrateVersion130ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_130.sql")));
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    EXPECT_EQ(130, VersionFromConnection(&connection));
    EXPECT_TRUE(connection.DoesColumnExist("generic_payment_instruments",
                                           "payment_instrument_type"));
  }
  DoMigration();
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    EXPECT_EQ(WebDatabase::kCurrentVersionNumber,
              VersionFromConnection(&connection));
    EXPECT_FALSE(connection.DoesColumnExist("generic_payment_instruments",
                                            "payment_instrument_type"));
  }
}

// Version 131 added new columns to the contact_info and local_addresses tables.
// These tables were since deprecated and replaced by addresses. The migration
// unit test to the current version thus no longer applies.

TEST_F(WebDatabaseMigrationTest, MigrateVersion132ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_132.sql")));
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    EXPECT_EQ(132, VersionFromConnection(&connection));
    EXPECT_TRUE(connection.DoesColumnExist("masked_ibans", "length"));
  }
  DoMigration();
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    EXPECT_EQ(WebDatabase::kCurrentVersionNumber,
              VersionFromConnection(&connection));
    EXPECT_FALSE(connection.DoesColumnExist("masked_ibans", "length"));
  }
}

// Tests that addresses stored in the legacy contact_info, local_addresses,
// contact_info_type_tokens and local_addresses_type_tokens tables are migrated
// to the addresses and address_type_tokens tables with the correct record type.
TEST_F(WebDatabaseMigrationTest, MigrateVersion133ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_133.sql")));
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    EXPECT_EQ(133, VersionFromConnection(&connection));
    EXPECT_TRUE(connection.DoesTableExist("contact_info"));
    EXPECT_TRUE(connection.DoesTableExist("contact_info_type_tokens"));
    EXPECT_TRUE(connection.DoesTableExist("local_addresses"));
    EXPECT_TRUE(connection.DoesTableExist("local_addresses_type_tokens"));
    EXPECT_FALSE(connection.DoesTableExist("addresses"));
    EXPECT_FALSE(connection.DoesTableExist("address_type_tokens"));

    // Insert a dummy local and account address to test that they are migrated
    // correctly.
    ASSERT_TRUE(connection.ExecuteScriptForTesting(R"(
      INSERT INTO contact_info (guid)
      VALUES ('00000000-0000-0000-0000-000000000000');
      INSERT INTO contact_info_type_tokens (guid, type, value)
      VALUES ('00000000-0000-0000-0000-000000000000', 7, 'value1');
      INSERT INTO local_addresses (guid)
      VALUES ('00000000-0000-0000-0000-000000000001');
      INSERT INTO local_addresses_type_tokens (guid, type, value)
      VALUES ('00000000-0000-0000-0000-000000000001', 9, 'value2');
    )"));
  }
  DoMigration();
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    EXPECT_EQ(WebDatabase::kCurrentVersionNumber,
              VersionFromConnection(&connection));
    EXPECT_FALSE(connection.DoesTableExist("contact_info"));
    EXPECT_FALSE(connection.DoesTableExist("contact_info_type_tokens"));
    EXPECT_FALSE(connection.DoesTableExist("local_addresses"));
    EXPECT_FALSE(connection.DoesTableExist("local_addresses_type_tokens"));
    EXPECT_TRUE(connection.DoesTableExist("addresses"));
    EXPECT_TRUE(connection.DoesTableExist("address_type_tokens"));

    // Expect both addresses in the migrated table with the correct record type.
    sql::Statement s_addresses(connection.GetUniqueStatement(
        "SELECT guid, record_type from addresses ORDER BY guid"));
    ASSERT_TRUE(s_addresses.Step());
    EXPECT_EQ(s_addresses.ColumnString(0),
              "00000000-0000-0000-0000-000000000000");
    EXPECT_EQ(
        s_addresses.ColumnInt(1),
        static_cast<int>(autofill::AutofillProfile::RecordType::kAccount));
    ASSERT_TRUE(s_addresses.Step());
    EXPECT_EQ(s_addresses.ColumnString(0),
              "00000000-0000-0000-0000-000000000001");
    EXPECT_EQ(s_addresses.ColumnInt(1),
              static_cast<int>(
                  autofill::AutofillProfile::RecordType::kLocalOrSyncable));
    ASSERT_FALSE(s_addresses.Step());

    // Expect that the information from the type tokens tables was merged.
    sql::Statement s_type_tokens(connection.GetUniqueStatement(
        "SELECT guid, type, value from address_type_tokens ORDER BY guid"));
    ASSERT_TRUE(s_type_tokens.Step());
    EXPECT_EQ(s_type_tokens.ColumnString(0),
              "00000000-0000-0000-0000-000000000000");
    EXPECT_EQ(s_type_tokens.ColumnInt(1), 7);
    EXPECT_EQ(s_type_tokens.ColumnString(2), "value1");
    ASSERT_TRUE(s_type_tokens.Step());
    EXPECT_EQ(s_type_tokens.ColumnString(0),
              "00000000-0000-0000-0000-000000000001");
    EXPECT_EQ(s_type_tokens.ColumnInt(1), 9);
    EXPECT_EQ(s_type_tokens.ColumnString(2), "value2");
    ASSERT_FALSE(s_type_tokens.Step());
  }
}
