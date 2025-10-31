// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/text/character_break_iterator.h"

#include "base/containers/adapters.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class CharacterBreakIteratorTest : public testing::Test {};

TEST_F(CharacterBreakIteratorTest, Offsets16) {
  struct {
    const char16_t* source;
    // Break offsets, other than 0 and the source length.
    const std::initializer_list<int> offsets;
  } kCases[] = {
      {u"", {}},
      {u"a", {}},
      {u"ab", {1}},
      // e + Combining grave accent, z
      {u"e\u0300z", {2}},
      // a, an emoji ZWJ sequence, z
      {u"a\U0001F635\u200d\U0001f4ABz", {1, 6}},
  };
  for (const auto& test_case : kCases) {
    StringView text(test_case.source);
    SCOPED_TRACE("Source: " + text.Utf8());
    int length = static_cast<int>(text.length());
    CharacterBreakIterator iter(text);
    if (length == 0) {
      EXPECT_EQ(kTextBreakDone, iter.Next());
      EXPECT_EQ(kTextBreakDone, iter.Preceding(0));
      continue;
    }
    for (int expected_offset : test_case.offsets) {
      EXPECT_EQ(expected_offset, iter.Next());
    }
    EXPECT_EQ(length, iter.Next());

    int offset = length;
    for (int expected_offset : base::Reversed(test_case.offsets)) {
      offset = iter.Preceding(offset);
      EXPECT_EQ(expected_offset, offset);
    }
    EXPECT_EQ(0, iter.Preceding(offset));
  }
}

}  // namespace blink
