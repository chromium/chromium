// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cerrno>
#include <memory>
#include <string>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "components/services/storage/indexed_db/leveldb/fake_leveldb_factory.h"
#include "components/services/storage/indexed_db/scopes/leveldb_scopes.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_database.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_factory.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "content/browser/indexed_db/indexed_db_backing_store.h"
#include "content/browser/indexed_db/indexed_db_class_factory.h"
#include "content/browser/indexed_db/indexed_db_leveldb_env.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/leveldatabase/env_chromium.h"

using base::StringPiece;

namespace base {
class TaskRunner;
}

namespace content {
namespace {

TEST(IndexedDBIOErrorTest, CleanUpTest) {
  base::test::TaskEnvironment task_env;
  const blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting("http://localhost:81");
  auto bucket_locator = storage::BucketLocator();
  bucket_locator.storage_key = storage_key;
  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());
  const base::FilePath path = temp_directory.GetPath();

  DefaultTransactionalLevelDBFactory transactional_leveldb_factory;
  auto task_runner = base::SequencedTaskRunner::GetCurrentDefault();
  std::unique_ptr<IndexedDBBackingStore> backing_store = std::make_unique<
      IndexedDBBackingStore>(
      IndexedDBBackingStore::Mode::kInMemory, &transactional_leveldb_factory,
      bucket_locator, path,
      transactional_leveldb_factory.CreateLevelDBDatabase(
          FakeLevelDBFactory::GetBrokenLevelDB(
              leveldb::Status::IOError("It's broken!"), path),
          nullptr, task_runner.get(),
          TransactionalLevelDBDatabase::kDefaultMaxOpenIteratorsPerDatabase),
      /*blob_storage_context=*/nullptr,
      /*file_system_access_context=*/nullptr,
      /*filesystem_proxy=*/nullptr,
      IndexedDBBackingStore::BlobFilesCleanedCallback(),
      IndexedDBBackingStore::ReportOutstandingBlobsCallback(), task_runner);
  leveldb::Status s = backing_store->Initialize(false);
  EXPECT_FALSE(s.ok());
  ASSERT_TRUE(temp_directory.Delete());
}

TEST(IndexedDBNonRecoverableIOErrorTest, NuancedCleanupTest) {
  base::test::TaskEnvironment task_env;
  const blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting("http://localhost:81");
  auto bucket_locator = storage::BucketLocator();
  bucket_locator.storage_key = storage_key;
  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());
  const base::FilePath path = temp_directory.GetPath();
  auto task_runner = base::SequencedTaskRunner::GetCurrentDefault();

  DefaultTransactionalLevelDBFactory transactional_leveldb_factory;
  std::array<leveldb::Status, 4> errors = {
      MakeIOError("some filename", "some message", leveldb_env::kNewLogger,
                  base::File::FILE_ERROR_NO_SPACE),
      MakeIOError("some filename", "some message", leveldb_env::kNewLogger,
                  base::File::FILE_ERROR_NO_MEMORY),
      MakeIOError("some filename", "some message", leveldb_env::kNewLogger,
                  base::File::FILE_ERROR_IO),
      MakeIOError("some filename", "some message", leveldb_env::kNewLogger,
                  base::File::FILE_ERROR_FAILED)};
  for (leveldb::Status error_status : errors) {
    std::unique_ptr<IndexedDBBackingStore> backing_store = std::make_unique<
        IndexedDBBackingStore>(
        IndexedDBBackingStore::Mode::kInMemory, &transactional_leveldb_factory,
        bucket_locator, path,
        transactional_leveldb_factory.CreateLevelDBDatabase(
            FakeLevelDBFactory::GetBrokenLevelDB(error_status, path), nullptr,
            task_runner.get(),
            TransactionalLevelDBDatabase::kDefaultMaxOpenIteratorsPerDatabase),
        /*blob_storage_context=*/nullptr,
        /*file_system_access_context=*/nullptr,
        /*filesystem_proxy=*/nullptr,
        IndexedDBBackingStore::BlobFilesCleanedCallback(),
        IndexedDBBackingStore::ReportOutstandingBlobsCallback(), task_runner);
    leveldb::Status s = backing_store->Initialize(false);
    ASSERT_TRUE(s.IsIOError());
  }
  ASSERT_TRUE(temp_directory.Delete());
}

}  // namespace
}  // namespace content
