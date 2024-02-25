// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/dawn_caching_interface.h"

#include <string>
#include <string_view>

#include "gpu/command_buffer/service/mocks.h"
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
  static constexpr gpu::GpuDiskCacheDawnWebGPUHandle kDawnHandle =
      gpu::GpuDiskCacheDawnWebGPUHandle(1);

  DawnCachingInterfaceFactory factory_;
  gpu::GpuDiskCacheHandle handle_ = kDawnHandle;
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
      []() -> scoped_refptr<detail::DawnCachingBackend> { return nullptr; }));

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
                                   base::Unretained(&decoder_client_mock_)));

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
    return base::MakeRefCounted<detail::DawnCachingBackend>(kCacheSize);
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
    return base::MakeRefCounted<detail::DawnCachingBackend>(kCacheSize);
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
    return base::MakeRefCounted<detail::DawnCachingBackend>(kCacheSize);
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

}  // namespace
}  // namespace gpu::webgpu
