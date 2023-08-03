// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_storage_sql_migrations.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_reporting.pb.h"
#include "content/browser/attribution_reporting/attribution_storage.h"
#include "content/browser/attribution_reporting/attribution_storage_sql.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/store_source_result.h"
#include "content/browser/attribution_reporting/test/configurable_storage_delegate.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

// Normalize schema strings to compare them reliabily. Notably, applies the
// following transformations:
// - Remove quotes as sometimes migrations cause table names to be string
//   literals.
// - Replaces ", " with "," as CREATE TABLE in schema will be represented with
//   or without a space depending if it got there by calling CREATE TABLE
//   directly or with an ALTER TABLE.
std::string NormalizeSchema(std::string input) {
  std::string output;
  base::RemoveChars(input, "\"", &output);
  base::ReplaceSubstringsAfterOffset(&output, 0, ", ", ",");
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

  static void CheckVersionNumbers(sql::Database* db) {
    {
      sql::Statement s(
          db->GetUniqueStatement("SELECT value FROM meta WHERE key='version'"));
      ASSERT_TRUE(s.Step());
      EXPECT_EQ(s.ColumnInt(0), AttributionStorageSql::kCurrentVersionNumber);
    }

    {
      sql::Statement s(db->GetUniqueStatement(
          "SELECT value FROM meta WHERE key='last_compatible_version'"));
      ASSERT_TRUE(s.Step());
      EXPECT_EQ(s.ColumnInt(0),
                AttributionStorageSql::kCompatibleVersionNumber);
    }
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

    CheckVersionNumbers(&db);

    // Check that expected tables are present.
    EXPECT_TRUE(db.DoesTableExist("reports"));
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

    sql::Statement s(db.GetUniqueStatement("SELECT COUNT(*) FROM sources"));

    ASSERT_TRUE(s.Step());
    ASSERT_EQ(1, s.ColumnInt(0));
  }

  MigrateDatabase();

  // Verify schema is current.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));

    CheckVersionNumbers(&db);

    EXPECT_EQ(NormalizeSchema(GetCurrentSchema()),
              NormalizeSchema(db.GetSchema()));

    // Verify that data is not preserved across the migration.
    sql::Statement s(db.GetUniqueStatement("SELECT COUNT(*) FROM sources"));

    ASSERT_TRUE(s.Step());
    ASSERT_EQ(0, s.ColumnInt(0));
  }

  // DB creation histograms should be recorded.
  histograms.ExpectTotalCount("Conversions.Storage.CreationTime", 1);
  histograms.ExpectTotalCount("Conversions.Storage.MigrationTime", 0);
}

TEST_F(AttributionStorageSqlMigrationsTest, MigrateVersion52ToCurrent) {
  base::HistogramTester histograms;
  LoadDatabase(GetVersionFilePath(52), DbPath());

  // Verify pre-conditions.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));

    sql::Statement s(db.GetUniqueStatement(
        "SELECT aggregatable_budget_consumed FROM sources"));
    ASSERT_TRUE(s.Step());
    ASSERT_EQ(0, s.ColumnInt(0));
    ASSERT_TRUE(s.Step());
    ASSERT_EQ(200, s.ColumnInt(0));
    ASSERT_FALSE(s.Step());
  }
  MigrateDatabase();

  // Verify schema is current.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));

    CheckVersionNumbers(&db);

    // Compare normalized schemas
    EXPECT_EQ(NormalizeSchema(GetCurrentSchema()),
              NormalizeSchema(db.GetSchema()));

    // Verify that data is preserved across the migration.
    sql::Statement s(
        db.GetUniqueStatement("SELECT aggregatable_budget_consumed, "
                              "num_aggregatable_reports FROM sources"));
    ASSERT_TRUE(s.Step());
    // First source has no budget consumed so hasn't made any reports.
    ASSERT_EQ(0, s.ColumnInt(0));
    ASSERT_EQ(0, s.ColumnInt(1));
    ASSERT_TRUE(s.Step());
    // Second source has budget consumed so we set their num reports to 1.
    ASSERT_EQ(200, s.ColumnInt(0));
    ASSERT_EQ(1, s.ColumnInt(1));
    ASSERT_FALSE(s.Step());
  }

  // DB creation histograms should be recorded.
  histograms.ExpectTotalCount("Conversions.Storage.CreationTime", 0);
  histograms.ExpectTotalCount("Conversions.Storage.MigrationTime", 1);
}

TEST_F(AttributionStorageSqlMigrationsTest, MigrateVersion53ToCurrent) {
  base::HistogramTester histograms;
  LoadDatabase(GetVersionFilePath(53), DbPath());

  // Verify pre-conditions.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));

    sql::Statement s(
        db.GetUniqueStatement("SELECT reporting_origin FROM rate_limits"));
    ASSERT_TRUE(s.Step());
    ASSERT_EQ("https://a.r.test", s.ColumnString(0));
  }
  MigrateDatabase();

  // Verify schema is current.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));

    CheckVersionNumbers(&db);

    // Compare normalized schemas
    EXPECT_EQ(NormalizeSchema(GetCurrentSchema()),
              NormalizeSchema(db.GetSchema()));

    // Verify that data is preserved across the migration.
    sql::Statement s(
        db.GetUniqueStatement("SELECT reporting_site FROM rate_limits"));
    ASSERT_TRUE(s.Step());
    ASSERT_EQ("https://r.test", s.ColumnString(0));
    ASSERT_FALSE(s.Step());
  }

  // DB creation histograms should be recorded.
  histograms.ExpectTotalCount("Conversions.Storage.CreationTime", 0);
  histograms.ExpectTotalCount("Conversions.Storage.MigrationTime", 1);
}

TEST_F(AttributionStorageSqlMigrationsTest, MigrateVersion54ToCurrent) {
  base::HistogramTester histograms;
  LoadDatabase(GetVersionFilePath(54), DbPath());

  // Verify pre-conditions.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));

    sql::Statement s(
        db.GetUniqueStatement("SELECT reporting_origin FROM rate_limits"));
    ASSERT_TRUE(s.Step());
    ASSERT_EQ("https://a.r.test", s.ColumnString(0));
  }
  MigrateDatabase();

  // Verify schema is current.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));

    CheckVersionNumbers(&db);

    // Compare normalized schemas
    EXPECT_EQ(NormalizeSchema(GetCurrentSchema()),
              NormalizeSchema(db.GetSchema()));

    // Verify that data is preserved across the migration.
    sql::Statement s(
        db.GetUniqueStatement("SELECT reporting_origin FROM rate_limits"));
    ASSERT_TRUE(s.Step());
    ASSERT_EQ("https://a.r.test", s.ColumnString(0));
    ASSERT_FALSE(s.Step());
  }

  // DB creation histograms should be recorded.
  histograms.ExpectTotalCount("Conversions.Storage.CreationTime", 0);
  histograms.ExpectTotalCount("Conversions.Storage.MigrationTime", 1);
}

TEST_F(AttributionStorageSqlMigrationsTest, MigrateVersion55ToCurrent) {
  base::HistogramTester histograms;
  LoadDatabase(GetVersionFilePath(55), DbPath());

  // Verify pre-conditions.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));
    ASSERT_FALSE(db.DoesColumnExist("sources", "read_only_source_data"));
  }
  MigrateDatabase();

  // Verify schema is current.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));

    CheckVersionNumbers(&db);

    // Compare normalized schemas
    EXPECT_EQ(NormalizeSchema(GetCurrentSchema()),
              NormalizeSchema(db.GetSchema()));

    // Verify that data is preserved across the migration.
    ASSERT_TRUE(db.DoesColumnExist("sources", "read_only_source_data"));
    sql::Statement s(
        db.GetUniqueStatement("SELECT read_only_source_data FROM sources"));
    ASSERT_TRUE(s.Step());
    proto::AttributionReadOnlySourceData msg;
    ASSERT_TRUE(msg.ParseFromString(s.ColumnString(0)));
    ASSERT_EQ(3, msg.max_event_level_reports());
    ASSERT_FALSE(s.Step());
  }

  // DB creation histograms should be recorded.
  histograms.ExpectTotalCount("Conversions.Storage.CreationTime", 0);
  histograms.ExpectTotalCount("Conversions.Storage.MigrationTime", 1);
}

}  // namespace content
