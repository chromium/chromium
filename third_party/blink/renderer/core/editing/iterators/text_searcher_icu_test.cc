// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/iterators/text_searcher_icu.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/editing/finder/find_options.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

String MakeUTF16(const char* str) {
  String utf16_string = String::FromUTF8(str);
  utf16_string.Ensure16Bit();
  return utf16_string;
}

}  // namespace

TEST(TextSearcherICUTest, FindSubstring) {
  TextSearcherICU searcher;
  const String& pattern = MakeUTF16("substring");
  searcher.SetPattern(pattern, FindOptions());

  const String& text = MakeUTF16("Long text with substring content.");
  searcher.SetText(text.Span16());

  std::optional<MatchResultICU> result = searcher.NextMatchResult();
  EXPECT_TRUE(result);
  EXPECT_NE(0u, result->start);
  EXPECT_NE(0u, result->length);
  ASSERT_LT(result->length, text.length());
  EXPECT_EQ(pattern, text.Substring(result->start, result->length));

  EXPECT_FALSE(searcher.NextMatchResult());
}

TEST(TextSearcherICUTest, FindIgnoreCaseSubstring) {
  TextSearcherICU searcher;
  const String& pattern = MakeUTF16("substring");
  searcher.SetPattern(pattern, FindOptions().SetCaseInsensitive(true));

  const String& text = MakeUTF16("Long text with SubStrinG content.");
  searcher.SetText(text.Span16());

  std::optional<MatchResultICU> result = searcher.NextMatchResult();
  EXPECT_TRUE(result);
  EXPECT_NE(0u, result->start);
  EXPECT_NE(0u, result->length);
  ASSERT_LT(result->length, text.length());
  EXPECT_EQ(pattern,
            text.Substring(result->start, result->length).DeprecatedLower());

  searcher.SetPattern(pattern, FindOptions());
  searcher.SetOffset(0u);
  EXPECT_FALSE(searcher.NextMatchResult());
}

TEST(TextSearcherICUTest, FindSubstringWithOffset) {
  TextSearcherICU searcher;
  const String& pattern = MakeUTF16("substring");
  searcher.SetPattern(pattern, FindOptions());

  const String& text =
      MakeUTF16("Long text with substring content. Second substring");
  searcher.SetText(text.Span16());

  std::optional<MatchResultICU> first_result = searcher.NextMatchResult();
  EXPECT_TRUE(first_result);
  EXPECT_NE(0u, first_result->start);
  EXPECT_NE(0u, first_result->length);

  std::optional<MatchResultICU> second_result = searcher.NextMatchResult();
  EXPECT_TRUE(second_result);
  EXPECT_NE(0u, second_result->start);
  EXPECT_NE(0u, second_result->length);

  searcher.SetOffset(first_result->start + first_result->length);

  std::optional<MatchResultICU> offset_result = searcher.NextMatchResult();
  EXPECT_TRUE(offset_result);
  EXPECT_EQ(offset_result->start, second_result->start);
  EXPECT_EQ(offset_result->length, second_result->length);

  searcher.SetOffset(first_result->start);

  offset_result = searcher.NextMatchResult();
  EXPECT_TRUE(offset_result);
  EXPECT_EQ(offset_result->start, first_result->start);
  EXPECT_EQ(offset_result->length, first_result->length);
}

TEST(TextSearcherICUTest, FindControlCharacter) {
  TextSearcherICU searcher;
  const String& pattern = MakeUTF16("\u0080");
  searcher.SetPattern(pattern, FindOptions());

  const String& text = MakeUTF16("some text");
  searcher.SetText(text.Span16());

  EXPECT_FALSE(searcher.NextMatchResult());
}

// Find-ruby-in-page relies on this behavior.
// crbug.com/40755728
TEST(TextSearcherICUTest, IgnoreNull) {
  TextSearcherICU searcher;
  const String pattern = MakeUTF16("substr");
  searcher.SetPattern(pattern, FindOptions());

  const String text(u" sub\0\0string ", 13u);
  searcher.SetText(text.Span16());

  std::optional<MatchResultICU> result = searcher.NextMatchResult();
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(1u, result->start);
  EXPECT_EQ(8u, result->length);  // Longer than "substr".
}

TEST(TextSearcherICUTest, NullInKanaLetters) {
  {
    TextSearcherICU searcher;
    // ha ha
    const String pattern(u"\u306F\u306F");
    searcher.SetPattern(pattern, FindOptions().SetCaseInsensitive(true));
    // ba NUL ba
    const String text(u"\u3070\0\u3070", 3u);
    searcher.SetText(text.Span16());

    std::optional<MatchResultICU> result = searcher.NextMatchResult();
    EXPECT_FALSE(result.has_value());
  }
  {
    TextSearcherICU searcher;
    // ba ba
    const String pattern(u"\u3070\u3070");
    searcher.SetPattern(pattern, FindOptions().SetCaseInsensitive(true));

    // ba NUL ba
    const String text(u"\u3070\0\u3070", 3u);
    searcher.SetText(text.Span16());

    std::optional<MatchResultICU> result = searcher.NextMatchResult();
    EXPECT_TRUE(result.has_value());
  }
}

// For http://crbug.com/1138877
TEST(TextSearcherICUTest, BrokenSurrogate) {
  TextSearcherICU searcher;
  UChar one[1];
  one[0] = 0xDB00;
  const String pattern(one, 1u);
  searcher.SetPattern(pattern, FindOptions().SetWholeWord(true));

  UChar two[2];
  two[0] = 0x0022;
  two[1] = 0xDB00;
  const String text(two, 2u);
  searcher.SetText(text.Span16());

  // Note: Because even if ICU find U+DB00 but ICU doesn't think U+DB00 as
  // word, we consider it doesn't match whole word.
  EXPECT_FALSE(searcher.NextMatchResult());
}

}  // namespace blink
