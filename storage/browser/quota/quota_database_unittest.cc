// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <iterator>
#include <set>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/cpp/buckets/constants.h"
#include "sql/database.h"
#include "sql/error_metrics.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/test/scoped_error_expecter.h"
#include "sql/test/test_helpers.h"
#include "storage/browser/quota/quota_database.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom-shared.h"
#include "url/gurl.h"

using ::blink::StorageKey;

namespace storage {

namespace {

// Declared to shorten the line lengths.
static const blink::mojom::StorageType kTemp =
    blink::mojom::StorageType::kTemporary;
static const blink::mojom::StorageType kPerm =
    blink::mojom::StorageType::kPersistent;

bool ContainsBucket(const std::set<BucketLocator>& buckets,
                    const BucketInfo& target_bucket) {
  auto it = buckets.find(target_bucket.ToBucketLocator());
  return it != buckets.end();
}

}  // namespace

// Test parameter indicates if the database should be created for incognito
// mode. True will create the database in memory.
class QuotaDatabaseTest : public testing::TestWithParam<bool> {
 protected:
  using QuotaTableEntry = QuotaDatabase::QuotaTableEntry;
  using BucketTableEntry = QuotaDatabase::BucketTableEntry;
  using EnsureOpenedMode = QuotaDatabase::EnsureOpenedMode;

  void SetUp() override { ASSERT_TRUE(temp_directory_.CreateUniqueTempDir()); }

  void TearDown() override { ASSERT_TRUE(temp_directory_.Delete()); }

  bool use_in_memory_db() const { return GetParam(); }

  base::FilePath DbPath() {
    return temp_directory_.GetPath().AppendASCII("quota_manager.db");
  }

  bool EnsureOpened(QuotaDatabase* db, EnsureOpenedMode mode) {
    return db->EnsureOpened(mode) == QuotaError::kNone;
  }

  template <typename EntryType>
  struct EntryVerifier {
    std::set<EntryType> table;

    template <typename Iterator>
    EntryVerifier(Iterator itr, Iterator end)
        : table(itr, end) {}

    bool Run(const EntryType& entry) {
      EXPECT_EQ(1u, table.erase(entry));
      return true;
    }
  };

  QuotaError DumpQuotaTable(QuotaDatabase* quota_database,
                            const QuotaDatabase::QuotaTableCallback& callback) {
    return quota_database->DumpQuotaTable(callback);
  }

  QuotaError DumpBucketTable(
      QuotaDatabase* quota_database,
      const QuotaDatabase::BucketTableCallback& callback) {
    return quota_database->DumpBucketTable(callback);
  }

  template <typename Container>
  void AssignQuotaTable(QuotaDatabase* quota_database, Container&& entries) {
    ASSERT_NE(quota_database->db_.get(), nullptr);
    for (const auto& entry : entries) {
      const char* kSql =
          // clang-format off
          "INSERT INTO quota(host, type, quota) "
            "VALUES (?, ?, ?)";
      // clang-format on
      sql::Statement statement;
      statement.Assign(
          quota_database->db_.get()->GetCachedStatement(SQL_FROM_HERE, kSql));
      ASSERT_TRUE(statement.is_valid());

      statement.BindString(0, entry.host);
      statement.BindInt(1, static_cast<int>(entry.type));
      statement.BindInt64(2, entry.quota);
      EXPECT_TRUE(statement.Run());
    }
    quota_database->Commit();
  }

  template <typename Container>
  void AssignBucketTable(QuotaDatabase* quota_database, Container&& entries) {
    ASSERT_NE(quota_database->db_.get(), (sql::Database*)nullptr);
    for (const auto& entry : entries) {
      const char* kSql =
          // clang-format off
          "INSERT INTO buckets("
              "id,"
              "storage_key,"
              "host,"
              "type,"
              "name,"
              "use_count,"
              "last_accessed,"
              "last_modified,"
              "expiration,"
              "quota) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, 0, 0)";
      // clang-format on
      sql::Statement statement;
      statement.Assign(
          quota_database->db_->GetCachedStatement(SQL_FROM_HERE, kSql));
      ASSERT_TRUE(statement.is_valid());

      statement.BindInt64(0, entry.bucket_id.value());
      statement.BindString(1, entry.storage_key.Serialize());
      statement.BindString(2, entry.storage_key.origin().host());
      statement.BindInt(3, static_cast<int>(entry.type));
      statement.BindString(4, entry.name);
      statement.BindInt(5, entry.use_count);
      statement.BindTime(6, entry.last_accessed);
      statement.BindTime(7, entry.last_modified);
      EXPECT_TRUE(statement.Run());
    }
    quota_database->Commit();
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::ScopedTempDir temp_directory_;
};

TEST_P(QuotaDatabaseTest, EnsureOpened) {
  QuotaDatabase db(use_in_memory_db() ? base::FilePath() : DbPath());
  EXPECT_FALSE(EnsureOpened(&db, EnsureOpenedMode::kFailIfNotFound));
  EXPECT_TRUE(EnsureOpened(&db, EnsureOpenedMode::kCreateIfNotFound));

  if (GetParam()) {
    // Path should not exist for incognito mode.
    ASSERT_FALSE(base::PathExists(DbPath()));
  } else {
    ASSERT_TRUE(base::PathExists(DbPath()));
  }
}

TEST_P(QuotaDatabaseTest, HostQuota) {
  QuotaDatabase db(use_in_memory_db() ? base::FilePath() : DbPath());
  EXPECT_TRUE(EnsureOpened(&db, EnsureOpenedMode::kCreateIfNotFound));

  const char* kHost = "foo.com";
  const int kQuota1 = 13579;
  const int kQuota2 = kQuota1 + 1024;

  QuotaErrorOr<int64_t> result = db.GetHostQuota(kHost, kTemp);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.error(), QuotaError::kNotFound);
  result = db.GetHostQuota(kHost, kPerm);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.error(), QuotaError::kNotFound);

  // Insert quota for temporary.
  EXPECT_EQ(db.SetHostQuota(kHost, kTemp, kQuota1), QuotaError::kNone);
  result = db.GetHostQuota(kHost, kTemp);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(kQuota1, result.value());

  // Update quota for temporary.
  EXPECT_EQ(db.SetHostQuota(kHost, kTemp, kQuota2), QuotaError::kNone);
  result = db.GetHostQuota(kHost, kTemp);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(kQuota2, result.value());

  // Quota for persistent must not be updated.
  result = db.GetHostQuota(kHost, kPerm);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.error(), QuotaError::kNotFound);

  // Delete temporary storage quota.
  EXPECT_EQ(db.DeleteHostQuota(kHost, kTemp), QuotaError::kNone);
  result = db.GetHostQuota(kHost, kTemp);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.error(), QuotaError::kNotFound);

  // Delete persistent quota by setting it to zero.
  EXPECT_EQ(db.SetHostQuota(kHost, kPerm, 0), QuotaError::kNone);
  result = db.GetHostQuota(kHost, kPerm);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.error(), QuotaError::kNotFound);
}

TEST_P(QuotaDatabaseTest, GetOrCreateBucket) {
  QuotaDatabase db(use_in_memory_db() ? base::FilePath() : DbPath());
  EXPECT_TRUE(EnsureOpened(&db, EnsureOpenedMode::kCreateIfNotFound));
  StorageKey storage_key =
      StorageKey::CreateFromStringForTesting("http://google/");
  std::string bucket_name = "google_bucket";

  QuotaErrorOr<BucketInfo> result =
      db.GetOrCreateBucket(storage_key, bucket_name);
  ASSERT_TRUE(result.ok());

  BucketInfo created_bucket = result.value();
  ASSERT_GT(created_bucket.id.value(), 0);
  ASSERT_EQ(created_bucket.name, bucket_name);
  ASSERT_EQ(created_bucket.storage_key, storage_key);
  ASSERT_EQ(created_bucket.type, kTemp);

  // Should return the same bucket when querying again.
  result = db.GetOrCreateBucket(storage_key, bucket_name);
  ASSERT_TRUE(result.ok());

  BucketInfo retrieved_bucket = result.value();
  ASSERT_EQ(retrieved_bucket.id, created_bucket.id);
  ASSERT_EQ(retrieved_bucket.name, created_bucket.name);
  ASSERT_EQ(retrieved_bucket.storage_key, created_bucket.storage_key);
  ASSERT_EQ(retrieved_bucket.type, created_bucket.type);
}

TEST_P(QuotaDatabaseTest, GetOrCreateBucketDeprecated) {
  QuotaDatabase db(use_in_memory_db() ? base::FilePath() : DbPath());
  EXPECT_TRUE(EnsureOpened(&db, EnsureOpenedMode::kCreateIfNotFound));
  StorageKey storage_key =
      StorageKey::CreateFromStringForTesting("http://google/");
  std::string bucket_name = "google_bucket";

  QuotaErrorOr<BucketInfo> result =
      db.GetOrCreateBucketDeprecated(storage_key, bucket_name, kPerm);
  ASSERT_TRUE(result.ok());

  BucketInfo created_bucket = result.value();
  ASSERT_GT(created_bucket.id.value(), 0);
  ASSERT_EQ(created_bucket.name, bucket_name);
  ASSERT_EQ(created_bucket.storage_key, storage_key);
  ASSERT_EQ(created_bucket.type, kPerm);

  // Should return the same bucket when querying again.
  result = db.GetOrCreateBucketDeprecated(storage_key, bucket_name, kPerm);
  ASSERT_TRUE(result.ok());

  BucketInfo retrieved_bucket = result.value();
  ASSERT_EQ(retrieved_bucket.id, created_bucket.id);
  ASSERT_EQ(retrieved_bucket.name, created_bucket.name);
  ASSERT_EQ(retrieved_bucket.storage_key, created_bucket.storage_key);
  ASSERT_EQ(retrieved_bucket.type, created_bucket.type);
}

TEST_P(QuotaDatabaseTest, GetBucket) {
  QuotaDatabase db(use_in_memory_db() ? base::FilePath() : DbPath());
  EXPECT_TRUE(EnsureOpened(&db, EnsureOpenedMode::kCreateIfNotFound));

  // Add a bucket entry into the bucket table.
  StorageKey storage_key =
      StorageKey::CreateFromStringForTesting("http://google/");
  std::string bucket_name = "google_bucket";
  QuotaErrorOr<BucketInfo> result =
      db.CreateBucketForTesting(storage_key, bucket_name, kPerm);
  ASSERT_TRUE(result.ok());

  BucketInfo created_bucket = result.value();
  ASSERT_GT(created_bucket.id.value(), 0);
  ASSERT_EQ(created_bucket.name, bucket_name);
  ASSERT_EQ(created_bucket.storage_key, storage_key);
  ASSERT_EQ(created_bucket.type, kPerm);

  result = db.GetBucket(storage_key, bucket_name, kPerm);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.value().id, created_bucket.id);
  EXPECT_EQ(result.value().name, created_bucket.name);
  EXPECT_EQ(result.value().storage_key, created_bucket.storage_key);
  ASSERT_EQ(result.value().type, created_bucket.type);

  // Can't retrieve buckets with name mismatch.
  result = db.GetBucket(storage_key, "does_not_exist", kPerm);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.error(), QuotaError::kNotFound);

  // Can't retrieve buckets with StorageKey mismatch.
  result =
      db.GetBucket(StorageKey::CreateFromStringForTesting("http://example/"),
                   bucket_name, kPerm);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.error(), QuotaError::kNotFound);
}

TEST_P(QuotaDatabaseTest, GetBucketById) {
  QuotaDatabase db(use_in_memory_db() ? base::FilePath() : DbPath());
  EXPECT_TRUE(EnsureOpened(&db, EnsureOpenedMode::kCreateIfNotFound));

  // Add a bucket entry into the bucket table.
  StorageKey storage_key =
      StorageKey::CreateFromStringForTesting("http://google/");
  std::string bucket_name = "google_bucket";
  QuotaErrorOr<BucketInfo> result =
      db.CreateBucketForTesting(storage_key, bucket_name, kPerm);
  ASSERT_TRUE(result.ok());

  BucketInfo created_bucket = result.value();
  ASSERT_GT(created_bucket.id.value(), 0);
  ASSERT_EQ(created_bucket.name, bucket_name);
  ASSERT_EQ(created_bucket.storage_key, storage_key);
  ASSERT_EQ(created_bucket.type, kPerm);

  result = db.GetBucketById(created_bucket.id);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.value().name, created_bucket.name);
  EXPECT_EQ(result.value().storage_key, created_bucket.storage_key);
  ASSERT_EQ(result.value().type, created_bucket.type);

  constexpr BucketId kNonExistentBucketId(7777);
  result = db.GetBucketById(BucketId(kNonExistentBucketId));
  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.error(), QuotaError::kNotFound);
}

TEST_P(QuotaDatabaseTest, GetBucketsForType) {
  QuotaDatabase db(use_in_memory_db() ? base::FilePath() : DbPath());
  EXPECT_TRUE(EnsureOpened(&db, EnsureOpenedMode::kCreateIfNotFound));

  const StorageKey storage_key1 =
      StorageKey::CreateFromStringForTesting("http://example-a/");
  const StorageKey storage_key2 =
      StorageKey::CreateFromStringForTesting("http://example-b/");
  const StorageKey storage_key3 =
      StorageKey::CreateFromStringForTesting("http://example-c/");

  QuotaErrorOr<BucketInfo> bucket_result =
      db.CreateBucketForTesting(storage_key1, "temp_bucket", kTemp);
  ASSERT_TRUE(bucket_result.ok());
  BucketInfo temp_bucket1 = bucket_result.value();

  bucket_result = db.CreateBucketForTesting(storage_key2, "temp_bucket", kTemp);
  ASSERT_TRUE(bucket_result.ok());
  BucketInfo temp_bucket2 = bucket_result.value();

  bucket_result = db.CreateBucketForTesting(storage_key1, "perm_bucket", kPerm);
  ASSERT_TRUE(bucket_result.ok());
  BucketInfo perm_bucket1 = bucket_result.value();

  bucket_result = db.CreateBucketForTesting(storage_key3, "perm_bucket", kPerm);
  ASSERT_TRUE(bucket_result.ok());
  BucketInfo perm_bucket2 = bucket_result.value();

  QuotaErrorOr<std::set<BucketLocator>> result = db.GetBucketsForType(kTemp);
  ASSERT_TRUE(result.ok());
  std::set<BucketLocator> buckets = result.value();
  ASSERT_EQ(2U, buckets.size());
  EXPECT_TRUE(ContainsBucket(buckets, temp_bucket1));
  EXPECT_TRUE(ContainsBucket(buckets, temp_bucket2));

  result = db.GetBucketsForType(kPerm);
  ASSERT_TRUE(result.ok());
  buckets = result.value();
  ASSERT_EQ(2U, buckets.size());
  EXPECT_TRUE(ContainsBucket(buckets, perm_bucket1));
  EXPECT_TRUE(ContainsBucket(buckets, perm_bucket2));
}

TEST_P(QuotaDatabaseTest, GetBucketsForHost) {
  QuotaDatabase db(use_in_memory_db() ? base::FilePath() : DbPath());
  EXPECT_TRUE(EnsureOpened(&db, EnsureOpenedMode::kCreateIfNotFound));

  QuotaErrorOr<BucketInfo> temp_example_bucket1 = db.CreateBucketForTesting(
      StorageKey::CreateFromStringForTesting("https://example.com/"), "default",
      kTemp);
  QuotaErrorOr<BucketInfo> temp_example_bucket2 = db.CreateBucketForTesting(
      StorageKey::CreateFromStringForTesting("http://example.com:123/"),
      "default", kTemp);
  QuotaErrorOr<BucketInfo> perm_google_bucket1 = db.CreateBucketForTesting(
      StorageKey::CreateFromStringForTesting("http://google.com/"), "default",
      kPerm);
  QuotaErrorOr<BucketInfo> temp_google_bucket2 = db.CreateBucketForTesting(
      StorageKey::CreateFromStringForTesting("http://google.com:123/"),
      "default", kTemp);

  QuotaErrorOr<std::set<BucketLocator>> result =
      db.GetBucketsForHost("example.com", kTemp);
  ASSERT_TRUE(result.ok());
  ASSERT_EQ(result->size(), 2U);
  EXPECT_TRUE(ContainsBucket(result.value(), temp_example_bucket1.value()));
  EXPECT_TRUE(ContainsBucket(result.value(), temp_example_bucket2.value()));

  result = db.GetBucketsForHost("example.com", kPerm);
  ASSERT_TRUE(result.ok());
  ASSERT_EQ(result->size(), 0U);

  result = db.GetBucketsForHost("google.com", kPerm);
  ASSERT_TRUE(result.ok());
  ASSERT_EQ(result->size(), 1U);
  EXPECT_TRUE(ContainsBucket(result.value(), perm_google_bucket1.value()));

  result = db.GetBucketsForHost("google.com", kTemp);
  ASSERT_TRUE(result.ok());
  ASSERT_EQ(result->size(), 1U);
  EXPECT_TRUE(ContainsBucket(result.value(), temp_google_bucket2.value()));
}

TEST_P(QuotaDatabaseTest, GetBucketsForStorageKey) {
  QuotaDatabase db(use_in_memory_db() ? base::FilePath() : DbPath());
  EXPECT_TRUE(EnsureOpened(&db, EnsureOpenedMode::kCreateIfNotFound));

  const StorageKey storage_key1 =
      StorageKey::CreateFromStringForTesting("http://example-a/");
  const StorageKey storage_key2 =
      StorageKey::CreateFromStringForTesting("http://example-b/");

  QuotaErrorOr<BucketInfo> bucket_result =
      db.CreateBucketForTesting(storage_key1, "temp_test1", kTemp);
  ASSERT_TRUE(bucket_result.ok());
  BucketInfo temp_bucket1 = bucket_result.value();

  bucket_result = db.CreateBucketForTesting(storage_key1, "temp_test2", kTemp);
  ASSERT_TRUE(bucket_result.ok());
  BucketInfo temp_bucket2 = bucket_result.value();

  bucket_result = db.CreateBucketForTesting(storage_key1, "perm_test", kPerm);
  ASSERT_TRUE(bucket_result.ok());
  BucketInfo perm_bucket1 = bucket_result.value();

  bucket_result = db.CreateBucketForTesting(storage_key2, "perm_test", kPerm);
  ASSERT_TRUE(bucket_result.ok());
  BucketInfo perm_bucket2 = bucket_result.value();

  QuotaErrorOr<std::set<BucketLocator>> result =
      db.GetBucketsForStorageKey(storage_key1, kTemp);
  ASSERT_TRUE(result.ok());
  std::set<BucketLocator> buckets = result.value();
  ASSERT_EQ(2U, buckets.size());
  EXPECT_TRUE(ContainsBucket(buckets, temp_bucket1));
  EXPECT_TRUE(ContainsBucket(buckets, temp_bucket2));

  result = db.GetBucketsForStorageKey(storage_key2, kPerm);
  ASSERT_TRUE(result.ok());
  buckets = result.value();
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(ContainsBucket(buckets, perm_bucket2));
}

TEST_P(QuotaDatabaseTest, GetBucketWithNoDb) {
  QuotaDatabase db(use_in_memory_db() ? base::FilePath() : DbPath());
  EXPECT_FALSE(EnsureOpened(&db, EnsureOpenedMode::kFailIfNotFound));

  StorageKey storage_key =
      StorageKey::CreateFromStringForTesting("http://google/");
  std::string bucket_name = "google_bucket";
  QuotaErrorOr<BucketInfo> result =
      db.GetBucket(storage_key, bucket_name, kTemp);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.error(), QuotaError::kNotFound);
}

// TODO(crbug.com/1216094): Update test to have its behavior on Fuchsia/Win
// match with other platforms, and enable test on all platforms.
#if BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_WIN)
#define MAYBE_GetBucketWithOpenDatabaseError \
  DISABLED_GetBucketWithOpenDatabaseError
#else
#define MAYBE_GetBucketWithOpenDatabaseError GetBucketWithOpenDatabaseError
#endif
TEST_F(QuotaDatabaseTest, MAYBE_GetBucketWithOpenDatabaseError) {
  base::HistogramTester histograms;
  sql::test::ScopedErrorExpecter expecter;
  expecter.ExpectError(SQLITE_CANTOPEN);

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  QuotaDatabase db(temp_dir.GetPath());

  StorageKey storage_key =
      StorageKey::CreateFromStringForTesting("http://google/");
  std::string bucket_name = "google_bucket";
  QuotaErrorOr<BucketInfo> result =
      db.GetBucket(storage_key, bucket_name, kTemp);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.error(), QuotaError::kDatabaseError);

  EXPECT_TRUE(expecter.SawExpectedErrors());
  histograms.ExpectTotalCount("Quota.QuotaDatabaseReset", 1);
  histograms.ExpectBucketCount("Quota.QuotaDatabaseReset",
                               DatabaseResetReason::kOpenDatabase, 1);
}

TEST_P(QuotaDatabaseTest, BucketLastAccessTimeLRU) {
  QuotaDatabase db(use_in_memory_db() ? base::FilePath() : DbPath());
  EXPECT_TRUE(EnsureOpened(&db, EnsureOpenedMode::kCreateIfNotFound));

  std::set<BucketId> bucket_exceptions;
  QuotaErrorOr<BucketLocator> result =
      db.GetLRUBucket(kTemp, bucket_exceptions, nullptr);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.error(), QuotaError::kNotFound);

  // Insert bucket entries into BucketTable.
  base::Time now = base::Time::Now();
  using Entry = QuotaDatabase::BucketTableEntry;
  Entry bucket1 = Entry(
      BucketId(1), StorageKey::CreateFromStringForTesting("http://example-a/"),
      kTemp, kDefaultBucketName, 99, now, now);
  Entry bucket2 = Entry(
      BucketId(2), StorageKey::CreateFromStringForTesting("http://example-b/"),
      kTemp, kDefaultBucketName, 0, now, now);
  Entry bucket3 = Entry(
      BucketId(3), StorageKey::CreateFromStringForTesting("http://example-c/"),
      kTemp, "bucket_c", 1, now, now);
  Entry bucket4 = Entry(
      BucketId(4), StorageKey::CreateFromStringForTesting("http://example-d/"),
      kPerm, "bucket_d", 5, now, now);
  Entry kTableEntries[] = {bucket1, bucket2, bucket3, bucket4};
  AssignBucketTable(&db, kTableEntries);

  // Update access time for three temporary storages, and
  EXPECT_EQ(db.SetBucketLastAccessTime(bucket1.bucket_id,
                                       base::Time::FromJavaTime(10)),
            QuotaError::kNone);
  EXPECT_EQ(db.SetBucketLastAccessTime(bucket2.bucket_id,
                                       base::Time::FromJavaTime(20)),
            QuotaError::kNone);
  EXPECT_EQ(db.SetBucketLastAccessTime(bucket3.bucket_id,
                                       base::Time::FromJavaTime(30)),
            QuotaError::kNone);

  // one persistent.
  EXPECT_EQ(db.SetBucketLastAccessTime(bucket4.bucket_id,
                                       base::Time::FromJavaTime(40)),
            QuotaError::kNone);

  result = db.GetLRUBucket(kTemp, bucket_exceptions, nullptr);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(bucket1.bucket_id, result.value().id);

  // Test that unlimited origins are excluded from eviction, but
  // protected origins are not excluded.
  auto policy = base::MakeRefCounted<MockSpecialStoragePolicy>();
  policy->AddUnlimited(bucket1.storage_key.origin().GetURL());
  policy->AddProtected(bucket2.storage_key.origin().GetURL());
  result = db.GetLRUBucket(kTemp, bucket_exceptions, policy.get());
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(bucket2.bucket_id, result.value().id);

  // Test that durable origins are excluded from eviction.
  policy->AddDurable(bucket2.storage_key.origin().GetURL());
  result = db.GetLRUBucket(kTemp, bucket_exceptions, policy.get());
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(bucket3.bucket_id, result.value().id);

  // Bucket exceptions exclude specified buckets.
  bucket_exceptions.insert(bucket1.bucket_id);
  result = db.GetLRUBucket(kTemp, bucket_exceptions, nullptr);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(bucket2.bucket_id, result.value().id);

  bucket_exceptions.insert(bucket2.bucket_id);
  result = db.GetLRUBucket(kTemp, bucket_exceptions, nullptr);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(bucket3.bucket_id, result.value().id);

  bucket_exceptions.insert(bucket3.bucket_id);
  result = db.GetLRUBucket(kTemp, bucket_exceptions, nullptr);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.error(), QuotaError::kNotFound);

  EXPECT_EQ(db.SetBucketLastAccessTime(bucket1.bucket_id, base::Time::Now()),
            QuotaError::kNone);

  // Delete storage_key/type last access time information.
  EXPECT_EQ(db.DeleteBucketInfo(bucket3.bucket_id), QuotaError::kNone);

  // Querying again to see if the deletion has worked.
  bucket_exceptions.clear();
  result = db.GetLRUBucket(kTemp, bucket_exceptions, nullptr);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(bucket2.bucket_id, result.value().id);

  bucket_exceptions.insert(bucket1.bucket_id);
  bucket_exceptions.insert(bucket2.bucket_id);
  result = db.GetLRUBucket(kTemp, bucket_exceptions, nullptr);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.error(), QuotaError::kNotFound);
}

TEST_P(QuotaDatabaseTest, SetStorageKeyLastAccessTime) {
  QuotaDatabase db(use_in_memory_db() ? base::FilePath() : DbPath());
  EXPECT_TRUE(EnsureOpened(&db, EnsureOpenedMode::kCreateIfNotFound));

  const StorageKey storage_key =
      StorageKey::CreateFromStringForTesting("http://example/");
  base::Time now = base::Time::Now();

  // Should error if bucket doesn't exist.
  EXPECT_EQ(db.SetStorageKeyLastAccessTime(storage_key, kTemp, now),
            QuotaError::kNotFound);

  QuotaErrorOr<BucketInfo> bucket =
      db.CreateBucketForTesting(storage_key, kDefaultBucketName, kTemp);

  EXPECT_EQ(db.SetStorageKeyLastAccessTime(storage_key, kTemp, now),
            QuotaError::kNone);

  QuotaErrorOr<QuotaDatabase::BucketTableEntry> info =
      db.GetBucketInfo(bucket->id);
  EXPECT_TRUE(info.ok());
  EXPECT_EQ(now, info->last_accessed);
  EXPECT_EQ(1, info->use_count);
}

TEST_P(QuotaDatabaseTest, GetStorageKeysForType) {
  QuotaDatabase db(use_in_memory_db() ? base::FilePath() : DbPath());
  EXPECT_TRUE(EnsureOpened(&db, EnsureOpenedMode::kCreateIfNotFound));

  const StorageKey storage_key1 =
      StorageKey::CreateFromStringForTesting("http://example-a/");
  const StorageKey storage_key2 =
      StorageKey::CreateFromStringForTesting("http://example-b/");
  const StorageKey storage_key3 =
      StorageKey::CreateFromStringForTesting("http://example-c/");

  db.CreateBucketForTesting(storage_key1, "bucket_a", kTemp);
  db.CreateBucketForTesting(storage_key2, "bucket_b", kTemp);
  db.CreateBucketForTesting(storage_key2, "bucket_b", kPerm);
  db.CreateBucketForTesting(storage_key3, "bucket_c", kPerm);

  QuotaErrorOr<std::set<StorageKey>> result = db.GetStorageKeysForType(kTemp);
  ASSERT_TRUE(result.ok());
  ASSERT_TRUE(base::Contains(result.value(), storage_key1));
  ASSERT_TRUE(base::Contains(result.value(), storage_key2));
  ASSERT_FALSE(base::Contains(result.value(), storage_key3));

  result = db.GetStorageKeysForType(kPerm);
  ASSERT_TRUE(result.ok());
  ASSERT_FALSE(base::Contains(result.value(), storage_key1));
  ASSERT_TRUE(base::Contains(result.value(), storage_key2));
  ASSERT_TRUE(base::Contains(result.value(), storage_key3));
}

TEST_P(QuotaDatabaseTest, BucketLastModifiedBetween) {
  QuotaDatabase db(use_in_memory_db() ? base::FilePath() : DbPath());
  EXPECT_TRUE(EnsureOpened(&db, EnsureOpenedMode::kCreateIfNotFound));

  QuotaErrorOr<std::set<BucketLocator>> result =
      db.GetBucketsModifiedBetween(kTemp, base::Time(), base::Time::Max());
  EXPECT_TRUE(result.ok());
  std::set<BucketLocator> buckets = result.value();
  EXPECT_TRUE(buckets.empty());

  QuotaErrorOr<BucketInfo> result1 = db.CreateBucketForTesting(
      StorageKey::CreateFromStringForTesting("http://example-a/"), "bucket_a",
      kTemp);
  EXPECT_TRUE(result1.ok());
  BucketInfo bucket1 = result1.value();
  QuotaErrorOr<BucketInfo> result2 = db.CreateBucketForTesting(
      StorageKey::CreateFromStringForTesting("http://example-b/"), "bucket_b",
      kTemp);
  EXPECT_TRUE(result2.ok());
  BucketInfo bucket2 = result2.value();
  QuotaErrorOr<BucketInfo> result3 = db.CreateBucketForTesting(
      StorageKey::CreateFromStringForTesting("http://example-c/"), "bucket_c",
      kTemp);
  EXPECT_TRUE(result3.ok());
  BucketInfo bucket3 = result3.value();
  QuotaErrorOr<BucketInfo> result4 = db.CreateBucketForTesting(
      StorageKey::CreateFromStringForTesting("http://example-d/"), "bucket_d",
      kPerm);
  EXPECT_TRUE(result4.ok());
  BucketInfo bucket4 = result4.value();

  // Report last modified time for the buckets.
  EXPECT_EQ(
      db.SetBucketLastModifiedTime(bucket1.id, base::Time::FromJavaTime(0)),
      QuotaError::kNone);
  EXPECT_EQ(
      db.SetBucketLastModifiedTime(bucket2.id, base::Time::FromJavaTime(10)),
      QuotaError::kNone);
  EXPECT_EQ(
      db.SetBucketLastModifiedTime(bucket3.id, base::Time::FromJavaTime(20)),
      QuotaError::kNone);
  EXPECT_EQ(
      db.SetBucketLastModifiedTime(bucket4.id, base::Time::FromJavaTime(30)),
      QuotaError::kNone);

  result = db.GetBucketsModifiedBetween(kTemp, base::Time(), base::Time::Max());
  EXPECT_TRUE(result.ok());
  buckets = result.value();
  EXPECT_EQ(3U, buckets.size());
  EXPECT_TRUE(ContainsBucket(buckets, bucket1));
  EXPECT_TRUE(ContainsBucket(buckets, bucket2));
  EXPECT_TRUE(ContainsBucket(buckets, bucket3));
  EXPECT_FALSE(ContainsBucket(buckets, bucket4));

  result = db.GetBucketsModifiedBetween(kTemp, base::Time::FromJavaTime(5),
                                        base::Time::Max());
  EXPECT_TRUE(result.ok());
  buckets = result.value();
  EXPECT_EQ(2U, buckets.size());
  EXPECT_FALSE(ContainsBucket(buckets, bucket1));
  EXPECT_TRUE(ContainsBucket(buckets, bucket2));
  EXPECT_TRUE(ContainsBucket(buckets, bucket3));
  EXPECT_FALSE(ContainsBucket(buckets, bucket4));

  result = db.GetBucketsModifiedBetween(kTemp, base::Time::FromJavaTime(15),
                                        base::Time::Max());
  EXPECT_TRUE(result.ok());
  buckets = result.value();
  EXPECT_EQ(1U, buckets.size());
  EXPECT_FALSE(ContainsBucket(buckets, bucket1));
  EXPECT_FALSE(ContainsBucket(buckets, bucket2));
  EXPECT_TRUE(ContainsBucket(buckets, bucket3));
  EXPECT_FALSE(ContainsBucket(buckets, bucket4));

  result = db.GetBucketsModifiedBetween(kTemp, base::Time::FromJavaTime(25),
                                        base::Time::Max());
  EXPECT_TRUE(result.ok());
  buckets = result.value();
  EXPECT_TRUE(buckets.empty());

  result = db.GetBucketsModifiedBetween(kTemp, base::Time::FromJavaTime(5),
                                        base::Time::FromJavaTime(15));
  EXPECT_TRUE(result.ok());
  buckets = result.value();
  EXPECT_EQ(1U, buckets.size());
  EXPECT_FALSE(ContainsBucket(buckets, bucket1));
  EXPECT_TRUE(ContainsBucket(buckets, bucket2));
  EXPECT_FALSE(ContainsBucket(buckets, bucket3));
  EXPECT_FALSE(ContainsBucket(buckets, bucket4));

  result = db.GetBucketsModifiedBetween(kTemp, base::Time::FromJavaTime(0),
                                        base::Time::FromJavaTime(20));
  EXPECT_TRUE(result.ok());
  buckets = result.value();
  EXPECT_EQ(2U, buckets.size());
  EXPECT_TRUE(ContainsBucket(buckets, bucket1));
  EXPECT_TRUE(ContainsBucket(buckets, bucket2));
  EXPECT_FALSE(ContainsBucket(buckets, bucket3));
  EXPECT_FALSE(ContainsBucket(buckets, bucket4));

  result = db.GetBucketsModifiedBetween(kPerm, base::Time::FromJavaTime(0),
                                        base::Time::FromJavaTime(35));
  EXPECT_TRUE(result.ok());
  buckets = result.value();
  EXPECT_EQ(1U, buckets.size());
  EXPECT_FALSE(ContainsBucket(buckets, bucket1));
  EXPECT_FALSE(ContainsBucket(buckets, bucket2));
  EXPECT_FALSE(ContainsBucket(buckets, bucket3));
  EXPECT_TRUE(ContainsBucket(buckets, bucket4));
}

TEST_P(QuotaDatabaseTest, RegisterInitialStorageKeyInfo) {
  QuotaDatabase db(use_in_memory_db() ? base::FilePath() : DbPath());

  base::flat_map<blink::mojom::StorageType, std::set<StorageKey>>
      storage_keys_by_type;
  const StorageKey kStorageKeys[] = {
      StorageKey::CreateFromStringForTesting("http://a/"),
      StorageKey::CreateFromStringForTesting("http://b/"),
      StorageKey::CreateFromStringForTesting("http://c/")};
  storage_keys_by_type.emplace(
      kTemp, std::set<StorageKey>(kStorageKeys, std::end(kStorageKeys)));
  storage_keys_by_type.emplace(
      kPerm, std::set<StorageKey>(kStorageKeys, std::end(kStorageKeys)));

  EXPECT_EQ(db.RegisterInitialStorageKeyInfo(storage_keys_by_type),
            QuotaError::kNone);

  QuotaErrorOr<BucketInfo> bucket_result =
      db.GetBucket(StorageKey::CreateFromStringForTesting("http://a/"),
                   kDefaultBucketName, kTemp);
  ASSERT_TRUE(bucket_result.ok());

  QuotaErrorOr<QuotaDatabase::BucketTableEntry> info =
      db.GetBucketInfo(bucket_result->id);
  EXPECT_TRUE(info.ok());
  EXPECT_EQ(0, info->use_count);

  EXPECT_EQ(db.SetStorageKeyLastAccessTime(
                StorageKey::CreateFromStringForTesting("http://a/"), kTemp,
                base::Time::FromDoubleT(1.0)),
            QuotaError::kNone);
  info = db.GetBucketInfo(bucket_result->id);
  EXPECT_TRUE(info.ok());
  EXPECT_EQ(1, info->use_count);

  EXPECT_EQ(db.RegisterInitialStorageKeyInfo(storage_keys_by_type),
            QuotaError::kNone);

  info = db.GetBucketInfo(bucket_result->id);
  EXPECT_TRUE(info.ok());
  EXPECT_EQ(1, info->use_count);
}

TEST_P(QuotaDatabaseTest, DumpQuotaTable) {
  QuotaTableEntry kTableEntries[] = {
      {.host = "http://go/", .type = kTemp, .quota = 1},
      {.host = "http://oo/", .type = kTemp, .quota = 2},
      {.host = "http://gle/", .type = kPerm, .quota = 3}};

  QuotaDatabase db(use_in_memory_db() ? base::FilePath() : DbPath());
  EXPECT_TRUE(EnsureOpened(&db, EnsureOpenedMode::kCreateIfNotFound));
  AssignQuotaTable(&db, kTableEntries);

  using Verifier = EntryVerifier<QuotaTableEntry>;
  Verifier verifier(kTableEntries, std::end(kTableEntries));
  EXPECT_EQ(
      DumpQuotaTable(&db, base::BindRepeating(&Verifier::Run,
                                              base::Unretained(&verifier))),
      QuotaError::kNone);
  EXPECT_TRUE(verifier.table.empty());
}

TEST_P(QuotaDatabaseTest, DumpBucketTable) {
  base::Time now = base::Time::Now();
  using Entry = QuotaDatabase::BucketTableEntry;
  Entry kTableEntries[] = {
      Entry(BucketId(1), StorageKey::CreateFromStringForTesting("http://go/"),
            kTemp, kDefaultBucketName, 2147483647, now, now),
      Entry(BucketId(2), StorageKey::CreateFromStringForTesting("http://oo/"),
            kTemp, kDefaultBucketName, 0, now, now),
      Entry(BucketId(3), StorageKey::CreateFromStringForTesting("http://gle/"),
            kTemp, kDefaultBucketName, 1, now, now),
  };

  QuotaDatabase db(use_in_memory_db() ? base::FilePath() : DbPath());
  EXPECT_TRUE(EnsureOpened(&db, EnsureOpenedMode::kCreateIfNotFound));
  AssignBucketTable(&db, kTableEntries);

  using Verifier = EntryVerifier<Entry>;
  Verifier verifier(kTableEntries, std::end(kTableEntries));
  EXPECT_EQ(
      DumpBucketTable(&db, base::BindRepeating(&Verifier::Run,
                                               base::Unretained(&verifier))),
      QuotaError::kNone);
  EXPECT_TRUE(verifier.table.empty());
}

TEST_P(QuotaDatabaseTest, GetBucketInfo) {
  using Entry = QuotaDatabase::BucketTableEntry;
  Entry kTableEntries[] = {
      Entry(BucketId(123), StorageKey::CreateFromStringForTesting("http://go/"),
            kTemp, "test_bucket", 100, base::Time(), base::Time())};

  QuotaDatabase db(use_in_memory_db() ? base::FilePath() : DbPath());
  EXPECT_TRUE(EnsureOpened(&db, EnsureOpenedMode::kCreateIfNotFound));
  AssignBucketTable(&db, kTableEntries);

  {
    QuotaErrorOr<QuotaDatabase::BucketTableEntry> entry =
        db.GetBucketInfo(kTableEntries[0].bucket_id);
    EXPECT_TRUE(entry.ok());
    EXPECT_EQ(kTableEntries[0].bucket_id, entry->bucket_id);
    EXPECT_EQ(kTableEntries[0].type, entry->type);
    EXPECT_EQ(kTableEntries[0].storage_key, entry->storage_key);
    EXPECT_EQ(kTableEntries[0].name, entry->name);
    EXPECT_EQ(kTableEntries[0].use_count, entry->use_count);
    EXPECT_EQ(kTableEntries[0].last_accessed, entry->last_accessed);
    EXPECT_EQ(kTableEntries[0].last_modified, entry->last_modified);
  }

  {
    // BucketId 456 is not in the database.
    QuotaErrorOr<QuotaDatabase::BucketTableEntry> entry =
        db.GetBucketInfo(BucketId(456));
    EXPECT_FALSE(entry.ok());
  }
}

// Non-parameterized tests.
TEST_F(QuotaDatabaseTest, BootstrapFlag) {
  QuotaDatabase db(DbPath());

  EXPECT_FALSE(db.IsBootstrapped());
  EXPECT_EQ(db.SetIsBootstrapped(true), QuotaError::kNone);
  EXPECT_TRUE(db.IsBootstrapped());
  EXPECT_EQ(db.SetIsBootstrapped(false), QuotaError::kNone);
  EXPECT_FALSE(db.IsBootstrapped());
}

TEST_F(QuotaDatabaseTest, OpenCorruptedDatabase) {
  base::HistogramTester histograms;
  // Create database, force corruption and close db by leaving scope.
  {
    QuotaDatabase db(DbPath());
    ASSERT_TRUE(EnsureOpened(&db, EnsureOpenedMode::kCreateIfNotFound));
    ASSERT_TRUE(sql::test::CorruptSizeInHeader(DbPath()));
  }
  // Reopen database and verify schema reset on reopen.
  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(SQLITE_CORRUPT);
    QuotaDatabase db(DbPath());
    ASSERT_TRUE(EnsureOpened(&db, EnsureOpenedMode::kFailIfNotFound));
    EXPECT_TRUE(expecter.SawExpectedErrors());
  }

  histograms.ExpectTotalCount("Quota.QuotaDatabaseReset", 1);
  histograms.ExpectBucketCount("Quota.QuotaDatabaseReset",
                               DatabaseResetReason::kCreateSchema, 1);

  EXPECT_GE(histograms.GetTotalSum("Quota.QuotaDatabaseError"), 1);
  EXPECT_GE(histograms.GetBucketCount("Quota.QuotaDatabaseError",
                                      sql::SqliteLoggedResultCode::kCorrupt),
            1);
}

TEST_F(QuotaDatabaseTest, GetOrCreateBucket_CorruptedDatabase) {
  QuotaDatabase db(DbPath());
  StorageKey storage_key =
      StorageKey::CreateFromStringForTesting("http://google/");
  std::string bucket_name = "google_bucket";

  {
    QuotaErrorOr<BucketInfo> result =
        db.GetOrCreateBucket(storage_key, bucket_name);
    ASSERT_TRUE(result.ok()) << "Failed to create bucket to be used in test";
  }

  // Bucket lookup uses the `buckets_by_storage_key` index.
  QuotaError corruption_error =
      db.CorruptForTesting(base::BindOnce([](const base::FilePath& db_path) {
        ASSERT_TRUE(
            sql::test::CorruptIndexRootPage(db_path, "buckets_by_storage_key"));
      }));
  ASSERT_EQ(QuotaError::kNone, corruption_error)
      << "Failed to corrupt the database";

  {
    base::HistogramTester histograms;

    QuotaErrorOr<BucketInfo> result =
        db.GetOrCreateBucket(storage_key, bucket_name);
    EXPECT_FALSE(result.ok());

    histograms.ExpectTotalCount("Quota.QuotaDatabaseError", 1);
    histograms.ExpectBucketCount("Quota.QuotaDatabaseError",
                                 sql::SqliteLoggedResultCode::kCorrupt, 1);
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         QuotaDatabaseTest,
                         /* is_incognito */ testing::Bool());

}  // namespace storage
