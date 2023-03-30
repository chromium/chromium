// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/bloom_filter.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace WTF {

class BloomFilterTest : public ::testing::Test {
 protected:
  template <unsigned keyBits>
  size_t BloomFilterBitArrayIndex(unsigned key) {
    return BloomFilter<keyBits>::BitArrayIndex(key);
  }

  template <unsigned keyBits>
  unsigned BloomFilterBitMask(unsigned key) {
    return BloomFilter<keyBits>::BitMask(key);
  }

  template <unsigned keyBits>
  void TestBloomFilterKeyBoundary() {
    BloomFilter<keyBits> filter;

    filter.Add(0);
    EXPECT_TRUE(filter.MayContain(0));
    const unsigned max_key_bits = BloomFilter<keyBits>::kMaxKeyBits;
    static_assert(max_key_bits + keyBits <= sizeof(unsigned) * 8);
    for (unsigned i = max_key_bits; i < max_key_bits + keyBits; i++) {
      unsigned hash = 1u << i;
      EXPECT_FALSE(filter.MayContain(hash)) << String::Format(
          "BloomFilter<%d>.Add(0) Must not contain 0x%08x", keyBits, hash);
    }
  }
};

TEST_F(BloomFilterTest, NonCountingBloomFilterBitArrayIndexTest) {
  EXPECT_EQ(BloomFilterBitArrayIndex<12>(0x00000000), 0u);
  EXPECT_EQ(BloomFilterBitArrayIndex<12>(0x0000001f), 0u);
  EXPECT_EQ(BloomFilterBitArrayIndex<12>(0xfffff000), 0u);
  EXPECT_EQ(BloomFilterBitArrayIndex<12>(0xfffff01f), 0u);
  EXPECT_EQ(BloomFilterBitArrayIndex<12>(0x00000020), 1u);
  EXPECT_EQ(BloomFilterBitArrayIndex<12>(0x0000003f), 1u);
  EXPECT_EQ(BloomFilterBitArrayIndex<12>(0xfffff020), 1u);
  EXPECT_EQ(BloomFilterBitArrayIndex<12>(0xfffff03f), 1u);
  EXPECT_EQ(BloomFilterBitArrayIndex<12>(0x00000800), 64u);
  EXPECT_EQ(BloomFilterBitArrayIndex<12>(0x0000081f), 64u);
  EXPECT_EQ(BloomFilterBitArrayIndex<12>(0xfffff800), 64u);
  EXPECT_EQ(BloomFilterBitArrayIndex<12>(0xfffff81f), 64u);
  EXPECT_EQ(BloomFilterBitArrayIndex<12>(0x00000ff8), 127u);
  EXPECT_EQ(BloomFilterBitArrayIndex<12>(0x00000fff), 127u);
  EXPECT_EQ(BloomFilterBitArrayIndex<12>(0xfffffff8), 127u);
  EXPECT_EQ(BloomFilterBitArrayIndex<12>(0xffffffff), 127u);
}

TEST_F(BloomFilterTest, NonCountingBloomFilterBitMaskTest) {
  EXPECT_EQ(BloomFilterBitMask<12>(0x00000000), 0x00000001u);
  EXPECT_EQ(BloomFilterBitMask<12>(0xffffffc0), 0x00000001u);
  EXPECT_EQ(BloomFilterBitMask<12>(0x00000001), 0x00000002u);
  EXPECT_EQ(BloomFilterBitMask<12>(0xffffffc1), 0x00000002u);
  EXPECT_EQ(BloomFilterBitMask<12>(0x0000000f), 0x00008000u);
  EXPECT_EQ(BloomFilterBitMask<12>(0xffffff0f), 0x00008000u);
  EXPECT_EQ(BloomFilterBitMask<12>(0x0000003e), 0x40000000u);
  EXPECT_EQ(BloomFilterBitMask<12>(0xfffffffe), 0x40000000u);
  EXPECT_EQ(BloomFilterBitMask<12>(0x0000003f), 0x80000000u);
  EXPECT_EQ(BloomFilterBitMask<12>(0xffffffff), 0x80000000u);
}

TEST_F(BloomFilterTest, NonCountingBloomFilterKeyBoundary) {
  TestBloomFilterKeyBoundary<12>();
  TestBloomFilterKeyBoundary<13>();
  TestBloomFilterKeyBoundary<14>();
  TestBloomFilterKeyBoundary<15>();
  TestBloomFilterKeyBoundary<16>();
}

TEST_F(BloomFilterTest, NonCountingBloomFilterBasic) {
  unsigned alfa = AtomicString("Alfa").Hash();
  unsigned bravo = AtomicString("Bravo").Hash();
  unsigned charlie = AtomicString("Charlie").Hash();

  BloomFilter<12> filter;
  EXPECT_FALSE(filter.MayContain(alfa));
  EXPECT_FALSE(filter.MayContain(bravo));
  EXPECT_FALSE(filter.MayContain(charlie));

  filter.Add(alfa);
  EXPECT_TRUE(filter.MayContain(alfa));
  EXPECT_FALSE(filter.MayContain(bravo));
  EXPECT_FALSE(filter.MayContain(charlie));

  filter.Add(bravo);
  EXPECT_TRUE(filter.MayContain(alfa));
  EXPECT_TRUE(filter.MayContain(bravo));
  EXPECT_FALSE(filter.MayContain(charlie));

  filter.Add(charlie);
  EXPECT_TRUE(filter.MayContain(alfa));
  EXPECT_TRUE(filter.MayContain(bravo));
  EXPECT_TRUE(filter.MayContain(charlie));

  filter.Clear();
  EXPECT_FALSE(filter.MayContain(alfa));
  EXPECT_FALSE(filter.MayContain(bravo));
  EXPECT_FALSE(filter.MayContain(charlie));
}

}  // namespace WTF
