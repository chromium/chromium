// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_storage_sql_migrations.h"

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
#include "content/browser/attribution_reporting/attribution_storage_sql.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
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

    sql::Statement s(
        db.GetUniqueStatement("SELECT COUNT(*) FROM event_level_reports"));

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
    sql::Statement s(
        db.GetUniqueStatement("SELECT COUNT(*) FROM event_level_reports"));

    ASSERT_TRUE(s.Step());
    ASSERT_EQ(0, s.ColumnInt(0));
  }

  // DB creation histograms should be recorded.
  histograms.ExpectTotalCount("Conversions.Storage.CreationTime", 1);
  histograms.ExpectTotalCount("Conversions.Storage.MigrationTime", 0);
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

    CheckVersionNumbers(&db);

    // Compare normalized schemas
    EXPECT_EQ(NormalizeSchema(GetCurrentSchema()),
              NormalizeSchema(db.GetSchema()));

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

    CheckVersionNumbers(&db);

    // Compare normalized schemas
    EXPECT_EQ(NormalizeSchema(GetCurrentSchema()),
              NormalizeSchema(db.GetSchema()));

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

    sql::Statement s(db.GetUniqueStatement(
        "SELECT expiry_time,num_attributions FROM sources"));

    ASSERT_TRUE(s.Step());
    ASSERT_EQ(8, s.ColumnInt(0));  // expiry_time
    ASSERT_EQ(9, s.ColumnInt(1));  // num_attributions
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
        "SELECT "
        "expiry_time,event_report_window_time,aggregatable_report_window_time,"
        "num_attributions FROM sources"));

    ASSERT_TRUE(s.Step());
    ASSERT_EQ(8, s.ColumnInt(0));  // expiry_time
    ASSERT_EQ(8, s.ColumnInt(1));  // event_report_window_time
    ASSERT_EQ(8, s.ColumnInt(2));  // aggregatable_report_window_time
    ASSERT_EQ(9, s.ColumnInt(3));  // num_attributions
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

    CheckVersionNumbers(&db);

    // Compare normalized schemas
    EXPECT_EQ(NormalizeSchema(GetCurrentSchema()),
              NormalizeSchema(db.GetSchema()));

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

TEST_F(AttributionStorageSqlMigrationsTest, MigrateVersion39ToCurrent) {
  base::HistogramTester histograms;
  LoadDatabase(GetVersionFilePath(39), DbPath());

  // Verify pre-conditions.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));
    ASSERT_TRUE(db.DoesIndexExist("contribution_aggregation_id_idx"));

    sql::Statement s(
        db.GetUniqueStatement("SELECT * FROM aggregatable_contributions"));

    ASSERT_TRUE(s.Step());
    ASSERT_EQ(11, s.ColumnInt(0));  // contribution_id
    ASSERT_EQ(21, s.ColumnInt(1));  // aggregation_id
    ASSERT_EQ(31, s.ColumnInt(2));  // key_high_bits
    ASSERT_EQ(41, s.ColumnInt(3));  // key_low_bits
    ASSERT_EQ(51, s.ColumnInt(4));  // value
    ASSERT_TRUE(s.Step());
    ASSERT_EQ(12, s.ColumnInt(0));  // contribution_id
    ASSERT_EQ(22, s.ColumnInt(1));  // aggregation_id
    ASSERT_EQ(32, s.ColumnInt(2));  // key_high_bits
    ASSERT_EQ(42, s.ColumnInt(3));  // key_low_bits
    ASSERT_EQ(52, s.ColumnInt(4));  // value
    ASSERT_FALSE(s.Step());
  }

  MigrateDatabase();

  // Verify schema is current.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));
    ASSERT_FALSE(db.DoesIndexExist("contribution_aggregation_id_idx"));

    CheckVersionNumbers(&db);

    // Compare without quotes as sometimes migrations cause table names to be
    // string literals.
    EXPECT_EQ(NormalizeSchema(GetCurrentSchema()),
              NormalizeSchema(db.GetSchema()));

    // Verify that data is preserved across the migration.
    sql::Statement s(
        db.GetUniqueStatement("SELECT * FROM aggregatable_contributions"));

    ASSERT_TRUE(s.Step());
    ASSERT_EQ(21, s.ColumnInt(0));  // aggregation_id
    ASSERT_EQ(11, s.ColumnInt(1));  // contribution_id
    ASSERT_EQ(31, s.ColumnInt(2));  // key_high_bits
    ASSERT_EQ(41, s.ColumnInt(3));  // key_low_bits
    ASSERT_EQ(51, s.ColumnInt(4));  // value
    ASSERT_TRUE(s.Step());
    ASSERT_EQ(22, s.ColumnInt(0));  // aggregation_id
    ASSERT_EQ(12, s.ColumnInt(1));  // contribution_id
    ASSERT_EQ(32, s.ColumnInt(2));  // key_high_bits
    ASSERT_EQ(42, s.ColumnInt(3));  // key_low_bits
    ASSERT_EQ(52, s.ColumnInt(4));  // value
    ASSERT_FALSE(s.Step());
  }

  // DB creation histograms should be recorded.
  histograms.ExpectTotalCount("Conversions.Storage.CreationTime", 0);
  histograms.ExpectTotalCount("Conversions.Storage.MigrationTime", 1);
}

TEST_F(AttributionStorageSqlMigrationsTest, MigrateVersion40ToCurrent) {
  base::HistogramTester histograms;
  LoadDatabase(GetVersionFilePath(40), DbPath());

  // Verify pre-conditions.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));
    ASSERT_FALSE(db.DoesColumnExist("aggregatable_report_metadata",
                                    "attestation_token"));

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

    CheckVersionNumbers(&db);

    // Compare normalized schemas
    EXPECT_EQ(NormalizeSchema(GetCurrentSchema()),
              NormalizeSchema(db.GetSchema()));

    // Verify that data is preserved across the migration.
    sql::Statement s(
        db.GetUniqueStatement("SELECT * FROM aggregatable_report_metadata"));

    ASSERT_TRUE(s.Step());
    ASSERT_EQ(1, s.ColumnInt(0));                           // aggregation_id
    ASSERT_EQ(sql::ColumnType::kNull, s.GetColumnType(9));  // attestation_token
    ASSERT_FALSE(s.Step());
  }

  // DB creation histograms should be recorded.
  histograms.ExpectTotalCount("Conversions.Storage.CreationTime", 0);
  histograms.ExpectTotalCount("Conversions.Storage.MigrationTime", 1);
}

TEST_F(AttributionStorageSqlMigrationsTest, MigrateVersion41ToCurrent) {
  base::HistogramTester histograms;
  LoadDatabase(GetVersionFilePath(41), DbPath());

  // Verify pre-conditions.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));
    ASSERT_TRUE(db.DoesColumnExist("rate_limits", "source_origin"));
    ASSERT_TRUE(db.DoesColumnExist("rate_limits", "destination_origin"));
    ASSERT_FALSE(db.DoesColumnExist("rate_limits", "context_origin"));

    static constexpr char kSql[] =
        "SELECT source_origin,destination_origin FROM rate_limits";
    sql::Statement s(db.GetUniqueStatement(kSql));

    ASSERT_TRUE(s.Step());
    ASSERT_EQ("b", s.ColumnString(0));
    ASSERT_EQ("d", s.ColumnString(1));
    ASSERT_TRUE(s.Step());
    ASSERT_EQ("g", s.ColumnString(0));
    ASSERT_EQ("i", s.ColumnString(1));
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
        db.GetUniqueStatement("SELECT context_origin FROM rate_limits"));

    ASSERT_TRUE(s.Step());
    ASSERT_EQ("b", s.ColumnString(0));  // from source_origin
    ASSERT_TRUE(s.Step());
    ASSERT_EQ("i", s.ColumnString(0));  // from destination_origin
    ASSERT_FALSE(s.Step());
  }

  // DB creation histograms should be recorded.
  histograms.ExpectTotalCount("Conversions.Storage.CreationTime", 0);
  histograms.ExpectTotalCount("Conversions.Storage.MigrationTime", 1);
}

TEST_F(AttributionStorageSqlMigrationsTest, MigrateVersion42ToCurrent) {
  base::HistogramTester histograms;
  LoadDatabase(GetVersionFilePath(42), DbPath());

  // Verify pre-conditions.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));
    ASSERT_TRUE(db.DoesColumnExist("rate_limits", "expiry_time"));
    ASSERT_FALSE(
        db.DoesColumnExist("rate_limits", "source_expiry_or_attribution_time"));

    static constexpr char kSql[] = "SELECT expiry_time FROM rate_limits";
    sql::Statement s(db.GetUniqueStatement(kSql));

    ASSERT_TRUE(s.Step());
    ASSERT_EQ(7, s.ColumnInt64(0));
    ASSERT_TRUE(s.Step());
    ASSERT_EQ(10, s.ColumnInt64(0));
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
        "SELECT source_expiry_or_attribution_time FROM rate_limits"));

    ASSERT_TRUE(s.Step());
    ASSERT_EQ(7, s.ColumnInt64(0));  // unchanged
    ASSERT_TRUE(s.Step());
    ASSERT_EQ(9, s.ColumnInt64(0));  // from time
    ASSERT_FALSE(s.Step());
  }

  // DB creation histograms should be recorded.
  histograms.ExpectTotalCount("Conversions.Storage.CreationTime", 0);
  histograms.ExpectTotalCount("Conversions.Storage.MigrationTime", 1);
}

TEST_F(AttributionStorageSqlMigrationsTest, MigrateVersion43ToCurrent) {
  base::HistogramTester histograms;
  LoadDatabase(GetVersionFilePath(43), DbPath());

  // Verify pre-conditions.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));

    {
      static constexpr char kSql[] = "SELECT * FROM event_level_reports";
      sql::Statement s(db.GetUniqueStatement(kSql));

      ASSERT_TRUE(s.Step());
      ASSERT_EQ(1, s.ColumnInt(0));
      ASSERT_EQ(2, s.ColumnInt(1));
      ASSERT_EQ(3, s.ColumnInt(2));
      ASSERT_EQ(4, s.ColumnInt(3));
      ASSERT_EQ(5, s.ColumnInt(4));
      ASSERT_EQ(6, s.ColumnInt(5));
      ASSERT_EQ(7, s.ColumnInt(6));
      ASSERT_EQ(8, s.ColumnInt(7));
      ASSERT_EQ(9, s.ColumnInt(8));
      ASSERT_FALSE(s.Step());
    }

    {
      static constexpr char kSql[] =
          "SELECT * FROM aggregatable_report_metadata";
      sql::Statement s(db.GetUniqueStatement(kSql));

      ASSERT_TRUE(s.Step());
      ASSERT_EQ(1, s.ColumnInt(0));
      ASSERT_EQ(2, s.ColumnInt(1));
      ASSERT_EQ(3, s.ColumnInt(2));
      ASSERT_EQ(4, s.ColumnInt(3));
      ASSERT_EQ(5, s.ColumnInt(4));
      ASSERT_EQ(6, s.ColumnInt(5));
      ASSERT_EQ(7, s.ColumnInt(6));
      ASSERT_EQ(8, s.ColumnInt(7));
      ASSERT_EQ(9, s.ColumnInt(8));
      ASSERT_EQ(10, s.ColumnInt(9));
      ASSERT_FALSE(s.Step());
    }
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

    {
      static constexpr char kSql[] = "SELECT * FROM event_level_reports";
      sql::Statement s(db.GetUniqueStatement(kSql));

      ASSERT_TRUE(s.Step());
      ASSERT_EQ(1, s.ColumnInt(0));
      ASSERT_EQ(2, s.ColumnInt(1));
      ASSERT_EQ(3, s.ColumnInt(2));
      ASSERT_EQ(4, s.ColumnInt(3));
      ASSERT_EQ(5, s.ColumnInt(4));
      ASSERT_EQ(6, s.ColumnInt(5));
      ASSERT_EQ(7, s.ColumnInt(6));
      ASSERT_EQ(8, s.ColumnInt(7));
      ASSERT_EQ(9, s.ColumnInt(8));
      ASSERT_EQ("https://d.test", s.ColumnString(9));
      ASSERT_FALSE(s.Step());
    }

    {
      static constexpr char kSql[] =
          "SELECT * FROM aggregatable_report_metadata";
      sql::Statement s(db.GetUniqueStatement(kSql));

      ASSERT_TRUE(s.Step());
      ASSERT_EQ(1, s.ColumnInt(0));
      ASSERT_EQ(2, s.ColumnInt(1));
      ASSERT_EQ(3, s.ColumnInt(2));
      ASSERT_EQ(4, s.ColumnInt(3));
      ASSERT_EQ(5, s.ColumnInt(4));
      ASSERT_EQ(6, s.ColumnInt(5));
      ASSERT_EQ(7, s.ColumnInt(6));
      ASSERT_EQ(8, s.ColumnInt(7));
      ASSERT_EQ(9, s.ColumnInt(8));
      ASSERT_EQ(10, s.ColumnInt(9));
      ASSERT_EQ("https://d.test", s.ColumnString(10));
      ASSERT_FALSE(s.Step());
    }
  }

  // DB creation histograms should be recorded.
  histograms.ExpectTotalCount("Conversions.Storage.CreationTime", 0);
  histograms.ExpectTotalCount("Conversions.Storage.MigrationTime", 1);
}

TEST_F(AttributionStorageSqlMigrationsTest, MigrateVersion44ToCurrent) {
  base::HistogramTester histograms;
  LoadDatabase(GetVersionFilePath(44), DbPath());

  // Verify pre-conditions.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));

    {
      static constexpr char kSql[] =
          "SELECT destination_origin FROM event_level_reports";
      sql::Statement s(db.GetUniqueStatement(kSql));

      ASSERT_TRUE(s.Step());
      ASSERT_EQ("https://a.d.test", s.ColumnString(0));
      ASSERT_FALSE(s.Step());
    }
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

    {
      static constexpr char kSql[] =
          "SELECT context_origin FROM event_level_reports";
      sql::Statement s(db.GetUniqueStatement(kSql));

      ASSERT_TRUE(s.Step());
      ASSERT_EQ("https://a.d.test", s.ColumnString(0));
      ASSERT_FALSE(s.Step());
    }
  }

  // DB creation histograms should be recorded.
  histograms.ExpectTotalCount("Conversions.Storage.CreationTime", 0);
  histograms.ExpectTotalCount("Conversions.Storage.MigrationTime", 1);
}

TEST_F(AttributionStorageSqlMigrationsTest, MigrateVersion45ToCurrent) {
  base::HistogramTester histograms;
  LoadDatabase(GetVersionFilePath(45), DbPath());

  // Verify pre-conditions.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));
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
  }

  // DB creation histograms should be recorded.
  histograms.ExpectTotalCount("Conversions.Storage.CreationTime", 0);
  histograms.ExpectTotalCount("Conversions.Storage.MigrationTime", 1);
}

TEST_F(AttributionStorageSqlMigrationsTest, MigrateVersion46ToCurrent) {
  base::HistogramTester histograms;
  LoadDatabase(GetVersionFilePath(46), DbPath());

  // Verify pre-conditions.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));
    ASSERT_TRUE(db.DoesColumnExist("sources", "destination_site"));

    sql::Statement s(db.GetUniqueStatement(
        "SELECT source_id,destination_site FROM sources"));

    ASSERT_TRUE(s.Step());
    ASSERT_EQ(2, s.ColumnInt(0));
    ASSERT_EQ("13", s.ColumnString(1));
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
        "SELECT source_id,destination_site FROM source_destinations"));

    ASSERT_TRUE(s.Step());
    ASSERT_EQ(2, s.ColumnInt(0));
    ASSERT_EQ("13", s.ColumnString(1));
    ASSERT_FALSE(s.Step());
  }

  // DB creation histograms should be recorded.
  histograms.ExpectTotalCount("Conversions.Storage.CreationTime", 0);
  histograms.ExpectTotalCount("Conversions.Storage.MigrationTime", 1);
}

}  // namespace content
