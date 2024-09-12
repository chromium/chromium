// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_storage_sql_migrations.h"

#include <string>
#include <string_view>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_reporting.pb.h"
#include "content/browser/attribution_reporting/attribution_resolver.h"
#include "content/browser/attribution_reporting/attribution_resolver_impl.h"
#include "content/browser/attribution_reporting/attribution_storage_sql.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/store_source_result.h"
#include "content/browser/attribution_reporting/test/configurable_storage_delegate.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/test/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

using ::testing::ElementsAre;

// Normalize schema strings to compare them reliabily. Notably, applies the
// following transformations:
// - Remove quotes as sometimes migrations cause table names to be string
//   literals.
// - Replaces ", " with "," as CREATE TABLE in schema will be represented with
//   or without a space depending if it got there by calling CREATE TABLE
//   directly or with an ALTER TABLE.
std::string NormalizeSchema(std::string_view input) {
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
    AttributionResolverImpl storage(
        temp_directory_.GetPath(),
        std::make_unique<ConfigurableStorageDelegate>());

    // We need to run an operation on storage to force the lazy initialization.
    std::ignore =
        static_cast<AttributionResolver*>(&storage)->GetAttributionReports(
            base::Time::Min());
  }

  base::FilePath DbPath() {
    return temp_directory_.GetPath().Append(FILE_PATH_LITERAL("Conversions"));
  }

  static base::FilePath GetVersionFilePath(int version_id) {
    return base::FilePath::FromASCII(
        base::StrCat({"version_", base::NumberToString(version_id), ".sql"}));
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
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_path);
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
    ASSERT_TRUE(db.Execute(contents));
  }

  base::ScopedTempDir temp_directory_;
};

TEST_F(AttributionStorageSqlMigrationsTest, MigrateEmptyToCurrent) {
  base::HistogramTester histograms;
  {
    AttributionResolverImpl storage(
        temp_directory_.GetPath(),
        std::make_unique<ConfigurableStorageDelegate>());

    // We need to perform an operation that is non-trivial on an empty database
    // to force initialization.
    static_cast<AttributionResolver*>(&storage)->StoreSource(
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
    sql::Statement s(db.GetUniqueStatement(
        "SELECT remaining_aggregatable_attribution_budget, "
        "num_aggregatable_attribution_reports FROM sources"));
    ASSERT_TRUE(s.Step());
    // First source has no budget consumed so hasn't made any reports.
    ASSERT_EQ(65536, s.ColumnInt(0));
    ASSERT_EQ(0, s.ColumnInt(1));
    ASSERT_TRUE(s.Step());
    // Second source has budget consumed so we set their num reports to 1.
    ASSERT_EQ(65336, s.ColumnInt(0));
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
    ASSERT_EQ("https://a.r.test", s.ColumnStringView(0));
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
        db.GetUniqueStatement("SELECT reporting_site,scope FROM "
                              "rate_limits ORDER BY id"));
    ASSERT_TRUE(s.Step());
    ASSERT_EQ("https://r.test", s.ColumnStringView(0));
    ASSERT_EQ(1, s.ColumnInt(1));
    ASSERT_TRUE(s.Step());
    ASSERT_EQ("https://r.test", s.ColumnStringView(0));
    ASSERT_EQ(2, s.ColumnInt(1));
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
    ASSERT_EQ("https://a.r.test", s.ColumnStringView(0));
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
        db.GetUniqueStatement("SELECT reporting_origin,scope FROM "
                              "rate_limits ORDER BY id"));
    ASSERT_TRUE(s.Step());
    ASSERT_EQ("https://a.r.test", s.ColumnStringView(0));
    ASSERT_EQ(1, s.ColumnInt(1));
    ASSERT_TRUE(s.Step());
    ASSERT_EQ("https://a.r.test", s.ColumnStringView(0));
    ASSERT_EQ(2, s.ColumnInt(1));
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
    {
      base::span<const uint8_t> blob = s.ColumnBlob(0);
      ASSERT_TRUE(msg.ParseFromArray(blob.data(), blob.size()));
    }
    EXPECT_EQ(3, msg.max_event_level_reports());
    EXPECT_FALSE(msg.has_randomized_response_rate());
    EXPECT_EQ(0, msg.event_level_report_window_start_time());
    EXPECT_THAT(msg.event_level_report_window_end_times(),
                ElementsAre(base::Hours(1).InMicroseconds()));
    ASSERT_FALSE(s.Step());
  }

  // DB creation histograms should be recorded.
  histograms.ExpectTotalCount("Conversions.Storage.CreationTime", 0);
  histograms.ExpectTotalCount("Conversions.Storage.MigrationTime", 1);
}

TEST_F(AttributionStorageSqlMigrationsTest, MigrateVersion56ToCurrent) {
  base::HistogramTester histograms;
  LoadDatabase(GetVersionFilePath(56), DbPath());

  {
    // Verify pre-conditions.
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));
  }
  {
    AttributionResolverImpl storage(
        temp_directory_.GetPath(),
        std::make_unique<ConfigurableStorageDelegate>());

    // Store a valid report to verify corruption deletion.
    static_cast<AttributionResolver*>(&storage)->StoreSource(
        SourceBuilder().Build());
    static_cast<AttributionResolver*>(&storage)->MaybeCreateAndStoreReport(
        DefaultTrigger());
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

    // Testing deletion of corrupted reports.
    size_t rows;
    static constexpr const char* kTablesExpectOne[] = {"sources", "reports",
                                                       "source_destinations"};
    for (const char* table : kTablesExpectOne) {
      sql::test::CountTableRows(&db, table, &rows);
      EXPECT_EQ(1u, rows) << table;
    }

    sql::test::CountTableRows(&db, "dedup_keys", &rows);
    EXPECT_EQ(0u, rows) << "dedup_keys";

    histograms.ExpectUniqueSample(
        "Conversions.CorruptSourcesDeletedOnMigration", 1, 1);
    histograms.ExpectUniqueSample(
        "Conversions.CorruptReportsDeletedOnMigration", 2, 1);
  }

  // DB creation histograms should be recorded.
  histograms.ExpectTotalCount("Conversions.Storage.CreationTime", 0);
  histograms.ExpectTotalCount("Conversions.Storage.MigrationTime", 1);
}

TEST_F(AttributionStorageSqlMigrationsTest, MigrateVersion58ToCurrent) {
  base::HistogramTester histograms;
  LoadDatabase(GetVersionFilePath(58), DbPath());

  // Verify pre-conditions.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));

    sql::Statement s(db.GetUniqueStatement(
        "SELECT reporting_origin,scope FROM rate_limits ORDER BY id"));
    ASSERT_TRUE(s.Step());
    ASSERT_EQ("https://a.r.test", s.ColumnStringView(0));
    ASSERT_EQ(0, s.ColumnInt(1));
    ASSERT_TRUE(s.Step());
    ASSERT_EQ("https://b.r.test", s.ColumnStringView(0));
    ASSERT_EQ(1, s.ColumnInt(1));
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
        db.GetUniqueStatement("SELECT reporting_origin,scope,report_id FROM "
                              "rate_limits ORDER BY id"));
    ASSERT_TRUE(s.Step());
    ASSERT_EQ("https://a.r.test", s.ColumnStringView(0));
    ASSERT_EQ(0, s.ColumnInt(1));
    ASSERT_EQ(-1, s.ColumnInt(2));
    ASSERT_TRUE(s.Step());
    ASSERT_EQ("https://b.r.test", s.ColumnStringView(0));
    ASSERT_EQ(1, s.ColumnInt(1));
    ASSERT_EQ(-1, s.ColumnInt(2));
    ASSERT_TRUE(s.Step());
    ASSERT_EQ("https://b.r.test", s.ColumnStringView(0));
    ASSERT_EQ(2, s.ColumnInt(1));
    ASSERT_EQ(-1, s.ColumnInt(2));
    ASSERT_FALSE(s.Step());
  }

  // DB creation histograms should be recorded.
  histograms.ExpectTotalCount("Conversions.Storage.CreationTime", 0);
  histograms.ExpectTotalCount("Conversions.Storage.MigrationTime", 1);
}

TEST_F(AttributionStorageSqlMigrationsTest, MigrateVersion59ToCurrent) {
  base::HistogramTester histograms;
  LoadDatabase(GetVersionFilePath(59), DbPath());

  // Verify pre-conditions.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));

    sql::Statement s(
        db.GetUniqueStatement("SELECT aggregatable_budget_consumed,"
                              "num_aggregatable_reports FROM sources"));
    ASSERT_TRUE(s.Step());
    ASSERT_EQ(300, s.ColumnInt(0));
    ASSERT_EQ(6, s.ColumnInt(1));
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
    sql::Statement s(db.GetUniqueStatement(
        "SELECT remaining_aggregatable_attribution_budget,"
        "num_aggregatable_attribution_reports FROM sources"));
    ASSERT_TRUE(s.Step());
    ASSERT_EQ(65236, s.ColumnInt(0));
    ASSERT_EQ(6, s.ColumnInt(1));
    ASSERT_FALSE(s.Step());
  }

  // DB creation histograms should be recorded.
  histograms.ExpectTotalCount("Conversions.Storage.CreationTime", 0);
  histograms.ExpectTotalCount("Conversions.Storage.MigrationTime", 1);
}

TEST_F(AttributionStorageSqlMigrationsTest, MigrateVersion60ToCurrent) {
  base::HistogramTester histograms;
  LoadDatabase(GetVersionFilePath(60), DbPath());

  // Verify pre-conditions.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));
    ASSERT_FALSE(
        db.DoesColumnExist("sources", "remaining_aggregatable_debug_budget"));
    ASSERT_FALSE(
        db.DoesColumnExist("sources", "num_aggregatable_debug_reports"));
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

    sql::Statement s(
        db.GetUniqueStatement("SELECT "
                              "remaining_aggregatable_debug_budget,"
                              "num_aggregatable_debug_reports FROM sources"));
    ASSERT_TRUE(s.Step());
    EXPECT_EQ(0, s.ColumnInt(0));
    EXPECT_EQ(0, s.ColumnInt(1));
    ASSERT_FALSE(s.Step());
  }

  // DB creation histograms should be recorded.
  histograms.ExpectTotalCount("Conversions.Storage.CreationTime", 0);
  histograms.ExpectTotalCount("Conversions.Storage.MigrationTime", 1);
}

TEST_F(AttributionStorageSqlMigrationsTest, MigrateVersion61ToCurrent) {
  base::HistogramTester histograms;
  LoadDatabase(GetVersionFilePath(61), DbPath());

  // Verify pre-conditions.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));
    ASSERT_FALSE(db.DoesTableExist("aggregatable_debug_rate_limits"));
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

    ASSERT_TRUE(db.DoesTableExist("aggregatable_debug_rate_limits"));

    // Verify the new table is empty.
    sql::Statement s(
        db.GetUniqueStatement("SELECT * FROM aggregatable_debug_rate_limits"));
    ASSERT_FALSE(s.Step());
  }

  // DB creation histograms should be recorded.
  histograms.ExpectTotalCount("Conversions.Storage.CreationTime", 0);
  histograms.ExpectTotalCount("Conversions.Storage.MigrationTime", 1);
}

TEST_F(AttributionStorageSqlMigrationsTest, MigrateVersion62ToCurrent) {
  base::HistogramTester histograms;
  LoadDatabase(GetVersionFilePath(62), DbPath());

  // Verify pre-conditions.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));
    ASSERT_FALSE(db.DoesColumnExist(
        "rate_limits", "deactivated_for_source_destination_limit"));
    ASSERT_FALSE(
        db.DoesColumnExist("rate_limits", "destination_limit_priority"));
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

    sql::Statement s(
        db.GetUniqueStatement("SELECT "
                              "deactivated_for_source_destination_limit,"
                              "destination_limit_priority FROM rate_limits"));
    ASSERT_TRUE(s.Step());
    ASSERT_EQ(0, s.ColumnInt(0));
    ASSERT_EQ(0, s.ColumnInt(1));
    ASSERT_FALSE(s.Step());
  }

  // DB creation histograms should be recorded.
  histograms.ExpectTotalCount("Conversions.Storage.CreationTime", 0);
  histograms.ExpectTotalCount("Conversions.Storage.MigrationTime", 1);
}

TEST_F(AttributionStorageSqlMigrationsTest, MigrateVersion63ToCurrent) {
  base::HistogramTester histograms;
  LoadDatabase(GetVersionFilePath(63), DbPath());

  // Verify pre-conditions.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));
    ASSERT_FALSE(db.DoesColumnExist("sources", "attribution_scopes_data"));
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
    sql::Statement s(
        db.GetUniqueStatement("SELECT attribution_scopes_data FROM sources"));
    ASSERT_TRUE(s.Step());
    EXPECT_EQ(sql::ColumnType::kNull, s.GetColumnType(0));
    ASSERT_FALSE(s.Step());
  }

  // DB creation histograms should be recorded.
  histograms.ExpectTotalCount("Conversions.Storage.CreationTime", 0);
  histograms.ExpectTotalCount("Conversions.Storage.MigrationTime", 1);
}

}  // namespace content
