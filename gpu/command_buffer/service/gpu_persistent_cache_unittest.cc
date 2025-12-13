// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gpu_persistent_cache.h"

#include "base/barrier_closure.h"
#include "base/containers/heap_array.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/trace_test_utils.h"
#include "base/trace_event/trace_config.h"
#include "base/trace_event/trace_log.h"
#include "components/persistent_cache/backend_storage.h"
#include "components/persistent_cache/backend_type.h"
#include "components/persistent_cache/pending_backend.h"
#include "components/persistent_cache/sqlite/vfs/sandboxed_file.h"
#include "gpu/command_buffer/service/memory_cache.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

namespace {
static constexpr size_t kDefaultMemoryCacheSizeForTesting = 1 << 16;
}

class GpuPersistentCacheTest : public testing::Test {
 public:
  void SetUp() override {
    cache_ = base::MakeRefCounted<GpuPersistentCache>("Test",
                                                      MakeDefaultMemoryCache());
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    backend_storage_.emplace(persistent_cache::BackendType::kSqlite,
                             temp_dir_.GetPath());
  }

 protected:
  static scoped_refptr<MemoryCache> MakeDefaultMemoryCache() {
    return base::MakeRefCounted<MemoryCache>(kDefaultMemoryCacheSizeForTesting);
  }

  void InitializeCache() {
    ASSERT_OK_AND_ASSIGN(
        auto pending_backend,
        backend_storage_->MakePendingBackend(
            base::FilePath(FILE_PATH_LITERAL("test")),
            /*single_connection=*/true, /*journal_mode_wal=*/true));
    cache_->InitializeCache(std::move(pending_backend));
  }

  void RunStoreAndLoadDataMultiThreaded(int num_threads);

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir temp_dir_;
  std::optional<persistent_cache::BackendStorage> backend_storage_;
  scoped_refptr<GpuPersistentCache> cache_;
};

TEST_F(GpuPersistentCacheTest,
       StoreAndLoadDataBeforeInitializeWithNoMemoryCache) {
  // Don't initialize cache.
  auto cache_with_no_memory_cache =
      base::MakeRefCounted<GpuPersistentCache>("Test", nullptr);
  const std::string key = "my_key";
  const std::string value = "my_value";

  // StoreData() won't do anything but also won't crash.
  cache_with_no_memory_cache->StoreData(key.c_str(), key.size(), value.c_str(),
                                        value.size());

  // LoadData() will return zero size since there is no cache yet.
  EXPECT_EQ(
      cache_with_no_memory_cache->LoadData(key.c_str(), key.size(), nullptr, 0),
      0u);
}

// Tests basic store and load functionality on a single thread.
TEST_F(GpuPersistentCacheTest, StoreAndLoadData) {
  InitializeCache();

  const std::string key = "my_key";
  const std::string value = "my_value";
  cache_->StoreData(key.c_str(), key.size(), value.c_str(), value.size());

  std::vector<char> buffer(value.size());
  size_t loaded_size =
      cache_->LoadData(key.c_str(), key.size(), buffer.data(), buffer.size());

  EXPECT_EQ(loaded_size, value.size());
  EXPECT_EQ(std::string(buffer.begin(), buffer.end()), value);
}

// Tests basic load and store using the Skia, ANGLE and Dawn caching interfaces.
TEST_F(GpuPersistentCacheTest, StoreAndLoadDataMixedInterfaces) {
  InitializeCache();

  // Insert 3 key/value pairs with the 3 caching interfaces.
  const std::string key_dawn = "my_key_dawn";
  const std::string value_dawn = "my_value_dawn";
  cache_->StoreData(key_dawn.c_str(), key_dawn.size(), value_dawn.c_str(),
                    value_dawn.size());

  const std::string key_gr = "my_key_gr";
  sk_sp<SkData> key_gr_data =
      SkData::MakeWithoutCopy(key_gr.c_str(), key_gr.size());
  const std::string value_gr = "my_value_gr";
  sk_sp<SkData> value_gr_data =
      SkData::MakeWithoutCopy(value_gr.c_str(), value_gr.size());
  cache_->store(*key_gr_data, *value_gr_data);

  const std::string key_gl = "my_key_gl";
  const std::string value_gl = "my_value_gl";
  cache_->GLBlobCacheSet(key_gl.c_str(), static_cast<int64_t>(key_gl.size()),
                         value_gl.c_str(),
                         static_cast<int64_t>(value_gl.size()));

  // Load with dawn::Platform::CachingInterface
  auto test_load_dawn = [this](const std::string& key,
                               const std::string& value) {
    std::vector<char> buffer(value.size());
    size_t loaded_size =
        cache_->LoadData(key.c_str(), key.size(), buffer.data(), buffer.size());

    EXPECT_EQ(loaded_size, value.size());
    EXPECT_EQ(std::string(buffer.begin(), buffer.end()), value);
  };
  test_load_dawn(key_dawn, value_dawn);
  test_load_dawn(key_gr, value_gr);
  test_load_dawn(key_gl, value_gl);

  // Load with GrContextOptions::PersistentCache
  auto test_load_gr = [this](const std::string& key, const std::string& value) {
    sk_sp<SkData> key_data = SkData::MakeWithoutCopy(key.c_str(), key.size());
    sk_sp buffer = cache_->load(*key_data);

    EXPECT_EQ(buffer->size(), value.size());
    EXPECT_EQ(
        std::string(static_cast<const char*>(buffer->data()), buffer->size()),
        value);
  };
  test_load_gr(key_dawn, value_dawn);
  test_load_gr(key_gr, value_gr);
  test_load_gr(key_gl, value_gl);

  // Load with GL_ANGLE_blob_cache
  auto test_load_gl = [this](const std::string& key, const std::string& value) {
    std::vector<char> buffer(value.size());
    int64_t loaded_size = cache_->GLBlobCacheGet(key.c_str(), key.size(),
                                                 buffer.data(), buffer.size());

    EXPECT_EQ(loaded_size, static_cast<int64_t>(value.size()));
    EXPECT_EQ(std::string(buffer.begin(), buffer.end()), value);
  };
  test_load_gl(key_dawn, value_dawn);
  test_load_gl(key_gr, value_gr);
  test_load_gl(key_gl, value_gl);
}

// Tests that loading a non-existent key returns 0.
TEST_F(GpuPersistentCacheTest, LoadNonExistentKey) {
  InitializeCache();

  const std::string key = "non_existent_key";
  std::vector<char> buffer(16);
  size_t loaded_size =
      cache_->LoadData(key.c_str(), key.size(), buffer.data(), buffer.size());
  EXPECT_EQ(loaded_size, 0u);
}

void GpuPersistentCacheTest::RunStoreAndLoadDataMultiThreaded(int num_threads) {
  constexpr int kNumOperationsPerThread = 2;

  base::RunLoop run_loop;
  auto barrier = base::BarrierClosure(num_threads, run_loop.QuitClosure());

  // Post tasks to multiple threads to store and immediately load data.
  for (int i = 0; i < num_threads; ++i) {
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(
            [](scoped_refptr<GpuPersistentCache> cache, int thread_id,
               base::OnceClosure done_closure) {
              for (int j = 0; j < kNumOperationsPerThread; ++j) {
                std::string key = "key_" + base::NumberToString(thread_id) +
                                  "_" + base::NumberToString(j);
                std::string value = "value_" + base::NumberToString(thread_id) +
                                    "_" + base::NumberToString(j);

                cache->StoreData(key.c_str(), key.size(), value.c_str(),
                                 value.size());

                std::vector<char> buffer(value.size());
                size_t loaded_size = cache->LoadData(
                    key.c_str(), key.size(), buffer.data(), buffer.size());
                ASSERT_EQ(loaded_size, value.size());
                ASSERT_EQ(std::string(buffer.begin(), buffer.end()), value);
              }
              std::move(done_closure).Run();
            },
            cache_, i, barrier));
  }

  // Wait for all threads to complete.
  run_loop.Run();

  // After all threads are done, verify from the main thread that all data is
  // still present and correct. This ensures that writes from different threads
  // did not corrupt each other's data.
  for (int i = 0; i < num_threads; ++i) {
    for (int j = 0; j < kNumOperationsPerThread; ++j) {
      std::string key =
          "key_" + base::NumberToString(i) + "_" + base::NumberToString(j);
      std::string value =
          "value_" + base::NumberToString(i) + "_" + base::NumberToString(j);
      std::vector<char> buffer(value.size());
      size_t loaded_size = cache_->LoadData(key.c_str(), key.size(),
                                            buffer.data(), buffer.size());
      EXPECT_EQ(loaded_size, value.size());
      EXPECT_EQ(std::string(buffer.begin(), buffer.end()), value);
    }
  }
}

// Tests that the cache can be safely written to and read from by multiple
// threads concurrently.
TEST_F(GpuPersistentCacheTest, StoreAndLoadDataMultiThreaded) {
  InitializeCache();

  RunStoreAndLoadDataMultiThreaded(8);
}

// Some internal sql code especially tracings checks that they are called on a
// correct sequence. This test verifies that we can use the cache on multiple
// threads without violating sequence checkers. There is no need to stress test
// with many threads like the above StoreAndLoadDataMultiThreaded. A minimal
// number of threads should suffice.
TEST_F(GpuPersistentCacheTest, StoreAndLoadDataMultiThreadedWithSqlTrace) {
  InitializeCache();

  base::test::TracingEnvironment tracing_environment;
  base::trace_event::TraceLog::GetInstance()->SetEnabled(
      base::trace_event::TraceConfig("sql", ""));

  RunStoreAndLoadDataMultiThreaded(3);

  base::trace_event::TraceLog::GetInstance()->SetDisabled();
}

class GpuPersistentCacheAsyncTest : public GpuPersistentCacheTest {
 protected:
  scoped_refptr<GpuPersistentCache> OpenAsyncCache(
      size_t max_pending_bytes_to_write = std::numeric_limits<size_t>::max()) {
    auto pending_backend = backend_storage_->MakePendingBackend(
        base::FilePath(FILE_PATH_LITERAL("test")),
        /*single_connection=*/true, /*journal_mode_wal=*/true);
    if (!pending_backend) {
      ADD_FAILURE() << "Failed to make pending backend for test cache";
      return nullptr;
    }

    GpuPersistentCache::AsyncDiskWriteOpts options;
    options.task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
    options.max_pending_bytes_to_write = max_pending_bytes_to_write;
    auto async_cache = base::MakeRefCounted<GpuPersistentCache>(
        "TestAsync", MakeDefaultMemoryCache(), std::move(options));
    async_cache->InitializeCache(*std::move(pending_backend));
    return async_cache;
  }
};

// Tests that when an async task runner is used, the data is written to disk
// after a delay. It also verifies that the underlying DiskCache is kept alive
// by the posted task, even after the GpuPersistentCache instance is destroyed.
TEST_F(GpuPersistentCacheAsyncTest, StoreAndLoadDataAsync) {
  const std::string key = "my_key";
  const std::string value = "my_value";

  scoped_refptr<GpuPersistentCache> async_cache = OpenAsyncCache();

  base::HistogramTester histogram_tester;

  // Store data. This will be a delayed write.
  async_cache->StoreData(key.c_str(), key.size(), value.c_str(), value.size());

  // No writes should have taken place yet.
  histogram_tester.ExpectTotalCount("GPU.PersistentCache.TestAsync.Store", 0);

  // Destroy the cache. The posted write task will keep the underlying
  // DiskCache alive until it has run.
  async_cache.reset();

  // Fast forward time to trigger the write.
  task_environment_.FastForwardBy(base::Seconds(2));

  // Now the store should have taken place.
  histogram_tester.ExpectTotalCount("GPU.PersistentCache.TestAsync.Store", 1);

  // And the data should be in the cache.
  async_cache = OpenAsyncCache();
  auto buffer = base::HeapArray<char>::Uninit(value.size());
  size_t loaded_size = async_cache->LoadData(key.c_str(), key.size(),
                                             buffer.data(), buffer.size());
  EXPECT_EQ(loaded_size, value.size());
  EXPECT_EQ(std::string(buffer.begin(), buffer.end()), value);
}

// Tests the idle-rescheduling logic. If another cache operation occurs during
// the delay period, the write task should be postponed.
TEST_F(GpuPersistentCacheAsyncTest, StoreAndLoadDataAsync_IdleReschedule) {
  const std::string key = "my_key";
  const std::string value = "my_value";

  scoped_refptr<GpuPersistentCache> async_cache = OpenAsyncCache();

  base::HistogramTester histogram_tester;

  // Store data. This will be a delayed write.
  async_cache->StoreData(key.c_str(), key.size(), value.c_str(), value.size());

  // Fast forward a bit, but less than the delay.
  task_environment_.FastForwardBy(base::Milliseconds(500));

  // Perform another operation to reset the idle timer.
  std::vector<char> dummy_buffer(1);
  async_cache->LoadData("some_other_key", 14, dummy_buffer.data(), 1);

  // Fast forward past the original delay time.
  task_environment_.FastForwardBy(base::Seconds(1));

  // The write should not have happened yet because it was rescheduled.
  histogram_tester.ExpectTotalCount("GPU.PersistentCache.TestAsync.Store", 0);

  // Fast forward again to let the rescheduled write complete.
  task_environment_.FastForwardBy(base::Seconds(1));

  // The write should now have happened.
  histogram_tester.ExpectTotalCount("GPU.PersistentCache.TestAsync.Store", 1);

  // Destroy the cache.
  async_cache.reset();

  // And the data should be there.
  async_cache = OpenAsyncCache();
  auto buffer = base::HeapArray<char>::Uninit(value.size());
  size_t loaded_size = async_cache->LoadData(key.c_str(), key.size(),
                                             buffer.data(), buffer.size());
  EXPECT_EQ(loaded_size, value.size());
  EXPECT_EQ(std::string(buffer.begin(), buffer.end()), value);
}

// Tests that if pending bytes exceed the limit, the write happens after the
// first delay without being rescheduled.
TEST_F(GpuPersistentCacheAsyncTest,
       StoreAndLoadDataAsync_ExceedMaxPendingBytes) {
  // Create the cache with a pending byte limit.
  scoped_refptr<GpuPersistentCache> async_cache =
      OpenAsyncCache(/*max_pending_bytes_to_write=*/10);

  const std::string key = "my_key";
  const std::string value = "my_value_is_longer_than_10";
  ASSERT_GT(key.size() + value.size(), 10u);

  base::HistogramTester histogram_tester;

  // Store data. This will be a delayed write.
  async_cache->StoreData(key.c_str(), key.size(), value.c_str(), value.size());

  // Fast forward a bit, but less than the delay.
  task_environment_.FastForwardBy(base::Milliseconds(500));

  // Perform another operation to reset the idle timer. This is to ensure that
  // the write is triggered by the pending bytes limit and not the idle timeout.
  std::vector<char> dummy_buffer(1);
  async_cache->LoadData("some_other_key", 14, dummy_buffer.data(), 1);

  // The write should not have happened yet.
  histogram_tester.ExpectTotalCount("GPU.PersistentCache.TestAsync.Store", 0);

  // Fast forward past the delay. The write should have happened because of the
  // pending bytes limit, even though the cache was not idle.
  task_environment_.FastForwardBy(base::Seconds(1));

  // The write should have happened.
  histogram_tester.ExpectTotalCount("GPU.PersistentCache.TestAsync.Store", 1);

  // Destroy the cache.
  async_cache.reset();

  // Verify that the data was written by reopening and reading from the cache.
  async_cache = OpenAsyncCache();
  auto buffer = base::HeapArray<char>::Uninit(value.size());
  size_t loaded_size = async_cache->LoadData(key.c_str(), key.size(),
                                             buffer.data(), buffer.size());
  EXPECT_EQ(loaded_size, value.size());
  EXPECT_EQ(std::string(buffer.begin(), buffer.end()), value);
}

// Test that the persistent cache uses the memory backing if no database files
// are set
TEST_F(GpuPersistentCacheTest, MemoryBackingOnly) {
  const std::string key = "my_key";
  const std::string value = "my_value";
  cache_->StoreData(key.c_str(), key.size(), value.c_str(), value.size());

  // Check that the entry exists in the cache.
  std::vector<char> buffer(value.size());
  size_t loaded_size =
      cache_->LoadData(key.c_str(), key.size(), buffer.data(), buffer.size());

  EXPECT_EQ(loaded_size, value.size());
  EXPECT_EQ(std::string(buffer.begin(), buffer.end()), value);
}

// Test that the persistent cache uses the memory backing before the database
// files are initialized
TEST_F(GpuPersistentCacheTest, MemoryBackingSyncedToDisk) {
  const std::string key = "my_key";
  const std::string value = "my_value";

  {
    // Store the data to the cache without initializing the database files
    auto cache = base::MakeRefCounted<GpuPersistentCache>(
        "Test", MakeDefaultMemoryCache());
    cache->StoreData(key.c_str(), key.size(), value.c_str(), value.size());

    // Initialize the cache, the memory storage will be written to disk.
    ASSERT_OK_AND_ASSIGN(
        auto pending_backend,
        backend_storage_->MakePendingBackend(
            base::FilePath(FILE_PATH_LITERAL("MemoryBackingSyncedToDisk")),
            /*single_connection=*/true, /*journal_mode_wal=*/true));

    cache->InitializeCache(std::move(pending_backend));
  }

  // Reload the same persistent cache from disk
  {
    auto cache = base::MakeRefCounted<GpuPersistentCache>(
        "Test", MakeDefaultMemoryCache());
    ASSERT_OK_AND_ASSIGN(
        auto pending_backend,
        backend_storage_->MakePendingBackend(
            base::FilePath(FILE_PATH_LITERAL("MemoryBackingSyncedToDisk")),
            /*single_connection=*/true, /*journal_mode_wal=*/true));
    cache->InitializeCache(std::move(pending_backend));

    // Check that the entry exists in the cache.
    std::vector<char> buffer(value.size());
    size_t loaded_size =
        cache->LoadData(key.c_str(), key.size(), buffer.data(), buffer.size());

    EXPECT_EQ(loaded_size, value.size());
    EXPECT_EQ(std::string(buffer.begin(), buffer.end()), value);
  }
}

// Verifies that data stored in a persistent cache can be loaded back.
TEST_F(GpuPersistentCacheTest, ReOpenCacheFromFile) {
  const std::string key = "my_key";
  const std::string value = "my_value";

  // Store data to the persistent cache via store interface.
  {
    scoped_refptr<MemoryCache> memory_cache =
        base::MakeRefCounted<MemoryCache>(1024);
    auto cache = base::MakeRefCounted<GpuPersistentCache>("Test", memory_cache);
    ASSERT_OK_AND_ASSIGN(
        auto pending_backend,
        backend_storage_->MakePendingBackend(
            base::FilePath(FILE_PATH_LITERAL("ReOpenCacheFromFile")),
            /*single_connection=*/true, /*journal_mode_wal=*/true));
    cache->InitializeCache(std::move(pending_backend));

    cache->StoreData(key.c_str(), key.size(), value.c_str(), value.size());

    // Check that the entry exists in the memory cache.
    auto memory_entry = memory_cache->Find(key);
    EXPECT_NE(nullptr, memory_entry);
    EXPECT_EQ(value.size(), memory_entry->DataSize());
    EXPECT_EQ(
        std::string(memory_entry->Data().begin(), memory_entry->Data().end()),
        value);

    // Check that the entry exists in the persistent cache.
    std::vector<char> buffer(value.size());
    size_t loaded_size =
        cache->LoadData(key.c_str(), key.size(), buffer.data(), buffer.size());

    EXPECT_EQ(loaded_size, value.size());
    EXPECT_EQ(std::string(buffer.begin(), buffer.end()), value);
  }

  // Reload the same persistent cache from disk
  {
    scoped_refptr<MemoryCache> memory_cache =
        base::MakeRefCounted<MemoryCache>(1024);
    auto cache = base::MakeRefCounted<GpuPersistentCache>("Test", memory_cache);
    ASSERT_OK_AND_ASSIGN(
        auto pending_backend,
        backend_storage_->MakePendingBackend(
            base::FilePath(FILE_PATH_LITERAL("ReOpenCacheFromFile")),
            /*single_connection=*/true, /*journal_mode_wal=*/true));
    cache->InitializeCache(std::move(pending_backend));

    // Check that the entry exists in the persistent cache.
    std::vector<char> buffer(value.size());
    size_t loaded_size =
        cache->LoadData(key.c_str(), key.size(), buffer.data(), buffer.size());

    EXPECT_EQ(loaded_size, value.size());
    EXPECT_EQ(std::string(buffer.begin(), buffer.end()), value);

    // Check that the memory cache now contains the same entry after the
    // LoadData() call above
    auto memory_entry = memory_cache->Find(key);
    EXPECT_NE(nullptr, memory_entry);
    EXPECT_EQ(value.size(), memory_entry->DataSize());
    EXPECT_EQ(
        std::string(memory_entry->Data().begin(), memory_entry->Data().end()),
        value);
  }
}

}  // namespace gpu
