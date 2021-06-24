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
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "sql/database.h"
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

const char kDefaultBucketName[] = "default";

}  // namespace

// Test parameter indicates if the database should be created for incognito
// mode. True will create the database in memory.
class QuotaDatabaseTest : public testing::TestWithParam<bool> {
 protected:
  using QuotaTableEntry = QuotaDatabase::QuotaTableEntry;
  using BucketTableEntry = QuotaDatabase::BucketTableEntry;
  using LazyOpenMode = QuotaDatabase::LazyOpenMode;

  void SetUp() override { ASSERT_TRUE(temp_directory_.CreateUniqueTempDir()); }

  void TearDown() override { ASSERT_TRUE(temp_directory_.Delete()); }

  bool use_in_memory_db() const { return GetParam(); }

  base::FilePath DbPath() {
    return temp_directory_.GetPath().AppendASCII("quota_manager.db");
  }

  bool LazyOpen(QuotaDatabase* db, LazyOpenMode mode) {
    return db->LazyOpen(mode) == QuotaError::kNone;
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

  bool DumpQuotaTable(QuotaDatabase* quota_database,
                      const QuotaDatabase::QuotaTableCallback& callback) {
    return quota_database->DumpQuotaTable(callback);
  }

  bool DumpBucketTable(QuotaDatabase* quota_database,
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
              "origin,"
              "type,"
              "name,"
              "use_count,"
              "last_accessed,"
              "last_modified,"
              "expiration,"
              "quota) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, 0, 0)";
      // clang-format on
      sql::Statement statement;
      statement.Assign(
          quota_database->db_->GetCachedStatement(SQL_FROM_HERE, kSql));
      ASSERT_TRUE(statement.is_valid());

      statement.BindInt64(0, entry.bucket_id.value());
      statement.BindString(1, entry.storage_key.Serialize());
      statement.BindInt(2, static_cast<int>(entry.type));
      statement.BindString(3, entry.name);
      statement.BindInt(4, entry.use_count);
      statement.BindTime(5, entry.last_accessed);
      statement.BindTime(6, entry.last_modified);
      EXPECT_TRUE(statement.Run());
    }
    quota_database->Commit();
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::ScopedTempDir temp_directory_;
};

TEST_P(QuotaDatabaseTest, LazyOpen) {
  QuotaDatabase db(use_in_memory_db() ? base::FilePath() : DbPath());
  EXPECT_FALSE(LazyOpen(&db, LazyOpenMode::kFailIfNotFound));
  EXPECT_TRUE(LazyOpen(&db, LazyOpenMode::kCreateIfNotFound));

  if (GetParam()) {
    // Path should not exist for incognito mode.
    ASSERT_FALSE(base::PathExists(DbPath()));
  } else {
    ASSERT_TRUE(base::PathExists(DbPath()));
  }
}

TEST_P(QuotaDatabaseTest, HostQuota) {
  QuotaDatabase db(use_in_memory_db() ? base::FilePath() : DbPath());
  EXPECT_TRUE(LazyOpen(&db, LazyOpenMode::kCreateIfNotFound));

  const char* kHost = "foo.com";
  const int kQuota1 = 13579;
  const int kQuota2 = kQuota1 + 1024;

  int64_t quota = -1;
  EXPECT_FALSE(db.GetHostQuota(kHost, kTemp, &quota));
  EXPECT_FALSE(db.GetHostQuota(kHost, kPerm, &quota));

  // Insert quota for temporary.
  EXPECT_TRUE(db.SetHostQuota(kHost, kTemp, kQuota1));
  EXPECT_TRUE(db.GetHostQuota(kHost, kTemp, &quota));
  EXPECT_EQ(kQuota1, quota);

  // Update quota for temporary.
  EXPECT_TRUE(db.SetHostQuota(kHost, kTemp, kQuota2));
  EXPECT_TRUE(db.GetHostQuota(kHost, kTemp, &quota));
  EXPECT_EQ(kQuota2, quota);

  // Quota for persistent must not be updated.
  EXPECT_FALSE(db.GetHostQuota(kHost, kPerm, &quota));

  // Delete temporary storage quota.
  EXPECT_TRUE(db.DeleteHostQuota(kHost, kTemp));
  EXPECT_FALSE(db.GetHostQuota(kHost, kTemp, &quota));

  // Delete persistent quota by setting it to zero.
  EXPECT_TRUE(db.SetHostQuota(kHost, kPerm, 0));
  EXPECT_FALSE(db.GetHostQuota(kHost, kPerm, &quota));
}

TEST_P(QuotaDatabaseTest, GetOrCreateBucket) {
  QuotaDatabase db(use_in_memory_db() ? base::FilePath() : DbPath());
  EXPECT_TRUE(LazyOpen(&db, LazyOpenMode::kCreateIfNotFound));
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

TEST_P(QuotaDatabaseTest, GetBucket) {
  QuotaDatabase db(use_in_memory_db() ? base::FilePath() : DbPath());
  EXPECT_TRUE(LazyOpen(&db, LazyOpenMode::kCreateIfNotFound));

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

TEST_P(QuotaDatabaseTest, GetBucketWithNoDb) {
  QuotaDatabase db(use_in_memory_db() ? base::FilePath() : DbPath());
  EXPECT_FALSE(LazyOpen(&db, LazyOpenMode::kFailIfNotFound));

  StorageKey storage_key =
      StorageKey::CreateFromStringForTesting("http://google/");
  std::string bucket_name = "google_bucket";
  QuotaErrorOr<BucketInfo> result =
      db.GetBucket(storage_key, bucket_name, kTemp);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.error(), QuotaError::kNotFound);
}

// TODO(crbug.com/1216094): Update test to have its behavior on Fuchsia match
// with other platforms, and enable test on all platforms.
#if !defined(OS_FUCHSIA)
TEST_F(QuotaDatabaseTest, GetBucketWithOpenDatabaseError) {
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
}
#endif  // !defined(OS_FUCHSIA)

TEST_P(QuotaDatabaseTest, StorageKeyLastAccessTimeLRU) {
  QuotaDatabase db(use_in_memory_db() ? base::FilePath() : DbPath());
  EXPECT_TRUE(LazyOpen(&db, LazyOpenMode::kCreateIfNotFound));

  std::set<StorageKey> exceptions;
  absl::optional<StorageKey> storage_key;
  EXPECT_TRUE(db.GetLRUStorageKey(kTemp, exceptions, nullptr, &storage_key));
  EXPECT_FALSE(storage_key.has_value());

  const StorageKey kStorageKey1 =
      StorageKey::CreateFromStringForTesting("http://a/");
  const StorageKey kStorageKey2 =
      StorageKey::CreateFromStringForTesting("http://b/");
  const StorageKey kStorageKey3 =
      StorageKey::CreateFromStringForTesting("http://c/");
  const StorageKey kStorageKey4 =
      StorageKey::CreateFromStringForTesting("http://p/");

  // Adding three temporary storages, and
  EXPECT_TRUE(db.SetStorageKeyLastAccessTime(kStorageKey1, kTemp,
                                             base::Time::FromJavaTime(10)));
  EXPECT_TRUE(db.SetStorageKeyLastAccessTime(kStorageKey2, kTemp,
                                             base::Time::FromJavaTime(20)));
  EXPECT_TRUE(db.SetStorageKeyLastAccessTime(kStorageKey3, kTemp,
                                             base::Time::FromJavaTime(30)));

  // one persistent.
  EXPECT_TRUE(db.SetStorageKeyLastAccessTime(kStorageKey4, kPerm,
                                             base::Time::FromJavaTime(40)));

  EXPECT_TRUE(db.GetLRUStorageKey(kTemp, exceptions, nullptr, &storage_key));
  EXPECT_EQ(kStorageKey1, storage_key.value());

  // Test that unlimited origins are excluded from eviction, but
  // protected origins are not excluded.
  scoped_refptr<MockSpecialStoragePolicy> policy(new MockSpecialStoragePolicy);
  policy->AddUnlimited(kStorageKey1.origin().GetURL());
  policy->AddProtected(kStorageKey2.origin().GetURL());
  EXPECT_TRUE(
      db.GetLRUStorageKey(kTemp, exceptions, policy.get(), &storage_key));
  EXPECT_EQ(kStorageKey2, storage_key.value());

  // Test that durable origins are excluded from eviction.
  policy->AddDurable(kStorageKey2.origin().GetURL());
  EXPECT_TRUE(
      db.GetLRUStorageKey(kTemp, exceptions, policy.get(), &storage_key));
  EXPECT_EQ(kStorageKey3, storage_key.value());

  exceptions.insert(kStorageKey1);
  EXPECT_TRUE(db.GetLRUStorageKey(kTemp, exceptions, nullptr, &storage_key));
  EXPECT_EQ(kStorageKey2, storage_key.value());

  exceptions.insert(kStorageKey2);
  EXPECT_TRUE(db.GetLRUStorageKey(kTemp, exceptions, nullptr, &storage_key));
  EXPECT_EQ(kStorageKey3, storage_key.value());

  exceptions.insert(kStorageKey3);
  EXPECT_TRUE(db.GetLRUStorageKey(kTemp, exceptions, nullptr, &storage_key));
  EXPECT_FALSE(storage_key.has_value());

  EXPECT_TRUE(
      db.SetStorageKeyLastAccessTime(kStorageKey1, kTemp, base::Time::Now()));

  // Delete StorageKey/type last access time information.
  EXPECT_TRUE(db.DeleteStorageKeyInfo(kStorageKey3, kTemp));

  // Querying again to see if the deletion has worked.
  exceptions.clear();
  EXPECT_TRUE(db.GetLRUStorageKey(kTemp, exceptions, nullptr, &storage_key));
  EXPECT_EQ(kStorageKey2, storage_key.value());

  exceptions.insert(kStorageKey1);
  exceptions.insert(kStorageKey2);
  EXPECT_TRUE(db.GetLRUStorageKey(kTemp, exceptions, nullptr, &storage_key));
  EXPECT_FALSE(storage_key.has_value());
}

TEST_P(QuotaDatabaseTest, BucketLastAccessTimeLRU) {
  QuotaDatabase db(use_in_memory_db() ? base::FilePath() : DbPath());
  EXPECT_TRUE(LazyOpen(&db, LazyOpenMode::kCreateIfNotFound));

  std::set<StorageKey> exceptions;
  absl::optional<BucketId> bucket_id;
  EXPECT_TRUE(db.GetLRUBucket(kTemp, exceptions, nullptr, &bucket_id));
  EXPECT_FALSE(bucket_id.has_value());

  // Insert bucket entries into BucketTable.
  base::Time now = base::Time::Now();
  using Entry = QuotaDatabase::BucketTableEntry;
  Entry bucket1 = Entry(
      BucketId(1), StorageKey::CreateFromStringForTesting("http://example-a/"),
      kTemp, "bucket_a", 99, now, now);
  Entry bucket2 = Entry(
      BucketId(2), StorageKey::CreateFromStringForTesting("http://example-b/"),
      kTemp, "bucket_b", 0, now, now);
  Entry bucket3 = Entry(
      BucketId(3), StorageKey::CreateFromStringForTesting("http://example-c/"),
      kTemp, "bucket_c", 1, now, now);
  Entry bucket4 = Entry(
      BucketId(4), StorageKey::CreateFromStringForTesting("http://example-d/"),
      kPerm, "bucket_d", 5, now, now);
  Entry kTableEntries[] = {bucket1, bucket2, bucket3, bucket4};
  AssignBucketTable(&db, kTableEntries);

  // Update access time for three temporary storages, and
  EXPECT_TRUE(db.SetBucketLastAccessTime(bucket1.bucket_id,
                                         base::Time::FromJavaTime(10)));
  EXPECT_TRUE(db.SetBucketLastAccessTime(bucket2.bucket_id,
                                         base::Time::FromJavaTime(20)));
  EXPECT_TRUE(db.SetBucketLastAccessTime(bucket3.bucket_id,
                                         base::Time::FromJavaTime(30)));

  // one persistent.
  EXPECT_TRUE(db.SetBucketLastAccessTime(bucket4.bucket_id,
                                         base::Time::FromJavaTime(40)));

  EXPECT_TRUE(db.GetLRUBucket(kTemp, exceptions, nullptr, &bucket_id));
  EXPECT_EQ(bucket1.bucket_id, bucket_id);

  // Test that unlimited origins are excluded from eviction, but
  // protected origins are not excluded.
  scoped_refptr<MockSpecialStoragePolicy> policy(new MockSpecialStoragePolicy);
  policy->AddUnlimited(bucket1.storage_key.origin().GetURL());
  policy->AddProtected(bucket2.storage_key.origin().GetURL());
  EXPECT_TRUE(db.GetLRUBucket(kTemp, exceptions, policy.get(), &bucket_id));
  EXPECT_EQ(bucket2.bucket_id, bucket_id);

  // Test that durable origins are excluded from eviction.
  policy->AddDurable(bucket2.storage_key.origin().GetURL());
  EXPECT_TRUE(db.GetLRUBucket(kTemp, exceptions, policy.get(), &bucket_id));
  EXPECT_EQ(bucket3.bucket_id, bucket_id);

  exceptions.insert(bucket1.storage_key);
  EXPECT_TRUE(db.GetLRUBucket(kTemp, exceptions, nullptr, &bucket_id));
  EXPECT_EQ(bucket2.bucket_id, bucket_id);

  exceptions.insert(bucket2.storage_key);
  EXPECT_TRUE(db.GetLRUBucket(kTemp, exceptions, nullptr, &bucket_id));
  EXPECT_EQ(bucket3.bucket_id, bucket_id);

  exceptions.insert(bucket3.storage_key);
  EXPECT_TRUE(db.GetLRUBucket(kTemp, exceptions, nullptr, &bucket_id));
  EXPECT_FALSE(bucket_id.has_value());

  EXPECT_TRUE(db.SetBucketLastAccessTime(bucket1.bucket_id, base::Time::Now()));

  // Delete storage_key/type last access time information.
  EXPECT_TRUE(db.DeleteBucketInfo(bucket3.bucket_id));

  // Querying again to see if the deletion has worked.
  exceptions.clear();
  EXPECT_TRUE(db.GetLRUBucket(kTemp, exceptions, nullptr, &bucket_id));
  EXPECT_EQ(bucket2.bucket_id, bucket_id);

  exceptions.insert(bucket1.storage_key);
  exceptions.insert(bucket2.storage_key);
  EXPECT_TRUE(db.GetLRUBucket(kTemp, exceptions, nullptr, &bucket_id));
  EXPECT_FALSE(bucket_id.has_value());
}

TEST_P(QuotaDatabaseTest, StorageKeyLastModifiedBetween) {
  QuotaDatabase db(use_in_memory_db() ? base::FilePath() : DbPath());
  EXPECT_TRUE(LazyOpen(&db, LazyOpenMode::kCreateIfNotFound));

  std::set<StorageKey> storage_keys;
  EXPECT_TRUE(db.GetStorageKeysModifiedBetween(
      kTemp, &storage_keys, base::Time(), base::Time::Max()));
  EXPECT_TRUE(storage_keys.empty());

  const StorageKey kStorageKey1 =
      StorageKey::CreateFromStringForTesting("http://a/");
  const StorageKey kStorageKey2 =
      StorageKey::CreateFromStringForTesting("http://b/");
  const StorageKey kStorageKey3 =
      StorageKey::CreateFromStringForTesting("http://c/");

  // Report last mod time for the test storage_keys.
  EXPECT_TRUE(db.SetStorageKeyLastModifiedTime(kStorageKey1, kTemp,
                                               base::Time::FromJavaTime(0)));
  EXPECT_TRUE(db.SetStorageKeyLastModifiedTime(kStorageKey2, kTemp,
                                               base::Time::FromJavaTime(10)));
  EXPECT_TRUE(db.SetStorageKeyLastModifiedTime(kStorageKey3, kTemp,
                                               base::Time::FromJavaTime(20)));

  EXPECT_TRUE(db.GetStorageKeysModifiedBetween(
      kTemp, &storage_keys, base::Time(), base::Time::Max()));
  EXPECT_EQ(3U, storage_keys.size());
  EXPECT_EQ(1U, storage_keys.count(kStorageKey1));
  EXPECT_EQ(1U, storage_keys.count(kStorageKey2));
  EXPECT_EQ(1U, storage_keys.count(kStorageKey3));

  EXPECT_TRUE(db.GetStorageKeysModifiedBetween(
      kTemp, &storage_keys, base::Time::FromJavaTime(5), base::Time::Max()));
  EXPECT_EQ(2U, storage_keys.size());
  EXPECT_EQ(0U, storage_keys.count(kStorageKey1));
  EXPECT_EQ(1U, storage_keys.count(kStorageKey2));
  EXPECT_EQ(1U, storage_keys.count(kStorageKey3));

  EXPECT_TRUE(db.GetStorageKeysModifiedBetween(
      kTemp, &storage_keys, base::Time::FromJavaTime(15), base::Time::Max()));
  EXPECT_EQ(1U, storage_keys.size());
  EXPECT_EQ(0U, storage_keys.count(kStorageKey1));
  EXPECT_EQ(0U, storage_keys.count(kStorageKey2));
  EXPECT_EQ(1U, storage_keys.count(kStorageKey3));

  EXPECT_TRUE(db.GetStorageKeysModifiedBetween(
      kTemp, &storage_keys, base::Time::FromJavaTime(25), base::Time::Max()));
  EXPECT_TRUE(storage_keys.empty());

  EXPECT_TRUE(db.GetStorageKeysModifiedBetween(kTemp, &storage_keys,
                                               base::Time::FromJavaTime(5),
                                               base::Time::FromJavaTime(15)));
  EXPECT_EQ(1U, storage_keys.size());
  EXPECT_EQ(0U, storage_keys.count(kStorageKey1));
  EXPECT_EQ(1U, storage_keys.count(kStorageKey2));
  EXPECT_EQ(0U, storage_keys.count(kStorageKey3));

  EXPECT_TRUE(db.GetStorageKeysModifiedBetween(kTemp, &storage_keys,
                                               base::Time::FromJavaTime(0),
                                               base::Time::FromJavaTime(20)));
  EXPECT_EQ(2U, storage_keys.size());
  EXPECT_EQ(1U, storage_keys.count(kStorageKey1));
  EXPECT_EQ(1U, storage_keys.count(kStorageKey2));
  EXPECT_EQ(0U, storage_keys.count(kStorageKey3));

  // Update storage key's mod time but for persistent storage.
  EXPECT_TRUE(db.SetStorageKeyLastModifiedTime(kStorageKey1, kPerm,
                                               base::Time::FromJavaTime(30)));

  // Must have no effects on temporary storage_keys info.
  EXPECT_TRUE(db.GetStorageKeysModifiedBetween(
      kTemp, &storage_keys, base::Time::FromJavaTime(5), base::Time::Max()));
  EXPECT_EQ(2U, storage_keys.size());
  EXPECT_EQ(0U, storage_keys.count(kStorageKey1));
  EXPECT_EQ(1U, storage_keys.count(kStorageKey2));
  EXPECT_EQ(1U, storage_keys.count(kStorageKey3));

  // One more update for persistent storage key.
  EXPECT_TRUE(db.SetStorageKeyLastModifiedTime(kStorageKey2, kPerm,
                                               base::Time::FromJavaTime(40)));

  EXPECT_TRUE(db.GetStorageKeysModifiedBetween(
      kPerm, &storage_keys, base::Time::FromJavaTime(25), base::Time::Max()));
  EXPECT_EQ(2U, storage_keys.size());
  EXPECT_EQ(1U, storage_keys.count(kStorageKey1));
  EXPECT_EQ(1U, storage_keys.count(kStorageKey2));
  EXPECT_EQ(0U, storage_keys.count(kStorageKey3));

  EXPECT_TRUE(db.GetStorageKeysModifiedBetween(
      kPerm, &storage_keys, base::Time::FromJavaTime(35), base::Time::Max()));
  EXPECT_EQ(1U, storage_keys.size());
  EXPECT_EQ(0U, storage_keys.count(kStorageKey1));
  EXPECT_EQ(1U, storage_keys.count(kStorageKey2));
  EXPECT_EQ(0U, storage_keys.count(kStorageKey3));
}

TEST_P(QuotaDatabaseTest, BucketLastModifiedBetween) {
  QuotaDatabase db(use_in_memory_db() ? base::FilePath() : DbPath());
  EXPECT_TRUE(LazyOpen(&db, LazyOpenMode::kCreateIfNotFound));

  std::set<BucketId> bucket_ids;
  EXPECT_TRUE(db.GetBucketsModifiedBetween(kTemp, &bucket_ids, base::Time(),
                                           base::Time::Max()));
  EXPECT_TRUE(bucket_ids.empty());

  // Insert bucket entries into BucketTable.
  base::Time now = base::Time::Now();
  using Entry = QuotaDatabase::BucketTableEntry;
  Entry bucket1 = Entry(
      BucketId(1), StorageKey::CreateFromStringForTesting("http://example-a/"),
      kTemp, "bucket_a", 0, now, now);
  Entry bucket2 = Entry(
      BucketId(2), StorageKey::CreateFromStringForTesting("http://example-b/"),
      kTemp, "bucket_b", 0, now, now);
  Entry bucket3 = Entry(
      BucketId(3), StorageKey::CreateFromStringForTesting("http://example-c/"),
      kTemp, "bucket_c", 0, now, now);
  Entry bucket4 = Entry(
      BucketId(4), StorageKey::CreateFromStringForTesting("http://example-d/"),
      kPerm, "bucket_d", 0, now, now);
  Entry kTableEntries[] = {bucket1, bucket2, bucket3, bucket4};
  AssignBucketTable(&db, kTableEntries);

  // Report last mod time for the buckets.
  EXPECT_TRUE(db.SetBucketLastModifiedTime(bucket1.bucket_id,
                                           base::Time::FromJavaTime(0)));
  EXPECT_TRUE(db.SetBucketLastModifiedTime(bucket2.bucket_id,
                                           base::Time::FromJavaTime(10)));
  EXPECT_TRUE(db.SetBucketLastModifiedTime(bucket3.bucket_id,
                                           base::Time::FromJavaTime(20)));
  EXPECT_TRUE(db.SetBucketLastModifiedTime(bucket4.bucket_id,
                                           base::Time::FromJavaTime(30)));

  EXPECT_TRUE(db.GetBucketsModifiedBetween(kTemp, &bucket_ids, base::Time(),
                                           base::Time::Max()));
  EXPECT_EQ(3U, bucket_ids.size());
  EXPECT_EQ(1U, bucket_ids.count(bucket1.bucket_id));
  EXPECT_EQ(1U, bucket_ids.count(bucket2.bucket_id));
  EXPECT_EQ(1U, bucket_ids.count(bucket3.bucket_id));
  EXPECT_EQ(0U, bucket_ids.count(bucket4.bucket_id));

  EXPECT_TRUE(db.GetBucketsModifiedBetween(
      kTemp, &bucket_ids, base::Time::FromJavaTime(5), base::Time::Max()));
  EXPECT_EQ(2U, bucket_ids.size());
  EXPECT_EQ(0U, bucket_ids.count(bucket1.bucket_id));
  EXPECT_EQ(1U, bucket_ids.count(bucket2.bucket_id));
  EXPECT_EQ(1U, bucket_ids.count(bucket3.bucket_id));
  EXPECT_EQ(0U, bucket_ids.count(bucket4.bucket_id));

  EXPECT_TRUE(db.GetBucketsModifiedBetween(
      kTemp, &bucket_ids, base::Time::FromJavaTime(15), base::Time::Max()));
  EXPECT_EQ(1U, bucket_ids.size());
  EXPECT_EQ(0U, bucket_ids.count(bucket1.bucket_id));
  EXPECT_EQ(0U, bucket_ids.count(bucket2.bucket_id));
  EXPECT_EQ(1U, bucket_ids.count(bucket3.bucket_id));
  EXPECT_EQ(0U, bucket_ids.count(bucket4.bucket_id));

  EXPECT_TRUE(db.GetBucketsModifiedBetween(
      kTemp, &bucket_ids, base::Time::FromJavaTime(25), base::Time::Max()));
  EXPECT_TRUE(bucket_ids.empty());

  EXPECT_TRUE(db.GetBucketsModifiedBetween(kTemp, &bucket_ids,
                                           base::Time::FromJavaTime(5),
                                           base::Time::FromJavaTime(15)));
  EXPECT_EQ(1U, bucket_ids.size());
  EXPECT_EQ(0U, bucket_ids.count(bucket1.bucket_id));
  EXPECT_EQ(1U, bucket_ids.count(bucket2.bucket_id));
  EXPECT_EQ(0U, bucket_ids.count(bucket3.bucket_id));
  EXPECT_EQ(0U, bucket_ids.count(bucket4.bucket_id));

  EXPECT_TRUE(db.GetBucketsModifiedBetween(kTemp, &bucket_ids,
                                           base::Time::FromJavaTime(0),
                                           base::Time::FromJavaTime(20)));
  EXPECT_EQ(2U, bucket_ids.size());
  EXPECT_EQ(1U, bucket_ids.count(bucket1.bucket_id));
  EXPECT_EQ(1U, bucket_ids.count(bucket2.bucket_id));
  EXPECT_EQ(0U, bucket_ids.count(bucket3.bucket_id));
  EXPECT_EQ(0U, bucket_ids.count(bucket4.bucket_id));

  EXPECT_TRUE(db.GetBucketsModifiedBetween(kPerm, &bucket_ids,
                                           base::Time::FromJavaTime(0),
                                           base::Time::FromJavaTime(35)));
  EXPECT_EQ(1U, bucket_ids.size());
  EXPECT_EQ(0U, bucket_ids.count(bucket1.bucket_id));
  EXPECT_EQ(0U, bucket_ids.count(bucket2.bucket_id));
  EXPECT_EQ(0U, bucket_ids.count(bucket3.bucket_id));
  EXPECT_EQ(1U, bucket_ids.count(bucket4.bucket_id));
}

TEST_P(QuotaDatabaseTest, RegisterInitialStorageKeyInfo) {
  QuotaDatabase db(use_in_memory_db() ? base::FilePath() : DbPath());

  const StorageKey kStorageKeys[] = {
      StorageKey::CreateFromStringForTesting("http://a/"),
      StorageKey::CreateFromStringForTesting("http://b/"),
      StorageKey::CreateFromStringForTesting("http://c/")};
  std::set<StorageKey> storage_keys(kStorageKeys, std::end(kStorageKeys));

  EXPECT_TRUE(db.RegisterInitialStorageKeyInfo(storage_keys, kTemp));

  QuotaDatabase::BucketTableEntry info;
  info.use_count = -1;
  EXPECT_TRUE(db.GetStorageKeyInfo(
      StorageKey::CreateFromStringForTesting("http://a/"), kTemp, &info));
  EXPECT_EQ(0, info.use_count);

  EXPECT_TRUE(db.SetStorageKeyLastAccessTime(
      StorageKey::CreateFromStringForTesting("http://a/"), kTemp,
      base::Time::FromDoubleT(1.0)));
  info.use_count = -1;
  EXPECT_TRUE(db.GetStorageKeyInfo(
      StorageKey::CreateFromStringForTesting("http://a/"), kTemp, &info));
  EXPECT_EQ(1, info.use_count);

  EXPECT_TRUE(db.RegisterInitialStorageKeyInfo(storage_keys, kTemp));

  info.use_count = -1;
  EXPECT_TRUE(db.GetStorageKeyInfo(
      StorageKey::CreateFromStringForTesting("http://a/"), kTemp, &info));
  EXPECT_EQ(1, info.use_count);
}

TEST_P(QuotaDatabaseTest, DumpQuotaTable) {
  QuotaTableEntry kTableEntries[] = {
      {.host = "http://go/", .type = kTemp, .quota = 1},
      {.host = "http://oo/", .type = kTemp, .quota = 2},
      {.host = "http://gle/", .type = kPerm, .quota = 3}};

  QuotaDatabase db(use_in_memory_db() ? base::FilePath() : DbPath());
  EXPECT_TRUE(LazyOpen(&db, LazyOpenMode::kCreateIfNotFound));
  AssignQuotaTable(&db, kTableEntries);

  using Verifier = EntryVerifier<QuotaTableEntry>;
  Verifier verifier(kTableEntries, std::end(kTableEntries));
  EXPECT_TRUE(DumpQuotaTable(
      &db, base::BindRepeating(&Verifier::Run, base::Unretained(&verifier))));
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
  EXPECT_TRUE(LazyOpen(&db, LazyOpenMode::kCreateIfNotFound));
  AssignBucketTable(&db, kTableEntries);

  using Verifier = EntryVerifier<Entry>;
  Verifier verifier(kTableEntries, std::end(kTableEntries));
  EXPECT_TRUE(DumpBucketTable(
      &db, base::BindRepeating(&Verifier::Run, base::Unretained(&verifier))));
  EXPECT_TRUE(verifier.table.empty());
}

TEST_P(QuotaDatabaseTest, GetStorageKeyInfo) {
  const StorageKey kStorageKey =
      StorageKey::CreateFromStringForTesting("http://go/");
  using Entry = QuotaDatabase::BucketTableEntry;
  Entry kTableEntries[] = {Entry(BucketId(1), kStorageKey, kTemp,
                                 kDefaultBucketName, 100, base::Time(),
                                 base::Time())};

  QuotaDatabase db(use_in_memory_db() ? base::FilePath() : DbPath());
  EXPECT_TRUE(LazyOpen(&db, LazyOpenMode::kCreateIfNotFound));
  AssignBucketTable(&db, kTableEntries);

  {
    Entry entry;
    EXPECT_TRUE(db.GetStorageKeyInfo(kStorageKey, kTemp, &entry));
    EXPECT_EQ(kTableEntries[0].type, entry.type);
    EXPECT_EQ(kTableEntries[0].storage_key, entry.storage_key);
    EXPECT_EQ(kTableEntries[0].name, entry.name);
    EXPECT_EQ(kTableEntries[0].use_count, entry.use_count);
    EXPECT_EQ(kTableEntries[0].last_accessed, entry.last_accessed);
    EXPECT_EQ(kTableEntries[0].last_modified, entry.last_modified);
  }

  {
    Entry entry;
    EXPECT_FALSE(db.GetStorageKeyInfo(
        StorageKey::CreateFromStringForTesting("http://notpresent.org/"), kTemp,
        &entry));
  }
}

TEST_P(QuotaDatabaseTest, GetBucketInfo) {
  using Entry = QuotaDatabase::BucketTableEntry;
  Entry kTableEntries[] = {
      Entry(BucketId(123), StorageKey::CreateFromStringForTesting("http://go/"),
            kTemp, "test_bucket", 100, base::Time(), base::Time())};

  QuotaDatabase db(use_in_memory_db() ? base::FilePath() : DbPath());
  EXPECT_TRUE(LazyOpen(&db, LazyOpenMode::kCreateIfNotFound));
  AssignBucketTable(&db, kTableEntries);

  {
    Entry entry;
    EXPECT_TRUE(db.GetBucketInfo(kTableEntries[0].bucket_id, &entry));
    EXPECT_EQ(kTableEntries[0].bucket_id, entry.bucket_id);
    EXPECT_EQ(kTableEntries[0].type, entry.type);
    EXPECT_EQ(kTableEntries[0].storage_key, entry.storage_key);
    EXPECT_EQ(kTableEntries[0].name, entry.name);
    EXPECT_EQ(kTableEntries[0].use_count, entry.use_count);
    EXPECT_EQ(kTableEntries[0].last_accessed, entry.last_accessed);
    EXPECT_EQ(kTableEntries[0].last_modified, entry.last_modified);
  }

  {
    // BucketId 456 is not in the database.
    Entry entry;
    EXPECT_FALSE(db.GetBucketInfo(BucketId(456), &entry));
  }
}

// Non-parameterized tests.
TEST_F(QuotaDatabaseTest, BootstrapFlag) {
  QuotaDatabase db(DbPath());

  EXPECT_FALSE(db.IsStorageKeyDatabaseBootstrapped());
  EXPECT_TRUE(db.SetStorageKeyDatabaseBootstrapped(true));
  EXPECT_TRUE(db.IsStorageKeyDatabaseBootstrapped());
  EXPECT_TRUE(db.SetStorageKeyDatabaseBootstrapped(false));
  EXPECT_FALSE(db.IsStorageKeyDatabaseBootstrapped());
}

TEST_F(QuotaDatabaseTest, OpenCorruptedDatabase) {
  // Create database, force corruption and close db by leaving scope.
  {
    QuotaDatabase db(DbPath());
    ASSERT_TRUE(LazyOpen(&db, LazyOpenMode::kCreateIfNotFound));
    ASSERT_TRUE(sql::test::CorruptSizeInHeader(DbPath()));
  }
  // Reopen database and verify schema reset on reopen.
  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(SQLITE_CORRUPT);
    QuotaDatabase db(DbPath());
    ASSERT_TRUE(LazyOpen(&db, LazyOpenMode::kFailIfNotFound));
    EXPECT_TRUE(expecter.SawExpectedErrors());
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         QuotaDatabaseTest,
                         /* is_incognito */ testing::Bool());

}  // namespace storage
