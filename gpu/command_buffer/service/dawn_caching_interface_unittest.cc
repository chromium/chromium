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
  DawnCachingInterfaceFactory factory(base::BindRepeating([]() {
    return base::MakeRefCounted<RefCountedDiskCacheBackend>(nullptr);
  }));

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
  auto dawn_caching_interface =
      factory_.CreateInstance(handle_, &decoder_client_mock_);

  EXPECT_CALL(decoder_client_mock_,
              CacheBlob(gpu::GpuDiskCacheType::kDawnWebGPU, std::string(kKey),
                        std::string(kData)));
  dawn_caching_interface->StoreData(kKey.data(), kKeySize, kData.data(),
                                    kDataSize);
}

}  // namespace
}  // namespace gpu::webgpu
