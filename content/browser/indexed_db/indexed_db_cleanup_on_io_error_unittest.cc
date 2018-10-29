// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cerrno>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/indexed_db/indexed_db_backing_store.h"
#include "content/browser/indexed_db/indexed_db_data_loss_info.h"
#include "content/browser/indexed_db/leveldb/leveldb_database.h"
#include "content/browser/indexed_db/leveldb/mock_leveldb_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/env_chromium.h"

using base::StringPiece;
using content::IndexedDBBackingStore;
using content::LevelDBComparator;
using content::LevelDBDatabase;
using content::LevelDBFactory;
using content::LevelDBSnapshot;
using testing::_;
using testing::Exactly;
using testing::Invoke;

namespace base {
class TaskRunner;
}

namespace content {
class IndexedDBFactory;
}

namespace {

class BustedLevelDBDatabase : public LevelDBDatabase {
 public:
  BustedLevelDBDatabase()
      : LevelDBDatabase(LevelDBDatabase::kDefaultMaxOpenIteratorsPerDatabase) {}
  static std::unique_ptr<LevelDBDatabase> Open(
      const base::FilePath& file_name,
      const LevelDBComparator* /*comparator*/) {
    return std::make_unique<BustedLevelDBDatabase>();
  }
  leveldb::Status Get(const base::StringPiece& key,
                      std::string* value,
                      bool* found,
                      const LevelDBSnapshot* = nullptr) override {
    return leveldb::Status::IOError("It's busted!");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BustedLevelDBDatabase);
};

class BustedLevelDBFactory : public LevelDBFactory {
 public:
  leveldb::Status OpenLevelDB(const base::FilePath& file_name,
                              const LevelDBComparator* comparator,
                              std::unique_ptr<LevelDBDatabase>* db,
                              bool* is_disk_full = nullptr) override {
    if (open_error_.ok())
      *db = BustedLevelDBDatabase::Open(file_name, comparator);
    return open_error_;
  }
  leveldb::Status DestroyLevelDB(const base::FilePath& file_name) override {
    return leveldb::Status::IOError("error");
  }
  void SetOpenError(const leveldb::Status& open_error) {
    open_error_ = open_error;
  }

 private:
  leveldb::Status open_error_;
};

TEST(IndexedDBIOErrorTest, CleanUpTest) {
  content::IndexedDBFactory* factory = nullptr;
  const url::Origin origin = url::Origin::Create(GURL("http://localhost:81"));
  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());
  const base::FilePath path = temp_directory.GetPath();

  BustedLevelDBFactory busted_factory;
  content::MockLevelDBFactory mock_leveldb_factory;
  ON_CALL(mock_leveldb_factory, OpenLevelDB(_, _, _, _)).WillByDefault(
      Invoke(&busted_factory, &BustedLevelDBFactory::OpenLevelDB));
  ON_CALL(mock_leveldb_factory, DestroyLevelDB(_)).WillByDefault(
      Invoke(&busted_factory, &BustedLevelDBFactory::DestroyLevelDB));

  EXPECT_CALL(mock_leveldb_factory, OpenLevelDB(_, _, _, _)).Times(Exactly(1));
  EXPECT_CALL(mock_leveldb_factory, DestroyLevelDB(_)).Times(Exactly(1));
  content::IndexedDBDataLossInfo data_loss_info;
  bool disk_full = false;
  base::SequencedTaskRunner* task_runner = nullptr;
  bool clean_journal = false;
  leveldb::Status s;
  scoped_refptr<IndexedDBBackingStore> backing_store =
      IndexedDBBackingStore::Open(factory, origin, path, &data_loss_info,
                                  &disk_full, &mock_leveldb_factory,
                                  task_runner, clean_journal, &s);
}

TEST(IndexedDBNonRecoverableIOErrorTest, NuancedCleanupTest) {
  content::IndexedDBFactory* factory = nullptr;
  const url::Origin origin = url::Origin::Create(GURL("http://localhost:81"));
  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());
  const base::FilePath path = temp_directory.GetPath();
  content::IndexedDBDataLossInfo data_loss_info;
  bool disk_full = false;
  base::SequencedTaskRunner* task_runner = nullptr;
  bool clean_journal = false;
  leveldb::Status s;

  BustedLevelDBFactory busted_factory;
  content::MockLevelDBFactory mock_leveldb_factory;
  ON_CALL(mock_leveldb_factory, OpenLevelDB(_, _, _, _)).WillByDefault(
      Invoke(&busted_factory, &BustedLevelDBFactory::OpenLevelDB));
  ON_CALL(mock_leveldb_factory, DestroyLevelDB(_)).WillByDefault(
      Invoke(&busted_factory, &BustedLevelDBFactory::DestroyLevelDB));

  EXPECT_CALL(mock_leveldb_factory, OpenLevelDB(_, _, _, _)).Times(Exactly(4));
  EXPECT_CALL(mock_leveldb_factory, DestroyLevelDB(_)).Times(Exactly(0));

  busted_factory.SetOpenError(MakeIOError("some filename", "some message",
                                          leveldb_env::kNewLogger,
                                          base::File::FILE_ERROR_NO_SPACE));
  scoped_refptr<IndexedDBBackingStore> backing_store =
      IndexedDBBackingStore::Open(factory, origin, path, &data_loss_info,
                                  &disk_full, &mock_leveldb_factory,
                                  task_runner, clean_journal, &s);
  ASSERT_TRUE(s.IsIOError());

  busted_factory.SetOpenError(MakeIOError("some filename",
                                          "some message",
                                          leveldb_env::kNewLogger,
                                          base::File::FILE_ERROR_NO_MEMORY));
  scoped_refptr<IndexedDBBackingStore> backing_store2 =
      IndexedDBBackingStore::Open(factory, origin, path, &data_loss_info,
                                  &disk_full, &mock_leveldb_factory,
                                  task_runner, clean_journal, &s);
  ASSERT_TRUE(s.IsIOError());

  busted_factory.SetOpenError(MakeIOError("some filename", "some message",
                                          leveldb_env::kNewLogger,
                                          base::File::FILE_ERROR_IO));
  scoped_refptr<IndexedDBBackingStore> backing_store3 =
      IndexedDBBackingStore::Open(factory, origin, path, &data_loss_info,
                                  &disk_full, &mock_leveldb_factory,
                                  task_runner, clean_journal, &s);
  ASSERT_TRUE(s.IsIOError());

  busted_factory.SetOpenError(MakeIOError("some filename",
                                          "some message",
                                          leveldb_env::kNewLogger,
                                          base::File::FILE_ERROR_FAILED));
  scoped_refptr<IndexedDBBackingStore> backing_store4 =
      IndexedDBBackingStore::Open(factory, origin, path, &data_loss_info,
                                  &disk_full, &mock_leveldb_factory,
                                  task_runner, clean_journal, &s);
  ASSERT_TRUE(s.IsIOError());
}

}  // namespace
