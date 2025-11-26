// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/backing_store.h"

#include <memory>
#include <string>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_expected_support.h"
#include "content/browser/indexed_db/indexed_db_value.h"
#include "content/browser/indexed_db/instance/backing_store_test_base.h"
#include "content/browser/indexed_db/instance/backing_store_util.h"
#include "content/browser/indexed_db/instance/bucket_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key_path.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_metadata.h"

using blink::IndexedDBIndexMetadata;
using blink::IndexedDBKey;
using blink::IndexedDBKeyPath;
using blink::IndexedDBKeyRange;
using blink::IndexedDBObjectStoreMetadata;

namespace {

enum class ExternalObjectTestType {
  kOnlyBlobs,
  kOnlyFileSystemAccessHandles,
  kBlobsAndFileSystemAccessHandles
};

}  // namespace

namespace content::indexed_db {

// Tests the backend-agnostic behavior of the backing store. Currently, these
// tests verify the correct behavior for both SQLite and LevelDB backing store
// implementations.
class BackingStoreTest : public testing::WithParamInterface<bool>,
                         public BackingStoreTestBase {
 public:
  BackingStoreTest() : BackingStoreTestBase(IsSqliteBackingStoreEnabled()) {}

  BackingStoreTest(const BackingStoreTest&) = delete;
  BackingStoreTest& operator=(const BackingStoreTest&) = delete;

  bool IsSqliteBackingStoreEnabled() { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    BackingStoreTest,
    testing::Bool(),
    [](const testing::TestParamInfo<BackingStoreTest::ParamType>& info) {
      return info.param ? "Sqlite" : "LevelDb";
    });

TEST_P(BackingStoreTest, PutGetConsistency) {
  const IndexedDBKey& key = key1_;
  IndexedDBValue& value = value1_;

  auto db_creation_result = backing_store()->CreateOrOpenDatabase(u"name");
  ASSERT_TRUE(db_creation_result.has_value());
  BackingStore::Database& db = **db_creation_result;

  {
    std::unique_ptr<BackingStore::Transaction> transaction1 =
        db.CreateTransaction(blink::mojom::IDBTransactionDurability::Relaxed,
                             blink::mojom::IDBTransactionMode::ReadWrite);

    transaction1->Begin(CreateDummyLock());
    EXPECT_TRUE(transaction1->PutRecord(1, key, value.Clone()).has_value());
    CommitTransactionAndVerify(*transaction1);
  }

  {
    std::unique_ptr<BackingStore::Transaction> transaction2 =
        db.CreateTransaction(blink::mojom::IDBTransactionDurability::Relaxed,
                             blink::mojom::IDBTransactionMode::ReadWrite);

    transaction2->Begin(CreateDummyLock());
    auto result = transaction2->GetRecord(1, key);
    EXPECT_TRUE(result.has_value());
    CommitTransactionAndVerify(*transaction2);
    EXPECT_EQ(base::span(value.bits), base::span(result->bits));
  }
}

// Tests what happens when a blob returns an error when being read.
TEST_P(BackingStoreTest, PutBrokenBlob) {
  const IndexedDBKey& key = key1_;
  IndexedDBValue& value = value1_;

  // Make a `FakeBlob` with no body (not an empty body), which will return an
  // error when read.
  value.external_objects.emplace_back(
      CreateBlobInfo(u"text/plain", std::nullopt));

  auto db_creation_result = backing_store()->CreateOrOpenDatabase(u"name");
  ASSERT_TRUE(db_creation_result.has_value());
  BackingStore::Database& db = **db_creation_result;

  std::unique_ptr<BackingStore::Transaction> transaction =
      db.CreateTransaction(blink::mojom::IDBTransactionDurability::Relaxed,
                           blink::mojom::IDBTransactionMode::ReadWrite);

  transaction->Begin(CreateDummyLock());
  EXPECT_TRUE(transaction->PutRecord(1, key, value.Clone()).has_value());
  EXPECT_FALSE(CommitTransactionPhaseOneAndVerify(*transaction));
  transaction->Rollback();
}

// Tests what happens when a transaction is being committed and a blob is being
// written (asynchronously) when Rollback() is invoked.
TEST_P(BackingStoreTest, RollbackDuringBlobWrite) {
  const IndexedDBKey& key = key1_;
  IndexedDBValue& value = value1_;
  value.external_objects.emplace_back(
      CreateBlobInfo(u"text/plain", "contents"));

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<BackingStore::Database> db,
                       backing_store()->CreateOrOpenDatabase(u"name"));
  ASSERT_TRUE(db.get());

  std::unique_ptr<BackingStore::Transaction> transaction =
      db->CreateTransaction(blink::mojom::IDBTransactionDurability::Relaxed,
                            blink::mojom::IDBTransactionMode::ReadWrite);

  transaction->Begin(CreateDummyLock());
  EXPECT_TRUE(transaction->PutRecord(1, key, value.Clone()).has_value());

  bool blob_write_callback_lives = false;
  EXPECT_TRUE(
      transaction
          ->CommitPhaseOne(
              /*blob_write_callback=*/
              base::IgnoreArgs<StatusOr<BlobWriteResult>>(base::BindOnce(
                  [](base::AutoReset<bool> auto_reset) {
                    ADD_FAILURE();
                    return Status::OK();
                  },
                  base::AutoReset(&blob_write_callback_lives, true))),
              /*serialize_fsa_handle=*/base::DoNothing())
          .ok());
  EXPECT_TRUE(blob_write_callback_lives);
  transaction->Rollback();

  // Make sure the blob write callback was dropped without being called. If
  // called, it will cause the test to fail with ADD_FAILURE().
  EXPECT_FALSE(blob_write_callback_lives);

  // Make sure there are no other errors as the backing store potentially
  // attempts to write blobs in the background. In particular, the LevelDB
  // store has to explicitly handle the Rollback case to prevent a crash, and
  // spinning a runloop is necessary to give it a chance to not crash.
  base::RunLoop().RunUntilIdle();
  base::RunLoop().RunUntilIdle();
}

TEST_P(BackingStoreTest, Snapshots) {
  auto db_creation_result = backing_store()->CreateOrOpenDatabase(u"name");
  ASSERT_TRUE(db_creation_result.has_value());
  BackingStore::Database& db = **db_creation_result;

  StatusOr<base::DictValue> empty_snapshot = SnapshotDatabase(db);
  ASSERT_TRUE(empty_snapshot.has_value());

  {
    std::unique_ptr<BackingStore::Transaction> transaction =
        CreateAndBeginTransaction(
            db, blink::mojom::IDBTransactionMode::VersionChange);

    EXPECT_TRUE(transaction
                    ->CreateObjectStore(1, u"object_store_name",
                                        IndexedDBKeyPath(u"object_store_key"),
                                        /*auto_increment=*/true)
                    .ok());
    CommitTransactionAndVerify(*transaction);
  }

  int total_record_count = 0;
  auto add_records = [&](size_t num_records) {
    std::unique_ptr<BackingStore::Transaction> transaction =
        db.CreateTransaction(blink::mojom::IDBTransactionDurability::Relaxed,
                             blink::mojom::IDBTransactionMode::ReadWrite);

    transaction->Begin(CreateDummyLock());

    for (size_t i = 0; i < num_records; ++i) {
      IndexedDBKey key(i + total_record_count,
                       blink::mojom::IDBKeyType::Number);
      EXPECT_TRUE(transaction->PutRecord(1, key, value1_.Clone()).has_value());
    }
    total_record_count += num_records;
    CommitTransactionAndVerify(*transaction);
  };
  add_records(100);

  StatusOr<base::DictValue> snapshot = SnapshotDatabase(db);
  ASSERT_TRUE(snapshot.has_value());

  // Adding a record changes the snapshot.
  add_records(3);
  StatusOr<base::DictValue> snapshot2 = SnapshotDatabase(db);
  ASSERT_TRUE(snapshot2.has_value());
  EXPECT_NE(*snapshot, *snapshot2);

  // Updating a value changes the snapshot.
  {
    std::unique_ptr<BackingStore::Transaction> transaction =
        db.CreateTransaction(blink::mojom::IDBTransactionDurability::Relaxed,
                             blink::mojom::IDBTransactionMode::ReadWrite);

    transaction->Begin(CreateDummyLock());

    IndexedDBKey key(15, blink::mojom::IDBKeyType::Number);
    EXPECT_TRUE(transaction->PutRecord(1, key, value2_.Clone()).has_value());
    CommitTransactionAndVerify(*transaction);
  };

  StatusOr<base::DictValue> snapshot3 = SnapshotDatabase(db);
  ASSERT_TRUE(snapshot3.has_value());
  EXPECT_NE(*snapshot2, *snapshot3);

  // Changing the updated value back to the original, snapshot should revert
  // too.
  {
    std::unique_ptr<BackingStore::Transaction> transaction =
        db.CreateTransaction(blink::mojom::IDBTransactionDurability::Relaxed,
                             blink::mojom::IDBTransactionMode::ReadWrite);

    transaction->Begin(CreateDummyLock());

    IndexedDBKey key(15, blink::mojom::IDBKeyType::Number);
    EXPECT_TRUE(transaction->PutRecord(1, key, value1_.Clone()).has_value());
    CommitTransactionAndVerify(*transaction);
  };
  StatusOr<base::DictValue> snapshot4 = SnapshotDatabase(db);
  ASSERT_TRUE(snapshot4.has_value());
  EXPECT_EQ(*snapshot2, *snapshot4);

  // Exercise the whole-store hashing code.
  add_records(1000);
  StatusOr<base::DictValue> snapshot5 = SnapshotDatabase(db);
  ASSERT_TRUE(snapshot5.has_value());
  EXPECT_LT(snapshot5->DebugString().size(), snapshot4->DebugString().size());

  add_records(2);
  StatusOr<base::DictValue> snapshot6 = SnapshotDatabase(db);
  ASSERT_TRUE(snapshot6.has_value());
  EXPECT_NE(*snapshot5, *snapshot6);
  // Size should not have changed since the row would change the digest but not
  // the size of the digest. Note that this sort of cheats because the digest is
  // actually omitted from the debug string due to being a binary, but even if
  // we were to encode it as a string (e.g. with base64), this check would pass.
  EXPECT_EQ(snapshot6->DebugString().size(), snapshot5->DebugString().size());

  // Delete all records and verify the snapshot works, and is distinct from the
  // one for a database that lacks object stores/indices.
  {
    std::unique_ptr<BackingStore::Transaction> transaction =
        db.CreateTransaction(blink::mojom::IDBTransactionDurability::Relaxed,
                             blink::mojom::IDBTransactionMode::ReadWrite);

    transaction->Begin(CreateDummyLock());

    EXPECT_TRUE(transaction->DeleteRange(1, blink::IndexedDBKeyRange()).ok());
    CommitTransactionAndVerify(*transaction);
  };
  StatusOr<base::DictValue> no_record_snapshot = SnapshotDatabase(db);
  ASSERT_TRUE(no_record_snapshot.has_value());
  EXPECT_NE(*empty_snapshot, *no_record_snapshot);
}

// Deleting an index should delete the index metadata and the index data.
TEST_P(BackingStoreTest, CreateAndDeleteIndex) {
  const int64_t object_store_id = 99;
  const IndexedDBKeyPath object_store_key_path(u"object_store_key");

  const int64_t index_id = 999;
  const IndexedDBKeyPath index_key_path(u"index_key");

  auto db = backing_store()->CreateOrOpenDatabase(u"database_name");
  ASSERT_TRUE(db.has_value());

  {
    std::unique_ptr<BackingStore::Transaction> transaction =
        CreateAndBeginTransaction(
            **db, blink::mojom::IDBTransactionMode::VersionChange);
    EXPECT_TRUE(transaction
                    ->CreateObjectStore(object_store_id, u"object_store_name",
                                        object_store_key_path,
                                        /*auto_increment=*/true)
                    .ok());

    EXPECT_TRUE(
        transaction
            ->CreateIndex(object_store_id, blink::IndexedDBIndexMetadata(
                                               u"index_name", index_id,
                                               index_key_path, /*unique=*/true,
                                               /*multi_entry=*/true))
            .ok());

    CommitTransactionAndVerify(*transaction);
  }

  EXPECT_EQ((*db)->GetMetadata().object_stores.size(), 1U);
  auto object_store_it =
      (*db)->GetMetadata().object_stores.find(object_store_id);
  ASSERT_NE(object_store_it, (*db)->GetMetadata().object_stores.end());
  const IndexedDBObjectStoreMetadata& object_store = object_store_it->second;
  EXPECT_NE(object_store.indexes.end(), object_store.indexes.find(index_id));

  {
    auto transaction = CreateAndBeginTransaction(
        **db, blink::mojom::IDBTransactionMode::VersionChange);

    auto record =
        transaction->PutRecord(object_store_id, key1_, value1_.Clone());
    EXPECT_TRUE(record.has_value());
    EXPECT_TRUE(
        transaction
            ->PutIndexDataForRecord(object_store_id, index_id, key2_, *record)
            .ok());
    auto pk = transaction->GetFirstPrimaryKeyForIndexKey(object_store_id,
                                                         index_id, key2_);
    EXPECT_TRUE(pk.has_value());
    EXPECT_TRUE(pk->IsValid());

    EXPECT_TRUE(transaction->DeleteIndex(object_store_id, index_id).ok());

    // The SQLite backing store CHECKs on invalid inputs, such as id which
    // refers to now-deleted index.
    if (!IsSqliteBackingStoreEnabled()) {
      pk = transaction->GetFirstPrimaryKeyForIndexKey(object_store_id, index_id,
                                                      key2_);
      EXPECT_TRUE(pk.has_value());
      EXPECT_FALSE(pk->IsValid());
    }

    CommitTransactionAndVerify(*transaction);
  }

  EXPECT_EQ(object_store.indexes.end(), object_store.indexes.find(index_id));
}

TEST_P(BackingStoreTest, CreateDatabase) {
  const std::u16string database_name(u"db1");
  const int64_t version = 9;

  const int64_t object_store_id = 99;
  const std::u16string object_store_name(u"object_store1");
  const bool auto_increment = true;
  const IndexedDBKeyPath object_store_key_path(u"object_store_key");

  const int64_t index_id = 999;
  const std::u16string index_name(u"index1");
  const bool unique = true;
  const bool multi_entry = true;
  const IndexedDBKeyPath index_key_path(u"index_key");

  {
    auto db1 = backing_store()->CreateOrOpenDatabase(database_name);
    ASSERT_TRUE(db1.has_value());
    UpdateDatabaseVersion(**db1, version);

    std::unique_ptr<indexed_db::BackingStore::Transaction> transaction =
        (*db1)->CreateTransaction(
            blink::mojom::IDBTransactionDurability::Relaxed,
            blink::mojom::IDBTransactionMode::VersionChange);
    transaction->Begin(CreateDummyLock());

    Status s =
        transaction->CreateObjectStore(object_store_id, object_store_name,
                                       object_store_key_path, auto_increment);
    EXPECT_TRUE(s.ok());

    const IndexedDBObjectStoreMetadata& object_store =
        (*db1)->GetMetadata().object_stores.find(object_store_id)->second;
    EXPECT_EQ(object_store.id, object_store_id);

    s = transaction->CreateIndex(
        object_store.id,
        blink::IndexedDBIndexMetadata(index_name, index_id, index_key_path,
                                      unique, multi_entry));
    EXPECT_TRUE(s.ok());

    const IndexedDBIndexMetadata& index =
        object_store.indexes.find(index_id)->second;
    EXPECT_EQ(index.id, index_id);
    CommitTransactionAndVerify(*transaction);
  }

  {
    auto db1 = backing_store()->CreateOrOpenDatabase(database_name);
    EXPECT_TRUE(db1.has_value());

    const blink::IndexedDBDatabaseMetadata& database = (*db1)->GetMetadata();

    EXPECT_EQ(1UL, database.object_stores.size());
    const IndexedDBObjectStoreMetadata& object_store =
        database.object_stores.find(object_store_id)->second;
    EXPECT_EQ(object_store_name, object_store.name);
    EXPECT_EQ(object_store_key_path, object_store.key_path);
    EXPECT_EQ(auto_increment, object_store.auto_increment);

    EXPECT_EQ(1UL, object_store.indexes.size());
    const IndexedDBIndexMetadata& index =
        object_store.indexes.find(index_id)->second;
    EXPECT_EQ(index_name, index.name);
    EXPECT_EQ(index_key_path, index.key_path);
    EXPECT_EQ(unique, index.unique);
    EXPECT_EQ(multi_entry, index.multi_entry);
  }
}

TEST_P(BackingStoreTest, DatabaseExists) {
  auto db1 = backing_store()->CreateOrOpenDatabase(u"db1");
  ASSERT_TRUE(db1.has_value());

  auto db2 = backing_store()->CreateOrOpenDatabase(u"db2");
  ASSERT_TRUE(db2.has_value());

  // Only databases with non-default versions should be counted as existing by
  // `DatabaseExists()`.
  UpdateDatabaseVersion(*db1.value(), 1);

  StatusOr<bool> db1_exists = backing_store()->DatabaseExists(u"db1");
  ASSERT_TRUE(db1_exists.has_value());
  EXPECT_TRUE(*db1_exists);

  StatusOr<bool> db2_exists = backing_store()->DatabaseExists(u"db2");
  ASSERT_TRUE(db2_exists.has_value());
  EXPECT_FALSE(*db2_exists);
}

TEST_P(BackingStoreTest, DatabaseNamesAreSorted) {
  // Hold on to one of the created databases.
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<BackingStore::Database> db,
                       backing_store()->CreateOrOpenDatabase(u"bb"));
  UpdateDatabaseVersion(*db, 1);

  // Create a couple of other databases but immediately close them.
  UpdateDatabaseVersion(**backing_store()->CreateOrOpenDatabase(u"c"), 1);
  UpdateDatabaseVersion(**backing_store()->CreateOrOpenDatabase(u"aaa"), 1);

  // Database names should be in sorted order.
  ASSERT_OK_AND_ASSIGN(
      std::vector<blink::mojom::IDBNameAndVersionPtr> names_and_versions,
      backing_store()->GetDatabaseNamesAndVersions());
  ASSERT_EQ(names_and_versions.size(), 3U);
  EXPECT_EQ(names_and_versions[0]->name, u"aaa");
  EXPECT_EQ(names_and_versions[1]->name, u"bb");
  EXPECT_EQ(names_and_versions[2]->name, u"c");
}

class BackingStoreTestWithExternalObjects
    : public testing::WithParamInterface<
          std::tuple<bool, ExternalObjectTestType>>,
      public BackingStoreWithExternalObjectsTestBase {
 public:
  BackingStoreTestWithExternalObjects()
      : BackingStoreWithExternalObjectsTestBase(IsSqliteBackingStoreEnabled()) {
  }

  BackingStoreTestWithExternalObjects(
      const BackingStoreTestWithExternalObjects&) = delete;
  BackingStoreTestWithExternalObjects& operator=(
      const BackingStoreTestWithExternalObjects&) = delete;

  bool IsSqliteBackingStoreEnabled() { return std::get<0>(GetParam()); }
  ExternalObjectTestType TestType() { return std::get<1>(GetParam()); }

  bool IncludesBlobs() override {
    return TestType() != ExternalObjectTestType::kOnlyFileSystemAccessHandles;
  }

  bool IncludesFileSystemAccessHandles() override {
    return TestType() != ExternalObjectTestType::kOnlyBlobs;
  }
};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    BackingStoreTestWithExternalObjects,
    testing::Combine(
        testing::Bool(),
        testing::Values(
            ExternalObjectTestType::kOnlyBlobs,
            ExternalObjectTestType::kOnlyFileSystemAccessHandles,
            ExternalObjectTestType::kBlobsAndFileSystemAccessHandles)),
    [](const testing::TestParamInfo<
        BackingStoreTestWithExternalObjects::ParamType>& info) {
      std::string external_object_type;
      switch (std::get<1>(info.param)) {
        case ExternalObjectTestType::kOnlyBlobs:
          external_object_type = "Blobs";
          break;
        case ExternalObjectTestType::kOnlyFileSystemAccessHandles:
          external_object_type = "FileSystemAccessHandles";
          break;
        case ExternalObjectTestType::kBlobsAndFileSystemAccessHandles:
          external_object_type = "BlobsAndFileSystemAccessHandles";
          break;
      }
      return base::StrCat({std::get<0>(info.param) ? "Sqlite" : "LevelDb",
                           external_object_type});
    });

TEST_P(BackingStoreTestWithExternalObjects, PutGetConsistency) {
  auto db_creation_result = backing_store()->CreateOrOpenDatabase(u"name");
  ASSERT_TRUE(db_creation_result.has_value());
  BackingStore::Database& db = **db_creation_result;
  {
    // Initiate transaction1 - writing blobs.
    auto transaction1 =
        db.CreateTransaction(blink::mojom::IDBTransactionDurability::Relaxed,
                             blink::mojom::IDBTransactionMode::ReadWrite);

    transaction1->Begin(CreateDummyLock());
    EXPECT_TRUE(transaction1->PutRecord(1, key3_, value3_.Clone()).has_value());
    CommitTransactionAndVerify(*transaction1);
  }

  // Initiate transaction2, reading blobs.
  {
    auto transaction2 =
        db.CreateTransaction(blink::mojom::IDBTransactionDurability::Relaxed,
                             blink::mojom::IDBTransactionMode::ReadWrite);
    // auto& transaction2 = *txn2;
    transaction2->Begin(CreateDummyLock());
    auto result = transaction2->GetRecord(1, key3_);
    EXPECT_TRUE(result.has_value());
    IndexedDBValue result_value = std::move(result.value());

    CommitTransactionAndVerify(*transaction2);
    EXPECT_EQ(base::span(value3_.bits), base::span(result_value.bits));

    EXPECT_TRUE(CheckBlobInfoMatches(result_value.external_objects));
  }

  // Initiate transaction3, deleting blobs.
  {
    auto transaction3 =
        db.CreateTransaction(blink::mojom::IDBTransactionDurability::Relaxed,
                             blink::mojom::IDBTransactionMode::ReadWrite);

    transaction3->Begin(CreateDummyLock());
    EXPECT_TRUE(
        transaction3
            ->DeleteRange(1, IndexedDBKeyRange(key3_.Clone(), key3_.Clone(),
                                               /*lower_open=*/false,
                                               /*upper_open=*/false))
            .ok());
    CommitTransactionAndVerify(*transaction3);
  }

  // Verify deletes
  {
    auto transaction4 =
        db.CreateTransaction(blink::mojom::IDBTransactionDurability::Relaxed,
                             blink::mojom::IDBTransactionMode::ReadWrite);

    transaction4->Begin(CreateDummyLock());
    auto result = transaction4->GetRecord(1, key3_);
    EXPECT_TRUE(result.has_value());
    IndexedDBValue result_value = std::move(result.value());

    CommitTransactionAndVerify(*transaction4);
    EXPECT_TRUE(result_value.empty());
  }
}

TEST_P(BackingStoreTestWithExternalObjects, DeleteRange) {
  auto db_creation_result = backing_store()->CreateOrOpenDatabase(u"name");
  ASSERT_TRUE(db_creation_result.has_value());
  BackingStore::Database& db = **db_creation_result;

  const auto keys =
      std::to_array({IndexedDBKey(u"key0"), IndexedDBKey(u"key1"),
                     IndexedDBKey(u"key2"), IndexedDBKey(u"key3")});

  // All of these delete ranges should result in the deletion of key1 and key2.
  const auto ranges = std::to_array({
      IndexedDBKeyRange(keys[1].Clone(), keys[2].Clone(), /*lower_open=*/false,
                        /*upper_open=*/false),
      IndexedDBKeyRange(keys[0].Clone(), keys[2].Clone(), /*lower_open=*/true,
                        /*upper_open=*/false),
      IndexedDBKeyRange(keys[1].Clone(), keys[3].Clone(), /*lower_open=*/false,
                        /*upper_open=*/true),
      IndexedDBKeyRange(keys[0].Clone(), keys[3].Clone(), /*lower_open=*/true,
                        /*upper_open=*/true),
  });

  for (size_t i = 0; i < std::size(ranges); ++i) {
    const int64_t object_store_id = i + 1;
    const IndexedDBKeyRange& range = ranges[i];

    std::vector<IndexedDBExternalObject> external_objects;
    for (size_t j = 0; j < 4; ++j) {
      std::string type = "type " + base::NumberToString(j);
      std::string payload = "payload " + base::NumberToString(j);
      external_objects.push_back(
          CreateBlobInfo(base::UTF8ToUTF16(type), payload));
    }

    // Reset from previous iteration.
    blob_context_->ClearWrites();
    file_system_access_context_->ClearWrites();

    auto values = std::to_array({
        IndexedDBValue("value0", {external_objects[0]}),
        IndexedDBValue("value1", {external_objects[1]}),
        IndexedDBValue("value2", {external_objects[2]}),
        IndexedDBValue("value3", {external_objects[3]}),
    });
    ASSERT_GE(keys.size(), values.size());

    {
      // Initiate transaction1 - write records.
      auto transaction1 =
          db.CreateTransaction(blink::mojom::IDBTransactionDurability::Relaxed,
                               blink::mojom::IDBTransactionMode::ReadWrite);
      transaction1->Begin(CreateDummyLock());
      BackingStore::RecordIdentifier record;
      for (size_t j = 0; j < values.size(); ++j) {
        EXPECT_TRUE(
            transaction1->PutRecord(object_store_id, keys[j], values[j].Clone())
                .has_value());
      }

      // Start committing transaction1.
      CommitTransactionAndVerify(*transaction1);
    }

    {
      // Initiate transaction 2 - delete range.
      auto transaction2 =
          db.CreateTransaction(blink::mojom::IDBTransactionDurability::Relaxed,
                               blink::mojom::IDBTransactionMode::ReadWrite);
      transaction2->Begin(CreateDummyLock());
      EXPECT_TRUE(transaction2->DeleteRange(object_store_id, range).ok());

      // Start committing transaction2.
      CommitTransactionAndVerify(*transaction2);
    }

    // Verify deletes
    for (size_t j = 0; j < keys.size(); ++j) {
      {
        auto transaction = db.CreateTransaction(
            blink::mojom::IDBTransactionDurability::Relaxed,
            blink::mojom::IDBTransactionMode::ReadWrite);
        transaction->Begin(CreateDummyLock());
        auto result = transaction->GetRecord(object_store_id, keys[j]);
        EXPECT_TRUE(result.has_value());
        IndexedDBValue result_value = std::move(result.value());

        CommitTransactionAndVerify(*transaction);

        if (j == 1 || j == 2) {
          EXPECT_TRUE(result_value.empty());
        } else {
          EXPECT_FALSE(result_value.empty());
          EXPECT_EQ(base::span(values[j].bits), base::span(result_value.bits));
        }
      }
    }
  }
}

TEST_P(BackingStoreTestWithExternalObjects, DeleteRangeEmptyRange) {
  auto db_creation_result = backing_store()->CreateOrOpenDatabase(u"name");
  ASSERT_TRUE(db_creation_result.has_value());
  BackingStore::Database& db = **db_creation_result;

  const auto keys = std::to_array({
      IndexedDBKey(u"key0"),
      IndexedDBKey(u"key1"),
      IndexedDBKey(u"key2"),
      IndexedDBKey(u"key3"),
      IndexedDBKey(u"key4"),
  });
  const auto ranges = std::to_array({
      IndexedDBKeyRange(keys[3].Clone(), keys[4].Clone(), /*lower_open=*/true,
                        /*upper_open=*/false),
      IndexedDBKeyRange(keys[2].Clone(), keys[1].Clone(), /*lower_open=*/false,
                        /*upper_open=*/false),
      IndexedDBKeyRange(keys[2].Clone(), keys[1].Clone(), /*lower_open=*/true,
                        /*upper_open=*/true),
  });

  for (size_t i = 0; i < std::size(ranges); ++i) {
    const int64_t object_store_id = i + 1;
    const IndexedDBKeyRange& range = ranges[i];

    std::vector<IndexedDBExternalObject> external_objects;
    for (size_t j = 0; j < 4; ++j) {
      std::string type = "type " + base::NumberToString(j);
      std::string payload = "payload " + base::NumberToString(j);
      external_objects.push_back(
          CreateBlobInfo(base::UTF8ToUTF16(type), payload));
    }

    // Reset from previous iteration.
    blob_context_->ClearWrites();
    file_system_access_context_->ClearWrites();

    const auto values = std::to_array({
        IndexedDBValue("value0", {external_objects[0]}),
        IndexedDBValue("value1", {external_objects[1]}),
        IndexedDBValue("value2", {external_objects[2]}),
        IndexedDBValue("value3", {external_objects[3]}),
    });
    ASSERT_GE(keys.size(), values.size());

    {
      // Initiate transaction1 - write records.
      auto transaction1 =
          db.CreateTransaction(blink::mojom::IDBTransactionDurability::Relaxed,
                               blink::mojom::IDBTransactionMode::ReadWrite);
      transaction1->Begin(CreateDummyLock());

      for (size_t j = 0; j < values.size(); ++j) {
        EXPECT_TRUE(
            transaction1->PutRecord(object_store_id, keys[j], values[j].Clone())
                .has_value());
      }
      // Start committing transaction1.
      CommitTransactionAndVerify(*transaction1);
    }

    // Initiate transaction 2 - delete range.
    {
      auto transaction2 =
          db.CreateTransaction(blink::mojom::IDBTransactionDurability::Relaxed,
                               blink::mojom::IDBTransactionMode::ReadWrite);
      transaction2->Begin(CreateDummyLock());
      EXPECT_TRUE(transaction2->DeleteRange(object_store_id, range).ok());

      CommitTransactionAndVerify(*transaction2);
    }

    // Verify that no records were deleted.
    for (size_t j = 0; j < values.size(); ++j) {
      {
        auto transaction3 = db.CreateTransaction(
            blink::mojom::IDBTransactionDurability::Relaxed,
            blink::mojom::IDBTransactionMode::ReadWrite);
        transaction3->Begin(CreateDummyLock());
        auto result = transaction3->GetRecord(object_store_id, keys[j]);
        EXPECT_TRUE(result.has_value());
        IndexedDBValue result_value = std::move(result.value());

        CommitTransactionAndVerify(*transaction3);

        // No records should have been deleted.
        EXPECT_FALSE(result_value.empty());
        EXPECT_EQ(base::span(values[j].bits), base::span(result_value.bits));
      }
    }
  }
}

// This tests that external objects are deleted when ClearObjectStore is called.
// See: http://crbug.com/488851
// TODO(enne): we could use more comprehensive testing for ClearObjectStore.
TEST_P(BackingStoreTestWithExternalObjects, ClearObjectStoreObjects) {
  std::vector<IndexedDBExternalObject> external_objects;
  for (size_t j = 0; j < 4; ++j) {
    std::string type = "type " + base::NumberToString(j);
    std::string payload = "payload " + base::NumberToString(j);
    external_objects.push_back(
        CreateBlobInfo(base::UTF8ToUTF16(type), payload));
  }

  const auto keys =
      std::to_array({IndexedDBKey(u"key0"), IndexedDBKey(u"key1"),
                     IndexedDBKey(u"key2"), IndexedDBKey(u"key3")});

  const auto values = std::to_array({
      IndexedDBValue("value0", {external_objects[0]}),
      IndexedDBValue("value1", {external_objects[1]}),
      IndexedDBValue("value2", {external_objects[2]}),
      IndexedDBValue("value3", {external_objects[3]}),
  });
  ASSERT_GE(keys.size(), values.size());

  const int64_t object_store_id = 999;

  auto db_creation_result = backing_store()->CreateOrOpenDatabase(u"name");
  ASSERT_TRUE(db_creation_result.has_value());
  BackingStore::Database& db = **db_creation_result;

  // Create two object stores, to verify that only one gets deleted.
  for (size_t i = 0; i < 2; ++i) {
    const int64_t write_object_store_id = object_store_id + i;

    {  // Initiate transaction1 - write records.
      auto transaction1 =
          db.CreateTransaction(blink::mojom::IDBTransactionDurability::Relaxed,
                               blink::mojom::IDBTransactionMode::ReadWrite);
      transaction1->Begin(CreateDummyLock());
      for (size_t j = 0; j < values.size(); ++j) {
        EXPECT_TRUE(
            transaction1
                ->PutRecord(write_object_store_id, keys[j], values[j].Clone())
                .has_value());
      }

      CommitTransactionAndVerify(*transaction1);
    }
  }

  // Initiate transaction 2 - delete object store
  {
    auto transaction2 =
        db.CreateTransaction(blink::mojom::IDBTransactionDurability::Relaxed,
                             blink::mojom::IDBTransactionMode::ReadWrite);
    transaction2->Begin(CreateDummyLock());
    IndexedDBValue result_value;
    EXPECT_TRUE(transaction2->ClearObjectStore(object_store_id).ok());

    // Start committing transaction2.
    CommitTransactionAndVerify(*transaction2);
  }

  // Verify that all blobs were removed.
  for (size_t j = 0; j < values.size(); ++j) {
    {
      auto transaction3 =
          db.CreateTransaction(blink::mojom::IDBTransactionDurability::Relaxed,
                               blink::mojom::IDBTransactionMode::ReadWrite);
      transaction3->Begin(CreateDummyLock());
      auto result = transaction3->GetRecord(object_store_id, keys[j]);
      EXPECT_TRUE(result.has_value());
      IndexedDBValue result_value = std::move(result.value());

      CommitTransactionAndVerify(*transaction3);
      EXPECT_TRUE(result_value.empty());
    }
  }
}

}  // namespace content::indexed_db
