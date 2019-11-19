// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/host/shader_disk_cache.h"

#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "net/base/test_completion_callback.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {
namespace {

const int kDefaultClientId = 42;
const char kCacheKey[] = "key";
const char kCacheValue[] = "cached value";
const char kCacheKey2[] = "key2";
const char kCacheValue2[] = "cached value2";

}  // namespace

class ShaderDiskCacheTest : public testing::Test {
 public:
  ShaderDiskCacheTest() = default;

  ~ShaderDiskCacheTest() override = default;

  const base::FilePath& cache_path() { return temp_dir_.GetPath(); }

  void InitCache() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    factory_.SetCacheInfo(kDefaultClientId, cache_path());
  }

  ShaderCacheFactory* factory() { return &factory_; }

 private:
  void TearDown() override {
    factory_.RemoveCacheInfo(kDefaultClientId);

    // Run all pending tasks before destroying TaskEnvironment. Otherwise,
    // SimpleEntryImpl instances bound to pending tasks are destroyed in an
    // incorrect state (see |state_| DCHECK in ~SimpleEntryImpl).
    task_environment_.RunUntilIdle();
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  ShaderCacheFactory factory_;

  DISALLOW_COPY_AND_ASSIGN(ShaderDiskCacheTest);
};

TEST_F(ShaderDiskCacheTest, ClearsCache) {
  InitCache();

  scoped_refptr<ShaderDiskCache> cache = factory()->Get(kDefaultClientId);
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

TEST_F(ShaderDiskCacheTest, ClearByPathTriggersCallback) {
  InitCache();
  factory()->Get(kDefaultClientId)->Cache(kCacheKey, kCacheValue);
  net::TestCompletionCallback test_callback;
  factory()->ClearByPath(cache_path(), base::Time(), base::Time::Max(),
      base::BindLambdaForTesting([&]() { test_callback.callback().Run(1); } ));
  ASSERT_TRUE(test_callback.WaitForResult());
}

// Important for clearing in-memory profiles.
TEST_F(ShaderDiskCacheTest, ClearByPathWithEmptyPathTriggersCallback) {
  net::TestCompletionCallback test_callback;
  factory()->ClearByPath(base::FilePath(), base::Time(), base::Time::Max(),
      base::BindLambdaForTesting([&]() { test_callback.callback().Run(1); } ));
  ASSERT_TRUE(test_callback.WaitForResult());
}

// For https://crbug.com/663589.
TEST_F(ShaderDiskCacheTest, SafeToDeleteCacheMidEntryOpen) {
  InitCache();

  // Create a cache and wait for it to open.
  scoped_refptr<ShaderDiskCache> cache = factory()->Get(kDefaultClientId);
  ASSERT_TRUE(cache.get() != nullptr);
  net::TestCompletionCallback available_cb;
  int rv = cache->SetAvailableCallback(available_cb.callback());
  ASSERT_EQ(net::OK, available_cb.GetResult(rv));
  EXPECT_EQ(0, cache->Size());

  // Start writing an entry to the cache but delete it before the backend has
  // finished opening the entry. There is a race here, so this usually (but
  // not always) crashes if there is a problem.
  cache->Cache(kCacheKey, kCacheValue);
  cache = nullptr;

  // Open a new cache (to pass time on the cache thread) and verify all is
  // well.
  cache = factory()->Get(kDefaultClientId);
  ASSERT_TRUE(cache.get() != nullptr);
  net::TestCompletionCallback available_cb2;
  int rv2 = cache->SetAvailableCallback(available_cb2.callback());
  ASSERT_EQ(net::OK, available_cb2.GetResult(rv2));
}

TEST_F(ShaderDiskCacheTest, MultipleLoaderCallbacks) {
  InitCache();

  // Create a cache and wait for it to open.
  scoped_refptr<ShaderDiskCache> cache = factory()->Get(kDefaultClientId);
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
  cache = factory()->Get(kDefaultClientId);
  ASSERT_TRUE(cache.get() != nullptr);
  int loaded_calls = 0;
  cache->set_shader_loaded_callback(base::BindLambdaForTesting(
      [&loaded_calls](const std::string& key, const std::string& value) {
        ++loaded_calls;
      }));
  net::TestCompletionCallback available_cb2;
  int rv2 = cache->SetAvailableCallback(available_cb2.callback());
  ASSERT_EQ(net::OK, available_cb2.GetResult(rv2));
  EXPECT_EQ(count, loaded_calls);
}

}  // namespace gpu
