// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/sql_shared_cache.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "net/base/features.h"
#include "net/disk_cache/sql/sql_persistent_store.h"
#include "net/disk_cache/sql/sql_shared_cache_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace disk_cache {

class SqlSharedCacheTest : public testing::TestWithParam<bool> {
 public:
  static std::string DescribeParams(
      const testing::TestParamInfo<ParamType>& info) {
    return info.param ? "WalEnabled" : "WalDisabled";
  }

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
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
    store_ = std::make_unique<SqlPersistentStore>(
        temp_dir_.GetPath(), 1024 * 1024, net::DISK_CACHE, task_runners_,
        async_task_manager_);
  }

  void TearDown() override {
    store_.reset();
    async_task_manager_.RunUntilAllTasksCompleteForTest();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::vector<scoped_refptr<base::SequencedTaskRunner>> task_runners_;
  SqlAsyncTaskManager async_task_manager_;
  std::unique_ptr<SqlPersistentStore> store_;
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         SqlSharedCacheTest,
                         testing::Bool(),
                         &SqlSharedCacheTest::DescribeParams);

TEST_P(SqlSharedCacheTest, BasicLifecycleAndHandleCount) {
  bool unreferenced_called = false;
  SqlSharedCache* unreferenced_cache_ptr = nullptr;

  auto cache = std::make_unique<SqlSharedCache>(
      "test_nik", *store_, temp_dir_.GetPath(),
      base::BindLambdaForTesting([&](SqlSharedCache& c) {
        unreferenced_called = true;
        unreferenced_cache_ptr = &c;
      }),
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN}));

  EXPECT_EQ(cache->nik_string(), "test_nik");
  EXPECT_FALSE(cache->shared_cache_db_id().has_value());
  EXPECT_FALSE(cache->IsReferenced());

  {
    scoped_refptr<SqlSharedCacheHandle> handle1 = cache->CreateHandle();
    EXPECT_TRUE(cache->IsReferenced());
    EXPECT_TRUE(handle1->get() != nullptr);
    EXPECT_EQ(handle1->get(), cache.get());

    {
      scoped_refptr<SqlSharedCacheHandle> handle2 = cache->CreateHandle();
      EXPECT_TRUE(cache->IsReferenced());
      EXPECT_FALSE(unreferenced_called);
    }
    EXPECT_TRUE(cache->IsReferenced());
    EXPECT_FALSE(unreferenced_called);
  }

  EXPECT_FALSE(cache->IsReferenced());
  EXPECT_TRUE(unreferenced_called);
  EXPECT_EQ(unreferenced_cache_ptr, cache.get());
}

TEST_P(SqlSharedCacheTest, InitIsolatedDatabaseAndCleanup) {
  auto cache = std::make_unique<SqlSharedCache>(
      "test_nik", *store_, temp_dir_.GetPath(), base::DoNothing(),
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN}));

  SqlSharedCacheDbId db_id(42);
  bool init_success = false;

  cache->InitIsolatedDatabase(
      db_id, base::BindLambdaForTesting(
                 [&](bool success) { init_success = success; }));

  async_task_manager_.RunUntilAllTasksCompleteForTest();

  EXPECT_EQ(cache->shared_cache_db_id(), db_id);
  EXPECT_TRUE(init_success);

  bool cleanup_done = false;
  cache->Cleanup(base::BindLambdaForTesting([&]() { cleanup_done = true; }));

  async_task_manager_.RunUntilAllTasksCompleteForTest();

  EXPECT_TRUE(cleanup_done);
}

TEST_P(SqlSharedCacheTest, CleanupWithoutIsolatedDatabase) {
  auto cache = std::make_unique<SqlSharedCache>(
      "test_nik", *store_, temp_dir_.GetPath(), base::DoNothing(),
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN}));

  bool cleanup_done = false;
  cache->Cleanup(base::BindLambdaForTesting([&]() { cleanup_done = true; }));

  EXPECT_TRUE(cleanup_done);
}

TEST_P(SqlSharedCacheTest, DestructionTriggersCleanup) {
  SqlSharedCacheDbId db_id(42);
  {
    auto cache = std::make_unique<SqlSharedCache>(
        "test_nik", *store_, temp_dir_.GetPath(), base::DoNothing(),
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
             base::TaskShutdownBehavior::BLOCK_SHUTDOWN}));

    bool init_success = false;
    cache->InitIsolatedDatabase(
        db_id, base::BindLambdaForTesting(
                   [&](bool success) { init_success = success; }));
    async_task_manager_.RunUntilAllTasksCompleteForTest();
    EXPECT_TRUE(init_success);
    // `cache` goes out of scope here. Its destruction triggers the destruction
    // of `isolated_database_` (SqlTrackedSequenceBound), which posts a deletion
    // task and registers it with `SqlAsyncTaskManager`.
  }

  // Verify that `RunUntilAllTasksCompleteForTest()` waits for the cleanup task
  // to complete.
  async_task_manager_.RunUntilAllTasksCompleteForTest();

  // Re-initializing another `SqlSharedCache` with the same db_id should succeed
  // without database access conflicts.
  auto new_cache = std::make_unique<SqlSharedCache>(
      "test_nik", *store_, temp_dir_.GetPath(), base::DoNothing(),
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN}));

  bool reinit_success = false;
  new_cache->InitIsolatedDatabase(
      db_id, base::BindLambdaForTesting(
                 [&](bool success) { reinit_success = success; }));
  async_task_manager_.RunUntilAllTasksCompleteForTest();
  EXPECT_TRUE(reinit_success);
}

}  // namespace disk_cache
