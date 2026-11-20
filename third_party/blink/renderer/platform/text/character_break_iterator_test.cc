// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/text/character_break_iterator.h"

#include <memory>
#include <ranges>
#include <string_view>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/numerics/safe_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/text/icu_error.h"
#include "third_party/blink/renderer/platform/text/text_break_iterator_internal_icu.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class CharacterBreakIteratorTest : public testing::Test {
 protected:
  // Returns an ICU iterator with `text` set.
  icu::BreakIterator& IcuIterator(base::span<const UChar> text) {
    if (!icu_iterator_) {
      IcuError status;
      icu_iterator_.reset(icu::BreakIterator::createCharacterInstance(
          CurrentTextBreakIcuLocale(), status));
      CHECK(U_SUCCESS(status));
    }
    SetText16(icu_iterator_.get(), text);
    return *icu_iterator_;
  }

  // Checks that `CharacterBreakIterator` returns the same as ICU.
  void ExpectSameAsIcu(const StringView& text) {
    String text16 = text.ToString();
    text16.Ensure16Bit();
    icu::BreakIterator& icu_iterator = IcuIterator(text16.Span16());
    const int length = base::checked_cast<int>(text.length());

    CharacterBreakIterator iterator(text);
    for (int offset = -1; offset <= length + 1; ++offset) {
      EXPECT_EQ(icu_iterator.isBoundary(offset), iterator.IsBreak(offset))
          << offset;
      EXPECT_EQ(icu_iterator.preceding(offset), iterator.Preceding(offset))
          << offset;
      EXPECT_EQ(icu_iterator.following(offset), iterator.Following(offset))
          << offset;
    }
    // Check `Next()` alone, and with the other functions moving the ICU
    // iterator in between.
    CharacterBreakIterator iterator_for_next(text);
    icu_iterator.first();
    int next;
    do {
      next = icu_iterator.next();
      EXPECT_EQ(next, iterator_for_next.Next());
      EXPECT_EQ(next, iterator.Next());
      EXPECT_EQ(icu_iterator.current(), iterator.Current());
      iterator.IsBreak(1);
      iterator.Preceding(length);
    } while (next != kTextBreakDone);
  }

 private:
  std::unique_ptr<icu::BreakIterator> icu_iterator_;
};

TEST_F(CharacterBreakIteratorTest, Offsets16) {
  struct {
    StringView source;
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
      // The sample grapheme clusters in UAX #29 Table 1a.
      {u"g\u0308", {}},
      {u"\u1100\u1161\u11A8", {}},
      {u"\u0BA8\u0BBF", {}},
      {u"\u0E01\u0E33", {}},
      {u"\u0937\u093F", {}},
      {u"\u0915\u094D\u0937\u093F", {}},
      // The tailored ones aren't grapheme clusters by default.
      {u"ch", {1}},
      {u"k\u02B7", {1}},
  };
  for (const auto& test_case : kCases) {
    StringView text = test_case.source;
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
    for (int expected_offset : std::views::reverse(test_case.offsets)) {
      offset = iter.Preceding(offset);
      EXPECT_EQ(expected_offset, offset);
    }
    EXPECT_EQ(0, iter.Preceding(offset));
  }
}

TEST_F(CharacterBreakIteratorTest, SameAsIcu) {
  const std::u16string_view kCases[] = {
      u"",
      u"a",
      u"\r\n",
      u"a\r\nb\rc\nd\n\r",
      // Dashes, quotes, CJK and kana.
      u"a\u2014b\u2019c\u4E00\u3042",
      // Combining marks.
      u"e\u0300\u0301z",
      // A Devanagari conjunct (GB9c) with a spacing mark.
      u"\u0915\u094D\u0937\u093F",
      // Hangul.
      u"\u1100\u1161\u11A8\uAC00\uAC01",
      // Prepend, and a Thai spacing mark.
      u"\u0600a\u0E01\u0E33",
      // An emoji ZWJ sequence, and regional indicators.
      u"a\U0001F635\u200D\U0001F4ABz\U0001F1EF\U0001F1F5\U0001F1FA",
      // Lone surrogates.
      u"a\xD800"
      u"b\xDC00"
      u"c\xD800",
      u"\u200Ba\uFFFC\u00AD",
      // The sample grapheme clusters in UAX #29 Table 1a, in a row.
      u"g\u0308\uAC01\u1100\u1161\u11A8\u0E01\u0BA8\u0BBF\u0E40\u0E01\u0E33"
      u"\u0937\u093F\u0915\u094D\u0937\u093F\u0E33\u0937\u093Fchk\u02B7",
      // Complex scripts mixed with Latin-1.
      u"a\u0915\u093F b\uAC00 c\u0E01\u0E33 d",
  };
  for (bool enabled : {false, true}) {
    SCOPED_TRACE(enabled);
    ScopedCharacterBreakIteratorFastPathForTest scoped_feature(enabled);
    for (std::u16string_view text : kCases) {
      ExpectSameAsIcu(StringView(base::span(text)));
    }
  }
}

TEST_F(CharacterBreakIteratorTest, SameAsIcu8) {
  const char* const kCases[] = {
      "",
      "a",
      "\r\n",
      "a\r\nb\rc\nd\n\r",
      "\r\r\n\n",
      // Latin-1 letters, a no-break space and a soft hyphen.
      "caf\xE9 na\xEFve\xA0\xAD\xFF",
  };
  for (const char* text : kCases) {
    const StringView view(text);
    ASSERT_TRUE(view.Is8Bit());
    ExpectSameAsIcu(view);
  }
}

// The fast path decides each break from the two Latin-1 code units around it,
// and the other functions use the same decision, so checking `IsBreak()` for
// every pair, in 8-bit and 16-bit text, covers it.
TEST_F(CharacterBreakIteratorTest, IsBreakSameAsIcuForAllLatin1Pairs) {
  for (UChar32 first = 0; first <= 0xFF; ++first) {
    for (UChar32 second = 0; second <= 0xFF; ++second) {
      const LChar chars[] = {static_cast<LChar>(first),
                             static_cast<LChar>(second)};
      const UChar chars16[] = {static_cast<UChar>(first),
                               static_cast<UChar>(second)};
      const bool is_break = IcuIterator(chars16).isBoundary(1);
      const auto text = StringView(base::span(chars));
      const base::span<const UChar> text16(chars16);
      CharacterBreakIterator iterator(text);
      CharacterBreakIterator iterator16(text16);
      EXPECT_EQ(is_break, iterator.IsBreak(1)) << first << " " << second;
      EXPECT_EQ(is_break, iterator16.IsBreak(1)) << first << " " << second;
    }
  }
}

}  // namespace blink
