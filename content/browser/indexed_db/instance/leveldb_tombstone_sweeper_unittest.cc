// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/leveldb_tombstone_sweeper.h"

#include <memory>
#include <string>
#include <utility>

#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "components/services/storage/indexed_db/leveldb/mock_level_db.h"
#include "components/services/storage/indexed_db/scopes/leveldb_scopes.h"
#include "components/services/storage/indexed_db/scopes/varint_coding.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_database.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_factory.h"
#include "content/browser/indexed_db/indexed_db_leveldb_operations.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key_path.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_metadata.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/leveldb_chrome.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/filter_policy.h"
#include "third_party/leveldatabase/src/include/leveldb/slice.h"

namespace content::indexed_db {

using ::testing::_;
using ::testing::Eq;
using ::testing::Return;
using ::testing::StrictMock;
using blink::IndexedDBDatabaseMetadata;
using blink::IndexedDBIndexMetadata;
using blink::IndexedDBKey;
using blink::IndexedDBKeyPath;
using blink::IndexedDBObjectStoreMetadata;

constexpr int kRoundIterations = 11;
constexpr int kMaxIterations = 100;

constexpr int64_t kDb1 = 1;
constexpr int64_t kDb2 = 1;
constexpr int64_t kOs1 = 3;
constexpr int64_t kOs2 = 5;
constexpr int64_t kOs3 = 8;
constexpr int64_t kOs4 = 9;
constexpr int64_t kIndex1 = 31;
constexpr int64_t kIndex2 = 32;
constexpr int64_t kIndex3 = 35;

MATCHER_P(SliceEq,
          str,
          std::string(negation ? "isn't" : "is") + " equal to " +
              base::HexEncode(str.data(), str.size())) {
  *result_listener << "which is " << base::HexEncode(arg.data(), arg.size());
  return std::string(arg.data(), arg.size()) == str;
}

leveldb_env::Options GetLevelDBOptions() {
  leveldb_env::Options options;
  options.comparator = indexed_db::GetDefaultLevelDBComparator();
  options.create_if_missing = true;
  options.write_buffer_size = 4 * 1024 * 1024;
  options.paranoid_checks = true;

  static base::NoDestructor<leveldb_env::ChromiumEnv> g_leveldb_env;
  options.env = g_leveldb_env.get();

  return options;
}

class LevelDbTombstoneSweeperTest : public testing::Test {
 public:
  LevelDbTombstoneSweeperTest() = default;
  ~LevelDbTombstoneSweeperTest() override = default;

  void PopulateMultiDBMetdata() {
    // db1
    //   os1
    //   os2
    //     index1
    //     index2
    metadata_.emplace_back(u"db1", kDb1, 1, 29);
    auto& db1 = metadata_.back();
    db1.object_stores[kOs1] = IndexedDBObjectStoreMetadata(
        u"os1", kOs1, IndexedDBKeyPath(), false, 1000);
    db1.object_stores[kOs2] = IndexedDBObjectStoreMetadata(
        u"os2", kOs2, IndexedDBKeyPath(), false, 1000);
    auto& os2 = db1.object_stores[kOs2];
    os2.indexes[kIndex1] = IndexedDBIndexMetadata(
        u"index1", kIndex1, IndexedDBKeyPath(), true, false);
    os2.indexes[kIndex2] = IndexedDBIndexMetadata(
        u"index2", kIndex2, IndexedDBKeyPath(), true, false);
    // db2
    //   os3
    //     index3
    //   os4
    metadata_.emplace_back(u"db2", kDb2, 1, 29);
    auto& db2 = metadata_.back();
    db2.object_stores[kOs3] = IndexedDBObjectStoreMetadata(
        u"os3", kOs3, IndexedDBKeyPath(), false, 1000);
    db2.object_stores[kOs4] = IndexedDBObjectStoreMetadata(
        u"os4", kOs4, IndexedDBKeyPath(), false, 1000);
    auto& os3 = db2.object_stores[kOs3];
    os3.indexes[kIndex3] = IndexedDBIndexMetadata(
        u"index3", kIndex3, IndexedDBKeyPath(), true, false);
  }

  void PopulateSingleIndexDBMetadata() {
    // db1
    //   os1
    //     index1
    metadata_.emplace_back(u"db1", kDb1, 1, 29);
    auto& db1 = metadata_.back();
    db1.object_stores[kOs1] = IndexedDBObjectStoreMetadata(
        u"os1", kOs1, IndexedDBKeyPath(), false, 1000);
    auto& os2 = db1.object_stores[kOs1];
    os2.indexes[kIndex1] = IndexedDBIndexMetadata(
        u"index1", kIndex1, IndexedDBKeyPath(), true, false);
  }

  void SetupMockDB() {
    sweeper_ = std::make_unique<LevelDbTombstoneSweeper>(
        kRoundIterations, kMaxIterations, &mock_db_);
    sweeper_->SetStartSeedsForTesting(0, 0, 0);
  }

  void SetupRealDB() {
    leveldb_env::Options options = GetLevelDBOptions();
    std::unique_ptr<leveldb::Env> in_memory_env =
        leveldb_chrome::NewMemEnv("in-memory-testing-db", options.env);
    options.env = in_memory_env.get();

    std::unique_ptr<leveldb::DB> db;
    leveldb::Status s = leveldb_env::OpenDB(options, std::string(), &db);
    ASSERT_TRUE(s.ok());
    scoped_refptr<LevelDBState> level_db_state =
        LevelDBState::CreateForInMemoryDB(std::move(in_memory_env),
                                          options.comparator, std::move(db),
                                          "in-memory-testing-db");
    in_memory_db_ = DefaultTransactionalLevelDBFactory().CreateLevelDBDatabase(
        std::move(level_db_state), nullptr, nullptr,
        TransactionalLevelDBDatabase::kDefaultMaxOpenIteratorsPerDatabase);
    sweeper_ = std::make_unique<LevelDbTombstoneSweeper>(
        kRoundIterations, kMaxIterations, in_memory_db_->db());
    sweeper_->SetStartSeedsForTesting(0, 0, 0);
  }

  void ExpectIndexEntry(leveldb::MockIterator& iterator,
                        int64_t db,
                        int64_t os,
                        int64_t index,
                        const IndexedDBKey& index_key,
                        const IndexedDBKey& primary_key,
                        int index_version) {
    testing::InSequence sequence_enforcer;

    EXPECT_CALL(iterator, key())
        .WillOnce(Return(
            IndexDataKey::Encode(db, os, index, index_key, primary_key)));
    std::string value_str;
    EncodeVarInt(index_version, &value_str);
    EncodeIDBKey(primary_key, &value_str);
    EXPECT_CALL(iterator, value()).WillOnce(Return(value_str));
  }

  void ExpectIndexAndExistsEntries(leveldb::MockIterator& iterator,
                                   int64_t db,
                                   int64_t os,
                                   int64_t index,
                                   const IndexedDBKey& index_key,
                                   const IndexedDBKey& primary_key,
                                   int index_version,
                                   int exists_version) {
    ExpectIndexEntry(iterator, db, os, index, index_key, primary_key,
                     index_version);

    testing::InSequence sequence_enforcer;

    std::string encoded_primary_key;
    EncodeIDBKey(primary_key, &encoded_primary_key);

    std::string exists_value;
    EncodeVarInt(exists_version, &exists_value);
    EXPECT_CALL(
        mock_db_,
        Get(_, SliceEq(ExistsEntryKey::Encode(db, os, encoded_primary_key)), _))
        .WillOnce(testing::DoAll(testing::SetArgPointee<2>(exists_value),
                                 Return(leveldb::Status::OK())));
  }

 protected:
  std::unique_ptr<TransactionalLevelDBDatabase> in_memory_db_;
  leveldb::MockLevelDB mock_db_;

  std::vector<IndexedDBDatabaseMetadata> metadata_;

  std::unique_ptr<LevelDbTombstoneSweeper> sweeper_;

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(LevelDbTombstoneSweeperTest, EmptyDB) {
  SetupMockDB();
  sweeper_->SetMetadata(&metadata_);
  EXPECT_TRUE(sweeper_->RunRound());
}

TEST_F(LevelDbTombstoneSweeperTest, NoTombstonesComplexDB) {
  SetupMockDB();
  PopulateMultiDBMetdata();
  sweeper_->SetMetadata(&metadata_);

  // We'll have one index entry per index, and simulate reaching the end.
  leveldb::MockIterator* mock_iterator = new leveldb::MockIterator();
  EXPECT_CALL(mock_db_, NewIterator(testing::_))
      .WillOnce(testing::Return(mock_iterator));

  // First index.
  {
    testing::InSequence sequence_enforcer;
    EXPECT_CALL(*mock_iterator,
                Seek(SliceEq(IndexDataKey::EncodeMinKey(kDb1, kOs2, kIndex1))));
    EXPECT_CALL(*mock_iterator, Valid()).Times(2).WillRepeatedly(Return(true));

    ExpectIndexAndExistsEntries(
        *mock_iterator, kDb1, kOs2, kIndex1,
        IndexedDBKey(10, blink::mojom::IDBKeyType::Number),
        IndexedDBKey(20, blink::mojom::IDBKeyType::Number), 1, 1);
    EXPECT_CALL(*mock_iterator, Next());

    EXPECT_CALL(*mock_iterator, Valid()).Times(2).WillRepeatedly(Return(true));
    // Return the beginning of the second index, which should cause us to error
    // & go restart our index seek.
    ExpectIndexEntry(*mock_iterator, kDb1, kOs2, kIndex2,
                     IndexedDBKey(30, blink::mojom::IDBKeyType::Number),
                     IndexedDBKey(10, blink::mojom::IDBKeyType::Number), 1);
  }

  // Second index.
  {
    testing::InSequence sequence_enforcer;
    EXPECT_CALL(*mock_iterator,
                Seek(SliceEq(IndexDataKey::EncodeMinKey(kDb1, kOs2, kIndex2))));
    EXPECT_CALL(*mock_iterator, Valid()).Times(2).WillRepeatedly(Return(true));
    ExpectIndexAndExistsEntries(
        *mock_iterator, kDb1, kOs2, kIndex2,
        IndexedDBKey(30, blink::mojom::IDBKeyType::Number),
        IndexedDBKey(10, blink::mojom::IDBKeyType::Number), 1, 1);
    EXPECT_CALL(*mock_iterator, Next());

    // Return next key, which should make it error
    EXPECT_CALL(*mock_iterator, Valid()).Times(2).WillRepeatedly(Return(true));
    ExpectIndexEntry(*mock_iterator, kDb2, kOs3, kIndex3,
                     IndexedDBKey(1501, blink::mojom::IDBKeyType::Number),
                     IndexedDBKey(15123, blink::mojom::IDBKeyType::Number), 12);
  }

  // Third index.
  {
    testing::InSequence sequence_enforcer;
    EXPECT_CALL(*mock_iterator,
                Seek(SliceEq(IndexDataKey::EncodeMinKey(kDb2, kOs3, kIndex3))));
    EXPECT_CALL(*mock_iterator, Valid()).Times(2).WillRepeatedly(Return(true));
    ExpectIndexAndExistsEntries(
        *mock_iterator, kDb2, kOs3, kIndex3,
        IndexedDBKey(1501, blink::mojom::IDBKeyType::Number),
        IndexedDBKey(15123, blink::mojom::IDBKeyType::Number), 12, 12);
    EXPECT_CALL(*mock_iterator, Next());

    // Return next key, which should make it error
    EXPECT_CALL(*mock_iterator, Valid()).WillOnce(Return(false));
    EXPECT_CALL(*mock_iterator, status())
        .WillOnce(Return(leveldb::Status::OK()));
    EXPECT_CALL(*mock_iterator, Valid()).WillOnce(Return(false));
  }

  ASSERT_TRUE(sweeper_->RunRound());
}

TEST_F(LevelDbTombstoneSweeperTest, AllTombstonesComplexDB) {
  SetupMockDB();
  PopulateMultiDBMetdata();
  sweeper_->SetMetadata(&metadata_);

  // We'll have one index entry per index, and simulate reaching the end.
  leveldb::MockIterator* mock_iterator = new leveldb::MockIterator();
  EXPECT_CALL(mock_db_, NewIterator(testing::_))
      .WillOnce(testing::Return(mock_iterator));

  // First index.
  {
    testing::InSequence sequence_enforcer;
    EXPECT_CALL(*mock_iterator,
                Seek(SliceEq(IndexDataKey::EncodeMinKey(kDb1, kOs2, kIndex1))));
    EXPECT_CALL(*mock_iterator, Valid()).Times(2).WillRepeatedly(Return(true));

    ExpectIndexAndExistsEntries(
        *mock_iterator, kDb1, kOs2, kIndex1,
        IndexedDBKey(10, blink::mojom::IDBKeyType::Number),
        IndexedDBKey(20, blink::mojom::IDBKeyType::Number), 1, 2);
    EXPECT_CALL(*mock_iterator, Next());

    EXPECT_CALL(*mock_iterator, Valid()).Times(2).WillRepeatedly(Return(true));
    // Return the beginning of the second index, which should cause us to error
    // & go restart our index seek.
    ExpectIndexEntry(*mock_iterator, kDb1, kOs2, kIndex2,
                     IndexedDBKey(30, blink::mojom::IDBKeyType::Number),
                     IndexedDBKey(10, blink::mojom::IDBKeyType::Number), 1);
  }

  // Second index.
  {
    testing::InSequence sequence_enforcer;
    EXPECT_CALL(*mock_iterator,
                Seek(SliceEq(IndexDataKey::EncodeMinKey(kDb1, kOs2, kIndex2))));
    EXPECT_CALL(*mock_iterator, Valid()).Times(2).WillRepeatedly(Return(true));
    ExpectIndexAndExistsEntries(
        *mock_iterator, kDb1, kOs2, kIndex2,
        IndexedDBKey(30, blink::mojom::IDBKeyType::Number),
        IndexedDBKey(10, blink::mojom::IDBKeyType::Number), 1, 2);
    EXPECT_CALL(*mock_iterator, Next());

    // Return next key, which should make it error
    EXPECT_CALL(*mock_iterator, Valid()).Times(2).WillRepeatedly(Return(true));
    ExpectIndexEntry(*mock_iterator, kDb2, kOs3, kIndex3,
                     IndexedDBKey(1501, blink::mojom::IDBKeyType::Number),
                     IndexedDBKey(15123, blink::mojom::IDBKeyType::Number), 12);
  }

  // Third index.
  {
    testing::InSequence sequence_enforcer;
    EXPECT_CALL(*mock_iterator,
                Seek(SliceEq(IndexDataKey::EncodeMinKey(kDb2, kOs3, kIndex3))));
    EXPECT_CALL(*mock_iterator, Valid()).Times(2).WillRepeatedly(Return(true));
    ExpectIndexAndExistsEntries(
        *mock_iterator, kDb2, kOs3, kIndex3,
        IndexedDBKey(1501, blink::mojom::IDBKeyType::Number),
        IndexedDBKey(15123, blink::mojom::IDBKeyType::Number), 12, 13);
    EXPECT_CALL(*mock_iterator, Next());

    // Return next key, which should make it error
    EXPECT_CALL(*mock_iterator, Valid()).WillOnce(Return(false));
    EXPECT_CALL(*mock_iterator, status())
        .WillOnce(Return(leveldb::Status::OK()));
    EXPECT_CALL(*mock_iterator, Valid()).WillOnce(Return(false));
  }

  EXPECT_CALL(mock_db_, Write(_, _));

  ASSERT_TRUE(sweeper_->RunRound());
}

TEST_F(LevelDbTombstoneSweeperTest, SimpleRealDBNoTombstones) {
  PopulateSingleIndexDBMetadata();
  SetupRealDB();
  sweeper_->SetMetadata(&metadata_);

  for (int i = 0; i < kRoundIterations; i++) {
    auto index_key = IndexedDBKey(i, blink::mojom::IDBKeyType::Number);
    auto primary_key = IndexedDBKey(i + 1, blink::mojom::IDBKeyType::Number);
    std::string value_str;
    EncodeVarInt(1, &value_str);
    EncodeIDBKey(primary_key, &value_str);
    in_memory_db_->Put(
        IndexDataKey::Encode(kDb1, kOs1, kIndex1, index_key, primary_key),
        &value_str);

    std::string exists_value;
    std::string encoded_primary_key;
    EncodeIDBKey(primary_key, &encoded_primary_key);
    EncodeVarInt(1, &exists_value);
    in_memory_db_->Put(ExistsEntryKey::Encode(kDb1, kOs1, encoded_primary_key),
                       &exists_value);
  }

  ASSERT_FALSE(sweeper_->RunRound());
  EXPECT_TRUE(sweeper_->RunRound());
}

TEST_F(LevelDbTombstoneSweeperTest, SimpleRealDBWithTombstones) {
  PopulateSingleIndexDBMetadata();
  SetupRealDB();
  sweeper_->SetMetadata(&metadata_);

  for (int i = 0; i < kRoundIterations + 1; i++) {
    auto index_key = IndexedDBKey(i, blink::mojom::IDBKeyType::Number);
    auto primary_key = IndexedDBKey(i + 1, blink::mojom::IDBKeyType::Number);
    std::string value_str;
    EncodeVarInt(1, &value_str);
    EncodeIDBKey(primary_key, &value_str);
    in_memory_db_->Put(
        IndexDataKey::Encode(kDb1, kOs1, kIndex1, index_key, primary_key),
        &value_str);

    std::string exists_value;
    std::string encoded_primary_key;
    EncodeIDBKey(primary_key, &encoded_primary_key);
    bool tombstone = i % 2 != 0;
    EncodeVarInt(tombstone ? 2 : 1, &exists_value);
    in_memory_db_->Put(ExistsEntryKey::Encode(kDb1, kOs1, encoded_primary_key),
                       &exists_value);
  }

  ASSERT_FALSE(sweeper_->RunRound());
  EXPECT_TRUE(sweeper_->RunRound());

  for (int i = 0; i < kRoundIterations + 1; i++) {
    if (i % 2 == 1) {
      std::string out;
      bool found = false;
      auto index_key = IndexedDBKey(i, blink::mojom::IDBKeyType::Number);
      auto primary_key = IndexedDBKey(i + 1, blink::mojom::IDBKeyType::Number);
      EXPECT_TRUE(in_memory_db_
                      ->Get(IndexDataKey::Encode(kDb1, kOs1, kIndex1, index_key,
                                                 primary_key),
                            &out, &found)
                      .ok());
      EXPECT_TRUE(!found);
    }
  }
}

TEST_F(LevelDbTombstoneSweeperTest, LevelDBError) {
  SetupMockDB();
  PopulateMultiDBMetdata();
  sweeper_->SetMetadata(&metadata_);

  // We'll have one index entry per index, and simulate reaching the end.
  leveldb::MockIterator* mock_iterator = new leveldb::MockIterator();
  EXPECT_CALL(mock_db_, NewIterator(testing::_))
      .WillOnce(testing::Return(mock_iterator));

  // First index.
  {
    testing::InSequence sequence_enforcer;
    EXPECT_CALL(*mock_iterator,
                Seek(SliceEq(IndexDataKey::EncodeMinKey(kDb1, kOs2, kIndex1))));
    EXPECT_CALL(*mock_iterator, Valid()).Times(2).WillRepeatedly(Return(true));

    ExpectIndexAndExistsEntries(
        *mock_iterator, kDb1, kOs2, kIndex1,
        IndexedDBKey(10, blink::mojom::IDBKeyType::Number),
        IndexedDBKey(20, blink::mojom::IDBKeyType::Number), 1, 1);
    EXPECT_CALL(*mock_iterator, Next());

    EXPECT_CALL(*mock_iterator, Valid()).Times(2).WillRepeatedly(Return(true));
    // Return the beginning of the second index, which should cause us to error
    // & go restart our index seek.
    ExpectIndexEntry(*mock_iterator, kDb1, kOs2, kIndex2,
                     IndexedDBKey(30, blink::mojom::IDBKeyType::Number),
                     IndexedDBKey(10, blink::mojom::IDBKeyType::Number), 1);
  }

  // Second index.
  {
    testing::InSequence sequence_enforcer;
    EXPECT_CALL(*mock_iterator,
                Seek(SliceEq(IndexDataKey::EncodeMinKey(kDb1, kOs2, kIndex2))));
    EXPECT_CALL(*mock_iterator, Valid()).Times(2).WillRepeatedly(Return(true));
    ExpectIndexAndExistsEntries(
        *mock_iterator, kDb1, kOs2, kIndex2,
        IndexedDBKey(30, blink::mojom::IDBKeyType::Number),
        IndexedDBKey(10, blink::mojom::IDBKeyType::Number), 1, 1);
    EXPECT_CALL(*mock_iterator, Next());

    // Return read error.
    EXPECT_CALL(*mock_iterator, Valid()).WillOnce(Return(false));
    EXPECT_CALL(*mock_iterator, status())
        .WillOnce(Return(leveldb::Status::Corruption("Test error")));
  }

  ASSERT_TRUE(sweeper_->RunRound());
}

}  // namespace content::indexed_db
