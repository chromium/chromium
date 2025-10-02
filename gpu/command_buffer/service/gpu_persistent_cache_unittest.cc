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
    auto db_path = temp_dir_.GetPath().AppendASCII("test.db");
    auto journal_path = temp_dir_.GetPath().AppendASCII("test.journal");

    persistent_cache::BackendParams params;

    params.type = persistent_cache::BackendType::kSqlite;
    params.db_file = CreateFile(db_path);
    params.db_file_is_writable = true;
    params.journal_file = CreateFile(journal_path);
    params.journal_file_is_writable = true;
    params.shared_lock = base::UnsafeSharedMemoryRegion::Create(
        sizeof(persistent_cache::LockState));
    ASSERT_TRUE(params.db_file.IsValid());
    ASSERT_TRUE(params.journal_file.IsValid());
    ASSERT_TRUE(params.shared_lock.IsValid());

    cache_.InitializeCache(std::move(params));
  }

 protected:
  base::File CreateFile(const base::FilePath& path) {
    return base::File(path, base::File::FLAG_CREATE_ALWAYS |
                                base::File::FLAG_READ | base::File::FLAG_WRITE);
  }

  void RunStoreAndLoadDataMultiThreaded(int num_threads);

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  GpuPersistentCache cache_{"Test"};
};

// Tests basic store and load functionality on a single thread.
TEST_F(GpuPersistentCacheTest, StoreAndLoadData) {
  const std::string key = "my_key";
  const std::string value = "my_value";
  cache_.StoreData(key.c_str(), key.size(), value.c_str(), value.size());

  std::vector<char> buffer(value.size());
  size_t loaded_size =
      cache_.LoadData(key.c_str(), key.size(), buffer.data(), buffer.size());

  EXPECT_EQ(loaded_size, value.size());
  EXPECT_EQ(std::string(buffer.begin(), buffer.end()), value);
}

// Tests that loading a non-existent key returns 0.
TEST_F(GpuPersistentCacheTest, LoadNonExistentKey) {
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
  RunStoreAndLoadDataMultiThreaded(8);
}

// Some internal sql code especially tracings checks that they are called on a correct
// sequence. This test verifies that we can use the cache on multiple threads without
// violating sequence checkers. There is no need to stress test with many threads like the
// above StoreAndLoadDataMultiThreaded. A minimal number of threads should suffice
TEST_F(GpuPersistentCacheTest, StoreAndLoadDataMultiThreadedWithSqlTrace) {
  base::test::TracingEnvironment tracing_environment;
  base::trace_event::TraceLog::GetInstance()->SetEnabled(
      base::trace_event::TraceConfig("sql", ""));

  RunStoreAndLoadDataMultiThreaded(3);

  base::trace_event::TraceLog::GetInstance()->SetDisabled();
}

}  // namespace gpu
