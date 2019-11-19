// Copyright 2017 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "absl/random/internal/fast_uniform_bits.h"

#include <random>

#include "gtest/gtest.h"

namespace absl {
namespace random_internal {
namespace {

template <typename IntType>
class FastUniformBitsTypedTest : public ::testing::Test {};

using IntTypes = ::testing::Types<uint8_t, uint16_t, uint32_t, uint64_t>;

TYPED_TEST_SUITE(FastUniformBitsTypedTest, IntTypes);

TYPED_TEST(FastUniformBitsTypedTest, BasicTest) {
  using Limits = std::numeric_limits<TypeParam>;
  using FastBits = FastUniformBits<TypeParam>;

  EXPECT_EQ(0, FastBits::min());
  EXPECT_EQ(Limits::max(), FastBits::max());

  constexpr int kIters = 10000;
  std::random_device rd;
  std::mt19937 gen(rd());
  FastBits fast;
  for (int i = 0; i < kIters; i++) {
    const auto v = fast(gen);
    EXPECT_LE(v, FastBits::max());
    EXPECT_GE(v, FastBits::min());
  }
}

template <typename UIntType, UIntType Lo, UIntType Hi, UIntType Val = Lo>
struct FakeUrbg {
  using result_type = UIntType;

  static constexpr result_type(max)() { return Hi; }
  static constexpr result_type(min)() { return Lo; }
  result_type operator()() { return Val; }
};

using UrngOddbits = FakeUrbg<uint8_t, 1, 0xfe, 0x73>;
using Urng4bits = FakeUrbg<uint8_t, 1, 0x10, 2>;
using Urng31bits = FakeUrbg<uint32_t, 1, 0xfffffffe, 0x60070f03>;
using Urng32bits = FakeUrbg<uint32_t, 0, 0xffffffff, 0x74010f01>;

TEST(FastUniformBitsTest, IsPowerOfTwoOrZero) {
  EXPECT_TRUE(IsPowerOfTwoOrZero(uint8_t{0}));
  EXPECT_TRUE(IsPowerOfTwoOrZero(uint8_t{1}));
  EXPECT_TRUE(IsPowerOfTwoOrZero(uint8_t{2}));
  EXPECT_FALSE(IsPowerOfTwoOrZero(uint8_t{3}));
  EXPECT_TRUE(IsPowerOfTwoOrZero(uint8_t{16}));
  EXPECT_FALSE(IsPowerOfTwoOrZero(uint8_t{17}));
  EXPECT_FALSE(IsPowerOfTwoOrZero((std::numeric_limits<uint8_t>::max)()));

  EXPECT_TRUE(IsPowerOfTwoOrZero(uint16_t{0}));
  EXPECT_TRUE(IsPowerOfTwoOrZero(uint16_t{1}));
  EXPECT_TRUE(IsPowerOfTwoOrZero(uint16_t{2}));
  EXPECT_FALSE(IsPowerOfTwoOrZero(uint16_t{3}));
  EXPECT_TRUE(IsPowerOfTwoOrZero(uint16_t{16}));
  EXPECT_FALSE(IsPowerOfTwoOrZero(uint16_t{17}));
  EXPECT_FALSE(IsPowerOfTwoOrZero((std::numeric_limits<uint16_t>::max)()));

  EXPECT_TRUE(IsPowerOfTwoOrZero(uint32_t{0}));
  EXPECT_TRUE(IsPowerOfTwoOrZero(uint32_t{1}));
  EXPECT_TRUE(IsPowerOfTwoOrZero(uint32_t{2}));
  EXPECT_FALSE(IsPowerOfTwoOrZero(uint32_t{3}));
  EXPECT_TRUE(IsPowerOfTwoOrZero(uint32_t{32}));
  EXPECT_FALSE(IsPowerOfTwoOrZero(uint32_t{17}));
  EXPECT_FALSE(IsPowerOfTwoOrZero((std::numeric_limits<uint32_t>::max)()));

  EXPECT_TRUE(IsPowerOfTwoOrZero(uint64_t{0}));
  EXPECT_TRUE(IsPowerOfTwoOrZero(uint64_t{1}));
  EXPECT_TRUE(IsPowerOfTwoOrZero(uint64_t{2}));
  EXPECT_FALSE(IsPowerOfTwoOrZero(uint64_t{3}));
  EXPECT_TRUE(IsPowerOfTwoOrZero(uint64_t{64}));
  EXPECT_FALSE(IsPowerOfTwoOrZero(uint64_t{17}));
  EXPECT_FALSE(IsPowerOfTwoOrZero((std::numeric_limits<uint64_t>::max)()));
}

TEST(FastUniformBitsTest, IntegerLog2) {
  EXPECT_EQ(IntegerLog2(uint16_t{0}), 0);
  EXPECT_EQ(IntegerLog2(uint16_t{1}), 0);
  EXPECT_EQ(IntegerLog2(uint16_t{2}), 1);
  EXPECT_EQ(IntegerLog2(uint16_t{3}), 1);
  EXPECT_EQ(IntegerLog2(uint16_t{4}), 2);
  EXPECT_EQ(IntegerLog2(uint16_t{5}), 2);
  EXPECT_EQ(IntegerLog2(std::numeric_limits<uint64_t>::max()), 63);
}

TEST(FastUniformBitsTest, RangeSize) {
  EXPECT_EQ((RangeSize<FakeUrbg<uint8_t, 0, 3>>()), 4);
  EXPECT_EQ((RangeSize<FakeUrbg<uint8_t, 2, 2>>()), 1);
  EXPECT_EQ((RangeSize<FakeUrbg<uint8_t, 2, 5>>()), 4);
  EXPECT_EQ((RangeSize<FakeUrbg<uint8_t, 2, 6>>()), 5);
  EXPECT_EQ((RangeSize<FakeUrbg<uint8_t, 2, 10>>()), 9);
  EXPECT_EQ(
      (RangeSize<FakeUrbg<uint8_t, 0, std::numeric_limits<uint8_t>::max()>>()),
      0);

  EXPECT_EQ((RangeSize<FakeUrbg<uint16_t, 0, 3>>()), 4);
  EXPECT_EQ((RangeSize<FakeUrbg<uint16_t, 2, 2>>()), 1);
  EXPECT_EQ((RangeSize<FakeUrbg<uint16_t, 2, 5>>()), 4);
  EXPECT_EQ((RangeSize<FakeUrbg<uint16_t, 2, 6>>()), 5);
  EXPECT_EQ((RangeSize<FakeUrbg<uint16_t, 1000, 1017>>()), 18);
  EXPECT_EQ((RangeSize<
                FakeUrbg<uint16_t, 0, std::numeric_limits<uint16_t>::max()>>()),
            0);

  EXPECT_EQ((RangeSize<FakeUrbg<uint32_t, 0, 3>>()), 4);
  EXPECT_EQ((RangeSize<FakeUrbg<uint32_t, 2, 2>>()), 1);
  EXPECT_EQ((RangeSize<FakeUrbg<uint32_t, 2, 5>>()), 4);
  EXPECT_EQ((RangeSize<FakeUrbg<uint32_t, 2, 6>>()), 5);
  EXPECT_EQ((RangeSize<FakeUrbg<uint32_t, 1000, 1017>>()), 18);
  EXPECT_EQ((RangeSize<FakeUrbg<uint32_t, 0, 0xffffffff>>()), 0);
  EXPECT_EQ((RangeSize<FakeUrbg<uint32_t, 1, 0xffffffff>>()), 0xffffffff);
  EXPECT_EQ((RangeSize<FakeUrbg<uint32_t, 1, 0xfffffffe>>()), 0xfffffffe);
  EXPECT_EQ((RangeSize<FakeUrbg<uint32_t, 2, 0xfffffffe>>()), 0xfffffffd);
  EXPECT_EQ((RangeSize<
                FakeUrbg<uint32_t, 0, std::numeric_limits<uint32_t>::max()>>()),
            0);

  EXPECT_EQ((RangeSize<FakeUrbg<uint64_t, 0, 3>>()), 4);
  EXPECT_EQ((RangeSize<FakeUrbg<uint64_t, 2, 2>>()), 1);
  EXPECT_EQ((RangeSize<FakeUrbg<uint64_t, 2, 5>>()), 4);
  EXPECT_EQ((RangeSize<FakeUrbg<uint64_t, 2, 6>>()), 5);
  EXPECT_EQ((RangeSize<FakeUrbg<uint64_t, 1000, 1017>>()), 18);
  EXPECT_EQ((RangeSize<FakeUrbg<uint64_t, 0, 0xffffffff>>()), 0x100000000ull);
  EXPECT_EQ((RangeSize<FakeUrbg<uint64_t, 1, 0xffffffff>>()), 0xffffffffull);
  EXPECT_EQ((RangeSize<FakeUrbg<uint64_t, 1, 0xfffffffe>>()), 0xfffffffeull);
  EXPECT_EQ((RangeSize<FakeUrbg<uint64_t, 2, 0xfffffffe>>()), 0xfffffffdull);
  EXPECT_EQ((RangeSize<FakeUrbg<uint64_t, 0, 0xffffffffffffffffull>>()), 0ull);
  EXPECT_EQ((RangeSize<FakeUrbg<uint64_t, 1, 0xffffffffffffffffull>>()),
            0xffffffffffffffffull);
  EXPECT_EQ((RangeSize<FakeUrbg<uint64_t, 1, 0xfffffffffffffffeull>>()),
            0xfffffffffffffffeull);
  EXPECT_EQ((RangeSize<FakeUrbg<uint64_t, 2, 0xfffffffffffffffeull>>()),
            0xfffffffffffffffdull);
  EXPECT_EQ((RangeSize<
                FakeUrbg<uint64_t, 0, std::numeric_limits<uint64_t>::max()>>()),
            0);
}

TEST(FastUniformBitsTest, PowerOfTwoSubRangeSize) {
  EXPECT_EQ((PowerOfTwoSubRangeSize<FakeUrbg<uint8_t, 0, 3>>()), 4);
  EXPECT_EQ((PowerOfTwoSubRangeSize<FakeUrbg<uint8_t, 2, 2>>()), 1);
  EXPECT_EQ((PowerOfTwoSubRangeSize<FakeUrbg<uint8_t, 2, 5>>()), 4);
  EXPECT_EQ((PowerOfTwoSubRangeSize<FakeUrbg<uint8_t, 2, 6>>()), 4);
  EXPECT_EQ((PowerOfTwoSubRangeSize<FakeUrbg<uint8_t, 2, 10>>()), 8);
  EXPECT_EQ((PowerOfTwoSubRangeSize<
                FakeUrbg<uint8_t, 0, std::numeric_limits<uint8_t>::max()>>()),
            0);

  EXPECT_EQ((PowerOfTwoSubRangeSize<FakeUrbg<uint16_t, 0, 3>>()), 4);
  EXPECT_EQ((PowerOfTwoSubRangeSize<FakeUrbg<uint16_t, 2, 2>>()), 1);
  EXPECT_EQ((PowerOfTwoSubRangeSize<FakeUrbg<uint16_t, 2, 5>>()), 4);
  EXPECT_EQ((PowerOfTwoSubRangeSize<FakeUrbg<uint16_t, 2, 6>>()), 4);
  EXPECT_EQ((PowerOfTwoSubRangeSize<FakeUrbg<uint16_t, 1000, 1017>>()), 16);
  EXPECT_EQ((PowerOfTwoSubRangeSize<
                FakeUrbg<uint16_t, 0, std::numeric_limits<uint16_t>::max()>>()),
            0);

  EXPECT_EQ((PowerOfTwoSubRangeSize<FakeUrbg<uint32_t, 0, 3>>()), 4);
  EXPECT_EQ((PowerOfTwoSubRangeSize<FakeUrbg<uint32_t, 2, 2>>()), 1);
  EXPECT_EQ((PowerOfTwoSubRangeSize<FakeUrbg<uint32_t, 2, 5>>()), 4);
  EXPECT_EQ((PowerOfTwoSubRangeSize<FakeUrbg<uint32_t, 2, 6>>()), 4);
  EXPECT_EQ((PowerOfTwoSubRangeSize<FakeUrbg<uint32_t, 1000, 1017>>()), 16);
  EXPECT_EQ((PowerOfTwoSubRangeSize<FakeUrbg<uint32_t, 0, 0xffffffff>>()), 0);
  EXPECT_EQ((PowerOfTwoSubRangeSize<FakeUrbg<uint32_t, 1, 0xffffffff>>()),
            0x80000000);
  EXPECT_EQ((PowerOfTwoSubRangeSize<FakeUrbg<uint32_t, 1, 0xfffffffe>>()),
            0x80000000);
  EXPECT_EQ((PowerOfTwoSubRangeSize<
                FakeUrbg<uint32_t, 0, std::numeric_limits<uint32_t>::max()>>()),
            0);

  EXPECT_EQ((PowerOfTwoSubRangeSize<FakeUrbg<uint64_t, 0, 3>>()), 4);
  EXPECT_EQ((PowerOfTwoSubRangeSize<FakeUrbg<uint64_t, 2, 2>>()), 1);
  EXPECT_EQ((PowerOfTwoSubRangeSize<FakeUrbg<uint64_t, 2, 5>>()), 4);
  EXPECT_EQ((PowerOfTwoSubRangeSize<FakeUrbg<uint64_t, 2, 6>>()), 4);
  EXPECT_EQ((PowerOfTwoSubRangeSize<FakeUrbg<uint64_t, 1000, 1017>>()), 16);
  EXPECT_EQ((PowerOfTwoSubRangeSize<FakeUrbg<uint64_t, 0, 0xffffffff>>()),
            0x100000000ull);
  EXPECT_EQ((PowerOfTwoSubRangeSize<FakeUrbg<uint64_t, 1, 0xffffffff>>()),
            0x80000000ull);
  EXPECT_EQ((PowerOfTwoSubRangeSize<FakeUrbg<uint64_t, 1, 0xfffffffe>>()),
            0x80000000ull);
  EXPECT_EQ(
      (PowerOfTwoSubRangeSize<FakeUrbg<uint64_t, 0, 0xffffffffffffffffull>>()),
      0);
  EXPECT_EQ(
      (PowerOfTwoSubRangeSize<FakeUrbg<uint64_t, 1, 0xffffffffffffffffull>>()),
      0x8000000000000000ull);
  EXPECT_EQ(
      (PowerOfTwoSubRangeSize<FakeUrbg<uint64_t, 1, 0xfffffffffffffffeull>>()),
      0x8000000000000000ull);
  EXPECT_EQ((PowerOfTwoSubRangeSize<
                FakeUrbg<uint64_t, 0, std::numeric_limits<uint64_t>::max()>>()),
            0);
}

TEST(FastUniformBitsTest, Urng4_VariousOutputs) {
  // Tests that how values are composed; the single-bit deltas should be spread
  // across each invocation.
  Urng4bits urng4;
  Urng31bits urng31;
  Urng32bits urng32;

  // 8-bit types
  {
    FastUniformBits<uint8_t> fast8;
    EXPECT_EQ(0x11, fast8(urng4));
    EXPECT_EQ(0x2, fast8(urng31));
    EXPECT_EQ(0x1, fast8(urng32));
  }

  // 16-bit types
  {
    FastUniformBits<uint16_t> fast16;
    EXPECT_EQ(0x1111, fast16(urng4));
    EXPECT_EQ(0xf02, fast16(urng31));
    EXPECT_EQ(0xf01, fast16(urng32));
  }

  // 32-bit types
  {
    FastUniformBits<uint32_t> fast32;
    EXPECT_EQ(0x11111111, fast32(urng4));
    EXPECT_EQ(0x0f020f02, fast32(urng31));
    EXPECT_EQ(0x74010f01, fast32(urng32));
  }

  // 64-bit types
  {
    FastUniformBits<uint64_t> fast64;
    EXPECT_EQ(0x1111111111111111, fast64(urng4));
    EXPECT_EQ(0x387811c3c0870f02, fast64(urng31));
    EXPECT_EQ(0x74010f0174010f01, fast64(urng32));
  }
}

TEST(FastUniformBitsTest, URBG32bitRegression) {
  // Validate with deterministic 32-bit std::minstd_rand
  // to ensure that operator() performs as expected.
  std::minstd_rand gen(1);
  FastUniformBits<uint64_t> fast64;

  EXPECT_EQ(0x05e47095f847c122ull, fast64(gen));
  EXPECT_EQ(0x8f82c1ba30b64d22ull, fast64(gen));
  EXPECT_EQ(0x3b971a3558155039ull, fast64(gen));
}

}  // namespace
}  // namespace random_internal
}  // namespace absl
