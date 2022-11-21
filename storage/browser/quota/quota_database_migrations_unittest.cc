// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/services/storage/public/cpp/constants.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/test/test_helpers.h"
#include "storage/browser/quota/quota_database.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace storage {

namespace {

const int kCurrentSchemaVersion = 9;
const int kCurrentCompatibleVersion = 9;

std::string RemoveQuotes(std::string input) {
  std::string output;
  base::RemoveChars(input, "\"", &output);
  return output;
}

}  // namespace

class QuotaDatabaseMigrationsTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_directory_.CreateUniqueTempDir());
    histograms_ = std::make_unique<base::HistogramTester>();
  }

  base::FilePath ProfilePath() { return temp_directory_.GetPath(); }

  base::FilePath DbPath() {
    return ProfilePath()
        .Append(kWebStorageDirectory)
        .AppendASCII("QuotaManager");
  }

 protected:
  // The textual contents of |file| are read from
  // "storage/test/data/quota_database/" and returned in the string
  // |contents|. Returns true if the file exists and is read successfully, false
  // otherwise.
  std::string GetDatabaseData(const char* file) {
    base::FilePath source_path;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &source_path);
    source_path = source_path.AppendASCII("storage/test/data/quota_database");
    source_path = source_path.AppendASCII(file);
    EXPECT_TRUE(base::PathExists(source_path));

    std::string contents;
    base::ReadFileToString(source_path, &contents);
    return contents;
  }

  bool LoadDatabase(const char* file, const base::FilePath& db_path) {
    std::string contents = GetDatabaseData(file);
    if (contents.empty())
      return false;

    sql::Database db;
    if (!base::CreateDirectory(db_path.DirName()) || !db.Open(db_path) ||
        !db.Execute(contents.data()))
      return false;
    return true;
  }

  void MigrateDatabase() {
    QuotaDatabase db(ProfilePath());
    EXPECT_EQ(db.EnsureOpened(), QuotaError::kNone);

    DCHECK_CALLED_ON_VALID_SEQUENCE(db.sequence_checker_);
    EXPECT_TRUE(db.db_.get());
  }

  std::string GetCurrentSchema() {
    base::FilePath current_version_path =
        temp_directory_.GetPath().AppendASCII("current_version.db");
    EXPECT_TRUE(LoadDatabase("version_9.sql", current_version_path));
    sql::Database db;
    EXPECT_TRUE(db.Open(current_version_path));
    return db.GetSchema();
  }

  std::string GetQuotaDatabaseSchema() {
    QuotaDatabase db(ProfilePath());
    EXPECT_EQ(db.EnsureOpened(), QuotaError::kNone);
    DCHECK_CALLED_ON_VALID_SEQUENCE(db.sequence_checker_);
    return db.db_->GetSchema();
  }

  size_t GetTotalHistogramCount() {
    return histograms_->GetTotalCountsForPrefix("Quota.DatabaseMigration")
        .size();
  }

  base::ScopedTempDir temp_directory_;
  std::unique_ptr<base::HistogramTester> histograms_;
};

// Verify that the schema created by a new `QuotaDatabase` instance matches the
// current test schema file.
TEST_F(QuotaDatabaseMigrationsTest, QuotaDatabaseSchemaMatchesTestSchema) {
  EXPECT_EQ(GetCurrentSchema(), GetQuotaDatabaseSchema());
}

TEST_F(QuotaDatabaseMigrationsTest, UpgradeSchemaFromV5) {
  ASSERT_TRUE(LoadDatabase("version_5.sql", DbPath()));

  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));

    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&db));
    sql::MetaTable meta_table;
    ASSERT_TRUE(
        meta_table.Init(&db, kCurrentSchemaVersion, kCurrentCompatibleVersion));
    ASSERT_EQ(meta_table.GetVersionNumber(), 5);
    ASSERT_EQ(meta_table.GetCompatibleVersionNumber(), 2);

    ASSERT_TRUE(db.DoesTableExist("HostQuotaTable"));
    ASSERT_TRUE(db.DoesTableExist("EvictionInfoTable"));
    ASSERT_TRUE(db.DoesTableExist("OriginInfoTable"));
    ASSERT_FALSE(db.DoesTableExist("buckets"));

    // Check populated data.
    EXPECT_EQ(
        "http://a/|0|123|13260644621105493|13242931862595604,"
        "http://b/|0|111|13250042735631065|13260999511438890,"
        "http://c/|1|321|13261163582572088|13261079941303629",
        sql::test::ExecuteWithResults(
            &db, "SELECT * FROM OriginInfoTable ORDER BY origin ASC", "|",
            ","));
    EXPECT_EQ("a.com,b.com,c.com",
              sql::test::ExecuteWithResults(
                  &db, "SELECT host FROM HostQuotaTable ORDER BY host ASC", "|",
                  ","));
  }

  MigrateDatabase();

  // Verify upgraded schema.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));

    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&db));
    sql::MetaTable meta_table;
    ASSERT_TRUE(
        meta_table.Init(&db, kCurrentSchemaVersion, kCurrentCompatibleVersion));
    ASSERT_EQ(meta_table.GetVersionNumber(), kCurrentSchemaVersion);
    ASSERT_EQ(meta_table.GetCompatibleVersionNumber(), kCurrentSchemaVersion);

    ASSERT_TRUE(db.DoesTableExist("quota"));
    ASSERT_TRUE(db.DoesTableExist("buckets"));
    ASSERT_FALSE(db.DoesTableExist("HostQuotaTable"));
    ASSERT_FALSE(db.DoesTableExist("EvictionInfoTable"));
    ASSERT_FALSE(db.DoesTableExist("OriginInfoTable"));

    // Check that OriginInfoTable data is migrated to bucket table.
    EXPECT_EQ(
        "1|http://a/|a|0|default|123|13260644621105493|13242931862595604|"
        "0|0|0|1,"
        "2|http://b/|b|0|default|111|13250042735631065|13260999511438890|"
        "0|0|0|1,"
        "3|http://c/|c|1|default|321|13261163582572088|13261079941303629|"
        "0|0|0|1",
        sql::test::ExecuteWithResults(
            &db, "SELECT * FROM buckets ORDER BY storage_key ASC", "|", ","));

    // Check that HostQuotaTable data is migrated to quota table.
    EXPECT_EQ("a.com,b.com,c.com",
              sql::test::ExecuteWithResults(
                  &db, "SELECT host FROM quota ORDER BY host ASC", "|", ","));

    EXPECT_EQ(GetCurrentSchema(), RemoveQuotes(db.GetSchema()));

    EXPECT_EQ(GetTotalHistogramCount(), 3u);
    histograms_->ExpectBucketCount("Quota.DatabaseMigrationFromV5ToV7",
                                   /*sample=*/true, /*expected_count=*/1);
    histograms_->ExpectBucketCount("Quota.DatabaseMigrationFromV7ToV8",
                                   /*sample=*/true, /*expected_count=*/1);
    histograms_->ExpectBucketCount("Quota.DatabaseMigrationFromV8ToV9",
                                   /*sample=*/true, /*expected_count=*/1);
  }
}

TEST_F(QuotaDatabaseMigrationsTest, UpgradeSchemaFromV6) {
  ASSERT_TRUE(LoadDatabase("version_6.sql", DbPath()));

  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));

    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&db));
    sql::MetaTable meta_table;
    ASSERT_TRUE(
        meta_table.Init(&db, kCurrentSchemaVersion, kCurrentCompatibleVersion));
    ASSERT_EQ(meta_table.GetVersionNumber(), 6);
    ASSERT_EQ(meta_table.GetCompatibleVersionNumber(), 6);

    ASSERT_TRUE(db.DoesTableExist("quota"));
    ASSERT_TRUE(db.DoesTableExist("buckets"));
    ASSERT_TRUE(db.DoesTableExist("eviction_info"));

    // Check populated data.
    EXPECT_EQ(
        "1|http://a/|0|bucket_a|123|13260644621105493|13242931862595604|"
        "9223372036854775807|0,"
        "2|http://b/|0|bucket_b|111|13250042735631065|13260999511438890|"
        "9223372036854775807|1000,"
        "3|http://c/|1|bucket_c|321|13261163582572088|13261079941303629|"
        "9223372036854775807|10000",
        sql::test::ExecuteWithResults(
            &db, "SELECT * FROM buckets ORDER BY id ASC", "|", ","));

    EXPECT_EQ("a.com,b.com,c.com",
              sql::test::ExecuteWithResults(
                  &db, "SELECT host FROM quota ORDER BY host ASC", "|", ","));
  }

  MigrateDatabase();

  // Verify upgraded schema.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));

    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&db));
    sql::MetaTable meta_table;
    ASSERT_TRUE(
        meta_table.Init(&db, kCurrentSchemaVersion, kCurrentCompatibleVersion));
    ASSERT_EQ(meta_table.GetVersionNumber(), kCurrentSchemaVersion);
    ASSERT_EQ(meta_table.GetCompatibleVersionNumber(), kCurrentSchemaVersion);

    ASSERT_TRUE(db.DoesTableExist("quota"));
    ASSERT_TRUE(db.DoesTableExist("buckets"));
    ASSERT_FALSE(db.DoesTableExist("eviction_info"));

    // Check that buckets data is still present.
    EXPECT_EQ(
        "1|http://a/|a|0|bucket_a|123|13260644621105493|13242931862595604|"
        "0|0|0|0,"
        "2|http://b/|b|0|bucket_b|111|13250042735631065|13260999511438890|"
        "0|1000|0|0,"
        "3|http://c/|c|1|bucket_c|321|13261163582572088|13261079941303629|"
        "0|10000|0|0",
        sql::test::ExecuteWithResults(
            &db, "SELECT * FROM buckets ORDER BY id ASC", "|", ","));

    // Check that quota data is still present.
    EXPECT_EQ("a.com,b.com,c.com",
              sql::test::ExecuteWithResults(
                  &db, "SELECT host FROM quota ORDER BY host ASC", "|", ","));

    EXPECT_EQ(GetCurrentSchema(), RemoveQuotes(db.GetSchema()));

    EXPECT_EQ(GetTotalHistogramCount(), 3u);
    histograms_->ExpectBucketCount("Quota.DatabaseMigrationFromV6ToV7",
                                   /*sample=*/true, /*expected_count=*/1);
    histograms_->ExpectBucketCount("Quota.DatabaseMigrationFromV7ToV8",
                                   /*sample=*/true, /*expected_count=*/1);
    histograms_->ExpectBucketCount("Quota.DatabaseMigrationFromV8ToV9",
                                   /*sample=*/true, /*expected_count=*/1);
  }
}

TEST_F(QuotaDatabaseMigrationsTest, UpgradeSchemaFromV7) {
  ASSERT_TRUE(LoadDatabase("version_7.sql", DbPath()));

  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));

    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&db));
    sql::MetaTable meta_table;
    ASSERT_TRUE(
        meta_table.Init(&db, kCurrentSchemaVersion, kCurrentCompatibleVersion));
    ASSERT_EQ(meta_table.GetVersionNumber(), 7);
    ASSERT_EQ(meta_table.GetCompatibleVersionNumber(), 7);

    ASSERT_TRUE(db.DoesTableExist("quota"));
    ASSERT_TRUE(db.DoesTableExist("buckets"));

    // Check populated data.
    EXPECT_EQ(
        "1|http://a/|0|bucket_a|123|13260644621105493|13242931862595604|"
        "9223372036854775807|0,"
        "2|http://b/|0|bucket_b|111|13250042735631065|13260999511438890|"
        "9223372036854775807|1000,"
        "3|chrome-extension://abc/|1|default|321|13261163582572088|"
        "13261079941303629|9223372036854775807|10000",
        sql::test::ExecuteWithResults(
            &db, "SELECT * FROM buckets ORDER BY id ASC", "|", ","));

    EXPECT_EQ("a.com,b.com,c.com",
              sql::test::ExecuteWithResults(
                  &db, "SELECT host FROM quota ORDER BY host ASC", "|", ","));
  }

  MigrateDatabase();

  // Verify upgraded schema.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));

    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&db));
    sql::MetaTable meta_table;
    ASSERT_TRUE(
        meta_table.Init(&db, kCurrentSchemaVersion, kCurrentCompatibleVersion));
    ASSERT_EQ(meta_table.GetVersionNumber(), kCurrentSchemaVersion);
    ASSERT_EQ(meta_table.GetCompatibleVersionNumber(), kCurrentSchemaVersion);

    ASSERT_TRUE(db.DoesTableExist("quota"));
    ASSERT_TRUE(db.DoesTableExist("buckets"));
    ASSERT_FALSE(db.DoesTableExist("eviction_info"));

    // Check that buckets data is still present.
    EXPECT_EQ(
        "1|http://a/|a|0|bucket_a|123|13260644621105493|13242931862595604|"
        "0|0|0|0,"
        "2|http://b/|b|0|bucket_b|111|13250042735631065|13260999511438890|"
        "0|1000|0|0,"
        "3|chrome-extension://abc/||1|default|321|13261163582572088|"
        "13261079941303629|0|10000|0|1",
        sql::test::ExecuteWithResults(
            &db, "SELECT * FROM buckets ORDER BY id ASC", "|", ","));

    // Check that quota data is still present.
    EXPECT_EQ("a.com,b.com,c.com",
              sql::test::ExecuteWithResults(
                  &db, "SELECT host FROM quota ORDER BY host ASC", "|", ","));

    EXPECT_EQ(GetCurrentSchema(), RemoveQuotes(db.GetSchema()));

    EXPECT_EQ(GetTotalHistogramCount(), 2u);
    histograms_->ExpectBucketCount("Quota.DatabaseMigrationFromV7ToV8",
                                   /*sample=*/true, /*expected_count=*/1);
    histograms_->ExpectBucketCount("Quota.DatabaseMigrationFromV8ToV9",
                                   /*sample=*/true, /*expected_count=*/1);
  }
}

TEST_F(QuotaDatabaseMigrationsTest, UpgradeSchemaFromV8) {
  ASSERT_TRUE(LoadDatabase("version_8.sql", DbPath()));

  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));

    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&db));
    sql::MetaTable meta_table;
    ASSERT_TRUE(
        meta_table.Init(&db, kCurrentSchemaVersion, kCurrentCompatibleVersion));
    ASSERT_EQ(meta_table.GetVersionNumber(), 8);
    ASSERT_EQ(meta_table.GetCompatibleVersionNumber(), 8);

    ASSERT_TRUE(db.DoesTableExist("quota"));
    ASSERT_TRUE(db.DoesTableExist("buckets"));

    // Check populated data.
    EXPECT_EQ(
        "1|http://a/|http://a/"
        "|0|bucket_a|123|13260644621105493|13242931862595604|"
        "9223372036854775807|0,"
        "2|http://b/|http://b/"
        "|0|bucket_b|111|13250042735631065|13260999511438890|"
        "9223372036854775807|1000,"
        "3|chrome-extension://abc/|chrome-extension://abc/"
        "|1|default|321|13261163582572088|"
        "13261079941303629|9223372036854775807|10000",
        sql::test::ExecuteWithResults(
            &db, "SELECT * FROM buckets ORDER BY id ASC", "|", ","));

    EXPECT_EQ("a.com,b.com,c.com",
              sql::test::ExecuteWithResults(
                  &db, "SELECT host FROM quota ORDER BY host ASC", "|", ","));
  }

  MigrateDatabase();

  // Verify upgraded schema.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(DbPath()));

    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&db));
    sql::MetaTable meta_table;
    ASSERT_TRUE(
        meta_table.Init(&db, kCurrentSchemaVersion, kCurrentCompatibleVersion));
    EXPECT_EQ(meta_table.GetVersionNumber(), kCurrentSchemaVersion);
    EXPECT_EQ(meta_table.GetCompatibleVersionNumber(), kCurrentSchemaVersion);

    ASSERT_TRUE(db.DoesTableExist("quota"));
    ASSERT_TRUE(db.DoesTableExist("buckets"));
    ASSERT_FALSE(db.DoesTableExist("eviction_info"));

    // Check that buckets data is still present.
    EXPECT_EQ(
        "1|http://a/|http://a/"
        "|0|bucket_a|123|13260644621105493|13242931862595604|"
        "0|0|0|0,"
        "2|http://b/|http://b/"
        "|0|bucket_b|111|13250042735631065|13260999511438890|"
        "0|1000|0|0,"
        "3|chrome-extension://abc/|chrome-extension://abc/"
        "|1|default|321|13261163582572088|"
        "13261079941303629|0|10000|0|1",
        sql::test::ExecuteWithResults(
            &db, "SELECT * FROM buckets ORDER BY id ASC", "|", ","));

    // Check that quota data is still present.
    EXPECT_EQ("a.com,b.com,c.com",
              sql::test::ExecuteWithResults(
                  &db, "SELECT host FROM quota ORDER BY host ASC", "|", ","));

    EXPECT_EQ(GetCurrentSchema(), RemoveQuotes(db.GetSchema()));

    EXPECT_EQ(GetTotalHistogramCount(), 1u);
    histograms_->ExpectBucketCount("Quota.DatabaseMigrationFromV8ToV9",
                                   /*sample=*/true, /*expected_count=*/1);
  }
}

}  // namespace storage
