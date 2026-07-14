// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/sql_shared_cache_manager.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "net/base/features.h"
#include "net/base/network_isolation_key.h"
#include "net/base/schemeful_site.h"
#include "net/disk_cache/sql/sql_persistent_store.h"
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
        async_task_manager_, /*cleanup_tracker=*/nullptr);
    manager_ =
        std::make_unique<SqlSharedCacheManager>(*store_, temp_dir_.GetPath());
  }

  void TearDown() override {
    manager_.reset();
    store_.reset();
    FlushPendingTask();
  }

  void InitManager() {
    bool init_done = false;
    manager_->Init(base::BindLambdaForTesting(
        [&](base::expected<void, SqlSharedCacheIndexDatabase::Error> result) {
          EXPECT_TRUE(result.has_value());
          init_done = true;
        }));
    FlushPendingTask();
    EXPECT_TRUE(init_done);
  }

 protected:
  void FlushPendingTask() {
    async_task_manager_.RunUntilAllTasksCompleteForTest();
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::vector<scoped_refptr<base::SequencedTaskRunner>> task_runners_;
  SqlAsyncTaskManager async_task_manager_;
  std::unique_ptr<SqlPersistentStore> store_;
  std::unique_ptr<SqlSharedCacheManager> manager_;
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         SqlSharedCacheManagerTest,
                         testing::Bool(),
                         &SqlSharedCacheManagerTest::DescribeParams);

TEST_P(SqlSharedCacheManagerTest, GetCacheByNikWithoutDbId) {
  InitManager();

  net::NetworkIsolationKey nik(net::SchemefulSite(GURL("https://foo.test")),
                               net::SchemefulSite(GURL("https://bar.test")));

  scoped_refptr<SqlSharedCacheHandle> handle;
  bool callback_run = false;

  manager_->GetCacheByNik(
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

  manager_->GetCacheByNik(
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
  InitManager();

  net::NetworkIsolationKey nik(net::SchemefulSite(GURL("https://foo.test")),
                               net::SchemefulSite(GURL("https://bar.test")));

  scoped_refptr<SqlSharedCacheHandle> handle;
  bool callback_run = false;

  manager_->GetCacheByNik(
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

  manager_->GetCacheByDbId(
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
  InitManager();

  net::NetworkIsolationKey nik(net::SchemefulSite(GURL("https://foo.test")),
                               net::SchemefulSite(GURL("https://bar.test")));

  // First create without DbId requirement
  scoped_refptr<SqlSharedCacheHandle> handle;
  manager_->GetCacheByNik(
      nik, /*require_shared_cache_db_id=*/false,
      base::BindLambdaForTesting([&](scoped_refptr<SqlSharedCacheHandle> h) {
        handle = std::move(h);
      }));
  FlushPendingTask();
  ASSERT_TRUE(handle);
  EXPECT_FALSE((*handle)->shared_cache_db_id().has_value());

  // Request again with require_shared_cache_db_id = true
  scoped_refptr<SqlSharedCacheHandle> handle_with_id;
  manager_->GetCacheByNik(
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
  InitManager();

  scoped_refptr<SqlSharedCacheHandle> handle;
  bool callback_run = false;

  manager_->GetCacheByDbId(
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
  InitManager();

  net::NetworkIsolationKey nik(net::SchemefulSite(GURL("https://foo.test")),
                               net::SchemefulSite(GURL("https://bar.test")));

  {
    scoped_refptr<SqlSharedCacheHandle> handle;
    manager_->GetCacheByNik(
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
  manager_->GetCacheByNik(
      nik, /*require_shared_cache_db_id=*/false,
      base::BindLambdaForTesting([&](scoped_refptr<SqlSharedCacheHandle> h) {
        new_handle = std::move(h);
      }));
  FlushPendingTask();
  ASSERT_TRUE(new_handle);
}

TEST_P(SqlSharedCacheManagerTest, DestructionTriggersCleanup) {
  InitManager();

  net::NetworkIsolationKey nik(net::SchemefulSite(GURL("https://foo.test")),
                               net::SchemefulSite(GURL("https://bar.test")));

  scoped_refptr<SqlSharedCacheHandle> handle;
  manager_->GetCacheByNik(
      nik, /*require_shared_cache_db_id=*/true,
      base::BindLambdaForTesting([&](scoped_refptr<SqlSharedCacheHandle> h) {
        handle = std::move(h);
      }));
  FlushPendingTask();
  ASSERT_TRUE(handle);

  handle.reset();
  FlushPendingTask();

  // Destroying `manager_` triggers
  // `index_database_.AsyncCall(&SqlSharedCacheIndexDatabase::Close)` in its
  // destructor.
  manager_.reset();

  // `RunUntilAllTasksCompleteForTest()` waits for the async close task to
  // complete.
  async_task_manager_.RunUntilAllTasksCompleteForTest();

  // Re-creating and initializing a new `SqlSharedCacheManager` should succeed.
  manager_ =
      std::make_unique<SqlSharedCacheManager>(*store_, temp_dir_.GetPath());
  InitManager();
}

}  // namespace disk_cache
