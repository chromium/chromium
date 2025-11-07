// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gpu_persistent_cache.h"

#include "base/barrier_closure.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/test/trace_test_utils.h"
#include "base/trace_event/trace_config.h"
#include "base/trace_event/trace_log.h"
#include "components/persistent_cache/backend_params.h"
#include "components/persistent_cache/sqlite/vfs/sandboxed_file.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

class GpuPersistentCacheTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    cache_params_ = CreateBackendParams(/*create_new=*/true);
  }

 protected:
  persistent_cache::BackendParams CreateBackendParams(bool create_new) {
    base::FilePath db_path = temp_dir_.GetPath().AppendASCII("test.db");
    base::FilePath journal_path =
        temp_dir_.GetPath().AppendASCII("test.journal");

    uint32_t file_flags = base::File::FLAG_READ | base::File::FLAG_WRITE;
    if (create_new) {
      file_flags |= base::File::FLAG_CREATE_ALWAYS;
    } else {
      file_flags |= base::File::FLAG_OPEN;
    }

    persistent_cache::BackendParams params;
    params.type = persistent_cache::BackendType::kSqlite;
    params.db_file = base::File(db_path, file_flags);
    params.db_file_is_writable = true;
    params.journal_file = base::File(journal_path, file_flags);
    params.journal_file_is_writable = true;
    params.shared_lock = base::UnsafeSharedMemoryRegion::Create(
        sizeof(persistent_cache::SharedAtomicLock));
    CHECK(params.db_file.IsValid());
    CHECK(params.journal_file.IsValid());
    CHECK(params.shared_lock.IsValid());
    return params;
  }

  void InitializeCache() { cache_.InitializeCache(std::move(cache_params_)); }

  void RunStoreAndLoadDataMultiThreaded(int num_threads);

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir temp_dir_;
  persistent_cache::BackendParams cache_params_;
  GpuPersistentCache cache_{"Test"};
};

TEST_F(GpuPersistentCacheTest, StoreAndLoadDataBeforeInitialize) {
  // Don't initialize cache.
  const std::string key = "my_key";
  const std::string value = "my_value";

  // StoreData() won't do anything but also won't crash.
  cache_.StoreData(key.c_str(), key.size(), value.c_str(), value.size());

  // LoadData() will return zero size since there is no cache yet.
  EXPECT_EQ(cache_.LoadData(key.c_str(), key.size(), nullptr, 0), 0u);
}

// Tests basic store and load functionality on a single thread.
TEST_F(GpuPersistentCacheTest, StoreAndLoadData) {
  InitializeCache();

  const std::string key = "my_key";
  const std::string value = "my_value";
  cache_.StoreData(key.c_str(), key.size(), value.c_str(), value.size());

  std::vector<char> buffer(value.size());
  size_t loaded_size =
      cache_.LoadData(key.c_str(), key.size(), buffer.data(), buffer.size());

  EXPECT_EQ(loaded_size, value.size());
  EXPECT_EQ(std::string(buffer.begin(), buffer.end()), value);
}

// Tests basic load and store using the Skia, ANGLE and Dawn caching interfaces.
TEST_F(GpuPersistentCacheTest, StoreAndLoadDataMixedInterfaces) {
  InitializeCache();

  // Insert 3 key/value pairs with the 3 caching interfaces.
  const std::string key_dawn = "my_key_dawn";
  const std::string value_dawn = "my_value_dawn";
  cache_.StoreData(key_dawn.c_str(), key_dawn.size(), value_dawn.c_str(),
                   value_dawn.size());

  const std::string key_gr = "my_key_gr";
  sk_sp<SkData> key_gr_data =
      SkData::MakeWithoutCopy(key_gr.c_str(), key_gr.size());
  const std::string value_gr = "my_value_gr";
  sk_sp<SkData> value_gr_data =
      SkData::MakeWithoutCopy(value_gr.c_str(), value_gr.size());
  cache_.store(*key_gr_data, *value_gr_data);

  const std::string key_gl = "my_key_gl";
  const std::string value_gl = "my_value_gl";
  cache_.GLBlobCacheSet(key_gl.c_str(), static_cast<int64_t>(key_gl.size()),
                        value_gl.c_str(),
                        static_cast<int64_t>(value_gl.size()));

  // Load with dawn::Platform::CachingInterface
  auto test_load_dawn = [this](const std::string& key,
                               const std::string& value) {
    std::vector<char> buffer(value.size());
    size_t loaded_size =
        cache_.LoadData(key.c_str(), key.size(), buffer.data(), buffer.size());

    EXPECT_EQ(loaded_size, value.size());
    EXPECT_EQ(std::string(buffer.begin(), buffer.end()), value);
  };
  test_load_dawn(key_dawn, value_dawn);
  test_load_dawn(key_gr, value_gr);
  test_load_dawn(key_gl, value_gl);

  // Load with GrContextOptions::PersistentCache
  auto test_load_gr = [this](const std::string& key, const std::string& value) {
    sk_sp<SkData> key_data = SkData::MakeWithoutCopy(key.c_str(), key.size());
    sk_sp buffer = cache_.load(*key_data);

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
    int64_t loaded_size = cache_.GLBlobCacheGet(key.c_str(), key.size(),
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
      cache_.LoadData(key.c_str(), key.size(), buffer.data(), buffer.size());
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
            [](GpuPersistentCache* cache, int thread_id,
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
            &cache_, i, barrier));
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
      size_t loaded_size = cache_.LoadData(key.c_str(), key.size(),
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
 public:
  void SetUp() override {
    GpuPersistentCacheTest::SetUp();
    GpuPersistentCache::AsyncDiskWriteOpts options;
    options.task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
    async_cache_ =
        std::make_unique<GpuPersistentCache>("TestAsync", std::move(options));
    async_cache_->InitializeCache(std::move(cache_params_));
  }

 protected:
  std::unique_ptr<GpuPersistentCache> async_cache_;
};

// Tests that when an async task runner is used, the data is written to disk
// after a delay. It also verifies that the underlying DiskCache is kept alive
// by the posted task, even after the GpuPersistentCache instance is destroyed.
TEST_F(GpuPersistentCacheAsyncTest, StoreAndLoadDataAsync) {
  const std::string key = "my_key";
  const std::string value = "my_value";

  // Store data. This will be a delayed write.
  async_cache_->StoreData(key.c_str(), key.size(), value.c_str(), value.size());

  // Destroy the cache. The posted write task will keep the underlying
  // DiskCache alive until it has run.
  async_cache_.reset();

  // To verify that the write is delayed, we can create a new cache that reads
  // from the same files.
  GpuPersistentCache read_cache("TestRead");
  read_cache.InitializeCache(CreateBackendParams(/*create_new=*/false));

  // Immediately loading should fail.
  std::vector<char> buffer(value.size());
  size_t loaded_size = read_cache.LoadData(key.c_str(), key.size(),
                                           buffer.data(), buffer.size());
  EXPECT_EQ(loaded_size, 0u);

  // Fast forward time to trigger the write.
  task_environment_.FastForwardBy(base::Seconds(2));

  // Now loading should succeed.
  loaded_size = read_cache.LoadData(key.c_str(), key.size(), buffer.data(),
                                    buffer.size());
  EXPECT_EQ(loaded_size, value.size());
  EXPECT_EQ(std::string(buffer.begin(), buffer.end()), value);
}

// Tests the idle-rescheduling logic. If another cache operation occurs during
// the delay period, the write task should be postponed.
TEST_F(GpuPersistentCacheAsyncTest, StoreAndLoadDataAsync_IdleReschedule) {
  const std::string key = "my_key";
  const std::string value = "my_value";

  // Store data. This will be a delayed write.
  async_cache_->StoreData(key.c_str(), key.size(), value.c_str(), value.size());

  // Fast forward a bit, but less than the delay.
  task_environment_.FastForwardBy(base::Milliseconds(500));

  // Perform another operation to reset the idle timer.
  std::vector<char> dummy_buffer(1);
  async_cache_->LoadData("some_other_key", 14, dummy_buffer.data(), 1);

  // Fast forward past the original delay time. The write should not have
  // happened yet because it was rescheduled.
  task_environment_.FastForwardBy(base::Seconds(1));

  // Verify write has not happened by reading from a new cache instance.
  // Destroy the original cache first.
  async_cache_.reset();

  GpuPersistentCache read_cache("TestRead");
  read_cache.InitializeCache(CreateBackendParams(/*create_new=*/false));

  std::vector<char> buffer(value.size());
  size_t loaded_size = read_cache.LoadData(key.c_str(), key.size(),
                                           buffer.data(), buffer.size());
  EXPECT_EQ(loaded_size, 0u);

  // Fast forward again to let the rescheduled write complete.
  task_environment_.FastForwardBy(base::Seconds(1));

  // Now it should be there.
  loaded_size = read_cache.LoadData(key.c_str(), key.size(), buffer.data(),
                                    buffer.size());
  EXPECT_EQ(loaded_size, value.size());
  EXPECT_EQ(std::string(buffer.begin(), buffer.end()), value);
}

// Tests that if pending bytes exceed the limit, the write happens after the
// first delay without being rescheduled.
TEST_F(GpuPersistentCacheAsyncTest,
       StoreAndLoadDataAsync_ExceedMaxPendingBytes) {
  // Recreate the cache with a pending byte limit.
  GpuPersistentCache::AsyncDiskWriteOpts options;
  options.task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
  options.max_pending_bytes_to_write = 10;
  async_cache_ =
      std::make_unique<GpuPersistentCache>("TestAsync", std::move(options));
  async_cache_->InitializeCache(CreateBackendParams(/*create_new=*/false));

  const std::string key = "my_key";
  const std::string value = "my_value_is_longer_than_10";
  ASSERT_GT(key.size() + value.size(), 10u);

  // Store data. This will be a delayed write.
  async_cache_->StoreData(key.c_str(), key.size(), value.c_str(), value.size());

  // Fast forward a bit, but less than the delay.
  task_environment_.FastForwardBy(base::Milliseconds(500));

  // Perform another operation to reset the idle timer. This is to ensure that
  // the write is triggered by the pending bytes limit and not the idle timeout.
  std::vector<char> dummy_buffer(1);
  async_cache_->LoadData("some_other_key", 14, dummy_buffer.data(), 1);

  // Fast forward past the delay. The write should have happened because of the
  // pending bytes limit, even though the cache was not idle.
  task_environment_.FastForwardBy(base::Seconds(1));

  // Destroy the cache.
  async_cache_.reset();

  // The write should have happened. We can verify this by creating a new cache
  // that reads from the same files.
  GpuPersistentCache read_cache("TestRead");
  read_cache.InitializeCache(CreateBackendParams(/*create_new=*/false));

  // Now loading should succeed.
  std::vector<char> buffer(value.size());
  size_t loaded_size = read_cache.LoadData(key.c_str(), key.size(),
                                           buffer.data(), buffer.size());
  EXPECT_EQ(loaded_size, value.size());
  EXPECT_EQ(std::string(buffer.begin(), buffer.end()), value);
}

}  // namespace gpu
