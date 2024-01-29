// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gr_shader_cache.h"

#include <thread>

#include "base/base64.h"
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

TEST_F(GrShaderCacheTest, MemoryPressure) {
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

  cache_.PurgeMemory(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
  EXPECT_EQ(cache_.num_cache_entries(), 0u);
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

}  // namespace raster
}  // namespace gpu
