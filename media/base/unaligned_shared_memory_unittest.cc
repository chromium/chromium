// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/unaligned_shared_memory.h"

#include <stdint.h>
#include <string.h>

#include <limits>

#include "base/logging.h"
#include "base/stl_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

const uint8_t kUnalignedData[] = "XXXhello";
const size_t kUnalignedDataSize = base::size(kUnalignedData);
const off_t kUnalignedOffset = 3;

const uint8_t kData[] = "hello";
const size_t kDataSize = base::size(kData);

base::UnsafeSharedMemoryRegion CreateRegion(const uint8_t* data, size_t size) {
  auto region = base::UnsafeSharedMemoryRegion::Create(size);
  auto mapping = region.Map();
  EXPECT_TRUE(mapping.IsValid());
  memcpy(mapping.memory(), data, size);
  return region;
}

base::ReadOnlySharedMemoryRegion CreateReadOnlyRegion(const uint8_t* data,
                                                      size_t size) {
  auto mapped_region = base::ReadOnlySharedMemoryRegion::Create(size);
  EXPECT_TRUE(mapped_region.IsValid());
  memcpy(mapped_region.mapping.memory(), data, size);
  return std::move(mapped_region.region);
}
}  // namespace

TEST(UnalignedSharedMemoryTest, CreateAndDestroyRegion) {
  auto region = CreateRegion(kData, kDataSize);
  UnalignedSharedMemory shm(
      base::UnsafeSharedMemoryRegion::TakeHandleForSerialization(
          std::move(region)),
      kDataSize, false);
}

TEST(UnalignedSharedMemoryTest, CreateAndDestroyReadOnlyRegion) {
  auto region = CreateReadOnlyRegion(kData, kDataSize);
  UnalignedSharedMemory shm(
      base::ReadOnlySharedMemoryRegion::TakeHandleForSerialization(
          std::move(region)),
      kDataSize, true);
}

TEST(UnalignedSharedMemoryTest, CreateAndDestroy_InvalidRegion) {
  UnalignedSharedMemory shm(base::subtle::PlatformSharedMemoryRegion(),
                            kDataSize, false);
}

TEST(UnalignedSharedMemoryTest, MapRegion) {
  auto region = CreateRegion(kData, kDataSize);
  UnalignedSharedMemory shm(
      base::UnsafeSharedMemoryRegion::TakeHandleForSerialization(
          std::move(region)),
      kDataSize, false);
  ASSERT_TRUE(shm.MapAt(0, kDataSize));
  EXPECT_EQ(0, memcmp(shm.memory(), kData, kDataSize));
}

TEST(UnalignedSharedMemoryTest, MapReadOnlyRegion) {
  auto region = CreateReadOnlyRegion(kData, kDataSize);
  UnalignedSharedMemory shm(
      base::ReadOnlySharedMemoryRegion::TakeHandleForSerialization(
          std::move(region)),
      kDataSize, true);
  ASSERT_TRUE(shm.MapAt(0, kDataSize));
  EXPECT_EQ(0, memcmp(shm.memory(), kData, kDataSize));
}

TEST(UnalignedSharedMemoryTest, Map_UnalignedRegion) {
  auto region = CreateRegion(kUnalignedData, kUnalignedDataSize);
  UnalignedSharedMemory shm(
      base::UnsafeSharedMemoryRegion::TakeHandleForSerialization(
          std::move(region)),
      kUnalignedDataSize, false);
  ASSERT_TRUE(shm.MapAt(kUnalignedOffset, kDataSize));
  EXPECT_EQ(0, memcmp(shm.memory(), kData, kDataSize));
}

TEST(UnalignedSharedMemoryTest, Map_UnalignedReadOnlyRegion) {
  auto region = CreateReadOnlyRegion(kUnalignedData, kUnalignedDataSize);
  UnalignedSharedMemory shm(
      base::ReadOnlySharedMemoryRegion::TakeHandleForSerialization(
          std::move(region)),
      kUnalignedDataSize, true);
  ASSERT_TRUE(shm.MapAt(kUnalignedOffset, kDataSize));
  EXPECT_EQ(0, memcmp(shm.memory(), kData, kDataSize));
}

TEST(UnalignedSharedMemoryTest, Map_InvalidRegion) {
  UnalignedSharedMemory shm(base::subtle::PlatformSharedMemoryRegion(),
                            kDataSize, true);
  ASSERT_FALSE(shm.MapAt(1, kDataSize));
  EXPECT_EQ(shm.memory(), nullptr);
}

TEST(UnalignedSharedMemoryTest, Map_NegativeOffsetRegion) {
  auto region = CreateRegion(kData, kDataSize);
  UnalignedSharedMemory shm(
      base::UnsafeSharedMemoryRegion::TakeHandleForSerialization(
          std::move(region)),
      kDataSize, false);
  ASSERT_FALSE(shm.MapAt(-1, kDataSize));
}

TEST(UnalignedSharedMemoryTest, Map_NegativeOffsetReadOnlyRegion) {
  auto region = CreateReadOnlyRegion(kData, kDataSize);
  UnalignedSharedMemory shm(
      base::ReadOnlySharedMemoryRegion::TakeHandleForSerialization(
          std::move(region)),
      kDataSize, true);
  ASSERT_FALSE(shm.MapAt(-1, kDataSize));
}

TEST(UnalignedSharedMemoryTest, Map_SizeOverflowRegion) {
  auto region = CreateRegion(kData, kDataSize);
  UnalignedSharedMemory shm(
      base::UnsafeSharedMemoryRegion::TakeHandleForSerialization(
          std::move(region)),
      kDataSize, false);
  ASSERT_FALSE(shm.MapAt(1, std::numeric_limits<size_t>::max()));
}

TEST(UnalignedSharedMemoryTest, Map_SizeOverflowReadOnlyRegion) {
  auto region = CreateReadOnlyRegion(kData, kDataSize);
  UnalignedSharedMemory shm(
      base::ReadOnlySharedMemoryRegion::TakeHandleForSerialization(
          std::move(region)),
      kDataSize, true);
  ASSERT_FALSE(shm.MapAt(1, std::numeric_limits<size_t>::max()));
}

TEST(UnalignedSharedMemoryTest, UnmappedRegionIsNullptr) {
  auto region = CreateRegion(kData, kDataSize);
  UnalignedSharedMemory shm(
      base::UnsafeSharedMemoryRegion::TakeHandleForSerialization(
          std::move(region)),
      kDataSize, false);
  ASSERT_EQ(shm.memory(), nullptr);
}

TEST(UnalignedSharedMemoryTest, UnmappedReadOnlyRegionIsNullptr) {
  auto region = CreateReadOnlyRegion(kData, kDataSize);
  UnalignedSharedMemory shm(
      base::ReadOnlySharedMemoryRegion::TakeHandleForSerialization(
          std::move(region)),
      kDataSize, true);
  ASSERT_EQ(shm.memory(), nullptr);
}

TEST(WritableUnalignedMappingTest, CreateAndDestroy) {
  auto region = CreateRegion(kData, kDataSize);
  WritableUnalignedMapping shm(region, kDataSize, 0);
  EXPECT_TRUE(shm.IsValid());
}

TEST(WritableUnalignedMappingTest, CreateAndDestroy_InvalidRegion) {
  base::UnsafeSharedMemoryRegion region;
  WritableUnalignedMapping shm(region, kDataSize, 0);
  EXPECT_FALSE(shm.IsValid());
}

TEST(WritableUnalignedMappingTest, Map) {
  auto region = CreateRegion(kData, kDataSize);
  WritableUnalignedMapping shm(region, kDataSize, 0);
  ASSERT_TRUE(shm.IsValid());
  EXPECT_EQ(0, memcmp(shm.memory(), kData, kDataSize));
}

TEST(WritableUnalignedMappingTest, Map_Unaligned) {
  auto region = CreateRegion(kUnalignedData, kUnalignedDataSize);
  WritableUnalignedMapping shm(region, kDataSize, kUnalignedOffset);
  ASSERT_TRUE(shm.IsValid());
  EXPECT_EQ(0, memcmp(shm.memory(), kData, kDataSize));
}

TEST(WritableUnalignedMappingTest, Map_InvalidRegion) {
  base::UnsafeSharedMemoryRegion region;
  WritableUnalignedMapping shm(region, kDataSize, 0);
  ASSERT_FALSE(shm.IsValid());
  EXPECT_EQ(shm.memory(), nullptr);
}

TEST(WritableUnalignedMappingTest, Map_NegativeOffset) {
  auto region = CreateRegion(kData, kDataSize);
  WritableUnalignedMapping shm(region, kDataSize, -1);
  ASSERT_FALSE(shm.IsValid());
}

TEST(WritableUnalignedMappingTest, Map_SizeOverflow) {
  auto region = CreateRegion(kData, kDataSize);
  WritableUnalignedMapping shm(region, std::numeric_limits<size_t>::max(), 1);
  ASSERT_FALSE(shm.IsValid());
}

TEST(ReadOnlyUnalignedMappingTest, CreateAndDestroy) {
  auto region = CreateReadOnlyRegion(kData, kDataSize);
  ReadOnlyUnalignedMapping shm(region, kDataSize, 0);
  EXPECT_TRUE(shm.IsValid());
}

TEST(ReadOnlyUnalignedMappingTest, CreateAndDestroy_InvalidRegion) {
  base::ReadOnlySharedMemoryRegion region;
  ReadOnlyUnalignedMapping shm(region, kDataSize, 0);
  EXPECT_FALSE(shm.IsValid());
}

TEST(ReadOnlyUnalignedMappingTest, Map) {
  auto region = CreateReadOnlyRegion(kData, kDataSize);
  ReadOnlyUnalignedMapping shm(region, kDataSize, 0);
  ASSERT_TRUE(shm.IsValid());
  EXPECT_EQ(0, memcmp(shm.memory(), kData, kDataSize));
}

TEST(ReadOnlyUnalignedMappingTest, Map_Unaligned) {
  auto region = CreateReadOnlyRegion(kUnalignedData, kUnalignedDataSize);
  ReadOnlyUnalignedMapping shm(region, kDataSize, kUnalignedOffset);
  ASSERT_TRUE(shm.IsValid());
  EXPECT_EQ(0, memcmp(shm.memory(), kData, kDataSize));
}

TEST(ReadOnlyUnalignedMappingTest, Map_InvalidRegion) {
  base::ReadOnlySharedMemoryRegion region;
  ReadOnlyUnalignedMapping shm(region, kDataSize, 0);
  ASSERT_FALSE(shm.IsValid());
  EXPECT_EQ(shm.memory(), nullptr);
}

TEST(ReadOnlyUnalignedMappingTest, Map_NegativeOffset) {
  auto region = CreateReadOnlyRegion(kData, kDataSize);
  ReadOnlyUnalignedMapping shm(region, kDataSize, -1);
  ASSERT_FALSE(shm.IsValid());
}

TEST(ReadOnlyUnalignedMappingTest, Map_SizeOverflow) {
  auto region = CreateReadOnlyRegion(kData, kDataSize);
  ReadOnlyUnalignedMapping shm(region, std::numeric_limits<size_t>::max(), 1);
  ASSERT_FALSE(shm.IsValid());
}

}  // namespace media
