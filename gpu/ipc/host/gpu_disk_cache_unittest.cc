// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/host/gpu_disk_cache.h"

#include <string>

#include "base/debug/leak_annotations.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "net/base/features.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {
namespace {

const char kCacheKey[] = "key";
const char kCacheValue[] = "cached value";
const char kCacheKey2[] = "key2";
const char kCacheValue2[] = "cached value2";

// Creates `dir` containing a single file of `size_kb` KB.
[[nodiscard]] bool CreateDirWithSize(const base::FilePath& dir, int size_kb) {
  return base::CreateDirectory(dir) &&
         base::WriteFile(dir.AppendASCII("data"),
                         std::string(size_kb * 1024, 'x'));
}

// Creates a cache for `handle`, waits until it is available and then releases
// it.
void CreateAndReleaseCache(GpuDiskCacheFactory* factory,
                           const GpuDiskCacheHandle& handle) {
  scoped_refptr<GpuDiskCache> cache = factory->Create(handle);
  ASSERT_TRUE(cache);
  net::TestCompletionCallback available_cb;
  ASSERT_EQ(net::OK, available_cb.GetResult(
                         cache->SetAvailableCallback(available_cb.callback())));
}

}  // namespace

class GpuDiskCacheTest : public testing::Test {
 protected:
  GpuDiskCacheTest() {
    // Leak the factory on purpose. In production, the factory is a singleton,
    // and when a GpuDiskCache object is created, a second reference to it is
    // added to a globally held Backend object. These instances may leak, by
    // design, and must have a valid reference to the factory, otherwise raw_ptr
    // checks will fail. See https://crbug.com/1486674
    factory_ = new GpuDiskCacheFactory;
    ANNOTATE_LEAKING_OBJECT_PTR(factory_);
  }

  GpuDiskCacheTest(const GpuDiskCacheTest&) = delete;
  GpuDiskCacheTest& operator=(const GpuDiskCacheTest&) = delete;

  ~GpuDiskCacheTest() override = default;

  base::FilePath cache_path() {
    return temp_dir_.GetPath().AppendASCII("cache");
  }

  void InitCache() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    handle_ =
        factory_->GetCacheHandle(GpuDiskCacheType::kGlShaders, cache_path());
  }

  GpuDiskCacheFactory* factory() { return factory_.get(); }

  void FlushCacheTasks() {
    disk_cache::FlushCacheThreadForTesting();
    task_environment_.RunUntilIdle();
  }

  void TearDown() override {
    // Run all pending tasks before destroying TaskEnvironment. Otherwise,
    // SimpleEntryImpl instances bound to pending tasks are destroyed in an
    // incorrect state (see |state_| DCHECK in ~SimpleEntryImpl).
    FlushCacheTasks();
  }

  int32_t GetCacheSize(GpuDiskCache* cache) {
    base::test::TestFuture<int32_t> future;
    base::expected<int32_t, net::Error> result =
        cache->Size(future.GetCallback());
    if (result.has_value()) {
      return result.value();
    }
    CHECK_EQ(result.error(), net::ERR_IO_PENDING);
    return future.Get();
  }

  // Creates a cache whose backend will fail to initialize (by placing a file
  // where the cache directory would be).
  scoped_refptr<GpuDiskCache> CreateCacheWithFailingBackend() {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::FilePath invalid_path = temp_dir_.GetPath().AppendASCII("not_a_dir");
    EXPECT_TRUE(base::WriteFile(invalid_path, "x"));
    handle_ =
        factory()->GetCacheHandle(GpuDiskCacheType::kGlShaders, invalid_path);
    return factory()->Create(handle_, base::DoNothing(), base::DoNothing(),
                             disk_cache::ResetHandling::kNeverReset);
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  raw_ptr<GpuDiskCacheFactory> factory_;
  GpuDiskCacheHandle handle_;
};

TEST_F(GpuDiskCacheTest, ClearsCache) {
  InitCache();

  scoped_refptr<GpuDiskCache> cache = factory()->Create(handle_);
  ASSERT_TRUE(cache.get() != nullptr);

  net::TestCompletionCallback available_cb;
  int rv = cache->SetAvailableCallback(available_cb.callback());
  ASSERT_EQ(net::OK, available_cb.GetResult(rv));
  EXPECT_EQ(0, GetCacheSize(cache.get()));

  cache->Cache(kCacheKey, kCacheValue);

  net::TestCompletionCallback complete_cb;
  rv = cache->SetCacheCompleteCallback(complete_cb.callback());
  ASSERT_EQ(net::OK, complete_cb.GetResult(rv));
  EXPECT_EQ(1, GetCacheSize(cache.get()));

  base::Time time;
  net::TestCompletionCallback clear_cb;
  rv = cache->Clear(time, time, clear_cb.callback());
  ASSERT_EQ(net::OK, clear_cb.GetResult(rv));
  EXPECT_EQ(0, GetCacheSize(cache.get()));
}

TEST_F(GpuDiskCacheTest, ClearByPathTriggersCallback) {
  InitCache();
  factory()->Create(handle_)->Cache(kCacheKey, kCacheValue);
  net::TestCompletionCallback test_callback;
  factory()->ClearByPath(
      cache_path(), base::Time(), base::Time::Max(),
      base::BindLambdaForTesting([&]() { test_callback.callback().Run(1); }));
  ASSERT_TRUE(test_callback.WaitForResult());
}

// Important for clearing in-memory profiles.
TEST_F(GpuDiskCacheTest, ClearByPathWithEmptyPathTriggersCallback) {
  net::TestCompletionCallback test_callback;
  factory()->ClearByPath(
      base::FilePath(), base::Time(), base::Time::Max(),
      base::BindLambdaForTesting([&]() { test_callback.callback().Run(1); }));
  ASSERT_TRUE(test_callback.WaitForResult());
}

TEST_F(GpuDiskCacheTest, ClearByPathWithNoExistingCache) {
  // Create a dir but not creating a gpu cache under it.
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

  net::TestCompletionCallback test_callback;
  factory()->ClearByPath(
      cache_path(), base::Time(), base::Time::Max(),
      base::BindLambdaForTesting([&]() { test_callback.callback().Run(1); }));
  ASSERT_TRUE(test_callback.WaitForResult());

  // No files should be written to the cache path.
  EXPECT_EQ(0, base::ComputeDirectorySize(cache_path()));
}

// For https://crbug.com/663589.
TEST_F(GpuDiskCacheTest, SafeToDeleteCacheMidEntryOpen) {
  InitCache();

  // Create a cache and wait for it to open.
  scoped_refptr<GpuDiskCache> cache = factory()->Create(handle_);
  ASSERT_TRUE(cache.get() != nullptr);
  net::TestCompletionCallback available_cb;
  int rv = cache->SetAvailableCallback(available_cb.callback());
  ASSERT_EQ(net::OK, available_cb.GetResult(rv));
  EXPECT_EQ(0, GetCacheSize(cache.get()));

  // Start writing an entry to the cache but delete it before the backend has
  // finished opening the entry. There is a race here, so this usually (but not
  // always) crashes if there is a problem.
  cache->Cache(kCacheKey, kCacheValue);
  cache = nullptr;

  // Open a new cache (to pass time on the cache thread) and verify all is well.
  cache = factory()->Create(handle_);
  ASSERT_TRUE(cache.get() != nullptr);
  net::TestCompletionCallback available_cb2;
  int rv2 = cache->SetAvailableCallback(available_cb2.callback());
  ASSERT_EQ(net::OK, available_cb2.GetResult(rv2));
}

TEST_F(GpuDiskCacheTest, MultipleLoaderCallbacks) {
  InitCache();

  // Create a cache and wait for it to open.
  scoped_refptr<GpuDiskCache> cache = factory()->Create(handle_);
  ASSERT_TRUE(cache.get() != nullptr);
  net::TestCompletionCallback available_cb;
  int rv = cache->SetAvailableCallback(available_cb.callback());
  ASSERT_EQ(net::OK, available_cb.GetResult(rv));
  EXPECT_EQ(0, GetCacheSize(cache.get()));

  // Write two entries, wait for them to complete.
  const int32_t count = 2;
  cache->Cache(kCacheKey, kCacheValue);
  cache->Cache(kCacheKey2, kCacheValue2);
  net::TestCompletionCallback complete_cb;
  rv = cache->SetCacheCompleteCallback(complete_cb.callback());
  ASSERT_EQ(net::OK, complete_cb.GetResult(rv));
  EXPECT_EQ(count, GetCacheSize(cache.get()));

  // Close, re-open, and verify that two entries were loaded.
  cache = nullptr;
  int loaded_calls = 0;
  cache = factory()->Create(
      handle_, base::BindLambdaForTesting(
                   [&loaded_calls](
                       const GpuDiskCacheHandle& handle, const std::string& key,
                       const std::string& value) { ++loaded_calls; }));
  ASSERT_TRUE(cache.get() != nullptr);
  net::TestCompletionCallback available_cb2;
  int rv2 = cache->SetAvailableCallback(available_cb2.callback());
  ASSERT_EQ(net::OK, available_cb2.GetResult(rv2));
  EXPECT_EQ(count, loaded_calls);
}

TEST_F(GpuDiskCacheTest, ModifyExistingKey) {
  InitCache();

  scoped_refptr<GpuDiskCache> cache = factory()->Create(handle_);
  ASSERT_TRUE(cache.get() != nullptr);

  {
    net::TestCompletionCallback available_cb;
    int rv = cache->SetAvailableCallback(available_cb.callback());
    ASSERT_EQ(net::OK, available_cb.GetResult(rv));
  }
  EXPECT_EQ(0, GetCacheSize(cache.get()));

  cache->Cache(kCacheKey, kCacheValue2);

  {
    net::TestCompletionCallback complete_cb;
    int rv = cache->SetCacheCompleteCallback(complete_cb.callback());
    ASSERT_EQ(net::OK, complete_cb.GetResult(rv));
  }
  EXPECT_EQ(1, GetCacheSize(cache.get()));

  // Cache a different value to the same key. The new value should be smaller
  // than old value to ensure the old value is fully removed from cache.
  cache->Cache(kCacheKey, kCacheValue);
  ASSERT_LT(std::string_view(kCacheKey).size(),
            std::string_view(kCacheKey2).size());

  {
    net::TestCompletionCallback complete_cb;
    int rv = cache->SetCacheCompleteCallback(complete_cb.callback());
    ASSERT_EQ(net::OK, complete_cb.GetResult(rv));
  }
  EXPECT_EQ(1, GetCacheSize(cache.get()));

  // Close, re-open, and verify that the second Cache() modified the value on
  // disk.
  cache = nullptr;
  std::vector<std::pair<std::string, std::string>> loaded_data;
  cache = factory()->Create(
      handle_,
      base::BindLambdaForTesting(
          [&loaded_data](const GpuDiskCacheHandle& handle,
                         const std::string& key, const std::string& value) {
            loaded_data.emplace_back(key, value);
          }));
  ASSERT_TRUE(cache.get() != nullptr);

  {
    net::TestCompletionCallback available_cb;
    int rv = cache->SetAvailableCallback(available_cb.callback());
    ASSERT_EQ(net::OK, available_cb.GetResult(rv));
  }
  EXPECT_THAT(loaded_data,
              testing::ElementsAre(std::pair(kCacheKey, kCacheValue)));
}

#if defined(ADDRESS_SANITIZER) && BUILDFLAG(IS_LINUX)
#define MAYBE_ReleasedCacheHandle DISABLED_ReleasedCacheHandle
#else
#define MAYBE_ReleasedCacheHandle ReleasedCacheHandle
#endif
TEST_F(GpuDiskCacheTest, MAYBE_ReleasedCacheHandle) {
  // Init cache registers the handle.
  InitCache();

  {
    // Create a cache and use it to remove the handle.
    scoped_refptr<GpuDiskCache> cache = factory()->Create(handle_);
    ASSERT_TRUE(cache.get() != nullptr);
    factory()->ReleaseCacheHandle(cache.get());
  }

  // It should no longer be possible to get or create a cache using the handle.
  {
    scoped_refptr<GpuDiskCache> cache = factory()->Create(handle_);
    ASSERT_TRUE(cache.get() == nullptr);
  }
  {
    scoped_refptr<GpuDiskCache> cache = factory()->Get(handle_);
    ASSERT_TRUE(cache.get() == nullptr);
  }
}

TEST_F(GpuDiskCacheTest, DestroyedCallbackCalledOneInstance) {
  InitCache();

  // Create a cache with a destroy callback set.
  bool destroyed = false;
  base::RunLoop run_loop;
  {
    scoped_refptr<GpuDiskCache> cache = factory()->Create(
        handle_, base::DoNothing(),
        base::BindLambdaForTesting(
            [&destroyed, &run_loop](const GpuDiskCacheHandle&) {
              destroyed = true;
              run_loop.Quit();
            }));
    ASSERT_TRUE(cache.get() != nullptr);
  }
  // Destroying the last and only reference to the cache should cause the
  // callback to run.
  run_loop.Run();
  EXPECT_TRUE(destroyed);
}

TEST_F(GpuDiskCacheTest, DestroyedCallbackCalledMultipleInstance) {
  InitCache();

  // Create a cache with a destroy callback set.
  bool destroyed = false;
  base::RunLoop run_loop;
  scoped_refptr<GpuDiskCache> cache_1 =
      factory()->Create(handle_, base::DoNothing(),
                        base::BindLambdaForTesting(
                            [&destroyed, &run_loop](const GpuDiskCacheHandle&) {
                              destroyed = true;
                              run_loop.Quit();
                            }));
  ASSERT_TRUE(cache_1.get() != nullptr);

  // Get another instance of the same cache.
  scoped_refptr<GpuDiskCache> cache_2 = factory()->Get(handle_);
  ASSERT_TRUE(cache_2.get() == cache_1.get());

  // Destroying one of the references should not trigger the callback.
  cache_1 = nullptr;
  EXPECT_FALSE(destroyed);

  // Destroying the last reference should trigger the callback.
  cache_2 = nullptr;
  run_loop.Run();
  EXPECT_TRUE(destroyed);
}

// Verifies that ClearByCache completes even when the disk cache backend fails
// to initialize.
TEST_F(GpuDiskCacheTest, ClearByCacheCompletesWhenCacheCreationFails) {
  scoped_refptr<GpuDiskCache> cache = CreateCacheWithFailingBackend();
  ASSERT_TRUE(cache.get() != nullptr);

  // Start a clear operation. The clear helper will call SetAvailableCallback
  // and pend because the cache is not yet available. When the async backend
  // creation fails, the pending callback fires and completes the clear.
  base::RunLoop run_loop;
  factory()->ClearByCache(cache, base::Time(), base::Time::Max(),
                          run_loop.QuitClosure());
  run_loop.Run();
}

// Verifies that SetAvailableCallback returns the error immediately when cache
// creation has already failed (i.e., the failure arrived before the clear
// operation started).
TEST_F(GpuDiskCacheTest, SetAvailableCallbackReturnsErrorAfterCreationFailure) {
  scoped_refptr<GpuDiskCache> cache = CreateCacheWithFailingBackend();
  ASSERT_TRUE(cache.get() != nullptr);

  // Wait for the async backend creation failure to arrive.
  net::TestCompletionCallback available_cb;
  int rv = cache->SetAvailableCallback(available_cb.callback());
  ASSERT_EQ(net::ERR_IO_PENDING, rv);
  ASSERT_NE(net::OK, available_cb.WaitForResult());

  // SetAvailableCallback should now return the error immediately.
  bool clear_completed = false;
  factory()->ClearByCache(cache, base::Time(), base::Time::Max(),
                          base::BindLambdaForTesting([&clear_completed]() {
                            clear_completed = true;
                          }));

  EXPECT_TRUE(clear_completed);
}

TEST_F(GpuDiskCacheTest, ShaderDiskCacheBackendExperimentDisabledDoesNotWipe) {
  InitCache();

  // Populate cache with the experiment disabled.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(
        net::features::kShaderDiskCacheBackendExperiment);

    scoped_refptr<GpuDiskCache> cache = factory()->Create(handle_);
    ASSERT_TRUE(cache);
    net::TestCompletionCallback available_cb;
    ASSERT_EQ(net::OK, available_cb.GetResult(cache->SetAvailableCallback(
                           available_cb.callback())));
    cache->Cache(kCacheKey, kCacheValue);
    net::TestCompletionCallback complete_cb;
    ASSERT_EQ(net::OK, complete_cb.GetResult(cache->SetCacheCompleteCallback(
                           complete_cb.callback())));
  }
  FlushCacheTasks();

  // Re-open with the experiment still disabled (Group 1: No-op) and verify
  // existing entries are preserved.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(
        net::features::kShaderDiskCacheBackendExperiment);

    std::vector<std::pair<std::string, std::string>> loaded_data;
    scoped_refptr<GpuDiskCache> cache = factory()->Create(
        handle_,
        base::BindLambdaForTesting([&loaded_data](const GpuDiskCacheHandle&,
                                                  const std::string& key,
                                                  const std::string& value) {
          loaded_data.emplace_back(key, value);
        }));
    ASSERT_TRUE(cache);
    net::TestCompletionCallback available_cb;
    ASSERT_EQ(net::OK, available_cb.GetResult(cache->SetAvailableCallback(
                           available_cb.callback())));
    EXPECT_THAT(loaded_data,
                testing::ElementsAre(std::pair(kCacheKey, kCacheValue)));
  }
}

TEST_F(GpuDiskCacheTest, ShaderDiskCacheBackendExperimentDefaultWipesOnce) {
  InitCache();

  // Populate a pre-experiment cache on disk.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(
        net::features::kShaderDiskCacheBackendExperiment);

    scoped_refptr<GpuDiskCache> cache = factory()->Create(handle_);
    ASSERT_TRUE(cache);
    net::TestCompletionCallback available_cb;
    ASSERT_EQ(net::OK, available_cb.GetResult(cache->SetAvailableCallback(
                           available_cb.callback())));
    cache->Cache(kCacheKey, kCacheValue);
    net::TestCompletionCallback complete_cb;
    ASSERT_EQ(net::OK, complete_cb.GetResult(cache->SetCacheCompleteCallback(
                           complete_cb.callback())));
  }
  FlushCacheTasks();

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      net::features::kShaderDiskCacheBackendExperiment, {{"type", "default"}});

  // First open in Group 2 (Status quo + wipe) must wipe the pre-existing cache
  // and write the new entry.
  {
    std::vector<std::pair<std::string, std::string>> loaded_data;
    scoped_refptr<GpuDiskCache> cache = factory()->Create(
        handle_,
        base::BindLambdaForTesting([&loaded_data](const GpuDiskCacheHandle&,
                                                  const std::string& key,
                                                  const std::string& value) {
          loaded_data.emplace_back(key, value);
        }));
    ASSERT_TRUE(cache);
    net::TestCompletionCallback available_cb;
    ASSERT_EQ(net::OK, available_cb.GetResult(cache->SetAvailableCallback(
                           available_cb.callback())));
    EXPECT_TRUE(loaded_data.empty());
    EXPECT_EQ(0, GetCacheSize(cache.get()));

    cache->Cache(kCacheKey2, kCacheValue2);
    net::TestCompletionCallback complete_cb;
    ASSERT_EQ(net::OK, complete_cb.GetResult(cache->SetCacheCompleteCallback(
                           complete_cb.callback())));
    EXPECT_EQ(1, GetCacheSize(cache.get()));
  }
  FlushCacheTasks();

  // Second open in Group 2 must NOT wipe the cache again.
  {
    std::vector<std::pair<std::string, std::string>> loaded_data;
    scoped_refptr<GpuDiskCache> cache = factory()->Create(
        handle_,
        base::BindLambdaForTesting([&loaded_data](const GpuDiskCacheHandle&,
                                                  const std::string& key,
                                                  const std::string& value) {
          loaded_data.emplace_back(key, value);
        }));
    ASSERT_TRUE(cache);
    net::TestCompletionCallback available_cb;
    ASSERT_EQ(net::OK, available_cb.GetResult(cache->SetAvailableCallback(
                           available_cb.callback())));
    EXPECT_THAT(loaded_data,
                testing::ElementsAre(std::pair(kCacheKey2, kCacheValue2)));
  }
}

TEST_F(GpuDiskCacheTest, ShaderDiskCacheBackendExperimentSimpleWipesOnce) {
  InitCache();

  // Populate a pre-experiment cache on disk.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(
        net::features::kShaderDiskCacheBackendExperiment);

    scoped_refptr<GpuDiskCache> cache = factory()->Create(handle_);
    ASSERT_TRUE(cache);
    net::TestCompletionCallback available_cb;
    ASSERT_EQ(net::OK, available_cb.GetResult(cache->SetAvailableCallback(
                           available_cb.callback())));
    cache->Cache(kCacheKey, kCacheValue);
    net::TestCompletionCallback complete_cb;
    ASSERT_EQ(net::OK, complete_cb.GetResult(cache->SetCacheCompleteCallback(
                           complete_cb.callback())));
  }
  FlushCacheTasks();

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      net::features::kShaderDiskCacheBackendExperiment, {{"type", "simple"}});

  // First open in Group 3 (SimpleCache + wipe) must wipe the pre-existing
  // cache, recreate it with SimpleCache, and write the new entry.
  {
    std::vector<std::pair<std::string, std::string>> loaded_data;
    scoped_refptr<GpuDiskCache> cache = factory()->Create(
        handle_,
        base::BindLambdaForTesting([&loaded_data](const GpuDiskCacheHandle&,
                                                  const std::string& key,
                                                  const std::string& value) {
          loaded_data.emplace_back(key, value);
        }));
    ASSERT_TRUE(cache);
    net::TestCompletionCallback available_cb;
    ASSERT_EQ(net::OK, available_cb.GetResult(cache->SetAvailableCallback(
                           available_cb.callback())));
    EXPECT_TRUE(loaded_data.empty());
    EXPECT_EQ(0, GetCacheSize(cache.get()));

    cache->Cache(kCacheKey2, kCacheValue2);
    net::TestCompletionCallback complete_cb;
    ASSERT_EQ(net::OK, complete_cb.GetResult(cache->SetCacheCompleteCallback(
                           complete_cb.callback())));
    EXPECT_EQ(1, GetCacheSize(cache.get()));
  }

  // Closing SimpleBackendImpl flushes the real index into `index-dir`.
  FlushCacheTasks();
  EXPECT_TRUE(base::DirectoryExists(cache_path().AppendASCII("index-dir")));

  // Second open in Group 3 must NOT wipe the cache again.
  {
    std::vector<std::pair<std::string, std::string>> loaded_data;
    scoped_refptr<GpuDiskCache> cache = factory()->Create(
        handle_,
        base::BindLambdaForTesting([&loaded_data](const GpuDiskCacheHandle&,
                                                  const std::string& key,
                                                  const std::string& value) {
          loaded_data.emplace_back(key, value);
        }));
    ASSERT_TRUE(cache);
    net::TestCompletionCallback available_cb;
    ASSERT_EQ(net::OK, available_cb.GetResult(cache->SetAvailableCallback(
                           available_cb.callback())));
    EXPECT_THAT(loaded_data,
                testing::ElementsAre(std::pair(kCacheKey2, kCacheValue2)));
  }
}

TEST_F(GpuDiskCacheTest,
       ShaderDiskCacheBackendExperimentWipesExistingSimpleCacheOnce) {
  InitCache();

  // Populate a pre-experiment SimpleCache on disk (simulating platforms such
  // as Android/Fuchsia where SimpleCache is the default backend).
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(
        net::features::kShaderDiskCacheBackendExperiment);

    base::test::TestFuture<disk_cache::BackendResult> create_future;
    disk_cache::BackendResult rv = disk_cache::CreateCacheBackend(
        net::SHADER_CACHE, net::CACHE_BACKEND_SIMPLE,
        /*file_operations=*/nullptr, cache_path(), /*max_bytes=*/0,
        disk_cache::ResetHandling::kResetOnError, /*net_log=*/nullptr,
        /*cache_encryption_delegate=*/nullptr, create_future.GetCallback());
    ASSERT_EQ(net::ERR_IO_PENDING, rv.net_error);
    disk_cache::BackendResult backend_result = create_future.Take();
    ASSERT_EQ(net::OK, backend_result.net_error);

    base::test::TestFuture<disk_cache::EntryResult> entry_future;
    disk_cache::EntryResult entry_rv = backend_result.backend->CreateEntry(
        kCacheKey, net::HIGHEST, entry_future.GetCallback());
    if (entry_rv.net_error() == net::ERR_IO_PENDING) {
      entry_rv = entry_future.Take();
    }
    ASSERT_EQ(net::OK, entry_rv.net_error());
    entry_rv.ReleaseEntry()->Close();
  }
  FlushCacheTasks();

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      net::features::kShaderDiskCacheBackendExperiment, {{"type", "simple"}});

  // First open under the experiment must wipe the pre-existing SimpleCache.
  {
    scoped_refptr<GpuDiskCache> cache = factory()->Create(handle_);
    ASSERT_TRUE(cache);
    net::TestCompletionCallback available_cb;
    ASSERT_EQ(net::OK, available_cb.GetResult(cache->SetAvailableCallback(
                           available_cb.callback())));
    EXPECT_EQ(0, GetCacheSize(cache.get()));

    cache->Cache(kCacheKey2, kCacheValue2);
    net::TestCompletionCallback complete_cb;
    ASSERT_EQ(net::OK, complete_cb.GetResult(cache->SetCacheCompleteCallback(
                           complete_cb.callback())));
    EXPECT_EQ(1, GetCacheSize(cache.get()));
  }
  FlushCacheTasks();

  // Second open under the experiment must preserve entries.
  {
    std::vector<std::pair<std::string, std::string>> loaded_data;
    scoped_refptr<GpuDiskCache> cache = factory()->Create(
        handle_,
        base::BindLambdaForTesting([&loaded_data](const GpuDiskCacheHandle&,
                                                  const std::string& key,
                                                  const std::string& value) {
          loaded_data.emplace_back(key, value);
        }));
    ASSERT_TRUE(cache);
    net::TestCompletionCallback available_cb;
    ASSERT_EQ(net::OK, available_cb.GetResult(cache->SetAvailableCallback(
                           available_cb.callback())));
    EXPECT_THAT(loaded_data,
                testing::ElementsAre(std::pair(kCacheKey2, kCacheValue2)));
  }
}

#if BUILDFLAG(IS_WIN)
TEST_F(GpuDiskCacheTest, RecordsDiskSizeWhenCacheIsCreated) {
  constexpr char kGlHistogram[] = "GPU.ShaderCache.DiskSize.GlShaders";
  constexpr char kWebGpuHistogram[] = "GPU.ShaderCache.DiskSize.DawnWebGPU";
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  const base::FilePath gl_path = cache_path().AppendASCII("GPUCache");
  const base::FilePath webgpu_path =
      cache_path().AppendASCII("DawnWebGPUCache");
  ASSERT_TRUE(CreateDirWithSize(gl_path, 10));
  ASSERT_TRUE(CreateDirWithSize(webgpu_path, 20));

  const GpuDiskCacheHandle gl_handle =
      factory()->GetCacheHandle(GpuDiskCacheType::kGlShaders, gl_path);
  factory()->GetCacheHandle(GpuDiskCacheType::kDawnWebGPU, webgpu_path);
  base::ThreadPoolInstance::Get()->FlushForTesting();

  // Registering a cache doesn't record its size.
  histogram_tester.ExpectTotalCount(kGlHistogram, 0);
  histogram_tester.ExpectTotalCount(kWebGpuHistogram, 0);

  ASSERT_NO_FATAL_FAILURE(CreateAndReleaseCache(factory(), gl_handle));
  base::ThreadPoolInstance::Get()->FlushForTesting();

  // The size can include files created by the cache backend, so only check a
  // lower bound.
  histogram_tester.ExpectTotalCount(kGlHistogram, 1);
  EXPECT_GE(histogram_tester.GetTotalSum(kGlHistogram), 10);
  histogram_tester.ExpectTotalCount(kWebGpuHistogram, 0);
}

TEST_F(GpuDiskCacheTest, RecordsDiskSizeOncePerPath) {
  constexpr char kHistogram[] = "GPU.ShaderCache.DiskSize.GlShaders";
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  const base::FilePath gl_path = cache_path().AppendASCII("GPUCache");
  ASSERT_TRUE(CreateDirWithSize(gl_path, 10));

  const GpuDiskCacheHandle gl_handle =
      factory()->GetCacheHandle(GpuDiskCacheType::kGlShaders, gl_path);
  {
    scoped_refptr<GpuDiskCache> cache = factory()->Create(gl_handle);
    ASSERT_TRUE(cache);
    net::TestCompletionCallback available_cb;
    ASSERT_EQ(net::OK, available_cb.GetResult(cache->SetAvailableCallback(
                           available_cb.callback())));
    // Release the only reference to `gl_handle`, which unregisters `gl_path`.
    factory()->ReleaseCacheHandle(cache.get());
  }

  // Registering `gl_path` again creates a new handle, but creating a cache for
  // it must not record the size again.
  const GpuDiskCacheHandle new_gl_handle =
      factory()->GetCacheHandle(GpuDiskCacheType::kGlShaders, gl_path);
  EXPECT_NE(gl_handle, new_gl_handle);
  ASSERT_NO_FATAL_FAILURE(CreateAndReleaseCache(factory(), new_gl_handle));
  base::ThreadPoolInstance::Get()->FlushForTesting();

  histogram_tester.ExpectTotalCount(kHistogram, 1);
}

TEST_F(GpuDiskCacheTest, RecordsDiskSizeForReservedHandles) {
  constexpr char kCompositorHistogram[] =
      "GPU.ShaderCache.DiskSize.DisplayCompositor";
  constexpr char kGrShaderHistogram[] = "GPU.ShaderCache.DiskSize.GrShader";
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  const base::FilePath compositor_path =
      cache_path().AppendASCII("ShaderCache");
  const base::FilePath gr_shader_path =
      cache_path().AppendASCII("GrShaderCache");
  ASSERT_TRUE(CreateDirWithSize(compositor_path, 10));
  ASSERT_TRUE(CreateDirWithSize(gr_shader_path, 20));

  GpuDiskCacheFactory::HandleToPathMap reserved_handles;
  reserved_handles.emplace(kDisplayCompositorGpuDiskCacheHandle,
                           compositor_path);
  reserved_handles.emplace(kGrShaderGpuDiskCacheHandle, gr_shader_path);
  // Leaked on purpose, like `factory_`.
  GpuDiskCacheFactory* reserved_factory =
      new GpuDiskCacheFactory(reserved_handles);
  ANNOTATE_LEAKING_OBJECT_PTR(reserved_factory);
  base::ThreadPoolInstance::Get()->FlushForTesting();

  // Registering the reserved handles doesn't record their sizes.
  histogram_tester.ExpectTotalCount(kCompositorHistogram, 0);
  histogram_tester.ExpectTotalCount(kGrShaderHistogram, 0);

  ASSERT_NO_FATAL_FAILURE(
      CreateAndReleaseCache(reserved_factory, kGrShaderGpuDiskCacheHandle));
  base::ThreadPoolInstance::Get()->FlushForTesting();

  histogram_tester.ExpectTotalCount(kGrShaderHistogram, 1);
  EXPECT_GE(histogram_tester.GetTotalSum(kGrShaderHistogram), 20);
  histogram_tester.ExpectTotalCount(kCompositorHistogram, 0);
}
#else
TEST_F(GpuDiskCacheTest, DoesNotRecordDiskSize) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  const base::FilePath gl_path = cache_path().AppendASCII("GPUCache");
  ASSERT_TRUE(CreateDirWithSize(gl_path, 10));

  ASSERT_NO_FATAL_FAILURE(CreateAndReleaseCache(
      factory(),
      factory()->GetCacheHandle(GpuDiskCacheType::kGlShaders, gl_path)));
  base::ThreadPoolInstance::Get()->FlushForTesting();

  // The sizes are only recorded on Windows.
  histogram_tester.ExpectTotalCount("GPU.ShaderCache.DiskSize.GlShaders", 0);
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace gpu
