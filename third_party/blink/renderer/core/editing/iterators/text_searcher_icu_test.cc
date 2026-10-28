// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/iterators/text_searcher_icu.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/editing/finder/find_options.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

String MakeUtf16(const char* str) {
  String utf16_string = String::FromUtf8(str);
  utf16_string.Ensure16Bit();
  return utf16_string;
}

}  // namespace

TEST(TextSearcherIcuTest, FindSubstring) {
  TextSearcherIcu searcher;
  const String& pattern = MakeUtf16("substring");
  searcher.SetPattern(pattern, FindOptions());

  const String& text = MakeUtf16("Long text with substring content.");
  searcher.SetText(text.Span16());

  std::optional<MatchResultIcu> result = searcher.NextMatchResult();
  EXPECT_TRUE(result);
  EXPECT_NE(0u, result->start);
  EXPECT_NE(0u, result->length);
  ASSERT_LT(result->length, text.length());
  EXPECT_EQ(pattern, text.substr(result->start, result->length));

  EXPECT_FALSE(searcher.NextMatchResult());
}

TEST(TextSearcherIcuTest, FindIgnoreCaseSubstring) {
  TextSearcherIcu searcher;
  const String& pattern = MakeUtf16("substring");
  searcher.SetPattern(pattern, FindOptions().SetCaseInsensitive(true));

  const String& text = MakeUtf16("Long text with SubStrinG content.");
  searcher.SetText(text.Span16());

  std::optional<MatchResultIcu> result = searcher.NextMatchResult();
  EXPECT_TRUE(result);
  EXPECT_NE(0u, result->start);
  EXPECT_NE(0u, result->length);
  ASSERT_LT(result->length, text.length());
  EXPECT_EQ(pattern,
            text.substr(result->start, result->length).DeprecatedLower());

  searcher.SetPattern(pattern, FindOptions());
  searcher.SetOffset(0u);
  EXPECT_FALSE(searcher.NextMatchResult());
}

TEST(TextSearcherIcuTest, FindSubstringWithOffset) {
  TextSearcherIcu searcher;
  const String& pattern = MakeUtf16("substring");
  searcher.SetPattern(pattern, FindOptions());

  const String& text =
      MakeUtf16("Long text with substring content. Second substring");
  searcher.SetText(text.Span16());

  std::optional<MatchResultIcu> first_result = searcher.NextMatchResult();
  EXPECT_TRUE(first_result);
  EXPECT_NE(0u, first_result->start);
  EXPECT_NE(0u, first_result->length);

  std::optional<MatchResultIcu> second_result = searcher.NextMatchResult();
  EXPECT_TRUE(second_result);
  EXPECT_NE(0u, second_result->start);
  EXPECT_NE(0u, second_result->length);

  searcher.SetOffset(first_result->start + first_result->length);

  std::optional<MatchResultIcu> offset_result = searcher.NextMatchResult();
  EXPECT_TRUE(offset_result);
  EXPECT_EQ(offset_result->start, second_result->start);
  EXPECT_EQ(offset_result->length, second_result->length);

  searcher.SetOffset(first_result->start);

  offset_result = searcher.NextMatchResult();
  EXPECT_TRUE(offset_result);
  EXPECT_EQ(offset_result->start, first_result->start);
  EXPECT_EQ(offset_result->length, first_result->length);
}

TEST(TextSearcherIcuTest, FindControlCharacter) {
  TextSearcherIcu searcher;
  const String& pattern = MakeUtf16("\u0080");
  searcher.SetPattern(pattern, FindOptions());

  const String& text = MakeUtf16("some text");
  searcher.SetText(text.Span16());

  EXPECT_FALSE(searcher.NextMatchResult());
}

// Find-ruby-in-page relies on this behavior.
// crbug.com/40755728
TEST(TextSearcherIcuTest, IgnoreNull) {
  TextSearcherIcu searcher;
  const String pattern = MakeUtf16("substr");
  searcher.SetPattern(pattern, FindOptions());

  const String text(base::span_from_cstring(u" sub\0\0string "));
  searcher.SetText(text.Span16());

  std::optional<MatchResultIcu> result = searcher.NextMatchResult();
  EXPECT_TRUE(result);
  EXPECT_EQ(1u, result->start);
  EXPECT_EQ(8u, result->length);  // Longer than "substr".
}

TEST(TextSearcherIcuTest, NullInKanaLetters) {
  {
    TextSearcherIcu searcher;
    // ha ha
    const String pattern(u"\u306F\u306F");
    searcher.SetPattern(pattern, FindOptions().SetCaseInsensitive(true));
    // ba NUL ba
    const String text(base::span_from_cstring(u"\u3070\0\u3070"));
    searcher.SetText(text.Span16());

    std::optional<MatchResultIcu> result = searcher.NextMatchResult();
    EXPECT_FALSE(result.has_value());
  }
  {
    TextSearcherIcu searcher;
    // ba ba
    const String pattern(u"\u3070\u3070");
    searcher.SetPattern(pattern, FindOptions().SetCaseInsensitive(true));

    // ba NUL ba
    const String text(base::span_from_cstring(u"\u3070\0\u3070"));
    searcher.SetText(text.Span16());

    std::optional<MatchResultIcu> result = searcher.NextMatchResult();
    EXPECT_TRUE(result.has_value());
  }
}

// For http://crbug.com/1138877
TEST(TextSearcherIcuTest, BrokenSurrogate) {
  TextSearcherIcu searcher;
  UChar one = 0xDB00;
  const String pattern(base::span_from_ref(one));
  searcher.SetPattern(pattern, FindOptions().SetWholeWord(true));

  UChar two[] = {0x0022, 0xDB00};
  const String text{base::span(two)};
  searcher.SetText(text.Span16());

  // Note: Because even if ICU find U+DB00 but ICU doesn't think U+DB00 as
  // word, we consider it doesn't match whole word.
  EXPECT_FALSE(searcher.NextMatchResult());
}

TEST(TextSearcherIcuTest, AsymmetricDiacriticSensitivity) {
  TextSearcherIcu searcher;
  // Text contains: "a A ä Ä á č c o ö e é"
  const String& text =
      MakeUtf16("a A \u00e4 \u00c4 \u00e1 \u010d c o \u00f6 e \u00e9");
  searcher.SetText(text.Span16());

  const FindOptions kIgnoreCase = FindOptions().SetCaseInsensitive(true);
  const FindOptions kMatchCase = FindOptions().SetCaseInsensitive(false);

  // Searching for base character 'a' (case-insensitive) should match 'a', 'A',
  // 'ä', 'Ä', and 'á'.
  const String& base_a = MakeUtf16("a");
  searcher.SetPattern(base_a, kIgnoreCase);
  std::optional<MatchResultIcu> result = searcher.NextMatchResult();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(0u, result->start);
  result = searcher.NextMatchResult();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(2u, result->start);
  result = searcher.NextMatchResult();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(4u, result->start);
  result = searcher.NextMatchResult();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(6u, result->start);
  result = searcher.NextMatchResult();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(8u, result->start);
  EXPECT_FALSE(searcher.NextMatchResult().has_value());

  // Searching explicitly for 'ä' (case-insensitive) should only match 'ä' and
  // 'Ä', NOT 'a', 'A', or 'á'.
  const String& umlaut_a = MakeUtf16("\u00e4");
  searcher.SetPattern(umlaut_a, kIgnoreCase);
  searcher.SetOffset(0u);
  result = searcher.NextMatchResult();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(4u, result->start);
  EXPECT_EQ(1u, result->length);
  result = searcher.NextMatchResult();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(6u, result->start);
  EXPECT_EQ(1u, result->length);
  EXPECT_FALSE(searcher.NextMatchResult().has_value());

  // Searching explicitly for 'ä' (case-sensitive) should only match lowercase
  // 'ä', NOT 'Ä'.
  searcher.SetPattern(umlaut_a, kMatchCase);
  searcher.SetOffset(0u);
  result = searcher.NextMatchResult();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(4u, result->start);
  EXPECT_EQ(1u, result->length);
  EXPECT_FALSE(searcher.NextMatchResult().has_value());

  // Searching explicitly for 'č' (case-insensitive) should match 'č' (at 10),
  // NOT 'c' (at 12).
  const String& caron_c = MakeUtf16("\u010d");
  searcher.SetPattern(caron_c, kIgnoreCase);
  searcher.SetOffset(0u);
  result = searcher.NextMatchResult();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(10u, result->start);
  EXPECT_FALSE(searcher.NextMatchResult().has_value());

  // Searching explicitly for 'ö' should match 'ö' (at 16), NOT 'o' (at 14).
  const String& umlaut_o = MakeUtf16("\u00f6");
  searcher.SetPattern(umlaut_o, kIgnoreCase);
  searcher.SetOffset(0u);
  result = searcher.NextMatchResult();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(16u, result->start);
  EXPECT_FALSE(searcher.NextMatchResult().has_value());

  // Searching explicitly for 'é' should match 'é' (at 20), NOT 'e' (at 18).
  const String& acute_e = MakeUtf16("\u00e9");
  searcher.SetPattern(acute_e, kIgnoreCase);
  searcher.SetOffset(0u);
  result = searcher.NextMatchResult();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(20u, result->start);
  EXPECT_FALSE(searcher.NextMatchResult().has_value());
}

TEST(TextSearcherIcuTest, AsymmetricDiacriticSearchDisabledFallback) {
  ScopedFindInPageAsymmetricDiacriticSearchForTest scoped_feature(false);
  TextSearcherIcu searcher;
  const String& text = MakeUtf16("a \u00e4");
  searcher.SetText(text.Span16());

  // When FindInPageAsymmetricDiacriticSearch is disabled, case-insensitive
  // search falls back to UCOL_PRIMARY where 'ä' matches both 'a' and 'ä'.
  const String& umlaut_a = MakeUtf16("\u00e4");
  searcher.SetPattern(umlaut_a, FindOptions().SetCaseInsensitive(true));
  std::optional<MatchResultIcu> result = searcher.NextMatchResult();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(0u, result->start);
  result = searcher.NextMatchResult();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(2u, result->start);
  EXPECT_FALSE(searcher.NextMatchResult().has_value());
}

}  // namespace blink
