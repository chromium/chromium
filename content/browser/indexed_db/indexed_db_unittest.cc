// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <utility>

#include "base/barrier_closure.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/default_clock.h"
#include "components/services/storage/indexed_db/locks/leveled_lock_manager.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_database.h"
#include "components/services/storage/public/mojom/indexed_db_control.mojom-test-utils.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_factory_impl.h"
#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"
#include "content/browser/indexed_db/indexed_db_leveldb_env.h"
#include "content/browser/indexed_db/indexed_db_storage_key_state.h"
#include "content/browser/indexed_db/mock_indexed_db_callbacks.h"
#include "content/browser/indexed_db/mock_indexed_db_database_callbacks.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

using blink::IndexedDBDatabaseMetadata;

namespace content {
namespace {

base::FilePath CreateAndReturnTempDir(base::ScopedTempDir* temp_dir) {
  CHECK(temp_dir->CreateUniqueTempDir());
  return temp_dir->GetPath();
}

void CreateAndBindTransactionPlaceholder(
    base::WeakPtr<IndexedDBTransaction> transaction) {}

class LevelDBLock {
 public:
  LevelDBLock() = default;
  LevelDBLock(leveldb::Env* env, leveldb::FileLock* lock)
      : env_(env), lock_(lock) {}

  LevelDBLock(const LevelDBLock&) = delete;
  LevelDBLock& operator=(const LevelDBLock&) = delete;

  ~LevelDBLock() {
    if (env_)
      env_->UnlockFile(lock_);
  }

 private:
  raw_ptr<leveldb::Env> env_ = nullptr;
  raw_ptr<leveldb::FileLock> lock_ = nullptr;
};

std::unique_ptr<LevelDBLock> LockForTesting(const base::FilePath& file_name) {
  leveldb::Env* env = IndexedDBLevelDBEnv::Get();
  base::FilePath lock_path = file_name.AppendASCII("LOCK");
  leveldb::FileLock* lock = nullptr;
  leveldb::Status status = env->LockFile(lock_path.AsUTF8Unsafe(), &lock);
  if (!status.ok())
    return nullptr;
  DCHECK(lock);
  return std::make_unique<LevelDBLock>(env, lock);
}

}  // namespace

class IndexedDBTest : public testing::Test {
 public:
  const blink::StorageKey kNormalStorageKey;
  const blink::StorageKey kSessionOnlyStorageKey;

  IndexedDBTest()
      : kNormalStorageKey(
            blink::StorageKey::CreateFromStringForTesting("http://normal/")),
        kSessionOnlyStorageKey(blink::StorageKey::CreateFromStringForTesting(
            "http://session-only/")),
        quota_manager_proxy_(
            base::MakeRefCounted<storage::MockQuotaManagerProxy>(
                nullptr,
                base::SequencedTaskRunnerHandle::Get())),
        context_(base::MakeRefCounted<IndexedDBContextImpl>(
            CreateAndReturnTempDir(&temp_dir_),
            quota_manager_proxy_.get(),
            base::DefaultClock::GetInstance(),
            /*blob_storage_context=*/mojo::NullRemote(),
            /*file_system_access_context=*/mojo::NullRemote(),
            base::SequencedTaskRunnerHandle::Get(),
            base::SequencedTaskRunnerHandle::Get())) {
    std::vector<storage::mojom::StoragePolicyUpdatePtr> policy_updates;
    bool should_purge_on_shutdown = true;
    policy_updates.emplace_back(storage::mojom::StoragePolicyUpdate::New(
        kSessionOnlyStorageKey.origin(), should_purge_on_shutdown));
    context_->ApplyPolicyUpdates(std::move(policy_updates));
  }

  IndexedDBTest(const IndexedDBTest&) = delete;
  IndexedDBTest& operator=(const IndexedDBTest&) = delete;

  ~IndexedDBTest() override = default;

  void RunPostedTasks() {
    base::RunLoop loop;
    context_->IDBTaskRunner()->PostTask(FROM_HERE, loop.QuitClosure());
    loop.Run();
  }

  void TearDown() override {
    if (context_ && !context_->IsInMemoryContext()) {
      IndexedDBFactoryImpl* factory = context_->GetIDBFactory();

      // Loop through all open storage_keys, and force close them, and request
      // the deletion of the leveldb state. Once the states are no longer
      // around, delete all of the databases on disk.
      auto open_factory_storage_keys = factory->GetOpenStorageKeys();
      for (const auto& storage_key : open_factory_storage_keys) {
        context_->ForceCloseSync(
            storage_key,
            storage::mojom::ForceCloseReason::FORCE_CLOSE_DELETE_ORIGIN);
      }
      // All leveldb databases are closed, and they can be deleted.
      for (auto storage_key : context_->GetAllStorageKeys()) {
        bool success = false;
        storage::mojom::IndexedDBControlAsyncWaiter waiter(context_.get());
        waiter.DeleteForStorageKey(storage_key, &success);
        EXPECT_TRUE(success);
      }
    }

    if (temp_dir_.IsValid())
      ASSERT_TRUE(temp_dir_.Delete());
  }

  base::FilePath GetFilePathForTesting(const blink::StorageKey& storage_key) {
    base::FilePath path;
    base::RunLoop run_loop;
    context()->GetFilePathForTesting(
        storage_key,
        base::BindLambdaForTesting([&](const base::FilePath& async_path) {
          path = async_path;
          run_loop.Quit();
        }));
    run_loop.Run();
    return path;
  }

 protected:
  IndexedDBContextImpl* context() const { return context_.get(); }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  scoped_refptr<storage::MockQuotaManagerProxy> quota_manager_proxy_;
  scoped_refptr<IndexedDBContextImpl> context_;
};

TEST_F(IndexedDBTest, ClearSessionOnlyDatabases) {
  base::FilePath normal_path;
  base::FilePath session_only_path;

  normal_path = GetFilePathForTesting(kNormalStorageKey);
  session_only_path = GetFilePathForTesting(kSessionOnlyStorageKey);
  ASSERT_TRUE(base::CreateDirectory(normal_path));
  ASSERT_TRUE(base::CreateDirectory(session_only_path));
  base::RunLoop().RunUntilIdle();

  context()->Shutdown();

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(base::DirectoryExists(normal_path));
  EXPECT_FALSE(base::DirectoryExists(session_only_path));
}

TEST_F(IndexedDBTest, SetForceKeepSessionState) {
  base::FilePath normal_path;
  base::FilePath session_only_path;

  // Save session state. This should bypass the destruction-time deletion.
  context()->SetForceKeepSessionState();

  normal_path = GetFilePathForTesting(kNormalStorageKey);
  session_only_path = GetFilePathForTesting(kSessionOnlyStorageKey);
  ASSERT_TRUE(base::CreateDirectory(normal_path));
  ASSERT_TRUE(base::CreateDirectory(session_only_path));
  base::RunLoop().RunUntilIdle();

  context()->Shutdown();

  base::RunLoop().RunUntilIdle();

  // No data was cleared because of SetForceKeepSessionState.
  EXPECT_TRUE(base::DirectoryExists(normal_path));
  EXPECT_TRUE(base::DirectoryExists(session_only_path));
}

class ForceCloseDBCallbacks : public IndexedDBCallbacks {
 public:
  ForceCloseDBCallbacks(scoped_refptr<IndexedDBContextImpl> idb_context,
                        const blink::StorageKey& storage_key)
      : IndexedDBCallbacks(nullptr,
                           storage_key,
                           mojo::NullAssociatedRemote(),
                           idb_context->IDBTaskRunner()),
        idb_context_(idb_context),
        storage_key_(storage_key) {}

  ForceCloseDBCallbacks(const ForceCloseDBCallbacks&) = delete;
  ForceCloseDBCallbacks& operator=(const ForceCloseDBCallbacks&) = delete;

  void OnSuccess() override {}
  void OnSuccess(std::unique_ptr<IndexedDBConnection> connection,
                 const IndexedDBDatabaseMetadata& metadata) override {
    connection_ = std::move(connection);
    idb_context_->ConnectionOpened(storage_key_, connection_.get());
  }

  IndexedDBConnection* connection() { return connection_.get(); }

 protected:
  ~ForceCloseDBCallbacks() override = default;

 private:
  scoped_refptr<IndexedDBContextImpl> idb_context_;
  blink::StorageKey storage_key_;
  std::unique_ptr<IndexedDBConnection> connection_;
};

TEST_F(IndexedDBTest, ForceCloseOpenDatabasesOnDelete) {
  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::CreateFromStringForTesting("http://test/");

  auto open_db_callbacks =
      base::MakeRefCounted<MockIndexedDBDatabaseCallbacks>();
  auto closed_db_callbacks =
      base::MakeRefCounted<MockIndexedDBDatabaseCallbacks>();
  auto open_callbacks =
      base::MakeRefCounted<ForceCloseDBCallbacks>(context(), kTestStorageKey);
  auto closed_callbacks =
      base::MakeRefCounted<ForceCloseDBCallbacks>(context(), kTestStorageKey);
  base::FilePath test_path = GetFilePathForTesting(kTestStorageKey);

  const int64_t host_transaction_id = 0;
  const int64_t version = 0;

  IndexedDBFactory* factory = context()->GetIDBFactory();

  auto create_transaction_callback1 =
      base::BindOnce(&CreateAndBindTransactionPlaceholder);
  factory->Open(u"opendb",
                std::make_unique<IndexedDBPendingConnection>(
                    open_callbacks, open_db_callbacks, host_transaction_id,
                    version, std::move(create_transaction_callback1)),
                kTestStorageKey, context()->data_path());
  EXPECT_TRUE(base::DirectoryExists(test_path));

  auto create_transaction_callback2 =
      base::BindOnce(&CreateAndBindTransactionPlaceholder);
  factory->Open(u"closeddb",
                std::make_unique<IndexedDBPendingConnection>(
                    closed_callbacks, closed_db_callbacks, host_transaction_id,
                    version, std::move(create_transaction_callback2)),
                kTestStorageKey, context()->data_path());
  RunPostedTasks();
  ASSERT_TRUE(closed_callbacks->connection());
  closed_callbacks->connection()->AbortTransactionsAndClose(
      IndexedDBConnection::CloseErrorHandling::kAbortAllReturnLastError);
  RunPostedTasks();

  context()->ForceCloseSync(
      kTestStorageKey,
      storage::mojom::ForceCloseReason::FORCE_CLOSE_DELETE_ORIGIN);
  EXPECT_TRUE(open_db_callbacks->forced_close_called());
  EXPECT_FALSE(closed_db_callbacks->forced_close_called());

  RunPostedTasks();

  bool success = false;
  storage::mojom::IndexedDBControlAsyncWaiter waiter(context());
  waiter.DeleteForStorageKey(kTestStorageKey, &success);
  EXPECT_TRUE(success);

  EXPECT_FALSE(base::DirectoryExists(test_path));
}

TEST_F(IndexedDBTest, DeleteFailsIfDirectoryLocked) {
  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::CreateFromStringForTesting("http://test/");

  base::FilePath test_path = GetFilePathForTesting(kTestStorageKey);
  ASSERT_TRUE(base::CreateDirectory(test_path));

  auto lock = LockForTesting(test_path);
  ASSERT_TRUE(lock);

  bool success = false;
  base::RunLoop loop;
  context()->IDBTaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        storage::mojom::IndexedDBControlAsyncWaiter waiter(context());
        waiter.DeleteForStorageKey(kTestStorageKey, &success);
        loop.Quit();
      }));
  loop.Run();
  EXPECT_FALSE(success);

  EXPECT_TRUE(base::DirectoryExists(test_path));
}

TEST_F(IndexedDBTest, ForceCloseOpenDatabasesOnCommitFailure) {
  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::CreateFromStringForTesting("http://test/");

  auto* factory =
      static_cast<IndexedDBFactoryImpl*>(context()->GetIDBFactory());

  const int64_t transaction_id = 1;

  auto callbacks = base::MakeRefCounted<MockIndexedDBCallbacks>();
  auto db_callbacks = base::MakeRefCounted<MockIndexedDBDatabaseCallbacks>();
  auto create_transaction_callback1 =
      base::BindOnce(&CreateAndBindTransactionPlaceholder);
  auto connection = std::make_unique<IndexedDBPendingConnection>(
      callbacks, db_callbacks,
      transaction_id, IndexedDBDatabaseMetadata::DEFAULT_VERSION,
      std::move(create_transaction_callback1));
  factory->Open(u"db", std::move(connection), kTestStorageKey,
                context()->data_path());
  RunPostedTasks();

  ASSERT_TRUE(callbacks->connection());

  // ConnectionOpened() is usually called by the dispatcher.
  context()->ConnectionOpened(kTestStorageKey, callbacks->connection());

  EXPECT_TRUE(factory->IsBackingStoreOpen(kTestStorageKey));

  // Simulate the write failure.
  leveldb::Status status = leveldb::Status::IOError("Simulated failure");
  factory->HandleBackingStoreFailure(kTestStorageKey);

  EXPECT_TRUE(db_callbacks->forced_close_called());
  EXPECT_FALSE(factory->IsBackingStoreOpen(kTestStorageKey));
}

TEST(LeveledLockManager, TestRangeDifferences) {
  LeveledLockRange range_db1;
  LeveledLockRange range_db2;
  LeveledLockRange range_db1_os1;
  LeveledLockRange range_db1_os2;
  for (int64_t i = 0; i < 512; ++i) {
    range_db1 = GetDatabaseLockRange(i);
    range_db2 = GetDatabaseLockRange(i + 1);
    range_db1_os1 = GetObjectStoreLockRange(i, i);
    range_db1_os2 = GetObjectStoreLockRange(i, i + 1);
    EXPECT_TRUE(range_db1.IsValid() && range_db2.IsValid() &&
                range_db1_os1.IsValid() && range_db1_os2.IsValid());
    EXPECT_LT(range_db1, range_db2);
    EXPECT_LT(range_db1, range_db1_os1);
    EXPECT_LT(range_db1, range_db1_os2);
    EXPECT_LT(range_db1_os1, range_db1_os2);
    EXPECT_LT(range_db1_os1, range_db2);
    EXPECT_LT(range_db1_os2, range_db2);
  }
}

}  // namespace content
