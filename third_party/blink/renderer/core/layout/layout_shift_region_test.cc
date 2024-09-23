// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/layout/layout_shift_region.h"

#include <gtest/gtest.h>
#include "cc/base/region.h"

namespace blink {

class LayoutShiftRegionTest : public testing::Test {};

TEST_F(LayoutShiftRegionTest, Basic) {
  LayoutShiftRegion region;
  EXPECT_EQ(0u, region.Area());

  region.AddRect(gfx::Rect(2, 1, 1, 3));
  EXPECT_EQ(3u, region.Area());

  region.AddRect(gfx::Rect(1, 2, 3, 1));
  EXPECT_EQ(5u, region.Area());

  region.AddRect(gfx::Rect(1, 2, 1, 1));
  region.AddRect(gfx::Rect(3, 2, 1, 1));
  region.AddRect(gfx::Rect(2, 1, 1, 1));
  region.AddRect(gfx::Rect(2, 3, 1, 1));
  EXPECT_EQ(5u, region.Area());

  region.AddRect(gfx::Rect(1, 1, 1, 1));
  EXPECT_EQ(6u, region.Area());

  region.AddRect(gfx::Rect(1, 1, 3, 3));
  EXPECT_EQ(9u, region.Area());

  region.AddRect(gfx::Rect(0, 0, 2, 2));
  EXPECT_EQ(12u, region.Area());

  region.AddRect(gfx::Rect(-1, -1, 2, 2));
  EXPECT_EQ(15u, region.Area());

  region.Reset();
  EXPECT_EQ(0u, region.Area());
}

TEST_F(LayoutShiftRegionTest, LargeRandom) {
  LayoutShiftRegion region;
  cc::Region naive_region;
  static const int data[] = {
      52613, 38528, 20785, 40550, 29734, 48229, 37113, 3520,  66776, 26746,
      20527, 11398, 27951, 50399, 37139, 17597, 20593, 57272, 12528, 5907,
      18369, 6955,  50779, 41129, 66685, 46725, 30708, 32429, 140,   55034,
      14770, 40886, 54560, 53666, 15350, 12692, 29354, 47388, 47542, 15474,
      17770, 70300, 27992, 6731,  47459, 42205, 45231, 9398,  15606, 2238,
      8387,  44579, 45222, 35626, 53932, 2907,  14899, 18234, 60609, 34125,
      23985, 48145, 40247, 25215, 64427, 41207, 29742, 35282, 21390, 12640,
      14653, 71326, 41293, 4593,  54114, 55398, 17797, 55637, 64133, 25985,
      45213, 6428,  6496,  37832, 31291, 27955, 32967, 4134,  35992, 3226,
      43190, 31310, 49828, 6737,  31847, 65511, 52287, 41393, 33728, 29813,
      32425, 74095, 41857, 2537,  14073, 16177, 23053, 75553, 3570,  76482,
      49801, 17920, 45628, 59408, 44788, 18020, 11607, 21027, 27095, 52992,
      37770, 51722, 15857, 38088, 22031, 68391, 66615, 2592,  91,    16324,
      64393, 51544, 3848,  1924,  90673, 16461, 97524, 42603, 122,   55027,
      7945,  10493, 89602, 38306, 73269, 72165, 15014, 23160, 10208, 66632,
      78104, 22252, 52910, 7870,  293,   61338, 54913, 48813, 3949,  6507,
      82176, 60067, 13639, 13096, 71024, 52767, 20514, 4716,  15125, 14158,
      24315, 46986, 62316, 95391, 8390,  1007,  9520,  67532, 69963, 20117,
      51649, 42999, 1441,  34966, 17616, 16544, 51218, 72116, 1780,  12254,
      52065, 67026, 88250, 39824, 1786,  22090, 14884, 41933, 46081, 25596,
      89968, 51346, 2479,  36409, 11513, 36037, 19481, 4287,  33831, 28199,
      56514, 52659, 54910, 14740, 43540, 45912, 44651, 4232,  15199, 45442,
      45856, 19374, 17597, 50923, 24227, 17000, 47585, 61718, 48390, 37848,
      23677, 2669,  49142, 37207, 30794, 11373, 41719, 40002, 39749, 39146,
      39144, 59801, 23772, 17552, 26731, 7802,  29291, 40281, 82706, 9370,
      7006,  75864, 94618, 75409, 5267,  5222,  47927, 19430, 4425,  14295,
      16662, 22094, 33027, 48759, 42250, 5205,  5424,  70064, 36751, 60688,
      45415, 24027, 37665, 88085, 16011, 8785,  12656, 1662,  68336, 62175,
      2132,  66236, 5301,  5174,  9575,  42509, 41511, 44451, 59069, 43296,
      3246,  11251, 37176, 25619, 60728, 36030, 40982, 33756, 46296, 4407,
      84886, 59809, 8127,  34846, 44433, 4366,  4823,  52452, 4594,  69662,
      59199, 18623, 29345, 36375, 20166, 12254, 30879, 84106, 29786, 7838,
      35875, 32227, 34871, 31142, 71453, 74402, 3243,  4475,  1974,  62754,
      80498, 26875, 22957, 25916, 74769, 66343, 18666, 28537, 41799, 54598,
      32617, 73615, 51275, 20602, 10642, 57506, 72158, 38152, 12552, 36601,
      29638, 28894, 67153, 27560, 1577,  67248, 65745, 53338, 4220,  20883,
      72059, 33747, 11195, 47783, 21251, 92912, 25,    4257,  17625, 29683,
      32964, 31019, 37510, 2205,  47755, 15187, 9769,  28377, 28890, 6955,
      31621, 21088, 54431, 30372, 14567, 47483, 80553, 4324,  10574, 870,
      59862, 86272, 8682,  49237, 85735, 10570, 21034, 50807, 47647, 37221,
  };
  uint64_t expected_area = 9201862875ul;
  for (unsigned i = 0; i < 100; i++) {
    const int* d = data + (i * 4);
    gfx::Rect r(d[0], d[1], d[2], d[3]);
    region.AddRect(r);
    naive_region.Union(r);
  }
  EXPECT_EQ(expected_area, region.Area());

  uint64_t naive_region_area = 0;
  for (gfx::Rect rect : naive_region)
    naive_region_area += rect.size().Area64();
  EXPECT_EQ(expected_area, naive_region_area);
}

// Creates a region like this:
//   █ █ █
//  ███████
//   █ █ █
//  ███████
//   █ █ █
//  ███████
//   █ █ █
TEST_F(LayoutShiftRegionTest, Waffle) {
  LayoutShiftRegion region;
  unsigned n = 250000;
  for (unsigned i = 2; i <= n; i += 2) {
    region.AddRect(gfx::Rect(i, 1, 1, n + 1));
    region.AddRect(gfx::Rect(1, i, n + 1, 1));
  }
  uint64_t half = n >> 1;
  uint64_t area = n * (half + 1) + half * half;
  EXPECT_EQ(area, region.Area());
}

}  // namespace blink
