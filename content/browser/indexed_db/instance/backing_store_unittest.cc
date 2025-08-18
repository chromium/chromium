// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/backing_store.h"

#include <memory>
#include <string>

#include "content/browser/indexed_db/indexed_db_value.h"
#include "content/browser/indexed_db/instance/backing_store_test_base.h"
#include "content/browser/indexed_db/instance/bucket_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key_path.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_metadata.h"

using blink::IndexedDBIndexMetadata;
using blink::IndexedDBKey;
using blink::IndexedDBKeyPath;
using blink::IndexedDBObjectStoreMetadata;

namespace content::indexed_db {

// Tests the backend-agnostic behavior of the backing store. Currently, these
// tests verify the correct behavior for both SQLite and LevelDB backing store
// implementations.
class BackingStoreTest : public testing::WithParamInterface<bool>,
                         public BackingStoreTestBase {
 public:
  BackingStoreTest()
      : sqlite_override_(BucketContext::OverrideShouldUseSqliteForTesting(
            IsSqliteBackingStoreEnabled())) {}

  BackingStoreTest(const BackingStoreTest&) = delete;
  BackingStoreTest& operator=(const BackingStoreTest&) = delete;

  bool IsSqliteBackingStoreEnabled() { return GetParam(); }

 private:
  base::AutoReset<std::optional<bool>> sqlite_override_;
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
    bool succeeded = false;
    EXPECT_TRUE(
        transaction1
            ->CommitPhaseOne(
                MockBlobStorageContext::CreateBlobWriteCallback(&succeeded),
                base::DoNothing())
            .ok());
    EXPECT_TRUE(succeeded);
    EXPECT_TRUE(transaction1->CommitPhaseTwo().ok());
  }

  {
    std::unique_ptr<BackingStore::Transaction> transaction2 =
        db.CreateTransaction(blink::mojom::IDBTransactionDurability::Relaxed,
                             blink::mojom::IDBTransactionMode::ReadWrite);

    transaction2->Begin(CreateDummyLock());
    auto result = transaction2->GetRecord(1, key);
    EXPECT_TRUE(result.has_value());
    bool succeeded = false;
    EXPECT_TRUE(
        transaction2
            ->CommitPhaseOne(
                MockBlobStorageContext::CreateBlobWriteCallback(&succeeded),
                base::DoNothing())
            .ok());
    EXPECT_TRUE(succeeded);
    EXPECT_TRUE(transaction2->CommitPhaseTwo().ok());
    EXPECT_EQ(value.bits, result->bits);
  }
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

    CommitTransaction(*transaction);
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
    pk = transaction->GetFirstPrimaryKeyForIndexKey(object_store_id, index_id,
                                                    key2_);
    EXPECT_TRUE(pk.has_value());
    EXPECT_FALSE(pk->IsValid());

    CommitTransaction(*transaction);
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

    bool succeeded = false;
    EXPECT_TRUE(
        transaction
            ->CommitPhaseOne(
                MockBlobStorageContext::CreateBlobWriteCallback(&succeeded),
                base::DoNothing())
            .ok());
    EXPECT_TRUE(succeeded);
    EXPECT_TRUE(transaction->CommitPhaseTwo().ok());
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

}  // namespace content::indexed_db
