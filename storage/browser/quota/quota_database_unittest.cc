// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "storage/browser/quota/quota_database.h"

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
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
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
#include "storage/browser/quota/quota_features.h"
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

static const blink::mojom::StorageType kStorageTemp =
    blink::mojom::StorageType::kTemporary;
static const blink::mojom::StorageType kStorageSync =
    blink::mojom::StorageType::kSyncable;

static constexpr char kDatabaseName[] = "QuotaManager";

bool ContainsBucket(const std::set<BucketLocator>& buckets,
                    const BucketInfo& target_bucket) {
  auto it = buckets.find(target_bucket.ToBucketLocator());
  return it != buckets.end();
}

}  // namespace

// Test parameter indicates if the database should be created for incognito
// mode, if stale buckets should be evicted, and if orphan buckets should be
// evicted.
class QuotaDatabaseTest
    : public testing::TestWithParam<std::tuple<bool, bool, bool>> {
 public:
  QuotaDatabaseTest() {
    clock_ = std::make_unique<base::SimpleTestClock>();
    QuotaDatabase::SetClockForTesting(clock_.get());
    feature_list_.InitWithFeatureStates(
        {{features::kEvictStaleQuotaStorage, evict_stale_buckets()},
         {features::kEvictOrphanQuotaStorage, evict_orphan_buckets()}});
  }

 protected:
  using BucketTableEntry = mojom::BucketTableEntry;

  void SetUp() override {
    clock_->SetNow(base::Time::Now());
    ASSERT_TRUE(temp_directory_.CreateUniqueTempDir());
  }

  void TearDown() override { ASSERT_TRUE(temp_directory_.Delete()); }

  bool use_in_memory_db() const { return std::get<0>(GetParam()); }

  bool evict_stale_buckets() const { return std::get<1>(GetParam()); }

  bool evict_orphan_buckets() const { return std::get<2>(GetParam()); }

  base::SimpleTestClock* clock() { return clock_.get(); }

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

  int GetTransactionNesting(QuotaDatabase* db) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(db->sequence_checker_);
    return db->db_->transaction_nesting();
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
      static constexpr char kSql[] =
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

      std::optional<StorageKey> storage_key =
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
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<base::SimpleTestClock> clock_;
};

TEST_P(QuotaDatabaseTest, EnsureOpened) {
  auto db = CreateDatabase(use_in_memory_db());
  EXPECT_TRUE(EnsureOpened(db.get()));

  if (use_in_memory_db()) {
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

  ASSERT_OK_AND_ASSIGN(BucketInfo created_bucket,
                       db->UpdateOrCreateBucket(params, 0));
  ASSERT_GT(created_bucket.id.value(), 0);
  ASSERT_EQ(created_bucket.name, params.name);
  ASSERT_EQ(created_bucket.storage_key, params.storage_key);
  ASSERT_EQ(created_bucket.type, kTemp);

  // Should return the same bucket when querying again.
  ASSERT_OK_AND_ASSIGN(BucketInfo retrieved_bucket,
                       db->UpdateOrCreateBucket(params, 0));
  ASSERT_EQ(retrieved_bucket.id, created_bucket.id);
  ASSERT_EQ(retrieved_bucket.name, created_bucket.name);
  ASSERT_EQ(retrieved_bucket.storage_key, created_bucket.storage_key);
  ASSERT_EQ(retrieved_bucket.type, created_bucket.type);

  // Test `max_bucket_count`.
  BucketInitParams params2(
      StorageKey::CreateFromStringForTesting("http://google/"),
      "google_bucket2");
  EXPECT_THAT(db->UpdateOrCreateBucket(params2, 1),
              base::test::ErrorIs(QuotaError::kQuotaExceeded));

  // It doesn't affect the update case.
  ASSERT_TRUE(db->UpdateOrCreateBucket(params, 1).has_value());
}

TEST_P(QuotaDatabaseTest, UpdateBucket) {
  auto db = CreateDatabase(use_in_memory_db());
  EXPECT_TRUE(EnsureOpened(db.get()));
  BucketInitParams params(
      StorageKey::CreateFromStringForTesting("http://google/"),
      "google_bucket");
  params.expiration = base::Time::Now() + base::Days(1);
  params.persistent = true;

  ASSERT_OK_AND_ASSIGN(BucketInfo created_bucket,
                       db->UpdateOrCreateBucket(params, 0));

  EXPECT_EQ(params.expiration, created_bucket.expiration);
  EXPECT_TRUE(created_bucket.persistent);

  // Should update the bucket when querying again.
  params.expiration = base::Time::Now() + base::Days(2);
  params.persistent = false;
  params.quota = 1024 * 1024 * 20;  // 20 MB
  params.durability = blink::mojom::BucketDurability::kStrict;
  ASSERT_OK_AND_ASSIGN(BucketInfo updated_bucket,
                       db->UpdateOrCreateBucket(params, 0));

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
  ASSERT_OK_AND_ASSIGN(BucketInfo queried_bucket,
                       db->UpdateOrCreateBucket(params, 0));

  // Expiration and persistence are unchanged.
  EXPECT_EQ(queried_bucket.expiration, updated_bucket.expiration);
  EXPECT_EQ(queried_bucket.persistent, updated_bucket.persistent);
}

TEST_P(QuotaDatabaseTest, GetOrCreateBucketDeprecated) {
  auto db = CreateDatabase(use_in_memory_db());
  EXPECT_TRUE(EnsureOpened(db.get()));
  StorageKey storage_key =
      StorageKey::CreateFromStringForTesting("http://google/");

  ASSERT_OK_AND_ASSIGN(BucketInfo created_bucket,
                       db->GetOrCreateBucketDeprecated(
                           {storage_key, kDefaultBucketName}, kSync));
  ASSERT_GT(created_bucket.id.value(), 0);
  ASSERT_EQ(created_bucket.name, kDefaultBucketName);
  ASSERT_EQ(created_bucket.storage_key, storage_key);
  ASSERT_EQ(created_bucket.type, kSync);

  // Should return the same bucket when querying again.
  ASSERT_OK_AND_ASSIGN(BucketInfo retrieved_bucket,
                       db->GetOrCreateBucketDeprecated(
                           {storage_key, kDefaultBucketName}, kSync));
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
  ASSERT_OK_AND_ASSIGN(
      BucketInfo created_bucket,
      db->CreateBucketForTesting(storage_key, bucket_name, kTemp));
  ASSERT_GT(created_bucket.id.value(), 0);
  ASSERT_EQ(created_bucket.name, bucket_name);
  ASSERT_EQ(created_bucket.storage_key, storage_key);
  ASSERT_EQ(created_bucket.type, kTemp);

  ASSERT_OK_AND_ASSIGN(BucketInfo queried_bucket,
                       db->GetBucket(storage_key, bucket_name, kTemp));
  EXPECT_EQ(queried_bucket.id, created_bucket.id);
  EXPECT_EQ(queried_bucket.name, created_bucket.name);
  EXPECT_EQ(queried_bucket.storage_key, created_bucket.storage_key);
  ASSERT_EQ(queried_bucket.type, created_bucket.type);

  // Can't retrieve buckets with name mismatch.
  EXPECT_THAT(db->GetBucket(storage_key, "does_not_exist", kTemp),
              base::test::ErrorIs(QuotaError::kNotFound));

  // Can't retrieve buckets with StorageKey mismatch.
  EXPECT_THAT(
      db->GetBucket(StorageKey::CreateFromStringForTesting("http://example/"),
                    bucket_name, kTemp),
      base::test::ErrorIs(QuotaError::kNotFound));
}

TEST_P(QuotaDatabaseTest, GetBucketById) {
  auto db = CreateDatabase(use_in_memory_db());
  EXPECT_TRUE(EnsureOpened(db.get()));

  // Add a bucket entry into the bucket table.
  StorageKey storage_key =
      StorageKey::CreateFromStringForTesting("http://google/");
  std::string bucket_name = "google_bucket";
  ASSERT_OK_AND_ASSIGN(
      BucketInfo created_bucket,
      db->CreateBucketForTesting(storage_key, bucket_name, kTemp));
  ASSERT_GT(created_bucket.id.value(), 0);
  ASSERT_EQ(created_bucket.name, bucket_name);
  ASSERT_EQ(created_bucket.storage_key, storage_key);
  ASSERT_EQ(created_bucket.type, kTemp);

  ASSERT_OK_AND_ASSIGN(BucketInfo queried_bucket,
                       db->GetBucketById(created_bucket.id));
  EXPECT_EQ(queried_bucket.name, created_bucket.name);
  EXPECT_EQ(queried_bucket.storage_key, created_bucket.storage_key);
  ASSERT_EQ(queried_bucket.type, created_bucket.type);

  constexpr BucketId kNonExistentBucketId(7777);
  EXPECT_THAT(db->GetBucketById(BucketId(kNonExistentBucketId)),
              base::test::ErrorIs(QuotaError::kNotFound));
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

  ASSERT_OK_AND_ASSIGN(
      BucketInfo temp_bucket1,
      db->CreateBucketForTesting(storage_key1, "temp_bucket", kTemp));

  ASSERT_OK_AND_ASSIGN(
      BucketInfo temp_bucket2,
      db->CreateBucketForTesting(storage_key2, "temp_bucket", kTemp));

  ASSERT_OK_AND_ASSIGN(
      BucketInfo sync_bucket1,
      db->CreateBucketForTesting(storage_key1, kDefaultBucketName, kSync));

  ASSERT_OK_AND_ASSIGN(
      BucketInfo sync_bucket2,
      db->CreateBucketForTesting(storage_key3, kDefaultBucketName, kSync));

  ASSERT_OK_AND_ASSIGN(std::set<BucketInfo> result,
                       db->GetBucketsForType(kTemp));
  std::set<BucketLocator> buckets = BucketInfosToBucketLocators(result);
  ASSERT_EQ(2U, buckets.size());
  EXPECT_TRUE(ContainsBucket(buckets, temp_bucket1));
  EXPECT_TRUE(ContainsBucket(buckets, temp_bucket2));

  ASSERT_OK_AND_ASSIGN(result, db->GetBucketsForType(kSync));
  buckets = BucketInfosToBucketLocators(result);
  ASSERT_EQ(2U, buckets.size());
  EXPECT_TRUE(ContainsBucket(buckets, sync_bucket1));
  EXPECT_TRUE(ContainsBucket(buckets, sync_bucket2));
}

TEST_P(QuotaDatabaseTest, GetBucketsForHost) {
  auto db = CreateDatabase(use_in_memory_db());
  EXPECT_TRUE(EnsureOpened(db.get()));

  ASSERT_OK_AND_ASSIGN(
      BucketInfo temp_example_bucket1,
      db->CreateBucketForTesting(
          StorageKey::CreateFromStringForTesting("https://example.com/"),
          kDefaultBucketName, kTemp));
  ASSERT_OK_AND_ASSIGN(
      BucketInfo temp_example_bucket2,
      db->CreateBucketForTesting(
          StorageKey::CreateFromStringForTesting("http://example.com:123/"),
          kDefaultBucketName, kTemp));
  ASSERT_OK_AND_ASSIGN(
      BucketInfo perm_google_bucket1,
      db->CreateBucketForTesting(
          StorageKey::CreateFromStringForTesting("http://google.com/"),
          kDefaultBucketName, kSync));
  ASSERT_OK_AND_ASSIGN(
      BucketInfo temp_google_bucket2,
      db->CreateBucketForTesting(
          StorageKey::CreateFromStringForTesting("http://google.com:123/"),
          kDefaultBucketName, kTemp));

  ASSERT_OK_AND_ASSIGN(std::set<BucketInfo> result,
                       db->GetBucketsForHost("example.com", kTemp));
  ASSERT_EQ(result.size(), 2U);
  EXPECT_TRUE(base::Contains(result, temp_example_bucket1));
  EXPECT_TRUE(base::Contains(result, temp_example_bucket2));

  ASSERT_OK_AND_ASSIGN(result, db->GetBucketsForHost("example.com", kSync));
  ASSERT_EQ(result.size(), 0U);

  ASSERT_OK_AND_ASSIGN(result, db->GetBucketsForHost("google.com", kSync));
  ASSERT_EQ(result.size(), 1U);
  EXPECT_TRUE(base::Contains(result, perm_google_bucket1));

  ASSERT_OK_AND_ASSIGN(result, db->GetBucketsForHost("google.com", kTemp));
  ASSERT_EQ(result.size(), 1U);
  EXPECT_TRUE(base::Contains(result, temp_google_bucket2));
}

TEST_P(QuotaDatabaseTest, GetBucketsForStorageKey) {
  auto db = CreateDatabase(use_in_memory_db());
  EXPECT_TRUE(EnsureOpened(db.get()));

  const StorageKey storage_key1 =
      StorageKey::CreateFromStringForTesting("http://example-a/");
  const StorageKey storage_key2 =
      StorageKey::CreateFromStringForTesting("http://example-b/");

  ASSERT_OK_AND_ASSIGN(
      BucketInfo temp_bucket1,
      db->CreateBucketForTesting(storage_key1, "temp_test1", kTemp));

  ASSERT_OK_AND_ASSIGN(
      BucketInfo temp_bucket2,
      db->CreateBucketForTesting(storage_key1, "temp_test2", kTemp));

  ASSERT_OK_AND_ASSIGN(
      BucketInfo sync_bucket1,
      db->CreateBucketForTesting(storage_key1, kDefaultBucketName, kSync));

  ASSERT_OK_AND_ASSIGN(
      BucketInfo sync_bucket2,
      db->CreateBucketForTesting(storage_key2, kDefaultBucketName, kSync));

  ASSERT_OK_AND_ASSIGN(std::set<BucketInfo> result,
                       db->GetBucketsForStorageKey(storage_key1, kTemp));
  std::set<BucketLocator> buckets = BucketInfosToBucketLocators(result);
  ASSERT_EQ(2U, buckets.size());
  EXPECT_TRUE(ContainsBucket(buckets, temp_bucket1));
  EXPECT_TRUE(ContainsBucket(buckets, temp_bucket2));

  ASSERT_OK_AND_ASSIGN(result,
                       db->GetBucketsForStorageKey(storage_key2, kSync));
  buckets = BucketInfosToBucketLocators(result);
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(ContainsBucket(buckets, sync_bucket2));
}

TEST_P(QuotaDatabaseTest, BucketLastAccessTimeLRU) {
  auto db = CreateDatabase(use_in_memory_db());
  EXPECT_TRUE(EnsureOpened(db.get()));

  std::set<BucketId> bucket_exceptions;
  EXPECT_THAT(
      db->GetBucketsForEviction(kTemp, 1, {}, bucket_exceptions, nullptr),
      base::test::ErrorIs(QuotaError::kNotFound));

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
  EXPECT_EQ(db->SetBucketLastAccessTime(
                bucket_id1, base::Time::FromMillisecondsSinceUnixEpoch(10)),
            QuotaError::kNone);
  EXPECT_EQ(db->SetBucketLastAccessTime(
                bucket_id2, base::Time::FromMillisecondsSinceUnixEpoch(20)),
            QuotaError::kNone);
  EXPECT_EQ(db->SetBucketLastAccessTime(
                bucket_id3, base::Time::FromMillisecondsSinceUnixEpoch(30)),
            QuotaError::kNone);

  // One persistent.
  EXPECT_EQ(db->SetBucketLastAccessTime(
                bucket_id4, base::Time::FromMillisecondsSinceUnixEpoch(40)),
            QuotaError::kNone);

  // One non-existent.
  EXPECT_EQ(db->SetBucketLastAccessTime(
                BucketId(777), base::Time::FromMillisecondsSinceUnixEpoch(40)),
            QuotaError::kNone);

  ASSERT_OK_AND_ASSIGN(
      std::set<BucketLocator> result,
      db->GetBucketsForEviction(kTemp, 1, {}, bucket_exceptions, nullptr));
  ASSERT_EQ(1U, result.size());
  EXPECT_EQ(bucket_id1, result.begin()->id);

  // Test that unlimited origins are excluded from eviction, but
  // protected origins are not excluded.
  auto policy = base::MakeRefCounted<MockSpecialStoragePolicy>();
  policy->AddUnlimited(storage_key1.origin().GetURL());
  policy->AddProtected(storage_key2.origin().GetURL());
  ASSERT_OK_AND_ASSIGN(
      result,
      db->GetBucketsForEviction(kTemp, 1, {}, bucket_exceptions, policy.get()));
  ASSERT_EQ(1U, result.size());
  EXPECT_EQ(bucket_id2, result.begin()->id);

  // Test that durable origins are excluded from eviction.
  policy->AddDurable(storage_key2.origin().GetURL());
  ASSERT_OK_AND_ASSIGN(
      result,
      db->GetBucketsForEviction(kTemp, 1, {}, bucket_exceptions, policy.get()));
  ASSERT_EQ(1U, result.size());
  EXPECT_EQ(bucket_id3, result.begin()->id);

  // Bucket exceptions exclude specified buckets.
  bucket_exceptions.insert(bucket_id1);
  ASSERT_OK_AND_ASSIGN(result, db->GetBucketsForEviction(
                                   kTemp, 1, {}, bucket_exceptions, nullptr));
  ASSERT_EQ(1U, result.size());
  EXPECT_EQ(bucket_id2, result.begin()->id);

  bucket_exceptions.insert(bucket_id2);
  ASSERT_OK_AND_ASSIGN(result, db->GetBucketsForEviction(
                                   kTemp, 1, {}, bucket_exceptions, nullptr));
  ASSERT_EQ(1U, result.size());
  EXPECT_EQ(bucket_id3, result.begin()->id);

  bucket_exceptions.insert(bucket_id3);
  EXPECT_THAT(
      db->GetBucketsForEviction(kTemp, 1, {}, bucket_exceptions, nullptr),
      base::test::ErrorIs(QuotaError::kNotFound));

  EXPECT_EQ(db->SetBucketLastAccessTime(bucket_id1, base::Time::Now()),
            QuotaError::kNone);

  BucketLocator bucket_locator =
      BucketLocator(bucket_id3, storage_key3,
                    static_cast<blink::mojom::StorageType>(bucket3->type),
                    bucket3->name == kDefaultBucketName);

  // Delete storage_key/type last access time information.
  ASSERT_OK_AND_ASSIGN(auto deleted, db->DeleteBucketData(bucket_locator));
  EXPECT_EQ(bucket_id3, BucketId::FromUnsafeValue(deleted->bucket_id));

  // Querying again to see if the deletion has worked.
  bucket_exceptions.clear();
  ASSERT_OK_AND_ASSIGN(result, db->GetBucketsForEviction(
                                   kTemp, 1, {}, bucket_exceptions, nullptr));
  ASSERT_EQ(1U, result.size());
  EXPECT_EQ(bucket_id2, result.begin()->id);

  bucket_exceptions.insert(bucket_id1);
  bucket_exceptions.insert(bucket_id2);
  EXPECT_THAT(
      db->GetBucketsForEviction(kTemp, 1, {}, bucket_exceptions, nullptr),
      base::test::ErrorIs(QuotaError::kNotFound));
}

TEST_P(QuotaDatabaseTest, BucketPersistence) {
  auto db = CreateDatabase(use_in_memory_db());
  EXPECT_TRUE(EnsureOpened(db.get()));

  std::set<BucketId> bucket_exceptions;
  EXPECT_THAT(
      db->GetBucketsForEviction(kTemp, 1, {}, bucket_exceptions, nullptr),
      base::test::ErrorIs(QuotaError::kNotFound));

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

  EXPECT_EQ(db->SetBucketLastAccessTime(
                bucket_id1, base::Time::FromMillisecondsSinceUnixEpoch(10)),
            QuotaError::kNone);
  EXPECT_EQ(db->SetBucketLastAccessTime(
                bucket_id2, base::Time::FromMillisecondsSinceUnixEpoch(20)),
            QuotaError::kNone);

  ASSERT_OK_AND_ASSIGN(
      std::set<BucketLocator> result,
      db->GetBucketsForEviction(kTemp, 1, {}, bucket_exceptions, nullptr));
  ASSERT_EQ(1U, result.size());
  EXPECT_EQ(bucket_id1, result.begin()->id);

  ASSERT_TRUE(db->UpdateBucketPersistence(bucket_id1, true).has_value());
  ASSERT_OK_AND_ASSIGN(result, db->GetBucketsForEviction(
                                   kTemp, 1, {}, bucket_exceptions, nullptr));
  ASSERT_EQ(1U, result.size());
  EXPECT_EQ(bucket_id2, result.begin()->id);
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

  ASSERT_OK_AND_ASSIGN(
      BucketInfo bucket,
      db->CreateBucketForTesting(storage_key, kDefaultBucketName, kTemp));

  EXPECT_EQ(db->SetStorageKeyLastAccessTime(storage_key, kTemp, now),
            QuotaError::kNone);

  ASSERT_OK_AND_ASSIGN(mojom::BucketTableEntryPtr info,
                       db->GetBucketInfoForTest(bucket.id));
  EXPECT_EQ(now, info->last_accessed);
  EXPECT_EQ(1, info->use_count);
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

  std::ignore = db->CreateBucketForTesting(storage_key1, "bucket_a", kTemp);
  std::ignore = db->CreateBucketForTesting(storage_key2, "bucket_b", kTemp);
  std::ignore =
      db->CreateBucketForTesting(storage_key2, kDefaultBucketName, kSync);
  std::ignore =
      db->CreateBucketForTesting(storage_key3, kDefaultBucketName, kSync);

  ASSERT_OK_AND_ASSIGN(std::set<StorageKey> result,
                       db->GetStorageKeysForType(kTemp));
  ASSERT_TRUE(base::Contains(result, storage_key1));
  ASSERT_TRUE(base::Contains(result, storage_key2));
  ASSERT_FALSE(base::Contains(result, storage_key3));

  ASSERT_OK_AND_ASSIGN(result, db->GetStorageKeysForType(kSync));
  ASSERT_FALSE(base::Contains(result, storage_key1));
  ASSERT_TRUE(base::Contains(result, storage_key2));
  ASSERT_TRUE(base::Contains(result, storage_key3));
}

TEST_P(QuotaDatabaseTest, BucketLastModifiedBetween) {
  auto db = CreateDatabase(use_in_memory_db());
  EXPECT_TRUE(EnsureOpened(db.get()));

  ASSERT_OK_AND_ASSIGN(
      std::set<BucketLocator> buckets,
      db->GetBucketsModifiedBetween(kTemp, base::Time(), base::Time::Max()));
  EXPECT_TRUE(buckets.empty());

  ASSERT_OK_AND_ASSIGN(
      BucketInfo bucket1,
      db->CreateBucketForTesting(
          StorageKey::CreateFromStringForTesting("http://example-a/"),
          "bucket_a", kTemp));
  ASSERT_OK_AND_ASSIGN(
      BucketInfo bucket2,
      db->CreateBucketForTesting(
          StorageKey::CreateFromStringForTesting("http://example-b/"),
          "bucket_b", kTemp));
  ASSERT_OK_AND_ASSIGN(
      BucketInfo bucket3,
      db->CreateBucketForTesting(
          StorageKey::CreateFromStringForTesting("http://example-c/"),
          "bucket_c", kTemp));
  ASSERT_OK_AND_ASSIGN(
      BucketInfo bucket4,
      db->CreateBucketForTesting(
          StorageKey::CreateFromStringForTesting("http://example-d/"),
          kDefaultBucketName, kSync));

  // Report last modified time for the buckets.
  EXPECT_EQ(db->SetBucketLastModifiedTime(
                bucket1.id, base::Time::FromMillisecondsSinceUnixEpoch(0)),
            QuotaError::kNone);
  EXPECT_EQ(db->SetBucketLastModifiedTime(
                bucket2.id, base::Time::FromMillisecondsSinceUnixEpoch(10)),
            QuotaError::kNone);
  EXPECT_EQ(db->SetBucketLastModifiedTime(
                bucket3.id, base::Time::FromMillisecondsSinceUnixEpoch(20)),
            QuotaError::kNone);
  EXPECT_EQ(db->SetBucketLastModifiedTime(
                bucket4.id, base::Time::FromMillisecondsSinceUnixEpoch(30)),
            QuotaError::kNone);

  // Non-existent bucket.
  EXPECT_EQ(db->SetBucketLastModifiedTime(
                BucketId(777), base::Time::FromMillisecondsSinceUnixEpoch(0)),
            QuotaError::kNone);

  ASSERT_OK_AND_ASSIGN(buckets, db->GetBucketsModifiedBetween(
                                    kTemp, base::Time(), base::Time::Max()));
  EXPECT_EQ(3U, buckets.size());
  EXPECT_TRUE(ContainsBucket(buckets, bucket1));
  EXPECT_TRUE(ContainsBucket(buckets, bucket2));
  EXPECT_TRUE(ContainsBucket(buckets, bucket3));
  EXPECT_FALSE(ContainsBucket(buckets, bucket4));

  ASSERT_OK_AND_ASSIGN(buckets,
                       db->GetBucketsModifiedBetween(
                           kTemp, base::Time::FromMillisecondsSinceUnixEpoch(5),
                           base::Time::Max()));
  EXPECT_EQ(2U, buckets.size());
  EXPECT_FALSE(ContainsBucket(buckets, bucket1));
  EXPECT_TRUE(ContainsBucket(buckets, bucket2));
  EXPECT_TRUE(ContainsBucket(buckets, bucket3));
  EXPECT_FALSE(ContainsBucket(buckets, bucket4));

  ASSERT_OK_AND_ASSIGN(
      buckets, db->GetBucketsModifiedBetween(
                   kTemp, base::Time::FromMillisecondsSinceUnixEpoch(15),
                   base::Time::Max()));
  EXPECT_EQ(1U, buckets.size());
  EXPECT_FALSE(ContainsBucket(buckets, bucket1));
  EXPECT_FALSE(ContainsBucket(buckets, bucket2));
  EXPECT_TRUE(ContainsBucket(buckets, bucket3));
  EXPECT_FALSE(ContainsBucket(buckets, bucket4));

  ASSERT_OK_AND_ASSIGN(
      buckets, db->GetBucketsModifiedBetween(
                   kTemp, base::Time::FromMillisecondsSinceUnixEpoch(25),
                   base::Time::Max()));
  EXPECT_TRUE(buckets.empty());

  ASSERT_OK_AND_ASSIGN(buckets,
                       db->GetBucketsModifiedBetween(
                           kTemp, base::Time::FromMillisecondsSinceUnixEpoch(5),
                           base::Time::FromMillisecondsSinceUnixEpoch(15)));
  EXPECT_EQ(1U, buckets.size());
  EXPECT_FALSE(ContainsBucket(buckets, bucket1));
  EXPECT_TRUE(ContainsBucket(buckets, bucket2));
  EXPECT_FALSE(ContainsBucket(buckets, bucket3));
  EXPECT_FALSE(ContainsBucket(buckets, bucket4));

  ASSERT_OK_AND_ASSIGN(buckets,
                       db->GetBucketsModifiedBetween(
                           kTemp, base::Time::FromMillisecondsSinceUnixEpoch(0),
                           base::Time::FromMillisecondsSinceUnixEpoch(20)));
  EXPECT_EQ(2U, buckets.size());
  EXPECT_TRUE(ContainsBucket(buckets, bucket1));
  EXPECT_TRUE(ContainsBucket(buckets, bucket2));
  EXPECT_FALSE(ContainsBucket(buckets, bucket3));
  EXPECT_FALSE(ContainsBucket(buckets, bucket4));

  ASSERT_OK_AND_ASSIGN(buckets,
                       db->GetBucketsModifiedBetween(
                           kSync, base::Time::FromMillisecondsSinceUnixEpoch(0),
                           base::Time::FromMillisecondsSinceUnixEpoch(35)));
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

  ASSERT_OK_AND_ASSIGN(
      BucketInfo bucket_result,
      db->GetBucket(StorageKey::CreateFromStringForTesting("http://a/"),
                    kDefaultBucketName, kTemp));

  ASSERT_OK_AND_ASSIGN(mojom::BucketTableEntryPtr info,
                       db->GetBucketInfoForTest(bucket_result.id));
  EXPECT_EQ(0, info->use_count);

  EXPECT_EQ(db->SetStorageKeyLastAccessTime(
                StorageKey::CreateFromStringForTesting("http://a/"), kTemp,
                base::Time::FromSecondsSinceUnixEpoch(1.0)),
            QuotaError::kNone);
  ASSERT_OK_AND_ASSIGN(info, db->GetBucketInfoForTest(bucket_result.id));
  EXPECT_EQ(1, info->use_count);

  EXPECT_EQ(db->RegisterInitialStorageKeyInfo(storage_keys_by_type),
            QuotaError::kNone);

  ASSERT_OK_AND_ASSIGN(info, db->GetBucketInfoForTest(bucket_result.id));
  EXPECT_EQ(1, info->use_count);
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

TEST_P(QuotaDatabaseTest, DeleteBucketData) {
  StorageKey storage_key =
      StorageKey::CreateFromStringForTesting("http://google/");
  std::string bucket_name = "inbox";

  // Create db, create a bucket and add bucket data. Close db by leaving scope.
  {
    auto db = CreateDatabase(/*is_incognito=*/false);
    EXPECT_TRUE(EnsureOpened(db.get()));
    ASSERT_OK_AND_ASSIGN(
        BucketInfo result,
        db->CreateBucketForTesting(storage_key, bucket_name, kTemp));
    BucketLocator bucket = result.ToBucketLocator();

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
    ASSERT_OK_AND_ASSIGN(BucketInfo result,
                         db->GetBucket(storage_key, bucket_name, kTemp));
    BucketLocator bucket = result.ToBucketLocator();

    const base::FilePath bucket_path = CreateBucketPath(ProfilePath(), bucket);
    ASSERT_TRUE(base::PathExists(bucket_path));

    ASSERT_TRUE(db->DeleteBucketData(bucket).has_value());
    ASSERT_FALSE(base::PathExists(bucket_path));
  }
}

// Non-parameterized tests.
TEST_P(QuotaDatabaseTest, BootstrapFlag) {
  auto db = CreateDatabase(/*is_incognito=*/false);

  EXPECT_FALSE(db->IsBootstrapped());
  EXPECT_EQ(db->SetIsBootstrapped(true), QuotaError::kNone);
  EXPECT_TRUE(db->IsBootstrapped());
  EXPECT_EQ(db->SetIsBootstrapped(false), QuotaError::kNone);
  EXPECT_FALSE(db->IsBootstrapped());
}

TEST_P(QuotaDatabaseTest, OpenCorruptedDatabase) {
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
    histograms.ExpectBucketCount("Quota.DatabaseSpecificError.Open",
                                 sql::SqliteLoggedResultCode::kCorrupt, 1);
    EXPECT_TRUE(expecter.SawExpectedErrors());

    // Ensure no nested transactions after reentrant calls to EnsureOpened()
    EXPECT_EQ(GetTransactionNesting(db.get()), 1);

    // Ensure data is deleted.
    base::FilePath storage_path = db->GetStoragePath();
    EXPECT_FALSE(base::IsDirectoryEmpty(storage_path));
  }

  histograms.ExpectTotalCount("Quota.QuotaDatabaseReset", 1);
  histograms.ExpectBucketCount("Quota.QuotaDatabaseReset",
                               DatabaseResetReason::kOpenDatabase, 1);
}

TEST_P(QuotaDatabaseTest, QuotaDatabasePathMigration) {
  const base::FilePath kLegacyFilePath =
      ProfilePath().AppendASCII(kDatabaseName);
  BucketInitParams params(
      StorageKey::CreateFromStringForTesting("http://google/"),
      "google_bucket");
  // Create database, add bucket and close by leaving scope.
  {
    auto db = CreateDatabase(/*is_incognito=*/false);
    ASSERT_TRUE(db->UpdateOrCreateBucket(params, 0).has_value());
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
    EXPECT_TRUE(
        db->GetBucket(params.storage_key, params.name, kTemp).has_value());
    EXPECT_FALSE(base::PathExists(kLegacyFilePath));
    EXPECT_TRUE(base::PathExists(DbPath()));
  }
}

// Test for crbug.com/1316581.
TEST_P(QuotaDatabaseTest, QuotaDatabasePathBadMigration) {
  const base::FilePath kLegacyFilePath =
      ProfilePath().AppendASCII(kDatabaseName);
  BucketInitParams params(
      StorageKey::CreateFromStringForTesting("http://google/"),
      "google_bucket");
  // Create database, add bucket and close by leaving scope.
  {
    auto db = CreateDatabase(/*is_incognito=*/false);
    ASSERT_TRUE(db->UpdateOrCreateBucket(params, 0).has_value());
  }
  // Copy db file paths to legacy file path to mimic bad migration state.
  base::CopyFile(DbPath(), kLegacyFilePath);

  // Reopen database, check that db is migrated and is in a good state.
  {
    auto db = CreateDatabase(/*is_incognito=*/false);
    EXPECT_TRUE(
        db->GetBucket(params.storage_key, params.name, kTemp).has_value());
    EXPECT_TRUE(base::PathExists(DbPath()));
  }
}

// Test for crbug.com/1322375.
//
// base::CreateDirectory behaves differently on Mac and allows directory
// migration to succeed when we expect failure.
#if !BUILDFLAG(IS_APPLE)
TEST_P(QuotaDatabaseTest, QuotaDatabaseDirectoryMigrationError) {
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
    ASSERT_TRUE(db->UpdateOrCreateBucket(google_params, 0).has_value());
    ASSERT_OK_AND_ASSIGN(auto result,
                         db->UpdateOrCreateBucket(example_params, 0));
    example_id = result.id;
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
    ASSERT_OK_AND_ASSIGN(auto result,
                         db->UpdateOrCreateBucket(example_params, 0));
    // Validate database reset by checking that bucket id doesn't match.
    EXPECT_NE(result.id, example_id);
  }
}
#endif  // !BUILDFLAG(IS_APPLE)

TEST_P(QuotaDatabaseTest, UpdateOrCreateBucket_CorruptedDatabase) {
  base::HistogramTester histograms;
  QuotaDatabase db(ProfilePath());
  BucketInitParams params(
      StorageKey::CreateFromStringForTesting("http://google/"),
      "google_bucket");
  int sqlite_error_code = 0;
  db.SetDbErrorCallback(base::BindRepeating(
      [](int* error_code_out, int error_code) { *error_code_out = error_code; },
      &sqlite_error_code));

  {
    ASSERT_TRUE(db.UpdateOrCreateBucket(params, 0).has_value())
        << "Failed to create bucket to be used in test";
    EXPECT_EQ(sqlite_error_code, static_cast<int>(sql::SqliteResultCode::kOk));
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
    EXPECT_FALSE(db.UpdateOrCreateBucket(params, 0).has_value());

    EXPECT_EQ(sqlite_error_code,
              static_cast<int>(sql::SqliteResultCode::kCorrupt));
  }
  histograms.ExpectTotalCount("Quota.DatabaseSpecificError.GetBucket", 1);
}

TEST_P(QuotaDatabaseTest, Expiration) {
  QuotaDatabase db(ProfilePath());

  // Default `expiration` value.
  BucketInitParams params(
      StorageKey::CreateFromStringForTesting("http://google/"),
      "google_bucket");
  ASSERT_OK_AND_ASSIGN(BucketInfo result, db.UpdateOrCreateBucket(params, 0));
  EXPECT_TRUE(result.expiration.is_null());

  ASSERT_OK_AND_ASSIGN(std::set<BucketInfo> expired_buckets,
                       db.GetExpiredBuckets(nullptr));
  EXPECT_EQ(0U, expired_buckets.size());

  // Non-default `expiration` value.
  BucketInitParams params2(
      StorageKey::CreateFromStringForTesting("http://example/"),
      "example_bucket");
  params2.expiration = base::Time::Now();
  ASSERT_OK_AND_ASSIGN(result, db.UpdateOrCreateBucket(params2, 0));
  EXPECT_EQ(params2.expiration, result.expiration);

  // Update `expiration` value.
  base::Time updated_time = base::Time::Now() + base::Days(1);
  ASSERT_OK_AND_ASSIGN(result,
                       db.UpdateBucketExpiration(result.id, updated_time));
  EXPECT_EQ(updated_time, result.expiration);

  ASSERT_OK_AND_ASSIGN(expired_buckets, db.GetExpiredBuckets(nullptr));
  EXPECT_EQ(0U, expired_buckets.size());

  // Set expiration to the past.
  updated_time = base::Time::Now() - base::Days(1);
  ASSERT_OK_AND_ASSIGN(result,
                       db.UpdateBucketExpiration(result.id, updated_time));
  EXPECT_EQ(updated_time, result.expiration);

  ASSERT_OK_AND_ASSIGN(expired_buckets, db.GetExpiredBuckets(nullptr));
  EXPECT_EQ(1U, expired_buckets.size());
}

TEST_P(QuotaDatabaseTest, Stale) {
  // Setup database with a few buckets.
  QuotaDatabase db(ProfilePath());
  BucketInitParams named_params(
      StorageKey::CreateFromStringForTesting("http://google/"),
      "google_bucket");
  ASSERT_OK_AND_ASSIGN(BucketInfo named_bucket,
                       db.UpdateOrCreateBucket(named_params, 0));
  BucketInitParams default_params(
      StorageKey::CreateFromStringForTesting("http://notgoogle/"),
      kDefaultBucketName);
  ASSERT_OK_AND_ASSIGN(BucketInfo default_bucket,
                       db.UpdateOrCreateBucket(default_params, 0));
  BucketInitParams persistent_params(
      StorageKey::CreateFromStringForTesting("http://alsonotgoogle/"),
      kDefaultBucketName);
  persistent_params.persistent = true;
  ASSERT_OK_AND_ASSIGN(BucketInfo persistent_bucket,
                       db.UpdateOrCreateBucket(persistent_params, 0));
  BucketInitParams expired_params(
      StorageKey::CreateFromStringForTesting("http://expired/"),
      "expired_bucket");
  expired_params.expiration = base::Time::Now() + base::Days(1);
  ASSERT_OK_AND_ASSIGN(BucketInfo expired_bucket,
                       db.UpdateOrCreateBucket(expired_params, 0));

  // Current accessed/modified time isn't stale.
  db.SetAlreadyEvictedStaleStorageForTesting(false);
  ASSERT_OK_AND_ASSIGN(std::set<BucketInfo> stale_buckets,
                       db.GetExpiredBuckets(nullptr));
  EXPECT_EQ(0U, stale_buckets.size());

  // Force expire bucket, this should always be returned.
  ASSERT_OK_AND_ASSIGN(expired_bucket, db.UpdateBucketExpiration(
                                           expired_bucket.id,
                                           base::Time::Now() - base::Days(1)));
  db.SetAlreadyEvictedStaleStorageForTesting(false);
  ASSERT_OK_AND_ASSIGN(stale_buckets, db.GetExpiredBuckets(nullptr));
  EXPECT_EQ(1U, stale_buckets.size());

  // Old accessed with current modified time isn't stale.
  EXPECT_EQ(db.SetBucketLastAccessTime(named_bucket.id,
                                       base::Time::Now() - base::Days(401)),
            QuotaError::kNone);
  db.SetAlreadyEvictedStaleStorageForTesting(false);
  ASSERT_OK_AND_ASSIGN(stale_buckets, db.GetExpiredBuckets(nullptr));
  EXPECT_EQ(1U, stale_buckets.size());

  // Current accessed with old modified time isn't stale.
  EXPECT_EQ(db.SetBucketLastAccessTime(named_bucket.id, base::Time::Now()),
            QuotaError::kNone);
  EXPECT_EQ(db.SetBucketLastModifiedTime(named_bucket.id,
                                         base::Time::Now() - base::Days(401)),
            QuotaError::kNone);
  db.SetAlreadyEvictedStaleStorageForTesting(false);
  ASSERT_OK_AND_ASSIGN(stale_buckets, db.GetExpiredBuckets(nullptr));
  EXPECT_EQ(1U, stale_buckets.size());

  // Old accessed/modified time is stale, but we need to wait a minute.
  EXPECT_EQ(db.SetBucketLastAccessTime(named_bucket.id,
                                       base::Time::Now() - base::Days(401)),
            QuotaError::kNone);
  db.SetAlreadyEvictedStaleStorageForTesting(false);
  ASSERT_OK_AND_ASSIGN(stale_buckets, db.GetExpiredBuckets(nullptr));
  EXPECT_EQ(1U, stale_buckets.size());

  // If we wait a minute after initialization then it's returned as stale as
  // long as it's our first check.
  clock()->SetNow(base::Time::Now() + base::Minutes(1));
  ASSERT_OK_AND_ASSIGN(stale_buckets, db.GetExpiredBuckets(nullptr));
  EXPECT_EQ(evict_stale_buckets() ? 2U : 1U, stale_buckets.size());
  ASSERT_OK_AND_ASSIGN(stale_buckets, db.GetExpiredBuckets(nullptr));
  EXPECT_EQ(1u, stale_buckets.size());
  db.SetAlreadyEvictedStaleStorageForTesting(false);
  ASSERT_OK_AND_ASSIGN(stale_buckets, db.GetExpiredBuckets(nullptr));
  EXPECT_EQ(evict_stale_buckets() ? 2U : 1U, stale_buckets.size());

  // 399 days ago isn't enough to be stale.
  EXPECT_EQ(db.SetBucketLastAccessTime(named_bucket.id,
                                       base::Time::Now() - base::Days(399)),
            QuotaError::kNone);
  EXPECT_EQ(db.SetBucketLastModifiedTime(named_bucket.id,
                                         base::Time::Now() - base::Days(399)),
            QuotaError::kNone);
  db.SetAlreadyEvictedStaleStorageForTesting(false);
  ASSERT_OK_AND_ASSIGN(stale_buckets, db.GetExpiredBuckets(nullptr));
  EXPECT_EQ(1U, stale_buckets.size());

  // But if we wait a day then it is enough at 400.
  clock()->SetNow(base::Time::Now() + base::Days(1));
  db.SetAlreadyEvictedStaleStorageForTesting(false);
  ASSERT_OK_AND_ASSIGN(stale_buckets, db.GetExpiredBuckets(nullptr));
  EXPECT_EQ(evict_stale_buckets() ? 2U : 1U, stale_buckets.size());

  // A default bucket can be stale.
  EXPECT_EQ(db.SetBucketLastAccessTime(default_bucket.id,
                                       base::Time::Now() - base::Days(401)),
            QuotaError::kNone);
  EXPECT_EQ(db.SetBucketLastModifiedTime(default_bucket.id,
                                         base::Time::Now() - base::Days(401)),
            QuotaError::kNone);
  db.SetAlreadyEvictedStaleStorageForTesting(false);
  ASSERT_OK_AND_ASSIGN(stale_buckets, db.GetExpiredBuckets(nullptr));
  EXPECT_EQ(evict_stale_buckets() ? 3U : 1U, stale_buckets.size());

  // A persistent bucket cannot be stale.
  EXPECT_EQ(db.SetBucketLastAccessTime(persistent_bucket.id,
                                       base::Time::Now() - base::Days(401)),
            QuotaError::kNone);
  EXPECT_EQ(db.SetBucketLastModifiedTime(persistent_bucket.id,
                                         base::Time::Now() - base::Days(401)),
            QuotaError::kNone);
  db.SetAlreadyEvictedStaleStorageForTesting(false);
  ASSERT_OK_AND_ASSIGN(stale_buckets, db.GetExpiredBuckets(nullptr));
  EXPECT_EQ(evict_stale_buckets() ? 3U : 1U, stale_buckets.size());

  // Special storage policies are respected for default buckets.
  auto policy = base::MakeRefCounted<MockSpecialStoragePolicy>();
  policy->AddUnlimited(default_bucket.storage_key.origin().GetURL());
  db.SetAlreadyEvictedStaleStorageForTesting(false);
  ASSERT_OK_AND_ASSIGN(stale_buckets, db.GetExpiredBuckets(policy.get()));
  EXPECT_EQ(evict_stale_buckets() ? 2U : 1U, stale_buckets.size());
  policy = base::MakeRefCounted<MockSpecialStoragePolicy>();
  policy->AddDurable(default_bucket.storage_key.origin().GetURL());
  db.SetAlreadyEvictedStaleStorageForTesting(false);
  ASSERT_OK_AND_ASSIGN(stale_buckets, db.GetExpiredBuckets(policy.get()));
  EXPECT_EQ(evict_stale_buckets() ? 2U : 1U, stale_buckets.size());

  // Special storage policies are not respected for named buckets.
  policy = base::MakeRefCounted<MockSpecialStoragePolicy>();
  policy->AddUnlimited(named_bucket.storage_key.origin().GetURL());
  db.SetAlreadyEvictedStaleStorageForTesting(false);
  ASSERT_OK_AND_ASSIGN(stale_buckets, db.GetExpiredBuckets(policy.get()));
  EXPECT_EQ(evict_stale_buckets() ? 3U : 1U, stale_buckets.size());
  policy = base::MakeRefCounted<MockSpecialStoragePolicy>();
  policy->AddDurable(named_bucket.storage_key.origin().GetURL());
  db.SetAlreadyEvictedStaleStorageForTesting(false);
  ASSERT_OK_AND_ASSIGN(stale_buckets, db.GetExpiredBuckets(policy.get()));
  EXPECT_EQ(evict_stale_buckets() ? 3U : 1U, stale_buckets.size());
}

TEST_P(QuotaDatabaseTest, Orphan) {
  // Setup database and check no orphaned buckets counted.
  QuotaDatabase db(ProfilePath());
  clock()->SetNow(base::Time::Now() + base::Minutes(1));
  {
    base::HistogramTester histograms;
    db.SetAlreadyEvictedStaleStorageForTesting(false);
    ASSERT_OK_AND_ASSIGN(std::set<BucketInfo> buckets,
                         db.GetExpiredBuckets(nullptr));
    EXPECT_EQ(0U, buckets.size());
    EXPECT_EQ(0, histograms.GetTotalSum("Quota.OrphanBucketCount"));
  }

  // First party bucket doesn't qualify, even if it's old.
  BucketInitParams first_party_params(
      StorageKey::CreateFromStringForTesting("http://firstparty/"),
      kDefaultBucketName);
  ASSERT_OK_AND_ASSIGN(BucketInfo first_party_bucket,
                       db.UpdateOrCreateBucket(first_party_params, 0));
  {
    base::HistogramTester histograms;
    db.SetAlreadyEvictedStaleStorageForTesting(false);
    ASSERT_OK_AND_ASSIGN(std::set<BucketInfo> buckets,
                         db.GetExpiredBuckets(nullptr));
    EXPECT_EQ(0U, buckets.size());
    EXPECT_EQ(0, histograms.GetTotalSum("Quota.OrphanBucketCount"));

    EXPECT_EQ(db.SetBucketLastAccessTime(first_party_bucket.id,
                                         base::Time::Now() - base::Days(2)),
              QuotaError::kNone);
    EXPECT_EQ(db.SetBucketLastModifiedTime(first_party_bucket.id,
                                           base::Time::Now() - base::Days(2)),
              QuotaError::kNone);
    db.SetAlreadyEvictedStaleStorageForTesting(false);
    ASSERT_OK_AND_ASSIGN(buckets, db.GetExpiredBuckets(nullptr));
    EXPECT_EQ(0U, buckets.size());
    EXPECT_EQ(0, histograms.GetTotalSum("Quota.OrphanBucketCount"));
  }

  // First party nonce bucket does qualify, but only if it's old and we haven't
  // already looked.
  BucketInitParams first_party_nonce_params(
      StorageKey::CreateWithNonce(
          url::Origin::Create(GURL("http://firstpartynonce/")),
          base::UnguessableToken::Create()),
      kDefaultBucketName);
  ASSERT_OK_AND_ASSIGN(BucketInfo first_party_nonce_bucket,
                       db.UpdateOrCreateBucket(first_party_nonce_params, 0));
  {
    base::HistogramTester histograms;
    db.SetAlreadyEvictedStaleStorageForTesting(false);
    ASSERT_OK_AND_ASSIGN(std::set<BucketInfo> buckets,
                         db.GetExpiredBuckets(nullptr));
    EXPECT_EQ(0U, buckets.size());
    EXPECT_EQ(0, histograms.GetTotalSum("Quota.OrphanBucketCount"));

    EXPECT_EQ(db.SetBucketLastAccessTime(first_party_nonce_bucket.id,
                                         base::Time::Now() - base::Days(2)),
              QuotaError::kNone);
    EXPECT_EQ(db.SetBucketLastModifiedTime(first_party_nonce_bucket.id,
                                           base::Time::Now() - base::Days(2)),
              QuotaError::kNone);
    ASSERT_OK_AND_ASSIGN(buckets, db.GetExpiredBuckets(nullptr));
    EXPECT_EQ(0U, buckets.size());
    EXPECT_EQ(0, histograms.GetTotalSum("Quota.OrphanBucketCount"));
    db.SetAlreadyEvictedStaleStorageForTesting(false);
    ASSERT_OK_AND_ASSIGN(buckets, db.GetExpiredBuckets(nullptr));
    EXPECT_EQ((evict_stale_buckets() && evict_orphan_buckets()) ? 1U : 0U,
              buckets.size());
    EXPECT_EQ((evict_stale_buckets() && evict_orphan_buckets()) ? 1 : 0,
              histograms.GetTotalSum("Quota.OrphanBucketCount"));
  }

  // Third party bucket doesn't qualify, even if it's old.
  BucketInitParams third_party_params(
      StorageKey::Create(url::Origin::Create(GURL("https://thirdparty/")),
                         net::SchemefulSite(GURL("https://thirdparty2/")),
                         blink::mojom::AncestorChainBit::kCrossSite),
      kDefaultBucketName);
  ASSERT_OK_AND_ASSIGN(BucketInfo third_party_bucket,
                       db.UpdateOrCreateBucket(third_party_params, 0));
  {
    base::HistogramTester histograms;
    db.SetAlreadyEvictedStaleStorageForTesting(false);
    ASSERT_OK_AND_ASSIGN(std::set<BucketInfo> buckets,
                         db.GetExpiredBuckets(nullptr));
    EXPECT_EQ((evict_stale_buckets() && evict_orphan_buckets()) ? 1U : 0U,
              buckets.size());
    EXPECT_EQ((evict_stale_buckets() && evict_orphan_buckets()) ? 1 : 0,
              histograms.GetTotalSum("Quota.OrphanBucketCount"));

    EXPECT_EQ(db.SetBucketLastAccessTime(third_party_bucket.id,
                                         base::Time::Now() - base::Days(2)),
              QuotaError::kNone);
    EXPECT_EQ(db.SetBucketLastModifiedTime(third_party_bucket.id,
                                           base::Time::Now() - base::Days(2)),
              QuotaError::kNone);
    db.SetAlreadyEvictedStaleStorageForTesting(false);
    ASSERT_OK_AND_ASSIGN(buckets, db.GetExpiredBuckets(nullptr));
    EXPECT_EQ((evict_stale_buckets() && evict_orphan_buckets()) ? 1 : 0U,
              buckets.size());
    EXPECT_EQ((evict_stale_buckets() && evict_orphan_buckets()) ? 2 : 0,
              histograms.GetTotalSum("Quota.OrphanBucketCount"));
  }

  // Third party nonce bucket does qualify, but only if it's old and we haven't
  // already looked.
  BucketInitParams third_party_nonce_params(
      StorageKey::Create(
          url::Origin::Create(GURL("https://thirdparty/")),
          net::SchemefulSite(url::Origin::Create(GURL("http://thirdparty2/"))
                                 .DeriveNewOpaqueOrigin()),
          blink::mojom::AncestorChainBit::kCrossSite),
      kDefaultBucketName);
  ASSERT_OK_AND_ASSIGN(BucketInfo third_party_nonce_bucket,
                       db.UpdateOrCreateBucket(third_party_nonce_params, 0));
  {
    base::HistogramTester histograms;
    db.SetAlreadyEvictedStaleStorageForTesting(false);
    ASSERT_OK_AND_ASSIGN(std::set<BucketInfo> buckets,
                         db.GetExpiredBuckets(nullptr));
    EXPECT_EQ((evict_stale_buckets() && evict_orphan_buckets()) ? 1U : 0U,
              buckets.size());
    EXPECT_EQ((evict_stale_buckets() && evict_orphan_buckets()) ? 1 : 0,
              histograms.GetTotalSum("Quota.OrphanBucketCount"));

    EXPECT_EQ(db.SetBucketLastAccessTime(third_party_nonce_bucket.id,
                                         base::Time::Now() - base::Days(2)),
              QuotaError::kNone);
    EXPECT_EQ(db.SetBucketLastModifiedTime(third_party_nonce_bucket.id,
                                           base::Time::Now() - base::Days(2)),
              QuotaError::kNone);
    ASSERT_OK_AND_ASSIGN(buckets, db.GetExpiredBuckets(nullptr));
    EXPECT_EQ(0U, buckets.size());
    EXPECT_EQ((evict_stale_buckets() && evict_orphan_buckets()) ? 1 : 0,
              histograms.GetTotalSum("Quota.OrphanBucketCount"));
    db.SetAlreadyEvictedStaleStorageForTesting(false);
    ASSERT_OK_AND_ASSIGN(buckets, db.GetExpiredBuckets(nullptr));
    EXPECT_EQ((evict_stale_buckets() && evict_orphan_buckets()) ? 2U : 0U,
              buckets.size());
    EXPECT_EQ((evict_stale_buckets() && evict_orphan_buckets()) ? 3 : 0,
              histograms.GetTotalSum("Quota.OrphanBucketCount"));
  }
}

TEST_P(QuotaDatabaseTest, PersistentPolicy) {
  QuotaDatabase db(ProfilePath());
  const auto storage_key =
      StorageKey::CreateFromStringForTesting("http://google/");
  BucketInitParams default_params =
      BucketInitParams::ForDefaultBucket(storage_key);
  BucketInitParams non_default_params(storage_key, "inbox");

  // Insert default bucket first (so it's LRU).
  ASSERT_OK_AND_ASSIGN(BucketInfo result,
                       db.UpdateOrCreateBucket(default_params, 0));
  const BucketId default_id = result.id;

  // Then non default bucket.
  ASSERT_OK_AND_ASSIGN(result, db.UpdateOrCreateBucket(non_default_params, 0));
  const BucketId non_default_id = result.id;
  EXPECT_NE(non_default_id, default_id);

  // Get evictable bucket --- should be the default one.
  auto policy = base::MakeRefCounted<MockSpecialStoragePolicy>();
  ASSERT_OK_AND_ASSIGN(
      std::set<BucketLocator> lru_result,
      db.GetBucketsForEviction(kTemp, 1, {}, {}, policy.get()));
  ASSERT_EQ(1U, lru_result.size());
  EXPECT_EQ(default_id, lru_result.begin()->id);

  // Check that durable policy applies to the default bucket but not the non
  // default (non default buckets use the persist columnn in the database).
  policy->AddDurable(storage_key.origin().GetURL());
  ASSERT_OK_AND_ASSIGN(
      lru_result, db.GetBucketsForEviction(kTemp, 1, {}, {}, policy.get()));
  ASSERT_EQ(1U, lru_result.size());
  EXPECT_EQ(non_default_id, lru_result.begin()->id);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    QuotaDatabaseTest,
    testing::Combine(/*use_in_memory_db=*/testing::Bool(),
                     /*evict_stale_buckets=*/testing::Bool(),
                     /*evict_orphan_buckets=*/testing::Bool()));

}  // namespace storage
