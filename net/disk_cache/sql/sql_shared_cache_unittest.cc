// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/sql_shared_cache.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/pickle.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "net/base/features.h"
#include "net/disk_cache/backend_cleanup_tracker.h"
#include "net/disk_cache/sql/sql_persistent_store.h"
#include "net/disk_cache/sql/sql_shared_cache_handle.h"
#include "net/disk_cache/sql/sql_shared_cache_isolated_database.h"
#include "net/disk_cache/sql/sql_shared_cache_manager.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace disk_cache {

namespace {

constexpr SqlSharedCacheDbId kTestDbId(42);

scoped_refptr<net::IOBufferWithSize> CreateHeadBuffer(
    const net::HttpResponseInfo& response_info,
    bool response_truncated = false) {
  auto pickle = response_info.MakePickle(/*skip_transient_headers=*/true,
                                         response_truncated);
  auto head_buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(pickle->size());
  head_buffer->span().copy_from(pickle->AsBytes());
  return head_buffer;
}

scoped_refptr<net::IOBufferWithSize> CreateDataBuffer(std::string_view data) {
  auto data_buffer = base::MakeRefCounted<net::IOBufferWithSize>(data.size());
  data_buffer->span().copy_from(base::as_bytes(base::span(data)));
  return data_buffer;
}

net::HttpResponseInfo CreateTestHttpResponseInfo() {
  net::HttpResponseInfo response_info;
  response_info.headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  response_info.response_time = base::Time::Now();
  return response_info;
}

}  // namespace

class SqlSharedCacheTest : public testing::TestWithParam<bool> {
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
    store_ = std::make_unique<SqlPersistentStore>(
        temp_dir_.GetPath(), 1024 * 1024, net::DISK_CACHE, task_runners_,
        async_task_manager_, cleanup_tracker_);
  }

  void TearDown() override {
    store_.reset();
    async_task_manager_.RunUntilAllTasksCompleteForTest();
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

  void PopulateStoreEntry(const CacheEntryKey& key,
                          const net::HttpResponseInfo& response_info,
                          std::string_view data,
                          bool response_truncated = false) {
    base::test::TestFuture<SqlPersistentStore::EntryInfoOrError> create_future;
    store_->OpenOrCreateEntry(key, create_future.GetCallback());
    async_task_manager_.RunUntilAllTasksCompleteForTest();
    auto create_result = create_future.Take();
    ASSERT_TRUE(create_result.has_value());
    auto res_id = create_result.value().res_id;

    auto head_buffer = CreateHeadBuffer(response_info, response_truncated);
    base::test::TestFuture<SqlPersistentStore::ResIdOrError> header_future;
    store_->WriteEntryDataAndMetadata(
        key, res_id, /*old_body_end=*/std::nullopt,
        EntryWriteBuffer(/*buffer=*/nullptr, /*size=*/0, /*offset=*/0),
        base::Time::Now(), /*new_hints=*/std::nullopt, head_buffer,
        head_buffer->size(), /*doomed_new_entry=*/false,
        header_future.GetCallback());
    async_task_manager_.RunUntilAllTasksCompleteForTest();
    ASSERT_TRUE(header_future.Get().has_value());

    if (!data.empty()) {
      auto data_buffer = CreateDataBuffer(data);
      base::test::TestFuture<SqlPersistentStore::ResIdOrError> write_future;
      store_->WriteEntryData(
          key, res_id, /*old_body_end=*/0,
          EntryWriteBuffer(data_buffer, data.size(), /*offset=*/0),
          /*truncate=*/false, /*doomed_new_entry=*/false,
          /*sparse_write=*/false, /*header_size=*/0,
          write_future.GetCallback());
      async_task_manager_.RunUntilAllTasksCompleteForTest();
      ASSERT_TRUE(write_future.Get().has_value());
    }
  }

  scoped_refptr<SqlSharedCacheHandle> CreateAndInitStoreAndCache() {
    base::test::TestFuture<SqlPersistentStore::Error> store_init_future;
    store_->Initialize(store_init_future.GetCallback());
    async_task_manager_.RunUntilAllTasksCompleteForTest();
    EXPECT_EQ(store_init_future.Get(), SqlPersistentStore::Error::kOk);

    auto* manager = store_->shared_cache_manager_for_testing();
    EXPECT_TRUE(manager);

    net::NetworkIsolationKey nik(net::SchemefulSite(GURL("https://foo.test")),
                                 net::SchemefulSite(GURL("https://bar.test")));
    base::test::TestFuture<scoped_refptr<SqlSharedCacheHandle>> handle_future;
    manager->GetCacheByNik(nik, /*require_shared_cache_db_id=*/true,
                           handle_future.GetCallback());
    async_task_manager_.RunUntilAllTasksCompleteForTest();
    scoped_refptr<SqlSharedCacheHandle> handle = handle_future.Take();
    EXPECT_TRUE(handle);
    return handle;
  }

  SqlPersistentStore::SharedCacheEligibleEntry CreateEligibleEntry(
      const CacheEntryKey& key,
      const GURL& url,
      const net::HttpResponseInfo& response_info) {
    SqlPersistentStore::SharedCacheEligibleEntry entry;
    entry.key = key;
    entry.url = url;
    entry.response_info =
        std::make_unique<net::HttpResponseInfo>(response_info);
    return entry;
  }

  void VerifyIsolatedDatabaseEntryData(SqlSharedCache& cache,
                                       const CacheEntryKey& key,
                                       SqlSharedCacheRowId row_id,
                                       std::string_view expected_data) {
    auto read_buffer =
        base::MakeRefCounted<net::IOBufferWithSize>(expected_data.size());
    base::test::TestFuture<SqlSharedCacheIsolatedDatabase::ReadResultOrError>
        read_future;
    cache.isolated_database_for_testing()
        .AsyncCall(&SqlSharedCacheIsolatedDatabase::Read)
        .WithArgs(key, row_id, static_cast<int>(expected_data.size()),
                  /*offset=*/0, read_buffer)
        .Then(read_future.GetCallback());
    async_task_manager_.RunUntilAllTasksCompleteForTest();
    auto read_result = read_future.Take();
    ASSERT_TRUE(read_result.has_value());
    EXPECT_EQ(read_result.value().read_bytes,
              static_cast<int>(expected_data.size()));
    if (!expected_data.empty()) {
      EXPECT_EQ(std::string_view(read_buffer->data(), expected_data.size()),
                expected_data);
    }
  }

  void VerifyIsolatedDatabaseEntryNotFound(SqlSharedCache& cache,
                                           const CacheEntryKey& key,
                                           SqlSharedCacheRowId row_id) {
    auto read_buffer = base::MakeRefCounted<net::IOBufferWithSize>(100);
    base::test::TestFuture<SqlSharedCacheIsolatedDatabase::ReadResultOrError>
        read_future;
    cache.isolated_database_for_testing()
        .AsyncCall(&SqlSharedCacheIsolatedDatabase::Read)
        .WithArgs(key, row_id, /*body_size=*/100, /*offset=*/0, read_buffer)
        .Then(read_future.GetCallback());
    async_task_manager_.RunUntilAllTasksCompleteForTest();
    auto read_result = read_future.Take();
    EXPECT_FALSE(read_result.has_value());
    EXPECT_EQ(read_result.error(),
              SqlSharedCacheIsolatedDatabase::Error::kEntryNotFound);
  }

  SqlPersistentStore::OptionalEntryInfoOrError OpenStoreEntry(
      const CacheEntryKey& key) {
    base::test::TestFuture<SqlPersistentStore::OptionalEntryInfoOrError> future;
    store_->OpenEntry(key, future.GetCallback());
    async_task_manager_.RunUntilAllTasksCompleteForTest();
    return future.Take();
  }

  void VerifyStoreEntrySharedCacheResourceId(
      const CacheEntryKey& key,
      SqlSharedCacheDbId expected_db_id,
      SqlSharedCacheRowId expected_row_id) {
    auto open_result = OpenStoreEntry(key);
    ASSERT_TRUE(open_result.has_value());
    ASSERT_TRUE(open_result.value().has_value());
    ASSERT_TRUE(open_result.value()->shared_cache_resource_id.has_value());
    EXPECT_EQ(open_result.value()->shared_cache_resource_id->db_id,
              expected_db_id);
    EXPECT_EQ(open_result.value()->shared_cache_resource_id->row_id,
              expected_row_id);
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
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
      cleanup_tracker_);

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
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
      cleanup_tracker_);

  bool init_success = false;

  cache->InitIsolatedDatabase(
      kTestDbId, base::BindLambdaForTesting(
                     [&](bool success) { init_success = success; }));

  async_task_manager_.RunUntilAllTasksCompleteForTest();

  EXPECT_EQ(cache->shared_cache_db_id(), kTestDbId);
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
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
      cleanup_tracker_);

  bool cleanup_done = false;
  cache->Cleanup(base::BindLambdaForTesting([&]() { cleanup_done = true; }));

  EXPECT_TRUE(cleanup_done);
}

TEST_P(SqlSharedCacheTest, DestructionTriggersCleanup) {
  {
    auto cache = std::make_unique<SqlSharedCache>(
        "test_nik", *store_, temp_dir_.GetPath(), base::DoNothing(),
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
             base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
        cleanup_tracker_);

    bool init_success = false;
    cache->InitIsolatedDatabase(
        kTestDbId, base::BindLambdaForTesting(
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
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
      cleanup_tracker_);

  bool reinit_success = false;
  new_cache->InitIsolatedDatabase(
      kTestDbId, base::BindLambdaForTesting(
                     [&](bool success) { reinit_success = success; }));
  async_task_manager_.RunUntilAllTasksCompleteForTest();
  EXPECT_TRUE(reinit_success);
}

TEST_P(SqlSharedCacheTest, CopyEntries) {
  auto handle = CreateAndInitStoreAndCache();
  auto* cache = handle->get();

  const CacheEntryKey kKey("credential_key/post_key/https://www.example.com/");
  const std::string kData = "example data";
  auto response_info = CreateTestHttpResponseInfo();

  PopulateStoreEntry(kKey, response_info, kData);

  base::queue<SqlPersistentStore::SharedCacheEligibleEntry> entries;
  entries.push(CreateEligibleEntry(kKey, GURL("https://www.example.com/"),
                                   response_info));

  auto abort_flag =
      base::MakeRefCounted<base::RefCountedData<std::atomic_bool>>(
          std::in_place, false);
  base::test::TestFuture<
      base::queue<SqlPersistentStore::SharedCacheEligibleEntry>>
      copy_future;
  cache->CopyEntries(std::move(entries), abort_flag, copy_future.GetCallback());

  async_task_manager_.RunUntilAllTasksCompleteForTest();
  auto unprocessed = copy_future.Take();
  EXPECT_TRUE(unprocessed.empty());

  VerifyIsolatedDatabaseEntryData(*cache, kKey, SqlSharedCacheRowId(1), kData);
  VerifyStoreEntrySharedCacheResourceId(kKey, *cache->shared_cache_db_id(),
                                        SqlSharedCacheRowId(1));
}

TEST_P(SqlSharedCacheTest, CopyEntriesMultiple) {
  auto handle = CreateAndInitStoreAndCache();
  auto* cache = handle->get();

  const CacheEntryKey kKey1(
      "credential_key/post_key/https://example.com/1.png");
  const CacheEntryKey kKey2(
      "credential_key/post_key/https://example.com/2.html");
  const CacheEntryKey kKey3(
      "credential_key/post_key/https://example.com/3.bin");

  const std::string kData1 = "image_bytes_1234";
  const std::string kData2 = "";              // 0-byte body
  const std::string kData3(600 * 1024, 'a');  // Multi-chunk body > 512KB

  auto response_info1 = CreateTestHttpResponseInfo();
  auto response_info2 = CreateTestHttpResponseInfo();
  auto response_info3 = CreateTestHttpResponseInfo();

  PopulateStoreEntry(kKey1, response_info1, kData1);
  PopulateStoreEntry(kKey2, response_info2, kData2);
  PopulateStoreEntry(kKey3, response_info3, kData3);

  base::queue<SqlPersistentStore::SharedCacheEligibleEntry> entries;
  entries.push(CreateEligibleEntry(kKey1, GURL("https://example.com/1.png"),
                                   response_info1));
  entries.push(CreateEligibleEntry(kKey2, GURL("https://example.com/2.html"),
                                   response_info2));
  entries.push(CreateEligibleEntry(kKey3, GURL("https://example.com/3.bin"),
                                   response_info3));

  auto abort_flag =
      base::MakeRefCounted<base::RefCountedData<std::atomic_bool>>(
          std::in_place, false);
  base::test::TestFuture<
      base::queue<SqlPersistentStore::SharedCacheEligibleEntry>>
      copy_future;
  cache->CopyEntries(std::move(entries), abort_flag, copy_future.GetCallback());

  async_task_manager_.RunUntilAllTasksCompleteForTest();
  auto unprocessed = copy_future.Take();
  EXPECT_TRUE(unprocessed.empty());

  VerifyIsolatedDatabaseEntryData(*cache, kKey1, SqlSharedCacheRowId(1),
                                  kData1);
  VerifyIsolatedDatabaseEntryData(*cache, kKey2, SqlSharedCacheRowId(2),
                                  kData2);
  VerifyIsolatedDatabaseEntryData(*cache, kKey3, SqlSharedCacheRowId(3),
                                  kData3);
  VerifyStoreEntrySharedCacheResourceId(kKey1, *cache->shared_cache_db_id(),
                                        SqlSharedCacheRowId(1));
  VerifyStoreEntrySharedCacheResourceId(kKey2, *cache->shared_cache_db_id(),
                                        SqlSharedCacheRowId(2));
  VerifyStoreEntrySharedCacheResourceId(kKey3, *cache->shared_cache_db_id(),
                                        SqlSharedCacheRowId(3));
}

TEST_P(SqlSharedCacheTest,
       CopyEntriesMoveBlobsToSharedCacheFailureCleansUpPartialEntry) {
  auto handle = CreateAndInitStoreAndCache();
  auto* cache = handle->get();

  const CacheEntryKey kKey(
      "credential_key/post_key/https://example.com/fail_move");
  std::string kData = "payload for move blobs failure test";
  auto response_info = CreateTestHttpResponseInfo();

  PopulateStoreEntry(kKey, response_info, kData);

  base::queue<SqlPersistentStore::SharedCacheEligibleEntry> entries;
  entries.push(CreateEligibleEntry(kKey, GURL("https://example.com/fail_move"),
                                   response_info));

  auto abort_flag =
      base::MakeRefCounted<base::RefCountedData<std::atomic_bool>>(
          std::in_place, false);
  base::test::TestFuture<
      base::queue<SqlPersistentStore::SharedCacheEligibleEntry>>
      copy_future;

  cache->CopyEntries(std::move(entries), abort_flag, copy_future.GetCallback());
  // OpenEntry task is already enqueued on store runner. Enabling simulate
  // failure now will cause MoveBlobsToSharedCache (which is enqueued later) to
  // fail.
  store_->SetSimulateDbFailureForTesting(true);

  async_task_manager_.RunUntilAllTasksCompleteForTest();
  auto unprocessed = copy_future.Take();
  EXPECT_TRUE(unprocessed.empty());

  // Verify that row 1 was cleaned up (deleted) from isolated database upon
  // failure.
  base::test::TestFuture<bool> has_row_future;
  cache->isolated_database_for_testing()
      .AsyncCall(&SqlSharedCacheIsolatedDatabase::HasRowForTesting)
      .WithArgs(SqlSharedCacheRowId(1))
      .Then(has_row_future.GetCallback());
  async_task_manager_.RunUntilAllTasksCompleteForTest();
  EXPECT_FALSE(has_row_future.Get());
}

TEST_P(SqlSharedCacheTest, CopyEntriesExceedingMaxCopySizeSkipped) {
  base::test::ScopedFeatureList custom_feature_list;
  custom_feature_list.InitAndEnableFeatureWithParameters(
      net::features::kDiskCacheBackendExperiment,
      {{net::features::kSqlDiskCacheMaxSharedCacheCopyEntrySize.name, "100"}});

  auto handle = CreateAndInitStoreAndCache();
  auto* cache = handle->get();

  const CacheEntryKey kKeySmall(
      "credential_key/post_key/https://example.com/small");
  const CacheEntryKey kKeyLarge(
      "credential_key/post_key/https://example.com/large");

  const std::string kSmallData = "50 bytes payload data here...";
  const std::string kLargeData(200,
                               'x');  // 200 bytes exceeds 100-byte threshold

  auto response_info_small = CreateTestHttpResponseInfo();
  auto response_info_large = CreateTestHttpResponseInfo();

  PopulateStoreEntry(kKeySmall, response_info_small, kSmallData);
  PopulateStoreEntry(kKeyLarge, response_info_large, kLargeData);

  base::queue<SqlPersistentStore::SharedCacheEligibleEntry> entries;
  entries.push(CreateEligibleEntry(kKeySmall, GURL("https://example.com/small"),
                                   response_info_small));
  entries.push(CreateEligibleEntry(kKeyLarge, GURL("https://example.com/large"),
                                   response_info_large));

  auto abort_flag =
      base::MakeRefCounted<base::RefCountedData<std::atomic_bool>>(
          std::in_place, false);
  base::test::TestFuture<
      base::queue<SqlPersistentStore::SharedCacheEligibleEntry>>
      copy_future;
  cache->CopyEntries(std::move(entries), abort_flag, copy_future.GetCallback());

  async_task_manager_.RunUntilAllTasksCompleteForTest();
  auto unprocessed = copy_future.Take();
  EXPECT_TRUE(unprocessed.empty());

  VerifyIsolatedDatabaseEntryData(*cache, kKeySmall, SqlSharedCacheRowId(1),
                                  kSmallData);
  VerifyIsolatedDatabaseEntryNotFound(*cache, kKeyLarge,
                                      SqlSharedCacheRowId(2));
}

TEST_P(SqlSharedCacheTest, CopyEntriesOpenEntryFailed) {
  auto handle = CreateAndInitStoreAndCache();
  auto* cache = handle->get();

  const CacheEntryKey kNonExistentKey(
      "credential_key/post_key/https://example.com/non_existent");
  auto response_info = CreateTestHttpResponseInfo();

  base::queue<SqlPersistentStore::SharedCacheEligibleEntry> entries;
  entries.push(CreateEligibleEntry(kNonExistentKey,
                                   GURL("https://example.com/non_existent"),
                                   response_info));

  auto abort_flag =
      base::MakeRefCounted<base::RefCountedData<std::atomic_bool>>(
          std::in_place, false);
  base::test::TestFuture<
      base::queue<SqlPersistentStore::SharedCacheEligibleEntry>>
      copy_future;
  cache->CopyEntries(std::move(entries), abort_flag, copy_future.GetCallback());

  async_task_manager_.RunUntilAllTasksCompleteForTest();
  auto unprocessed = copy_future.Take();
  EXPECT_TRUE(unprocessed.empty());

  VerifyIsolatedDatabaseEntryNotFound(*cache, kNonExistentKey,
                                      SqlSharedCacheRowId(1));
}

TEST_P(SqlSharedCacheTest, CopyEntriesParseResponseInfoMismatch) {
  auto handle = CreateAndInitStoreAndCache();
  auto* cache = handle->get();

  const CacheEntryKey kKey(
      "credential_key/post_key/https://example.com/mismatch");
  auto response_info_store = CreateTestHttpResponseInfo();
  PopulateStoreEntry(kKey, response_info_store, "test data");

  // Create response info for entry with mismatched response_time
  auto response_info_mismatched = CreateTestHttpResponseInfo();
  response_info_mismatched.response_time =
      response_info_store.response_time + base::Seconds(10);

  base::queue<SqlPersistentStore::SharedCacheEligibleEntry> entries;
  entries.push(CreateEligibleEntry(kKey, GURL("https://example.com/mismatch"),
                                   response_info_mismatched));

  auto abort_flag =
      base::MakeRefCounted<base::RefCountedData<std::atomic_bool>>(
          std::in_place, false);
  base::test::TestFuture<
      base::queue<SqlPersistentStore::SharedCacheEligibleEntry>>
      copy_future;
  cache->CopyEntries(std::move(entries), abort_flag, copy_future.GetCallback());

  async_task_manager_.RunUntilAllTasksCompleteForTest();
  auto unprocessed = copy_future.Take();
  EXPECT_TRUE(unprocessed.empty());

  VerifyIsolatedDatabaseEntryNotFound(*cache, kKey, SqlSharedCacheRowId(1));
}

TEST_P(SqlSharedCacheTest, CopyEntriesResponseTruncatedSkipped) {
  auto handle = CreateAndInitStoreAndCache();
  auto* cache = handle->get();

  const CacheEntryKey kKey(
      "credential_key/post_key/https://example.com/truncated");
  auto response_info = CreateTestHttpResponseInfo();
  PopulateStoreEntry(kKey, response_info, "truncated payload",
                     /*response_truncated=*/true);

  base::queue<SqlPersistentStore::SharedCacheEligibleEntry> entries;
  entries.push(CreateEligibleEntry(kKey, GURL("https://example.com/truncated"),
                                   response_info));

  auto abort_flag =
      base::MakeRefCounted<base::RefCountedData<std::atomic_bool>>(
          std::in_place, false);
  base::test::TestFuture<
      base::queue<SqlPersistentStore::SharedCacheEligibleEntry>>
      copy_future;
  cache->CopyEntries(std::move(entries), abort_flag, copy_future.GetCallback());

  async_task_manager_.RunUntilAllTasksCompleteForTest();
  auto unprocessed = copy_future.Take();
  EXPECT_TRUE(unprocessed.empty());

  VerifyIsolatedDatabaseEntryNotFound(*cache, kKey, SqlSharedCacheRowId(1));
}

TEST_P(SqlSharedCacheTest, CopyEntriesReadSuccessAndFailure) {
  base::test::ScopedFeatureList custom_feature_list;
  custom_feature_list.InitAndEnableFeatureWithParameters(
      net::features::kDiskCacheBackendExperiment,
      {{net::features::kSqlDiskCacheMaxSharedCacheCopyEntrySize.name, "100"}});

  auto handle = CreateAndInitStoreAndCache();
  auto* cache = handle->get();

  const CacheEntryKey kSuccessKey(
      "credential_key/post_key/https://example.com/success");
  const CacheEntryKey kExceededKey(
      "credential_key/post_key/https://example.com/exceeded");

  const std::string kSuccessData = "small valid payload";
  const std::string kExceededData(200, 'z');

  auto response_info_success = CreateTestHttpResponseInfo();
  auto response_info_exceeded = CreateTestHttpResponseInfo();

  PopulateStoreEntry(kSuccessKey, response_info_success, kSuccessData);
  PopulateStoreEntry(kExceededKey, response_info_exceeded, kExceededData);

  base::queue<SqlPersistentStore::SharedCacheEligibleEntry> entries;
  entries.push(CreateEligibleEntry(
      kSuccessKey, GURL("https://example.com/success"), response_info_success));
  entries.push(CreateEligibleEntry(kExceededKey,
                                   GURL("https://example.com/exceeded"),
                                   response_info_exceeded));

  auto abort_flag =
      base::MakeRefCounted<base::RefCountedData<std::atomic_bool>>(
          std::in_place, false);
  base::test::TestFuture<
      base::queue<SqlPersistentStore::SharedCacheEligibleEntry>>
      copy_future;
  cache->CopyEntries(std::move(entries), abort_flag, copy_future.GetCallback());

  async_task_manager_.RunUntilAllTasksCompleteForTest();
  auto unprocessed = copy_future.Take();
  EXPECT_TRUE(unprocessed.empty());

  // 1. Verify succeeded entry can be read from isolated database.
  VerifyIsolatedDatabaseEntryData(*cache, kSuccessKey, SqlSharedCacheRowId(1),
                                  kSuccessData);

  // 2. Verify failed/skipped entry cannot be read from isolated database.
  VerifyIsolatedDatabaseEntryNotFound(*cache, kExceededKey,
                                      SqlSharedCacheRowId(999));
}

TEST_P(SqlSharedCacheTest, CopyEntriesExceedingReadBufferSize) {
  base::test::ScopedFeatureList custom_feature_list;
  custom_feature_list.InitAndEnableFeatureWithParameters(
      net::features::kDiskCacheBackendExperiment,
      {{net::features::kSqlDiskCacheSharedCacheReadBufferSize.name, "50"}});

  auto handle = CreateAndInitStoreAndCache();
  auto* cache = handle->get();

  const CacheEntryKey kKey(
      "credential_key/post_key/https://example.com/chunked");
  // 120 bytes body will be read in 3 chunks (50 + 50 + 20 bytes).
  // Use sequential, varying characters so any offset misalignment is detected.
  std::string kData;
  kData.reserve(120);
  for (int i = 0; i < 120; ++i) {
    kData.push_back(static_cast<char>('a' + (i % 26)));
  }
  auto response_info = CreateTestHttpResponseInfo();

  PopulateStoreEntry(kKey, response_info, kData);

  base::queue<SqlPersistentStore::SharedCacheEligibleEntry> entries;
  entries.push(CreateEligibleEntry(kKey, GURL("https://example.com/chunked"),
                                   response_info));

  auto abort_flag =
      base::MakeRefCounted<base::RefCountedData<std::atomic_bool>>(
          std::in_place, false);
  base::test::TestFuture<
      base::queue<SqlPersistentStore::SharedCacheEligibleEntry>>
      copy_future;
  cache->CopyEntries(std::move(entries), abort_flag, copy_future.GetCallback());

  async_task_manager_.RunUntilAllTasksCompleteForTest();
  auto unprocessed = copy_future.Take();
  EXPECT_TRUE(unprocessed.empty());

  VerifyIsolatedDatabaseEntryData(*cache, kKey, SqlSharedCacheRowId(1), kData);
}

TEST_P(SqlSharedCacheTest, CopyEntriesAborted) {
  auto handle = CreateAndInitStoreAndCache();
  auto* cache = handle->get();

  base::queue<SqlPersistentStore::SharedCacheEligibleEntry> entries;
  entries.push(CreateEligibleEntry(
      CacheEntryKey("credential_key/post_key/https://www.example.com/"),
      GURL("https://www.example.com/"), net::HttpResponseInfo()));

  auto abort_flag =
      base::MakeRefCounted<base::RefCountedData<std::atomic_bool>>(
          std::in_place, true);
  base::test::TestFuture<
      base::queue<SqlPersistentStore::SharedCacheEligibleEntry>>
      copy_future;
  cache->CopyEntries(std::move(entries), abort_flag, copy_future.GetCallback());

  async_task_manager_.RunUntilAllTasksCompleteForTest();
  auto unprocessed = copy_future.Take();
  EXPECT_EQ(unprocessed.size(), 1u);
}

TEST_P(SqlSharedCacheTest, CopyEntriesWriteBodyFailureCleansUpPartialEntry) {
  base::test::ScopedFeatureList custom_feature_list;
  custom_feature_list.InitAndEnableFeatureWithParameters(
      net::features::kDiskCacheBackendExperiment,
      {{net::features::kSqlDiskCacheSharedCacheReadBufferSize.name, "50"}});

  auto handle = CreateAndInitStoreAndCache();
  auto* cache = handle->get();

  const CacheEntryKey kKey(
      "credential_key/post_key/https://example.com/fail_write");
  std::string kData(120, 'a');
  auto response_info = CreateTestHttpResponseInfo();

  PopulateStoreEntry(kKey, response_info, kData);

  base::queue<SqlPersistentStore::SharedCacheEligibleEntry> entries;
  entries.push(CreateEligibleEntry(kKey, GURL("https://example.com/fail_write"),
                                   response_info));

  auto abort_flag =
      base::MakeRefCounted<base::RefCountedData<std::atomic_bool>>(
          std::in_place, false);
  base::test::TestFuture<
      base::queue<SqlPersistentStore::SharedCacheEligibleEntry>>
      copy_future;

  // Simulate DB failure specifically for WriteBody so Insert succeeds but body
  // write fails.
  cache->isolated_database_for_testing()
      .AsyncCall(&SqlSharedCacheIsolatedDatabase::
                     SetSimulateDbFailureCallbackForTesting)
      .WithArgs(base::BindRepeating(
          [](SqlSharedCacheIsolatedDatabase::OperationForTesting op) {
            return op == SqlSharedCacheIsolatedDatabase::OperationForTesting::
                             kWriteBody;
          }));

  cache->CopyEntries(std::move(entries), abort_flag, copy_future.GetCallback());

  async_task_manager_.RunUntilAllTasksCompleteForTest();
  auto unprocessed = copy_future.Take();
  EXPECT_TRUE(unprocessed.empty());

  // Verify that row 1 was cleaned up (deleted) from isolated database upon
  // failure.
  base::test::TestFuture<bool> has_row_future;
  cache->isolated_database_for_testing()
      .AsyncCall(&SqlSharedCacheIsolatedDatabase::HasRowForTesting)
      .WithArgs(SqlSharedCacheRowId(1))
      .Then(has_row_future.GetCallback());
  async_task_manager_.RunUntilAllTasksCompleteForTest();
  EXPECT_FALSE(has_row_future.Get());
}

TEST_P(SqlSharedCacheTest, DeleteEntries) {
  auto handle = CreateAndInitStoreAndCache();
  auto* cache = handle->get();

  const CacheEntryKey kKey1("credential_key/post_key/https://example.com/1");
  const CacheEntryKey kKey2("credential_key/post_key/https://example.com/2");
  std::string kData = "test_data";
  auto response_info = CreateTestHttpResponseInfo();

  PopulateStoreEntry(kKey1, response_info, kData);
  PopulateStoreEntry(kKey2, response_info, kData);

  base::queue<SqlPersistentStore::SharedCacheEligibleEntry> entries;
  entries.push(
      CreateEligibleEntry(kKey1, GURL("https://example.com/1"), response_info));
  entries.push(
      CreateEligibleEntry(kKey2, GURL("https://example.com/2"), response_info));

  auto abort_flag =
      base::MakeRefCounted<base::RefCountedData<std::atomic_bool>>(
          std::in_place, false);
  base::test::TestFuture<
      base::queue<SqlPersistentStore::SharedCacheEligibleEntry>>
      copy_future;
  cache->CopyEntries(std::move(entries), abort_flag, copy_future.GetCallback());
  async_task_manager_.RunUntilAllTasksCompleteForTest();
  EXPECT_TRUE(copy_future.Take().empty());

  // Verify row 1 and row 2 exist in isolated database.
  VerifyIsolatedDatabaseEntryData(*cache, kKey1, SqlSharedCacheRowId(1), kData);
  VerifyIsolatedDatabaseEntryData(*cache, kKey2, SqlSharedCacheRowId(2), kData);

  // Delete row 1.
  base::test::TestFuture<
      base::expected<void, SqlSharedCacheIsolatedDatabase::Error>>
      delete_future1;
  cache->DeleteEntries({SqlSharedCacheRowId(1)}, delete_future1.GetCallback());
  async_task_manager_.RunUntilAllTasksCompleteForTest();
  EXPECT_TRUE(delete_future1.Get().has_value());

  // Delete row 2.
  base::test::TestFuture<
      base::expected<void, SqlSharedCacheIsolatedDatabase::Error>>
      delete_future2;
  cache->DeleteEntries({SqlSharedCacheRowId(2)}, delete_future2.GetCallback());
  async_task_manager_.RunUntilAllTasksCompleteForTest();
  EXPECT_TRUE(delete_future2.Get().has_value());
}

TEST_P(SqlSharedCacheTest, DeleteMultipleEntriesAtOnce) {
  auto handle = CreateAndInitStoreAndCache();
  auto* cache = handle->get();

  const CacheEntryKey kKey1("credential_key/post_key/https://example.com/1");
  const CacheEntryKey kKey2("credential_key/post_key/https://example.com/2");
  const CacheEntryKey kKey3("credential_key/post_key/https://example.com/3");
  std::string kData = "test_data";
  auto response_info = CreateTestHttpResponseInfo();

  PopulateStoreEntry(kKey1, response_info, kData);
  PopulateStoreEntry(kKey2, response_info, kData);
  PopulateStoreEntry(kKey3, response_info, kData);

  base::queue<SqlPersistentStore::SharedCacheEligibleEntry> entries;
  entries.push(
      CreateEligibleEntry(kKey1, GURL("https://example.com/1"), response_info));
  entries.push(
      CreateEligibleEntry(kKey2, GURL("https://example.com/2"), response_info));
  entries.push(
      CreateEligibleEntry(kKey3, GURL("https://example.com/3"), response_info));

  auto abort_flag =
      base::MakeRefCounted<base::RefCountedData<std::atomic_bool>>(
          std::in_place, false);
  base::test::TestFuture<
      base::queue<SqlPersistentStore::SharedCacheEligibleEntry>>
      copy_future;
  cache->CopyEntries(std::move(entries), abort_flag, copy_future.GetCallback());
  async_task_manager_.RunUntilAllTasksCompleteForTest();
  EXPECT_TRUE(copy_future.Take().empty());

  // Delete row 1 and row 2 simultaneously.
  base::test::TestFuture<
      base::expected<void, SqlSharedCacheIsolatedDatabase::Error>>
      delete_future1;
  cache->DeleteEntries({SqlSharedCacheRowId(1), SqlSharedCacheRowId(2)},
                       delete_future1.GetCallback());
  async_task_manager_.RunUntilAllTasksCompleteForTest();
  EXPECT_TRUE(delete_future1.Get().has_value());

  // Delete remaining row 3.
  base::test::TestFuture<
      base::expected<void, SqlSharedCacheIsolatedDatabase::Error>>
      delete_future2;
  cache->DeleteEntries({SqlSharedCacheRowId(3)}, delete_future2.GetCallback());
  async_task_manager_.RunUntilAllTasksCompleteForTest();
  EXPECT_TRUE(delete_future2.Get().has_value());
}

TEST_P(SqlSharedCacheTest, DeleteNonExistentEntries) {
  auto handle = CreateAndInitStoreAndCache();
  auto* cache = handle->get();

  const CacheEntryKey kKey1("credential_key/post_key/https://example.com/1");
  std::string kData = "test_data";
  auto response_info = CreateTestHttpResponseInfo();

  PopulateStoreEntry(kKey1, response_info, kData);

  base::queue<SqlPersistentStore::SharedCacheEligibleEntry> entries;
  entries.push(
      CreateEligibleEntry(kKey1, GURL("https://example.com/1"), response_info));

  auto abort_flag =
      base::MakeRefCounted<base::RefCountedData<std::atomic_bool>>(
          std::in_place, false);
  base::test::TestFuture<
      base::queue<SqlPersistentStore::SharedCacheEligibleEntry>>
      copy_future;
  cache->CopyEntries(std::move(entries), abort_flag, copy_future.GetCallback());
  async_task_manager_.RunUntilAllTasksCompleteForTest();
  EXPECT_TRUE(copy_future.Take().empty());

  // Delete non-existent row 999.
  base::test::TestFuture<
      base::expected<void, SqlSharedCacheIsolatedDatabase::Error>>
      delete_future1;
  cache->DeleteEntries({SqlSharedCacheRowId(999)},
                       delete_future1.GetCallback());
  async_task_manager_.RunUntilAllTasksCompleteForTest();
  EXPECT_TRUE(delete_future1.Get().has_value());

  // Delete existing row 1 and non-existent row 999 together.
  base::test::TestFuture<
      base::expected<void, SqlSharedCacheIsolatedDatabase::Error>>
      delete_future2;
  cache->DeleteEntries({SqlSharedCacheRowId(1), SqlSharedCacheRowId(999)},
                       delete_future2.GetCallback());
  async_task_manager_.RunUntilAllTasksCompleteForTest();
  EXPECT_TRUE(delete_future2.Get().has_value());
}

TEST_P(SqlSharedCacheTest, DeleteEntriesWithoutIsolatedDatabase) {
  auto cache = std::make_unique<SqlSharedCache>(
      "test_nik", *store_, temp_dir_.GetPath(), base::DoNothing(),
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
      cleanup_tracker_);

  base::test::TestFuture<
      base::expected<void, SqlSharedCacheIsolatedDatabase::Error>>
      delete_future;
  cache->DeleteEntries({SqlSharedCacheRowId(1)}, delete_future.GetCallback());
  async_task_manager_.RunUntilAllTasksCompleteForTest();
  auto result = delete_future.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      result.error(),
      SqlSharedCacheIsolatedDatabase::Error::kIsolatedDatabaseNotAvailable);
}

TEST_P(SqlSharedCacheTest, GetBlobHandleSuccess) {
  auto handle = CreateAndInitStoreAndCache();
  auto* cache = handle->get();

  const CacheEntryKey kKey(
      "credential_key/post_key/https://example.com/blob_test");
  auto response_info = CreateTestHttpResponseInfo();
  std::string body_data = "Blob data test";
  PopulateStoreEntry(kKey, response_info, body_data);

  base::queue<SqlPersistentStore::SharedCacheEligibleEntry> entries;
  entries.push(CreateEligibleEntry(kKey, GURL("https://example.com/blob_test"),
                                   response_info));

  auto abort_flag =
      base::MakeRefCounted<base::RefCountedData<std::atomic_bool>>(
          std::in_place, false);
  base::test::TestFuture<
      base::queue<SqlPersistentStore::SharedCacheEligibleEntry>>
      copy_future;
  cache->CopyEntries(std::move(entries), abort_flag, copy_future.GetCallback());

  async_task_manager_.RunUntilAllTasksCompleteForTest();
  auto unprocessed = copy_future.Take();
  EXPECT_TRUE(unprocessed.empty());

  base::test::TestFuture<base::expected<scoped_refptr<SqlSharedCacheBlobHandle>,
                                        SqlSharedCacheIsolatedDatabase::Error>>
      blob_future;
  cache->GetBlobHandle(kKey, SqlSharedCacheRowId(1), body_data.size(),
                       blob_future.GetCallback());
  async_task_manager_.RunUntilAllTasksCompleteForTest();
  auto result = blob_future.Take();
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result.value());
}

TEST_P(SqlSharedCacheTest, GetBlobHandleWithoutIsolatedDatabase) {
  auto cache = std::make_unique<SqlSharedCache>(
      "test_nik", *store_, temp_dir_.GetPath(), base::DoNothing(),
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
      cleanup_tracker_);

  const CacheEntryKey kKey("https://example.com/blob_test");
  base::test::TestFuture<base::expected<scoped_refptr<SqlSharedCacheBlobHandle>,
                                        SqlSharedCacheIsolatedDatabase::Error>>
      blob_future;
  cache->GetBlobHandle(kKey, SqlSharedCacheRowId(1), 10,
                       blob_future.GetCallback());
  auto result = blob_future.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      result.error(),
      SqlSharedCacheIsolatedDatabase::Error::kIsolatedDatabaseNotAvailable);
}

}  // namespace disk_cache
