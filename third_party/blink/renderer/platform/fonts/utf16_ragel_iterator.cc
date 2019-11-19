// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/utf16_ragel_iterator.h"

#include "third_party/blink/renderer/platform/text/character.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"

namespace blink {

namespace {

char EmojiSegmentationCategory(UChar32 codepoint) {
  if (codepoint <= 0x7F) {
    if (Character::IsEmojiKeycapBase(codepoint))
      return UTF16RagelIterator::KEYCAP_BASE;
    return UTF16RagelIterator::kMaxEmojiScannerCategory;
  }
  // For the grammar to work, we need to check for more specific character
  // classes first, then expand towards more generic ones. So we match single
  // characters and small ranges first, then return EMOJI and
  // EMOJI_TEXT_PRESENTATION for the remaining ones.
  if (codepoint == kCombiningEnclosingKeycapCharacter)
    return UTF16RagelIterator::COMBINING_ENCLOSING_KEYCAP;
  if (codepoint == kCombiningEnclosingCircleBackslashCharacter)
    return UTF16RagelIterator::COMBINING_ENCLOSING_CIRCLE_BACKSLASH;
  if (codepoint == kZeroWidthJoinerCharacter)
    return UTF16RagelIterator::ZWJ;
  if (codepoint == kVariationSelector15Character)
    return UTF16RagelIterator::VS15;
  if (codepoint == kVariationSelector16Character)
    return UTF16RagelIterator::VS16;
  if (codepoint == 0x1F3F4)
    return UTF16RagelIterator::TAG_BASE;
  if (Character::IsEmojiTagSequence(codepoint))
    return UTF16RagelIterator::TAG_SEQUENCE;
  if (codepoint == kCancelTag) {
    // http://www.unicode.org/reports/tr51/#def_emoji_tag_sequence
    // defines a TAG_TERM grammar rule for U+E007F CANCEL TAG.
    return UTF16RagelIterator::TAG_TERM;
  }
  if (Character::IsEmojiModifierBase(codepoint))
    return UTF16RagelIterator::EMOJI_MODIFIER_BASE;
  if (Character::IsModifier(codepoint))
    return UTF16RagelIterator::EMOJI_MODIFIER;
  if (Character::IsRegionalIndicator(codepoint))
    return UTF16RagelIterator::REGIONAL_INDICATOR;

  if (Character::IsEmojiEmojiDefault(codepoint))
    return UTF16RagelIterator::EMOJI_EMOJI_PRESENTATION;
  if (Character::IsEmojiTextDefault(codepoint))
    return UTF16RagelIterator::EMOJI_TEXT_PRESENTATION;
  if (Character::IsEmoji(codepoint))
    return UTF16RagelIterator::EMOJI;

  // Ragel state machine will interpret unknown category as "any".
  return UTF16RagelIterator::kMaxEmojiScannerCategory;
}

}  // namespace

UTF16RagelIterator& UTF16RagelIterator::SetCursor(unsigned new_cursor) {
  CHECK_GE(new_cursor, 0u);
  CHECK_LT(new_cursor, buffer_size_);
  cursor_ = new_cursor;
  UpdateCachedCategory();
  return *this;
}

void UTF16RagelIterator::UpdateCachedCategory() {
  if (cursor_ >= buffer_size_)
    return;
  cached_category_ = EmojiSegmentationCategory(Codepoint());
}

}  // namespace blink
