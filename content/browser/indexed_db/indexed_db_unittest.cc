// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <utility>

#include "base/barrier_closure.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/default_clock.h"
#include "components/services/storage/indexed_db/scopes/scopes_lock_manager.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_database.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_execution_context_connection_tracker.h"
#include "content/browser/indexed_db/indexed_db_factory_impl.h"
#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"
#include "content/browser/indexed_db/indexed_db_leveldb_env.h"
#include "content/browser/indexed_db/indexed_db_origin_state.h"
#include "content/browser/indexed_db/mock_indexed_db_callbacks.h"
#include "content/browser/indexed_db/mock_indexed_db_database_callbacks.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_utils.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

using blink::IndexedDBDatabaseMetadata;
using url::Origin;

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
  LevelDBLock() : env_(nullptr), lock_(nullptr) {}
  LevelDBLock(leveldb::Env* env, leveldb::FileLock* lock)
      : env_(env), lock_(lock) {}
  ~LevelDBLock() {
    if (env_)
      env_->UnlockFile(lock_);
  }

 private:
  leveldb::Env* env_;
  leveldb::FileLock* lock_;

  DISALLOW_COPY_AND_ASSIGN(LevelDBLock);
};

std::unique_ptr<LevelDBLock> LockForTesting(const base::FilePath& file_name) {
  leveldb::Env* env = IndexedDBLevelDBEnv::Get();
  base::FilePath lock_path = file_name.AppendASCII("LOCK");
  leveldb::FileLock* lock = nullptr;
  leveldb::Status status = env->LockFile(lock_path.AsUTF8Unsafe(), &lock);
  if (!status.ok())
    return std::unique_ptr<LevelDBLock>();
  DCHECK(lock);
  return std::make_unique<LevelDBLock>(env, lock);
}

}  // namespace

class IndexedDBTest : public testing::Test {
 public:
  const Origin kNormalOrigin;
  const Origin kSessionOnlyOrigin;

  IndexedDBTest()
      : kNormalOrigin(url::Origin::Create(GURL("http://normal/"))),
        kSessionOnlyOrigin(url::Origin::Create(GURL("http://session-only/"))),
        special_storage_policy_(
            base::MakeRefCounted<MockSpecialStoragePolicy>()),
        quota_manager_proxy_(
            base::MakeRefCounted<MockQuotaManagerProxy>(nullptr, nullptr)),
        context_(base::MakeRefCounted<IndexedDBContextImpl>(
            CreateAndReturnTempDir(&temp_dir_),
            /*special_storage_policy=*/special_storage_policy_.get(),
            quota_manager_proxy_.get(),
            base::DefaultClock::GetInstance(),
            base::SequencedTaskRunnerHandle::Get())) {
    special_storage_policy_->AddSessionOnly(kSessionOnlyOrigin.GetURL());
  }
  ~IndexedDBTest() override {
    quota_manager_proxy_->SimulateQuotaManagerDestroyed();
  }

  void RunPostedTasks() {
    base::RunLoop loop;
    context_->TaskRunner()->PostTask(FROM_HERE, loop.QuitClosure());
    loop.Run();
  }

  void TearDown() override {
    if (context_ && !context_->IsInMemoryContext()) {
      IndexedDBFactoryImpl* factory = context_->GetIDBFactory();

      // Loop through all open origins, and force close them, and request the
      // deletion of the leveldb state. Once the states are no longer around,
      // delete all of the databases on disk.
      auto open_factory_origins = factory->GetOpenOrigins();
      base::RunLoop loop;
      auto callback = base::BarrierClosure(
          open_factory_origins.size(), base::BindLambdaForTesting([&]() {
            // All leveldb databases are closed, and they can be deleted.
            for (auto origin : context_->GetAllOrigins()) {
              context_->DeleteForOrigin(origin);
            }
            loop.Quit();
          }));
      for (auto origin : open_factory_origins) {
        IndexedDBOriginState* per_origin_factory =
            factory->GetOriginFactory(origin);
        per_origin_factory->backing_store()
            ->db()
            ->leveldb_state()
            ->RequestDestruction(callback,
                                 base::SequencedTaskRunnerHandle::Get());
        context_->ForceClose(origin,
                             IndexedDBContextImpl::FORCE_CLOSE_DELETE_ORIGIN);
      }
      loop.Run();
    }
    if (temp_dir_.IsValid())
      ASSERT_TRUE(temp_dir_.Delete());
  }

 protected:
  IndexedDBContextImpl* context() const { return context_.get(); }
  scoped_refptr<MockSpecialStoragePolicy> special_storage_policy_;
  scoped_refptr<MockQuotaManagerProxy> quota_manager_proxy_;

 private:
  BrowserTaskEnvironment task_environment_;
  TestBrowserContext browser_context_;
  base::ScopedTempDir temp_dir_;
  scoped_refptr<IndexedDBContextImpl> context_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBTest);
};

TEST_F(IndexedDBTest, ClearSessionOnlyDatabases) {
  base::FilePath normal_path;
  base::FilePath session_only_path;

  normal_path = context()->GetFilePathForTesting(kNormalOrigin);
  session_only_path = context()->GetFilePathForTesting(kSessionOnlyOrigin);
  ASSERT_TRUE(base::CreateDirectory(normal_path));
  ASSERT_TRUE(base::CreateDirectory(session_only_path));
  RunAllTasksUntilIdle();
  quota_manager_proxy_->SimulateQuotaManagerDestroyed();

  RunAllTasksUntilIdle();

  context()->Shutdown();

  RunAllTasksUntilIdle();

  EXPECT_TRUE(base::DirectoryExists(normal_path));
  EXPECT_FALSE(base::DirectoryExists(session_only_path));
}

TEST_F(IndexedDBTest, SetForceKeepSessionState) {
  base::FilePath normal_path;
  base::FilePath session_only_path;

  // Save session state. This should bypass the destruction-time deletion.
  context()->SetForceKeepSessionState();

  normal_path = context()->GetFilePathForTesting(kNormalOrigin);
  session_only_path = context()->GetFilePathForTesting(kSessionOnlyOrigin);
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
                        const Origin& origin)
      : IndexedDBCallbacks(nullptr,
                           origin,
                           mojo::NullAssociatedRemote(),
                           idb_context->TaskRunner()),
        idb_context_(idb_context),
        origin_(origin) {}

  void OnSuccess() override {}
  void OnSuccess(const std::vector<base::string16>&) override {}
  void OnSuccess(std::unique_ptr<IndexedDBConnection> connection,
                 const IndexedDBDatabaseMetadata& metadata) override {
    connection_ = std::move(connection);
    idb_context_->ConnectionOpened(origin_, connection_.get());
  }

  IndexedDBConnection* connection() { return connection_.get(); }

 protected:
  ~ForceCloseDBCallbacks() override {}

 private:
  scoped_refptr<IndexedDBContextImpl> idb_context_;
  Origin origin_;
  std::unique_ptr<IndexedDBConnection> connection_;
  DISALLOW_COPY_AND_ASSIGN(ForceCloseDBCallbacks);
};

TEST_F(IndexedDBTest, ForceCloseOpenDatabasesOnDelete) {
  const Origin kTestOrigin = Origin::Create(GURL("http://test/"));

  auto open_db_callbacks =
      base::MakeRefCounted<MockIndexedDBDatabaseCallbacks>();
  auto closed_db_callbacks =
      base::MakeRefCounted<MockIndexedDBDatabaseCallbacks>();
  auto open_callbacks =
      base::MakeRefCounted<ForceCloseDBCallbacks>(context(), kTestOrigin);
  auto closed_callbacks =
      base::MakeRefCounted<ForceCloseDBCallbacks>(context(), kTestOrigin);
  base::FilePath test_path = context()->GetFilePathForTesting(kTestOrigin);

  const int64_t host_transaction_id = 0;
  const int64_t version = 0;

  IndexedDBFactory* factory = context()->GetIDBFactory();

  auto create_transaction_callback1 =
      base::BindOnce(&CreateAndBindTransactionPlaceholder);
  factory->Open(base::ASCIIToUTF16("opendb"),
                std::make_unique<IndexedDBPendingConnection>(
                    open_callbacks, open_db_callbacks,
                    IndexedDBExecutionContextConnectionTracker::Handle::
                        CreateForTesting(),
                    host_transaction_id, version,
                    std::move(create_transaction_callback1)),
                kTestOrigin, context()->data_path());
  EXPECT_TRUE(base::DirectoryExists(test_path));

  auto create_transaction_callback2 =
      base::BindOnce(&CreateAndBindTransactionPlaceholder);
  factory->Open(base::ASCIIToUTF16("closeddb"),
                std::make_unique<IndexedDBPendingConnection>(
                    closed_callbacks, closed_db_callbacks,
                    IndexedDBExecutionContextConnectionTracker::Handle::
                        CreateForTesting(),
                    host_transaction_id, version,
                    std::move(create_transaction_callback2)),
                kTestOrigin, context()->data_path());
  RunPostedTasks();
  ASSERT_TRUE(closed_callbacks->connection());
  closed_callbacks->connection()->AbortTransactionsAndClose(
      IndexedDBConnection::CloseErrorHandling::kAbortAllReturnLastError);
  RunPostedTasks();

  context()->ForceClose(kTestOrigin,
                        IndexedDBContextImpl::FORCE_CLOSE_DELETE_ORIGIN);
  EXPECT_TRUE(open_db_callbacks->forced_close_called());
  EXPECT_FALSE(closed_db_callbacks->forced_close_called());

  RunPostedTasks();

  context()->DeleteForOrigin(kTestOrigin);

  EXPECT_FALSE(base::DirectoryExists(test_path));
}

TEST_F(IndexedDBTest, DeleteFailsIfDirectoryLocked) {
  const Origin kTestOrigin = Origin::Create(GURL("http://test/"));

  base::FilePath test_path = context()->GetFilePathForTesting(kTestOrigin);
  ASSERT_TRUE(base::CreateDirectory(test_path));

  auto lock = LockForTesting(test_path);
  ASSERT_TRUE(lock);

  base::RunLoop loop;
  context()->TaskRunner()->PostTask(FROM_HERE,
                                    base::BindLambdaForTesting([&]() {
                                      context()->DeleteForOrigin(kTestOrigin);
                                      loop.Quit();
                                    }));
  loop.Run();

  EXPECT_TRUE(base::DirectoryExists(test_path));
}

TEST_F(IndexedDBTest, ForceCloseOpenDatabasesOnCommitFailure) {
  const Origin kTestOrigin = Origin::Create(GURL("http://test/"));

  auto* factory =
      static_cast<IndexedDBFactoryImpl*>(context()->GetIDBFactory());

  const int64_t transaction_id = 1;

  auto callbacks = base::MakeRefCounted<MockIndexedDBCallbacks>();
  auto db_callbacks = base::MakeRefCounted<MockIndexedDBDatabaseCallbacks>();
  auto create_transaction_callback1 =
      base::BindOnce(&CreateAndBindTransactionPlaceholder);
  auto connection = std::make_unique<IndexedDBPendingConnection>(
      callbacks, db_callbacks,
      IndexedDBExecutionContextConnectionTracker::Handle::CreateForTesting(),
      transaction_id, IndexedDBDatabaseMetadata::DEFAULT_VERSION,
      std::move(create_transaction_callback1));
  factory->Open(base::ASCIIToUTF16("db"), std::move(connection),
                Origin(kTestOrigin), context()->data_path());
  RunPostedTasks();

  ASSERT_TRUE(callbacks->connection());

  // ConnectionOpened() is usually called by the dispatcher.
  context()->ConnectionOpened(kTestOrigin, callbacks->connection());

  EXPECT_TRUE(factory->IsBackingStoreOpen(kTestOrigin));

  // Simulate the write failure.
  leveldb::Status status = leveldb::Status::IOError("Simulated failure");
  factory->HandleBackingStoreFailure(kTestOrigin);

  EXPECT_TRUE(db_callbacks->forced_close_called());
  EXPECT_FALSE(factory->IsBackingStoreOpen(kTestOrigin));
}

TEST(ScopesLockManager, TestRangeDifferences) {
  ScopeLockRange range_db1;
  ScopeLockRange range_db2;
  ScopeLockRange range_db1_os1;
  ScopeLockRange range_db1_os2;
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
