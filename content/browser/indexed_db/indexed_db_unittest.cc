// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <utility>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_factory_impl.h"
#include "content/browser/indexed_db/mock_indexed_db_callbacks.h"
#include "content/browser/indexed_db/mock_indexed_db_database_callbacks.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/test_browser_thread_bundle.h"
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
            base::MakeRefCounted<MockQuotaManagerProxy>(nullptr, nullptr)) {
    special_storage_policy_->AddSessionOnly(kSessionOnlyOrigin.GetURL());
  }
  ~IndexedDBTest() override {
    quota_manager_proxy_->SimulateQuotaManagerDestroyed();
  }

 protected:
  scoped_refptr<MockSpecialStoragePolicy> special_storage_policy_;
  scoped_refptr<MockQuotaManagerProxy> quota_manager_proxy_;

 private:
  TestBrowserThreadBundle thread_bundle_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBTest);
};

TEST_F(IndexedDBTest, ClearSessionOnlyDatabases) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath normal_path;
  base::FilePath session_only_path;

  // Create the scope which will ensure we run the destructor of the context
  // which should trigger the clean up.
  {
    auto idb_context = base::MakeRefCounted<IndexedDBContextImpl>(
        temp_dir.GetPath(), special_storage_policy_.get(),
        quota_manager_proxy_.get());

    normal_path = idb_context->GetFilePathForTesting(kNormalOrigin);
    session_only_path = idb_context->GetFilePathForTesting(kSessionOnlyOrigin);
    ASSERT_TRUE(base::CreateDirectory(normal_path));
    ASSERT_TRUE(base::CreateDirectory(session_only_path));
    RunAllTasksUntilIdle();
    quota_manager_proxy_->SimulateQuotaManagerDestroyed();
  }

  RunAllTasksUntilIdle();

  EXPECT_TRUE(base::DirectoryExists(normal_path));
  EXPECT_FALSE(base::DirectoryExists(session_only_path));
}

TEST_F(IndexedDBTest, SetForceKeepSessionState) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath normal_path;
  base::FilePath session_only_path;

  // Create the scope which will ensure we run the destructor of the context.
  {
    // Create some indexedDB paths.
    // With the levelDB backend, these are directories.
    auto idb_context = base::MakeRefCounted<IndexedDBContextImpl>(
        temp_dir.GetPath(), special_storage_policy_.get(),
        quota_manager_proxy_.get());

    // Save session state. This should bypass the destruction-time deletion.
    idb_context->SetForceKeepSessionState();

    normal_path = idb_context->GetFilePathForTesting(kNormalOrigin);
    session_only_path = idb_context->GetFilePathForTesting(kSessionOnlyOrigin);
    ASSERT_TRUE(base::CreateDirectory(normal_path));
    ASSERT_TRUE(base::CreateDirectory(session_only_path));
    base::RunLoop().RunUntilIdle();
  }

  // Make sure we wait until the destructor has run.
  base::RunLoop().RunUntilIdle();

  // No data was cleared because of SetForceKeepSessionState.
  EXPECT_TRUE(base::DirectoryExists(normal_path));
  EXPECT_TRUE(base::DirectoryExists(session_only_path));
}

class ForceCloseDBCallbacks : public IndexedDBCallbacks {
 public:
  ForceCloseDBCallbacks(scoped_refptr<IndexedDBContextImpl> idb_context,
                        const Origin& origin)
      : IndexedDBCallbacks(nullptr, origin, nullptr, idb_context->TaskRunner()),
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
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  auto idb_context = base::MakeRefCounted<IndexedDBContextImpl>(
      temp_dir.GetPath(), special_storage_policy_.get(),
      quota_manager_proxy_.get());

  const Origin kTestOrigin = Origin::Create(GURL("http://test/"));

  auto open_db_callbacks =
      base::MakeRefCounted<MockIndexedDBDatabaseCallbacks>();
  auto closed_db_callbacks =
      base::MakeRefCounted<MockIndexedDBDatabaseCallbacks>();
  auto open_callbacks =
      base::MakeRefCounted<ForceCloseDBCallbacks>(idb_context, kTestOrigin);
  auto closed_callbacks =
      base::MakeRefCounted<ForceCloseDBCallbacks>(idb_context, kTestOrigin);

  base::RunLoop loop;
  idb_context->TaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        const int child_process_id = 0;
        const int64_t host_transaction_id = 0;
        const int64_t version = 0;

        IndexedDBFactory* factory = idb_context->GetIDBFactory();

        base::FilePath test_path =
            idb_context->GetFilePathForTesting(kTestOrigin);

        factory->Open(base::ASCIIToUTF16("opendb"),
                      std::make_unique<IndexedDBPendingConnection>(
                          open_callbacks, open_db_callbacks, child_process_id,
                          host_transaction_id, version),
                      kTestOrigin, idb_context->data_path());
        EXPECT_TRUE(base::DirectoryExists(test_path));

        factory->Open(base::ASCIIToUTF16("closeddb"),
                      std::make_unique<IndexedDBPendingConnection>(
                          closed_callbacks, closed_db_callbacks,
                          child_process_id, host_transaction_id, version),
                      kTestOrigin, idb_context->data_path());

        closed_callbacks->connection()->Close();

        idb_context->DeleteForOrigin(kTestOrigin);

        EXPECT_TRUE(open_db_callbacks->forced_close_called());
        EXPECT_FALSE(closed_db_callbacks->forced_close_called());
        EXPECT_FALSE(base::DirectoryExists(test_path));
        loop.Quit();
      }));
  loop.Run();
}

TEST_F(IndexedDBTest, DeleteFailsIfDirectoryLocked) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const Origin kTestOrigin = Origin::Create(GURL("http://test/"));

  auto idb_context = base::MakeRefCounted<IndexedDBContextImpl>(
      temp_dir.GetPath(), special_storage_policy_.get(),
      quota_manager_proxy_.get());

  base::FilePath test_path = idb_context->GetFilePathForTesting(kTestOrigin);
  ASSERT_TRUE(base::CreateDirectory(test_path));

  auto lock = LevelDBDatabase::LockForTesting(test_path);
  ASSERT_TRUE(lock);

  base::RunLoop loop;
  idb_context->TaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        idb_context->DeleteForOrigin(kTestOrigin);
        loop.Quit();
      }));
  loop.Run();

  EXPECT_TRUE(base::DirectoryExists(test_path));
}

TEST_F(IndexedDBTest, ForceCloseOpenDatabasesOnCommitFailure) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  auto idb_context = base::MakeRefCounted<IndexedDBContextImpl>(
      temp_dir.GetPath(), special_storage_policy_.get(),
      quota_manager_proxy_.get());

  auto temp_path = temp_dir.GetPath();
  auto callbacks = base::MakeRefCounted<MockIndexedDBCallbacks>();
  auto db_callbacks = base::MakeRefCounted<MockIndexedDBDatabaseCallbacks>();

  base::RunLoop loop;
  idb_context->TaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        const Origin kTestOrigin = Origin::Create(GURL("http://test/"));

        auto* factory =
            static_cast<IndexedDBFactoryImpl*>(idb_context->GetIDBFactory());

        const int child_process_id = 0;
        const int64_t transaction_id = 1;

        auto connection = std::make_unique<IndexedDBPendingConnection>(
            callbacks, db_callbacks, child_process_id, transaction_id,
            IndexedDBDatabaseMetadata::DEFAULT_VERSION);
        factory->Open(base::ASCIIToUTF16("db"), std::move(connection),
                      Origin(kTestOrigin), temp_path);

        EXPECT_TRUE(callbacks->connection());

        // ConnectionOpened() is usually called by the dispatcher.
        idb_context->ConnectionOpened(kTestOrigin, callbacks->connection());

        EXPECT_TRUE(factory->IsBackingStoreOpen(kTestOrigin));

        // Simulate the write failure.
        leveldb::Status status = leveldb::Status::IOError("Simulated failure");
        factory->HandleBackingStoreFailure(kTestOrigin);

        EXPECT_TRUE(db_callbacks->forced_close_called());
        EXPECT_FALSE(factory->IsBackingStoreOpen(kTestOrigin));

        loop.Quit();
      }));
  loop.Run();
}

}  // namespace content
