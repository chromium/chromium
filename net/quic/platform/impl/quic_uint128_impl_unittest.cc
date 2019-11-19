// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <limits>

#include "net/quic/platform/impl/quic_uint128_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace quic {

namespace test {

constexpr QuicUint128Impl kuint128max(std::numeric_limits<uint64_t>::max(),
                                      std::numeric_limits<uint64_t>::max());

TEST(Int128, AllTests) {
  QuicUint128Impl zero(0);
  QuicUint128Impl one(1);
  QuicUint128Impl one_2arg(0, 1);
  QuicUint128Impl two(0, 2);
  QuicUint128Impl three(0, 3);
  QuicUint128Impl big(2000, 2);
  QuicUint128Impl big_minus_one(2000, 1);
  QuicUint128Impl bigger(2001, 1);
  QuicUint128Impl biggest(kuint128max);
  QuicUint128Impl high_low(1, 0);
  QuicUint128Impl low_high(0, std::numeric_limits<uint64_t>::max());
  EXPECT_LT(one, two);
  EXPECT_GT(two, one);
  EXPECT_LT(one, big);
  EXPECT_LT(one, big);
  EXPECT_EQ(one, one_2arg);
  EXPECT_NE(one, two);
  EXPECT_GT(big, one);
  EXPECT_GE(big, two);
  EXPECT_GE(big, big_minus_one);
  EXPECT_GT(big, big_minus_one);
  EXPECT_LT(big_minus_one, big);
  EXPECT_LE(big_minus_one, big);
  EXPECT_NE(big_minus_one, big);
  EXPECT_LT(big, biggest);
  EXPECT_LE(big, biggest);
  EXPECT_GT(biggest, big);
  EXPECT_GE(biggest, big);
  EXPECT_EQ(big, ~~big);
  EXPECT_EQ(one, one | one);
  EXPECT_EQ(big, big | big);
  EXPECT_EQ(one, one | zero);
  EXPECT_EQ(one, one & one);
  EXPECT_EQ(big, big & big);
  EXPECT_EQ(zero, one & zero);
  EXPECT_EQ(zero, big & ~big);
  EXPECT_EQ(zero, one ^ one);
  EXPECT_EQ(zero, big ^ big);
  EXPECT_EQ(one, one ^ zero);
  EXPECT_EQ(big, big << 0);
  EXPECT_EQ(big, big >> 0);
  EXPECT_GT(big << 1, big);
  EXPECT_LT(big >> 1, big);
  EXPECT_EQ(big, (big << 10) >> 10);
  EXPECT_EQ(big, (big >> 1) << 1);
  EXPECT_EQ(one, (one << 80) >> 80);
  EXPECT_EQ(zero, (one >> 80) << 80);
  EXPECT_EQ(zero, big >> 128);
  EXPECT_EQ(zero, big << 128);
  EXPECT_EQ(QuicUint128High64Impl(biggest),
            std::numeric_limits<uint64_t>::max());
  EXPECT_EQ(QuicUint128Low64Impl(biggest),
            std::numeric_limits<uint64_t>::max());
  EXPECT_EQ(zero + one, one);
  EXPECT_EQ(one + one, two);
  EXPECT_EQ(big_minus_one + one, big);
  EXPECT_EQ(one - one, zero);
  EXPECT_EQ(one - zero, one);
  EXPECT_EQ(zero - one, biggest);
  EXPECT_EQ(big - big, zero);
  EXPECT_EQ(big - one, big_minus_one);
  EXPECT_EQ(big + std::numeric_limits<uint64_t>::max(), bigger);
  EXPECT_EQ(biggest + 1, zero);
  EXPECT_EQ(zero - 1, biggest);
  EXPECT_EQ(high_low - one, low_high);
  EXPECT_EQ(low_high + one, high_low);
  EXPECT_EQ(QuicUint128High64Impl((QuicUint128Impl(1) << 64) - 1), 0u);
  EXPECT_EQ(QuicUint128Low64Impl((QuicUint128Impl(1) << 64) - 1),
            std::numeric_limits<uint64_t>::max());
  EXPECT_TRUE(!!one);
  EXPECT_TRUE(!!high_low);
  EXPECT_FALSE(!!zero);
  EXPECT_FALSE(!one);
  EXPECT_FALSE(!high_low);
  EXPECT_TRUE(!zero);
  EXPECT_TRUE(zero == 0);
  EXPECT_FALSE(zero != 0);
  EXPECT_FALSE(one == 0);
  EXPECT_TRUE(one != 0);

  QuicUint128Impl test = zero;
  EXPECT_EQ(++test, one);
  EXPECT_EQ(test, one);
  EXPECT_EQ(test++, one);
  EXPECT_EQ(test, two);
  EXPECT_EQ(test -= 2, zero);
  EXPECT_EQ(test, zero);
  EXPECT_EQ(test += 2, two);
  EXPECT_EQ(test, two);
  EXPECT_EQ(--test, one);
  EXPECT_EQ(test, one);
  EXPECT_EQ(test--, one);
  EXPECT_EQ(test, zero);
  EXPECT_EQ(test |= three, three);
  EXPECT_EQ(test &= one, one);
  EXPECT_EQ(test ^= three, two);
  EXPECT_EQ(test >>= 1, one);
  EXPECT_EQ(test <<= 1, two);

  EXPECT_EQ(big, -(-big));
  EXPECT_EQ(two, -((-one) - 1));
  EXPECT_EQ(kuint128max, -one);
  EXPECT_EQ(zero, -zero);
}

TEST(Int128, OperatorAssignReturnRef) {
  QuicUint128Impl v(1);
  (v += 4) -= 3;
  EXPECT_EQ(2, v);
}

TEST(Int128, Multiply) {
  QuicUint128Impl a, b, c;

  // Zero test.
  a = 0;
  b = 0;
  c = a * b;
  EXPECT_EQ(0, c);

  // Max carries.
  a = QuicUint128Impl(0) - 1;
  b = QuicUint128Impl(0) - 1;
  c = a * b;
  EXPECT_EQ(1, c);

  // Self-operation with max carries.
  c = QuicUint128Impl(0) - 1;
  c *= c;
  EXPECT_EQ(1, c);

  // 1-bit x 1-bit.
  for (int i = 0; i < 64; ++i) {
    for (int j = 0; j < 64; ++j) {
      a = QuicUint128Impl(1) << i;
      b = QuicUint128Impl(1) << j;
      c = a * b;
      EXPECT_EQ(QuicUint128Impl(1) << (i + j), c);
    }
  }

  // Verified with dc.
  a = QuicUint128Impl(0xffffeeeeddddccccULL, 0xbbbbaaaa99998888ULL);
  b = QuicUint128Impl(0x7777666655554444ULL, 0x3333222211110000ULL);
  c = a * b;
  EXPECT_EQ(QuicUint128Impl(0x530EDA741C71D4C3ULL, 0xBF25975319080000ULL), c);
  EXPECT_EQ(0, c - b * a);
  EXPECT_EQ(a * a - b * b, (a + b) * (a - b));

  // Verified with dc.
  a = QuicUint128Impl(0x0123456789abcdefULL, 0xfedcba9876543210ULL);
  b = QuicUint128Impl(0x02468ace13579bdfULL, 0xfdb97531eca86420ULL);
  c = a * b;
  EXPECT_EQ(QuicUint128Impl(0x97a87f4f261ba3f2ULL, 0x342d0bbf48948200ULL), c);
  EXPECT_EQ(0, c - b * a);
  EXPECT_EQ(a * a - b * b, (a + b) * (a - b));
}

TEST(Int128, AliasTests) {
  QuicUint128Impl x1(1, 2);
  QuicUint128Impl x2(2, 4);
  x1 += x1;
  EXPECT_EQ(x2, x1);

  QuicUint128Impl x3(1, 1ull << 63);
  QuicUint128Impl x4(3, 0);
  x3 += x3;
  EXPECT_EQ(x4, x3);
}

}  // namespace test

}  // namespace quic
