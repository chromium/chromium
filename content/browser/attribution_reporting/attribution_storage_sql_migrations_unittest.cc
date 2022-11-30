// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_storage_sql.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/guid.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_storage.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
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

}  // namespace

class AttributionStorageSqlMigrationsTest : public testing::Test {
 public:
  AttributionStorageSqlMigrationsTest() = default;

  void SetUp() override { ASSERT_TRUE(temp_directory_.CreateUniqueTempDir()); }

  void MigrateDatabase() {
    AttributionStorageSql storage(
        temp_directory_.GetPath(),
        std::make_unique<ConfigurableStorageDelegate>());

    // We need to run an operation on storage to force the lazy initialization.
    std::ignore =
        static_cast<AttributionStorage*>(&storage)->GetAttributionReports(
            base::Time::Min());
  }

  base::FilePath DbPath() {
    return temp_directory_.GetPath().Append(FILE_PATH_LITERAL("Conversions"));
  }

  static base::FilePath GetVersionFilePath(int version_id) {
    // Should be safe cross platform because StringPrintf has overloads for wide
    // strings.
    return base::FilePath(
        base::StringPrintf(FILE_PATH_LITERAL("version_%d.sql"), version_id));
  }

  std::string GetCurrentSchema() {
    base::FilePath current_version_path = temp_directory_.GetPath().Append(
        FILE_PATH_LITERAL("TestCurrentVersion.db"));
    LoadDatabase(
        GetVersionFilePath(AttributionStorageSql::kCurrentVersionNumber),
        current_version_path);
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

  void LoadDatabase(const base::FilePath& file, const base::FilePath& db_path) {
    std::string contents;
    ASSERT_TRUE(GetDatabaseData(file, &contents));

    sql::Database db;
    ASSERT_TRUE(db.Open(db_path));
    ASSERT_TRUE(db.Execute(contents.data()));
  }

  base::ScopedTempDir temp_directory_;
};

TEST_F(AttributionStorageSqlMigrationsTest, MigrateEmptyToCurrent) {
  base::HistogramTester histograms;
  {
    AttributionStorageSql storage(
        temp_directory_.GetPath(),
        std::make_unique<ConfigurableStorageDelegate>());

    // We need to perform an operation that is non-trivial on an empty database
    // to force initialization.
    static_cast<AttributionStorage*>(&storage)->StoreSource(
        SourceBuilder(base::Time::Min()).Build());
  }

  // Verify schema is current.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));

    // Check version.
    EXPECT_EQ(AttributionStorageSql::kCurrentVersionNumber,
              VersionFromDatabase(&db));

    // Check that expected tables are present.
    EXPECT_TRUE(db.DoesTableExist("event_level_reports"));
    EXPECT_TRUE(db.DoesTableExist("sources"));
    EXPECT_TRUE(db.DoesTableExist("meta"));

    EXPECT_EQ(GetCurrentSchema(), db.GetSchema());
  }

  // DB creation histograms should be recorded.
  histograms.ExpectTotalCount("Conversions.Storage.CreationTime", 1);
  histograms.ExpectTotalCount("Conversions.Storage.MigrationTime", 0);
}

TEST_F(AttributionStorageSqlMigrationsTest, MigrateLatestDeprecatedToCurrent) {
  base::HistogramTester histograms;
  LoadDatabase(
      GetVersionFilePath(AttributionStorageSql::kDeprecatedVersionNumber),
      DbPath());

  // Verify pre-conditions.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));

    sql::Statement s(db.GetUniqueStatement("SELECT COUNT(*) FROM conversions"));

    ASSERT_TRUE(s.Step());
    ASSERT_EQ(1, s.ColumnInt(0));
  }

  MigrateDatabase();

  // Verify schema is current.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));

    // Check version.
    EXPECT_EQ(AttributionStorageSql::kCurrentVersionNumber,
              VersionFromDatabase(&db));

    // Compare without quotes as sometimes migrations cause table names to be
    // string literals.
    EXPECT_EQ(RemoveQuotes(GetCurrentSchema()), RemoveQuotes(db.GetSchema()));

    // Verify that data is not preserved across the migration.
    sql::Statement s(
        db.GetUniqueStatement("SELECT COUNT(*) FROM event_level_reports"));

    ASSERT_TRUE(s.Step());
    ASSERT_EQ(0, s.ColumnInt(0));
  }

  // DB creation histograms should be recorded.
  histograms.ExpectTotalCount("Conversions.Storage.CreationTime", 1);
  histograms.ExpectTotalCount("Conversions.Storage.MigrationTime", 0);
}

TEST_F(AttributionStorageSqlMigrationsTest, MigrateVersion33ToCurrent) {
  base::HistogramTester histograms;
  LoadDatabase(GetVersionFilePath(33), DbPath());

  // Verify pre-conditions.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));
    ASSERT_FALSE(db.DoesColumnExist("aggregatable_report_metadata",
                                    "initial_report_time"));

    sql::Statement s(
        db.GetUniqueStatement("SELECT * FROM aggregatable_report_metadata"));

    ASSERT_TRUE(s.Step());
    ASSERT_EQ(6, s.ColumnInt(5));  // report_time
    ASSERT_FALSE(s.Step());
  }

  MigrateDatabase();

  // Verify schema is current.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));

    // Check version.
    EXPECT_EQ(AttributionStorageSql::kCurrentVersionNumber,
              VersionFromDatabase(&db));

    // Compare without quotes as sometimes migrations cause table names to be
    // string literals.
    EXPECT_EQ(RemoveQuotes(GetCurrentSchema()), RemoveQuotes(db.GetSchema()));

    // Verify that data is preserved across the migration.
    sql::Statement s(
        db.GetUniqueStatement("SELECT * FROM aggregatable_report_metadata"));

    ASSERT_TRUE(s.Step());
    ASSERT_EQ(6, s.ColumnInt(5));  // report_time
    ASSERT_EQ(6, s.ColumnInt(7));  // initial_report_time
    ASSERT_FALSE(s.Step());
  }

  // DB creation histograms should be recorded.
  histograms.ExpectTotalCount("Conversions.Storage.CreationTime", 0);
  histograms.ExpectTotalCount("Conversions.Storage.MigrationTime", 1);
}

TEST_F(AttributionStorageSqlMigrationsTest, MigrateVersion34ToCurrent) {
  base::HistogramTester histograms;
  LoadDatabase(GetVersionFilePath(34), DbPath());

  // Verify pre-conditions.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));
    ASSERT_FALSE(db.DoesColumnExist("rate_limits", "expiry_time"));

    sql::Statement s(db.GetUniqueStatement("SELECT * FROM rate_limits"));

    ASSERT_TRUE(s.Step());
    ASSERT_EQ(9, s.ColumnInt64(8));  // time
    ASSERT_TRUE(s.Step());
    ASSERT_EQ(9, s.ColumnInt64(8));  // time
    ASSERT_TRUE(s.Step());
    ASSERT_EQ(9, s.ColumnInt64(8));  // time
    ASSERT_FALSE(s.Step());
  }

  MigrateDatabase();

  // Verify schema is current.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));

    // Check version.
    EXPECT_EQ(AttributionStorageSql::kCurrentVersionNumber,
              VersionFromDatabase(&db));

    // Compare without quotes as sometimes migrations cause table names to be
    // string literals.
    EXPECT_EQ(RemoveQuotes(GetCurrentSchema()), RemoveQuotes(db.GetSchema()));

    // Verify that data is preserved across the migration.
    sql::Statement s(db.GetUniqueStatement("SELECT * FROM rate_limits"));

    ASSERT_TRUE(s.Step());
    ASSERT_EQ(7, s.ColumnInt64(9));  // expiry_time with matching source
    ASSERT_TRUE(s.Step());
    EXPECT_EQ(9 + base::Days(30).InMicroseconds(),
              s.ColumnInt64(9));  // expiry_time without matching source
    ASSERT_TRUE(s.Step());
    EXPECT_EQ(0, s.ColumnInt64(9));  // expiry_time for attribution
    ASSERT_FALSE(s.Step());
  }

  // DB creation histograms should be recorded.
  histograms.ExpectTotalCount("Conversions.Storage.CreationTime", 0);
  histograms.ExpectTotalCount("Conversions.Storage.MigrationTime", 1);
}

TEST_F(AttributionStorageSqlMigrationsTest, MigrateVersion35ToCurrent) {
  base::HistogramTester histograms;
  LoadDatabase(GetVersionFilePath(35), DbPath());

  // Verify pre-conditions.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));
    ASSERT_TRUE(db.DoesIndexExist("sources_by_origin"));
    ASSERT_FALSE(db.DoesIndexExist("active_sources_by_source_origin"));
  }

  MigrateDatabase();

  // Verify schema is current.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));

    // Check version.
    EXPECT_EQ(AttributionStorageSql::kCurrentVersionNumber,
              VersionFromDatabase(&db));

    // Compare without quotes as sometimes migrations cause table names to be
    // string literals.
    EXPECT_EQ(RemoveQuotes(GetCurrentSchema()), RemoveQuotes(db.GetSchema()));

    ASSERT_FALSE(db.DoesIndexExist("sources_by_origin"));
    ASSERT_TRUE(db.DoesIndexExist("active_sources_by_source_origin"));
  }

  // DB creation histograms should be recorded.
  histograms.ExpectTotalCount("Conversions.Storage.CreationTime", 0);
  histograms.ExpectTotalCount("Conversions.Storage.MigrationTime", 1);
}

TEST_F(AttributionStorageSqlMigrationsTest, MigrateVersion36ToCurrent) {
  base::HistogramTester histograms;
  LoadDatabase(GetVersionFilePath(36), DbPath());

  // Verify pre-conditions.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));
    ASSERT_FALSE(db.DoesColumnExist("dedup_keys", "report_type"));

    sql::Statement s(db.GetUniqueStatement("SELECT * FROM dedup_keys"));

    ASSERT_TRUE(s.Step());
    ASSERT_EQ(1, s.ColumnInt64(0));  // source_id
    ASSERT_EQ(2, s.ColumnInt64(1));  // dedup_key
    ASSERT_FALSE(s.Step());
  }

  MigrateDatabase();

  // Verify schema is current.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));

    // Check version.
    EXPECT_EQ(AttributionStorageSql::kCurrentVersionNumber,
              VersionFromDatabase(&db));

    // Compare without quotes as sometimes migrations cause table names to be
    // string literals.
    EXPECT_EQ(RemoveQuotes(GetCurrentSchema()), RemoveQuotes(db.GetSchema()));

    // Verify that data is preserved across the migration.
    sql::Statement s(db.GetUniqueStatement("SELECT * FROM dedup_keys"));

    ASSERT_TRUE(s.Step());
    ASSERT_EQ(1, s.ColumnInt64(0));  // source_id
    ASSERT_EQ(0, s.ColumnInt(1));    // report_type
    ASSERT_EQ(2, s.ColumnInt64(2));  // dedup_key
    ASSERT_FALSE(s.Step());
  }

  // DB creation histograms should be recorded.
  histograms.ExpectTotalCount("Conversions.Storage.CreationTime", 0);
  histograms.ExpectTotalCount("Conversions.Storage.MigrationTime", 1);
}

TEST_F(AttributionStorageSqlMigrationsTest, MigrateVersion37ToCurrent) {
  base::HistogramTester histograms;
  LoadDatabase(GetVersionFilePath(37), DbPath());

  // Verify pre-conditions.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));
    ASSERT_FALSE(db.DoesColumnExist("sources", "event_report_window"));
    ASSERT_FALSE(db.DoesColumnExist("sources", "aggregatable_report_window"));

    sql::Statement s(db.GetUniqueStatement("SELECT * FROM sources"));

    ASSERT_TRUE(s.Step());
    ASSERT_EQ(8, s.ColumnInt(6));  // expiry_time
    ASSERT_EQ(9, s.ColumnInt(7));  // num_attributions
    ASSERT_FALSE(s.Step());
  }

  MigrateDatabase();

  // Verify schema is current.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));

    // Check version.
    EXPECT_EQ(AttributionStorageSql::kCurrentVersionNumber,
              VersionFromDatabase(&db));

    // Compare without quotes as sometimes migrations cause table names to be
    // string literals.
    EXPECT_EQ(RemoveQuotes(GetCurrentSchema()), RemoveQuotes(db.GetSchema()));

    // Verify that data is preserved across the migration.
    sql::Statement s(db.GetUniqueStatement("SELECT * FROM sources"));

    ASSERT_TRUE(s.Step());
    ASSERT_EQ(8, s.ColumnInt(6));  // expiry_time
    ASSERT_EQ(8, s.ColumnInt(7));  // event_report_window
    ASSERT_EQ(8, s.ColumnInt(8));  // aggregatable_report_window
    ASSERT_EQ(9, s.ColumnInt(9));  // num_attributions
    ASSERT_FALSE(s.Step());
  }

  // DB creation histograms should be recorded.
  histograms.ExpectTotalCount("Conversions.Storage.CreationTime", 0);
  histograms.ExpectTotalCount("Conversions.Storage.MigrationTime", 1);
}

TEST_F(AttributionStorageSqlMigrationsTest, MigrateVersion38ToCurrent) {
  base::HistogramTester histograms;
  LoadDatabase(GetVersionFilePath(38), DbPath());

  // Verify pre-conditions.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));
    ASSERT_FALSE(db.DoesColumnExist("aggregatable_report_metadata",
                                    "aggregation_coordinator"));

    sql::Statement s(
        db.GetUniqueStatement("SELECT * FROM aggregatable_report_metadata"));

    ASSERT_TRUE(s.Step());
    ASSERT_EQ(1, s.ColumnInt(0));  // aggregation_id
    ASSERT_FALSE(s.Step());
  }

  MigrateDatabase();

  // Verify schema is current.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));

    // Check version.
    EXPECT_EQ(AttributionStorageSql::kCurrentVersionNumber,
              VersionFromDatabase(&db));

    // Compare without quotes as sometimes migrations cause table names to be
    // string literals.
    EXPECT_EQ(RemoveQuotes(GetCurrentSchema()), RemoveQuotes(db.GetSchema()));

    // Verify that data is preserved across the migration.
    sql::Statement s(
        db.GetUniqueStatement("SELECT * FROM aggregatable_report_metadata"));

    ASSERT_TRUE(s.Step());
    ASSERT_EQ(1, s.ColumnInt(0));  // aggregation_id
    ASSERT_EQ(0, s.ColumnInt(8));  // aggregation_coordinator
    ASSERT_FALSE(s.Step());
  }

  // DB creation histograms should be recorded.
  histograms.ExpectTotalCount("Conversions.Storage.CreationTime", 0);
  histograms.ExpectTotalCount("Conversions.Storage.MigrationTime", 1);
}

}  // namespace content
