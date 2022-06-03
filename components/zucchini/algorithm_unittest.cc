// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/algorithm.h"

#include <stddef.h>
#include <stdint.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace zucchini {

namespace {

// Casting functions to specify signed 8-bit and 16-bit integer constants.
// For example, signed8(0xFF) == int8_t(-1).
inline int8_t signed8(uint8_t v) {
  return *reinterpret_cast<const int8_t*>(&v);
}

inline int32_t signed16(uint16_t v) {
  return *reinterpret_cast<const int16_t*>(&v);
}

}  // namespace

TEST(AlgorithmTest, RangeIsBounded) {
  // Basic tests.
  EXPECT_TRUE(RangeIsBounded<uint8_t>(0U, +0U, 10U));
  EXPECT_TRUE(RangeIsBounded<uint8_t>(0U, +10U, 10U));
  EXPECT_TRUE(RangeIsBounded<uint8_t>(1U, +9U, 10U));
  EXPECT_FALSE(RangeIsBounded<uint8_t>(1U, +10U, 10U));
  EXPECT_TRUE(RangeIsBounded<uint8_t>(8U, +1U, 10U));
  EXPECT_TRUE(RangeIsBounded<uint8_t>(8U, +2U, 10U));
  EXPECT_TRUE(RangeIsBounded<uint8_t>(9U, +0U, 10U));
  EXPECT_FALSE(RangeIsBounded<uint8_t>(10U, +0U, 10U));  // !
  EXPECT_FALSE(RangeIsBounded<uint8_t>(100U, +0U, 10U));
  EXPECT_FALSE(RangeIsBounded<uint8_t>(100U, +1U, 10U));

  // Test at boundary of overflow.
  EXPECT_TRUE(RangeIsBounded<uint8_t>(42U, +137U, 255U));
  EXPECT_TRUE(RangeIsBounded<uint8_t>(0U, +255U, 255U));
  EXPECT_TRUE(RangeIsBounded<uint8_t>(1U, +254U, 255U));
  EXPECT_FALSE(RangeIsBounded<uint8_t>(1U, +255U, 255U));
  EXPECT_TRUE(RangeIsBounded<uint8_t>(254U, +0U, 255U));
  EXPECT_TRUE(RangeIsBounded<uint8_t>(254U, +1U, 255U));
  EXPECT_FALSE(RangeIsBounded<uint8_t>(255U, +0U, 255U));
  EXPECT_FALSE(RangeIsBounded<uint8_t>(255U, +3U, 255U));

  // Test with uint32_t.
  EXPECT_TRUE(RangeIsBounded<uint32_t>(0U, +0x1000U, 0x2000U));
  EXPECT_TRUE(RangeIsBounded<uint32_t>(0x0FFFU, +0x1000U, 0x2000U));
  EXPECT_TRUE(RangeIsBounded<uint32_t>(0x1000U, +0x1000U, 0x2000U));
  EXPECT_FALSE(RangeIsBounded<uint32_t>(0x1000U, +0x1001U, 0x2000U));
  EXPECT_TRUE(RangeIsBounded<uint32_t>(0x1FFFU, +1U, 0x2000U));
  EXPECT_FALSE(RangeIsBounded<uint32_t>(0x2000U, +0U, 0x2000U));  // !
  EXPECT_FALSE(RangeIsBounded<uint32_t>(0x3000U, +0U, 0x2000U));
  EXPECT_FALSE(RangeIsBounded<uint32_t>(0x3000U, +1U, 0x2000U));
  EXPECT_TRUE(RangeIsBounded<uint32_t>(0U, +0xFFFFFFFEU, 0xFFFFFFFFU));
  EXPECT_TRUE(RangeIsBounded<uint32_t>(0U, +0xFFFFFFFFU, 0xFFFFFFFFU));
  EXPECT_TRUE(RangeIsBounded<uint32_t>(1U, +0xFFFFFFFEU, 0xFFFFFFFFU));
  EXPECT_FALSE(RangeIsBounded<uint32_t>(1U, +0xFFFFFFFFU, 0xFFFFFFFFU));
  EXPECT_TRUE(RangeIsBounded<uint32_t>(0x80000000U, +0x7FFFFFFFU, 0xFFFFFFFFU));
  EXPECT_FALSE(
      RangeIsBounded<uint32_t>(0x80000000U, +0x80000000U, 0xFFFFFFFFU));
  EXPECT_TRUE(RangeIsBounded<uint32_t>(0xFFFFFFFEU, +1U, 0xFFFFFFFFU));
  EXPECT_FALSE(RangeIsBounded<uint32_t>(0xFFFFFFFFU, +0U, 0xFFFFFFFFU));  // !
  EXPECT_FALSE(
      RangeIsBounded<uint32_t>(0xFFFFFFFFU, +0xFFFFFFFFU, 0xFFFFFFFFU));
}

TEST(AlgorithmTest, RangeCovers) {
  // Basic tests.
  EXPECT_TRUE(RangeCovers<uint8_t>(0U, +10U, 0U));
  EXPECT_TRUE(RangeCovers<uint8_t>(0U, +10U, 5U));
  EXPECT_TRUE(RangeCovers<uint8_t>(0U, +10U, 9U));
  EXPECT_FALSE(RangeCovers<uint8_t>(0U, +10U, 10U));
  EXPECT_FALSE(RangeCovers<uint8_t>(0U, +10U, 100U));
  EXPECT_FALSE(RangeCovers<uint8_t>(0U, +10U, 255U));

  EXPECT_FALSE(RangeCovers<uint8_t>(42U, +137U, 0U));
  EXPECT_FALSE(RangeCovers<uint8_t>(42U, +137U, 41U));
  EXPECT_TRUE(RangeCovers<uint8_t>(42U, +137U, 42U));
  EXPECT_TRUE(RangeCovers<uint8_t>(42U, +137U, 100U));
  EXPECT_TRUE(RangeCovers<uint8_t>(42U, +137U, 178U));
  EXPECT_FALSE(RangeCovers<uint8_t>(42U, +137U, 179U));
  EXPECT_FALSE(RangeCovers<uint8_t>(42U, +137U, 255U));

  // 0-size ranges.
  EXPECT_FALSE(RangeCovers<uint8_t>(42U, +0U, 41U));
  EXPECT_FALSE(RangeCovers<uint8_t>(42U, +0U, 42U));
  EXPECT_FALSE(RangeCovers<uint8_t>(42U, +0U, 43U));

  // Test at boundary of overflow.
  EXPECT_TRUE(RangeCovers<uint8_t>(254U, +1U, 254U));
  EXPECT_FALSE(RangeCovers<uint8_t>(254U, +1U, 255U));
  EXPECT_FALSE(RangeCovers<uint8_t>(255U, +0U, 255U));
  EXPECT_TRUE(RangeCovers<uint8_t>(255U, +1U, 255U));
  EXPECT_FALSE(RangeCovers<uint8_t>(255U, +5U, 0U));

  // Test with unit32_t.
  EXPECT_FALSE(RangeCovers<uint32_t>(1234567U, +7654321U, 0U));
  EXPECT_FALSE(RangeCovers<uint32_t>(1234567U, +7654321U, 1234566U));
  EXPECT_TRUE(RangeCovers<uint32_t>(1234567U, +7654321U, 1234567U));
  EXPECT_TRUE(RangeCovers<uint32_t>(1234567U, +7654321U, 4444444U));
  EXPECT_TRUE(RangeCovers<uint32_t>(1234567U, +7654321U, 8888887U));
  EXPECT_FALSE(RangeCovers<uint32_t>(1234567U, +7654321U, 8888888U));
  EXPECT_FALSE(RangeCovers<uint32_t>(1234567U, +7654321U, 0x80000000U));
  EXPECT_FALSE(RangeCovers<uint32_t>(1234567U, +7654321U, 0xFFFFFFFFU));
  EXPECT_FALSE(RangeCovers<uint32_t>(0xFFFFFFFFU, +0, 0xFFFFFFFFU));
  EXPECT_TRUE(RangeCovers<uint32_t>(0xFFFFFFFFU, +1, 0xFFFFFFFFU));
  EXPECT_FALSE(RangeCovers<uint32_t>(0xFFFFFFFFU, +2, 0));
}

TEST(AlgorithmTest, InclusiveClamp) {
  EXPECT_EQ(1U, InclusiveClamp<uint32_t>(0U, 1U, 9U));
  EXPECT_EQ(1U, InclusiveClamp<uint32_t>(1U, 1U, 9U));
  EXPECT_EQ(5U, InclusiveClamp<uint32_t>(5U, 1U, 9U));
  EXPECT_EQ(8U, InclusiveClamp<uint32_t>(8U, 1U, 9U));
  EXPECT_EQ(9U, InclusiveClamp<uint32_t>(9U, 1U, 9U));
  EXPECT_EQ(9U, InclusiveClamp<uint32_t>(10U, 1U, 9U));
  EXPECT_EQ(9U, InclusiveClamp<uint32_t>(0xFFFFFFFFU, 1U, 9U));
  EXPECT_EQ(42U, InclusiveClamp<uint32_t>(0U, 42U, 42U));
  EXPECT_EQ(42U, InclusiveClamp<uint32_t>(41U, 42U, 42U));
  EXPECT_EQ(42U, InclusiveClamp<uint32_t>(42U, 42U, 42U));
  EXPECT_EQ(42U, InclusiveClamp<uint32_t>(43U, 42U, 42U));
  EXPECT_EQ(0U, InclusiveClamp<uint32_t>(0U, 0U, 0U));
  EXPECT_EQ(0xFFFFFFFF,
            InclusiveClamp<uint32_t>(0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF));
}

TEST(AlgorithmTest, AlignCeil) {
  EXPECT_EQ(0U, AlignCeil<uint32_t>(0U, 2U));
  EXPECT_EQ(2U, AlignCeil<uint32_t>(1U, 2U));
  EXPECT_EQ(2U, AlignCeil<uint32_t>(2U, 2U));
  EXPECT_EQ(4U, AlignCeil<uint32_t>(3U, 2U));
  EXPECT_EQ(4U, AlignCeil<uint32_t>(4U, 2U));
  EXPECT_EQ(11U, AlignCeil<uint32_t>(10U, 11U));
  EXPECT_EQ(11U, AlignCeil<uint32_t>(11U, 11U));
  EXPECT_EQ(22U, AlignCeil<uint32_t>(12U, 11U));
  EXPECT_EQ(22U, AlignCeil<uint32_t>(21U, 11U));
  EXPECT_EQ(22U, AlignCeil<uint32_t>(22U, 11U));
  EXPECT_EQ(33U, AlignCeil<uint32_t>(23U, 11U));
}

TEST(AlgorithmTest, IncrementForAlignCeil) {
  struct TestCase {
    int exp;  // Increment to |pos| to get the next nearest aligned value.
    int pos;
  };
  TestCase kTestCases2[] = {
      {0, 0},    {1, 1},    {0, 2},   {1, 3},   {0, 4},   {1, 5},
      {1, 97},   {0, 98},   {1, 99},  {0, 100}, {1, -1},  {0, -2},
      {1, -101}, {0, -100}, {1, -99}, {0, -98}, {1, -97}, {0, -96},
  };
  for (const auto& test_case : kTestCases2) {
    EXPECT_EQ(test_case.exp, IncrementForAlignCeil2<int32_t>(test_case.pos));
    if (test_case.pos >= 0)
      EXPECT_EQ(test_case.exp, IncrementForAlignCeil2<uint32_t>(test_case.pos));
  }
  TestCase kTestCases4[] = {
      {0, 0},    {3, 1},    {2, 2},   {1, 3},   {0, 4},   {3, 5},
      {3, 97},   {2, 98},   {1, 99},  {0, 100}, {1, -1},  {2, -2},
      {1, -101}, {0, -100}, {3, -99}, {2, -98}, {1, -97}, {0, -96},
  };
  for (const auto& test_case : kTestCases4) {
    EXPECT_EQ(test_case.exp, IncrementForAlignCeil4<int32_t>(test_case.pos));
    if (test_case.pos >= 0)
      EXPECT_EQ(test_case.exp, IncrementForAlignCeil4<uint32_t>(test_case.pos));
  }
}

TEST(AlgorithmTest, GetBit) {
  // 0xC5 = 0b1100'0101.
  constexpr uint8_t v = 0xC5;
  EXPECT_EQ(uint8_t(1), (GetBit<0>(v)));
  EXPECT_EQ(int8_t(0), (GetBit<1>(signed8(v))));
  EXPECT_EQ(uint8_t(1), (GetBit<2>(v)));
  EXPECT_EQ(int8_t(0), (GetBit<3>(signed8(v))));
  EXPECT_EQ(uint8_t(0), (GetBit<4>(v)));
  EXPECT_EQ(int8_t(0), (GetBit<5>(signed8(v))));
  EXPECT_EQ(uint8_t(1), (GetBit<6>(v)));
  EXPECT_EQ(int8_t(1), (GetBit<7>(signed8(v))));

  EXPECT_EQ(int16_t(1), (GetBit<3, int16_t>(0x0008)));
  EXPECT_EQ(uint16_t(0), (GetBit<14, uint16_t>(0xB000)));
  EXPECT_EQ(uint16_t(1), (GetBit<15, uint16_t>(0xB000)));

  EXPECT_EQ(uint32_t(1), (GetBit<0, uint32_t>(0xFFFFFFFF)));
  EXPECT_EQ(int32_t(1), (GetBit<31, int32_t>(0xFFFFFFFF)));

  EXPECT_EQ(uint32_t(0), (GetBit<0, uint32_t>(0xFF00A596)));
  EXPECT_EQ(int32_t(1), (GetBit<1, int32_t>(0xFF00A596)));
  EXPECT_EQ(uint32_t(1), (GetBit<4, uint32_t>(0xFF00A596)));
  EXPECT_EQ(int32_t(1), (GetBit<7, int32_t>(0xFF00A596)));
  EXPECT_EQ(uint32_t(0), (GetBit<9, uint32_t>(0xFF00A596)));
  EXPECT_EQ(int32_t(0), (GetBit<16, int32_t>(0xFF00A59)));
  EXPECT_EQ(uint32_t(1), (GetBit<24, uint32_t>(0xFF00A596)));
  EXPECT_EQ(int32_t(1), (GetBit<31, int32_t>(0xFF00A596)));

  EXPECT_EQ(uint64_t(0), (GetBit<62, uint64_t>(0xB000000000000000ULL)));
  EXPECT_EQ(int64_t(1), (GetBit<63, int64_t>(0xB000000000000000LL)));
}

TEST(AlgorithmTest, GetBits) {
  // Zero-extended: Basic cases for various values.
  uint32_t test_cases[] = {0, 1, 2, 7, 137, 0x10000, 0x69969669, 0xFFFFFFFF};
  for (uint32_t v : test_cases) {
    EXPECT_EQ(uint32_t(v & 0xFF), (GetUnsignedBits<0, 7>(v)));
    EXPECT_EQ(uint32_t((v >> 8) & 0xFF), (GetUnsignedBits<8, 15>(v)));
    EXPECT_EQ(uint32_t((v >> 16) & 0xFF), (GetUnsignedBits<16, 23>(v)));
    EXPECT_EQ(uint32_t((v >> 24) & 0xFF), (GetUnsignedBits<24, 31>(v)));
    EXPECT_EQ(uint32_t(v & 0xFFFF), (GetUnsignedBits<0, 15>(v)));
    EXPECT_EQ(uint32_t((v >> 1) & 0x3FFFFFFF), (GetUnsignedBits<1, 30>(v)));
    EXPECT_EQ(uint32_t((v >> 2) & 0x0FFFFFFF), (GetUnsignedBits<2, 29>(v)));
    EXPECT_EQ(uint32_t(v), (GetUnsignedBits<0, 31>(v)));
  }

  // Zero-extended: Reading off various nibbles.
  EXPECT_EQ(uint32_t(0x4), (GetUnsignedBits<20, 23>(0x00432100U)));
  EXPECT_EQ(uint32_t(0x43), (GetUnsignedBits<16, 23>(0x00432100)));
  EXPECT_EQ(uint32_t(0x432), (GetUnsignedBits<12, 23>(0x00432100U)));
  EXPECT_EQ(uint32_t(0x4321), (GetUnsignedBits<8, 23>(0x00432100)));
  EXPECT_EQ(uint32_t(0x321), (GetUnsignedBits<8, 19>(0x00432100U)));
  EXPECT_EQ(uint32_t(0x21), (GetUnsignedBits<8, 15>(0x00432100)));
  EXPECT_EQ(uint32_t(0x1), (GetUnsignedBits<8, 11>(0x00432100U)));

  // Sign-extended: 0x3CA5 = 0b0011'1100'1010'0101.
  EXPECT_EQ(signed16(0xFFFF), (GetSignedBits<0, 0>(0x3CA5U)));
  EXPECT_EQ(signed16(0x0001), (GetSignedBits<0, 1>(0x3CA5)));
  EXPECT_EQ(signed16(0xFFFD), (GetSignedBits<0, 2>(0x3CA5U)));
  EXPECT_EQ(signed16(0x0005), (GetSignedBits<0, 4>(0x3CA5)));
  EXPECT_EQ(signed16(0xFFA5), (GetSignedBits<0, 7>(0x3CA5U)));
  EXPECT_EQ(signed16(0xFCA5), (GetSignedBits<0, 11>(0x3CA5)));
  EXPECT_EQ(signed16(0x0005), (GetSignedBits<0, 3>(0x3CA5U)));
  EXPECT_EQ(signed16(0xFFFA), (GetSignedBits<4, 7>(0x3CA5)));
  EXPECT_EQ(signed16(0xFFFC), (GetSignedBits<8, 11>(0x3CA5U)));
  EXPECT_EQ(signed16(0x0003), (GetSignedBits<12, 15>(0x3CA5)));
  EXPECT_EQ(signed16(0x0000), (GetSignedBits<4, 4>(0x3CA5U)));
  EXPECT_EQ(signed16(0xFFFF), (GetSignedBits<5, 5>(0x3CA5)));
  EXPECT_EQ(signed16(0x0002), (GetSignedBits<4, 6>(0x3CA5U)));
  EXPECT_EQ(signed16(0x1E52), (GetSignedBits<1, 14>(0x3CA5)));
  EXPECT_EQ(signed16(0xFF29), (GetSignedBits<2, 13>(0x3CA5U)));
  EXPECT_EQ(int32_t(0x00001E52), (GetSignedBits<1, 14>(0x3CA5)));
  EXPECT_EQ(int32_t(0xFFFFFF29), (GetSignedBits<2, 13>(0x3CA5U)));

  // 64-bits: Extract from middle 0x66 = 0b0110'0110.
  EXPECT_EQ(uint64_t(0x0000000000000009LL),
            (GetUnsignedBits<30, 33>(int64_t(0x2222222661111111LL))));
  EXPECT_EQ(int64_t(0xFFFFFFFFFFFFFFF9LL),
            (GetSignedBits<30, 33>(uint64_t(0x2222222661111111LL))));
}

TEST(AlgorithmTest, SignExtend) {
  // 0x6A = 0b0110'1010.
  EXPECT_EQ(uint8_t(0x00), (SignExtend<uint8_t>(0, 0x6A)));
  EXPECT_EQ(signed8(0xFE), (SignExtend<int8_t>(1, signed8(0x6A))));
  EXPECT_EQ(uint8_t(0x02), (SignExtend<uint8_t>(2, 0x6A)));
  EXPECT_EQ(signed8(0xFA), (SignExtend<int8_t>(3, signed8(0x6A))));
  EXPECT_EQ(uint8_t(0x0A), (SignExtend<uint8_t>(4, 0x6A)));
  EXPECT_EQ(signed8(0xEA), (SignExtend<int8_t>(5, signed8(0x6A))));
  EXPECT_EQ(uint8_t(0xEA), (SignExtend<uint8_t>(6, 0x6A)));
  EXPECT_EQ(signed8(0x6A), (SignExtend<int8_t>(7, signed8(0x6A))));

  EXPECT_EQ(signed16(0xFFFA), (SignExtend<int16_t>(3, 0x6A)));
  EXPECT_EQ(uint16_t(0x000A), (SignExtend<uint16_t>(4, 0x6A)));

  EXPECT_EQ(int32_t(0xFFFF8000), (SignExtend<int32_t>(15, 0x00008000)));
  EXPECT_EQ(uint32_t(0x00008000U), (SignExtend<uint32_t>(16, 0x00008000)));
  EXPECT_EQ(int32_t(0xFFFFFC00), (SignExtend<int32_t>(10, 0x00000400)));
  EXPECT_EQ(uint32_t(0xFFFFFFFFU), (SignExtend<uint32_t>(31, 0xFFFFFFFF)));

  EXPECT_EQ(int64_t(0xFFFFFFFFFFFFFE6ALL),
            (SignExtend<int64_t>(9, 0x000000000000026ALL)));
  EXPECT_EQ(int64_t(0x000000000000016ALL),
            (SignExtend<int64_t>(9, 0xFFFFFFFFFFFFFD6ALL)));
  EXPECT_EQ(uint64_t(0xFFFFFFFFFFFFFE6AULL),
            (SignExtend<uint64_t>(9, 0x000000000000026AULL)));
  EXPECT_EQ(uint64_t(0x000000000000016AULL),
            (SignExtend<uint64_t>(9, 0xFFFFFFFFFFFFFD6AULL)));
}

TEST(AlgorithmTest, SignExtendTemplated) {
  // 0x6A = 0b0110'1010.
  EXPECT_EQ(uint8_t(0x00), (SignExtend<0, uint8_t>(0x6A)));
  EXPECT_EQ(signed8(0xFE), (SignExtend<1, int8_t>(signed8(0x6A))));
  EXPECT_EQ(uint8_t(0x02), (SignExtend<2, uint8_t>(0x6A)));
  EXPECT_EQ(signed8(0xFA), (SignExtend<3, int8_t>(signed8(0x6A))));
  EXPECT_EQ(uint8_t(0x0A), (SignExtend<4, uint8_t>(0x6A)));
  EXPECT_EQ(signed8(0xEA), (SignExtend<5, int8_t>(signed8(0x6A))));
  EXPECT_EQ(uint8_t(0xEA), (SignExtend<6, uint8_t>(0x6A)));
  EXPECT_EQ(signed8(0x6A), (SignExtend<7, int8_t>(signed8(0x6A))));

  EXPECT_EQ(signed16(0xFFFA), (SignExtend<3, int16_t>(0x6A)));
  EXPECT_EQ(uint16_t(0x000A), (SignExtend<4, uint16_t>(0x6A)));

  EXPECT_EQ(int32_t(0xFFFF8000), (SignExtend<15, int32_t>(0x00008000)));
  EXPECT_EQ(uint32_t(0x00008000U), (SignExtend<16, uint32_t>(0x00008000)));
  EXPECT_EQ(int32_t(0xFFFFFC00), (SignExtend<10, int32_t>(0x00000400)));
  EXPECT_EQ(uint32_t(0xFFFFFFFFU), (SignExtend<31, uint32_t>(0xFFFFFFFF)));

  EXPECT_EQ(int64_t(0xFFFFFFFFFFFFFE6ALL),
            (SignExtend<9, int64_t>(0x000000000000026ALL)));
  EXPECT_EQ(int64_t(0x000000000000016ALL),
            (SignExtend<9, int64_t>(0xFFFFFFFFFFFFFD6ALL)));
  EXPECT_EQ(uint64_t(0xFFFFFFFFFFFFFE6AULL),
            (SignExtend<9, uint64_t>(0x000000000000026AULL)));
  EXPECT_EQ(uint64_t(0x000000000000016AULL),
            (SignExtend<9, uint64_t>(0xFFFFFFFFFFFFFD6AULL)));
}

TEST(AlgorithmTest, SignedFit) {
  for (int v = -0x80; v < 0x80; ++v) {
    EXPECT_EQ(v >= -1 && v < 1, (SignedFit<1, int8_t>(v)));
    EXPECT_EQ(v >= -1 && v < 1, (SignedFit<1, uint8_t>(v)));
    EXPECT_EQ(v >= -2 && v < 2, (SignedFit<2, int8_t>(v)));
    EXPECT_EQ(v >= -4 && v < 4, (SignedFit<3, uint8_t>(v)));
    EXPECT_EQ(v >= -8 && v < 8, (SignedFit<4, int16_t>(v)));
    EXPECT_EQ(v >= -16 && v < 16, (SignedFit<5, uint32_t>(v)));
    EXPECT_EQ(v >= -32 && v < 32, (SignedFit<6, int32_t>(v)));
    EXPECT_EQ(v >= -64 && v < 64, (SignedFit<7, uint64_t>(v)));
    EXPECT_TRUE((SignedFit<8, int8_t>(v)));
    EXPECT_TRUE((SignedFit<8, uint8_t>(v)));
  }

  EXPECT_TRUE((SignedFit<16, uint32_t>(0x00000000)));
  EXPECT_TRUE((SignedFit<16, uint32_t>(0x00007FFF)));
  EXPECT_TRUE((SignedFit<16, uint32_t>(0xFFFF8000)));
  EXPECT_TRUE((SignedFit<16, uint32_t>(0xFFFFFFFF)));
  EXPECT_TRUE((SignedFit<16, int32_t>(0x00007FFF)));
  EXPECT_TRUE((SignedFit<16, int32_t>(0xFFFF8000)));

  EXPECT_FALSE((SignedFit<16, uint32_t>(0x80000000)));
  EXPECT_FALSE((SignedFit<16, uint32_t>(0x7FFFFFFF)));
  EXPECT_FALSE((SignedFit<16, uint32_t>(0x00008000)));
  EXPECT_FALSE((SignedFit<16, uint32_t>(0xFFFF7FFF)));
  EXPECT_FALSE((SignedFit<16, int32_t>(0x00008000)));
  EXPECT_FALSE((SignedFit<16, int32_t>(0xFFFF7FFF)));

  EXPECT_TRUE((SignedFit<48, int64_t>(0x00007FFFFFFFFFFFLL)));
  EXPECT_TRUE((SignedFit<48, int64_t>(0xFFFF800000000000LL)));
  EXPECT_FALSE((SignedFit<48, int64_t>(0x0008000000000000LL)));
  EXPECT_FALSE((SignedFit<48, int64_t>(0xFFFF7FFFFFFFFFFFLL)));
}

}  // namespace zucchini
