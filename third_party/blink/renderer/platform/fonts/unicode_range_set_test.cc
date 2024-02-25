// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/unicode_range_set.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

static const UChar kHiraganaA[2] = {0x3042, 0};

TEST(UnicodeRangeSet, Empty) {
  HeapVector<UnicodeRange> ranges;
  UnicodeRangeSet* set =
      MakeGarbageCollected<UnicodeRangeSet>(std::move(ranges));
  EXPECT_TRUE(set->IsEntireRange());
  EXPECT_EQ(0u, set->size());
  EXPECT_FALSE(set->IntersectsWith(String()));
  EXPECT_TRUE(set->IntersectsWith(String("a")));
  EXPECT_TRUE(set->IntersectsWith(String(kHiraganaA)));
}

TEST(UnicodeRangeSet, SingleCharacter) {
  HeapVector<UnicodeRange> ranges;
  ranges.push_back(UnicodeRange('b', 'b'));
  UnicodeRangeSet* set =
      MakeGarbageCollected<UnicodeRangeSet>(std::move(ranges));
  EXPECT_FALSE(set->IsEntireRange());
  EXPECT_FALSE(set->IntersectsWith(String()));
  EXPECT_FALSE(set->IntersectsWith(String("a")));
  EXPECT_TRUE(set->IntersectsWith(String("b")));
  EXPECT_FALSE(set->IntersectsWith(String("c")));
  EXPECT_TRUE(set->IntersectsWith(String("abc")));
  EXPECT_FALSE(set->IntersectsWith(String(kHiraganaA)));
  ASSERT_EQ(1u, set->size());
  EXPECT_EQ('b', set->RangeAt(0).From());
  EXPECT_EQ('b', set->RangeAt(0).To());
}

TEST(UnicodeRangeSet, TwoRanges) {
  HeapVector<UnicodeRange> ranges;
  ranges.push_back(UnicodeRange('6', '7'));
  ranges.push_back(UnicodeRange('2', '4'));
  UnicodeRangeSet* set =
      MakeGarbageCollected<UnicodeRangeSet>(std::move(ranges));
  EXPECT_FALSE(set->IsEntireRange());
  EXPECT_FALSE(set->IntersectsWith(String()));
  EXPECT_FALSE(set->IntersectsWith(String("1")));
  EXPECT_TRUE(set->IntersectsWith(String("2")));
  EXPECT_TRUE(set->IntersectsWith(String("3")));
  EXPECT_TRUE(set->IntersectsWith(String("4")));
  EXPECT_FALSE(set->IntersectsWith(String("5")));
  EXPECT_TRUE(set->IntersectsWith(String("6")));
  EXPECT_TRUE(set->IntersectsWith(String("7")));
  EXPECT_FALSE(set->IntersectsWith(String("8")));
  ASSERT_EQ(2u, set->size());
  EXPECT_EQ('2', set->RangeAt(0).From());
  EXPECT_EQ('4', set->RangeAt(0).To());
  EXPECT_EQ('6', set->RangeAt(1).From());
  EXPECT_EQ('7', set->RangeAt(1).To());
}

TEST(UnicodeRangeSet, Overlap) {
  HeapVector<UnicodeRange> ranges;
  ranges.push_back(UnicodeRange('0', '2'));
  ranges.push_back(UnicodeRange('1', '1'));
  ranges.push_back(UnicodeRange('3', '5'));
  ranges.push_back(UnicodeRange('4', '6'));
  UnicodeRangeSet* set =
      MakeGarbageCollected<UnicodeRangeSet>(std::move(ranges));
  ASSERT_EQ(1u, set->size());
  EXPECT_EQ('0', set->RangeAt(0).From());
  EXPECT_EQ('6', set->RangeAt(0).To());
}

TEST(UnicodeRangeSet, Non8Bit) {
  HeapVector<UnicodeRange> ranges;
  ranges.push_back(UnicodeRange(0x3042, 0x3042));
  UnicodeRangeSet* set =
      MakeGarbageCollected<UnicodeRangeSet>(std::move(ranges));
  ASSERT_EQ(1u, set->size());
  EXPECT_EQ(0x3042, set->RangeAt(0).From());
  EXPECT_EQ(0x3042, set->RangeAt(0).To());
  EXPECT_FALSE(set->IntersectsWith(String("a")));
  EXPECT_TRUE(set->IntersectsWith(String(kHiraganaA)));
}

}  // namespace blink
