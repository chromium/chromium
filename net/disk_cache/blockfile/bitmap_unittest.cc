// Copyright 2009 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/disk_cache/blockfile/bitmap.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(BitmapTest, OverAllocate) {
  // Test that we don't over allocate on boundaries.
  disk_cache::Bitmap map32(32, false);
  EXPECT_EQ(1, map32.ArraySize());

  disk_cache::Bitmap map64(64, false);
  EXPECT_EQ(2, map64.ArraySize());
}

TEST(BitmapTest, DefaultConstructor) {
  // Verify that the default constructor doesn't allocate a bitmap.
  disk_cache::Bitmap map;
  EXPECT_EQ(0, map.Size());
  EXPECT_EQ(0, map.ArraySize());
  EXPECT_TRUE(nullptr == map.GetMap());
}

TEST(BitmapTest, Basics) {
  disk_cache::Bitmap bitmap(80, true);
  const uint32_t kValue = 0x74f10060;

  // Test proper allocation size.
  EXPECT_EQ(80, bitmap.Size());
  EXPECT_EQ(3, bitmap.ArraySize());

  // Test Set/GetMapElement.
  EXPECT_EQ(0U, bitmap.GetMapElement(1));
  bitmap.SetMapElement(1, kValue);
  EXPECT_EQ(kValue, bitmap.GetMapElement(1));

  // Test Set/Get.
  EXPECT_TRUE(bitmap.Get(48));
  EXPECT_FALSE(bitmap.Get(49));
  EXPECT_FALSE(bitmap.Get(50));
  bitmap.Set(49, true);
  EXPECT_TRUE(bitmap.Get(48));
  EXPECT_TRUE(bitmap.Get(49));
  EXPECT_FALSE(bitmap.Get(50));
  bitmap.Set(49, false);
  EXPECT_TRUE(bitmap.Get(48));
  EXPECT_FALSE(bitmap.Get(49));
  EXPECT_FALSE(bitmap.Get(50));

  for (int i = 0; i < 80; i++)
    bitmap.Set(i, (i % 7) == 0);
  for (int i = 0; i < 80; i++)
    EXPECT_EQ(bitmap.Get(i), (i % 7) == 0);
}

TEST(BitmapTest, Toggle) {
  static const int kSize = 100;
  disk_cache::Bitmap map(kSize, true);
  for (int i = 0; i < 100; i += 3)
    map.Toggle(i);
  for (int i = 0; i < 100; i += 9)
    map.Toggle(i);
  for (int i = 0; i < 100; ++i)
    EXPECT_EQ((i % 3 == 0) && (i % 9 != 0), map.Get(i));
}

TEST(BitmapTest, Resize) {
  const int kSize1 = 50;
  const int kSize2 = 100;
  const int kSize3 = 30;
  disk_cache::Bitmap map(kSize1, true);
  map.Resize(kSize1, true);
  EXPECT_EQ(kSize1, map.Size());
  EXPECT_FALSE(map.Get(0));
  EXPECT_FALSE(map.Get(kSize1 - 1));

  map.Resize(kSize2, true);
  EXPECT_FALSE(map.Get(kSize1 - 1));
  EXPECT_FALSE(map.Get(kSize1));
  EXPECT_FALSE(map.Get(kSize2 - 1));
  EXPECT_EQ(kSize2, map.Size());

  map.Resize(kSize3, true);
  EXPECT_FALSE(map.Get(kSize3 - 1));
  EXPECT_EQ(kSize3, map.Size());
}

TEST(BitmapTest, Map) {
  // Tests Set/GetMap and the constructor that takes an array.
  const int kMapSize = 80;
  char local_map[kMapSize];
  for (int i = 0; i < kMapSize; i++)
    local_map[i] = static_cast<char>(i);

  disk_cache::Bitmap bitmap(kMapSize * 8, false);
  bitmap.SetMap(reinterpret_cast<uint32_t*>(local_map), kMapSize / 4);
  for (int i = 0; i < kMapSize; i++) {
    if (i % 2)
      EXPECT_TRUE(bitmap.Get(i * 8));
    else
      EXPECT_FALSE(bitmap.Get(i * 8));
  }

  EXPECT_EQ(0, memcmp(local_map, bitmap.GetMap(), kMapSize));

  // Now let's create a bitmap that shares local_map as storage.
  disk_cache::Bitmap bitmap2(reinterpret_cast<uint32_t*>(local_map),
                             kMapSize * 8, kMapSize / 4);
  EXPECT_EQ(0, memcmp(local_map, bitmap2.GetMap(), kMapSize));

  local_map[kMapSize / 2] = 'a';
  EXPECT_EQ(0, memcmp(local_map, bitmap2.GetMap(), kMapSize));
  EXPECT_NE(0, memcmp(local_map, bitmap.GetMap(), kMapSize));
}

TEST(BitmapTest, SetAll) {
  // Tests SetAll and Clear.
  const int kMapSize = 80;
  char ones[kMapSize];
  char zeros[kMapSize];
  memset(ones, 0xff, kMapSize);
  memset(zeros, 0, kMapSize);

  disk_cache::Bitmap map(kMapSize * 8, true);
  EXPECT_EQ(0, memcmp(zeros, map.GetMap(), kMapSize));
  map.SetAll(true);
  EXPECT_EQ(0, memcmp(ones, map.GetMap(), kMapSize));
  map.SetAll(false);
  EXPECT_EQ(0, memcmp(zeros, map.GetMap(), kMapSize));
  map.SetAll(true);
  map.Clear();
  EXPECT_EQ(0, memcmp(zeros, map.GetMap(), kMapSize));
}

TEST(BitmapTest, Range) {
  // Tests SetRange() and TestRange().
  disk_cache::Bitmap map(100, true);
  EXPECT_FALSE(map.TestRange(0, 100, true));
  map.Set(50, true);
  EXPECT_TRUE(map.TestRange(0, 100, true));

  map.SetAll(false);
  EXPECT_FALSE(map.TestRange(0, 1, true));
  EXPECT_FALSE(map.TestRange(30, 31, true));
  EXPECT_FALSE(map.TestRange(98, 99, true));
  EXPECT_FALSE(map.TestRange(99, 100, true));
  EXPECT_FALSE(map.TestRange(0, 100, true));

  EXPECT_TRUE(map.TestRange(0, 1, false));
  EXPECT_TRUE(map.TestRange(31, 32, false));
  EXPECT_TRUE(map.TestRange(32, 33, false));
  EXPECT_TRUE(map.TestRange(99, 100, false));
  EXPECT_TRUE(map.TestRange(0, 32, false));

  map.SetRange(11, 21, true);
  for (int i = 0; i < 100; i++)
    EXPECT_EQ(map.Get(i), (i >= 11) && (i < 21));

  EXPECT_TRUE(map.TestRange(0, 32, true));
  EXPECT_TRUE(map.TestRange(0, 100, true));
  EXPECT_TRUE(map.TestRange(11, 21, true));
  EXPECT_TRUE(map.TestRange(15, 16, true));
  EXPECT_TRUE(map.TestRange(5, 12, true));
  EXPECT_TRUE(map.TestRange(5, 11, false));
  EXPECT_TRUE(map.TestRange(20, 60, true));
  EXPECT_TRUE(map.TestRange(21, 60, false));

  map.SetAll(true);
  EXPECT_FALSE(map.TestRange(0, 100, false));

  map.SetRange(70, 99, false);
  EXPECT_TRUE(map.TestRange(69, 99, false));
  EXPECT_TRUE(map.TestRange(70, 100, false));
  EXPECT_FALSE(map.TestRange(70, 99, true));
}

TEST(BitmapTest, FindNextSetBitBeforeLimit) {
  // Test FindNextSetBitBeforeLimit. Only check bits from 111 to 277 (limit
  // bit == 278). Should find all multiples of 27 in that range.
  disk_cache::Bitmap map(500, true);
  for (int i = 0; i < 500; i++)
    map.Set(i, (i % 27) == 0);

  int find_me = 135;  // First one expected.
  for (int index = 111; map.FindNextSetBitBeforeLimit(&index, 278);
       ++index) {
    EXPECT_EQ(index, find_me);
    find_me += 27;
  }
  EXPECT_EQ(find_me, 297);  // The next find_me after 278.
}

TEST(BitmapTest, FindNextSetBitBeforeLimitAligned) {
  // Test FindNextSetBitBeforeLimit on aligned scans.
  disk_cache::Bitmap map(256, true);
  for (int i = 0; i < 256; i++)
    map.Set(i, (i % 32) == 0);
  for (int i = 0; i < 256; i += 32) {
    int index = i + 1;
    EXPECT_FALSE(map.FindNextSetBitBeforeLimit(&index, i + 32));
  }
}

TEST(BitmapTest, FindNextSetBit) {
  // Test FindNextSetBit. Check all bits in map. Should find multiples
  // of 7 from 0 to 98.
  disk_cache::Bitmap map(100, true);
  for (int i = 0; i < 100; i++)
    map.Set(i, (i % 7) == 0);

  int find_me = 0;  // First one expected.
  for (int index = 0; map.FindNextSetBit(&index); ++index) {
    EXPECT_EQ(index, find_me);
    find_me += 7;
  }
  EXPECT_EQ(find_me, 105);  // The next find_me after 98.
}

TEST(BitmapTest, FindNextBit) {
  // Almost the same test as FindNextSetBit, but find zeros instead of ones.
  disk_cache::Bitmap map(100, false);
  map.SetAll(true);
  for (int i = 0; i < 100; i++)
    map.Set(i, (i % 7) != 0);

  int find_me = 0;  // First one expected.
  for (int index = 0; map.FindNextBit(&index, 100, false); ++index) {
    EXPECT_EQ(index, find_me);
    find_me += 7;
  }
  EXPECT_EQ(find_me, 105);  // The next find_me after 98.
}

TEST(BitmapTest, SimpleFindBits) {
  disk_cache::Bitmap bitmap(64, true);
  bitmap.SetMapElement(0, 0x7ff10060);

  // Bit at index off.
  int index = 0;
  EXPECT_EQ(5, bitmap.FindBits(&index, 63, false));
  EXPECT_EQ(0, index);

  EXPECT_EQ(2, bitmap.FindBits(&index, 63, true));
  EXPECT_EQ(5, index);

  index = 0;
  EXPECT_EQ(2, bitmap.FindBits(&index, 63, true));
  EXPECT_EQ(5, index);

  index = 6;
  EXPECT_EQ(9, bitmap.FindBits(&index, 63, false));
  EXPECT_EQ(7, index);

  // Bit at index on.
  index = 16;
  EXPECT_EQ(1, bitmap.FindBits(&index, 63, true));
  EXPECT_EQ(16, index);

  index = 17;
  EXPECT_EQ(11, bitmap.FindBits(&index, 63, true));
  EXPECT_EQ(20, index);

  index = 31;
  EXPECT_EQ(0, bitmap.FindBits(&index, 63, true));
  EXPECT_EQ(31, index);

  // With a limit.
  index = 8;
  EXPECT_EQ(0, bitmap.FindBits(&index, 16, true));
}

TEST(BitmapTest, MultiWordFindBits) {
  disk_cache::Bitmap bitmap(500, true);
  bitmap.SetMapElement(10, 0xff00);

  int index = 0;
  EXPECT_EQ(0, bitmap.FindBits(&index, 300, true));

  EXPECT_EQ(8, bitmap.FindBits(&index, 500, true));
  EXPECT_EQ(328, index);

  bitmap.SetMapElement(10, 0xff000000);
  bitmap.SetMapElement(11, 0xff);

  index = 0;
  EXPECT_EQ(16, bitmap.FindBits(&index, 500, true));
  EXPECT_EQ(344, index);

  index = 0;
  EXPECT_EQ(4, bitmap.FindBits(&index, 348, true));
  EXPECT_EQ(344, index);
}
