// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/dcheck_is_on.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "components/services/storage/indexed_db/scopes/varint_coding.h"
#include "components/services/storage/indexed_db/transactional_leveldb/leveldb_write_batch.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_database.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/cpp/buckets/constants.h"
#include "content/browser/indexed_db/indexed_db_external_object.h"
#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"
#include "content/browser/indexed_db/indexed_db_value.h"
#include "content/browser/indexed_db/instance/backing_store_test_base.h"
#include "content/browser/indexed_db/instance/leveldb/backing_store.h"
#include "content/browser/indexed_db/instance/leveldb/cleanup_scheduler.h"
#include "content/browser/indexed_db/status.h"
#include "net/base/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"
#include "url/gurl.h"

using blink::IndexedDBKey;
using blink::IndexedDBKeyPath;
using blink::IndexedDBKeyRange;
using blink::IndexedDBObjectStoreMetadata;
using blink::StorageKey;

namespace content::indexed_db::level_db {

namespace {

using DatabaseMetadata = BackingStore::DatabaseMetadata;

int64_t GetId(indexed_db::BackingStore::Database& db) {
  return *reinterpret_cast<const DatabaseMetadata&>(db.GetMetadata()).id;
}

}  // namespace

class LevelDbBackingStoreTest : public BackingStoreTestBase {
 public:
  LevelDbBackingStoreTest() : BackingStoreTestBase(/*use_sqlite=*/false) {}

  LevelDbBackingStoreTest(const LevelDbBackingStoreTest&) = delete;
  LevelDbBackingStoreTest& operator=(const LevelDbBackingStoreTest&) = delete;

  level_db::BackingStore* backing_store() {
    return static_cast<level_db::BackingStore*>(
        BackingStoreTestBase::backing_store());
  }
};

class LevelDbBackingStoreTestForThirdPartyStoragePartitioning
    : public testing::WithParamInterface<bool>,
      public LevelDbBackingStoreTest {
 public:
  LevelDbBackingStoreTestForThirdPartyStoragePartitioning() {
    scoped_feature_list_.InitWithFeatureState(
        net::features::kThirdPartyStoragePartitioning,
        IsThirdPartyStoragePartitioningEnabled());
  }

  bool IsThirdPartyStoragePartitioningEnabled() { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    LevelDbBackingStoreTestForThirdPartyStoragePartitioning,
    testing::Bool());

enum class ExternalObjectTestType {
  kOnlyBlobs,
  kOnlyFileSystemAccessHandles,
  kBlobsAndFileSystemAccessHandles
};

class LevelDbBackingStoreWithExternalObjectsTestBase
    : public BackingStoreWithExternalObjectsTestBase {
 public:
  LevelDbBackingStoreWithExternalObjectsTestBase()
      : BackingStoreWithExternalObjectsTestBase(/*use_sqlite=*/false) {}

  LevelDbBackingStoreWithExternalObjectsTestBase(
      const LevelDbBackingStoreWithExternalObjectsTestBase&) = delete;
  LevelDbBackingStoreWithExternalObjectsTestBase& operator=(
      const LevelDbBackingStoreWithExternalObjectsTestBase&) = delete;

  bool CheckBlobReadsMatchWrites(
      const std::vector<IndexedDBExternalObject>& reads) const {
    if (blob_context_->writes().size() +
            file_system_access_context_->writes().size() !=
        reads.size()) {
      return false;
    }
    std::set<base::FilePath> ids;
    for (const auto& write : blob_context_->writes()) {
      ids.insert(write.path);
    }
    if (ids.size() != blob_context_->writes().size()) {
      return false;
    }
    for (const auto& read : reads) {
      switch (read.object_type()) {
        case IndexedDBExternalObject::ObjectType::kBlob:
        case IndexedDBExternalObject::ObjectType::kFile:
          if (ids.count(read.indexed_db_file_path()) != 1) {
            return false;
          }
          break;
        case IndexedDBExternalObject::ObjectType::kFileSystemAccessHandle:
          if (read.serialized_file_system_access_handle().size() != 1 ||
              read.serialized_file_system_access_handle()[0] >
                  file_system_access_context_->writes().size()) {
            return false;
          }
          break;
      }
    }
    return true;
  }

  bool CheckBlobWrites() {
    size_t num_empty_blobs = 0;
    for (const auto& info : external_objects_) {
      if (info.object_type() == IndexedDBExternalObject::ObjectType::kFile &&
          !info.size()) {
        num_empty_blobs++;
      }
    }

    size_t num_written = blob_context_->writes().size() +
                         file_system_access_context_->writes().size();
    if (num_written != external_objects_.size() - num_empty_blobs) {
      return false;
    }
    for (size_t i = 0; i < blob_context_->writes().size(); ++i) {
      const MockBlobStorageContext::BlobWrite& desc =
          blob_context_->writes()[i];
      const IndexedDBExternalObject& info = external_objects_[i];
      if (!info.size()) {
        continue;
      }

      DCHECK(desc.blob.is_bound());
      DCHECK(desc.blob.is_connected());
    }
    for (size_t i = 0; i < file_system_access_context_->writes().size(); ++i) {
      const IndexedDBExternalObject& info =
          external_objects_[blob_context_->writes().size() + i];
      base::UnguessableToken info_token;
      {
        base::RunLoop loop;
        info.file_system_access_token_remote()->GetInternalID(
            base::BindLambdaForTesting(
                [&](const base::UnguessableToken& token) {
                  info_token = token;
                  loop.Quit();
                }));
        loop.Run();
      }
      base::UnguessableToken written_token;
      {
        base::RunLoop loop;
        file_system_access_context_->writes()[i]->GetInternalID(
            base::BindLambdaForTesting(
                [&](const base::UnguessableToken& token) {
                  written_token = token;
                  loop.Quit();
                }));
        loop.Run();
      }
      if (info_token != written_token) {
        EXPECT_EQ(info_token, written_token);
        return false;
      }
    }
    return true;
  }

  void VerifyNumBlobsRemoved(int deleted_count) {
#if DCHECK_IS_ON()
    EXPECT_EQ(deleted_count + removed_blobs_count_,
              backing_store()->NumBlobFilesDeletedForTesting());
    removed_blobs_count_ += deleted_count;
#endif
  }

  void CheckFirstNBlobsRemoved(size_t deleted_count) {
    VerifyNumBlobsRemoved(deleted_count);

    for (size_t i = 0; i < deleted_count; ++i) {
      EXPECT_FALSE(base::PathExists(blob_context_->writes()[i].path));
    }
  }

  level_db::BackingStore* backing_store() {
    return static_cast<level_db::BackingStore*>(
        BackingStoreTestBase::backing_store());
  }

#if DCHECK_IS_ON()
 private:
  // Number of blob deletions previously counted by a call to
  // `VerifyNumBlobsRemoved()`.
  int removed_blobs_count_ = 0;
#endif
};

class LevelDbBackingStoreTestWithExternalObjects
    : public testing::WithParamInterface<ExternalObjectTestType>,
      public LevelDbBackingStoreWithExternalObjectsTestBase {
 public:
  LevelDbBackingStoreTestWithExternalObjects() = default;

  LevelDbBackingStoreTestWithExternalObjects(
      const LevelDbBackingStoreTestWithExternalObjects&) = delete;
  LevelDbBackingStoreTestWithExternalObjects& operator=(
      const LevelDbBackingStoreTestWithExternalObjects&) = delete;

  virtual ExternalObjectTestType TestType() { return GetParam(); }

  bool IncludesBlobs() override {
    return TestType() != ExternalObjectTestType::kOnlyFileSystemAccessHandles;
  }

  bool IncludesFileSystemAccessHandles() override {
    return TestType() != ExternalObjectTestType::kOnlyBlobs;
  }
};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    LevelDbBackingStoreTestWithExternalObjects,
    ::testing::Values(
        ExternalObjectTestType::kOnlyBlobs,
        ExternalObjectTestType::kOnlyFileSystemAccessHandles,
        ExternalObjectTestType::kBlobsAndFileSystemAccessHandles));

class LevelDbBackingStoreTestWithBlobs
    : public LevelDbBackingStoreTestWithExternalObjects {
 public:
  bool IncludesBlobs() override { return true; }

  bool IncludesFileSystemAccessHandles() override { return false; }
};

// http://crbug.com/1131151
// Validate that recovery journal cleanup during a transaction does
// not delete blobs that were just written.
TEST_P(LevelDbBackingStoreTestWithExternalObjects, BlobWriteCleanup) {
  BackingStore::Database db(*backing_store(),
                            BackingStore::DatabaseMetadata{u"name"});
  db.metadata().id = 1;
  const auto keys =
      std::to_array({IndexedDBKey(u"key0"), IndexedDBKey(u"key1"),
                     IndexedDBKey(u"key2"), IndexedDBKey(u"key3")});

  const int64_t object_store_id = 1;

  external_objects().clear();
  for (size_t j = 0; j < 4; ++j) {
    std::string type = "type " + base::NumberToString(j);
    external_objects().push_back(
        CreateBlobInfo(base::UTF8ToUTF16(type), "payload"));
  }

  auto values = std::to_array({
      IndexedDBValue("value0", {external_objects()[0]}),
      IndexedDBValue("value1", {external_objects()[1]}),
      IndexedDBValue("value2", {external_objects()[2]}),
      IndexedDBValue("value3", {external_objects()[3]}),
  });
  ASSERT_GE(keys.size(), values.size());

  // Validate that cleaning up after writing blobs does not delete those
  // blobs.
  backing_store()->SetExecuteJournalCleaningOnNoTransactionsForTesting();

  auto transaction1 =
      db.CreateTransaction(blink::mojom::IDBTransactionDurability::Relaxed,
                           blink::mojom::IDBTransactionMode::ReadWrite);
  transaction1->Begin(CreateDummyLock());
  for (size_t i = 0; i < values.size(); ++i) {
    EXPECT_TRUE(
        transaction1->PutRecord(object_store_id, keys[i], std::move(values[i]))
            .has_value());
  }

  // Start committing transaction1.
  CommitTransactionPhaseOneAndVerify(*transaction1);
  EXPECT_TRUE(CheckBlobWrites());

  // Finish committing transaction1.
  EXPECT_TRUE(transaction1->CommitPhaseTwo().ok());

  // Verify lack of blob removals.
  VerifyNumBlobsRemoved(0);
}

TEST_P(LevelDbBackingStoreTestWithExternalObjects,
       BlobJournalInterleavedTransactions) {
  BackingStore::Database db(*backing_store(),
                            BackingStore::DatabaseMetadata{u"name"});
  db.metadata().id = 1;
  // Initiate transaction1.
  auto transaction1 =
      db.CreateTransaction(blink::mojom::IDBTransactionDurability::Relaxed,
                           blink::mojom::IDBTransactionMode::ReadWrite);
  transaction1->Begin(CreateDummyLock());
  EXPECT_TRUE(transaction1->PutRecord(1, key3_, value3_.Clone()).has_value());
  CommitTransactionPhaseOneAndVerify(*transaction1);

  // Verify transaction1 phase one completed as expected.
  EXPECT_TRUE(CheckBlobWrites());
  VerifyNumBlobsRemoved(0);

  // Initiate transaction2.
  auto transaction2 =
      db.CreateTransaction(blink::mojom::IDBTransactionDurability::Relaxed,
                           blink::mojom::IDBTransactionMode::ReadWrite);
  transaction2->Begin(CreateDummyLock());
  EXPECT_TRUE(transaction2->PutRecord(1, key1_, value1_.Clone()).has_value());
  CommitTransactionPhaseOneAndVerify(*transaction2);

  // Verify transaction2 phase one completed.
  EXPECT_TRUE(CheckBlobWrites());
  VerifyNumBlobsRemoved(0);

  // Finalize both transactions.
  EXPECT_TRUE(transaction1->CommitPhaseTwo().ok());
  VerifyNumBlobsRemoved(0);

  EXPECT_TRUE(transaction2->CommitPhaseTwo().ok());
  VerifyNumBlobsRemoved(0);
}

TEST_P(LevelDbBackingStoreTestWithExternalObjects, ActiveBlobJournal) {
  BackingStore::Database db(*backing_store(),
                            BackingStore::DatabaseMetadata{u"name"});
  db.metadata().id = 1;
  auto transaction1 =
      db.CreateTransaction(blink::mojom::IDBTransactionDurability::Relaxed,
                           blink::mojom::IDBTransactionMode::ReadWrite);
  transaction1->Begin(CreateDummyLock());
  EXPECT_TRUE(transaction1->PutRecord(1, key3_, value3_.Clone()).has_value());
  CommitTransactionPhaseOneAndVerify(*transaction1);

  EXPECT_TRUE(CheckBlobWrites());
  EXPECT_TRUE(transaction1->CommitPhaseTwo().ok());

  auto transaction2 =
      db.CreateTransaction(blink::mojom::IDBTransactionDurability::Relaxed,
                           blink::mojom::IDBTransactionMode::ReadWrite);
  transaction2->Begin(CreateDummyLock());

  auto result = transaction2->GetRecord(1, key3_);
  EXPECT_TRUE(result.has_value());
  IndexedDBValue read_result_value = std::move(result.value());

  CommitTransactionAndVerify(*transaction2);
  EXPECT_EQ(base::span(value3_.bits), base::span(read_result_value.bits));
  EXPECT_TRUE(CheckBlobInfoMatches(read_result_value.external_objects));
  EXPECT_TRUE(CheckBlobReadsMatchWrites(read_result_value.external_objects));
  for (const IndexedDBExternalObject& external_object :
       read_result_value.external_objects) {
    if (external_object.mark_used_callback()) {
      external_object.mark_used_callback().Run();
    }
  }

  auto transaction3 =
      db.CreateTransaction(blink::mojom::IDBTransactionDurability::Relaxed,
                           blink::mojom::IDBTransactionMode::ReadWrite);
  transaction3->Begin(CreateDummyLock());
  EXPECT_TRUE(
      transaction3
          ->DeleteRange(1, IndexedDBKeyRange(key3_.Clone(), {}, false, false))
          .ok());
  CommitTransactionAndVerify(*transaction3);
  VerifyNumBlobsRemoved(0);
  for (const IndexedDBExternalObject& external_object :
       read_result_value.external_objects) {
    if (external_object.release_callback()) {
      external_object.release_callback().Run();
    }
  }
  task_environment_.RunUntilIdle();

  if (TestType() != ExternalObjectTestType::kOnlyFileSystemAccessHandles) {
    EXPECT_TRUE(backing_store()->IsBlobCleanupPending());
    EXPECT_EQ(
        3, backing_store()->NumAggregatedJournalCleaningRequestsForTesting());
    backing_store()->SetNumAggregatedJournalCleaningRequestsForTesting(
        BackingStore::kMaxJournalCleanRequests - 1);
    backing_store()->StartJournalCleaningTimer();
    CheckFirstNBlobsRemoved(3);
#if DCHECK_IS_ON()
    EXPECT_EQ(3, backing_store()->NumBlobFilesDeletedForTesting());
#endif
  }

  EXPECT_FALSE(backing_store()->IsBlobCleanupPending());
}

TEST_F(LevelDbBackingStoreTest, DatabaseIdsAreNonzeroAndIncreaseMonotonically) {
  auto db1 = backing_store()->CreateOrOpenDatabase(u"db1");
  ASSERT_TRUE(db1.has_value());
  EXPECT_GT(GetId(**db1), 0);

  auto db2 = backing_store()->CreateOrOpenDatabase(u"db2");
  ASSERT_TRUE(db2.has_value());
  EXPECT_GT(GetId(**db2), GetId(**db1));
}

// Make sure that using very high ( more than 32 bit ) values for
// database_id and object_store_id still work.
TEST_F(LevelDbBackingStoreTest, HighIds) {
  IndexedDBKey& key1 = key1_;
  IndexedDBKey& key2 = key2_;
  IndexedDBValue& value1 = value1_;

  const int64_t high_database_id = 1ULL << 35;
  const int64_t high_object_store_id = 1ULL << 39;
  // index_ids are capped at 32 bits for storage purposes.
  const int64_t high_index_id = 1ULL << 29;

  const int64_t invalid_high_index_id = 1ULL << 37;

  BackingStore::Database db(*backing_store(),
                            BackingStore::DatabaseMetadata{u"name"});
  db.metadata().id = high_database_id;

  const IndexedDBKey& index_key = key2;
  std::string index_key_raw;
  EncodeIDBKey(index_key, &index_key_raw);
  {
    auto txn1 =
        db.CreateTransaction(blink::mojom::IDBTransactionDurability::Relaxed,
                             blink::mojom::IDBTransactionMode::ReadWrite);
    auto& transaction1 = *txn1;
    transaction1.Begin(CreateDummyLock());
    auto record =
        transaction1.PutRecord(high_object_store_id, key1, value1.Clone());
    EXPECT_TRUE(record.has_value());

    Status s = transaction1.PutIndexDataForRecord(
        high_object_store_id, invalid_high_index_id, index_key, *record);
    EXPECT_FALSE(s.ok());

    s = transaction1.PutIndexDataForRecord(high_object_store_id, high_index_id,
                                           index_key, *record);
    EXPECT_TRUE(s.ok());

    CommitTransactionAndVerify(transaction1);
  }

  {
    auto txn2 =
        db.CreateTransaction(blink::mojom::IDBTransactionDurability::Relaxed,
                             blink::mojom::IDBTransactionMode::ReadWrite);
    auto& transaction2 = *txn2;
    transaction2.Begin(CreateDummyLock());
    auto result = transaction2.GetRecord(high_object_store_id, key1);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(base::span(value1.bits), base::span(result->bits));

    EXPECT_FALSE(transaction2
                     .GetFirstPrimaryKeyForIndexKey(
                         high_object_store_id, invalid_high_index_id, index_key)
                     .has_value());

    auto new_primary_key = transaction2.GetFirstPrimaryKeyForIndexKey(
        high_object_store_id, high_index_id, index_key);
    ASSERT_TRUE(new_primary_key.has_value());
    EXPECT_TRUE(new_primary_key->Equals(key1));

    CommitTransactionAndVerify(transaction2);
  }
}

// Make sure that other invalid ids do not crash.
TEST_F(LevelDbBackingStoreTest, InvalidIds) {
  const IndexedDBKey& key = key1_;
  IndexedDBValue& value = value1_;

  // valid ids for use when testing invalid ids
  const int64_t database_id = 1;
  const int64_t object_store_id = 1;
  const int64_t index_id = kMinimumIndexId;
  // index_ids must be > kMinimumIndexId
  const int64_t invalid_low_index_id = 19;
  IndexedDBValue result_value;

  BackingStore::Database db(*backing_store(),
                            BackingStore::DatabaseMetadata{u"name"});
  db.metadata().id = database_id;
  auto txn =
      db.CreateTransaction(blink::mojom::IDBTransactionDurability::Relaxed,
                           blink::mojom::IDBTransactionMode::ReadWrite);

  BackingStore::Transaction& transaction1 =
      *reinterpret_cast<BackingStore::Transaction*>(txn.get());
  transaction1.Begin(CreateDummyLock());

  db.metadata().id = database_id;
  EXPECT_FALSE(transaction1.PutRecord(KeyPrefix::kInvalidId, key, value.Clone())
                   .has_value());
  db.metadata().id = database_id;
  EXPECT_FALSE(transaction1.PutRecord(0, key, value.Clone()).has_value());
  db.metadata().id = KeyPrefix::kInvalidId;
  EXPECT_FALSE(
      transaction1.PutRecord(object_store_id, key, value.Clone()).has_value());
  db.metadata().id = 0;
  EXPECT_FALSE(
      transaction1.PutRecord(object_store_id, key, value.Clone()).has_value());

  db.metadata().id = database_id;
  auto result = transaction1.GetRecord(KeyPrefix::kInvalidId, key);
  EXPECT_FALSE(result.has_value());
  db.metadata().id = database_id;
  result = transaction1.GetRecord(0, key);
  EXPECT_FALSE(result.has_value());
  db.metadata().id = KeyPrefix::kInvalidId;
  result = transaction1.GetRecord(object_store_id, key);
  EXPECT_FALSE(result.has_value());
  db.metadata().id = 0;
  result = transaction1.GetRecord(object_store_id, key);
  EXPECT_FALSE(result.has_value());

  db.metadata().id = database_id;
  EXPECT_FALSE(transaction1
                   .GetFirstPrimaryKeyForIndexKey(object_store_id,
                                                  KeyPrefix::kInvalidId, key)
                   .has_value());
  EXPECT_FALSE(transaction1
                   .GetFirstPrimaryKeyForIndexKey(object_store_id,
                                                  invalid_low_index_id, key)
                   .has_value());
  EXPECT_FALSE(
      transaction1.GetFirstPrimaryKeyForIndexKey(object_store_id, 0, key)
          .has_value());

  db.metadata().id = KeyPrefix::kInvalidId;
  EXPECT_FALSE(
      transaction1.GetFirstPrimaryKeyForIndexKey(object_store_id, index_id, key)
          .has_value());
  db.metadata().id = database_id;
  EXPECT_FALSE(
      transaction1
          .GetFirstPrimaryKeyForIndexKey(KeyPrefix::kInvalidId, index_id, key)
          .has_value());
}

TEST_P(LevelDbBackingStoreTestForThirdPartyStoragePartitioning,
       ReadCorruptionInfoForOpaqueStorageKey) {
  storage::BucketLocator bucket_locator;
  bucket_locator.storage_key =
      blink::StorageKey::CreateFirstParty(url::Origin());
  bucket_locator.is_default = true;

  // No `path_base`.
  EXPECT_TRUE(
      indexed_db::ReadCorruptionInfo(base::FilePath(), bucket_locator).empty());
}

TEST_P(LevelDbBackingStoreTestForThirdPartyStoragePartitioning,
       ReadCorruptionInfoForFirstPartyStorageKey) {
  storage::BucketLocator bucket_locator;
  const base::FilePath path_base = temp_dir_.GetPath();
  bucket_locator.storage_key =
      blink::StorageKey::CreateFromStringForTesting("http://www.google.com/");
  bucket_locator.id = storage::BucketId::FromUnsafeValue(1);
  bucket_locator.is_default = true;
  ASSERT_FALSE(path_base.empty());

  // File not found.
  EXPECT_TRUE(
      indexed_db::ReadCorruptionInfo(path_base, bucket_locator).empty());

  const base::FilePath info_path =
      path_base.AppendASCII("http_www.google.com_0.indexeddb.leveldb")
          .AppendASCII("corruption_info.json");
  ASSERT_TRUE(CreateDirectory(info_path.DirName()));

  // Empty file.
  std::string dummy_data;
  ASSERT_TRUE(base::WriteFile(info_path, dummy_data));
  EXPECT_TRUE(
      indexed_db::ReadCorruptionInfo(path_base, bucket_locator).empty());
  EXPECT_FALSE(PathExists(info_path));

  // File size > 4 KB.
  dummy_data.resize(5000, 'c');
  ASSERT_TRUE(base::WriteFile(info_path, dummy_data));
  EXPECT_TRUE(
      indexed_db::ReadCorruptionInfo(path_base, bucket_locator).empty());
  EXPECT_FALSE(PathExists(info_path));

  // Random string.
  ASSERT_TRUE(base::WriteFile(info_path, "foo bar"));
  EXPECT_TRUE(
      indexed_db::ReadCorruptionInfo(path_base, bucket_locator).empty());
  EXPECT_FALSE(PathExists(info_path));

  // Not a dictionary.
  ASSERT_TRUE(base::WriteFile(info_path, "[]"));
  EXPECT_TRUE(
      indexed_db::ReadCorruptionInfo(path_base, bucket_locator).empty());
  EXPECT_FALSE(PathExists(info_path));

  // Empty dictionary.
  ASSERT_TRUE(base::WriteFile(info_path, "{}"));
  EXPECT_TRUE(
      indexed_db::ReadCorruptionInfo(path_base, bucket_locator).empty());
  EXPECT_FALSE(PathExists(info_path));

  // Dictionary, no message key.
  ASSERT_TRUE(base::WriteFile(info_path, "{\"foo\":\"bar\"}"));
  EXPECT_TRUE(
      indexed_db::ReadCorruptionInfo(path_base, bucket_locator).empty());
  EXPECT_FALSE(PathExists(info_path));

  // Dictionary, message key.
  ASSERT_TRUE(base::WriteFile(info_path, "{\"message\":\"bar\"}"));
  std::string message =
      indexed_db::ReadCorruptionInfo(path_base, bucket_locator);
  EXPECT_FALSE(message.empty());
  EXPECT_FALSE(PathExists(info_path));
  EXPECT_EQ("bar", message);

  // Dictionary, message key and more.
  ASSERT_TRUE(base::WriteFile(info_path, "{\"message\":\"foo\",\"bar\":5}"));
  message = indexed_db::ReadCorruptionInfo(path_base, bucket_locator);
  EXPECT_FALSE(message.empty());
  EXPECT_FALSE(PathExists(info_path));
  EXPECT_EQ("foo", message);
}

TEST_P(LevelDbBackingStoreTestForThirdPartyStoragePartitioning,
       ReadCorruptionInfoForThirdPartyStorageKey) {
  storage::BucketLocator bucket_locator;
  bucket_locator.storage_key = blink::StorageKey::Create(
      url::Origin::Create(GURL("http://www.google.com/")),
      net::SchemefulSite(GURL("http://www.youtube.com/")),
      blink::mojom::AncestorChainBit::kCrossSite);
  bucket_locator.id = storage::BucketId::FromUnsafeValue(1);
  bucket_locator.is_default = true;
  const base::FilePath path_base = temp_dir_.GetPath();
  ASSERT_FALSE(path_base.empty());

  // File not found.
  EXPECT_TRUE(
      indexed_db::ReadCorruptionInfo(path_base, bucket_locator).empty());

  base::FilePath info_path =
      path_base.AppendASCII("http_www.google.com_0.indexeddb.leveldb")
          .AppendASCII("corruption_info.json");
  if (IsThirdPartyStoragePartitioningEnabled()) {
    info_path = path_base.AppendASCII("indexeddb.leveldb")
                    .AppendASCII("corruption_info.json");
  }
  ASSERT_TRUE(CreateDirectory(info_path.DirName()));

  // Empty file.
  std::string dummy_data;
  ASSERT_TRUE(base::WriteFile(info_path, dummy_data));
  EXPECT_TRUE(
      indexed_db::ReadCorruptionInfo(path_base, bucket_locator).empty());
  EXPECT_FALSE(PathExists(info_path));

  // File size > 4 KB.
  dummy_data.resize(5000, 'c');
  ASSERT_TRUE(base::WriteFile(info_path, dummy_data));
  EXPECT_TRUE(
      indexed_db::ReadCorruptionInfo(path_base, bucket_locator).empty());
  EXPECT_FALSE(PathExists(info_path));

  // Random string.
  ASSERT_TRUE(base::WriteFile(info_path, "foo bar"));
  EXPECT_TRUE(
      indexed_db::ReadCorruptionInfo(path_base, bucket_locator).empty());
  EXPECT_FALSE(PathExists(info_path));

  // Not a dictionary.
  ASSERT_TRUE(base::WriteFile(info_path, "[]"));
  EXPECT_TRUE(
      indexed_db::ReadCorruptionInfo(path_base, bucket_locator).empty());
  EXPECT_FALSE(PathExists(info_path));

  // Empty dictionary.
  ASSERT_TRUE(base::WriteFile(info_path, "{}"));
  EXPECT_TRUE(
      indexed_db::ReadCorruptionInfo(path_base, bucket_locator).empty());
  EXPECT_FALSE(PathExists(info_path));

  // Dictionary, no message key.
  ASSERT_TRUE(base::WriteFile(info_path, "{\"foo\":\"bar\"}"));
  EXPECT_TRUE(
      indexed_db::ReadCorruptionInfo(path_base, bucket_locator).empty());
  EXPECT_FALSE(PathExists(info_path));

  // Dictionary, message key.
  ASSERT_TRUE(base::WriteFile(info_path, "{\"message\":\"bar\"}"));
  std::string message =
      indexed_db::ReadCorruptionInfo(path_base, bucket_locator);
  EXPECT_FALSE(message.empty());
  EXPECT_FALSE(PathExists(info_path));
  EXPECT_EQ("bar", message);

  // Dictionary, message key and more.
  ASSERT_TRUE(base::WriteFile(info_path, "{\"message\":\"foo\",\"bar\":5}"));
  message = indexed_db::ReadCorruptionInfo(path_base, bucket_locator);
  EXPECT_FALSE(message.empty());
  EXPECT_FALSE(PathExists(info_path));
  EXPECT_EQ("foo", message);
}
namespace {

// v3 Blob Data is encoded as a series of:
//   { is_file [bool], blob_number [int64_t as varInt],
//     type [string-with-length, may be empty],
//     (for Blobs only) size [int64_t as varInt]
//     (for Files only) fileName [string-with-length]
//   }
// There is no length field; just read until you run out of data.
std::string EncodeV3BlobInfos(
    const std::vector<IndexedDBExternalObject>& blob_info) {
  std::string ret;
  for (const auto& info : blob_info) {
    DCHECK(info.object_type() == IndexedDBExternalObject::ObjectType::kFile ||
           info.object_type() == IndexedDBExternalObject::ObjectType::kBlob);
    bool is_file =
        info.object_type() == IndexedDBExternalObject::ObjectType::kFile;
    EncodeBool(is_file, &ret);
    EncodeVarInt(info.blob_number(), &ret);
    EncodeStringWithLength(info.type(), &ret);
    if (is_file) {
      EncodeStringWithLength(info.file_name(), &ret);
    } else {
      EncodeVarInt(info.size(), &ret);
    }
  }
  return ret;
}

int64_t GetTotalBlobSize(MockBlobStorageContext* blob_context) {
  int64_t space_used = 0;
  for (const MockBlobStorageContext::BlobWrite& write :
       blob_context->writes()) {
    space_used += base::GetFileSize(write.path).value_or(0);
  }
  return space_used;
}

}  // namespace

// This test ensures that the rollback process correctly handles blob cleanup
// in abort scenarios, specifically verifying that blob writes are rolled back
// and no orphaned blobs are left on disk after an abort.
// For more details, refer to https://crbug.com/41460842
TEST_P(LevelDbBackingStoreTestWithExternalObjects, RollbackClearsDiskSpace) {
  // Enable writing files to disk for the blob context.
  blob_context_->SetWriteFilesToDisk(true);

  // Ensure no disk space is used initially.
  int64_t initial_disk_space = GetTotalBlobSize(blob_context_.get());
  ASSERT_EQ(initial_disk_space, 0);

  // The initial transaction is necessary to establish a baseline blob on disk.
  // This ensures that the rollback process only affects the blobs inserted in
  // the second transaction, allowing us to verify that the rollback correctly
  // handles blob cleanup without impacting pre-existing blobs.
  BackingStore::Database db(*backing_store(),
                            BackingStore::DatabaseMetadata{u"name"});
  db.metadata().id = 1;
  auto it =
      db.CreateTransaction(blink::mojom::IDBTransactionDurability::Relaxed,
                           blink::mojom::IDBTransactionMode::ReadWrite);
  auto& initial_transaction = *it;
  initial_transaction.Begin(CreateDummyLock());

  // Insert an initial blob.
  std::string initial_blob_name = "initial_blob";
  IndexedDBExternalObject initial_blob = CreateBlobInfo(
      base::UTF8ToUTF16(initial_blob_name), std::string('a', 100));
  IndexedDBValue initial_value("initial_value", {initial_blob});
  IndexedDBKey initial_key(u"initial_key");
  EXPECT_TRUE(initial_transaction
                  .PutRecord(/*object_store_id=*/1, initial_key,
                             std::move(initial_value))
                  .has_value());

  // Commit the initial transaction (Phase 1 and Phase 2).
  CommitTransactionAndVerify(initial_transaction);

  // Track the path of the initially written blob.
  ASSERT_GT(blob_context_->writes().size(), 0u);
  base::FilePath initial_blob_path = blob_context_->writes()[0].path;

  // Ensure disk space is used after committing the initial transaction.
  int64_t disk_space_after_committed_transaction =
      GetTotalBlobSize(blob_context_.get());
  ASSERT_GT(disk_space_after_committed_transaction, 0);

  // Start a new transaction for the test scenario (rollback).
  auto txn =
      db.CreateTransaction(blink::mojom::IDBTransactionDurability::Relaxed,
                           blink::mojom::IDBTransactionMode::ReadWrite);
  auto& transaction = *txn;
  transaction.Begin(CreateDummyLock());

  // Prepare test data for second transaction.
  IndexedDBKey key(u"key0");
  std::string name = "name0";
  IndexedDBExternalObject test_blob =
      CreateBlobInfo(base::UTF8ToUTF16(name), std::string('a', 100));
  IndexedDBValue value = IndexedDBValue("value0", {test_blob});

  // Insert additional blob that will be rolled back.
  EXPECT_TRUE(
      transaction.PutRecord(/*object_store_id=*/1, key, std::move(value))
          .has_value());

  // Simulate commit phase 1 to ensure that the blob is written to disk.
  CommitTransactionPhaseOneAndVerify(transaction);

  // Verify that disk space has increased after commit phase 1.
  int64_t disk_space_after_commit_phase_one =
      GetTotalBlobSize(blob_context_.get());
  ASSERT_GT(disk_space_after_commit_phase_one,
            disk_space_after_committed_transaction);

  // Set the condition to trigger immediate journal cleaning.
  backing_store()->SetNumAggregatedJournalCleaningRequestsForTesting(
      BackingStore::kMaxJournalCleanRequests - 1);

  // Introduce the rollback: Simulate a failure or abort in the transaction.
  transaction.Rollback();

  // Wait for the cleanup process to complete.
  base::RunLoop cleanup_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, cleanup_loop.QuitClosure());
  cleanup_loop.Run();

  // Verify that the number of writes is as expected.
  EXPECT_EQ(blob_context_->writes().size(), 2u);
  // After the rollback, verify that the blobs inserted in the transaction are
  // cleaned up.
  for (const MockBlobStorageContext::BlobWrite& write :
       blob_context_->writes()) {
    if (write.path == initial_blob_path) {
      // Ensure initial blob still exists.
      EXPECT_TRUE(base::PathExists(write.path));
    } else {
      // Ensure rolled-back blobs are deleted.
      EXPECT_FALSE(base::PathExists(write.path));
    }
  }

  // Finally, verify that the disk space is back to its original state after the
  // rollback.
  int64_t disk_space_after_rollback = GetTotalBlobSize(blob_context_.get());
  ASSERT_EQ(disk_space_after_committed_transaction, disk_space_after_rollback);
}

TEST_F(LevelDbBackingStoreTestWithBlobs, SchemaUpgradeV3ToV4) {
  const int64_t object_store_id = 99;

  const std::u16string database_name(u"db1");
  const int64_t version = 9;

  const std::u16string object_store_name(u"object_store1");
  const bool auto_increment = true;
  const IndexedDBKeyPath object_store_key_path(u"object_store_key");

  auto db = backing_store()->CreateOrOpenDatabase(database_name);
  ASSERT_TRUE(db.has_value());
  const int64_t database_id = GetId(**db);
  EXPECT_GT(database_id, 0);
  UpdateDatabaseVersion(**db, version);
  {
    auto txn = (*db)->CreateTransaction(
        blink::mojom::IDBTransactionDurability::Relaxed,
        blink::mojom::IDBTransactionMode::VersionChange);
    auto& transaction = *txn;
    transaction.Begin(CreateDummyLock());

    Status s =
        transaction.CreateObjectStore(object_store_id, object_store_name,
                                      object_store_key_path, auto_increment);
    EXPECT_TRUE(s.ok());

    const IndexedDBObjectStoreMetadata& object_store =
        (*db)->GetMetadata().object_stores.find(object_store_id)->second;
    EXPECT_EQ(object_store.id, object_store_id);

    CommitTransactionAndVerify(transaction);
  }
  task_environment_.RunUntilIdle();

  // Initiate transaction1 - writing blobs.
  auto transaction1 =
      (*db)->CreateTransaction(blink::mojom::IDBTransactionDurability::Relaxed,
                               blink::mojom::IDBTransactionMode::ReadWrite);
  transaction1->Begin(CreateDummyLock());
  EXPECT_TRUE(transaction1->PutRecord(object_store_id, key3_, value3_.Clone())
                  .has_value());
  CommitTransactionPhaseOneAndVerify(*transaction1);

  // Finish up transaction1, verifying blob writes.
  EXPECT_TRUE(CheckBlobWrites());
  ASSERT_TRUE(transaction1->CommitPhaseTwo().ok());
  transaction1.reset();

  task_environment_.RunUntilIdle();

  // Change entries to be v3, and change the schema to be v3.
  std::unique_ptr<LevelDBWriteBatch> write_batch = LevelDBWriteBatch::Create();
  const std::string schema_version_key = SchemaVersionKey::Encode();
  ASSERT_TRUE(
      indexed_db::PutInt(write_batch.get(), schema_version_key, 3).ok());
  const std::string object_store_data_key =
      ObjectStoreDataKey::Encode(database_id, object_store_id, key3_);
  std::string_view leveldb_key_piece(object_store_data_key);
  BlobEntryKey blob_entry_key;
  ASSERT_TRUE(BlobEntryKey::FromObjectStoreDataKey(&leveldb_key_piece,
                                                   &blob_entry_key));
  ASSERT_EQ(blob_context_->writes().size(), 3u);
  auto& writes = blob_context_->writes();
  external_objects()[0].set_blob_number(writes[0].GetBlobNumber());
  external_objects()[1].set_blob_number(writes[1].GetBlobNumber());
  external_objects()[2].set_blob_number(writes[2].GetBlobNumber());
  std::string v3_blob_data = EncodeV3BlobInfos(external_objects());
  write_batch->Put(std::string_view(blob_entry_key.Encode()),
                   std::string_view(v3_blob_data));
  ASSERT_TRUE(backing_store()->db()->Write(write_batch.get()).ok());

  // The migration code uses the physical files on disk, so those need to be
  // written with the correct size & timestamp.
  base::FilePath file1_path = writes[1].path;
  base::FilePath file2_path = writes[2].path;
  ASSERT_TRUE(CreateDirectory(file1_path.DirName()));
  ASSERT_TRUE(CreateDirectory(file2_path.DirName()));
  base::File file1(file1_path,
                   base::File::FLAG_WRITE | base::File::FLAG_CREATE_ALWAYS);
  ASSERT_TRUE(file1.IsValid());
  ASSERT_TRUE(
      file1.WriteAtCurrentPosAndCheck(base::as_byte_span(kBlobFileData1)));
  ASSERT_TRUE(file1.SetTimes(external_objects()[1].last_modified(),
                             external_objects()[1].last_modified()));
  file1.Close();

  base::File file2(file2_path,
                   base::File::FLAG_WRITE | base::File::FLAG_CREATE_ALWAYS);
  ASSERT_TRUE(file2.IsValid());
  ASSERT_TRUE(
      file2.WriteAtCurrentPosAndCheck(base::as_byte_span(kBlobFileData2)));
  ASSERT_TRUE(file2.SetTimes(external_objects()[2].last_modified(),
                             external_objects()[2].last_modified()));
  file2.Close();

  db->reset();
  DestroyFactoryAndBackingStore();
  CreateFactoryAndBackingStore();

  // There should be no corruption.
  ASSERT_TRUE(data_loss_info_.status == blink::mojom::IDBDataLoss::None);

  // Re-open the database.
  db = backing_store()->CreateOrOpenDatabase(database_name);
  ASSERT_TRUE(db.has_value());
  EXPECT_EQ(GetId(**db), database_id);
  EXPECT_EQ((*db)->GetMetadata().version, version);

  // Initiate transaction2, reading blobs.
  auto txn2 =
      (*db)->CreateTransaction(blink::mojom::IDBTransactionDurability::Relaxed,
                               blink::mojom::IDBTransactionMode::ReadWrite);
  auto& transaction2 = *txn2;
  transaction2.Begin(CreateDummyLock());
  auto result = transaction2.GetRecord(object_store_id, key3_);
  ASSERT_TRUE(result.has_value());
  IndexedDBValue result_value = std::move(result.value());

  // Finish up transaction2, verifying blob reads.
  CommitTransactionAndVerify(transaction2);
  EXPECT_EQ(base::span(value3_.bits), base::span(result_value.bits));
  EXPECT_TRUE(CheckBlobInfoMatches(result_value.external_objects));
}

TEST_F(LevelDbBackingStoreTestWithBlobs, SchemaUpgradeV4ToV5) {
  const int64_t object_store_id = 99;

  const std::u16string database_name(u"db1");
  const int64_t version = 9;

  const std::u16string object_store_name(u"object_store1");
  const bool auto_increment = true;
  const IndexedDBKeyPath object_store_key_path(u"object_store_key");

  // Add an empty blob here to test with.  Empty blobs are not written
  // to disk, so it's important to verify that a database with empty blobs
  // should be considered still valid.
  external_objects().push_back(
      CreateFileInfo(u"empty blob", u"file type", base::Time::Now(), ""));
  // The V5 migration checks files on disk, so make sure our fake blob
  // context writes something there to check.
  blob_context_->SetWriteFilesToDisk(true);

  auto db = backing_store()->CreateOrOpenDatabase(database_name);
  EXPECT_TRUE(db.has_value());
  const int64_t database_id = GetId(**db);
  EXPECT_GT(database_id, 0);
  UpdateDatabaseVersion(**db, version);
  {
    auto txn = (*db)->CreateTransaction(
        blink::mojom::IDBTransactionDurability::Relaxed,
        blink::mojom::IDBTransactionMode::VersionChange);
    auto& transaction = *txn;
    transaction.Begin(CreateDummyLock());

    Status s =
        transaction.CreateObjectStore(object_store_id, object_store_name,
                                      object_store_key_path, auto_increment);
    EXPECT_TRUE(s.ok());

    const IndexedDBObjectStoreMetadata& object_store =
        (*db)->GetMetadata().object_stores.find(object_store_id)->second;
    EXPECT_EQ(object_store.id, object_store_id);

    CommitTransactionAndVerify(transaction);
  }
  task_environment_.RunUntilIdle();

  // Initiate transaction - writing blobs.
  auto transaction =
      (*db)->CreateTransaction(blink::mojom::IDBTransactionDurability::Relaxed,
                               blink::mojom::IDBTransactionMode::ReadWrite);
  transaction->Begin(CreateDummyLock());

  IndexedDBKey key(u"key");
  IndexedDBValue value = IndexedDBValue("value3", external_objects());

  EXPECT_TRUE(transaction->PutRecord(object_store_id, key, std::move(value))
                  .has_value());
  CommitTransactionPhaseOneAndVerify(*transaction);

  // Finish up transaction, verifying blob writes.
  EXPECT_TRUE(CheckBlobWrites());
  ASSERT_TRUE(transaction->CommitPhaseTwo().ok());
  transaction.reset();

  task_environment_.RunUntilIdle();
  ASSERT_EQ(blob_context_->writes().size(), 3u);

  // Verify V4 to V5 conversion with all blobs intact has no data loss.
  {
    // Change the schema to be v4.
    const int64_t old_version = 4;
    std::unique_ptr<LevelDBWriteBatch> write_batch =
        LevelDBWriteBatch::Create();
    const std::string schema_version_key = SchemaVersionKey::Encode();
    ASSERT_TRUE(
        indexed_db::PutInt(write_batch.get(), schema_version_key, old_version)
            .ok());
    ASSERT_TRUE(backing_store()->db()->Write(write_batch.get()).ok());

    DestroyFactoryAndBackingStore();
    CreateFactoryAndBackingStore();

    // There should be no corruption here.
    ASSERT_EQ(data_loss_info_.status, blink::mojom::IDBDataLoss::None);
  }

  // Verify V4 to V5 conversion with missing blobs has data loss.
  {
    // Change the schema to be v4.
    const int64_t old_version = 4;
    std::unique_ptr<LevelDBWriteBatch> write_batch =
        LevelDBWriteBatch::Create();
    const std::string schema_version_key = SchemaVersionKey::Encode();
    ASSERT_TRUE(
        indexed_db::PutInt(write_batch.get(), schema_version_key, old_version)
            .ok());
    ASSERT_TRUE(backing_store()->db()->Write(write_batch.get()).ok());

    // Pick a blob we wrote arbitrarily and delete it.
    auto path = blob_context_->writes()[1].path;
    base::DeleteFile(path);

    DestroyFactoryAndBackingStore();
    CreateFactoryAndBackingStore();

    // This should be corrupted.
    ASSERT_NE(data_loss_info_.status, blink::mojom::IDBDataLoss::None);
    DestroyFactoryAndBackingStore();
  }
}

class LevelDbBackingStoreTestForCleanupScheduler
    : public LevelDbBackingStoreTest {
 public:
  LevelDbBackingStoreTestForCleanupScheduler() {
    scoped_feature_list_.InitAndEnableFeature(kIdbInSessionDbCleanup);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(LevelDbBackingStoreTestForCleanupScheduler,
       SchedulerInitializedIfTombstoneThresholdExceeded) {
  backing_store()->OnTransactionComplete(false);
  EXPECT_FALSE(backing_store()
                   ->GetLevelDBCleanupSchedulerForTesting()
                   .GetRunningStateForTesting()
                   .has_value());
  backing_store()->OnTransactionComplete(true);
  EXPECT_TRUE(backing_store()
                  ->GetLevelDBCleanupSchedulerForTesting()
                  .GetRunningStateForTesting()
                  .has_value());
}

TEST_F(LevelDbBackingStoreTest, TombstoneSweeperTiming) {
  // Open a connection.
  EXPECT_FALSE(backing_store()->ShouldRunTombstoneSweeper());

  // Move the clock to run the tasks in the next close sequence.
  task_environment_.FastForwardBy(kMaxGlobalSweepDelay);

  EXPECT_TRUE(backing_store()->ShouldRunTombstoneSweeper());

  // Move clock forward to trigger next sweep, but storage key has longer
  // sweep minimum, so no tasks should execute.
  task_environment_.FastForwardBy(kMaxGlobalSweepDelay);

  EXPECT_FALSE(backing_store()->ShouldRunTombstoneSweeper());

  //  Finally, move the clock forward so the storage key should allow a sweep.
  task_environment_.FastForwardBy(kMaxBucketSweepDelay);

  EXPECT_TRUE(backing_store()->ShouldRunTombstoneSweeper());
}

TEST_F(LevelDbBackingStoreTest, CompactionTaskTiming) {
  EXPECT_FALSE(backing_store()->ShouldRunCompaction());

  // Move the clock to run the tasks in the next close sequence.
  task_environment_.FastForwardBy(kMaxGlobalCompactionDelay);

  EXPECT_TRUE(backing_store()->ShouldRunCompaction());

  // Move clock forward to trigger next compaction, but storage key has longer
  // compaction minimum, so no tasks should execute.
  task_environment_.FastForwardBy(kMaxGlobalCompactionDelay);

  EXPECT_FALSE(backing_store()->ShouldRunCompaction());

  // Finally, move the clock forward so the storage key should allow a
  // compaction.
  task_environment_.FastForwardBy(kMaxBucketCompactionDelay);

  EXPECT_TRUE(backing_store()->ShouldRunCompaction());
}

TEST_F(LevelDbBackingStoreTest, InSessionCleanupVerification) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/
      {content::indexed_db::level_db::kIdbInSessionDbCleanup,
       content::indexed_db::level_db::kIdbVerifyInSessionDbCleanup},
      /*disabled_features=*/{});

  const int object_store_id = 1;
  auto db_creation_result = backing_store()->CreateOrOpenDatabase(u"name");
  ASSERT_TRUE(db_creation_result.has_value());
  indexed_db::BackingStore::Database& db = **db_creation_result;

  {
    auto txn =
        db.CreateTransaction(blink::mojom::IDBTransactionDurability::Relaxed,
                             blink::mojom::IDBTransactionMode::VersionChange);
    txn->Begin(CreateDummyLock());

    EXPECT_TRUE(txn->CreateObjectStore(object_store_id, u"object_store_name",
                                       IndexedDBKeyPath(u"object_store_key"),
                                       /*auto_increment=*/true)
                    .ok());
    CommitTransactionAndVerify(*txn);
  }

  {
    auto txn =
        db.CreateTransaction(blink::mojom::IDBTransactionDurability::Relaxed,
                             blink::mojom::IDBTransactionMode::ReadWrite);
    txn->Begin(CreateDummyLock());
    EXPECT_TRUE(txn->PutRecord(object_store_id, IndexedDBKey("key"),
                               IndexedDBValue("value", {}))
                    .has_value());
    CommitTransactionAndVerify(*txn);
  }

  // Verify that cleanup verification only occurs once in a while, not on every
  // cleanup.
  base::HistogramTester histograms;

  // Verify on first cleanup.
  backing_store()->OnCleanupStarted();
  backing_store()->OnCleanupDone();
  histograms.ExpectBucketCount(
      "IndexedDB.LevelDB.InSessionCleanupVerificationEvent",
      level_db::BackingStore::InSessionCleanupVerificationEvent::
          kCleanupStarted,
      1);

  // Don't verify on next few cleanups.
  for (int i = 0; i < 60; ++i) {
    backing_store()->OnCleanupStarted();
    backing_store()->OnCleanupDone();
  }

  histograms.ExpectBucketCount(
      "IndexedDB.LevelDB.InSessionCleanupVerificationEvent",
      level_db::BackingStore::InSessionCleanupVerificationEvent::
          kCleanupStarted,
      1);

  // Verify again eventually.
  for (int i = 0; i < 60; ++i) {
    backing_store()->OnCleanupStarted();
    backing_store()->OnCleanupDone();
  }

  histograms.ExpectBucketCount(
      "IndexedDB.LevelDB.InSessionCleanupVerificationEvent",
      level_db::BackingStore::InSessionCleanupVerificationEvent::
          kCleanupStarted,
      2);
}

}  // namespace content::indexed_db::level_db
