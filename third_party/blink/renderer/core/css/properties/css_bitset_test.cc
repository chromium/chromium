// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/css/properties/css_bitset.h"

#include <bitset>

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

template <size_t kBits>
std::bitset<kBits> ToStdBitsetUsingHas(const CSSBitsetBase<kBits>& bitset) {
  std::bitset<kBits> ret;
  for (size_t i = 0; i < kBits; ++i) {
    if (bitset.Has(static_cast<CSSPropertyID>(i))) {
      ret.set(i);
    }
  }
  return ret;
}

template <size_t kBits>
std::bitset<kBits> ToStdBitsetUsingIterator(
    const CSSBitsetBase<kBits>& bitset) {
  std::bitset<kBits> ret;
  for (CSSPropertyID id : bitset) {
    size_t bit = static_cast<size_t>(id);
    DCHECK(!ret.test(bit));
    ret.set(bit);
  }
  return ret;
}

// Creates a CSSBitsetBase with kBits capacity, sets the specified bits via
// CSSBitsetBase::Set, and then verifies that the correct bits are observed
// via both CSSBitsetBase::Has, and CSSBitsetBase::begin()/end().
template <size_t kBits>
void AssertBitset(const size_t* begin, const size_t* end) {
  std::bitset<kBits> expected;

  CSSBitsetBase<kBits> actual;
  EXPECT_FALSE(actual.HasAny());

  for (const size_t* b = begin; b != end; b++) {
    actual.Set(static_cast<CSSPropertyID>(*b));
    expected.set(*b);
  }

  EXPECT_EQ(expected, ToStdBitsetUsingHas(actual));
  EXPECT_EQ(expected, ToStdBitsetUsingIterator(actual));
}

template <size_t kBits>
void AssertBitset(std::initializer_list<size_t> bits) {
  AssertBitset<kBits>(bits.begin(), bits.end());
}

}  // namespace

TEST(CSSBitsetTest, BaseBitCount1) {
  static_assert(CSSBitsetBase<1>::kChunks == 1u, "Correct chunk count");
  AssertBitset<1>({});
  AssertBitset<1>({0});
}

TEST(CSSBitsetTest, BaseBitCount63) {
  static_assert(CSSBitsetBase<63>::kChunks == 1u, "Correct chunk count");
  AssertBitset<63>({});
  AssertBitset<63>({0});
  AssertBitset<63>({1});
  AssertBitset<63>({13});
  AssertBitset<63>({62});

  AssertBitset<63>({0, 1});
  AssertBitset<63>({0, 62});
  AssertBitset<63>({61, 62});
  AssertBitset<63>({0, 1, 13, 61, 62});
}

TEST(CSSBitsetTest, BaseBitCount64) {
  static_assert(CSSBitsetBase<64>::kChunks == 1u, "Correct chunk count");
  AssertBitset<64>({});
  AssertBitset<64>({0});
  AssertBitset<64>({1});
  AssertBitset<64>({13});
  AssertBitset<64>({63});

  AssertBitset<64>({0, 1});
  AssertBitset<64>({0, 63});
  AssertBitset<64>({62, 63});
  AssertBitset<64>({0, 1, 13, 62, 63});
}

TEST(CSSBitsetTest, BaseBitCount65) {
  static_assert(CSSBitsetBase<65>::kChunks == 2u, "Correct chunk count");
  AssertBitset<65>({});
  AssertBitset<65>({0});
  AssertBitset<65>({1});
  AssertBitset<65>({13});
  AssertBitset<65>({63});
  AssertBitset<65>({64});

  AssertBitset<65>({0, 1});
  AssertBitset<65>({0, 64});
  AssertBitset<65>({63, 64});
  AssertBitset<65>({0, 1, 13, 63, 64});
}

TEST(CSSBitsetTest, BaseBitCount127) {
  static_assert(CSSBitsetBase<127>::kChunks == 2u, "Correct chunk count");
  AssertBitset<127>({});
  AssertBitset<127>({0});
  AssertBitset<127>({1});
  AssertBitset<127>({13});
  AssertBitset<127>({125});
  AssertBitset<127>({126});

  AssertBitset<127>({0, 1});
  AssertBitset<127>({0, 126});
  AssertBitset<127>({125, 126});
  AssertBitset<127>({0, 1, 13, 125, 126});
}

TEST(CSSBitsetTest, BaseBitCount128) {
  static_assert(CSSBitsetBase<128>::kChunks == 2u, "Correct chunk count");
  AssertBitset<128>({});
  AssertBitset<128>({0});
  AssertBitset<128>({1});
  AssertBitset<128>({13});
  AssertBitset<128>({126});
  AssertBitset<128>({127});

  AssertBitset<128>({0, 1});
  AssertBitset<128>({0, 127});
  AssertBitset<128>({126, 127});
  AssertBitset<128>({0, 1, 13, 126, 127});
  AssertBitset<128>({0, 1, 13, 63, 64, 65, 126, 127});
}

TEST(CSSBitsetTest, BaseBitCount129) {
  static_assert(CSSBitsetBase<129>::kChunks == 3u, "Correct chunk count");
  AssertBitset<129>({});
  AssertBitset<129>({0});
  AssertBitset<129>({1});
  AssertBitset<129>({13});
  AssertBitset<129>({127});
  AssertBitset<129>({128});

  AssertBitset<129>({0, 1});
  AssertBitset<129>({0, 128});
  AssertBitset<129>({127, 128});
  AssertBitset<129>({0, 1, 13, 127, 128});
  AssertBitset<129>({0, 1, 13, 63, 64, 65, 127, 128});
}

TEST(CSSBitsetTest, AllBits) {
  std::vector<size_t> all_bits;
  for (size_t i = 0; i < kNumCSSProperties; ++i) {
    all_bits.push_back(i);
  }

  AssertBitset<1>(all_bits.data(), all_bits.data() + 1);
  AssertBitset<2>(all_bits.data(), all_bits.data() + 2);
  AssertBitset<63>(all_bits.data(), all_bits.data() + 63);
  AssertBitset<64>(all_bits.data(), all_bits.data() + 64);
  AssertBitset<65>(all_bits.data(), all_bits.data() + 65);
  AssertBitset<127>(all_bits.data(), all_bits.data() + 127);
  AssertBitset<128>(all_bits.data(), all_bits.data() + 128);
  AssertBitset<129>(all_bits.data(), all_bits.data() + 129);
}

TEST(CSSBitsetTest, NoBits) {
  size_t i = 0;
  AssertBitset<1>(&i, &i);
  AssertBitset<2>(&i, &i);
  AssertBitset<63>(&i, &i);
  AssertBitset<64>(&i, &i);
  AssertBitset<65>(&i, &i);
  AssertBitset<127>(&i, &i);
  AssertBitset<128>(&i, &i);
  AssertBitset<129>(&i, &i);
}

TEST(CSSBitsetTest, SingleBit) {
  for (size_t i = 0; i < 1; ++i) {
    AssertBitset<1>(&i, &i + 1);
  }

  for (size_t i = 0; i < 2; ++i) {
    AssertBitset<2>(&i, &i + 1);
  }

  for (size_t i = 0; i < 63; ++i) {
    AssertBitset<63>(&i, &i + 1);
  }

  for (size_t i = 0; i < 64; ++i) {
    AssertBitset<64>(&i, &i + 1);
  }

  for (size_t i = 0; i < 65; ++i) {
    AssertBitset<65>(&i, &i + 1);
  }

  for (size_t i = 0; i < 127; ++i) {
    AssertBitset<127>(&i, &i + 1);
  }

  for (size_t i = 0; i < 128; ++i) {
    AssertBitset<128>(&i, &i + 1);
  }

  for (size_t i = 0; i < 129; ++i) {
    AssertBitset<129>(&i, &i + 1);
  }
}

TEST(CSSBitsetTest, Default) {
  CSSBitset bitset;
  for (auto id : CSSPropertyIDList()) {
    EXPECT_FALSE(bitset.Has(id));
  }
  EXPECT_FALSE(bitset.HasAny());
}

TEST(CSSBitsetTest, SetAndHas) {
  CSSBitset bitset;
  EXPECT_FALSE(bitset.Has(CSSPropertyID::kVariable));
  EXPECT_FALSE(bitset.Has(CSSPropertyID::kWidth));
  EXPECT_FALSE(bitset.Has(CSSPropertyID::kHeight));
  bitset.Set(CSSPropertyID::kVariable);
  bitset.Set(CSSPropertyID::kWidth);
  bitset.Set(CSSPropertyID::kHeight);
  EXPECT_TRUE(bitset.Has(CSSPropertyID::kVariable));
  EXPECT_TRUE(bitset.Has(CSSPropertyID::kWidth));
  EXPECT_TRUE(bitset.Has(CSSPropertyID::kHeight));
}

TEST(CSSBitsetTest, Or) {
  CSSBitset bitset;
  EXPECT_FALSE(bitset.Has(CSSPropertyID::kWidth));
  bitset.Or(CSSPropertyID::kWidth, false);
  EXPECT_FALSE(bitset.Has(CSSPropertyID::kWidth));
  bitset.Or(CSSPropertyID::kWidth, true);
  EXPECT_TRUE(bitset.Has(CSSPropertyID::kWidth));
}

TEST(CSSBitsetTest, HasAny) {
  CSSBitset bitset;
  EXPECT_FALSE(bitset.HasAny());
  bitset.Set(CSSPropertyID::kVariable);
  EXPECT_TRUE(bitset.HasAny());
}

TEST(CSSBitsetTest, Reset) {
  CSSBitset bitset;
  EXPECT_FALSE(bitset.HasAny());
  bitset.Set(CSSPropertyID::kHeight);
  EXPECT_TRUE(bitset.HasAny());
  EXPECT_TRUE(bitset.Has(CSSPropertyID::kHeight));
  bitset.Reset();
  EXPECT_FALSE(bitset.HasAny());
  EXPECT_FALSE(bitset.Has(CSSPropertyID::kHeight));
}

TEST(CSSBitsetTest, Iterator) {
  CSSBitset actual;
  actual.Set(CSSPropertyID::kHeight);
  actual.Set(CSSPropertyID::kWidth);
  actual.Set(CSSPropertyID::kVariable);

  std::bitset<kNumCSSPropertyIDs> expected;
  expected.set(static_cast<size_t>(CSSPropertyID::kHeight));
  expected.set(static_cast<size_t>(CSSPropertyID::kWidth));
  expected.set(static_cast<size_t>(CSSPropertyID::kVariable));

  EXPECT_EQ(expected, ToStdBitsetUsingIterator(actual));
}

TEST(CSSBitsetTest, Equals) {
  CSSBitset b1;
  CSSBitset b2;
  EXPECT_EQ(b1, b2);

  for (CSSPropertyID id : CSSPropertyIDList()) {
    b1.Set(id);
    EXPECT_NE(b1, b2);

    b2.Set(id);
    EXPECT_EQ(b1, b2);
  }
}

TEST(CSSBitsetTest, Copy) {
  EXPECT_EQ(CSSBitset(), CSSBitset());

  CSSBitset b1;
  for (CSSPropertyID id : CSSPropertyIDList()) {
    CSSBitset b2;
    b1.Set(id);
    b2.Set(id);
    EXPECT_EQ(b1, CSSBitset(b1));
    EXPECT_EQ(b2, CSSBitset(b2));
  }
}

TEST(CSSBitsetTest, InitializerList) {
  for (CSSPropertyID id : CSSPropertyIDList()) {
    CSSBitset bitset({CSSPropertyID::kColor, id});
    EXPECT_TRUE(bitset.Has(CSSPropertyID::kColor));
    EXPECT_TRUE(bitset.Has(id));
  }
}

}  // namespace blink
