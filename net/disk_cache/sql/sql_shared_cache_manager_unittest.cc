// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/sql_shared_cache_manager.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "net/base/features.h"
#include "net/base/network_isolation_key.h"
#include "net/base/schemeful_site.h"
#include "net/disk_cache/backend_cleanup_tracker.h"
#include "net/disk_cache/sql/mock_shared_cache_client_remote.h"
#include "net/disk_cache/sql/sql_persistent_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace disk_cache {

class SqlSharedCacheManagerTest : public testing::TestWithParam<bool> {
 public:
  static std::string DescribeParams(
      const testing::TestParamInfo<ParamType>& info) {
    return info.param ? "WalEnabled" : "WalDisabled";
  }

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    cleanup_tracker_ = BackendCleanupTracker::TryCreate(temp_dir_.GetPath(),
                                                        base::DoNothing());
    CHECK(cleanup_tracker_);
    if (GetParam()) {
      feature_list_.InitWithFeaturesAndParameters(
          {{net::features::kRendererAccessibleHttpCache,
            {{net::features::kRendererAccessibleHttpCacheWalMode.name,
              "true"}}}},
          {});
    } else {
      feature_list_.InitWithFeaturesAndParameters(
          {{net::features::kRendererAccessibleHttpCache,
            {{net::features::kRendererAccessibleHttpCacheWalMode.name,
              "false"}}}},
          {});
    }
    task_runners_.push_back(base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
         base::TaskShutdownBehavior::BLOCK_SHUTDOWN}));
    CreateAndInitStore();
  }

  void TearDown() override {
    store_.reset();
    FlushPendingTask();
    WaitForCleanup();
  }

 protected:
  void WaitForCleanup() {
    if (!cleanup_tracker_) {
      return;
    }
    base::RunLoop run_loop;
    cleanup_tracker_->AddPostCleanupCallback(run_loop.QuitClosure());
    cleanup_tracker_ = nullptr;
    run_loop.Run();
    cleanup_tracker_ = BackendCleanupTracker::TryCreate(temp_dir_.GetPath(),
                                                        base::DoNothing());
    CHECK(cleanup_tracker_);
  }

  void CreateStore(int64_t max_bytes = 0) {
    CHECK(!store_);
    store_ = std::make_unique<SqlPersistentStore>(
        temp_dir_.GetPath(), max_bytes, net::CacheType::DISK_CACHE,
        task_runners_, async_task_manager_, cleanup_tracker_);
  }

  SqlPersistentStore::Error Init() {
    base::test::TestFuture<SqlPersistentStore::Error> future;
    store_->Initialize(future.GetCallback());
    return future.Get();
  }

  void CreateAndInitStore() {
    CreateStore();
    ASSERT_EQ(Init(), SqlPersistentStore::Error::kOk);
  }

  SqlSharedCacheManager* GetManager() {
    return store_->shared_cache_manager_for_testing();
  }

  net::NetworkIsolationKey CreateNik(const std::string& url_str) {
    net::SchemefulSite site((GURL(url_str)));
    return net::NetworkIsolationKey(site, site);
  }

  void GetCacheByNik(
      const net::NetworkIsolationKey& nik,
      bool require_shared_cache_db_id,
      base::OnceCallback<void(scoped_refptr<SqlSharedCacheHandle>)> callback) {
    GetManager()->GetCacheByNik(nik, require_shared_cache_db_id,
                                std::move(callback));
  }

  void FlushPendingTask() {
    async_task_manager_.RunUntilAllTasksCompleteForTest();
  }

  base::test::ScopedFeatureList feature_list_;
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::vector<scoped_refptr<base::SequencedTaskRunner>> task_runners_;
  SqlAsyncTaskManager async_task_manager_;
  std::unique_ptr<SqlPersistentStore> store_;
  scoped_refptr<BackendCleanupTracker> cleanup_tracker_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         SqlSharedCacheManagerTest,
                         testing::Bool(),
                         &SqlSharedCacheManagerTest::DescribeParams);

TEST_P(SqlSharedCacheManagerTest, GetCacheByNikWithoutDbId) {
  net::NetworkIsolationKey nik(net::SchemefulSite(GURL("https://foo.test")),
                               net::SchemefulSite(GURL("https://bar.test")));

  scoped_refptr<SqlSharedCacheHandle> handle;
  bool callback_run = false;

  GetManager()->GetCacheByNik(
      nik, /*require_shared_cache_db_id=*/false,
      base::BindLambdaForTesting([&](scoped_refptr<SqlSharedCacheHandle> h) {
        handle = std::move(h);
        callback_run = true;
      }));

  FlushPendingTask();
  EXPECT_TRUE(callback_run);
  ASSERT_TRUE(handle);
  EXPECT_TRUE(handle->get() != nullptr);
  EXPECT_FALSE((*handle)->shared_cache_db_id().has_value());

  // Getting again with same NIK should return handle to same cache instance.
  scoped_refptr<SqlSharedCacheHandle> handle2;
  bool callback2_run = false;

  GetManager()->GetCacheByNik(
      nik, /*require_shared_cache_db_id=*/false,
      base::BindLambdaForTesting([&](scoped_refptr<SqlSharedCacheHandle> h) {
        handle2 = std::move(h);
        callback2_run = true;
      }));

  FlushPendingTask();
  EXPECT_TRUE(callback2_run);
  ASSERT_TRUE(handle2);
  EXPECT_EQ(handle->get(), handle2->get());
}

TEST_P(SqlSharedCacheManagerTest, GetCacheByNikWithDbId) {
  net::NetworkIsolationKey nik(net::SchemefulSite(GURL("https://foo.test")),
                               net::SchemefulSite(GURL("https://bar.test")));

  scoped_refptr<SqlSharedCacheHandle> handle;
  bool callback_run = false;

  GetManager()->GetCacheByNik(
      nik, /*require_shared_cache_db_id=*/true,
      base::BindLambdaForTesting([&](scoped_refptr<SqlSharedCacheHandle> h) {
        handle = std::move(h);
        callback_run = true;
      }));

  FlushPendingTask();
  EXPECT_TRUE(callback_run);
  ASSERT_TRUE(handle);
  EXPECT_TRUE(handle->get() != nullptr);
  EXPECT_TRUE((*handle)->shared_cache_db_id().has_value());

  SqlSharedCacheDbId db_id = *(*handle)->shared_cache_db_id();

  // Now query by DbId
  scoped_refptr<SqlSharedCacheHandle> handle_by_id;
  bool callback_by_id_run = false;

  GetManager()->GetCacheByDbId(
      db_id,
      base::BindLambdaForTesting([&](scoped_refptr<SqlSharedCacheHandle> h) {
        handle_by_id = std::move(h);
        callback_by_id_run = true;
      }));

  FlushPendingTask();
  EXPECT_TRUE(callback_by_id_run);
  ASSERT_TRUE(handle_by_id);
  EXPECT_EQ(handle->get(), handle_by_id->get());
}

TEST_P(SqlSharedCacheManagerTest, GetCacheByNikUpgradeToDbId) {
  net::NetworkIsolationKey nik(net::SchemefulSite(GURL("https://foo.test")),
                               net::SchemefulSite(GURL("https://bar.test")));

  // First create without DbId requirement
  scoped_refptr<SqlSharedCacheHandle> handle;
  GetManager()->GetCacheByNik(
      nik, /*require_shared_cache_db_id=*/false,
      base::BindLambdaForTesting([&](scoped_refptr<SqlSharedCacheHandle> h) {
        handle = std::move(h);
      }));
  FlushPendingTask();
  ASSERT_TRUE(handle);
  EXPECT_FALSE((*handle)->shared_cache_db_id().has_value());

  // Request again with require_shared_cache_db_id = true
  scoped_refptr<SqlSharedCacheHandle> handle_with_id;
  GetManager()->GetCacheByNik(
      nik, /*require_shared_cache_db_id=*/true,
      base::BindLambdaForTesting([&](scoped_refptr<SqlSharedCacheHandle> h) {
        handle_with_id = std::move(h);
      }));
  FlushPendingTask();
  ASSERT_TRUE(handle_with_id);
  EXPECT_EQ(handle->get(), handle_with_id->get());
  EXPECT_TRUE((*handle_with_id)->shared_cache_db_id().has_value());
}

TEST_P(SqlSharedCacheManagerTest, GetCacheByDbIdNonExistent) {
  scoped_refptr<SqlSharedCacheHandle> handle;
  bool callback_run = false;

  GetManager()->GetCacheByDbId(
      SqlSharedCacheDbId(99999),
      base::BindLambdaForTesting([&](scoped_refptr<SqlSharedCacheHandle> h) {
        handle = std::move(h);
        callback_run = true;
      }));

  FlushPendingTask();
  EXPECT_TRUE(callback_run);
  EXPECT_FALSE(handle);
}

TEST_P(SqlSharedCacheManagerTest, CacheUnreferencedDeletion) {
  net::NetworkIsolationKey nik(net::SchemefulSite(GURL("https://foo.test")),
                               net::SchemefulSite(GURL("https://bar.test")));

  {
    scoped_refptr<SqlSharedCacheHandle> handle;
    GetManager()->GetCacheByNik(
        nik, /*require_shared_cache_db_id=*/false,
        base::BindLambdaForTesting([&](scoped_refptr<SqlSharedCacheHandle> h) {
          handle = std::move(h);
        }));
    FlushPendingTask();
    ASSERT_TRUE(handle);
  }

  // Handle went out of scope, unreferenced task should post and run.
  FlushPendingTask();

  // Fetching again should create a new cache instance.
  scoped_refptr<SqlSharedCacheHandle> new_handle;
  GetManager()->GetCacheByNik(
      nik, /*require_shared_cache_db_id=*/false,
      base::BindLambdaForTesting([&](scoped_refptr<SqlSharedCacheHandle> h) {
        new_handle = std::move(h);
      }));
  FlushPendingTask();
  ASSERT_TRUE(new_handle);
}

TEST_P(SqlSharedCacheManagerTest, DestructionTriggersCleanup) {
  net::NetworkIsolationKey nik(net::SchemefulSite(GURL("https://foo.test")),
                               net::SchemefulSite(GURL("https://bar.test")));

  scoped_refptr<SqlSharedCacheHandle> handle;
  GetManager()->GetCacheByNik(
      nik, /*require_shared_cache_db_id=*/true,
      base::BindLambdaForTesting([&](scoped_refptr<SqlSharedCacheHandle> h) {
        handle = std::move(h);
      }));
  FlushPendingTask();
  ASSERT_TRUE(handle);

  handle.reset();
  FlushPendingTask();

  // Destroying store resets `shared_cache_manager_` which triggers Close in
  // destructor.
  store_.reset();

  // `RunUntilAllTasksCompleteForTest()` waits for the async close task to
  // complete.
  FlushPendingTask();
  WaitForCleanup();

  // Re-creating and initializing a new store (and thus SqlSharedCacheManager)
  // should succeed.
  CreateAndInitStore();
}

TEST_P(SqlSharedCacheManagerTest, DeleteResourcesEmpty) {
  base::test::TestFuture<void> future;
  GetManager()->DeleteResources({}, future.GetCallback());
  FlushPendingTask();
  EXPECT_TRUE(future.IsReady());
}

TEST_P(SqlSharedCacheManagerTest, DeleteResourcesNonExistentDbId) {
  base::test::TestFuture<void> future;
  GetManager()->DeleteResources(
      {{SqlSharedCacheDbId(99999), SqlSharedCacheRowId(1)}},
      future.GetCallback());
  FlushPendingTask();
  EXPECT_TRUE(future.IsReady());
}

TEST_P(SqlSharedCacheManagerTest, DeleteResourcesSingleCache) {
  net::NetworkIsolationKey nik(net::SchemefulSite(GURL("https://foo.test")),
                               net::SchemefulSite(GURL("https://bar.test")));

  scoped_refptr<SqlSharedCacheHandle> handle;
  GetManager()->GetCacheByNik(
      nik, /*require_shared_cache_db_id=*/true,
      base::BindLambdaForTesting([&](scoped_refptr<SqlSharedCacheHandle> h) {
        handle = std::move(h);
      }));
  FlushPendingTask();
  ASSERT_TRUE(handle);
  ASSERT_TRUE((*handle)->shared_cache_db_id().has_value());
  SqlSharedCacheDbId db_id = *(*handle)->shared_cache_db_id();

  // Insert two test entries.
  CacheEntryKey key1("0/0/https://example.com/1");
  CacheEntryKey key2("0/0/https://example.com/2");
  auto headers = base::MakeRefCounted<net::IOBufferWithSize>(4);
  headers->span().copy_from(base::span<const uint8_t>({1, 2, 3, 4}));
  auto body = base::MakeRefCounted<net::IOBufferWithSize>(3);
  body->span().copy_from(base::span<const uint8_t>({5, 6, 7}));

  base::test::TestFuture<base::expected<SqlSharedCacheRowId,
                                        SqlSharedCacheIsolatedDatabase::Error>>
      insert_future1;
  (*handle)
      ->isolated_database_for_testing()
      .AsyncCall(&SqlSharedCacheIsolatedDatabase::Insert)
      .WithArgs(key1, headers, 3, body)
      .Then(insert_future1.GetCallback());
  base::test::TestFuture<base::expected<SqlSharedCacheRowId,
                                        SqlSharedCacheIsolatedDatabase::Error>>
      insert_future2;
  (*handle)
      ->isolated_database_for_testing()
      .AsyncCall(&SqlSharedCacheIsolatedDatabase::Insert)
      .WithArgs(key2, headers, 3, body)
      .Then(insert_future2.GetCallback());
  FlushPendingTask();

  auto insert_res1 = insert_future1.Take();
  auto insert_res2 = insert_future2.Take();
  ASSERT_TRUE(insert_res1.has_value());
  ASSERT_TRUE(insert_res2.has_value());
  SqlSharedCacheRowId row1 = *insert_res1;
  SqlSharedCacheRowId row2 = *insert_res2;

  // Verify entries can be read before deletion.
  auto read_buf = base::MakeRefCounted<net::IOBufferWithSize>(3);
  base::test::TestFuture<SqlSharedCacheIsolatedDatabase::ReadResultOrError>
      read_before_future;
  (*handle)
      ->isolated_database_for_testing()
      .AsyncCall(&SqlSharedCacheIsolatedDatabase::Read)
      .WithArgs(key1, row1, /*body_size=*/3, /*offset=*/0, read_buf)
      .Then(read_before_future.GetCallback());
  FlushPendingTask();
  EXPECT_TRUE(read_before_future.Get().has_value());

  // Delete resources using SqlSharedCacheManager.
  base::test::TestFuture<void> delete_future;
  GetManager()->DeleteResources({{db_id, row1}, {db_id, row2}},
                                delete_future.GetCallback());
  FlushPendingTask();
  EXPECT_TRUE(delete_future.IsReady());

  // Verify entries cannot be read after deletion.
  base::test::TestFuture<SqlSharedCacheIsolatedDatabase::ReadResultOrError>
      read_after_future1;
  (*handle)
      ->isolated_database_for_testing()
      .AsyncCall(&SqlSharedCacheIsolatedDatabase::Read)
      .WithArgs(key1, row1, /*body_size=*/3, /*offset=*/0, read_buf)
      .Then(read_after_future1.GetCallback());
  base::test::TestFuture<SqlSharedCacheIsolatedDatabase::ReadResultOrError>
      read_after_future2;
  (*handle)
      ->isolated_database_for_testing()
      .AsyncCall(&SqlSharedCacheIsolatedDatabase::Read)
      .WithArgs(key2, row2, /*body_size=*/3, /*offset=*/0, read_buf)
      .Then(read_after_future2.GetCallback());
  FlushPendingTask();

  auto read_res1 = read_after_future1.Take();
  auto read_res2 = read_after_future2.Take();
  EXPECT_FALSE(read_res1.has_value());
  EXPECT_EQ(read_res1.error(),
            SqlSharedCacheIsolatedDatabase::Error::kEntryNotFound);
  EXPECT_FALSE(read_res2.has_value());
  EXPECT_EQ(read_res2.error(),
            SqlSharedCacheIsolatedDatabase::Error::kEntryNotFound);
}

TEST_P(SqlSharedCacheManagerTest, DeleteResourcesMultipleCaches) {
  net::NetworkIsolationKey nik1(net::SchemefulSite(GURL("https://foo1.test")),
                                net::SchemefulSite(GURL("https://bar1.test")));
  net::NetworkIsolationKey nik2(net::SchemefulSite(GURL("https://foo2.test")),
                                net::SchemefulSite(GURL("https://bar2.test")));

  scoped_refptr<SqlSharedCacheHandle> handle1;
  GetManager()->GetCacheByNik(
      nik1, /*require_shared_cache_db_id=*/true,
      base::BindLambdaForTesting([&](scoped_refptr<SqlSharedCacheHandle> h) {
        handle1 = std::move(h);
      }));
  scoped_refptr<SqlSharedCacheHandle> handle2;
  GetManager()->GetCacheByNik(
      nik2, /*require_shared_cache_db_id=*/true,
      base::BindLambdaForTesting([&](scoped_refptr<SqlSharedCacheHandle> h) {
        handle2 = std::move(h);
      }));
  FlushPendingTask();
  ASSERT_TRUE(handle1);
  ASSERT_TRUE(handle2);
  ASSERT_TRUE((*handle1)->shared_cache_db_id().has_value());
  ASSERT_TRUE((*handle2)->shared_cache_db_id().has_value());

  SqlSharedCacheDbId db_id1 = *(*handle1)->shared_cache_db_id();
  SqlSharedCacheDbId db_id2 = *(*handle2)->shared_cache_db_id();

  // Insert entries into cache 1 and cache 2.
  CacheEntryKey key1("0/0/https://example.com/1");
  CacheEntryKey key2("0/0/https://example.com/2");
  auto headers = base::MakeRefCounted<net::IOBufferWithSize>(4);
  headers->span().copy_from(base::span<const uint8_t>({1, 2, 3, 4}));
  auto body = base::MakeRefCounted<net::IOBufferWithSize>(3);
  body->span().copy_from(base::span<const uint8_t>({5, 6, 7}));

  base::test::TestFuture<base::expected<SqlSharedCacheRowId,
                                        SqlSharedCacheIsolatedDatabase::Error>>
      insert_future1;
  (*handle1)
      ->isolated_database_for_testing()
      .AsyncCall(&SqlSharedCacheIsolatedDatabase::Insert)
      .WithArgs(key1, headers, 3, body)
      .Then(insert_future1.GetCallback());
  base::test::TestFuture<base::expected<SqlSharedCacheRowId,
                                        SqlSharedCacheIsolatedDatabase::Error>>
      insert_future2;
  (*handle2)
      ->isolated_database_for_testing()
      .AsyncCall(&SqlSharedCacheIsolatedDatabase::Insert)
      .WithArgs(key2, headers, 3, body)
      .Then(insert_future2.GetCallback());
  FlushPendingTask();

  auto insert_res1 = insert_future1.Take();
  auto insert_res2 = insert_future2.Take();
  ASSERT_TRUE(insert_res1.has_value());
  ASSERT_TRUE(insert_res2.has_value());
  SqlSharedCacheRowId row1 = *insert_res1;
  SqlSharedCacheRowId row2 = *insert_res2;

  // Delete resources across multiple caches.
  base::test::TestFuture<void> delete_future;
  GetManager()->DeleteResources({{db_id1, row1}, {db_id2, row2}},
                                delete_future.GetCallback());
  FlushPendingTask();
  EXPECT_TRUE(delete_future.IsReady());

  // Verify entries in both caches cannot be read after deletion.
  auto read_buf = base::MakeRefCounted<net::IOBufferWithSize>(3);
  base::test::TestFuture<SqlSharedCacheIsolatedDatabase::ReadResultOrError>
      read_after_future1;
  (*handle1)
      ->isolated_database_for_testing()
      .AsyncCall(&SqlSharedCacheIsolatedDatabase::Read)
      .WithArgs(key1, row1, /*body_size=*/3, /*offset=*/0, read_buf)
      .Then(read_after_future1.GetCallback());
  base::test::TestFuture<SqlSharedCacheIsolatedDatabase::ReadResultOrError>
      read_after_future2;
  (*handle2)
      ->isolated_database_for_testing()
      .AsyncCall(&SqlSharedCacheIsolatedDatabase::Read)
      .WithArgs(key2, row2, /*body_size=*/3, /*offset=*/0, read_buf)
      .Then(read_after_future2.GetCallback());
  FlushPendingTask();

  auto read_res1 = read_after_future1.Take();
  auto read_res2 = read_after_future2.Take();
  EXPECT_FALSE(read_res1.has_value());
  EXPECT_EQ(read_res1.error(),
            SqlSharedCacheIsolatedDatabase::Error::kEntryNotFound);
  EXPECT_FALSE(read_res2.has_value());
  EXPECT_EQ(read_res2.error(),
            SqlSharedCacheIsolatedDatabase::Error::kEntryNotFound);
}

TEST_P(SqlSharedCacheManagerTest, RegisterClientNewCache) {
  auto* manager = GetManager();
  net::NetworkIsolationKey nik = CreateNik("https://example.com");

  auto client = std::make_unique<MockSharedCacheClientRemote>();
  auto* client_ptr = client.get();

  EXPECT_EQ(manager->GetSharedCachesSizeForTest(), 0u);

  manager->RegisterClient(nik, std::move(client));

  // Wait for the cache to be registered in the manager, which involves
  // async DB access. The disconnect handler is set during cache creation.
  client_ptr->WaitUntilDisconnectHandlerSet();

  // The cache is created but client is not initialized because there is no DB
  // ID yet.
  EXPECT_EQ(manager->GetSharedCachesSizeForTest(), 1u);
  EXPECT_FALSE(client_ptr->initialize_called());
}

TEST_P(SqlSharedCacheManagerTest, RegisterClientExistingCacheNoDbId) {
  auto* manager = GetManager();
  net::NetworkIsolationKey nik = CreateNik("https://example.com");

  auto client1 = std::make_unique<MockSharedCacheClientRemote>();
  auto* client_ptr1 = client1.get();

  auto client2 = std::make_unique<MockSharedCacheClientRemote>();
  auto* client_ptr2 = client2.get();

  EXPECT_EQ(manager->GetSharedCachesSizeForTest(), 0u);

  // Register the first client.
  manager->RegisterClient(nik, std::move(client1));

  // Register the second client immediately before db_task_runner finishes.
  manager->RegisterClient(nik, std::move(client2));

  client_ptr1->WaitUntilDisconnectHandlerSet();
  client_ptr2->WaitUntilDisconnectHandlerSet();

  // Both clients registered, but neither is initialized.
  EXPECT_EQ(manager->GetSharedCachesSizeForTest(), 1u);
  EXPECT_FALSE(client_ptr1->initialize_called());
  EXPECT_FALSE(client_ptr2->initialize_called());

  // Requesting the cache with require_shared_cache_db_id=true allocates a DB ID
  // and initializes the underlying isolated database.
  base::test::TestFuture<scoped_refptr<SqlSharedCacheHandle>> future;
  GetCacheByNik(nik, /*require_shared_cache_db_id=*/true, future.GetCallback());
  scoped_refptr<SqlSharedCacheHandle> keep_alive_handle = future.Take();
  ASSERT_TRUE(keep_alive_handle);

  // Both clients should now be initialized.
  client_ptr1->WaitUntilInitialized();
  client_ptr2->WaitUntilInitialized();

  EXPECT_EQ(client_ptr1->initialize_call_count(), 1u);
  EXPECT_EQ(client_ptr2->initialize_call_count(), 1u);
}

TEST_P(SqlSharedCacheManagerTest,
       RegisterClientWhileInitIsolatedDatabaseInFlight) {
  auto* manager = GetManager();
  net::NetworkIsolationKey nik = CreateNik("https://example.com");

  auto client1 = std::make_unique<MockSharedCacheClientRemote>();
  auto* client_ptr1 = client1.get();

  auto client2 = std::make_unique<MockSharedCacheClientRemote>();
  auto* client_ptr2 = client2.get();

  auto client3 = std::make_unique<MockSharedCacheClientRemote>();
  auto* client_ptr3 = client3.get();

  // 1. Register client1 before InitIsolatedDatabase.
  manager->RegisterClient(nik, std::move(client1));
  client_ptr1->WaitUntilDisconnectHandlerSet();
  EXPECT_EQ(client_ptr1->initialize_call_count(), 0u);

  // 2. Request cache with require_shared_cache_db_id=true to trigger
  // InitIsolatedDatabase asynchronously.
  base::test::TestFuture<scoped_refptr<SqlSharedCacheHandle>> future;
  GetCacheByNik(nik, /*require_shared_cache_db_id=*/true, future.GetCallback());

  // 3. Immediately register client2 while InitIsolatedDatabase is in flight.
  manager->RegisterClient(nik, std::move(client2));

  scoped_refptr<SqlSharedCacheHandle> keep_alive_handle = future.Take();
  ASSERT_TRUE(keep_alive_handle);

  // 4. Register client3 after DB initialization is complete.
  manager->RegisterClient(nik, std::move(client3));

  // Wait for all clients to be initialized.
  client_ptr1->WaitUntilInitialized();
  client_ptr2->WaitUntilInitialized();
  client_ptr3->WaitUntilInitialized();
  FlushPendingTask();

  // Verify that each client is initialized exactly once.
  EXPECT_EQ(client_ptr1->initialize_call_count(), 1u);
  EXPECT_EQ(client_ptr2->initialize_call_count(), 1u);
  EXPECT_EQ(client_ptr3->initialize_call_count(), 1u);
}

TEST_P(SqlSharedCacheManagerTest, RegisterClientExistingCacheWithDbId) {
  auto* manager = GetManager();
  net::NetworkIsolationKey nik = CreateNik("https://example.com");

  auto client1 = std::make_unique<MockSharedCacheClientRemote>();
  auto* client_ptr1 = client1.get();

  EXPECT_EQ(manager->GetSharedCachesSizeForTest(), 0u);

  // Force creation of a DB ID by requesting with
  // require_shared_cache_db_id=true.
  base::test::TestFuture<scoped_refptr<SqlSharedCacheHandle>> future;
  GetCacheByNik(nik, /*require_shared_cache_db_id=*/true, future.GetCallback());
  scoped_refptr<SqlSharedCacheHandle> keep_alive_handle = future.Get();

  EXPECT_TRUE(keep_alive_handle);

  EXPECT_EQ(manager->GetSharedCachesSizeForTest(), 1u);
  EXPECT_EQ(manager->GetSharedCachesByDbIdSizeForTest(), 1u);

  // Register the first client. The cache should already have a db_id.
  manager->RegisterClient(nik, std::move(client1));

  // Now it should be initialized because the DB ID exists.
  client_ptr1->WaitUntilInitialized();
  EXPECT_TRUE(client_ptr1->initialize_called());
}

TEST_P(SqlSharedCacheManagerTest, RegisterClientDifferentNik) {
  auto* manager = GetManager();
  net::NetworkIsolationKey nik1 = CreateNik("https://example.com");
  net::NetworkIsolationKey nik2 = CreateNik("https://example.net");

  auto client1 = std::make_unique<MockSharedCacheClientRemote>();
  auto* client_ptr1 = client1.get();

  auto client2 = std::make_unique<MockSharedCacheClientRemote>();
  auto* client_ptr2 = client2.get();

  EXPECT_EQ(manager->GetSharedCachesSizeForTest(), 0u);

  manager->RegisterClient(nik1, std::move(client1));
  manager->RegisterClient(nik2, std::move(client2));

  client_ptr1->WaitUntilDisconnectHandlerSet();
  client_ptr2->WaitUntilDisconnectHandlerSet();

  EXPECT_EQ(manager->GetSharedCachesSizeForTest(), 2u);
  EXPECT_FALSE(client_ptr1->initialize_called());
  EXPECT_FALSE(client_ptr2->initialize_called());
}

TEST_P(SqlSharedCacheManagerTest, ClientDisconnected) {
  auto* manager = GetManager();
  net::NetworkIsolationKey nik = CreateNik("https://example.com");

  auto client = std::make_unique<MockSharedCacheClientRemote>();
  auto* client_ptr = client.get();

  manager->RegisterClient(nik, std::move(client));
  client_ptr->WaitUntilDisconnectHandlerSet();

  EXPECT_EQ(manager->GetSharedCachesSizeForTest(), 1u);
  EXPECT_TRUE(client_ptr->has_disconnect_handler());

  // Simulate client disconnection.
  client_ptr->RunDisconnectHandler();

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return manager->GetSharedCachesSizeForTest() == 0u; }));
}

TEST_P(SqlSharedCacheManagerTest, ClientDisconnectBeforeDbInit) {
  auto* manager = GetManager();
  net::NetworkIsolationKey nik = CreateNik("https://example.com");

  auto client = std::make_unique<MockSharedCacheClientRemote>();
  auto* client_ptr = client.get();

  manager->RegisterClient(nik, std::move(client));
  client_ptr->WaitUntilDisconnectHandlerSet();

  EXPECT_EQ(manager->GetSharedCachesSizeForTest(), 1u);

  // Disconnect before DB initialization.
  client_ptr->RunDisconnectHandler();

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return manager->GetSharedCachesSizeForTest() == 0u; }));

  FlushPendingTask();
  EXPECT_EQ(manager->GetSharedCachesSizeForTest(), 0u);
}

TEST_P(SqlSharedCacheManagerTest, PartialClientDisconnect) {
  auto* manager = GetManager();
  net::NetworkIsolationKey nik = CreateNik("https://example.com");

  auto client1 = std::make_unique<MockSharedCacheClientRemote>();
  auto* client_ptr1 = client1.get();

  auto client2 = std::make_unique<MockSharedCacheClientRemote>();
  auto* client_ptr2 = client2.get();

  manager->RegisterClient(nik, std::move(client1));
  manager->RegisterClient(nik, std::move(client2));

  client_ptr1->WaitUntilDisconnectHandlerSet();
  client_ptr2->WaitUntilDisconnectHandlerSet();

  EXPECT_EQ(manager->GetSharedCachesSizeForTest(), 1u);

  // Disconnect the first client. The cache should remain alive because client2
  // is still connected.
  client_ptr1->RunDisconnectHandler();

  FlushPendingTask();
  EXPECT_EQ(manager->GetSharedCachesSizeForTest(), 1u);

  // Disconnect the second client. Now the cache should be destroyed.
  client_ptr2->RunDisconnectHandler();

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return manager->GetSharedCachesSizeForTest() == 0u; }));
}

TEST_P(SqlSharedCacheManagerTest, RegisterClientDbFailure) {
  auto* manager = GetManager();
  manager->SetSimulateDbFailureForTesting(true);

  net::NetworkIsolationKey nik = CreateNik("https://example.com");

  base::RunLoop destroy_run_loop;
  auto client = std::make_unique<MockSharedCacheClientRemote>();
  client->SetOnDestroyHandler(destroy_run_loop.QuitClosure());

  EXPECT_EQ(manager->GetSharedCachesSizeForTest(), 0u);

  manager->RegisterClient(nik, std::move(client));

  // The client should be destroyed because DB initialization fails and
  // the manager drops the client.
  destroy_run_loop.Run();

  EXPECT_EQ(manager->GetSharedCachesSizeForTest(), 0u);
}

TEST_P(SqlSharedCacheManagerTest,
       RegisterClientExistingCacheWithDbIdAndHashes) {
  auto* manager = GetManager();
  net::NetworkIsolationKey nik = CreateNik("https://example.com");

  // Force creation of a DB ID.
  base::test::TestFuture<scoped_refptr<SqlSharedCacheHandle>> future;
  GetCacheByNik(nik, /*require_shared_cache_db_id=*/true, future.GetCallback());
  scoped_refptr<SqlSharedCacheHandle> keep_alive_handle = future.Take();
  ASSERT_TRUE(keep_alive_handle);

  // Insert an entry into the isolated database.
  const std::string kKey1 = "0/0/https://example.com/res1";
  const std::string kKey2 = "0/0/https://example.com/res2";
  CacheEntryKey entry_key1(kKey1);
  CacheEntryKey entry_key2(kKey2);
  const std::string kData = "Data";
  auto buffer = base::MakeRefCounted<net::StringIOBuffer>(kData);

  base::test::TestFuture<base::expected<SqlSharedCacheRowId,
                                        SqlSharedCacheIsolatedDatabase::Error>>
      insert_future1;
  (*keep_alive_handle)
      ->isolated_database_for_testing()
      .AsyncCall(&SqlSharedCacheIsolatedDatabase::Insert)
      .WithArgs(entry_key1, /*headers=*/nullptr, kData.size(), buffer)
      .Then(insert_future1.GetCallback());
  FlushPendingTask();
  ASSERT_TRUE(insert_future1.Take().has_value());

  base::test::TestFuture<base::expected<SqlSharedCacheRowId,
                                        SqlSharedCacheIsolatedDatabase::Error>>
      insert_future2;
  (*keep_alive_handle)
      ->isolated_database_for_testing()
      .AsyncCall(&SqlSharedCacheIsolatedDatabase::Insert)
      .WithArgs(entry_key2, /*headers=*/nullptr, kData.size(), buffer)
      .Then(insert_future2.GetCallback());
  FlushPendingTask();
  ASSERT_TRUE(insert_future2.Take().has_value());

  // Release the keep-alive handle so the cache instance is destroyed.
  keep_alive_handle = nullptr;
  FlushPendingTask();
  EXPECT_EQ(manager->GetSharedCachesSizeForTest(), 0u);

  // Register client for the existing NIK with stored resources.
  // The newly created cache should load existing hashes from the database.
  auto client = std::make_unique<MockSharedCacheClientRemote>();
  auto* client_ptr = client.get();
  manager->RegisterClient(nik, std::move(client));

  client_ptr->WaitUntilInitialized();
  client_ptr->WaitUntilOnResourcesAdded();

  EXPECT_TRUE(client_ptr->initialize_called());
  EXPECT_TRUE(client_ptr->on_resources_added_called());
  EXPECT_EQ(client_ptr->new_hashes().size(), 2u);

  std::vector<uint32_t> expected_hashes = {
      static_cast<uint32_t>(entry_key1.resource_url_hash().value()),
      static_cast<uint32_t>(entry_key2.resource_url_hash().value())};
  EXPECT_THAT(client_ptr->new_hashes(),
              testing::UnorderedElementsAreArray(expected_hashes));
}

}  // namespace disk_cache
