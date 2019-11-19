// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/symbols_iterator.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"
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

class SymbolsIteratorTest : public testing::Test {
 protected:
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

TEST_F(SymbolsIteratorTest, Empty) {
  String empty(g_empty_string16_bit);
  SymbolsIterator symbols_iterator(empty.Characters16(), empty.length());
  unsigned limit = 0;
  FontFallbackPriority symbols_font = FontFallbackPriority::kInvalid;
  DCHECK(!symbols_iterator.Consume(&limit, &symbols_font));
  ASSERT_EQ(limit, 0u);
  ASSERT_EQ(symbols_font, FontFallbackPriority::kInvalid);
}

TEST_F(SymbolsIteratorTest, Space) {
  CheckRuns({{" ", FontFallbackPriority::kText}});
}

TEST_F(SymbolsIteratorTest, Latin) {
  CheckRuns({{"Aa", FontFallbackPriority::kText}});
}

TEST_F(SymbolsIteratorTest, BMPEmoji) {
  CheckRuns({{"⌚⌛⌚⌛⌚⌛⌚⌛", FontFallbackPriority::kEmojiEmoji}});
}

TEST_F(SymbolsIteratorTest, LatinColorEmojiTextEmoji) {
  CheckRuns({{"a", FontFallbackPriority::kText},
             {"⌚", FontFallbackPriority::kEmojiEmoji},
             {"☎", FontFallbackPriority::kText}});
}

TEST_F(SymbolsIteratorTest, IgnoreVSInMath) {
  CheckRuns({{u8"⊆⊇⊈\U0000FE0E⊙⊚⊚", FontFallbackPriority::kText}});
}

TEST_F(SymbolsIteratorTest, IgnoreVS15InText) {
  CheckRuns({{u8"abcdef\U0000FE0Eghji", FontFallbackPriority::kText}});
}

TEST_F(SymbolsIteratorTest, IgnoreVS16InText) {
  CheckRuns({{u8"abcdef\U0000FE0Fghji", FontFallbackPriority::kText}});
}

TEST_F(SymbolsIteratorTest, AllHexValuesText) {
  // Helps with detecting incorrect emoji pattern definitions which are
  // missing a \U000... prefix for example.
  CheckRuns({{"abcdef0123456789ABCDEF", FontFallbackPriority::kText}});
}

TEST_F(SymbolsIteratorTest, NumbersAndHashNormalAndEmoji) {
  CheckRuns({{"0123456789#*", FontFallbackPriority::kText},
             {"0\uFE0F⃣1\uFE0F⃣2\uFE0F⃣3\uFE0F⃣4\uFE0F⃣5\uFE0F⃣6\uFE0F⃣7\uFE0F⃣8\uFE0F⃣9"
              "\uFE0F⃣*\uFE0F⃣",
              FontFallbackPriority::kEmojiEmoji},
             {"0123456789#*", FontFallbackPriority::kText}});
}

TEST_F(SymbolsIteratorTest, VS16onDigits) {
  CheckRuns({{"#", FontFallbackPriority::kText},
             {"#\uFE0F\u20E3", FontFallbackPriority::kEmojiEmoji},
             {"#", FontFallbackPriority::kText}});
}

TEST_F(SymbolsIteratorTest, EmojiVS15AndVS16) {
  CheckRuns({{u8"\U0001F642", FontFallbackPriority::kEmojiEmoji},
             {u8"\U0001F642\U0000FE0E", FontFallbackPriority::kText},
             {u8"\U0001F642\U0000FE0F", FontFallbackPriority::kEmojiEmoji}});
}

TEST_F(SymbolsIteratorTest, MultipleMisplacedVS) {
  CheckRuns({
      {u8"\U0000FE0E\U0000FE0F", FontFallbackPriority::kText},
      {u8"\U0001F642\U0000FE0F", FontFallbackPriority::kEmojiEmoji},
      {u8"\U0001F642\U0000FE0E", FontFallbackPriority::kText},
      {u8"\U0001F642\U0000FE0F", FontFallbackPriority::kEmojiEmoji},
      {u8"\U0000FE0E\U0000FE0F", FontFallbackPriority::kText},
      {u8"\U0001F642\U0000FE0F", FontFallbackPriority::kEmojiEmoji},
      {u8"\U0001F642\U0000FE0E\U0000FE0E\U0000FE0F",
       FontFallbackPriority::kText},
  });
}

TEST_F(SymbolsIteratorTest, SingleFlag) {
  CheckRuns({{"🇺", FontFallbackPriority::kText}});
}

TEST_F(SymbolsIteratorTest, CombiningCircle) {
  CheckRuns({{"◌́◌̀◌̈◌̂◌̄◌̊", FontFallbackPriority::kText}});
}

TEST_F(SymbolsIteratorTest, CombiningEnclosingCircleBackslash) {
  CheckRuns({{"A⃠B⃠C⃠", FontFallbackPriority::kText},
             {"🚷🚯🚱🔞📵🚭🚫🎙⃠📸⃠🔫⃠",
              FontFallbackPriority::kEmojiEmoji},
             {"a⃠b⃠c⃠", FontFallbackPriority::kText}});
}

// TODO: Perhaps check for invalid country indicator combinations?

TEST_F(SymbolsIteratorTest, FlagsVsNonFlags) {
  CheckRuns({{"🇺🇸", FontFallbackPriority::kEmojiEmoji},  // "US"
             {"🇸abc", FontFallbackPriority::kText},
             {"🇺🇸", FontFallbackPriority::kEmojiEmoji},
             {"a🇿", FontFallbackPriority::kText}});
}

TEST_F(SymbolsIteratorTest, EmojiVS15) {
  // A VS15 after the anchor must trigger text display.
  CheckRuns({{"⚓\U0000FE0E", FontFallbackPriority::kText},
             {"⛵", FontFallbackPriority::kEmojiEmoji}});
}

TEST_F(SymbolsIteratorTest, EmojiZWSSequences) {
  CheckRuns(
      {{"👩‍👩‍👧‍👦👩‍❤️‍💋‍👨",
        FontFallbackPriority::kEmojiEmoji},
       {"abcd", FontFallbackPriority::kText},
       {u8"\U0001F469\U0000200D\U0001F469", FontFallbackPriority::kEmojiEmoji},
       {u8"\U0000200Defgh", FontFallbackPriority::kText}});
}

TEST_F(SymbolsIteratorTest, AllEmojiZWSSequences) {
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

TEST_F(SymbolsIteratorTest, ModifierPlusGender) {
  CheckRuns({{"⛹🏻‍♂", FontFallbackPriority::kEmojiEmoji}});
}

TEST_F(SymbolsIteratorTest, TextMemberZwjSequence) {
  CheckRuns({{"👨‍⚕️", FontFallbackPriority::kEmojiEmoji}});
}

TEST_F(SymbolsIteratorTest, FacepalmCartwheelShrugModifierFemale) {
  CheckRuns({{"🤦‍♀🤸‍♀🤷‍♀🤷🏾‍♀",
              FontFallbackPriority::kEmojiEmoji}});
}

TEST_F(SymbolsIteratorTest, AesculapiusMaleFemalEmoji) {
  // Emoji Data 4 has upgraded those three characters to Emoji.
  CheckRuns({{"a⚕♀♂", FontFallbackPriority::kText}});
}

TEST_F(SymbolsIteratorTest, EyeSpeechBubble) {
  CheckRuns({{"👁‍🗨", FontFallbackPriority::kEmojiEmoji}});
}

TEST_F(SymbolsIteratorTest, Modifier) {
  CheckRuns({{"👶🏿", FontFallbackPriority::kEmojiEmoji}});
}

TEST_F(SymbolsIteratorTest, DingbatsMiscSymbolsModifier) {
  CheckRuns({{"⛹🏻✍🏻✊🏼", FontFallbackPriority::kEmojiEmoji}});
}

TEST_F(SymbolsIteratorTest, ExtraZWJPrefix) {
  CheckRuns({{u8"\U0000200D", FontFallbackPriority::kText},
             {u8"\U0001F469\U0000200D\U00002764\U0000FE0F\U0000200D\U0001F48B"
              u8"\U0000200D\U0001F468",
              FontFallbackPriority::kEmojiEmoji}});
}

TEST_F(SymbolsIteratorTest, StrayZWJAndVS) {
  CheckRuns({{u8"\U0000200D\U0000FE0E\U0000FE0E\U0000FE0E\U0000200D\U0000200D",
              FontFallbackPriority::kText},
             {u8"\U0001F469\U0000200D\U00002764\U0000FE0F\U0000200D\U0001F48B"
              u8"\U0000200D\U0001F468",
              FontFallbackPriority::kEmojiEmoji},
             {u8"\U0000200D\U0000FE0E\U0000FE0E\U0000FE0E\U0000200D\U0000200D",
              FontFallbackPriority::kText}});
}

TEST_F(SymbolsIteratorTest, Arrows) {
  CheckRuns({{"x→←x←↑↓→", FontFallbackPriority::kText}});
}

TEST_F(SymbolsIteratorTest, JudgePilot) {
  CheckRuns({{"👨‍⚖️👩‍⚖️👨🏼‍⚖️👩🏼‍⚖️",
              FontFallbackPriority::kEmojiEmoji}});
}

TEST_F(SymbolsIteratorTest, EmojiPunctuationText) {
  CheckRuns({{"⁉⁉⁉⁈⁈⁈", FontFallbackPriority::kText}});
}

// Extracted from http://unicode.org/emoji/charts/emoji-released.html for Emoji
// v5.0, except for the subdivision-flag section.
// Before ICU 59 new emoji sequences and new single emoji are not detected as
// emoji type text and sequences get split up in the middle so that shaping
// cannot form the right glyph from the emoji font. Running this as one run in
// one test ensures that the new emoji form an unbroken emoji-type sequence.
TEST_F(SymbolsIteratorTest, Emoji5AdditionsExceptFlags) {
  CheckRuns(
      {{u8"\U0001F9D4\U0001F3FB\U0001F9D4\U0001F3FC\U0001F9D4\U0001F3FD"
        u8"\U0001F9D4\U0001F3FE\U0001F9D4\U0001F3FF\U0001F931\U0001F931"
        u8"\U0001F3FB\U0001F931\U0001F3FC\U0001F931\U0001F3FD\U0001F931"
        u8"\U0001F3FE\U0001F931\U0001F3FF\U0001F9D9\U0001F9D9\U0001F3FB"
        u8"\U0001F9D9\U0001F3FC\U0001F9D9\U0001F3FD\U0001F9D9\U0001F3FE"
        u8"\U0001F9D9\U0001F3FF\U0001F9D9\U0000200D\U00002640\U0000FE0F"
        u8"\U0001F9D9\U0001F3FB\U0000200D\U00002640\U0000FE0F\U0001F9D9"
        u8"\U0001F3FC\U0000200D\U00002640\U0000FE0F\U0001F9D9\U0001F3FD"
        u8"\U0000200D\U00002640\U0000FE0F\U0001F9D9\U0001F3FE\U0000200D"
        u8"\U00002640\U0000FE0F\U0001F9D9\U0001F3FF\U0000200D\U00002640"
        u8"\U0000FE0F\U0001F9D9\U0000200D\U00002642\U0000FE0F\U0001F9D9"
        u8"\U0001F3FB\U0000200D\U00002642\U0000FE0F\U0001F9D9\U0001F3FC"
        u8"\U0000200D\U00002642\U0000FE0F\U0001F9D9\U0001F3FD\U0000200D"
        u8"\U00002642\U0000FE0F\U0001F9D9\U0001F3FE\U0000200D\U00002642"
        u8"\U0000FE0F\U0001F9D9\U0001F3FF\U0000200D\U00002642\U0000FE0F"
        u8"\U0001F9DA\U0001F9DA\U0001F3FB\U0001F9DA\U0001F3FC\U0001F9DA"
        u8"\U0001F3FD\U0001F9DA\U0001F3FE\U0001F9DA\U0001F3FF\U0001F9DA"
        u8"\U0000200D\U00002640\U0000FE0F\U0001F9DA\U0001F3FB\U0000200D"
        u8"\U00002640\U0000FE0F\U0001F9DA\U0001F3FC\U0000200D\U00002640"
        u8"\U0000FE0F\U0001F9DA\U0001F3FD\U0000200D\U00002640\U0000FE0F"
        u8"\U0001F9DA\U0001F3FE\U0000200D\U00002640\U0000FE0F\U0001F9DA"
        u8"\U0001F3FF\U0000200D\U00002640\U0000FE0F\U0001F9DA\U0000200D"
        u8"\U00002642\U0000FE0F\U0001F9DA\U0001F3FB\U0000200D\U00002642"
        u8"\U0000FE0F\U0001F9DA\U0001F3FC\U0000200D\U00002642\U0000FE0F"
        u8"\U0001F9DA\U0001F3FD\U0000200D\U00002642\U0000FE0F\U0001F9DA"
        u8"\U0001F3FE\U0000200D\U00002642\U0000FE0F\U0001F9DA\U0001F3FF"
        u8"\U0000200D\U00002642\U0000FE0F\U0001F9DB\U0001F9DB\U0001F3FB"
        u8"\U0001F9DB\U0001F3FC\U0001F9DB\U0001F3FD\U0001F9DB\U0001F3FE"
        u8"\U0001F9DB\U0001F3FF\U0001F9DB\U0000200D\U00002640\U0000FE0F"
        u8"\U0001F9DB\U0001F3FB\U0000200D\U00002640\U0000FE0F\U0001F9DB"
        u8"\U0001F3FC\U0000200D\U00002640\U0000FE0F\U0001F9DB\U0001F3FD"
        u8"\U0000200D\U00002640\U0000FE0F\U0001F9DB\U0001F3FE\U0000200D"
        u8"\U00002640\U0000FE0F\U0001F9DB\U0001F3FF\U0000200D\U00002640"
        u8"\U0000FE0F\U0001F9DB\U0000200D\U00002642\U0000FE0F\U0001F9DB"
        u8"\U0001F3FB\U0000200D\U00002642\U0000FE0F\U0001F9DB\U0001F3FC"
        u8"\U0000200D\U00002642\U0000FE0F\U0001F9DB\U0001F3FD\U0000200D"
        u8"\U00002642\U0000FE0F\U0001F9DB\U0001F3FE\U0000200D\U00002642"
        u8"\U0000FE0F\U0001F9DB\U0001F3FF\U0000200D\U00002642\U0000FE0F"
        u8"\U0001F9DC\U0001F9DC\U0001F3FB\U0001F9DC\U0001F3FC\U0001F9DC"
        u8"\U0001F3FD\U0001F9DC\U0001F3FE\U0001F9DC\U0001F3FF\U0001F9DC"
        u8"\U0000200D\U00002640\U0000FE0F\U0001F9DC\U0001F3FB\U0000200D"
        u8"\U00002640\U0000FE0F\U0001F9DC\U0001F3FC\U0000200D\U00002640"
        u8"\U0000FE0F\U0001F9DC\U0001F3FD\U0000200D\U00002640\U0000FE0F"
        u8"\U0001F9DC\U0001F3FE\U0000200D\U00002640\U0000FE0F\U0001F9DC"
        u8"\U0001F3FF\U0000200D\U00002640\U0000FE0F\U0001F9DC\U0000200D"
        u8"\U00002642\U0000FE0F\U0001F9DC\U0001F3FB\U0000200D\U00002642"
        u8"\U0000FE0F\U0001F9DC\U0001F3FC\U0000200D\U00002642\U0000FE0F"
        u8"\U0001F9DC\U0001F3FD\U0000200D\U00002642\U0000FE0F\U0001F9DC"
        u8"\U0001F3FE\U0000200D\U00002642\U0000FE0F\U0001F9DC\U0001F3FF"
        u8"\U0000200D\U00002642\U0000FE0F\U0001F9DD\U0001F9DD\U0001F3FB"
        u8"\U0001F9DD\U0001F3FC\U0001F9DD\U0001F3FD\U0001F9DD\U0001F3FE"
        u8"\U0001F9DD\U0001F3FF\U0001F9DD\U0000200D\U00002640\U0000FE0F"
        u8"\U0001F9DD\U0001F3FB\U0000200D\U00002640\U0000FE0F\U0001F9DD"
        u8"\U0001F3FC\U0000200D\U00002640\U0000FE0F\U0001F9DD\U0001F3FD"
        u8"\U0000200D\U00002640\U0000FE0F\U0001F9DD\U0001F3FE\U0000200D"
        u8"\U00002640\U0000FE0F\U0001F9DD\U0001F3FF\U0000200D\U00002640"
        u8"\U0000FE0F\U0001F9DD\U0000200D\U00002642\U0000FE0F\U0001F9DD"
        u8"\U0001F3FB\U0000200D\U00002642\U0000FE0F\U0001F9DD\U0001F3FC"
        u8"\U0000200D\U00002642\U0000FE0F\U0001F9DD\U0001F3FD\U0000200D"
        u8"\U00002642\U0000FE0F\U0001F9DD\U0001F3FE\U0000200D\U00002642"
        u8"\U0000FE0F\U0001F9DD\U0001F3FF\U0000200D\U00002642\U0000FE0F"
        u8"\U0001F9DE\U0001F9DE\U0000200D\U00002640\U0000FE0F\U0001F9DE"
        u8"\U0000200D\U00002642\U0000FE0F\U0001F9DF\U0001F9DF\U0000200D"
        u8"\U00002640\U0000FE0F\U0001F9DF\U0000200D\U00002642\U0000FE0F"
        u8"\U0001F9D6\U0001F9D6\U0001F3FB\U0001F9D6\U0001F3FC\U0001F9D6"
        u8"\U0001F3FD\U0001F9D6\U0001F3FE\U0001F9D6\U0001F3FF\U0001F9D6"
        u8"\U0000200D\U00002640\U0000FE0F\U0001F9D6\U0001F3FB\U0000200D"
        u8"\U00002640\U0000FE0F\U0001F9D6\U0001F3FC\U0000200D\U00002640"
        u8"\U0000FE0F\U0001F9D6\U0001F3FD\U0000200D\U00002640\U0000FE0F"
        u8"\U0001F9D6\U0001F3FE\U0000200D\U00002640\U0000FE0F\U0001F9D6"
        u8"\U0001F3FF\U0000200D\U00002640\U0000FE0F\U0001F9D6\U0000200D"
        u8"\U00002642\U0000FE0F\U0001F9D6\U0001F3FB\U0000200D\U00002642"
        u8"\U0000FE0F\U0001F9D6\U0001F3FC\U0000200D\U00002642\U0000FE0F"
        u8"\U0001F9D6\U0001F3FD\U0000200D\U00002642\U0000FE0F\U0001F9D6"
        u8"\U0001F3FE\U0000200D\U00002642\U0000FE0F\U0001F9D6\U0001F3FF"
        u8"\U0000200D\U00002642\U0000FE0F\U0001F9D7\U0001F9D7\U0001F3FB"
        u8"\U0001F9D7\U0001F3FC\U0001F9D7\U0001F3FD\U0001F9D7\U0001F3FE"
        u8"\U0001F9D7\U0001F3FF\U0001F9D7\U0000200D\U00002640\U0000FE0F"
        u8"\U0001F9D7\U0001F3FB\U0000200D\U00002640\U0000FE0F\U0001F9D7"
        u8"\U0001F3FC\U0000200D\U00002640\U0000FE0F\U0001F9D7\U0001F3FD"
        u8"\U0000200D\U00002640\U0000FE0F\U0001F9D7\U0001F3FE\U0000200D"
        u8"\U00002640\U0000FE0F\U0001F9D7\U0001F3FF\U0000200D\U00002640"
        u8"\U0000FE0F\U0001F9D7\U0000200D\U00002642\U0000FE0F\U0001F9D7"
        u8"\U0001F3FB\U0000200D\U00002642\U0000FE0F\U0001F9D7\U0001F3FC"
        u8"\U0000200D\U00002642\U0000FE0F\U0001F9D7\U0001F3FD\U0000200D"
        u8"\U00002642\U0000FE0F\U0001F9D7\U0001F3FE\U0000200D\U00002642"
        u8"\U0000FE0F\U0001F9D7\U0001F3FF\U0000200D\U00002642\U0000FE0F"
        u8"\U0001F9D8\U0001F9D8\U0001F3FB\U0001F9D8\U0001F3FC\U0001F9D8"
        u8"\U0001F3FD\U0001F9D8\U0001F3FE\U0001F9D8\U0001F3FF\U0001F9D8"
        u8"\U0000200D\U00002640\U0000FE0F\U0001F9D8\U0001F3FB\U0000200D"
        u8"\U00002640\U0000FE0F\U0001F9D8\U0001F3FC\U0000200D\U00002640"
        u8"\U0000FE0F\U0001F9D8\U0001F3FD\U0000200D\U00002640\U0000FE0F"
        u8"\U0001F9D8\U0001F3FE\U0000200D\U00002640\U0000FE0F\U0001F9D8"
        u8"\U0001F3FF\U0000200D\U00002640\U0000FE0F\U0001F9D8\U0000200D"
        u8"\U00002642\U0000FE0F\U0001F9D8\U0001F3FB\U0000200D\U00002642"
        u8"\U0000FE0F\U0001F9D8\U0001F3FC\U0000200D\U00002642\U0000FE0F"
        u8"\U0001F9D8\U0001F3FD\U0000200D\U00002642\U0000FE0F\U0001F9D8"
        u8"\U0001F3FE\U0000200D\U00002642\U0000FE0F\U0001F9D8\U0001F3FF"
        u8"\U0000200D\U00002642\U0000FE0F\U0001F91F\U0001F91F\U0001F3FB"
        u8"\U0001F91F\U0001F3FC\U0001F91F\U0001F3FD\U0001F91F\U0001F3FE"
        u8"\U0001F91F\U0001F3FF\U0001F932\U0001F932\U0001F3FB\U0001F932"
        u8"\U0001F3FC\U0001F932\U0001F3FD\U0001F932\U0001F3FE\U0001F932"
        u8"\U0001F3FF\U0001F9E0\U0001F9E1\U0001F9E3\U0001F9E4\U0001F9E5"
        u8"\U0001F9E6\U0001F9E2\U0001F993\U0001F992\U0001F994\U0001F995"
        u8"\U0001F996\U0001F997\U0001F965\U0001F966\U0001F968\U0001F969"
        u8"\U0001F96A\U0001F963\U0001F96B\U0001F95F\U0001F960\U0001F961"
        u8"\U0001F967\U0001F964\U0001F962\U0001F6F8\U0001F6F7\U0001F94C",
        FontFallbackPriority::kEmojiEmoji}});
}

TEST_F(SymbolsIteratorTest, EmojiSubdivisionFlags) {
  CheckRuns({{u8"\U0001F3F4\U000E0067\U000E0062\U000E0077\U000E006C\U000E0073"
              u8"\U000E007F\U0001F3F4\U000E0067\U000E0062\U000E0073\U000E0063"
              u8"\U000E0074\U000E007F\U0001F3F4",
              FontFallbackPriority::kEmojiEmoji},
             // Tag sequences on their own do not mean they're emoji.
             {u8"\U000E0067\U000E0062", FontFallbackPriority::kText}});
}

// Extracted from http://unicode.org/emoji/charts/emoji-released.html for Emoji
// v11, removed U+265F Chess Pawn and U+267E as they do not have default emoji
// presentation.
TEST_F(SymbolsIteratorTest, Emoji11Additions) {
  CheckRuns(
      {{u8"\U0001F970\U0001F975\U0001F976\U0001F973\U0001F974\U0001F97A"
        u8"\U0001F468\U0000200D\U0001F9B0\U0001F468\U0001F3FB\U0000200D"
        u8"\U0001F9B0\U0001F468\U0001F3FC\U0000200D\U0001F9B0\U0001F468"
        u8"\U0001F3FD\U0000200D\U0001F9B0\U0001F468\U0001F3FE\U0000200D"
        u8"\U0001F9B0\U0001F468\U0001F3FF\U0000200D\U0001F9B0\U0001F468"
        u8"\U0000200D\U0001F9B1\U0001F468\U0001F3FB\U0000200D\U0001F9B1"
        u8"\U0001F468\U0001F3FC\U0000200D\U0001F9B1\U0001F468\U0001F3FD"
        u8"\U0000200D\U0001F9B1\U0001F468\U0001F3FE\U0000200D\U0001F9B1"
        u8"\U0001F468\U0001F3FF\U0000200D\U0001F9B1\U0001F468\U0000200D"
        u8"\U0001F9B3\U0001F468\U0001F3FB\U0000200D\U0001F9B3\U0001F468"
        u8"\U0001F3FC\U0000200D\U0001F9B3\U0001F468\U0001F3FD\U0000200D"
        u8"\U0001F9B3\U0001F468\U0001F3FE\U0000200D\U0001F9B3\U0001F468"
        u8"\U0001F3FF\U0000200D\U0001F9B3\U0001F468\U0000200D\U0001F9B2"
        u8"\U0001F468\U0001F3FB\U0000200D\U0001F9B2\U0001F468\U0001F3FC"
        u8"\U0000200D\U0001F9B2\U0001F468\U0001F3FD\U0000200D\U0001F9B2"
        u8"\U0001F468\U0001F3FE\U0000200D\U0001F9B2\U0001F468\U0001F3FF"
        u8"\U0000200D\U0001F9B2\U0001F469\U0000200D\U0001F9B0\U0001F469"
        u8"\U0001F3FB\U0000200D\U0001F9B0\U0001F469\U0001F3FC\U0000200D"
        u8"\U0001F9B0\U0001F469\U0001F3FD\U0000200D\U0001F9B0\U0001F469"
        u8"\U0001F3FE\U0000200D\U0001F9B0\U0001F469\U0001F3FF\U0000200D"
        u8"\U0001F9B0\U0001F469\U0000200D\U0001F9B1\U0001F469\U0001F3FB"
        u8"\U0000200D\U0001F9B1\U0001F469\U0001F3FC\U0000200D\U0001F9B1"
        u8"\U0001F469\U0001F3FD\U0000200D\U0001F9B1\U0001F469\U0001F3FE"
        u8"\U0000200D\U0001F9B1\U0001F469\U0001F3FF\U0000200D\U0001F9B1"
        u8"\U0001F469\U0000200D\U0001F9B3\U0001F469\U0001F3FB\U0000200D"
        u8"\U0001F9B3\U0001F469\U0001F3FC\U0000200D\U0001F9B3\U0001F469"
        u8"\U0001F3FD\U0000200D\U0001F9B3\U0001F469\U0001F3FE\U0000200D"
        u8"\U0001F9B3\U0001F469\U0001F3FF\U0000200D\U0001F9B3\U0001F469"
        u8"\U0000200D\U0001F9B2\U0001F469\U0001F3FB\U0000200D\U0001F9B2"
        u8"\U0001F469\U0001F3FC\U0000200D\U0001F9B2\U0001F469\U0001F3FD"
        u8"\U0000200D\U0001F9B2\U0001F469\U0001F3FE\U0000200D\U0001F9B2"
        u8"\U0001F469\U0001F3FF\U0000200D\U0001F9B2\U0001F9B8\U0001F9B8"
        u8"\U0001F3FB\U0001F9B8\U0001F3FC\U0001F9B8\U0001F3FD\U0001F9B8"
        u8"\U0001F3FE\U0001F9B8\U0001F3FF\U0001F9B8\U0000200D\U00002640"
        u8"\U0000FE0F\U0001F9B8\U0001F3FB\U0000200D\U00002640\U0000FE0F"
        u8"\U0001F9B8\U0001F3FC\U0000200D\U00002640\U0000FE0F\U0001F9B8"
        u8"\U0001F3FD\U0000200D\U00002640\U0000FE0F\U0001F9B8\U0001F3FE"
        u8"\U0000200D\U00002640\U0000FE0F\U0001F9B8\U0001F3FF\U0000200D"
        u8"\U00002640\U0000FE0F\U0001F9B8\U0000200D\U00002642\U0000FE0F"
        u8"\U0001F9B8\U0001F3FB\U0000200D\U00002642\U0000FE0F\U0001F9B8"
        u8"\U0001F3FC\U0000200D\U00002642\U0000FE0F\U0001F9B8\U0001F3FD"
        u8"\U0000200D\U00002642\U0000FE0F\U0001F9B8\U0001F3FE\U0000200D"
        u8"\U00002642\U0000FE0F\U0001F9B8\U0001F3FF\U0000200D\U00002642"
        u8"\U0000FE0F\U0001F9B9\U0001F9B9\U0001F3FB\U0001F9B9\U0001F3FC"
        u8"\U0001F9B9\U0001F3FD\U0001F9B9\U0001F3FE\U0001F9B9\U0001F3FF"
        u8"\U0001F9B9\U0000200D\U00002640\U0000FE0F\U0001F9B9\U0001F3FB"
        u8"\U0000200D\U00002640\U0000FE0F\U0001F9B9\U0001F3FC\U0000200D"
        u8"\U00002640\U0000FE0F\U0001F9B9\U0001F3FD\U0000200D\U00002640"
        u8"\U0000FE0F\U0001F9B9\U0001F3FE\U0000200D\U00002640\U0000FE0F"
        u8"\U0001F9B9\U0001F3FF\U0000200D\U00002640\U0000FE0F\U0001F9B9"
        u8"\U0000200D\U00002642\U0000FE0F\U0001F9B9\U0001F3FB\U0000200D"
        u8"\U00002642\U0000FE0F\U0001F9B9\U0001F3FC\U0000200D\U00002642"
        u8"\U0000FE0F\U0001F9B9\U0001F3FD\U0000200D\U00002642\U0000FE0F"
        u8"\U0001F9B9\U0001F3FE\U0000200D\U00002642\U0000FE0F\U0001F9B9"
        u8"\U0001F3FF\U0000200D\U00002642\U0000FE0F\U0001F9B5\U0001F9B5"
        u8"\U0001F3FB\U0001F9B5\U0001F3FC\U0001F9B5\U0001F3FD\U0001F9B5"
        u8"\U0001F3FE\U0001F9B5\U0001F3FF\U0001F9B6\U0001F9B6\U0001F3FB"
        u8"\U0001F9B6\U0001F3FC\U0001F9B6\U0001F3FD\U0001F9B6\U0001F3FE"
        u8"\U0001F9B6\U0001F3FF\U0001F9B4\U0001F9B7\U0001F9B0\U0001F9B1"
        u8"\U0001F9B3\U0001F9B2\U0001F97D\U0001F97C\U0001F97E\U0001F97F"
        u8"\U0001F99D\U0001F999\U0001F99B\U0001F998\U0001F9A1\U0001F9A2"
        u8"\U0001F99A\U0001F99C\U0001F99E\U0001F99F\U0001F9A0\U0001F96D"
        u8"\U0001F96C\U0001F96F\U0001F9C2\U0001F96E\U0001F9C1\U0001F9ED"
        u8"\U0001F9F1\U0001F6F9\U0001F9F3\U0001F9E8\U0001F9E7\U0001F94E"
        u8"\U0001F94F\U0001F94D\U0001F9FF\U0001F9E9\U0001F9F8\U0001F9F5"
        u8"\U0001F9F6\U0001F9EE\U0001F9FE\U0001F9F0\U0001F9F2\U0001F9EA"
        u8"\U0001F9EB\U0001F9EC\U0001F9F4\U0001F9F7\U0001F9F9\U0001F9FA"
        u8"\U0001F9FB\U0001F9FC\U0001F9FD\U0001F9EF\U0001F3F4\U0000200D"
        u8"\U00002620\U0000FE0F",
        FontFallbackPriority::kEmojiEmoji}});
}

}  // namespace blink
