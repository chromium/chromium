// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/host/gpu_disk_cache.h"

#include "base/debug/leak_annotations.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "net/base/test_completion_callback.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {
namespace {

const char kCacheKey[] = "key";
const char kCacheValue[] = "cached value";
const char kCacheKey2[] = "key2";
const char kCacheValue2[] = "cached value2";

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

  const base::FilePath& cache_path() { return temp_dir_.GetPath(); }

  void InitCache() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    handle_ =
        factory_->GetCacheHandle(GpuDiskCacheType::kGlShaders, cache_path());
  }

  GpuDiskCacheFactory* factory() { return factory_.get(); }

  void TearDown() override {
    // Run all pending tasks before destroying TaskEnvironment. Otherwise,
    // SimpleEntryImpl instances bound to pending tasks are destroyed in an
    // incorrect state (see |state_| DCHECK in ~SimpleEntryImpl).
    task_environment_.RunUntilIdle();
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
  EXPECT_EQ(0, cache->Size());

  cache->Cache(kCacheKey, kCacheValue);

  net::TestCompletionCallback complete_cb;
  rv = cache->SetCacheCompleteCallback(complete_cb.callback());
  ASSERT_EQ(net::OK, complete_cb.GetResult(rv));
  EXPECT_EQ(1, cache->Size());

  base::Time time;
  net::TestCompletionCallback clear_cb;
  rv = cache->Clear(time, time, clear_cb.callback());
  ASSERT_EQ(net::OK, clear_cb.GetResult(rv));
  EXPECT_EQ(0, cache->Size());
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
  EXPECT_EQ(0, cache->Size());

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
  EXPECT_EQ(0, cache->Size());

  // Write two entries, wait for them to complete.
  const int32_t count = 2;
  cache->Cache(kCacheKey, kCacheValue);
  cache->Cache(kCacheKey2, kCacheValue2);
  net::TestCompletionCallback complete_cb;
  rv = cache->SetCacheCompleteCallback(complete_cb.callback());
  ASSERT_EQ(net::OK, complete_cb.GetResult(rv));
  EXPECT_EQ(count, cache->Size());

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

TEST_F(GpuDiskCacheTest, ReleasedCacheHandle) {
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

}  // namespace gpu
