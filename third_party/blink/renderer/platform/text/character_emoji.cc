// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <unicode/uchar.h>
#include <unicode/ucptrie.h>

#include "base/check_op.h"
#include "third_party/blink/renderer/platform/text/character.h"
#include "third_party/blink/renderer/platform/text/character_emoji_data.h"
#include "third_party/blink/renderer/platform/text/icu_error.h"

namespace blink::unicode {

namespace {

[[maybe_unused]] uint8_t GetEmojiProperty(UChar32 ch) {
  if constexpr (kUseBlinkEmojiData) {
    static const UCPTrie* trie = [] {
      IcuError error;
      UCPTrie* created = ucptrie_openFromBinary(
          UCPTrieType::UCPTRIE_TYPE_FAST,
          UCPTrieValueWidth::UCPTRIE_VALUE_BITS_8, kSerializedEmojiData,
          kSerializedEmojiDataSize, nullptr, &error);
      DCHECK_EQ(error, U_ZERO_ERROR);
      return created;
    }();
    return static_cast<uint8_t>(ucptrie_get(trie, ch));
  } else {
    return 0;
  }
}

}  // namespace

bool IsEmoji(UChar32 ch) {
  if constexpr (kUseBlinkEmojiData) {
    return GetEmojiProperty(ch) & kIsEmojiBit;
  } else {
    return u_hasBinaryProperty(ch, UCHAR_EMOJI);
  }
}

bool IsEmojiTextDefault(UChar32 ch) {
  if constexpr (kUseBlinkEmojiData) {
    return (GetEmojiProperty(ch) & (kIsEmojiBit | kIsEmojiPresentationBit)) ==
           kIsEmojiBit;
  } else {
    return u_hasBinaryProperty(ch, UCHAR_EMOJI) &&
           !u_hasBinaryProperty(ch, UCHAR_EMOJI_PRESENTATION);
  }
}

bool IsEmojiEmojiDefault(UChar32 ch) {
  if constexpr (kUseBlinkEmojiData) {
    return GetEmojiProperty(ch) & kIsEmojiPresentationBit;
  } else {
    return u_hasBinaryProperty(ch, UCHAR_EMOJI_PRESENTATION);
  }
}

bool IsEmojiModifierBase(UChar32 ch) {
  if constexpr (kUseBlinkEmojiData) {
    return GetEmojiProperty(ch) & kIsEmojiModifierBaseBit;
  } else {
    return u_hasBinaryProperty(ch, UCHAR_EMOJI_MODIFIER_BASE);
  }
}

bool IsEmojiReserved(UChar32 ch) {
  // `Extended_Pictographic` is for pictographic symbols, as well as reserved
  // ranges in blocks largely associated with emoji characters. Intersect with
  // `GC=Cn` (Unassigned) to get the reserved ranges.
  // https://unicode.org/reports/tr44/#Extended_Pictographic
  return u_charType(ch) == U_UNASSIGNED && IsExtendedPictographic(ch);
}

bool IsEmojiIncludingReserved(UChar32 ch) {
  return IsEmoji(ch) || IsEmojiReserved(ch);
}

bool IsRegionalIndicator(UChar32 ch) {
  return (ch >= 0x1F1E6 && ch <= 0x1F1FF);
}

}  // namespace blink::unicode
