// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/390223051): Remove C-library calls to fix the errors.
#pragma allow_unsafe_libc_calls
#endif

#include "gpu/command_buffer/service/dawn_caching_interface.h"

#include <string>
#include <string_view>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_feature_list.h"
#include "components/persistent_cache/backend_params.h"
#include "components/persistent_cache/sqlite/vfs/sandboxed_file.h"
#include "gpu/command_buffer/service/gpu_persistent_cache.h"
#include "gpu/command_buffer/service/mocks.h"
#include "gpu/config/gpu_finch_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu::webgpu {
namespace {

using ::testing::StrictMock;

class DawnCachingInterfaceTest : public testing::Test {
 protected:
  static constexpr std::string_view kKey = "cache key";
  static constexpr std::string_view kData = "some data";
  static constexpr size_t kKeySize = kKey.size();
  static constexpr size_t kDataSize = kData.size();
  static constexpr gpu::GpuDiskCacheDawnWebGPUHandle kDawnWebGPUHandle =
      gpu::GpuDiskCacheDawnWebGPUHandle(1);
  static constexpr gpu::GpuDiskCacheDawnGraphiteHandle kDawnGraphiteHandle =
      gpu::GpuDiskCacheDawnGraphiteHandle(2);

  DawnCachingInterfaceFactory factory_;
  gpu::GpuDiskCacheHandle handle_ = kDawnWebGPUHandle;
  StrictMock<MockDecoderClient> decoder_client_mock_;
};

TEST_F(DawnCachingInterfaceTest, LoadNonexistentSize) {
  auto dawn_caching_interface = factory_.CreateInstance(handle_);
  EXPECT_EQ(
      0u, dawn_caching_interface->LoadData(kKey.data(), kKeySize, nullptr, 0));
}

TEST_F(DawnCachingInterfaceTest, StoreThenLoadSameInterface) {
  auto dawn_caching_interface = factory_.CreateInstance(handle_);
  dawn_caching_interface->StoreData(kKey.data(), kKeySize, kData.data(),
                                    kDataSize);

  char buffer[kDataSize];
  EXPECT_EQ(kDataSize, dawn_caching_interface->LoadData(kKey.data(), kKeySize,
                                                        nullptr, 0));
  EXPECT_EQ(kDataSize, dawn_caching_interface->LoadData(kKey.data(), kKeySize,
                                                        buffer, kDataSize));
  EXPECT_EQ(0, memcmp(buffer, kData.data(), kDataSize));
}

TEST_F(DawnCachingInterfaceTest, StoreThenLoadSameHandle) {
  auto store_interface = factory_.CreateInstance(handle_);
  store_interface->StoreData(kKey.data(), kKeySize, kData.data(), kDataSize);

  auto load_interface = factory_.CreateInstance(handle_);
  char buffer[kDataSize];
  EXPECT_EQ(kDataSize,
            load_interface->LoadData(kKey.data(), kKeySize, nullptr, 0));
  EXPECT_EQ(kDataSize,
            load_interface->LoadData(kKey.data(), kKeySize, buffer, kDataSize));
  EXPECT_EQ(0, memcmp(buffer, kData.data(), kDataSize));
}

TEST_F(DawnCachingInterfaceTest, StoreDestroyThenLoadSameHandle) {
  auto store_interface = factory_.CreateInstance(handle_);
  store_interface->StoreData(kKey.data(), kKeySize, kData.data(), kDataSize);
  store_interface.reset();

  auto load_interface = factory_.CreateInstance(handle_);
  char buffer[kDataSize];
  EXPECT_EQ(kDataSize,
            load_interface->LoadData(kKey.data(), kKeySize, nullptr, 0));
  EXPECT_EQ(kDataSize,
            load_interface->LoadData(kKey.data(), kKeySize, buffer, kDataSize));
  EXPECT_EQ(0, memcmp(buffer, kData.data(), kDataSize));
}

// If the handle is released before a new cache is created, the new cache should
// use a new in-memory cache.
TEST_F(DawnCachingInterfaceTest, StoreReleaseThenLoad) {
  auto store_interface = factory_.CreateInstance(handle_);
  store_interface->StoreData(kKey.data(), kKeySize, kData.data(), kDataSize);
  store_interface.reset();
  factory_.ReleaseHandle(handle_);

  auto load_interface = factory_.CreateInstance(handle_);
  EXPECT_EQ(0u, load_interface->LoadData(kKey.data(), kKeySize, nullptr, 0));
}

TEST_F(DawnCachingInterfaceTest, IncognitoCachesDoNotShare) {
  auto interface_1 = factory_.CreateInstance();
  interface_1->StoreData(kKey.data(), kKeySize, kData.data(), kDataSize);

  auto interface_2 = factory_.CreateInstance();
  EXPECT_EQ(0u, interface_2->LoadData(kKey.data(), kKeySize, nullptr, 0));
}

TEST_F(DawnCachingInterfaceTest, UnableToCreateBackend) {
  // This factory mimics what happens when we are unable to create a backend.
  DawnCachingInterfaceFactory factory(base::BindRepeating(
      []() -> scoped_refptr<detail::DawnMemoryCache> { return nullptr; }));

  // Without an actual backend, all loads and stores should do nothing.
  {
    auto incongnito_interface = factory.CreateInstance();
    incongnito_interface->StoreData(kKey.data(), kKeySize, kData.data(),
                                    kDataSize);
    EXPECT_EQ(
        0u, incongnito_interface->LoadData(kKey.data(), kKeySize, nullptr, 0));
  }
  {
    auto handle_interface = factory.CreateInstance(handle_);
    handle_interface->StoreData(kKey.data(), kKeySize, kData.data(), kDataSize);
    EXPECT_EQ(0u,
              handle_interface->LoadData(kKey.data(), kKeySize, nullptr, 0));
  }
}

TEST_F(DawnCachingInterfaceTest, StoreTriggersHostSide) {
  auto dawn_caching_interface = factory_.CreateInstance(
      handle_, base::BindRepeating(&MockDecoderClient::CacheBlob,
                                   base::Unretained(&decoder_client_mock_),
                                   gpu::GpuDiskCacheType::kDawnWebGPU));

  EXPECT_CALL(decoder_client_mock_,
              CacheBlob(gpu::GpuDiskCacheType::kDawnWebGPU, std::string(kKey),
                        std::string(kData)));
  dawn_caching_interface->StoreData(kKey.data(), kKeySize, kData.data(),
                                    kDataSize);
}

TEST_F(DawnCachingInterfaceTest, TestMaxSizeEviction) {
  // Verifies that a cache size that should only fit one entry will only keep
  // one entry in memory.
  static constexpr std::string_view kKey1 = "1";
  static constexpr std::string_view kData1 = "1";
  static constexpr std::string_view kKey2 = "2";
  static constexpr std::string_view kData2 = "2";
  static_assert(kKey1.size() == kKey2.size());
  static_assert(kData1.size() == kData2.size());
  static constexpr size_t kKeySize = kKey1.size();
  static constexpr size_t kDataSize = kData1.size();
  static constexpr size_t kCacheSize = 2u * kKeySize + 2u * kDataSize - 1u;

  DawnCachingInterfaceFactory factory(base::BindRepeating([]() {
    return base::MakeRefCounted<detail::DawnMemoryCache>(kCacheSize);
  }));

  auto interface = factory.CreateInstance();
  interface->StoreData(kKey1.data(), kKeySize, kData1.data(), kDataSize);
  interface->StoreData(kKey2.data(), kKeySize, kData2.data(), kDataSize);

  EXPECT_EQ(0u, interface->LoadData(kKey1.data(), 1u, nullptr, 0));
  EXPECT_EQ(kDataSize, interface->LoadData(kKey2.data(), 1u, nullptr, 0));
}

TEST_F(DawnCachingInterfaceTest, TestLruEviction) {
  // Verifies that a cache size that should only fit two entries evicts the
  // proper entry when a third one is stored.
  static constexpr std::string_view kKey1 = "1";
  static constexpr std::string_view kData1 = "1";
  static constexpr std::string_view kKey2 = "2";
  static constexpr std::string_view kData2 = "2";
  static constexpr std::string_view kKey3 = "3";
  static constexpr std::string_view kData3 = "3";
  static_assert(kKey1.size() == kKey2.size());
  static_assert(kKey2.size() == kKey3.size());
  static_assert(kData1.size() == kData2.size());
  static_assert(kData2.size() == kData3.size());
  static constexpr size_t kKeySize = kKey1.size();
  static constexpr size_t kDataSize = kData1.size();
  static constexpr size_t kCacheSize = 3u * kKeySize + 3u * kDataSize - 1u;

  DawnCachingInterfaceFactory factory(base::BindRepeating([]() {
    return base::MakeRefCounted<detail::DawnMemoryCache>(kCacheSize);
  }));

  // Even though Key1 was stored first, because we loaded it once, Key2 should
  // be the one to be evicted when Key3 is added.
  auto interface = factory.CreateInstance();
  interface->StoreData(kKey1.data(), kKeySize, kData1.data(), kDataSize);
  interface->StoreData(kKey2.data(), kKeySize, kData2.data(), kDataSize);
  EXPECT_EQ(kDataSize, interface->LoadData(kKey1.data(), 1u, nullptr, 0));
  interface->StoreData(kKey3.data(), kKeySize, kData3.data(), kDataSize);

  EXPECT_EQ(kDataSize, interface->LoadData(kKey1.data(), 1u, nullptr, 0));
  EXPECT_EQ(0u, interface->LoadData(kKey2.data(), 1u, nullptr, 0));
  EXPECT_EQ(kDataSize, interface->LoadData(kKey3.data(), 1u, nullptr, 0));
}

// Entries that are too large for the size of the cache are not cached and do
// not cause any crashes. This is a regression test for dawn:2034.
TEST_F(DawnCachingInterfaceTest, TestVeryLargeEntrySize) {
  static constexpr std::string_view kSmall = "1";
  static constexpr std::string_view kLarge = "11111";
  static constexpr size_t kSmallSize = kSmall.size();
  static constexpr size_t kLargeSize = kLarge.size();
  static constexpr size_t kCacheSize = kLargeSize - 1u;

  DawnCachingInterfaceFactory factory(base::BindRepeating([]() {
    return base::MakeRefCounted<detail::DawnMemoryCache>(kCacheSize);
  }));
  auto interface = factory.CreateInstance();

  {
    // When the key is larger than the cache size but the value is not, caching
    // fails.
    interface->StoreData(kLarge.data(), kLargeSize, kSmall.data(), kSmallSize);
    EXPECT_EQ(0u, interface->LoadData(kLarge.data(), kLargeSize, nullptr, 0));
  }
  {
    // When the key is smaller than the cache size, but the value is not,
    // caching fails.
    interface->StoreData(kSmall.data(), kSmallSize, kLarge.data(), kLargeSize);
    EXPECT_EQ(0u, interface->LoadData(kSmall.data(), kSmallSize, nullptr, 0));
  }
  {
    // When the both the key and the value is larger than the cache size,
    // caching fails.
    interface->StoreData(kLarge.data(), kLargeSize, kLarge.data(), kLargeSize);
    EXPECT_EQ(0u, interface->LoadData(kLarge.data(), kLargeSize, nullptr, 0));
  }
}

TEST_F(DawnCachingInterfaceTest, TestMemoryPressureCritical) {
  // Verifies that on PurgeMemory the cache becomes empty for critical pressure
  // levels without `kAggressiveShaderCacheLimits` feature flag.
  static constexpr std::string_view kKey1 = "1";
  static constexpr std::string_view kData1 = "1";
  static constexpr size_t kKeySize = kKey1.size();
  static constexpr size_t kDataSize = kData1.size();
  static constexpr size_t kCacheSize = 2u * kKeySize + 2u * kDataSize - 1u;

  DawnCachingInterfaceFactory factory(base::BindRepeating([]() {
    return base::MakeRefCounted<detail::DawnMemoryCache>(kCacheSize);
  }));

  // Pass handles here so that the backends_ are populated.
  auto interfaces = {factory.CreateInstance(kDawnGraphiteHandle),
                     factory.CreateInstance(kDawnWebGPUHandle)};
  for (auto& interface : interfaces) {
    interface->StoreData(kKey1.data(), kKeySize, kData1.data(), kDataSize);
    EXPECT_EQ(kDataSize, interface->LoadData(kKey1.data(), 1u, nullptr, 0));

    factory.PurgeMemory(base::MEMORY_PRESSURE_LEVEL_CRITICAL);
    EXPECT_EQ(0u, interface->LoadData(kKey1.data(), 1u, nullptr, 0));
  }
}

TEST_F(DawnCachingInterfaceTest, TestAggressiveCacheAndMemoryPressure) {
  // Verifies PurgeMemory with `kAggressiveShaderCacheLimits` feature flag.
  base::test::ScopedFeatureList feature_list{
      ::features::kAggressiveShaderCacheLimits};
  static constexpr std::string_view kKey1 = "1";
  static constexpr std::string_view kData1 = "1";
  static constexpr size_t kKeySize = kKey1.size();
  static constexpr size_t kDataSize = kData1.size();
  static constexpr size_t kCacheSize = 2u * kKeySize + 2u * kDataSize - 1u;

  DawnCachingInterfaceFactory factory(base::BindRepeating([]() {
    return base::MakeRefCounted<detail::DawnMemoryCache>(kCacheSize);
  }));

  // Pass handles here so that the backends_ are populated.
  auto interfaces = {factory.CreateInstance(kDawnGraphiteHandle),
                     factory.CreateInstance(kDawnWebGPUHandle)};
  for (auto& interface : interfaces) {
    interface->StoreData(kKey1.data(), kKeySize, kData1.data(), kDataSize);
    EXPECT_EQ(kDataSize, interface->LoadData(kKey1.data(), 1u, nullptr, 0));

    // Moderate memory pressure is ignored
    factory.PurgeMemory(base::MEMORY_PRESSURE_LEVEL_MODERATE);
    EXPECT_EQ(kDataSize, interface->LoadData(kKey1.data(), 1u, nullptr, 0));

    // But not critical, except on Android
    factory.PurgeMemory(base::MEMORY_PRESSURE_LEVEL_CRITICAL);
#if BUILDFLAG(IS_ANDROID)
    EXPECT_EQ(kDataSize, interface->LoadData(kKey1.data(), 1u, nullptr, 0));
#else
    EXPECT_EQ(0u, interface->LoadData(kKey1.data(), 1u, nullptr, 0));
#endif
  }
}

// Verifies that data stored in a persistent cache can be loaded back.
// TODO: crbug.com/450470858 - Reenable the test after failure is fixed.
TEST_F(DawnCachingInterfaceTest, DISABLED_StoreAndLoadWithPersistentCache) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  auto shared_lock = base::UnsafeSharedMemoryRegion::Create(
      sizeof(persistent_cache::LockState));
  ASSERT_TRUE(shared_lock.IsValid());
  auto OpenPersistentCache =
      [&temp_dir, &shared_lock]() -> std::unique_ptr<GpuPersistentCache> {
    auto db_path = temp_dir.GetPath().AppendASCII("test.db");
    auto journal_path = temp_dir.GetPath().AppendASCII("test.journal");

    persistent_cache::BackendParams params;
    params.type = persistent_cache::BackendType::kSqlite;
    params.db_file =
        base::File(db_path, base::File::FLAG_OPEN_ALWAYS |
                                base::File::FLAG_READ | base::File::FLAG_WRITE);
    params.journal_file = base::File(
        journal_path, base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_READ |
                          base::File::FLAG_WRITE);
    params.shared_lock = shared_lock.Duplicate();
    CHECK(params.db_file.IsValid());
    CHECK(params.journal_file.IsValid());

    auto persistent_cache = std::make_unique<GpuPersistentCache>("Test");
    persistent_cache->InitializeCache(std::move(params));
    return persistent_cache;
  };

  // Store data to the persistent cache via store interface.
  {
    scoped_refptr<detail::DawnMemoryCache> memory_cache =
        base::MakeRefCounted<detail::DawnMemoryCache>(1024);
    DawnCachingInterfaceFactory store_factory(base::BindRepeating(
        [](scoped_refptr<detail::DawnMemoryCache> cache) { return cache; },
        memory_cache));
    auto store_interface =
        store_factory.CreateInstance(handle_, OpenPersistentCache());
    store_interface->StoreData(kKey.data(), kKeySize, kData.data(), kDataSize);

    // Check that the entry exists in the memory cache.
    char buffer[kDataSize];
    EXPECT_EQ(kDataSize, memory_cache->LoadData(std::string(kKey), nullptr, 0));
    EXPECT_EQ(kDataSize,
              memory_cache->LoadData(std::string(kKey), buffer, kDataSize));
    EXPECT_EQ(0, memcmp(buffer, kData.data(), kDataSize));
  }

  // Use the same persistent cache but with different memory cache.
  {
    scoped_refptr<detail::DawnMemoryCache> memory_cache2 =
        base::MakeRefCounted<detail::DawnMemoryCache>(1024);
    DawnCachingInterfaceFactory load_factory(base::BindRepeating(
        [](scoped_refptr<detail::DawnMemoryCache> cache) { return cache; },
        memory_cache2));
    auto load_interface =
        load_factory.CreateInstance(handle_, OpenPersistentCache());

    EXPECT_EQ(0u, memory_cache2->LoadData(std::string(kKey), nullptr, 0));

    // Verify that we can query the existing entry.
    char buffer[kDataSize];
    EXPECT_EQ(kDataSize,
              load_interface->LoadData(kKey.data(), kKeySize, nullptr, 0));
    EXPECT_EQ(kDataSize, load_interface->LoadData(kKey.data(), kKeySize, buffer,
                                                  kDataSize));
    EXPECT_EQ(0, memcmp(buffer, kData.data(), kDataSize));

    // Check that the memory cache now contains the same entry after the
    // LoadData() call above
    EXPECT_EQ(kDataSize,
              memory_cache2->LoadData(std::string(kKey), nullptr, 0));
  }
}

}  // namespace
}  // namespace gpu::webgpu
