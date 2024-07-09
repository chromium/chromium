// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/fonts/utf16_ragel_iterator.h"

#include <unicode/unistr.h>

#include "base/test/gtest_util.h"
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
                                    std::size(class_examples_codepoints));
  const EmojiSegmentationCategory categories[] = {
      EmojiSegmentationCategory::COMBINING_ENCLOSING_KEYCAP,
      EmojiSegmentationCategory::COMBINING_ENCLOSING_CIRCLE_BACKSLASH,
      EmojiSegmentationCategory::ZWJ,
      EmojiSegmentationCategory::VS15,
      EmojiSegmentationCategory::VS16,
      EmojiSegmentationCategory::TAG_BASE,
      EmojiSegmentationCategory::TAG_SEQUENCE,
      EmojiSegmentationCategory::TAG_TERM,
      EmojiSegmentationCategory::EMOJI_MODIFIER_BASE,
      EmojiSegmentationCategory::EMOJI_MODIFIER,
      EmojiSegmentationCategory::REGIONAL_INDICATOR,
      EmojiSegmentationCategory::KEYCAP_BASE,
      EmojiSegmentationCategory::EMOJI_EMOJI_PRESENTATION,
      EmojiSegmentationCategory::EMOJI_TEXT_PRESENTATION};
  UTF16RagelIterator ragel_iterator(
      reinterpret_cast<const UChar*>(class_examples_unicode_string.getBuffer()),
      class_examples_unicode_string.length());
  for (const EmojiSegmentationCategory& category : categories) {
    CHECK_EQ(category, *ragel_iterator);
    ragel_iterator++;
  }

  UTF16RagelIterator reverse_ragel_iterator(
      reinterpret_cast<const UChar*>(class_examples_unicode_string.getBuffer()),
      class_examples_unicode_string.length(),
      class_examples_unicode_string.length() - 1);
  size_t i = std::size(categories) - 1;
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
                                    std::size(class_examples_codepoints));

  UTF16RagelIterator ragel_iterator(
      reinterpret_cast<const UChar*>(class_examples_unicode_string.getBuffer()),
      class_examples_unicode_string.length());

  CHECK_EQ(*ragel_iterator, EmojiSegmentationCategory::VS15);
  CHECK_EQ(*(ragel_iterator + 2), EmojiSegmentationCategory::VS15);
  CHECK_EQ(*(ragel_iterator + 3), EmojiSegmentationCategory::VS16);
  CHECK_EQ(*(ragel_iterator + 5), EmojiSegmentationCategory::VS16);

  CHECK_EQ(*(ragel_iterator += 3), EmojiSegmentationCategory::VS16);
  CHECK_EQ(*(ragel_iterator += 2), EmojiSegmentationCategory::VS16);
  CHECK_EQ(*(ragel_iterator -= 4), EmojiSegmentationCategory::VS15);
  CHECK_EQ(*(ragel_iterator += 1), EmojiSegmentationCategory::VS15);

  ragel_iterator += 3;

  UTF16RagelIterator ragel_iterator_begin = ragel_iterator - 5;
  CHECK(ragel_iterator != ragel_iterator_begin);
  CHECK(ragel_iterator == ragel_iterator.end() - 1);

  CHECK_EQ(*ragel_iterator, EmojiSegmentationCategory::VS16);
  CHECK_EQ(*(ragel_iterator - 2), EmojiSegmentationCategory::VS16);
  CHECK_EQ(*(ragel_iterator - 3), EmojiSegmentationCategory::VS15);
  CHECK_EQ(*(ragel_iterator - 5), EmojiSegmentationCategory::VS15);
}

TEST(UTF16RagelIteratorTest, InvalidOperationOnEmpty) {
  UTF16RagelIterator ragel_iterator;
  CHECK_EQ(ragel_iterator.Cursor(), 0u);
  EXPECT_DCHECK_DEATH(ragel_iterator++);
  EXPECT_DCHECK_DEATH(ragel_iterator--);
  EXPECT_DCHECK_DEATH(*ragel_iterator);
}

TEST(UTF16RagelIteratorTest, CursorPositioning) {
  UChar32 flags_codepoints[] = {0x1F99E, 0x1F99E, 0x1F99E,
                                kLeftSpeechBubbleCharacter};

  icu::UnicodeString flags_unicode_string = icu::UnicodeString::fromUTF32(
      flags_codepoints, std::size(flags_codepoints));
  UTF16RagelIterator ragel_iterator(
      reinterpret_cast<const UChar*>(flags_unicode_string.getBuffer()),
      flags_unicode_string.length());

  CHECK_EQ(ragel_iterator.end().Cursor(), 8u);

  CHECK_EQ(*ragel_iterator,
           EmojiSegmentationCategory::EMOJI_EMOJI_PRESENTATION);
  CHECK_EQ(*(ragel_iterator.SetCursor(4)),
           EmojiSegmentationCategory::EMOJI_EMOJI_PRESENTATION);
  CHECK_EQ(*(ragel_iterator.SetCursor(6)),
           EmojiSegmentationCategory::EMOJI_TEXT_PRESENTATION);

  EXPECT_DCHECK_DEATH(ragel_iterator.SetCursor(-1));
  EXPECT_DCHECK_DEATH(ragel_iterator.SetCursor(ragel_iterator.end().Cursor()));
}

}  // namespace blink
