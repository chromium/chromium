// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/base/decoder_buffer.h"

#include <stdint.h>
#include <string.h>

#include <memory>

#include "base/containers/heap_array.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "media/base/test_data_util.h"
#include "media/base/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(DecoderBufferTest, Constructors) {
  scoped_refptr<DecoderBuffer> buffer(new DecoderBuffer(0));
  EXPECT_FALSE(buffer->data());
  EXPECT_EQ(0u, buffer->size());
  EXPECT_TRUE(buffer->empty());
  EXPECT_EQ(base::span(*buffer), base::span<const uint8_t>());
  EXPECT_FALSE(buffer->end_of_stream());
  EXPECT_FALSE(buffer->is_key_frame());

  const size_t kTestSize = 10;
  scoped_refptr<DecoderBuffer> buffer3(new DecoderBuffer(kTestSize));
  ASSERT_TRUE(buffer3.get());
  EXPECT_EQ(kTestSize, buffer3->size());
  EXPECT_FALSE(buffer3->empty());
}

TEST(DecoderBufferTest, CreateEOSBuffer) {
  auto buffer = DecoderBuffer::CreateEOSBuffer();
  EXPECT_TRUE(buffer->end_of_stream());
  EXPECT_FALSE(buffer->next_config());

  buffer = DecoderBuffer::CreateEOSBuffer(TestAudioConfig::Normal());
  EXPECT_TRUE(buffer->end_of_stream());
  ASSERT_TRUE(buffer->next_config());
  {
    auto config = buffer->next_config().value();
    auto* ac = absl::get_if<AudioDecoderConfig>(&config);
    ASSERT_TRUE(ac);
    EXPECT_TRUE(ac->Matches(TestAudioConfig::Normal()));
  }

  buffer = DecoderBuffer::CreateEOSBuffer(TestVideoConfig::Normal());
  EXPECT_TRUE(buffer->end_of_stream());
  ASSERT_TRUE(buffer->next_config());
  {
    auto config = buffer->next_config().value();
    auto* vc = absl::get_if<VideoDecoderConfig>(&config);
    ASSERT_TRUE(vc);
    EXPECT_TRUE(vc->Matches(TestVideoConfig::Normal()));
  }
}

TEST(DecoderBufferTest, CopyFrom) {
  const uint8_t kData[] = "hello";
  const size_t kDataSize = std::size(kData);

  scoped_refptr<DecoderBuffer> buffer2(DecoderBuffer::CopyFrom(kData));
  ASSERT_TRUE(buffer2.get());
  EXPECT_NE(kData, buffer2->data());
  EXPECT_EQ(buffer2->size(), kDataSize);
  EXPECT_EQ(base::span(*buffer2), base::span(kData));
  EXPECT_FALSE(buffer2->end_of_stream());
  EXPECT_FALSE(buffer2->is_key_frame());
}

TEST(DecoderBufferTest, FromArray) {
  const uint8_t kData[] = "hello";
  const size_t kDataSize = std::size(kData);
  auto ptr = base::HeapArray<uint8_t>::CopiedFrom(kData);
  auto buffer = DecoderBuffer::FromArray(std::move(ptr));
  ASSERT_TRUE(buffer.get());
  EXPECT_EQ(buffer->size(), kDataSize);
  EXPECT_EQ(base::span(*buffer), base::span(kData));
  EXPECT_FALSE(buffer->end_of_stream());
  EXPECT_FALSE(buffer->is_key_frame());
}

TEST(DecoderBufferTest, FromPlatformSharedMemoryRegion) {
  const uint8_t kData[] = "hello";
  const size_t kDataSize = std::size(kData);

  auto region = base::UnsafeSharedMemoryRegion::Create(kDataSize);
  auto mapping = region.Map();
  ASSERT_TRUE(mapping.IsValid());
  memcpy(mapping.GetMemoryAs<uint8_t>(), kData, kDataSize);

  scoped_refptr<DecoderBuffer> buffer(
      DecoderBuffer::FromSharedMemoryRegion(std::move(region), 0, kDataSize));
  ASSERT_TRUE(buffer.get());
  EXPECT_EQ(buffer->size(), kDataSize);
  EXPECT_EQ(base::span(*buffer), base::span(kData));
  EXPECT_FALSE(buffer->end_of_stream());
  EXPECT_FALSE(buffer->is_key_frame());
}

TEST(DecoderBufferTest, FromPlatformSharedMemoryRegion_Unaligned) {
  const uint8_t kData[] = "XXXhello";
  const size_t kDataSize = std::size(kData);
  const off_t kDataOffset = 3;

  auto region = base::UnsafeSharedMemoryRegion::Create(kDataSize);
  auto mapping = region.Map();
  ASSERT_TRUE(mapping.IsValid());
  memcpy(mapping.GetMemoryAs<uint8_t>(), kData, kDataSize);

  scoped_refptr<DecoderBuffer> buffer(DecoderBuffer::FromSharedMemoryRegion(
      std::move(region), kDataOffset, kDataSize - kDataOffset));
  ASSERT_TRUE(buffer.get());
  EXPECT_EQ(buffer->size(), kDataSize - kDataOffset);
  EXPECT_EQ(base::span(*buffer), base::span(kData).subspan(kDataOffset));
  EXPECT_FALSE(buffer->end_of_stream());
  EXPECT_FALSE(buffer->is_key_frame());
}

TEST(DecoderBufferTest, FromPlatformSharedMemoryRegion_ZeroSize) {
  const uint8_t kData[] = "hello";
  const size_t kDataSize = std::size(kData);

  auto region = base::UnsafeSharedMemoryRegion::Create(kDataSize);
  auto mapping = region.Map();
  ASSERT_TRUE(mapping.IsValid());
  memcpy(mapping.memory(), kData, kDataSize);

  scoped_refptr<DecoderBuffer> buffer(
      DecoderBuffer::FromSharedMemoryRegion(std::move(region), 0, 0));
  ASSERT_FALSE(buffer.get());
}

TEST(DecoderBufferTest, FromSharedMemoryRegion) {
  const uint8_t kData[] = "hello";
  const size_t kDataSize = std::size(kData);

  auto mapping_region = base::ReadOnlySharedMemoryRegion::Create(kDataSize);
  ASSERT_TRUE(mapping_region.IsValid());
  memcpy(mapping_region.mapping.GetMemoryAs<uint8_t>(), kData, kDataSize);

  scoped_refptr<DecoderBuffer> buffer(DecoderBuffer::FromSharedMemoryRegion(
      std::move(mapping_region.region), 0, kDataSize));
  ASSERT_TRUE(buffer.get());
  EXPECT_EQ(buffer->size(), kDataSize);
  EXPECT_EQ(base::span(*buffer), base::span(kData));
  EXPECT_FALSE(buffer->end_of_stream());
  EXPECT_FALSE(buffer->is_key_frame());
}

TEST(DecoderBufferTest, FromSharedMemoryRegion_Unaligned) {
  const uint8_t kData[] = "XXXhello";
  const size_t kDataSize = std::size(kData);
  const off_t kDataOffset = 3;

  auto mapping_region = base::ReadOnlySharedMemoryRegion::Create(kDataSize);
  ASSERT_TRUE(mapping_region.IsValid());
  memcpy(mapping_region.mapping.GetMemoryAs<uint8_t>(), kData, kDataSize);

  scoped_refptr<DecoderBuffer> buffer(DecoderBuffer::FromSharedMemoryRegion(
      std::move(mapping_region.region), kDataOffset, kDataSize - kDataOffset));

  ASSERT_TRUE(buffer.get());
  EXPECT_EQ(buffer->size(), kDataSize - kDataOffset);
  EXPECT_EQ(base::span(*buffer), base::span(kData).subspan(kDataOffset));
  EXPECT_FALSE(buffer->end_of_stream());
  EXPECT_FALSE(buffer->is_key_frame());
}

TEST(DecoderBufferTest, FromSharedMemoryRegion_ZeroSize) {
  const uint8_t kData[] = "hello";
  const size_t kDataSize = std::size(kData);

  auto mapping_region = base::ReadOnlySharedMemoryRegion::Create(kDataSize);
  memcpy(mapping_region.mapping.GetMemoryAs<uint8_t>(), kData, kDataSize);

  scoped_refptr<DecoderBuffer> buffer(DecoderBuffer::FromSharedMemoryRegion(
      std::move(mapping_region.region), 0, 0));

  ASSERT_FALSE(buffer.get());
}

TEST(DecoderBufferTest, FromExternalMemory) {
  constexpr uint8_t kData[] = "hello";
  constexpr size_t kDataSize = std::size(kData);

  auto external_memory = std::make_unique<ExternalMemoryAdapterForTesting>(
      base::make_span(kData, kDataSize));
  auto buffer = DecoderBuffer::FromExternalMemory(std::move(external_memory));
  ASSERT_TRUE(buffer.get());
  EXPECT_EQ(buffer->size(), kDataSize);
  EXPECT_EQ(base::span(*buffer), base::span(kData));
  EXPECT_FALSE(buffer->end_of_stream());
  EXPECT_FALSE(buffer->is_key_frame());
}

TEST(DecoderBufferTest, ReadingWriting) {
  const uint8_t kData[] = "hello";
  const size_t kDataSize = std::size(kData);

  scoped_refptr<DecoderBuffer> buffer(new DecoderBuffer(kDataSize));
  ASSERT_TRUE(buffer.get());

  uint8_t* data = buffer->writable_data();
  ASSERT_TRUE(data);
  ASSERT_EQ(kDataSize, buffer->size());
  base::span(data, buffer->size()).copy_from(kData);
  const uint8_t* read_only_data = buffer->data();
  ASSERT_EQ(data, read_only_data);
  EXPECT_EQ(base::span(*buffer), base::span(kData));
  EXPECT_FALSE(buffer->end_of_stream());
}

TEST(DecoderBufferTest, DecryptConfig) {
  scoped_refptr<DecoderBuffer> buffer(new DecoderBuffer(0));
  EXPECT_FALSE(buffer->decrypt_config());

  const char kKeyId[] = "key id";
  const char kIv[] = "0123456789abcdef";
  std::vector<SubsampleEntry> subsamples;
  subsamples.push_back(SubsampleEntry(10, 5));
  subsamples.push_back(SubsampleEntry(15, 7));

  std::unique_ptr<DecryptConfig> decrypt_config =
      DecryptConfig::CreateCencConfig(kKeyId, kIv, subsamples);

  buffer->set_decrypt_config(
      DecryptConfig::CreateCencConfig(kKeyId, kIv, subsamples));

  EXPECT_TRUE(buffer->decrypt_config());
  EXPECT_TRUE(buffer->decrypt_config()->Matches(*decrypt_config));
}

TEST(DecoderBufferTest, IsKeyFrame) {
  scoped_refptr<DecoderBuffer> buffer(new DecoderBuffer(0));
  EXPECT_FALSE(buffer->is_key_frame());

  buffer->set_is_key_frame(false);
  EXPECT_FALSE(buffer->is_key_frame());

  buffer->set_is_key_frame(true);
  EXPECT_TRUE(buffer->is_key_frame());
}

TEST(DecoderBufferTest, SideData) {
  scoped_refptr<DecoderBuffer> buffer(new DecoderBuffer(0));
  EXPECT_FALSE(buffer->has_side_data());

  constexpr uint64_t kSecureHandle = 42;
  const std::vector<uint32_t> kSpatialLayers = {1, 2, 3};
  const std::vector<uint8_t> kAlphaData = {9, 8, 7};

  buffer->WritableSideData().secure_handle = kSecureHandle;
  buffer->WritableSideData().spatial_layers = kSpatialLayers;
  buffer->WritableSideData().alpha_data = kAlphaData;
  EXPECT_TRUE(buffer->has_side_data());
  EXPECT_EQ(buffer->side_data()->secure_handle, kSecureHandle);
  EXPECT_EQ(buffer->side_data()->spatial_layers, kSpatialLayers);
  EXPECT_EQ(buffer->side_data()->alpha_data, kAlphaData);

  buffer->set_side_data(std::nullopt);
  EXPECT_FALSE(buffer->has_side_data());
}

TEST(DecoderBufferTest, IsEncrypted) {
  scoped_refptr<DecoderBuffer> buffer(new DecoderBuffer(0));
  EXPECT_FALSE(buffer->is_encrypted());

  const char kKeyId[] = "key id";
  const char kIv[] = "0123456789abcdef";
  std::vector<SubsampleEntry> subsamples;
  subsamples.emplace_back(10, 5);
  subsamples.emplace_back(15, 7);

  std::unique_ptr<DecryptConfig> decrypt_config =
      DecryptConfig::CreateCencConfig(kKeyId, kIv, subsamples);

  buffer->set_decrypt_config(
      DecryptConfig::CreateCencConfig(kKeyId, kIv, subsamples));

  EXPECT_TRUE(buffer->is_encrypted());
}

}  // namespace media
