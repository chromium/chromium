// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/utf16_ragel_iterator.h"

#include <unicode/unistr.h>

#include "base/stl_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/text/character.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"

namespace blink {

TEST(UTF16RagelIteratorTest, CharacterClasses) {
  UChar32 class_examples_codepoints[] = {
      kCombiningEnclosingKeycapCharacter,
      kCombiningEnclosingCircleBackslashCharacter,
      kZeroWidthJoinerCharacter,
      kVariationSelector15Character,
      kVariationSelector16Character,
      0x1f3f4,
      0xE0030,
      kCancelTag,
      0x261D,
      0x1F3FB,
      0x1F1E6,
      0x0030,
      0x231A,
      0x00A9};
  icu::UnicodeString class_examples_unicode_string =
      icu::UnicodeString::fromUTF32(class_examples_codepoints,
                                    base::size(class_examples_codepoints));
  char categories[] = {UTF16RagelIterator::COMBINING_ENCLOSING_KEYCAP,
                       UTF16RagelIterator::COMBINING_ENCLOSING_CIRCLE_BACKSLASH,
                       UTF16RagelIterator::ZWJ,
                       UTF16RagelIterator::VS15,
                       UTF16RagelIterator::VS16,
                       UTF16RagelIterator::TAG_BASE,
                       UTF16RagelIterator::TAG_SEQUENCE,
                       UTF16RagelIterator::TAG_TERM,
                       UTF16RagelIterator::EMOJI_MODIFIER_BASE,
                       UTF16RagelIterator::EMOJI_MODIFIER,
                       UTF16RagelIterator::REGIONAL_INDICATOR,
                       UTF16RagelIterator::KEYCAP_BASE,
                       UTF16RagelIterator::EMOJI_EMOJI_PRESENTATION,
                       UTF16RagelIterator::EMOJI_TEXT_PRESENTATION};
  UTF16RagelIterator ragel_iterator(
      reinterpret_cast<const UChar*>(class_examples_unicode_string.getBuffer()),
      class_examples_unicode_string.length());
  for (char& category : categories) {
    CHECK_EQ(category, *ragel_iterator);
    ragel_iterator++;
  }

  UTF16RagelIterator reverse_ragel_iterator(
      reinterpret_cast<const UChar*>(class_examples_unicode_string.getBuffer()),
      class_examples_unicode_string.length(),
      class_examples_unicode_string.length() - 1);
  size_t i = base::size(categories) - 1;
  while (reverse_ragel_iterator.Cursor() > 0) {
    CHECK_EQ(categories[i], *reverse_ragel_iterator);
    i--;
    reverse_ragel_iterator--;
  };
}

TEST(UTF16RagelIteratorTest, ArithmeticOperators) {
  UChar32 class_examples_codepoints[] = {
      kVariationSelector15Character, kVariationSelector15Character,
      kVariationSelector15Character, kVariationSelector16Character,
      kVariationSelector16Character, kVariationSelector16Character,
  };
  icu::UnicodeString class_examples_unicode_string =
      icu::UnicodeString::fromUTF32(class_examples_codepoints,
                                    base::size(class_examples_codepoints));

  UTF16RagelIterator ragel_iterator(
      reinterpret_cast<const UChar*>(class_examples_unicode_string.getBuffer()),
      class_examples_unicode_string.length());

  CHECK_EQ(*ragel_iterator, UTF16RagelIterator::VS15);
  CHECK_EQ(*(ragel_iterator + 2), UTF16RagelIterator::VS15);
  CHECK_EQ(*(ragel_iterator + 3), UTF16RagelIterator::VS16);
  CHECK_EQ(*(ragel_iterator + 5), UTF16RagelIterator::VS16);

  CHECK_EQ(*(ragel_iterator += 3), UTF16RagelIterator::VS16);
  CHECK_EQ(*(ragel_iterator += 2), UTF16RagelIterator::VS16);
  CHECK_EQ(*(ragel_iterator -= 4), UTF16RagelIterator::VS15);
  CHECK_EQ(*(ragel_iterator += 1), UTF16RagelIterator::VS15);

  ragel_iterator += 3;

  UTF16RagelIterator ragel_iterator_begin = ragel_iterator - 5;
  CHECK(ragel_iterator != ragel_iterator_begin);
  CHECK(ragel_iterator == ragel_iterator.end() - 1);

  CHECK_EQ(*ragel_iterator, UTF16RagelIterator::VS16);
  CHECK_EQ(*(ragel_iterator - 2), UTF16RagelIterator::VS16);
  CHECK_EQ(*(ragel_iterator - 3), UTF16RagelIterator::VS15);
  CHECK_EQ(*(ragel_iterator - 5), UTF16RagelIterator::VS15);
}

TEST(UTF16RagelIteratorTest, InvalidOperationOnEmpty) {
  UTF16RagelIterator ragel_iterator;
  CHECK_EQ(ragel_iterator.Cursor(), 0u);
  EXPECT_DEATH_IF_SUPPORTED(ragel_iterator++, "");
  EXPECT_DEATH_IF_SUPPORTED(ragel_iterator--, "");
  EXPECT_DEATH_IF_SUPPORTED(*ragel_iterator, "");
}

TEST(UTF16RagelIteratorTest, CursorPositioning) {
  UChar32 flags_codepoints[] = {0x1F99E, 0x1F99E, 0x1F99E,
                                kLeftSpeechBubbleCharacter};

  icu::UnicodeString flags_unicode_string = icu::UnicodeString::fromUTF32(
      flags_codepoints, base::size(flags_codepoints));
  UTF16RagelIterator ragel_iterator(
      reinterpret_cast<const UChar*>(flags_unicode_string.getBuffer()),
      flags_unicode_string.length());

  CHECK_EQ(ragel_iterator.end().Cursor(), 8u);

  CHECK_EQ(*ragel_iterator, UTF16RagelIterator::EMOJI_EMOJI_PRESENTATION);
  CHECK_EQ(*(ragel_iterator.SetCursor(4)),
           UTF16RagelIterator::EMOJI_EMOJI_PRESENTATION);
  CHECK_EQ(*(ragel_iterator.SetCursor(6)),
           UTF16RagelIterator::EMOJI_TEXT_PRESENTATION);

  EXPECT_DEATH_IF_SUPPORTED(ragel_iterator.SetCursor(-1), "");
  EXPECT_DEATH_IF_SUPPORTED(
      ragel_iterator.SetCursor(ragel_iterator.end().Cursor()), "");
}

}  // namespace blink
