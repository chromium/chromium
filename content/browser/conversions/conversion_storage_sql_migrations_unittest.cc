// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/conversions/conversion_storage_sql.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "content/browser/conversions/conversion_storage.h"
#include "content/browser/conversions/conversion_test_utils.h"
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

const int kCurrentVersionNumber = 2;

}  // namespace

class ConversionStorageSqlMigrationsTest : public testing::Test {
 public:
  ConversionStorageSqlMigrationsTest() = default;

  void SetUp() override { ASSERT_TRUE(temp_directory_.CreateUniqueTempDir()); }

  void MigrateDatabase() {
    base::SimpleTestClock clock;
    ConversionStorageSql storage(
        temp_directory_.GetPath(),
        std::make_unique<ConfigurableStorageDelegate>(), &clock);

    // We need to run an operation on storage to force the lazy initialization.
    static_cast<ConversionStorage*>(&storage)->GetConversionsToReport(
        base::Time::Min());
  }

  base::FilePath DbPath() {
    return temp_directory_.GetPath().Append(FILE_PATH_LITERAL("Conversions"));
  }

  std::string GetCurrentSchema() {
    base::FilePath current_version_path = temp_directory_.GetPath().Append(
        FILE_PATH_LITERAL("TestCurrentVersion.db"));
    LoadDatabase(FILE_PATH_LITERAL("version_2.sql"), current_version_path);
    sql::Database db;
    EXPECT_TRUE(db.Open(current_version_path));
    return db.GetSchema();
  }

 protected:
  // The textual contents of |file| are read from
  // "content/test/data/conversions/databases/" and returned in the string
  // |contents|. Returns true if the file exists and is read successfully, false
  // otherwise.
  bool GetDatabaseData(const base::FilePath& file, std::string* contents) {
    base::FilePath source_path;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &source_path);
    source_path =
        source_path.AppendASCII("content/test/data/conversions/databases");
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

TEST_F(ConversionStorageSqlMigrationsTest, MigrateEmptyToCurrent) {
  base::HistogramTester histograms;
  {
    base::SimpleTestClock clock;
    ConversionStorageSql storage(
        temp_directory_.GetPath(),
        std::make_unique<ConfigurableStorageDelegate>(), &clock);

    // We need to perform an operation that is non-trivial on an empty database
    // to force initialization.
    static_cast<ConversionStorage*>(&storage)->StoreImpression(
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

TEST_F(ConversionStorageSqlMigrationsTest, MigrateVersion1ToCurrent) {
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

}  // namespace content
