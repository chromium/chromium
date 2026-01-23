// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gr_shader_cache.h"

#include <thread>

#include "base/base64.h"
#include "base/memory/memory_pressure_listener_registry.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "gpu/config/gpu_finch_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {
namespace raster {
namespace {
constexpr char kShaderKey[] = "key";
constexpr char kShader[] = "shader";
constexpr size_t kCacheLimit = 1024u;

}  // namespace

class GrShaderCacheTest : public GrShaderCache::Client, public testing::Test {
 public:
  GrShaderCacheTest() : cache_(kCacheLimit, this) {}

  void StoreShader(const std::string& key, const std::string& shader) override {
    disk_cache_[key] = shader;
  }

  base::MemoryPressureListenerRegistry memory_pressure_listener_registry_;
  base::test::TaskEnvironment task_environment_;

  GrShaderCache cache_;
  std::unordered_map<std::string, std::string> disk_cache_;
};

TEST_F(GrShaderCacheTest, DoesNotCacheForIncognito) {
  int32_t incognito_client_id = 2;
  auto key = SkData::MakeWithCString(kShaderKey);
  auto shader = SkData::MakeWithCString(kShader);
  {
    GrShaderCache::ScopedCacheUse cache_use(&cache_, incognito_client_id);
    EXPECT_EQ(cache_.load(*key), nullptr);
    cache_.store(*key, *shader);
  }
  EXPECT_EQ(disk_cache_.size(), 0u);

  int32_t regular_client_id = 3;
  cache_.CacheClientIdOnDisk(regular_client_id);
  {
    GrShaderCache::ScopedCacheUse cache_use(&cache_, regular_client_id);
    auto cached_shader = cache_.load(*key);
    ASSERT_TRUE(cached_shader);
    EXPECT_TRUE(cached_shader->equals(shader.get()));
  }
  EXPECT_EQ(disk_cache_.size(), 1u);

  {
    GrShaderCache::ScopedCacheUse cache_use(&cache_, regular_client_id);
    auto second_key = SkData::MakeWithCString("key2");
    EXPECT_EQ(cache_.load(*second_key), nullptr);
    cache_.store(*second_key, *shader);
  }
  EXPECT_EQ(disk_cache_.size(), 2u);
}

TEST_F(GrShaderCacheTest, LoadedFromDisk) {
  int32_t regular_client_id = 3;
  cache_.CacheClientIdOnDisk(regular_client_id);
  auto key = SkData::MakeWithCopy(kShaderKey, strlen(kShaderKey));
  auto shader = SkData::MakeWithCString(kShader);

  std::string key_str(static_cast<const char*>(key->data()), key->size());
  std::string shader_str(static_cast<const char*>(shader->data()),
                         shader->size());
  std::string encoded_key = base::Base64Encode(key_str);
  cache_.PopulateCache(encoded_key, shader_str);
  {
    GrShaderCache::ScopedCacheUse cache_use(&cache_, regular_client_id);
    auto cached_shader = cache_.load(*key);
    ASSERT_TRUE(cached_shader);
    EXPECT_TRUE(cached_shader->equals(shader.get()));
  }
  EXPECT_EQ(disk_cache_.size(), 0u);
}

TEST_F(GrShaderCacheTest, EnforcesLimits) {
  int32_t regular_client_id = 3;
  cache_.CacheClientIdOnDisk(regular_client_id);

  auto key = SkData::MakeWithCopy(kShaderKey, strlen(kShaderKey));
  auto shader = SkData::MakeUninitialized(kCacheLimit);
  {
    GrShaderCache::ScopedCacheUse cache_use(&cache_, regular_client_id);
    EXPECT_EQ(cache_.load(*key), nullptr);
    cache_.store(*key, *shader);
  }
  EXPECT_EQ(cache_.num_cache_entries(), 1u);

  {
    auto second_key = SkData::MakeWithCString("key2");
    GrShaderCache::ScopedCacheUse cache_use(&cache_, regular_client_id);
    EXPECT_EQ(cache_.load(*second_key), nullptr);
    cache_.store(*second_key, *shader);
  }
  EXPECT_EQ(cache_.num_cache_entries(), 1u);

  {
    auto third_key = SkData::MakeWithCString("key3");
    GrShaderCache::ScopedCacheUse cache_use(&cache_, regular_client_id);
    EXPECT_EQ(cache_.load(*third_key), nullptr);
    std::string key_str(static_cast<const char*>(third_key->data()),
                        third_key->size());
    std::string shader_str(static_cast<const char*>(shader->data()),
                           shader->size());
    cache_.PopulateCache(key_str, shader_str);
  }
  EXPECT_EQ(cache_.num_cache_entries(), 1u);
}

TEST_F(GrShaderCacheTest, StoringSameEntry) {
  int32_t regular_client_id = 3;
  cache_.CacheClientIdOnDisk(regular_client_id);

  auto key = SkData::MakeWithCopy(kShaderKey, strlen(kShaderKey));
  auto shader = SkData::MakeUninitialized(kCacheLimit);
  {
    GrShaderCache::ScopedCacheUse cache_use(&cache_, regular_client_id);
    EXPECT_EQ(cache_.load(*key), nullptr);
    cache_.store(*key, *shader);
  }
  EXPECT_EQ(cache_.num_cache_entries(), 1u);
  EXPECT_EQ(cache_.curr_size_bytes_for_testing(), shader->size());

  auto shader2 = SkData::MakeUninitialized(kCacheLimit / 2);
  {
    GrShaderCache::ScopedCacheUse cache_use(&cache_, regular_client_id);
    EXPECT_NE(cache_.load(*key), nullptr);
    cache_.store(*key, *shader2);
  }
  EXPECT_EQ(cache_.num_cache_entries(), 1u);
  EXPECT_EQ(cache_.curr_size_bytes_for_testing(), shader2->size());
}

TEST_F(GrShaderCacheTest, PopulateFromDiskAfterStoring) {
  int32_t regular_client_id = 3;
  cache_.CacheClientIdOnDisk(regular_client_id);

  auto key = SkData::MakeWithCopy(kShaderKey, strlen(kShaderKey));
  auto shader = SkData::MakeWithCString(kShader);
  {
    GrShaderCache::ScopedCacheUse cache_use(&cache_, regular_client_id);
    EXPECT_EQ(cache_.load(*key), nullptr);
    cache_.store(*key, *shader);
  }
  EXPECT_EQ(cache_.num_cache_entries(), 1u);
  EXPECT_EQ(cache_.curr_size_bytes_for_testing(), shader->size());

  // Try storing a different shader with the same key.
  std::string key_str(static_cast<const char*>(key->data()), key->size());
  std::string shader_str(static_cast<const char*>(shader->data()),
                         shader->size() / 2);
  cache_.PopulateCache(key_str, shader_str);
  {
    GrShaderCache::ScopedCacheUse cache_use(&cache_, regular_client_id);
    auto cached_shader = cache_.load(*key);
    ASSERT_TRUE(cached_shader);
    EXPECT_TRUE(cached_shader->equals(shader.get()));
  }
  EXPECT_EQ(disk_cache_.size(), 1u);
}

// This test creates GrShaderCache::ScopedCacheUse object from 2 different
// thread which exists together. In a non thread safe GrShaderCache, this will
// hit DCHECKS in ScopedCacheUse::ScopedCacheUse() since the current_client_id
// already exists from 1st object. In a thread safe model, it will not hit
// DCHECKS.
TEST_F(GrShaderCacheTest, MultipleThreadsUsingSameCache) {
  int32_t regular_client_id = 3;
  int32_t new_client_id = 4;
  cache_.CacheClientIdOnDisk(regular_client_id);

  auto key = SkData::MakeWithCopy(kShaderKey, strlen(kShaderKey));
  auto shader = SkData::MakeWithCString(kShader);

  GrShaderCache::ScopedCacheUse cache_use1(&cache_, regular_client_id);
  EXPECT_EQ(cache_.load(*key), nullptr);
  cache_.store(*key, *shader);

  EXPECT_EQ(cache_.num_cache_entries(), 1u);
  EXPECT_EQ(cache_.curr_size_bytes_for_testing(), shader->size());

  // Different client id to use cache on a different thread.
  std::thread second_thread([&]() {
    auto key2 = SkData::MakeWithCString("key2");
    GrShaderCache::ScopedCacheUse cache_use2(&cache_, new_client_id);
    EXPECT_EQ(cache_.load(*key2), nullptr);

    // Store same shader on a different key.
    cache_.store(*key2, *shader);
  });
  second_thread.join();

  EXPECT_EQ(cache_.num_cache_entries(), 2u);
  EXPECT_EQ(cache_.curr_size_bytes_for_testing(), 2 * shader->size());
}

TEST_F(GrShaderCacheTest, MemoryPressure) {
  int32_t regular_client_id = 3;
  cache_.CacheClientIdOnDisk(regular_client_id);

  // Fill the cache to its limit.
  const size_t entry_size = kCacheLimit / 4;
  auto shader = SkData::MakeUninitialized(entry_size);
  {
    GrShaderCache::ScopedCacheUse cache_use(&cache_, regular_client_id);
    for (int i = 0; i < 4; ++i) {
      auto key =
          SkData::MakeWithCString(base::StringPrintf("key%d", i).c_str());
      cache_.store(*key, *shader);
    }
  }
  EXPECT_EQ(cache_.num_cache_entries(), 4u);
  EXPECT_EQ(cache_.curr_size_bytes_for_testing(), kCacheLimit);

  // Trigger moderate memory pressure.
  base::MemoryPressureListenerRegistry::SimulatePressureNotificationAsync(
      base::MEMORY_PRESSURE_LEVEL_MODERATE, task_environment_.QuitClosure());
  task_environment_.RunUntilQuit();

  // Moderate memory pressure reduces limit by 4x.
  // New limit: kCacheLimit / 4 = 256.
  // Since each entry is 256, only 1 entry should remain.
  EXPECT_EQ(cache_.num_cache_entries(), 1u);
  EXPECT_EQ(cache_.curr_size_bytes_for_testing(), entry_size);

  // Verify that the limit is still enforced for new stores.
  {
    GrShaderCache::ScopedCacheUse cache_use(&cache_, regular_client_id);
    auto key = SkData::MakeWithCString("new_key");
    cache_.store(*key, *shader);
  }
  EXPECT_EQ(cache_.num_cache_entries(), 1u);

  // Trigger critical memory pressure.
  base::MemoryPressureListenerRegistry::SimulatePressureNotificationAsync(
      base::MEMORY_PRESSURE_LEVEL_CRITICAL, task_environment_.QuitClosure());
  task_environment_.RunUntilQuit();

  // Critical memory pressure sets limit to 0.
  EXPECT_EQ(cache_.num_cache_entries(), 0u);
  EXPECT_EQ(cache_.curr_size_bytes_for_testing(), 0u);

  // Verify that the limit is still enforced for new stores.
  {
    GrShaderCache::ScopedCacheUse cache_use(&cache_, regular_client_id);
    auto key = SkData::MakeWithCString("new_key_critical");
    cache_.store(*key, *shader);
  }
  EXPECT_EQ(cache_.num_cache_entries(), 0u);

  // Restore memory pressure to none.
  base::MemoryPressureListenerRegistry::SimulatePressureNotificationAsync(
      base::MEMORY_PRESSURE_LEVEL_NONE, task_environment_.QuitClosure());
  task_environment_.RunUntilQuit();

  // Limit should be restored to kCacheLimit.
  // We can now store more entries.
  {
    GrShaderCache::ScopedCacheUse cache_use(&cache_, regular_client_id);
    for (int i = 0; i < 4; ++i) {
      auto key = SkData::MakeWithCString(
          base::StringPrintf("restore_key%d", i).c_str());
      cache_.store(*key, *shader);
    }
  }
  EXPECT_EQ(cache_.num_cache_entries(), 4u);
  EXPECT_EQ(cache_.curr_size_bytes_for_testing(), kCacheLimit);
}

}  // namespace raster
}  // namespace gpu
