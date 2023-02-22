// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <iterator>
#include <memory>
#include <set>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/cpp/buckets/constants.h"
#include "components/services/storage/public/cpp/constants.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/sqlite_result_code.h"
#include "sql/sqlite_result_code_values.h"
#include "sql/statement.h"
#include "sql/test/scoped_error_expecter.h"
#include "sql/test/test_helpers.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/quota/quota_database.h"
#include "storage/browser/quota/quota_internals.mojom.h"
#include "storage/browser/quota/storage_directory_util.h"
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
static const blink::mojom::StorageType kSync =
    blink::mojom::StorageType::kSyncable;

static const storage::mojom::StorageType kStorageTemp =
    storage::mojom::StorageType::kTemporary;
static const storage::mojom::StorageType kStorageSync =
    storage::mojom::StorageType::kSyncable;

static constexpr char kDatabaseName[] = "QuotaManager";

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
  using BucketTableEntry = mojom::BucketTableEntry;

  void SetUp() override { ASSERT_TRUE(temp_directory_.CreateUniqueTempDir()); }

  void TearDown() override { ASSERT_TRUE(temp_directory_.Delete()); }

  bool use_in_memory_db() const { return GetParam(); }

  base::FilePath ProfilePath() { return temp_directory_.GetPath(); }

  base::FilePath DbPath() {
    return ProfilePath()
        .Append(kWebStorageDirectory)
        .AppendASCII(kDatabaseName);
  }

  std::unique_ptr<QuotaDatabase> CreateDatabase(bool is_incognito) {
    return std::make_unique<QuotaDatabase>(is_incognito ? base::FilePath()
                                                        : ProfilePath());
  }

  bool EnsureOpened(QuotaDatabase* db) {
    return db->EnsureOpened() == QuotaError::kNone;
  }

  template <typename EntryType>
  struct EntryVerifier {
    std::set<EntryType> table;

    template <size_t length>
    explicit EntryVerifier(const EntryType (&entries)[length]) {
      for (size_t i = 0; i < length; ++i) {
        table.insert(entries[i]->Clone());
      }
    }

    bool Run(EntryType entry) {
      EXPECT_EQ(1u, table.erase(std::move(entry)));
      return true;
    }
  };

  QuotaError DumpBucketTable(
      QuotaDatabase* quota_database,
      const QuotaDatabase::BucketTableCallback& callback) {
    return quota_database->DumpBucketTable(callback);
  }

  template <typename Container>
  void AssignBucketTable(QuotaDatabase* quota_database, Container&& entries) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(quota_database->sequence_checker_);
    ASSERT_TRUE(quota_database->db_);
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
              "quota,"
              "persistent,"
              "durability) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, 0, 0, 0, 0)";
      // clang-format on
      sql::Statement statement;
      statement.Assign(
          quota_database->db_->GetCachedStatement(SQL_FROM_HERE, kSql));
      ASSERT_TRUE(statement.is_valid());

      absl::optional<StorageKey> storage_key =
          StorageKey::Deserialize(entry->storage_key);
      ASSERT_TRUE(storage_key.has_value());

      statement.BindInt64(0, entry->bucket_id);
      statement.BindString(1, entry->storage_key);
      statement.BindString(2, std::move(storage_key).value().origin().host());
      statement.BindInt(3, static_cast<int>(entry->type));
      statement.BindString(4, entry->name);
      statement.BindInt(5, static_cast<int>(entry->use_count));
      statement.BindTime(6, entry->last_accessed);
      statement.BindTime(7, entry->last_modified);
      EXPECT_TRUE(statement.Run());
    }
    quota_database->Commit();
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::ScopedTempDir temp_directory_;
};

TEST_P(QuotaDatabaseTest, EnsureOpened) {
  auto db = CreateDatabase(use_in_memory_db());
  EXPECT_TRUE(EnsureOpened(db.get()));

  if (GetParam()) {
    // Path should not exist for incognito mode.
    ASSERT_FALSE(base::PathExists(DbPath()));
  } else {
    ASSERT_TRUE(base::PathExists(DbPath()));
  }
}

TEST_P(QuotaDatabaseTest, RazeAndReopenWithNoDb) {
  QuotaDatabase db(use_in_memory_db() ? base::FilePath() : DbPath());
  // RazeAndReopen() with no db tries to create the db one last time.
  EXPECT_EQ(db.RazeAndReopen(), QuotaError::kNone);

  if (GetParam()) {
    // Path should not exist for incognito mode.
    ASSERT_FALSE(base::PathExists(DbPath()));
  } else {
    ASSERT_TRUE(base::PathExists(DbPath()));
  }
}

TEST_P(QuotaDatabaseTest, UpdateOrCreateBucket) {
  auto db = CreateDatabase(use_in_memory_db());
  EXPECT_TRUE(EnsureOpened(db.get()));
  BucketInitParams params(
      StorageKey::CreateFromStringForTesting("http://google/"),
      "google_bucket");

  QuotaErrorOr<BucketInfo> result = db->UpdateOrCreateBucket(params, 0);
  ASSERT_TRUE(result.ok());

  BucketInfo created_bucket = result.value();
  ASSERT_GT(created_bucket.id.value(), 0);
  ASSERT_EQ(created_bucket.name, params.name);
  ASSERT_EQ(created_bucket.storage_key, params.storage_key);
  ASSERT_EQ(created_bucket.type, kTemp);

  // Should return the same bucket when querying again.
  result = db->UpdateOrCreateBucket(params, 0);
  ASSERT_TRUE(result.ok());

  BucketInfo retrieved_bucket = result.value();
  ASSERT_EQ(retrieved_bucket.id, created_bucket.id);
  ASSERT_EQ(retrieved_bucket.name, created_bucket.name);
  ASSERT_EQ(retrieved_bucket.storage_key, created_bucket.storage_key);
  ASSERT_EQ(retrieved_bucket.type, created_bucket.type);

  // Test `max_bucket_count`.
  BucketInitParams params2(
      StorageKey::CreateFromStringForTesting("http://google/"),
      "google_bucket2");
  result = db->UpdateOrCreateBucket(params2, 1);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ(QuotaError::kQuotaExceeded, result.error());

  // It doesn't affect the update case.
  result = db->UpdateOrCreateBucket(params, 1);
  ASSERT_TRUE(result.ok());
}

TEST_P(QuotaDatabaseTest, UpdateBucket) {
  auto db = CreateDatabase(use_in_memory_db());
  EXPECT_TRUE(EnsureOpened(db.get()));
  BucketInitParams params(
      StorageKey::CreateFromStringForTesting("http://google/"),
      "google_bucket");
  params.expiration = base::Time::Now() + base::Days(1);
  params.persistent = true;

  QuotaErrorOr<BucketInfo> result = db->UpdateOrCreateBucket(params, 0);
  ASSERT_TRUE(result.ok());
  BucketInfo created_bucket = result.value();

  EXPECT_EQ(params.expiration, result->expiration);
  EXPECT_TRUE(result->persistent);

  // Should update the bucket when querying again.
  params.expiration = base::Time::Now() + base::Days(2);
  params.persistent = false;
  params.quota = 1024 * 1024 * 20;  // 20 MB
  params.durability = blink::mojom::BucketDurability::kStrict;
  result = db->UpdateOrCreateBucket(params, 0);
  ASSERT_TRUE(result.ok());
  BucketInfo updated_bucket = result.value();

  EXPECT_EQ(updated_bucket.id, created_bucket.id);

  // Expiration is updated.
  EXPECT_EQ(updated_bucket.expiration, params.expiration);
  EXPECT_NE(updated_bucket.expiration, created_bucket.expiration);

  // Persistence is updated.
  EXPECT_EQ(updated_bucket.persistent, params.persistent);
  EXPECT_NE(updated_bucket.persistent, created_bucket.persistent);

  // Quota can't be updated.
  EXPECT_NE(updated_bucket.quota, params.quota);
  EXPECT_EQ(updated_bucket.quota, created_bucket.quota);

  // Durability can't be updated.
  EXPECT_NE(updated_bucket.durability, *params.durability);
  EXPECT_EQ(updated_bucket.durability, created_bucket.durability);

  // Query, but without explicit policies.
  params.expiration = base::Time();
  params.persistent.reset();
  result = db->UpdateOrCreateBucket(params, 0);
  ASSERT_TRUE(result.ok());

  // Expiration and persistence are unchanged.
  EXPECT_EQ(result->expiration, updated_bucket.expiration);
  EXPECT_EQ(result->persistent, updated_bucket.persistent);
}

TEST_P(QuotaDatabaseTest, GetOrCreateBucketDeprecated) {
  auto db = CreateDatabase(use_in_memory_db());
  EXPECT_TRUE(EnsureOpened(db.get()));
  StorageKey storage_key =
      StorageKey::CreateFromStringForTesting("http://google/");

  QuotaErrorOr<BucketInfo> result =
      db->GetOrCreateBucketDeprecated({storage_key, kDefaultBucketName}, kSync);
  ASSERT_TRUE(result.ok());

  BucketInfo created_bucket = result.value();
  ASSERT_GT(created_bucket.id.value(), 0);
  ASSERT_EQ(created_bucket.name, kDefaultBucketName);
  ASSERT_EQ(created_bucket.storage_key, storage_key);
  ASSERT_EQ(created_bucket.type, kSync);

  // Should return the same bucket when querying again.
  result =
      db->GetOrCreateBucketDeprecated({storage_key, kDefaultBucketName}, kSync);
  ASSERT_TRUE(result.ok());

  BucketInfo retrieved_bucket = result.value();
  ASSERT_EQ(retrieved_bucket.id, created_bucket.id);
  ASSERT_EQ(retrieved_bucket.name, created_bucket.name);
  ASSERT_EQ(retrieved_bucket.storage_key, created_bucket.storage_key);
  ASSERT_EQ(retrieved_bucket.type, created_bucket.type);
}

TEST_P(QuotaDatabaseTest, GetBucket) {
  auto db = CreateDatabase(use_in_memory_db());
  EXPECT_TRUE(EnsureOpened(db.get()));

  // Add a bucket entry into the bucket table.
  StorageKey storage_key =
      StorageKey::CreateFromStringForTesting("http://google/");
  std::string bucket_name = "google_bucket";
  QuotaErrorOr<BucketInfo> result =
      db->CreateBucketForTesting(storage_key, bucket_name, kTemp);
  ASSERT_TRUE(result.ok());

  BucketInfo created_bucket = result.value();
  ASSERT_GT(created_bucket.id.value(), 0);
  ASSERT_EQ(created_bucket.name, bucket_name);
  ASSERT_EQ(created_bucket.storage_key, storage_key);
  ASSERT_EQ(created_bucket.type, kTemp);

  result = db->GetBucket(storage_key, bucket_name, kTemp);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.value().id, created_bucket.id);
  EXPECT_EQ(result.value().name, created_bucket.name);
  EXPECT_EQ(result.value().storage_key, created_bucket.storage_key);
  ASSERT_EQ(result.value().type, created_bucket.type);

  // Can't retrieve buckets with name mismatch.
  result = db->GetBucket(storage_key, "does_not_exist", kTemp);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.error(), QuotaError::kNotFound);

  // Can't retrieve buckets with StorageKey mismatch.
  result =
      db->GetBucket(StorageKey::CreateFromStringForTesting("http://example/"),
                    bucket_name, kTemp);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.error(), QuotaError::kNotFound);
}

TEST_P(QuotaDatabaseTest, GetBucketById) {
  auto db = CreateDatabase(use_in_memory_db());
  EXPECT_TRUE(EnsureOpened(db.get()));

  // Add a bucket entry into the bucket table.
  StorageKey storage_key =
      StorageKey::CreateFromStringForTesting("http://google/");
  std::string bucket_name = "google_bucket";
  QuotaErrorOr<BucketInfo> result =
      db->CreateBucketForTesting(storage_key, bucket_name, kTemp);
  ASSERT_TRUE(result.ok());

  BucketInfo created_bucket = result.value();
  ASSERT_GT(created_bucket.id.value(), 0);
  ASSERT_EQ(created_bucket.name, bucket_name);
  ASSERT_EQ(created_bucket.storage_key, storage_key);
  ASSERT_EQ(created_bucket.type, kTemp);

  result = db->GetBucketById(created_bucket.id);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.value().name, created_bucket.name);
  EXPECT_EQ(result.value().storage_key, created_bucket.storage_key);
  ASSERT_EQ(result.value().type, created_bucket.type);

  constexpr BucketId kNonExistentBucketId(7777);
  result = db->GetBucketById(BucketId(kNonExistentBucketId));
  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.error(), QuotaError::kNotFound);
}

TEST_P(QuotaDatabaseTest, GetBucketsForType) {
  auto db = CreateDatabase(use_in_memory_db());
  EXPECT_TRUE(EnsureOpened(db.get()));

  const StorageKey storage_key1 =
      StorageKey::CreateFromStringForTesting("http://example-a/");
  const StorageKey storage_key2 =
      StorageKey::CreateFromStringForTesting("http://example-b/");
  const StorageKey storage_key3 =
      StorageKey::CreateFromStringForTesting("http://example-c/");

  QuotaErrorOr<BucketInfo> bucket_result =
      db->CreateBucketForTesting(storage_key1, "temp_bucket", kTemp);
  ASSERT_TRUE(bucket_result.ok());
  BucketInfo temp_bucket1 = bucket_result.value();

  bucket_result =
      db->CreateBucketForTesting(storage_key2, "temp_bucket", kTemp);
  ASSERT_TRUE(bucket_result.ok());
  BucketInfo temp_bucket2 = bucket_result.value();

  bucket_result =
      db->CreateBucketForTesting(storage_key1, kDefaultBucketName, kSync);
  ASSERT_TRUE(bucket_result.ok());
  BucketInfo sync_bucket1 = bucket_result.value();

  bucket_result =
      db->CreateBucketForTesting(storage_key3, kDefaultBucketName, kSync);
  ASSERT_TRUE(bucket_result.ok());
  BucketInfo sync_bucket2 = bucket_result.value();

  QuotaErrorOr<std::set<BucketInfo>> result = db->GetBucketsForType(kTemp);
  ASSERT_TRUE(result.ok());
  std::set<BucketLocator> buckets = BucketInfosToBucketLocators(result.value());
  ASSERT_EQ(2U, buckets.size());
  EXPECT_TRUE(ContainsBucket(buckets, temp_bucket1));
  EXPECT_TRUE(ContainsBucket(buckets, temp_bucket2));

  result = db->GetBucketsForType(kSync);
  ASSERT_TRUE(result.ok());
  buckets = BucketInfosToBucketLocators(result.value());
  ASSERT_EQ(2U, buckets.size());
  EXPECT_TRUE(ContainsBucket(buckets, sync_bucket1));
  EXPECT_TRUE(ContainsBucket(buckets, sync_bucket2));
}

TEST_P(QuotaDatabaseTest, GetBucketsForHost) {
  auto db = CreateDatabase(use_in_memory_db());
  EXPECT_TRUE(EnsureOpened(db.get()));

  QuotaErrorOr<BucketInfo> temp_example_bucket1 = db->CreateBucketForTesting(
      StorageKey::CreateFromStringForTesting("https://example.com/"),
      kDefaultBucketName, kTemp);
  QuotaErrorOr<BucketInfo> temp_example_bucket2 = db->CreateBucketForTesting(
      StorageKey::CreateFromStringForTesting("http://example.com:123/"),
      kDefaultBucketName, kTemp);
  QuotaErrorOr<BucketInfo> perm_google_bucket1 = db->CreateBucketForTesting(
      StorageKey::CreateFromStringForTesting("http://google.com/"),
      kDefaultBucketName, kSync);
  QuotaErrorOr<BucketInfo> temp_google_bucket2 = db->CreateBucketForTesting(
      StorageKey::CreateFromStringForTesting("http://google.com:123/"),
      kDefaultBucketName, kTemp);

  QuotaErrorOr<std::set<BucketInfo>> result =
      db->GetBucketsForHost("example.com", kTemp);
  ASSERT_TRUE(result.ok());
  ASSERT_EQ(result->size(), 2U);
  EXPECT_TRUE(base::Contains(result.value(), temp_example_bucket1.value()));
  EXPECT_TRUE(base::Contains(result.value(), temp_example_bucket2.value()));

  result = db->GetBucketsForHost("example.com", kSync);
  ASSERT_TRUE(result.ok());
  ASSERT_EQ(result->size(), 0U);

  result = db->GetBucketsForHost("google.com", kSync);
  ASSERT_TRUE(result.ok());
  ASSERT_EQ(result->size(), 1U);
  EXPECT_TRUE(base::Contains(result.value(), perm_google_bucket1.value()));

  result = db->GetBucketsForHost("google.com", kTemp);
  ASSERT_TRUE(result.ok());
  ASSERT_EQ(result->size(), 1U);
  EXPECT_TRUE(base::Contains(result.value(), temp_google_bucket2.value()));
}

TEST_P(QuotaDatabaseTest, GetBucketsForStorageKey) {
  auto db = CreateDatabase(use_in_memory_db());
  EXPECT_TRUE(EnsureOpened(db.get()));

  const StorageKey storage_key1 =
      StorageKey::CreateFromStringForTesting("http://example-a/");
  const StorageKey storage_key2 =
      StorageKey::CreateFromStringForTesting("http://example-b/");

  QuotaErrorOr<BucketInfo> bucket_result =
      db->CreateBucketForTesting(storage_key1, "temp_test1", kTemp);
  ASSERT_TRUE(bucket_result.ok());
  BucketInfo temp_bucket1 = bucket_result.value();

  bucket_result = db->CreateBucketForTesting(storage_key1, "temp_test2", kTemp);
  ASSERT_TRUE(bucket_result.ok());
  BucketInfo temp_bucket2 = bucket_result.value();

  bucket_result =
      db->CreateBucketForTesting(storage_key1, kDefaultBucketName, kSync);
  ASSERT_TRUE(bucket_result.ok());
  BucketInfo sync_bucket1 = bucket_result.value();

  bucket_result =
      db->CreateBucketForTesting(storage_key2, kDefaultBucketName, kSync);
  ASSERT_TRUE(bucket_result.ok());
  BucketInfo sync_bucket2 = bucket_result.value();

  QuotaErrorOr<std::set<BucketInfo>> result =
      db->GetBucketsForStorageKey(storage_key1, kTemp);
  ASSERT_TRUE(result.ok());
  std::set<BucketLocator> buckets = BucketInfosToBucketLocators(result.value());
  ASSERT_EQ(2U, buckets.size());
  EXPECT_TRUE(ContainsBucket(buckets, temp_bucket1));
  EXPECT_TRUE(ContainsBucket(buckets, temp_bucket2));

  result = db->GetBucketsForStorageKey(storage_key2, kSync);
  ASSERT_TRUE(result.ok());
  buckets = BucketInfosToBucketLocators(result.value());
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(ContainsBucket(buckets, sync_bucket2));
}

TEST_P(QuotaDatabaseTest, BucketLastAccessTimeLRU) {
  auto db = CreateDatabase(use_in_memory_db());
  EXPECT_TRUE(EnsureOpened(db.get()));

  std::set<BucketId> bucket_exceptions;
  QuotaErrorOr<BucketLocator> result =
      db->GetLruEvictableBucket(kTemp, bucket_exceptions, nullptr);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.error(), QuotaError::kNotFound);

  // Insert bucket entries into BucketTable.
  base::Time now = base::Time::Now();
  using Entry = mojom::BucketTableEntryPtr;
  StorageKey storage_key1 =
      StorageKey::CreateFromStringForTesting("http://example-a/");
  StorageKey storage_key2 =
      StorageKey::CreateFromStringForTesting("http://example-b/");
  StorageKey storage_key3 =
      StorageKey::CreateFromStringForTesting("http://example-c/");
  StorageKey storage_key4 =
      StorageKey::CreateFromStringForTesting("http://example-d/");

  BucketId bucket_id1 = BucketId(1);
  BucketId bucket_id2 = BucketId(2);
  BucketId bucket_id3 = BucketId(3);
  BucketId bucket_id4 = BucketId(4);

  Entry bucket1 = mojom::BucketTableEntry::New(
      bucket_id1.value(), storage_key1.Serialize(), kStorageTemp,
      kDefaultBucketName, -1, 99, now, now);
  Entry bucket2 = mojom::BucketTableEntry::New(
      bucket_id2.value(), storage_key2.Serialize(), kStorageTemp,
      kDefaultBucketName, -1, 0, now, now);
  Entry bucket3 =
      mojom::BucketTableEntry::New(bucket_id3.value(), storage_key3.Serialize(),
                                   kStorageTemp, "bucket_c", -1, 1, now, now);
  Entry bucket4 =
      mojom::BucketTableEntry::New(bucket_id4.value(), storage_key4.Serialize(),
                                   kStorageSync, "bucket_d", -1, 5, now, now);
  Entry kTableEntries[] = {bucket1->Clone(), bucket2->Clone(), bucket3->Clone(),
                           bucket4->Clone()};
  AssignBucketTable(db.get(), kTableEntries);

  // Update access time for three temporary storages, and
  EXPECT_EQ(
      db->SetBucketLastAccessTime(bucket_id1, base::Time::FromJavaTime(10)),
      QuotaError::kNone);
  EXPECT_EQ(
      db->SetBucketLastAccessTime(bucket_id2, base::Time::FromJavaTime(20)),
      QuotaError::kNone);
  EXPECT_EQ(
      db->SetBucketLastAccessTime(bucket_id3, base::Time::FromJavaTime(30)),
      QuotaError::kNone);

  // One persistent.
  EXPECT_EQ(
      db->SetBucketLastAccessTime(bucket_id4, base::Time::FromJavaTime(40)),
      QuotaError::kNone);

  // One non-existent.
  EXPECT_EQ(
      db->SetBucketLastAccessTime(BucketId(777), base::Time::FromJavaTime(40)),
      QuotaError::kNone);

  result = db->GetLruEvictableBucket(kTemp, bucket_exceptions, nullptr);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(bucket_id1, result.value().id);

  // Test that unlimited origins are excluded from eviction, but
  // protected origins are not excluded.
  auto policy = base::MakeRefCounted<MockSpecialStoragePolicy>();
  policy->AddUnlimited(storage_key1.origin().GetURL());
  policy->AddProtected(storage_key2.origin().GetURL());
  result = db->GetLruEvictableBucket(kTemp, bucket_exceptions, policy.get());
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(bucket_id2, result.value().id);

  // Test that durable origins are excluded from eviction.
  policy->AddDurable(storage_key2.origin().GetURL());
  result = db->GetLruEvictableBucket(kTemp, bucket_exceptions, policy.get());
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(bucket_id3, result.value().id);

  // Bucket exceptions exclude specified buckets.
  bucket_exceptions.insert(bucket_id1);
  result = db->GetLruEvictableBucket(kTemp, bucket_exceptions, nullptr);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(bucket_id2, result.value().id);

  bucket_exceptions.insert(bucket_id2);
  result = db->GetLruEvictableBucket(kTemp, bucket_exceptions, nullptr);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(bucket_id3, result.value().id);

  bucket_exceptions.insert(bucket_id3);
  result = db->GetLruEvictableBucket(kTemp, bucket_exceptions, nullptr);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.error(), QuotaError::kNotFound);

  EXPECT_EQ(db->SetBucketLastAccessTime(bucket_id1, base::Time::Now()),
            QuotaError::kNone);

  BucketLocator bucket_locator =
      BucketLocator(bucket_id3, storage_key3,
                    static_cast<blink::mojom::StorageType>(bucket3->type),
                    bucket3->name == kDefaultBucketName);

  // Delete storage_key/type last access time information.
  auto deleted = db->DeleteBucketData(bucket_locator);
  ASSERT_TRUE(deleted.ok());
  EXPECT_EQ(bucket_id3, BucketId::FromUnsafeValue(deleted.value()->bucket_id));

  // Querying again to see if the deletion has worked.
  bucket_exceptions.clear();
  result = db->GetLruEvictableBucket(kTemp, bucket_exceptions, nullptr);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(bucket_id2, result.value().id);

  bucket_exceptions.insert(bucket_id1);
  bucket_exceptions.insert(bucket_id2);
  result = db->GetLruEvictableBucket(kTemp, bucket_exceptions, nullptr);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.error(), QuotaError::kNotFound);
}

TEST_P(QuotaDatabaseTest, BucketPersistence) {
  auto db = CreateDatabase(use_in_memory_db());
  EXPECT_TRUE(EnsureOpened(db.get()));

  std::set<BucketId> bucket_exceptions;
  QuotaErrorOr<BucketLocator> result =
      db->GetLruEvictableBucket(kTemp, bucket_exceptions, nullptr);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.error(), QuotaError::kNotFound);

  // Insert bucket entries into BucketTable.
  base::Time now = base::Time::Now();
  using Entry = mojom::BucketTableEntryPtr;

  StorageKey storage_key1 =
      StorageKey::CreateFromStringForTesting("http://example-a/");
  StorageKey storage_key2 =
      StorageKey::CreateFromStringForTesting("http://example-b/");

  BucketId bucket_id1 = BucketId(1);
  BucketId bucket_id2 = BucketId(2);

  Entry bucket1 = mojom::BucketTableEntry::New(
      bucket_id1.value(), storage_key1.Serialize(), kStorageTemp,
      kDefaultBucketName, -1, 99, now, now);
  Entry bucket2 = mojom::BucketTableEntry::New(
      bucket_id2.value(), storage_key2.Serialize(), kStorageTemp,
      kDefaultBucketName, -1, 0, now, now);
  Entry kTableEntries[] = {bucket1->Clone(), bucket2->Clone()};
  AssignBucketTable(db.get(), kTableEntries);

  EXPECT_EQ(
      db->SetBucketLastAccessTime(bucket_id1, base::Time::FromJavaTime(10)),
      QuotaError::kNone);
  EXPECT_EQ(
      db->SetBucketLastAccessTime(bucket_id2, base::Time::FromJavaTime(20)),
      QuotaError::kNone);

  result = db->GetLruEvictableBucket(kTemp, bucket_exceptions, nullptr);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(bucket_id1, result.value().id);

  ASSERT_TRUE(db->UpdateBucketPersistence(bucket_id1, true).ok());
  result = db->GetLruEvictableBucket(kTemp, bucket_exceptions, nullptr);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(bucket_id2, result.value().id);
}

TEST_P(QuotaDatabaseTest, SetStorageKeyLastAccessTime) {
  auto db = CreateDatabase(use_in_memory_db());
  EXPECT_TRUE(EnsureOpened(db.get()));

  const StorageKey storage_key =
      StorageKey::CreateFromStringForTesting("http://example/");
  base::Time now = base::Time::Now();

  // Doesn't error if bucket doesn't exist.
  EXPECT_EQ(db->SetStorageKeyLastAccessTime(storage_key, kTemp, now),
            QuotaError::kNone);

  QuotaErrorOr<BucketInfo> bucket =
      db->CreateBucketForTesting(storage_key, kDefaultBucketName, kTemp);

  EXPECT_EQ(db->SetStorageKeyLastAccessTime(storage_key, kTemp, now),
            QuotaError::kNone);

  QuotaErrorOr<mojom::BucketTableEntryPtr> info =
      db->GetBucketInfoForTest(bucket->id);
  EXPECT_TRUE(info.ok());
  EXPECT_EQ(now, info.value()->last_accessed);
  EXPECT_EQ(1, info.value()->use_count);
}

TEST_P(QuotaDatabaseTest, GetStorageKeysForType) {
  auto db = CreateDatabase(use_in_memory_db());
  EXPECT_TRUE(EnsureOpened(db.get()));

  const StorageKey storage_key1 =
      StorageKey::CreateFromStringForTesting("http://example-a/");
  const StorageKey storage_key2 =
      StorageKey::CreateFromStringForTesting("http://example-b/");
  const StorageKey storage_key3 =
      StorageKey::CreateFromStringForTesting("http://example-c/");

  db->CreateBucketForTesting(storage_key1, "bucket_a", kTemp);
  db->CreateBucketForTesting(storage_key2, "bucket_b", kTemp);
  db->CreateBucketForTesting(storage_key2, kDefaultBucketName, kSync);
  db->CreateBucketForTesting(storage_key3, kDefaultBucketName, kSync);

  QuotaErrorOr<std::set<StorageKey>> result = db->GetStorageKeysForType(kTemp);
  ASSERT_TRUE(result.ok());
  ASSERT_TRUE(base::Contains(result.value(), storage_key1));
  ASSERT_TRUE(base::Contains(result.value(), storage_key2));
  ASSERT_FALSE(base::Contains(result.value(), storage_key3));

  result = db->GetStorageKeysForType(kSync);
  ASSERT_TRUE(result.ok());
  ASSERT_FALSE(base::Contains(result.value(), storage_key1));
  ASSERT_TRUE(base::Contains(result.value(), storage_key2));
  ASSERT_TRUE(base::Contains(result.value(), storage_key3));
}

TEST_P(QuotaDatabaseTest, BucketLastModifiedBetween) {
  auto db = CreateDatabase(use_in_memory_db());
  EXPECT_TRUE(EnsureOpened(db.get()));

  QuotaErrorOr<std::set<BucketLocator>> result =
      db->GetBucketsModifiedBetween(kTemp, base::Time(), base::Time::Max());
  EXPECT_TRUE(result.ok());
  std::set<BucketLocator> buckets = result.value();
  EXPECT_TRUE(buckets.empty());

  QuotaErrorOr<BucketInfo> result1 = db->CreateBucketForTesting(
      StorageKey::CreateFromStringForTesting("http://example-a/"), "bucket_a",
      kTemp);
  EXPECT_TRUE(result1.ok());
  BucketInfo bucket1 = result1.value();
  QuotaErrorOr<BucketInfo> result2 = db->CreateBucketForTesting(
      StorageKey::CreateFromStringForTesting("http://example-b/"), "bucket_b",
      kTemp);
  EXPECT_TRUE(result2.ok());
  BucketInfo bucket2 = result2.value();
  QuotaErrorOr<BucketInfo> result3 = db->CreateBucketForTesting(
      StorageKey::CreateFromStringForTesting("http://example-c/"), "bucket_c",
      kTemp);
  EXPECT_TRUE(result3.ok());
  BucketInfo bucket3 = result3.value();
  QuotaErrorOr<BucketInfo> result4 = db->CreateBucketForTesting(
      StorageKey::CreateFromStringForTesting("http://example-d/"),
      kDefaultBucketName, kSync);
  EXPECT_TRUE(result4.ok());
  BucketInfo bucket4 = result4.value();

  // Report last modified time for the buckets.
  EXPECT_EQ(
      db->SetBucketLastModifiedTime(bucket1.id, base::Time::FromJavaTime(0)),
      QuotaError::kNone);
  EXPECT_EQ(
      db->SetBucketLastModifiedTime(bucket2.id, base::Time::FromJavaTime(10)),
      QuotaError::kNone);
  EXPECT_EQ(
      db->SetBucketLastModifiedTime(bucket3.id, base::Time::FromJavaTime(20)),
      QuotaError::kNone);
  EXPECT_EQ(
      db->SetBucketLastModifiedTime(bucket4.id, base::Time::FromJavaTime(30)),
      QuotaError::kNone);

  // Non-existent bucket.
  EXPECT_EQ(
      db->SetBucketLastModifiedTime(BucketId(777), base::Time::FromJavaTime(0)),
      QuotaError::kNone);

  result =
      db->GetBucketsModifiedBetween(kTemp, base::Time(), base::Time::Max());
  EXPECT_TRUE(result.ok());
  buckets = result.value();
  EXPECT_EQ(3U, buckets.size());
  EXPECT_TRUE(ContainsBucket(buckets, bucket1));
  EXPECT_TRUE(ContainsBucket(buckets, bucket2));
  EXPECT_TRUE(ContainsBucket(buckets, bucket3));
  EXPECT_FALSE(ContainsBucket(buckets, bucket4));

  result = db->GetBucketsModifiedBetween(kTemp, base::Time::FromJavaTime(5),
                                         base::Time::Max());
  EXPECT_TRUE(result.ok());
  buckets = result.value();
  EXPECT_EQ(2U, buckets.size());
  EXPECT_FALSE(ContainsBucket(buckets, bucket1));
  EXPECT_TRUE(ContainsBucket(buckets, bucket2));
  EXPECT_TRUE(ContainsBucket(buckets, bucket3));
  EXPECT_FALSE(ContainsBucket(buckets, bucket4));

  result = db->GetBucketsModifiedBetween(kTemp, base::Time::FromJavaTime(15),
                                         base::Time::Max());
  EXPECT_TRUE(result.ok());
  buckets = result.value();
  EXPECT_EQ(1U, buckets.size());
  EXPECT_FALSE(ContainsBucket(buckets, bucket1));
  EXPECT_FALSE(ContainsBucket(buckets, bucket2));
  EXPECT_TRUE(ContainsBucket(buckets, bucket3));
  EXPECT_FALSE(ContainsBucket(buckets, bucket4));

  result = db->GetBucketsModifiedBetween(kTemp, base::Time::FromJavaTime(25),
                                         base::Time::Max());
  EXPECT_TRUE(result.ok());
  buckets = result.value();
  EXPECT_TRUE(buckets.empty());

  result = db->GetBucketsModifiedBetween(kTemp, base::Time::FromJavaTime(5),
                                         base::Time::FromJavaTime(15));
  EXPECT_TRUE(result.ok());
  buckets = result.value();
  EXPECT_EQ(1U, buckets.size());
  EXPECT_FALSE(ContainsBucket(buckets, bucket1));
  EXPECT_TRUE(ContainsBucket(buckets, bucket2));
  EXPECT_FALSE(ContainsBucket(buckets, bucket3));
  EXPECT_FALSE(ContainsBucket(buckets, bucket4));

  result = db->GetBucketsModifiedBetween(kTemp, base::Time::FromJavaTime(0),
                                         base::Time::FromJavaTime(20));
  EXPECT_TRUE(result.ok());
  buckets = result.value();
  EXPECT_EQ(2U, buckets.size());
  EXPECT_TRUE(ContainsBucket(buckets, bucket1));
  EXPECT_TRUE(ContainsBucket(buckets, bucket2));
  EXPECT_FALSE(ContainsBucket(buckets, bucket3));
  EXPECT_FALSE(ContainsBucket(buckets, bucket4));

  result = db->GetBucketsModifiedBetween(kSync, base::Time::FromJavaTime(0),
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
  auto db = CreateDatabase(use_in_memory_db());

  base::flat_map<blink::mojom::StorageType, std::set<StorageKey>>
      storage_keys_by_type;
  const StorageKey kStorageKeys[] = {
      StorageKey::CreateFromStringForTesting("http://a/"),
      StorageKey::CreateFromStringForTesting("http://b/"),
      StorageKey::CreateFromStringForTesting("http://c/")};
  storage_keys_by_type.emplace(
      kTemp, std::set<StorageKey>(kStorageKeys, std::end(kStorageKeys)));
  storage_keys_by_type.emplace(
      kSync, std::set<StorageKey>(kStorageKeys, std::end(kStorageKeys)));

  EXPECT_EQ(db->RegisterInitialStorageKeyInfo(storage_keys_by_type),
            QuotaError::kNone);

  QuotaErrorOr<BucketInfo> bucket_result =
      db->GetBucket(StorageKey::CreateFromStringForTesting("http://a/"),
                    kDefaultBucketName, kTemp);
  ASSERT_TRUE(bucket_result.ok());

  QuotaErrorOr<mojom::BucketTableEntryPtr> info =
      db->GetBucketInfoForTest(bucket_result->id);
  EXPECT_TRUE(info.ok());
  EXPECT_EQ(0, info.value()->use_count);

  EXPECT_EQ(db->SetStorageKeyLastAccessTime(
                StorageKey::CreateFromStringForTesting("http://a/"), kTemp,
                base::Time::FromDoubleT(1.0)),
            QuotaError::kNone);
  info = db->GetBucketInfoForTest(bucket_result->id);
  EXPECT_TRUE(info.ok());
  EXPECT_EQ(1, info.value()->use_count);

  EXPECT_EQ(db->RegisterInitialStorageKeyInfo(storage_keys_by_type),
            QuotaError::kNone);

  info = db->GetBucketInfoForTest(bucket_result->id);
  EXPECT_TRUE(info.ok());
  EXPECT_EQ(1, info.value()->use_count);
}

TEST_P(QuotaDatabaseTest, DumpBucketTable) {
  base::Time now = base::Time::Now();
  using Entry = mojom::BucketTableEntryPtr;

  StorageKey storage_key1 =
      StorageKey::CreateFromStringForTesting("http://go/");
  StorageKey storage_key2 =
      StorageKey::CreateFromStringForTesting("http://oo/");
  StorageKey storage_key3 =
      StorageKey::CreateFromStringForTesting("http://gle/");

  Entry kTableEntries[] = {
      mojom::BucketTableEntry::New(1, storage_key1.Serialize(), kStorageTemp,
                                   kDefaultBucketName, -1, 2147483647, now,
                                   now),
      mojom::BucketTableEntry::New(2, storage_key2.Serialize(), kStorageTemp,
                                   kDefaultBucketName, -1, 0, now, now),
      mojom::BucketTableEntry::New(3, storage_key3.Serialize(), kStorageTemp,
                                   kDefaultBucketName, -1, 1, now, now),
  };

  auto db = CreateDatabase(use_in_memory_db());
  EXPECT_TRUE(EnsureOpened(db.get()));
  AssignBucketTable(db.get(), kTableEntries);

  using Verifier = EntryVerifier<Entry>;
  Verifier verifier(kTableEntries);
  EXPECT_EQ(DumpBucketTable(db.get(),
                            base::BindRepeating(&Verifier::Run,
                                                base::Unretained(&verifier))),
            QuotaError::kNone);
  EXPECT_TRUE(verifier.table.empty());
}

TEST_F(QuotaDatabaseTest, DeleteBucketData) {
  StorageKey storage_key =
      StorageKey::CreateFromStringForTesting("http://google/");
  std::string bucket_name = "inbox";

  // Create db, create a bucket and add bucket data. Close db by leaving scope.
  {
    auto db = CreateDatabase(/*is_incognito=*/false);
    EXPECT_TRUE(EnsureOpened(db.get()));
    QuotaErrorOr<BucketInfo> result =
        db->CreateBucketForTesting(storage_key, bucket_name, kTemp);
    ASSERT_TRUE(result.ok());
    BucketLocator bucket = result->ToBucketLocator();

    const base::FilePath idb_bucket_path = CreateClientBucketPath(
        ProfilePath(), bucket, QuotaClientType::kIndexedDatabase);
    ASSERT_TRUE(base::CreateDirectory(idb_bucket_path));
    ASSERT_TRUE(base::WriteFile(idb_bucket_path.AppendASCII("FakeStorage"),
                                "fake_content"));
  }

  // Reopen db and verify that previously added bucket data is gone on deletion.
  {
    auto db = CreateDatabase(/*is_incognito=*/false);
    EXPECT_TRUE(EnsureOpened(db.get()));
    QuotaErrorOr<BucketInfo> result =
        db->GetBucket(storage_key, bucket_name, kTemp);
    ASSERT_TRUE(result.ok());
    BucketLocator bucket = result->ToBucketLocator();

    const base::FilePath bucket_path = CreateBucketPath(ProfilePath(), bucket);
    ASSERT_TRUE(base::PathExists(bucket_path));

    ASSERT_TRUE(db->DeleteBucketData(bucket).ok());
    ASSERT_FALSE(base::PathExists(bucket_path));
  }
}

// Non-parameterized tests.
TEST_F(QuotaDatabaseTest, BootstrapFlag) {
  auto db = CreateDatabase(/*is_incognito=*/false);

  EXPECT_FALSE(db->IsBootstrapped());
  EXPECT_EQ(db->SetIsBootstrapped(true), QuotaError::kNone);
  EXPECT_TRUE(db->IsBootstrapped());
  EXPECT_EQ(db->SetIsBootstrapped(false), QuotaError::kNone);
  EXPECT_FALSE(db->IsBootstrapped());
}

TEST_F(QuotaDatabaseTest, OpenCorruptedDatabase) {
  base::HistogramTester histograms;
  // Create database, force corruption and close db by leaving scope.
  {
    auto db = CreateDatabase(/*is_incognito=*/false);
    ASSERT_TRUE(EnsureOpened(db.get()));
    ASSERT_TRUE(sql::test::CorruptSizeInHeader(DbPath()));

    // Add fake data into storage directory.
    base::FilePath storage_path = db->GetStoragePath();
    ASSERT_TRUE(base::CreateDirectory(storage_path));
    ASSERT_TRUE(base::WriteFile(storage_path.AppendASCII("FakeStorage"),
                                "dummy_content"));
  }
  // Reopen database and verify schema reset on reopen.
  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(SQLITE_CORRUPT);
    auto db = CreateDatabase(/*is_incognito=*/false);
    ASSERT_TRUE(EnsureOpened(db.get()));
    EXPECT_TRUE(expecter.SawExpectedErrors());

    // Ensure data is deleted.
    base::FilePath storage_path = db->GetStoragePath();
    EXPECT_FALSE(base::IsDirectoryEmpty(storage_path));
  }

  histograms.ExpectTotalCount("Quota.QuotaDatabaseReset", 1);
  histograms.ExpectBucketCount("Quota.QuotaDatabaseReset",
                               DatabaseResetReason::kCreateSchema, 1);

  EXPECT_GE(histograms.GetTotalSum("Quota.QuotaDatabaseError"), 1);
  EXPECT_GE(histograms.GetBucketCount("Quota.QuotaDatabaseError",
                                      sql::SqliteLoggedResultCode::kCorrupt),
            1);
}

TEST_F(QuotaDatabaseTest, QuotaDatabasePathMigration) {
  const base::FilePath kLegacyFilePath =
      ProfilePath().AppendASCII(kDatabaseName);
  BucketInitParams params(
      StorageKey::CreateFromStringForTesting("http://google/"),
      "google_bucket");
  // Create database, add bucket and close by leaving scope.
  {
    auto db = CreateDatabase(/*is_incognito=*/false);
    auto result = db->UpdateOrCreateBucket(params, 0);
    ASSERT_TRUE(result.ok());
  }
  // Move db file paths to legacy file path for path migration test setup.
  {
    base::Move(DbPath(), kLegacyFilePath);
    base::Move(sql::Database::JournalPath(DbPath()),
               sql::Database::JournalPath(kLegacyFilePath));
  }
  // Reopen database, check that db is migrated to new path with bucket data.
  {
    auto db = CreateDatabase(/*is_incognito=*/false);
    auto result = db->GetBucket(params.storage_key, params.name, kTemp);
    EXPECT_TRUE(result.ok());
    EXPECT_FALSE(base::PathExists(kLegacyFilePath));
    EXPECT_TRUE(base::PathExists(DbPath()));
  }
}

// Test for crbug.com/1316581.
TEST_F(QuotaDatabaseTest, QuotaDatabasePathBadMigration) {
  const base::FilePath kLegacyFilePath =
      ProfilePath().AppendASCII(kDatabaseName);
  BucketInitParams params(
      StorageKey::CreateFromStringForTesting("http://google/"),
      "google_bucket");
  // Create database, add bucket and close by leaving scope.
  {
    auto db = CreateDatabase(/*is_incognito=*/false);
    auto result = db->UpdateOrCreateBucket(params, 0);
    ASSERT_TRUE(result.ok());
  }
  // Copy db file paths to legacy file path to mimic bad migration state.
  base::CopyFile(DbPath(), kLegacyFilePath);

  // Reopen database, check that db is migrated and is in a good state.
  {
    auto db = CreateDatabase(/*is_incognito=*/false);
    auto result = db->GetBucket(params.storage_key, params.name, kTemp);
    EXPECT_TRUE(result.ok());
    EXPECT_TRUE(base::PathExists(DbPath()));
  }
}

// Test for crbug.com/1322375.
//
// base::CreateDirectory behaves differently on Mac and allows directory
// migration to succeed when we expect failure.
#if !BUILDFLAG(IS_APPLE)
TEST_F(QuotaDatabaseTest, QuotaDatabaseDirectoryMigrationError) {
  const base::FilePath kLegacyFilePath =
      ProfilePath().AppendASCII(kDatabaseName);
  BucketInitParams google_params = BucketInitParams::ForDefaultBucket(
      StorageKey::CreateFromStringForTesting("http://google/"));
  BucketInitParams example_params = BucketInitParams::ForDefaultBucket(
      StorageKey::CreateFromStringForTesting("http://example/"));
  BucketId example_id;
  // Create database, add bucket and close by leaving scope.
  {
    auto db = CreateDatabase(/*is_incognito=*/false);
    // Create two buckets to check that ids are different after database reset.
    auto result = db->UpdateOrCreateBucket(google_params, 0);
    ASSERT_TRUE(result.ok());
    result = db->UpdateOrCreateBucket(example_params, 0);
    ASSERT_TRUE(result.ok());
    example_id = result->id;
  }
  {
    // Delete database files to force a bad migration state.
    base::DeleteFile(DbPath());
    base::DeleteFile(sql::Database::JournalPath(DbPath()));

    // Create a directory with the database file path to force directory
    // migration to fail.
    base::CreateDirectory(kLegacyFilePath);
  }
  {
    // Open database to trigger migration. Migration failure forces a database
    // reset.
    auto db = CreateDatabase(/*is_incognito=*/false);
    auto result = db->UpdateOrCreateBucket(example_params, 0);
    ASSERT_TRUE(result.ok());
    // Validate database reset by checking that bucket id doesn't match.
    EXPECT_NE(result->id, example_id);
  }
}
#endif  // !BUILDFLAG(IS_APPLE)

TEST_F(QuotaDatabaseTest, UpdateOrCreateBucket_CorruptedDatabase) {
  QuotaDatabase db(ProfilePath());
  BucketInitParams params(
      StorageKey::CreateFromStringForTesting("http://google/"),
      "google_bucket");

  {
    QuotaErrorOr<BucketInfo> result = db.UpdateOrCreateBucket(params, 0);
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

    QuotaErrorOr<BucketInfo> result = db.UpdateOrCreateBucket(params, 0);
    EXPECT_FALSE(result.ok());

    histograms.ExpectTotalCount("Quota.QuotaDatabaseError", 1);
    histograms.ExpectBucketCount("Quota.QuotaDatabaseError",
                                 sql::SqliteLoggedResultCode::kCorrupt, 1);
  }
}

TEST_P(QuotaDatabaseTest, Expiration) {
  QuotaDatabase db(ProfilePath());

  // Default `expiration` value.
  BucketInitParams params(
      StorageKey::CreateFromStringForTesting("http://google/"),
      "google_bucket");
  QuotaErrorOr<BucketInfo> result = db.UpdateOrCreateBucket(params, 0);
  ASSERT_TRUE(result.ok());
  EXPECT_TRUE(result->expiration.is_null());

  QuotaErrorOr<std::set<BucketInfo>> expired_buckets = db.GetExpiredBuckets();
  ASSERT_TRUE(expired_buckets.ok());
  EXPECT_EQ(0U, expired_buckets->size());

  // Non-default `expiration` value.
  BucketInitParams params2(
      StorageKey::CreateFromStringForTesting("http://example/"),
      "example_bucket");
  params2.expiration = base::Time::Now();
  result = db.UpdateOrCreateBucket(params2, 0);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(params2.expiration, result->expiration);

  // Update `expiration` value.
  base::Time updated_time = base::Time::Now() + base::Days(1);
  result = db.UpdateBucketExpiration(result->id, updated_time);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(updated_time, result->expiration);

  expired_buckets = db.GetExpiredBuckets();
  ASSERT_TRUE(expired_buckets.ok());
  EXPECT_EQ(0U, expired_buckets->size());

  // Set expiration to the past.
  updated_time = base::Time::Now() - base::Days(1);
  result = db.UpdateBucketExpiration(result->id, updated_time);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(updated_time, result->expiration);

  expired_buckets = db.GetExpiredBuckets();
  ASSERT_TRUE(expired_buckets.ok());
  EXPECT_EQ(1U, expired_buckets->size());
}

TEST_P(QuotaDatabaseTest, Persistent) {
  QuotaDatabase db(ProfilePath());

  // Default `persistent` value.
  BucketInitParams params(
      StorageKey::CreateFromStringForTesting("http://google/"),
      "google_bucket");
  QuotaErrorOr<BucketInfo> result = db.UpdateOrCreateBucket(params, 0);
  ASSERT_TRUE(result.ok());
  EXPECT_FALSE(params.persistent.has_value());
  EXPECT_FALSE(result->persistent);

  // Non-default `persistent` value.
  BucketInitParams params2(
      StorageKey::CreateFromStringForTesting("http://example/"),
      "example_bucket");
  params2.persistent = !params2.persistent;
  result = db.UpdateOrCreateBucket(params2, 0);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(params2.persistent, result->persistent);

  // Update `persistent` value.
  EXPECT_TRUE(result->persistent);
  result = db.UpdateBucketPersistence(result->id, false);
  ASSERT_TRUE(result.ok());
  EXPECT_FALSE(result->persistent);
}

INSTANTIATE_TEST_SUITE_P(All,
                         QuotaDatabaseTest,
                         /* is_incognito */ testing::Bool());

}  // namespace storage
