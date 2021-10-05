// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_storage_sql.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_storage.h"
#include "content/browser/attribution_reporting/conversion_test_utils.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

std::string RemoveQuotes(std::string input) {
  std::string output;
  base::RemoveChars(input, "\"", &output);
  return output;
}

const int kCurrentVersionNumber = 14;

}  // namespace

class AttributionStorageSqlMigrationsTest : public testing::Test {
 public:
  AttributionStorageSqlMigrationsTest() = default;

  void SetUp() override { ASSERT_TRUE(temp_directory_.CreateUniqueTempDir()); }

  void MigrateDatabase() {
    base::SimpleTestClock clock;
    AttributionStorageSql storage(
        temp_directory_.GetPath(),
        std::make_unique<ConfigurableStorageDelegate>(), &clock);

    // We need to run an operation on storage to force the lazy initialization.
    ignore_result(
        static_cast<AttributionStorage*>(&storage)->GetConversionsToReport(
            base::Time::Min()));
  }

  base::FilePath DbPath() {
    return temp_directory_.GetPath().Append(FILE_PATH_LITERAL("Conversions"));
  }

  std::string GetCurrentSchema() {
    base::FilePath current_version_path = temp_directory_.GetPath().Append(
        FILE_PATH_LITERAL("TestCurrentVersion.db"));
    LoadDatabase(FILE_PATH_LITERAL("version_14.sql"), current_version_path);
    sql::Database db;
    EXPECT_TRUE(db.Open(current_version_path));
    return db.GetSchema();
  }

 protected:
  // The textual contents of |file| are read from
  // "content/test/data/attribution_reporting/databases/" and returned in the
  // string |contents|. Returns true if the file exists and is read
  // successfully, false otherwise.
  bool GetDatabaseData(const base::FilePath& file, std::string* contents) {
    base::FilePath source_path;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &source_path);
    source_path = source_path.AppendASCII(
        "content/test/data/attribution_reporting/databases");
    source_path = source_path.Append(file);
    return base::PathExists(source_path) &&
           base::ReadFileToString(source_path, contents);
  }

  static int VersionFromDatabase(sql::Database* db) {
    // Get version.
    sql::Statement s(
        db->GetUniqueStatement("SELECT value FROM meta WHERE key='version'"));
    if (!s.Step())
      return 0;
    return s.ColumnInt(0);
  }

  void LoadDatabase(const base::FilePath::StringType& file,
                    const base::FilePath& db_path) {
    std::string contents;
    ASSERT_TRUE(GetDatabaseData(base::FilePath(file), &contents));

    sql::Database db;
    ASSERT_TRUE(db.Open(db_path));
    ASSERT_TRUE(db.Execute(contents.data()));
  }

  base::ScopedTempDir temp_directory_;
};

TEST_F(AttributionStorageSqlMigrationsTest, MigrateEmptyToCurrent) {
  base::HistogramTester histograms;
  {
    base::SimpleTestClock clock;
    AttributionStorageSql storage(
        temp_directory_.GetPath(),
        std::make_unique<ConfigurableStorageDelegate>(), &clock);

    // We need to perform an operation that is non-trivial on an empty database
    // to force initialization.
    static_cast<AttributionStorage*>(&storage)->StoreImpression(
        ImpressionBuilder(base::Time::Min()).Build());
  }

  // Verify schema is current.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));

    // Check version.
    EXPECT_EQ(kCurrentVersionNumber, VersionFromDatabase(&db));

    // Check that expected tables are present.
    EXPECT_TRUE(db.DoesTableExist("conversions"));
    EXPECT_TRUE(db.DoesTableExist("impressions"));
    EXPECT_TRUE(db.DoesTableExist("meta"));

    EXPECT_EQ(GetCurrentSchema(), db.GetSchema());
  }

  // DB creation histograms should be recorded.
  histograms.ExpectTotalCount("Conversions.Storage.CreationTime", 1);
  histograms.ExpectTotalCount("Conversions.Storage.MigrationTime", 0);
}

TEST_F(AttributionStorageSqlMigrationsTest, MigrateVersion1ToCurrent) {
  base::HistogramTester histograms;
  LoadDatabase(FILE_PATH_LITERAL("version_1.sql"), DbPath());

  // Verify pre-conditions.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));

    ASSERT_FALSE(db.DoesTableExist("meta"));
    ASSERT_FALSE(db.DoesColumnExist("impressions", "conversion_destination"));

    sql::Statement s(
        db.GetUniqueStatement("SELECT conversion_origin FROM impressions"));

    ASSERT_TRUE(s.Step());
    ASSERT_EQ("https://sub.conversion.test", s.ColumnString(0));
    ASSERT_FALSE(s.Step());
  }

  MigrateDatabase();

  // Verify schema is current.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));

    // Check version.
    EXPECT_EQ(kCurrentVersionNumber, VersionFromDatabase(&db));

    // Check that expected tables are present.
    EXPECT_TRUE(db.DoesTableExist("conversions"));
    EXPECT_TRUE(db.DoesTableExist("impressions"));
    EXPECT_TRUE(db.DoesTableExist("meta"));

    // Compare without quotes as sometimes migrations cause table names to be
    // string literals.
    EXPECT_EQ(RemoveQuotes(GetCurrentSchema()), RemoveQuotes(db.GetSchema()));

    // Check that the relevant schema changes are made.
    EXPECT_TRUE(db.DoesColumnExist("impressions", "conversion_destination"));

    // Verify that data is preserved across the migration.
    size_t rows = 0;
    sql::test::CountTableRows(&db, "impressions", &rows);
    EXPECT_EQ(1u, rows);

    sql::Statement s(db.GetUniqueStatement(
        "SELECT conversion_origin, conversion_destination FROM impressions"));

    ASSERT_TRUE(s.Step());
    ASSERT_EQ("https://sub.conversion.test", s.ColumnString(0));
    ASSERT_EQ("https://conversion.test", s.ColumnString(1));
    ASSERT_FALSE(s.Step());
  }

  // DB migration histograms should be recorded.
  histograms.ExpectTotalCount("Conversions.Storage.CreationTime", 0);
  histograms.ExpectTotalCount("Conversions.Storage.MigrationTime", 1);
}

TEST_F(AttributionStorageSqlMigrationsTest, MigrateVersion2ToCurrent) {
  base::HistogramTester histograms;
  LoadDatabase(FILE_PATH_LITERAL("version_2.sql"), DbPath());

  // Verify pre-conditions.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));

    ASSERT_FALSE(db.DoesColumnExist("impressions", "source_type"));
    ASSERT_FALSE(db.DoesColumnExist("impressions", "attributed_truthfully"));
  }

  MigrateDatabase();

  // Verify schema is current.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));

    // Check version.
    EXPECT_EQ(kCurrentVersionNumber, VersionFromDatabase(&db));

    // Check that expected tables are present.
    EXPECT_TRUE(db.DoesTableExist("conversions"));
    EXPECT_TRUE(db.DoesTableExist("impressions"));
    EXPECT_TRUE(db.DoesTableExist("meta"));

    // Compare without quotes as sometimes migrations cause table names to be
    // string literals.
    EXPECT_EQ(RemoveQuotes(GetCurrentSchema()), RemoveQuotes(db.GetSchema()));

    // Check that the relevant schema changes are made.
    EXPECT_TRUE(db.DoesColumnExist("impressions", "source_type"));
    EXPECT_TRUE(db.DoesColumnExist("impressions", "attributed_truthfully"));

    // Verify that data is preserved across the migration.
    size_t rows = 0;
    sql::test::CountTableRows(&db, "impressions", &rows);
    EXPECT_EQ(1u, rows);

    sql::Statement s(db.GetUniqueStatement(
        "SELECT source_type, attributed_truthfully FROM impressions"));

    ASSERT_TRUE(s.Step());
    ASSERT_EQ(0, s.ColumnInt(0));
    ASSERT_EQ(true, s.ColumnBool(1));
    ASSERT_FALSE(s.Step());
  }

  // DB migration histograms should be recorded.
  histograms.ExpectTotalCount("Conversions.Storage.CreationTime", 0);
  histograms.ExpectTotalCount("Conversions.Storage.MigrationTime", 1);
}

TEST_F(AttributionStorageSqlMigrationsTest, MigrateVersion3ToCurrent) {
  base::HistogramTester histograms;
  LoadDatabase(FILE_PATH_LITERAL("version_3.sql"), DbPath());

  // Verify pre-conditions.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));

    ASSERT_FALSE(db.DoesTableExist("rate_limits"));
  }

  MigrateDatabase();

  // Verify schema is current.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));

    // Check version.
    EXPECT_EQ(kCurrentVersionNumber, VersionFromDatabase(&db));

    // Check that expected tables are present.
    EXPECT_TRUE(db.DoesTableExist("conversions"));
    EXPECT_TRUE(db.DoesTableExist("impressions"));
    EXPECT_TRUE(db.DoesTableExist("meta"));
    EXPECT_TRUE(db.DoesTableExist("rate_limits"));

    // Compare without quotes as sometimes migrations cause table names to be
    // string literals.
    EXPECT_EQ(RemoveQuotes(GetCurrentSchema()), RemoveQuotes(db.GetSchema()));
  }

  // DB migration histograms should be recorded.
  histograms.ExpectTotalCount("Conversions.Storage.CreationTime", 0);
  histograms.ExpectTotalCount("Conversions.Storage.MigrationTime", 1);
}

TEST_F(AttributionStorageSqlMigrationsTest, MigrateVersion4ToCurrent) {
  base::HistogramTester histograms;
  LoadDatabase(FILE_PATH_LITERAL("version_4.sql"), DbPath());

  // Verify pre-conditions.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));

    ASSERT_TRUE(db.DoesColumnExist("conversions", "attribution_credit"));

    sql::Statement s(db.GetUniqueStatement(
        "SELECT conversion_id FROM conversions ORDER BY conversion_id"));
    ASSERT_TRUE(s.Step());
    ASSERT_EQ(1, s.ColumnInt(0));
    ASSERT_TRUE(s.Step());
    ASSERT_EQ(2, s.ColumnInt(0));
    ASSERT_TRUE(s.Step());
    ASSERT_EQ(3, s.ColumnInt(0));
    ASSERT_FALSE(s.Step());
  }

  MigrateDatabase();

  // Verify schema is current.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));

    // Check version.
    EXPECT_EQ(kCurrentVersionNumber, VersionFromDatabase(&db));

    // Check that expected tables are present.
    EXPECT_TRUE(db.DoesTableExist("conversions"));
    EXPECT_TRUE(db.DoesTableExist("impressions"));
    EXPECT_TRUE(db.DoesTableExist("meta"));
    EXPECT_TRUE(db.DoesTableExist("rate_limits"));

    // Check that the expected column is dropped.
    EXPECT_FALSE(db.DoesColumnExist("conversions", "attribution_credit"));

    // Check that only the 0-credit conversions are deleted.
    sql::Statement s(db.GetUniqueStatement(
        "SELECT conversion_id FROM conversions ORDER BY conversion_id"));
    ASSERT_TRUE(s.Step());
    ASSERT_EQ(1, s.ColumnInt(0));
    ASSERT_TRUE(s.Step());
    ASSERT_EQ(3, s.ColumnInt(0));
    ASSERT_FALSE(s.Step());

    // Compare without quotes as sometimes migrations cause table names to be
    // string literals.
    EXPECT_EQ(RemoveQuotes(GetCurrentSchema()), RemoveQuotes(db.GetSchema()));
  }

  // DB migration histograms should be recorded.
  histograms.ExpectTotalCount("Conversions.Storage.CreationTime", 0);
  histograms.ExpectTotalCount("Conversions.Storage.MigrationTime", 1);
}

TEST_F(AttributionStorageSqlMigrationsTest, MigrateVersion5ToCurrent) {
  base::HistogramTester histograms;
  LoadDatabase(FILE_PATH_LITERAL("version_5.sql"), DbPath());

  // Verify pre-conditions.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));

    ASSERT_FALSE(db.DoesColumnExist("impressions", "priority"));
  }

  MigrateDatabase();

  // Verify schema is current.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));

    // Check version.
    EXPECT_EQ(kCurrentVersionNumber, VersionFromDatabase(&db));

    // Check that expected tables are present.
    EXPECT_TRUE(db.DoesTableExist("conversions"));
    EXPECT_TRUE(db.DoesTableExist("impressions"));
    EXPECT_TRUE(db.DoesTableExist("meta"));
    EXPECT_TRUE(db.DoesTableExist("rate_limits"));

    // Compare without quotes as sometimes migrations cause table names to be
    // string literals.
    EXPECT_EQ(RemoveQuotes(GetCurrentSchema()), RemoveQuotes(db.GetSchema()));

    // Check that the relevant schema changes are made.
    EXPECT_TRUE(db.DoesColumnExist("impressions", "priority"));

    // Verify that data is preserved across the migration.
    size_t rows = 0;
    sql::test::CountTableRows(&db, "impressions", &rows);
    EXPECT_EQ(1u, rows);

    sql::Statement s(db.GetUniqueStatement(
        "SELECT conversion_origin, priority FROM impressions"));

    ASSERT_TRUE(s.Step());
    ASSERT_EQ("https://conversion.test", s.ColumnString(0));
    ASSERT_EQ(0, s.ColumnInt64(1));
    ASSERT_FALSE(s.Step());
  }

  // DB migration histograms should be recorded.
  histograms.ExpectTotalCount("Conversions.Storage.CreationTime", 0);
  histograms.ExpectTotalCount("Conversions.Storage.MigrationTime", 1);
}

TEST_F(AttributionStorageSqlMigrationsTest, MigrateVersion6ToCurrent) {
  base::HistogramTester histograms;
  LoadDatabase(FILE_PATH_LITERAL("version_6.sql"), DbPath());

  // Verify pre-conditions.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));

    ASSERT_FALSE(db.DoesColumnExist("impressions", "impression_site"));
  }

  MigrateDatabase();

  // Verify schema is current.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));

    // Check version.
    EXPECT_EQ(kCurrentVersionNumber, VersionFromDatabase(&db));

    // Compare without quotes as sometimes migrations cause table names to be
    // string literals.
    EXPECT_EQ(RemoveQuotes(GetCurrentSchema()), RemoveQuotes(db.GetSchema()));

    // Check that the relevant schema changes are made.
    EXPECT_TRUE(db.DoesColumnExist("impressions", "impression_site"));

    // Verify that data is preserved across the migration.
    size_t rows = 0;
    sql::test::CountTableRows(&db, "impressions", &rows);
    EXPECT_EQ(2u, rows);

    sql::Statement s(db.GetUniqueStatement(
        "SELECT impression_origin, impression_site FROM impressions"));

    ASSERT_TRUE(s.Step());
    ASSERT_EQ("https://a.impression.test", s.ColumnString(0));
    ASSERT_EQ("https://impression.test", s.ColumnString(1));
    ASSERT_TRUE(s.Step());
    ASSERT_EQ("https://b.impression.test", s.ColumnString(0));
    ASSERT_EQ("https://impression.test", s.ColumnString(1));
    ASSERT_FALSE(s.Step());
  }

  // DB migration histograms should be recorded.
  histograms.ExpectTotalCount("Conversions.Storage.CreationTime", 0);
  histograms.ExpectTotalCount("Conversions.Storage.MigrationTime", 1);
}

TEST_F(AttributionStorageSqlMigrationsTest, MigrateVersion7ToCurrent) {
  base::HistogramTester histograms;
  LoadDatabase(FILE_PATH_LITERAL("version_7.sql"), DbPath());

  // Verify pre-conditions.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));

    sql::Statement impression_statement(db.GetUniqueStatement(
        "SELECT impression_data FROM impressions ORDER BY impression_id"));
    ASSERT_TRUE(impression_statement.Step());
    ASSERT_EQ("18446744073709551615", impression_statement.ColumnString(0));
    ASSERT_TRUE(impression_statement.Step());
    ASSERT_EQ("invalid", impression_statement.ColumnString(0));
    ASSERT_FALSE(impression_statement.Step());

    sql::Statement conversion_statement(
        db.GetUniqueStatement("SELECT conversion_data, impression_id FROM "
                              "conversions ORDER BY conversion_id"));
    ASSERT_TRUE(conversion_statement.Step());
    ASSERT_EQ("234", conversion_statement.ColumnString(0));
    ASSERT_EQ(29L, conversion_statement.ColumnInt64(1));
    ASSERT_TRUE(conversion_statement.Step());
    ASSERT_EQ("invalid", conversion_statement.ColumnString(0));
    ASSERT_EQ(sql::ColumnType::kNull, conversion_statement.GetColumnType(1));
    ASSERT_FALSE(conversion_statement.Step());
  }

  MigrateDatabase();

  // Verify schema is current.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));

    // Check version.
    EXPECT_EQ(kCurrentVersionNumber, VersionFromDatabase(&db));

    // Check that expected tables are present.
    EXPECT_TRUE(db.DoesTableExist("conversions"));
    EXPECT_TRUE(db.DoesTableExist("impressions"));
    EXPECT_TRUE(db.DoesTableExist("meta"));
    EXPECT_TRUE(db.DoesTableExist("rate_limits"));

    // Compare without quotes as sometimes migrations cause table names to be
    // string literals.
    EXPECT_EQ(RemoveQuotes(GetCurrentSchema()), RemoveQuotes(db.GetSchema()));

    sql::Statement impression_statement(db.GetUniqueStatement(
        "SELECT impression_data FROM impressions ORDER BY impression_id"));
    ASSERT_TRUE(impression_statement.Step());
    ASSERT_EQ(18446744073709551615UL,
              static_cast<uint64_t>(impression_statement.ColumnInt64(0)));
    ASSERT_TRUE(impression_statement.Step());
    ASSERT_EQ(0L, impression_statement.ColumnInt64(0));
    ASSERT_FALSE(impression_statement.Step());

    sql::Statement conversion_statement(
        db.GetUniqueStatement("SELECT conversion_data, impression_id FROM "
                              "conversions ORDER BY conversion_id"));
    ASSERT_TRUE(conversion_statement.Step());
    ASSERT_EQ(234L, conversion_statement.ColumnInt64(0));
    ASSERT_EQ(29L, conversion_statement.ColumnInt64(1));
    ASSERT_TRUE(conversion_statement.Step());
    ASSERT_EQ(0L, conversion_statement.ColumnInt64(0));
    ASSERT_EQ(0L, conversion_statement.ColumnInt64(1));
    ASSERT_FALSE(conversion_statement.Step());
  }

  // DB migration histograms should be recorded.
  histograms.ExpectTotalCount("Conversions.Storage.CreationTime", 0);
  histograms.ExpectTotalCount("Conversions.Storage.MigrationTime", 1);
}

TEST_F(AttributionStorageSqlMigrationsTest, MigrateVersion8ToCurrent) {
  base::HistogramTester histograms;
  LoadDatabase(FILE_PATH_LITERAL("version_8.sql"), DbPath());

  // Verify pre-conditions.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));

    ASSERT_FALSE(db.DoesColumnExist("conversions", "priority"));
  }

  MigrateDatabase();

  // Verify schema is current.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));

    // Check version.
    EXPECT_EQ(kCurrentVersionNumber, VersionFromDatabase(&db));

    // Compare without quotes as sometimes migrations cause table names to be
    // string literals.
    EXPECT_EQ(RemoveQuotes(GetCurrentSchema()), RemoveQuotes(db.GetSchema()));

    // Check that the relevant schema changes are made.
    EXPECT_TRUE(db.DoesColumnExist("conversions", "priority"));
  }

  // DB migration histograms should be recorded.
  histograms.ExpectTotalCount("Conversions.Storage.CreationTime", 0);
  histograms.ExpectTotalCount("Conversions.Storage.MigrationTime", 1);
}

TEST_F(AttributionStorageSqlMigrationsTest, MigrateVersion9ToCurrent) {
  base::HistogramTester histograms;
  LoadDatabase(FILE_PATH_LITERAL("version_9.sql"), DbPath());

  // Verify pre-conditions.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));

    ASSERT_FALSE(db.DoesTableExist("dedup_keys"));
  }

  MigrateDatabase();

  // Verify schema is current.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));

    // Check version.
    EXPECT_EQ(kCurrentVersionNumber, VersionFromDatabase(&db));

    // Compare without quotes as sometimes migrations cause table names to be
    // string literals.
    EXPECT_EQ(RemoveQuotes(GetCurrentSchema()), RemoveQuotes(db.GetSchema()));

    // Check that the relevant schema changes are made.
    EXPECT_TRUE(db.DoesTableExist("dedup_keys"));
  }

  // DB migration histograms should be recorded.
  histograms.ExpectTotalCount("Conversions.Storage.CreationTime", 0);
  histograms.ExpectTotalCount("Conversions.Storage.MigrationTime", 1);
}

TEST_F(AttributionStorageSqlMigrationsTest, MigrateVersion10ToCurrent) {
  base::HistogramTester histograms;
  LoadDatabase(FILE_PATH_LITERAL("version_10.sql"), DbPath());

  // Verify pre-conditions.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));

    ASSERT_TRUE(db.DoesIndexExist("impression_site_idx"));
    ASSERT_FALSE(db.DoesIndexExist("event_source_impression_site_idx"));
  }

  MigrateDatabase();

  // Verify schema is current.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));

    // Check version.
    EXPECT_EQ(kCurrentVersionNumber, VersionFromDatabase(&db));

    // Compare without quotes as sometimes migrations cause table names to be
    // string literals.
    EXPECT_EQ(RemoveQuotes(GetCurrentSchema()), RemoveQuotes(db.GetSchema()));

    // Check that the relevant schema changes are made.
    ASSERT_FALSE(db.DoesIndexExist("impression_site_idx"));
    ASSERT_TRUE(db.DoesIndexExist("event_source_impression_site_idx"));
  }

  // DB migration histograms should be recorded.
  histograms.ExpectTotalCount("Conversions.Storage.CreationTime", 0);
  histograms.ExpectTotalCount("Conversions.Storage.MigrationTime", 1);
}

TEST_F(AttributionStorageSqlMigrationsTest, MigrateVersion11ToCurrent) {
  base::HistogramTester histograms;
  LoadDatabase(FILE_PATH_LITERAL("version_11.sql"), DbPath());

  // Verify pre-conditions.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));
    ASSERT_FALSE(db.DoesColumnExist("rate_limits", "bucket"));
    ASSERT_FALSE(db.DoesColumnExist("rate_limits", "value"));
    ASSERT_FALSE(
        db.DoesIndexExist("rate_limit_attribution_type_conversion_time_idx"));
    ASSERT_TRUE(db.DoesIndexExist("rate_limit_conversion_time_idx"));

    sql::Statement s(db.GetUniqueStatement("SELECT * FROM rate_limits"));
    ASSERT_TRUE(s.Step());
    ASSERT_EQ(1, s.ColumnInt(0));
    ASSERT_EQ(0, s.ColumnInt(1));
    ASSERT_EQ(11, s.ColumnInt64(2));
    ASSERT_EQ("https://a.example", s.ColumnString(3));
    ASSERT_EQ("https://a.a.example", s.ColumnString(4));
    ASSERT_EQ("https://b.example", s.ColumnString(5));
    ASSERT_EQ("https://b.b.example", s.ColumnString(6));
    ASSERT_EQ(7, s.ColumnInt64(7));
    ASSERT_FALSE(s.Step());
  }

  MigrateDatabase();

  // Verify schema is current.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));

    // Check version.
    EXPECT_EQ(kCurrentVersionNumber, VersionFromDatabase(&db));

    // Compare without quotes as sometimes migrations cause table names to be
    // string literals.
    EXPECT_EQ(RemoveQuotes(GetCurrentSchema()), RemoveQuotes(db.GetSchema()));

    // Check that the relevant schema changes are made.
    ASSERT_TRUE(db.DoesColumnExist("rate_limits", "bucket"));
    ASSERT_TRUE(db.DoesColumnExist("rate_limits", "value"));
    ASSERT_TRUE(
        db.DoesIndexExist("rate_limit_attribution_type_conversion_time_idx"));
    ASSERT_FALSE(db.DoesIndexExist("rate_limit_conversion_time_idx"));

    // Verify that data is preserved across the migration.
    sql::Statement s(db.GetUniqueStatement("SELECT * FROM rate_limits"));
    ASSERT_TRUE(s.Step());
    ASSERT_EQ(1, s.ColumnInt(0));
    ASSERT_EQ(0, s.ColumnInt(1));
    ASSERT_EQ(11, s.ColumnInt64(2));
    ASSERT_EQ("https://a.example", s.ColumnString(3));
    ASSERT_EQ("https://a.a.example", s.ColumnString(4));
    ASSERT_EQ("https://b.example", s.ColumnString(5));
    ASSERT_EQ("https://b.b.example", s.ColumnString(6));
    ASSERT_EQ(7, s.ColumnInt64(7));
    ASSERT_EQ(0, s.ColumnInt64(8));
    ASSERT_EQ(1, s.ColumnInt64(9));
    ASSERT_FALSE(s.Step());
  }

  // DB migration histograms should be recorded.
  histograms.ExpectTotalCount("Conversions.Storage.CreationTime", 0);
  histograms.ExpectTotalCount("Conversions.Storage.MigrationTime", 1);
}

TEST_F(AttributionStorageSqlMigrationsTest, MigrateVersion12ToCurrent) {
  base::HistogramTester histograms;
  LoadDatabase(FILE_PATH_LITERAL("version_12.sql"), DbPath());

  auto check_data = [](sql::Database& db) {
    sql::Statement s(db.GetUniqueStatement("SELECT * FROM impressions"));
    ASSERT_TRUE(s.Step());
    ASSERT_EQ(1, s.ColumnInt(0));
    ASSERT_EQ(2, s.ColumnInt(1));
    ASSERT_EQ("a", s.ColumnString(2));
    ASSERT_EQ("b", s.ColumnString(3));
    ASSERT_EQ("c", s.ColumnString(4));
    ASSERT_EQ(3, s.ColumnInt(5));
    ASSERT_EQ(4, s.ColumnInt(6));
    ASSERT_EQ(5, s.ColumnInt(7));
    ASSERT_EQ(6, s.ColumnInt(8));
    ASSERT_EQ("d", s.ColumnString(9));
    ASSERT_EQ(7, s.ColumnInt(10));
    ASSERT_EQ(8, s.ColumnInt(11));
    ASSERT_EQ(9, s.ColumnInt(12));
    ASSERT_EQ("e", s.ColumnString(13));
    ASSERT_FALSE(s.Step());

    sql::Statement t(db.GetUniqueStatement("SELECT * FROM conversions"));
    ASSERT_TRUE(t.Step());
    ASSERT_EQ(10, t.ColumnInt(0));
    ASSERT_EQ(11, t.ColumnInt(1));
    ASSERT_EQ(12, t.ColumnInt(2));
    ASSERT_EQ(13, t.ColumnInt(3));
    ASSERT_EQ(14, t.ColumnInt(4));
    ASSERT_EQ(15, t.ColumnInt(5));
    ASSERT_FALSE(t.Step());
  };

  // Verify pre-conditions.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));
    check_data(db);
  }

  MigrateDatabase();

  // Verify schema is current.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));

    // Check version.
    EXPECT_EQ(kCurrentVersionNumber, VersionFromDatabase(&db));

    // Compare without quotes as sometimes migrations cause table names to be
    // string literals.
    EXPECT_EQ(RemoveQuotes(GetCurrentSchema()), RemoveQuotes(db.GetSchema()));

    check_data(db);
  }

  // DB migration histograms should be recorded.
  histograms.ExpectTotalCount("Conversions.Storage.CreationTime", 0);
  histograms.ExpectTotalCount("Conversions.Storage.MigrationTime", 1);
}

TEST_F(AttributionStorageSqlMigrationsTest, MigrateVersion13ToCurrent) {
  base::HistogramTester histograms;
  LoadDatabase(FILE_PATH_LITERAL("version_13.sql"), DbPath());

  // Verify pre-conditions.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));
    ASSERT_FALSE(db.DoesColumnExist("conversions", "failed_send_attempts"));

    sql::Statement s(db.GetUniqueStatement("SELECT * FROM conversions"));

    ASSERT_TRUE(s.Step());
    ASSERT_EQ(1, s.ColumnInt(0));
    ASSERT_EQ(2, s.ColumnInt(1));
    ASSERT_EQ(3, s.ColumnInt(2));
    ASSERT_EQ(4, s.ColumnInt(3));
    ASSERT_EQ(5, s.ColumnInt(4));
    ASSERT_EQ(6, s.ColumnInt(5));
    ASSERT_FALSE(s.Step());
  }

  MigrateDatabase();

  // Verify schema is current.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));

    // Check version.
    EXPECT_EQ(kCurrentVersionNumber, VersionFromDatabase(&db));

    // Compare without quotes as sometimes migrations cause table names to be
    // string literals.
    EXPECT_EQ(RemoveQuotes(GetCurrentSchema()), RemoveQuotes(db.GetSchema()));

    // Check that the relevant schema changes are made.
    ASSERT_TRUE(db.DoesColumnExist("conversions", "failed_send_attempts"));

    // Verify that data is preserved across the migration.
    sql::Statement s(db.GetUniqueStatement("SELECT * FROM conversions"));

    ASSERT_TRUE(s.Step());
    ASSERT_EQ(1, s.ColumnInt(0));
    ASSERT_EQ(2, s.ColumnInt(1));
    ASSERT_EQ(3, s.ColumnInt(2));
    ASSERT_EQ(4, s.ColumnInt(3));
    ASSERT_EQ(5, s.ColumnInt(4));
    ASSERT_EQ(6, s.ColumnInt(5));
    ASSERT_EQ(0, s.ColumnInt(6));
    ASSERT_FALSE(s.Step());
  }

  // DB migration histograms should be recorded.
  histograms.ExpectTotalCount("Conversions.Storage.CreationTime", 0);
  histograms.ExpectTotalCount("Conversions.Storage.MigrationTime", 1);
}

}  // namespace content
