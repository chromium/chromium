// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/symbols_iterator.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/fonts/font_variant_emoji.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

struct FallbackTestRun {
  std::string text;
  FontFallbackPriority font_fallback_priority;
};

struct FallbackExpectedRun {
  unsigned limit;
  FontFallbackPriority font_fallback_priority;

  FallbackExpectedRun(unsigned the_limit,
                      FontFallbackPriority the_font_fallback_priority)
      : limit(the_limit), font_fallback_priority(the_font_fallback_priority) {}
};

const bool FontVariantEmojiFlagValues[] = {true, false};

class SymbolsIteratorTest : public testing::Test {
 protected:
  void SetUp() override { ScopedFontVariantEmojiForTest scoped_feature(true); }

  void CheckRuns(const Vector<FallbackTestRun>& runs) {
    StringBuilder text;
    text.Ensure16Bit();
    Vector<FallbackExpectedRun> expect;
    for (auto& run : runs) {
      text.Append(String::FromUTF8(run.text.c_str()));
      expect.push_back(
          FallbackExpectedRun(text.length(), run.font_fallback_priority));
    }
    SymbolsIterator symbols_iterator(text.Characters16(), text.length());
    VerifyRuns(&symbols_iterator, expect);
  }

  void VerifyRuns(SymbolsIterator* symbols_iterator,
                  const Vector<FallbackExpectedRun>& expect) {
    unsigned limit;
    FontFallbackPriority font_fallback_priority;
    size_t run_count = 0;
    while (symbols_iterator->Consume(&limit, &font_fallback_priority)) {
      ASSERT_LT(run_count, expect.size());
      ASSERT_EQ(expect[run_count].limit, limit);
      ASSERT_EQ(expect[run_count].font_fallback_priority,
                font_fallback_priority);
      ++run_count;
    }
    ASSERT_EQ(expect.size(), run_count);
  }
};

class SymbolsIteratorWithFontVariantEmojiParamTest
    : public SymbolsIteratorTest,
      public testing::WithParamInterface<bool> {
  void SetUp() override {
    ScopedFontVariantEmojiForTest scoped_feature(GetParam());
  }
};

INSTANTIATE_TEST_SUITE_P(SymbolsIteratorTest,
                         SymbolsIteratorWithFontVariantEmojiParamTest,
                         testing::ValuesIn(FontVariantEmojiFlagValues));

TEST_P(SymbolsIteratorWithFontVariantEmojiParamTest, Empty) {
  String empty(g_empty_string16_bit);
  SymbolsIterator symbols_iterator(empty.Characters16(), empty.length());
  unsigned limit = 0;
  FontFallbackPriority symbols_font = FontFallbackPriority::kInvalid;
  DCHECK(!symbols_iterator.Consume(&limit, &symbols_font));
  ASSERT_EQ(limit, 0u);
  ASSERT_EQ(symbols_font, FontFallbackPriority::kInvalid);
}

TEST_P(SymbolsIteratorWithFontVariantEmojiParamTest, Space) {
  CheckRuns({{" ", FontFallbackPriority::kText}});
}

TEST_P(SymbolsIteratorWithFontVariantEmojiParamTest, Latin) {
  CheckRuns({{"Aa", FontFallbackPriority::kText}});
}

TEST_P(SymbolsIteratorWithFontVariantEmojiParamTest, BMPEmoji) {
  CheckRuns({{"⌚⌛⌚⌛⌚⌛⌚⌛", FontFallbackPriority::kEmojiEmoji}});
}

TEST_P(SymbolsIteratorWithFontVariantEmojiParamTest, LatinColorEmojiTextEmoji) {
  CheckRuns({{"a", FontFallbackPriority::kText},
             {"⌚", FontFallbackPriority::kEmojiEmoji},
             {"☎", FontFallbackPriority::kText}});
}

TEST_P(SymbolsIteratorWithFontVariantEmojiParamTest, IgnoreVSInMath) {
  CheckRuns({{"⊆⊇⊈\U0000FE0E⊙⊚⊚", FontFallbackPriority::kText}});
}

TEST_P(SymbolsIteratorWithFontVariantEmojiParamTest, IgnoreVS15InText) {
  CheckRuns({{"abcdef\U0000FE0Eghji", FontFallbackPriority::kText}});
}

TEST_P(SymbolsIteratorWithFontVariantEmojiParamTest, IgnoreVS16InText) {
  CheckRuns({{"abcdef\U0000FE0Fghji", FontFallbackPriority::kText}});
}

TEST_P(SymbolsIteratorWithFontVariantEmojiParamTest, AllHexValuesText) {
  // Helps with detecting incorrect emoji pattern definitions which are
  // missing a \U000... prefix for example.
  CheckRuns({{"abcdef0123456789ABCDEF", FontFallbackPriority::kText}});
}

TEST_P(SymbolsIteratorWithFontVariantEmojiParamTest,
       NumbersAndHashNormalAndEmoji) {
  CheckRuns({{"0123456789#*", FontFallbackPriority::kText},
             {"0\uFE0F⃣1\uFE0F⃣2\uFE0F⃣3\uFE0F⃣4\uFE0F⃣5\uFE0F⃣6\uFE0F⃣7\uFE0F⃣8\uFE0F⃣9"
              "\uFE0F⃣*\uFE0F⃣",
              FontFallbackPriority::kEmojiEmojiWithVS},
             {"0123456789#*", FontFallbackPriority::kText}});
}

TEST_P(SymbolsIteratorWithFontVariantEmojiParamTest, VS16onDigits) {
  CheckRuns({{"#", FontFallbackPriority::kText},
             {"#\uFE0F\u20E3", FontFallbackPriority::kEmojiEmojiWithVS},
             {"#", FontFallbackPriority::kText}});
}

TEST_P(SymbolsIteratorWithFontVariantEmojiParamTest, VS15onDigits) {
  CheckRuns({{"#", FontFallbackPriority::kText},
             {"#\uFE0E\u20E3", FontFallbackPriority::kEmojiTextWithVS},
             {"#", FontFallbackPriority::kText}});
}

TEST_F(SymbolsIteratorTest, EmojiVS15AndVS16) {
  CheckRuns(
      {{"\U0001F642", FontFallbackPriority::kEmojiEmoji},
       {"\U0001F642\U0000FE0E", FontFallbackPriority::kEmojiTextWithVS},
       {"\U0001F642\U0000FE0F", FontFallbackPriority::kEmojiEmojiWithVS}});
}

TEST_F(SymbolsIteratorTest, EmojiVSSplitStartsWithText) {
  CheckRuns({{"abc", FontFallbackPriority::kText},
             {"\U00002614", FontFallbackPriority::kEmojiEmoji},
             {"\U00002603", FontFallbackPriority::kText},
             {"\U00002614\U0000FE0E", FontFallbackPriority::kEmojiTextWithVS}});
}

TEST_F(SymbolsIteratorTest, EmojiVSSplitStartsWithEmoji) {
  CheckRuns(
      {{"\U00002614\U0000FE0E", FontFallbackPriority::kEmojiTextWithVS},
       {"abc", FontFallbackPriority::kText},
       {"\U00002614\U0000FE0E", FontFallbackPriority::kEmojiTextWithVS},
       {"\U00002614", FontFallbackPriority::kEmojiEmoji},
       {"\U00002614\U0000FE0F", FontFallbackPriority::kEmojiEmojiWithVS}});
}

TEST_F(SymbolsIteratorTest, EmojiVSSplitWithExcessiveVS) {
  CheckRuns({{"\U00002603", FontFallbackPriority::kText},
             {"\U00002603\U0000FE0E", FontFallbackPriority::kEmojiTextWithVS},
             {"\U00002614\U0000FE0F", FontFallbackPriority::kEmojiEmojiWithVS},
             {"\U00002614", FontFallbackPriority::kEmojiEmoji}});
}

TEST_F(SymbolsIteratorTest, EmojiAndTextVSSplit) {
  CheckRuns({{"\U00002603", FontFallbackPriority::kText},
             {"\U00002603\U0000FE0F", FontFallbackPriority::kEmojiEmojiWithVS},
             {"\U00002614\U0000FE0E", FontFallbackPriority::kEmojiTextWithVS},
             {"\U00002614", FontFallbackPriority::kEmojiEmoji}});
}

TEST_F(SymbolsIteratorTest, EmojiTextVSSplit) {
  CheckRuns({{"\U00002614\U0000FE0F", FontFallbackPriority::kEmojiEmojiWithVS},
             {"a\U00002603bc", FontFallbackPriority::kText},
             {"\U00002603\U0000FE0E\U00002614\U0000FE0E",
              FontFallbackPriority::kEmojiTextWithVS}});
}

TEST_F(SymbolsIteratorTest, ZWJWithVS) {
  // Variation selector 16 after ZWJ sequences is excessive, hence it should not
  // affect segmentation.
  // ZWJ sequences from the test below:
  // 26D3 FE0F 200D 1F4A5; RGI_Emoji_ZWJ_Sequence; broken chain (⛓️‍💥)
  // 1F469 200D 2764 FE0F 200D 1F48B 200D 1F468; RGI_Emoji_ZWJ_Sequence; kiss:
  // woman, man (👩‍❤️‍💋‍👨)
  // https://www.unicode.org/Public/emoji/15.1/emoji-zwj-sequences.txt
  CheckRuns({{"abc", FontFallbackPriority::kText},
             {"\U000026D3\U0000FE0F\U0000200D\U0001F4A5\U0000FE0F"
              "\U0001F469\U0000200D\U00002764\U0000FE0F\U0000200D\U0001F48B"
              "\U0000200D\U0001F468"
              "\U000026D3\U0000FE0F\U0000200D\U0001F4A5",
              FontFallbackPriority::kEmojiEmoji},
             {"\U0000FE0E", FontFallbackPriority::kText}});
}

TEST_F(SymbolsIteratorTest, MultipleMisplacedVS) {
  CheckRuns({
      {"\U0000FE0E\U0000FE0F", FontFallbackPriority::kText},
      {"\U0001F642\U0000FE0F", FontFallbackPriority::kEmojiEmojiWithVS},
      {"\U0001F642\U0000FE0E", FontFallbackPriority::kEmojiTextWithVS},
      {"\U0001F642\U0000FE0F", FontFallbackPriority::kEmojiEmojiWithVS},
      {"\U0000FE0E\U0000FE0F", FontFallbackPriority::kText},
      {"\U0001F642\U0000FE0F", FontFallbackPriority::kEmojiEmojiWithVS},
      {"\U0001F642\U0000FE0E", FontFallbackPriority::kEmojiTextWithVS},
      {"\U0000FE0E\U0000FE0F", FontFallbackPriority::kText},
  });
}

TEST_P(SymbolsIteratorWithFontVariantEmojiParamTest, SingleFlag) {
  CheckRuns({{"🇺", FontFallbackPriority::kText}});
}

TEST_P(SymbolsIteratorWithFontVariantEmojiParamTest, CombiningCircle) {
  CheckRuns({{"◌́◌̀◌̈◌̂◌̄◌̊", FontFallbackPriority::kText}});
}

TEST_P(SymbolsIteratorWithFontVariantEmojiParamTest,
       CombiningEnclosingCircleBackslash) {
  CheckRuns({{"A⃠B⃠C⃠", FontFallbackPriority::kText},
             {"🚷🚯🚱🔞📵🚭🚫🎙⃠📸⃠🔫⃠",
              FontFallbackPriority::kEmojiEmoji},
             {"a⃠b⃠c⃠", FontFallbackPriority::kText}});
}

// TODO: Perhaps check for invalid country indicator combinations?

TEST_P(SymbolsIteratorWithFontVariantEmojiParamTest, FlagsVsNonFlags) {
  CheckRuns({{"🇺🇸", FontFallbackPriority::kEmojiEmoji},  // "US"
             {"🇸abc", FontFallbackPriority::kText},
             {"🇺🇸", FontFallbackPriority::kEmojiEmoji},
             {"a🇿", FontFallbackPriority::kText}});
}

TEST_P(SymbolsIteratorWithFontVariantEmojiParamTest, EmojiVS15) {
  // A VS15 after the anchor must trigger text display.
  CheckRuns({{"⚓\U0000FE0E", FontFallbackPriority::kEmojiTextWithVS},
             {"⛵", FontFallbackPriority::kEmojiEmoji}});
}

TEST_P(SymbolsIteratorWithFontVariantEmojiParamTest, EmojiZWSSequences) {
  CheckRuns(
      {{"👩‍👩‍👧‍👦👩‍❤️‍💋‍👨",
        FontFallbackPriority::kEmojiEmoji},
       {"abcd", FontFallbackPriority::kText},
       {"\U0001F469\U0000200D\U0001F469", FontFallbackPriority::kEmojiEmoji},
       {"\U0000200Defgh", FontFallbackPriority::kText}});
}

TEST_P(SymbolsIteratorWithFontVariantEmojiParamTest, AllEmojiZWSSequences) {
  // clang-format gets confused by Emojis, http://llvm.org/PR30530
  // clang-format off
  CheckRuns(
      {{"💏👩‍❤️‍💋‍👨👨‍❤️‍💋‍👨👩‍❤️‍💋‍👩💑👩‍❤️‍👨👨‍❤"
        "️"
        "‍👨👩‍❤️"
        "‍👩👪👨‍👩‍👦👨‍👩‍👧👨‍👩‍👧‍👦👨‍👩‍👦‍👦👨‍👩‍👧‍👧👨‍👨"
        "‍"
        "👦👨‍👨‍👧👨‍👨‍👧‍👦👨‍👨‍👦‍👦👨‍👨‍👧"
        "‍"
        "👧"
        "👩‍👩‍👦👩‍👩‍👧👩‍👩‍👧‍👦👩‍👩‍👦‍👦👩‍👩‍👧‍👧👁"
        "‍"
        "🗨",
        FontFallbackPriority::kEmojiEmoji}});
  // clang-format on
}

TEST_P(SymbolsIteratorWithFontVariantEmojiParamTest, ModifierPlusGender) {
  CheckRuns({{"⛹🏻‍♂", FontFallbackPriority::kEmojiEmoji}});
}

TEST_P(SymbolsIteratorWithFontVariantEmojiParamTest, TextMemberZwjSequence) {
  CheckRuns({{"👨‍⚕️", FontFallbackPriority::kEmojiEmoji}});
}

TEST_P(SymbolsIteratorWithFontVariantEmojiParamTest,
       FacepalmCartwheelShrugModifierFemale) {
  CheckRuns({{"🤦‍♀🤸‍♀🤷‍♀🤷🏾‍♀",
              FontFallbackPriority::kEmojiEmoji}});
}

TEST_P(SymbolsIteratorWithFontVariantEmojiParamTest,
       AesculapiusMaleFemalEmoji) {
  // Emoji Data 4 has upgraded those three characters to Emoji.
  CheckRuns({{"a⚕♀♂", FontFallbackPriority::kText}});
}

TEST_P(SymbolsIteratorWithFontVariantEmojiParamTest, EyeSpeechBubble) {
  CheckRuns({{"👁‍🗨", FontFallbackPriority::kEmojiEmoji}});
}

TEST_P(SymbolsIteratorWithFontVariantEmojiParamTest, Modifier) {
  CheckRuns({{"👶🏿", FontFallbackPriority::kEmojiEmoji}});
}

TEST_P(SymbolsIteratorWithFontVariantEmojiParamTest,
       DingbatsMiscSymbolsModifier) {
  CheckRuns({{"⛹🏻✍🏻✊🏼", FontFallbackPriority::kEmojiEmoji}});
}

TEST_P(SymbolsIteratorWithFontVariantEmojiParamTest, ExtraZWJPrefix) {
  CheckRuns({{"\U0000200D", FontFallbackPriority::kText},
             {"\U0001F469\U0000200D\U00002764\U0000FE0F\U0000200D\U0001F48B"
              "\U0000200D\U0001F468",
              FontFallbackPriority::kEmojiEmoji}});
}

TEST_P(SymbolsIteratorWithFontVariantEmojiParamTest, StrayZWJAndVS) {
  CheckRuns({{"\U0000200D\U0000FE0E\U0000FE0E\U0000FE0E\U0000200D\U0000200D",
              FontFallbackPriority::kText},
             {"\U0001F469\U0000200D\U00002764\U0000FE0F\U0000200D\U0001F48B"
              "\U0000200D\U0001F468",
              FontFallbackPriority::kEmojiEmoji},
             {"\U0000200D\U0000FE0E\U0000FE0E\U0000FE0E\U0000200D\U0000200D",
              FontFallbackPriority::kText}});
}

TEST_P(SymbolsIteratorWithFontVariantEmojiParamTest, Arrows) {
  CheckRuns({{"x→←x←↑↓→", FontFallbackPriority::kText}});
}

TEST_P(SymbolsIteratorWithFontVariantEmojiParamTest, JudgePilot) {
  CheckRuns({{"👨‍⚖️👩‍⚖️👨🏼‍⚖️👩🏼‍⚖️",
              FontFallbackPriority::kEmojiEmoji}});
}

TEST_P(SymbolsIteratorWithFontVariantEmojiParamTest, EmojiPunctuationText) {
  CheckRuns({{"⁉⁉⁉⁈⁈⁈", FontFallbackPriority::kText}});
}

// Extracted from http://unicode.org/emoji/charts/emoji-released.html for Emoji
// v5.0, except for the subdivision-flag section.
// Before ICU 59 new emoji sequences and new single emoji are not detected as
// emoji type text and sequences get split up in the middle so that shaping
// cannot form the right glyph from the emoji font. Running this as one run in
// one test ensures that the new emoji form an unbroken emoji-type sequence.
TEST_P(SymbolsIteratorWithFontVariantEmojiParamTest,
       Emoji5AdditionsExceptFlags) {
  CheckRuns(
      {{"\U0001F9D4\U0001F3FB\U0001F9D4\U0001F3FC\U0001F9D4\U0001F3FD"
        "\U0001F9D4\U0001F3FE\U0001F9D4\U0001F3FF\U0001F931\U0001F931"
        "\U0001F3FB\U0001F931\U0001F3FC\U0001F931\U0001F3FD\U0001F931"
        "\U0001F3FE\U0001F931\U0001F3FF\U0001F9D9\U0001F9D9\U0001F3FB"
        "\U0001F9D9\U0001F3FC\U0001F9D9\U0001F3FD\U0001F9D9\U0001F3FE"
        "\U0001F9D9\U0001F3FF\U0001F9D9\U0000200D\U00002640\U0000FE0F"
        "\U0001F9D9\U0001F3FB\U0000200D\U00002640\U0000FE0F\U0001F9D9"
        "\U0001F3FC\U0000200D\U00002640\U0000FE0F\U0001F9D9\U0001F3FD"
        "\U0000200D\U00002640\U0000FE0F\U0001F9D9\U0001F3FE\U0000200D"
        "\U00002640\U0000FE0F\U0001F9D9\U0001F3FF\U0000200D\U00002640"
        "\U0000FE0F\U0001F9D9\U0000200D\U00002642\U0000FE0F\U0001F9D9"
        "\U0001F3FB\U0000200D\U00002642\U0000FE0F\U0001F9D9\U0001F3FC"
        "\U0000200D\U00002642\U0000FE0F\U0001F9D9\U0001F3FD\U0000200D"
        "\U00002642\U0000FE0F\U0001F9D9\U0001F3FE\U0000200D\U00002642"
        "\U0000FE0F\U0001F9D9\U0001F3FF\U0000200D\U00002642\U0000FE0F"
        "\U0001F9DA\U0001F9DA\U0001F3FB\U0001F9DA\U0001F3FC\U0001F9DA"
        "\U0001F3FD\U0001F9DA\U0001F3FE\U0001F9DA\U0001F3FF\U0001F9DA"
        "\U0000200D\U00002640\U0000FE0F\U0001F9DA\U0001F3FB\U0000200D"
        "\U00002640\U0000FE0F\U0001F9DA\U0001F3FC\U0000200D\U00002640"
        "\U0000FE0F\U0001F9DA\U0001F3FD\U0000200D\U00002640\U0000FE0F"
        "\U0001F9DA\U0001F3FE\U0000200D\U00002640\U0000FE0F\U0001F9DA"
        "\U0001F3FF\U0000200D\U00002640\U0000FE0F\U0001F9DA\U0000200D"
        "\U00002642\U0000FE0F\U0001F9DA\U0001F3FB\U0000200D\U00002642"
        "\U0000FE0F\U0001F9DA\U0001F3FC\U0000200D\U00002642\U0000FE0F"
        "\U0001F9DA\U0001F3FD\U0000200D\U00002642\U0000FE0F\U0001F9DA"
        "\U0001F3FE\U0000200D\U00002642\U0000FE0F\U0001F9DA\U0001F3FF"
        "\U0000200D\U00002642\U0000FE0F\U0001F9DB\U0001F9DB\U0001F3FB"
        "\U0001F9DB\U0001F3FC\U0001F9DB\U0001F3FD\U0001F9DB\U0001F3FE"
        "\U0001F9DB\U0001F3FF\U0001F9DB\U0000200D\U00002640\U0000FE0F"
        "\U0001F9DB\U0001F3FB\U0000200D\U00002640\U0000FE0F\U0001F9DB"
        "\U0001F3FC\U0000200D\U00002640\U0000FE0F\U0001F9DB\U0001F3FD"
        "\U0000200D\U00002640\U0000FE0F\U0001F9DB\U0001F3FE\U0000200D"
        "\U00002640\U0000FE0F\U0001F9DB\U0001F3FF\U0000200D\U00002640"
        "\U0000FE0F\U0001F9DB\U0000200D\U00002642\U0000FE0F\U0001F9DB"
        "\U0001F3FB\U0000200D\U00002642\U0000FE0F\U0001F9DB\U0001F3FC"
        "\U0000200D\U00002642\U0000FE0F\U0001F9DB\U0001F3FD\U0000200D"
        "\U00002642\U0000FE0F\U0001F9DB\U0001F3FE\U0000200D\U00002642"
        "\U0000FE0F\U0001F9DB\U0001F3FF\U0000200D\U00002642\U0000FE0F"
        "\U0001F9DC\U0001F9DC\U0001F3FB\U0001F9DC\U0001F3FC\U0001F9DC"
        "\U0001F3FD\U0001F9DC\U0001F3FE\U0001F9DC\U0001F3FF\U0001F9DC"
        "\U0000200D\U00002640\U0000FE0F\U0001F9DC\U0001F3FB\U0000200D"
        "\U00002640\U0000FE0F\U0001F9DC\U0001F3FC\U0000200D\U00002640"
        "\U0000FE0F\U0001F9DC\U0001F3FD\U0000200D\U00002640\U0000FE0F"
        "\U0001F9DC\U0001F3FE\U0000200D\U00002640\U0000FE0F\U0001F9DC"
        "\U0001F3FF\U0000200D\U00002640\U0000FE0F\U0001F9DC\U0000200D"
        "\U00002642\U0000FE0F\U0001F9DC\U0001F3FB\U0000200D\U00002642"
        "\U0000FE0F\U0001F9DC\U0001F3FC\U0000200D\U00002642\U0000FE0F"
        "\U0001F9DC\U0001F3FD\U0000200D\U00002642\U0000FE0F\U0001F9DC"
        "\U0001F3FE\U0000200D\U00002642\U0000FE0F\U0001F9DC\U0001F3FF"
        "\U0000200D\U00002642\U0000FE0F\U0001F9DD\U0001F9DD\U0001F3FB"
        "\U0001F9DD\U0001F3FC\U0001F9DD\U0001F3FD\U0001F9DD\U0001F3FE"
        "\U0001F9DD\U0001F3FF\U0001F9DD\U0000200D\U00002640\U0000FE0F"
        "\U0001F9DD\U0001F3FB\U0000200D\U00002640\U0000FE0F\U0001F9DD"
        "\U0001F3FC\U0000200D\U00002640\U0000FE0F\U0001F9DD\U0001F3FD"
        "\U0000200D\U00002640\U0000FE0F\U0001F9DD\U0001F3FE\U0000200D"
        "\U00002640\U0000FE0F\U0001F9DD\U0001F3FF\U0000200D\U00002640"
        "\U0000FE0F\U0001F9DD\U0000200D\U00002642\U0000FE0F\U0001F9DD"
        "\U0001F3FB\U0000200D\U00002642\U0000FE0F\U0001F9DD\U0001F3FC"
        "\U0000200D\U00002642\U0000FE0F\U0001F9DD\U0001F3FD\U0000200D"
        "\U00002642\U0000FE0F\U0001F9DD\U0001F3FE\U0000200D\U00002642"
        "\U0000FE0F\U0001F9DD\U0001F3FF\U0000200D\U00002642\U0000FE0F"
        "\U0001F9DE\U0001F9DE\U0000200D\U00002640\U0000FE0F\U0001F9DE"
        "\U0000200D\U00002642\U0000FE0F\U0001F9DF\U0001F9DF\U0000200D"
        "\U00002640\U0000FE0F\U0001F9DF\U0000200D\U00002642\U0000FE0F"
        "\U0001F9D6\U0001F9D6\U0001F3FB\U0001F9D6\U0001F3FC\U0001F9D6"
        "\U0001F3FD\U0001F9D6\U0001F3FE\U0001F9D6\U0001F3FF\U0001F9D6"
        "\U0000200D\U00002640\U0000FE0F\U0001F9D6\U0001F3FB\U0000200D"
        "\U00002640\U0000FE0F\U0001F9D6\U0001F3FC\U0000200D\U00002640"
        "\U0000FE0F\U0001F9D6\U0001F3FD\U0000200D\U00002640\U0000FE0F"
        "\U0001F9D6\U0001F3FE\U0000200D\U00002640\U0000FE0F\U0001F9D6"
        "\U0001F3FF\U0000200D\U00002640\U0000FE0F\U0001F9D6\U0000200D"
        "\U00002642\U0000FE0F\U0001F9D6\U0001F3FB\U0000200D\U00002642"
        "\U0000FE0F\U0001F9D6\U0001F3FC\U0000200D\U00002642\U0000FE0F"
        "\U0001F9D6\U0001F3FD\U0000200D\U00002642\U0000FE0F\U0001F9D6"
        "\U0001F3FE\U0000200D\U00002642\U0000FE0F\U0001F9D6\U0001F3FF"
        "\U0000200D\U00002642\U0000FE0F\U0001F9D7\U0001F9D7\U0001F3FB"
        "\U0001F9D7\U0001F3FC\U0001F9D7\U0001F3FD\U0001F9D7\U0001F3FE"
        "\U0001F9D7\U0001F3FF\U0001F9D7\U0000200D\U00002640\U0000FE0F"
        "\U0001F9D7\U0001F3FB\U0000200D\U00002640\U0000FE0F\U0001F9D7"
        "\U0001F3FC\U0000200D\U00002640\U0000FE0F\U0001F9D7\U0001F3FD"
        "\U0000200D\U00002640\U0000FE0F\U0001F9D7\U0001F3FE\U0000200D"
        "\U00002640\U0000FE0F\U0001F9D7\U0001F3FF\U0000200D\U00002640"
        "\U0000FE0F\U0001F9D7\U0000200D\U00002642\U0000FE0F\U0001F9D7"
        "\U0001F3FB\U0000200D\U00002642\U0000FE0F\U0001F9D7\U0001F3FC"
        "\U0000200D\U00002642\U0000FE0F\U0001F9D7\U0001F3FD\U0000200D"
        "\U00002642\U0000FE0F\U0001F9D7\U0001F3FE\U0000200D\U00002642"
        "\U0000FE0F\U0001F9D7\U0001F3FF\U0000200D\U00002642\U0000FE0F"
        "\U0001F9D8\U0001F9D8\U0001F3FB\U0001F9D8\U0001F3FC\U0001F9D8"
        "\U0001F3FD\U0001F9D8\U0001F3FE\U0001F9D8\U0001F3FF\U0001F9D8"
        "\U0000200D\U00002640\U0000FE0F\U0001F9D8\U0001F3FB\U0000200D"
        "\U00002640\U0000FE0F\U0001F9D8\U0001F3FC\U0000200D\U00002640"
        "\U0000FE0F\U0001F9D8\U0001F3FD\U0000200D\U00002640\U0000FE0F"
        "\U0001F9D8\U0001F3FE\U0000200D\U00002640\U0000FE0F\U0001F9D8"
        "\U0001F3FF\U0000200D\U00002640\U0000FE0F\U0001F9D8\U0000200D"
        "\U00002642\U0000FE0F\U0001F9D8\U0001F3FB\U0000200D\U00002642"
        "\U0000FE0F\U0001F9D8\U0001F3FC\U0000200D\U00002642\U0000FE0F"
        "\U0001F9D8\U0001F3FD\U0000200D\U00002642\U0000FE0F\U0001F9D8"
        "\U0001F3FE\U0000200D\U00002642\U0000FE0F\U0001F9D8\U0001F3FF"
        "\U0000200D\U00002642\U0000FE0F\U0001F91F\U0001F91F\U0001F3FB"
        "\U0001F91F\U0001F3FC\U0001F91F\U0001F3FD\U0001F91F\U0001F3FE"
        "\U0001F91F\U0001F3FF\U0001F932\U0001F932\U0001F3FB\U0001F932"
        "\U0001F3FC\U0001F932\U0001F3FD\U0001F932\U0001F3FE\U0001F932"
        "\U0001F3FF\U0001F9E0\U0001F9E1\U0001F9E3\U0001F9E4\U0001F9E5"
        "\U0001F9E6\U0001F9E2\U0001F993\U0001F992\U0001F994\U0001F995"
        "\U0001F996\U0001F997\U0001F965\U0001F966\U0001F968\U0001F969"
        "\U0001F96A\U0001F963\U0001F96B\U0001F95F\U0001F960\U0001F961"
        "\U0001F967\U0001F964\U0001F962\U0001F6F8\U0001F6F7\U0001F94C",
        FontFallbackPriority::kEmojiEmoji}});
}

TEST_P(SymbolsIteratorWithFontVariantEmojiParamTest, EmojiSubdivisionFlags) {
  CheckRuns({{"\U0001F3F4\U000E0067\U000E0062\U000E0077\U000E006C\U000E0073"
              "\U000E007F\U0001F3F4\U000E0067\U000E0062\U000E0073\U000E0063"
              "\U000E0074\U000E007F\U0001F3F4",
              FontFallbackPriority::kEmojiEmoji},
             // Tag sequences on their own do not mean they're emoji.
             {"\U000E0067\U000E0062", FontFallbackPriority::kText}});
}

// Extracted from http://unicode.org/emoji/charts/emoji-released.html for Emoji
// v11, removed U+265F Chess Pawn and U+267E as they do not have default emoji
// presentation.
TEST_P(SymbolsIteratorWithFontVariantEmojiParamTest, Emoji11Additions) {
  CheckRuns(
      {{"\U0001F970\U0001F975\U0001F976\U0001F973\U0001F974\U0001F97A"
        "\U0001F468\U0000200D\U0001F9B0\U0001F468\U0001F3FB\U0000200D"
        "\U0001F9B0\U0001F468\U0001F3FC\U0000200D\U0001F9B0\U0001F468"
        "\U0001F3FD\U0000200D\U0001F9B0\U0001F468\U0001F3FE\U0000200D"
        "\U0001F9B0\U0001F468\U0001F3FF\U0000200D\U0001F9B0\U0001F468"
        "\U0000200D\U0001F9B1\U0001F468\U0001F3FB\U0000200D\U0001F9B1"
        "\U0001F468\U0001F3FC\U0000200D\U0001F9B1\U0001F468\U0001F3FD"
        "\U0000200D\U0001F9B1\U0001F468\U0001F3FE\U0000200D\U0001F9B1"
        "\U0001F468\U0001F3FF\U0000200D\U0001F9B1\U0001F468\U0000200D"
        "\U0001F9B3\U0001F468\U0001F3FB\U0000200D\U0001F9B3\U0001F468"
        "\U0001F3FC\U0000200D\U0001F9B3\U0001F468\U0001F3FD\U0000200D"
        "\U0001F9B3\U0001F468\U0001F3FE\U0000200D\U0001F9B3\U0001F468"
        "\U0001F3FF\U0000200D\U0001F9B3\U0001F468\U0000200D\U0001F9B2"
        "\U0001F468\U0001F3FB\U0000200D\U0001F9B2\U0001F468\U0001F3FC"
        "\U0000200D\U0001F9B2\U0001F468\U0001F3FD\U0000200D\U0001F9B2"
        "\U0001F468\U0001F3FE\U0000200D\U0001F9B2\U0001F468\U0001F3FF"
        "\U0000200D\U0001F9B2\U0001F469\U0000200D\U0001F9B0\U0001F469"
        "\U0001F3FB\U0000200D\U0001F9B0\U0001F469\U0001F3FC\U0000200D"
        "\U0001F9B0\U0001F469\U0001F3FD\U0000200D\U0001F9B0\U0001F469"
        "\U0001F3FE\U0000200D\U0001F9B0\U0001F469\U0001F3FF\U0000200D"
        "\U0001F9B0\U0001F469\U0000200D\U0001F9B1\U0001F469\U0001F3FB"
        "\U0000200D\U0001F9B1\U0001F469\U0001F3FC\U0000200D\U0001F9B1"
        "\U0001F469\U0001F3FD\U0000200D\U0001F9B1\U0001F469\U0001F3FE"
        "\U0000200D\U0001F9B1\U0001F469\U0001F3FF\U0000200D\U0001F9B1"
        "\U0001F469\U0000200D\U0001F9B3\U0001F469\U0001F3FB\U0000200D"
        "\U0001F9B3\U0001F469\U0001F3FC\U0000200D\U0001F9B3\U0001F469"
        "\U0001F3FD\U0000200D\U0001F9B3\U0001F469\U0001F3FE\U0000200D"
        "\U0001F9B3\U0001F469\U0001F3FF\U0000200D\U0001F9B3\U0001F469"
        "\U0000200D\U0001F9B2\U0001F469\U0001F3FB\U0000200D\U0001F9B2"
        "\U0001F469\U0001F3FC\U0000200D\U0001F9B2\U0001F469\U0001F3FD"
        "\U0000200D\U0001F9B2\U0001F469\U0001F3FE\U0000200D\U0001F9B2"
        "\U0001F469\U0001F3FF\U0000200D\U0001F9B2\U0001F9B8\U0001F9B8"
        "\U0001F3FB\U0001F9B8\U0001F3FC\U0001F9B8\U0001F3FD\U0001F9B8"
        "\U0001F3FE\U0001F9B8\U0001F3FF\U0001F9B8\U0000200D\U00002640"
        "\U0000FE0F\U0001F9B8\U0001F3FB\U0000200D\U00002640\U0000FE0F"
        "\U0001F9B8\U0001F3FC\U0000200D\U00002640\U0000FE0F\U0001F9B8"
        "\U0001F3FD\U0000200D\U00002640\U0000FE0F\U0001F9B8\U0001F3FE"
        "\U0000200D\U00002640\U0000FE0F\U0001F9B8\U0001F3FF\U0000200D"
        "\U00002640\U0000FE0F\U0001F9B8\U0000200D\U00002642\U0000FE0F"
        "\U0001F9B8\U0001F3FB\U0000200D\U00002642\U0000FE0F\U0001F9B8"
        "\U0001F3FC\U0000200D\U00002642\U0000FE0F\U0001F9B8\U0001F3FD"
        "\U0000200D\U00002642\U0000FE0F\U0001F9B8\U0001F3FE\U0000200D"
        "\U00002642\U0000FE0F\U0001F9B8\U0001F3FF\U0000200D\U00002642"
        "\U0000FE0F\U0001F9B9\U0001F9B9\U0001F3FB\U0001F9B9\U0001F3FC"
        "\U0001F9B9\U0001F3FD\U0001F9B9\U0001F3FE\U0001F9B9\U0001F3FF"
        "\U0001F9B9\U0000200D\U00002640\U0000FE0F\U0001F9B9\U0001F3FB"
        "\U0000200D\U00002640\U0000FE0F\U0001F9B9\U0001F3FC\U0000200D"
        "\U00002640\U0000FE0F\U0001F9B9\U0001F3FD\U0000200D\U00002640"
        "\U0000FE0F\U0001F9B9\U0001F3FE\U0000200D\U00002640\U0000FE0F"
        "\U0001F9B9\U0001F3FF\U0000200D\U00002640\U0000FE0F\U0001F9B9"
        "\U0000200D\U00002642\U0000FE0F\U0001F9B9\U0001F3FB\U0000200D"
        "\U00002642\U0000FE0F\U0001F9B9\U0001F3FC\U0000200D\U00002642"
        "\U0000FE0F\U0001F9B9\U0001F3FD\U0000200D\U00002642\U0000FE0F"
        "\U0001F9B9\U0001F3FE\U0000200D\U00002642\U0000FE0F\U0001F9B9"
        "\U0001F3FF\U0000200D\U00002642\U0000FE0F\U0001F9B5\U0001F9B5"
        "\U0001F3FB\U0001F9B5\U0001F3FC\U0001F9B5\U0001F3FD\U0001F9B5"
        "\U0001F3FE\U0001F9B5\U0001F3FF\U0001F9B6\U0001F9B6\U0001F3FB"
        "\U0001F9B6\U0001F3FC\U0001F9B6\U0001F3FD\U0001F9B6\U0001F3FE"
        "\U0001F9B6\U0001F3FF\U0001F9B4\U0001F9B7\U0001F9B0\U0001F9B1"
        "\U0001F9B3\U0001F9B2\U0001F97D\U0001F97C\U0001F97E\U0001F97F"
        "\U0001F99D\U0001F999\U0001F99B\U0001F998\U0001F9A1\U0001F9A2"
        "\U0001F99A\U0001F99C\U0001F99E\U0001F99F\U0001F9A0\U0001F96D"
        "\U0001F96C\U0001F96F\U0001F9C2\U0001F96E\U0001F9C1\U0001F9ED"
        "\U0001F9F1\U0001F6F9\U0001F9F3\U0001F9E8\U0001F9E7\U0001F94E"
        "\U0001F94F\U0001F94D\U0001F9FF\U0001F9E9\U0001F9F8\U0001F9F5"
        "\U0001F9F6\U0001F9EE\U0001F9FE\U0001F9F0\U0001F9F2\U0001F9EA"
        "\U0001F9EB\U0001F9EC\U0001F9F4\U0001F9F7\U0001F9F9\U0001F9FA"
        "\U0001F9FB\U0001F9FC\U0001F9FD\U0001F9EF\U0001F3F4\U0000200D"
        "\U00002620\U0000FE0F",
        FontFallbackPriority::kEmojiEmoji}});
}

}  // namespace blink
