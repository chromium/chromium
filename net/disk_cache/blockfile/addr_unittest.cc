// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/blockfile/addr.h"
#include "net/disk_cache/disk_cache_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace disk_cache {

TEST_F(DiskCacheTest, CacheAddr_Size) {
  Addr addr1(0);
  EXPECT_FALSE(addr1.is_initialized());

  // The object should not be more expensive than the actual address.
  EXPECT_EQ(sizeof(uint32_t), sizeof(addr1));
}

TEST_F(DiskCacheTest, CacheAddr_ValidValues) {
  Addr addr2(BLOCK_1K, 3, 5, 25);
  EXPECT_EQ(BLOCK_1K, addr2.file_type());
  EXPECT_EQ(3, addr2.num_blocks());
  EXPECT_EQ(5, addr2.FileNumber());
  EXPECT_EQ(25, addr2.start_block());
  EXPECT_EQ(1024, addr2.BlockSize());
}

TEST_F(DiskCacheTest, CacheAddr_InvalidValues) {
  Addr addr3(BLOCK_4K, 0x44, 0x41508, 0x952536);
  EXPECT_EQ(BLOCK_4K, addr3.file_type());
  EXPECT_EQ(4, addr3.num_blocks());
  EXPECT_EQ(8, addr3.FileNumber());
  EXPECT_EQ(0x2536, addr3.start_block());
  EXPECT_EQ(4096, addr3.BlockSize());
}

TEST_F(DiskCacheTest, CacheAddr_SanityCheck) {
  // First a few valid values.
  EXPECT_TRUE(Addr(0).SanityCheck());
  EXPECT_TRUE(Addr(0x80001000).SanityCheck());
  EXPECT_TRUE(Addr(0xC3FFFFFF).SanityCheck());
  EXPECT_TRUE(Addr(0xC0FFFFFF).SanityCheck());

  // Not initialized.
  EXPECT_FALSE(Addr(0x20).SanityCheck());
  EXPECT_FALSE(Addr(0x10001000).SanityCheck());

  // Invalid file type.
  EXPECT_FALSE(Addr(0xD0001000).SanityCheck());
  EXPECT_FALSE(Addr(0xF0000000).SanityCheck());

  // Reserved bits.
  EXPECT_FALSE(Addr(0x14000000).SanityCheck());
  EXPECT_FALSE(Addr(0x18000000).SanityCheck());
}

}  // namespace disk_cache
