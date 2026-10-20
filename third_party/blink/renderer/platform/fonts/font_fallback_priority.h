// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_FALLBACK_PRIORITY_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_FALLBACK_PRIORITY_H_

namespace blink {

// http://unicode.org/reports/tr51/#Presentation_Style discusses the differences
// between emoji in text and the emoji in emoji presentation. In that sense, the
// EmojiEmoji wording is taken from there.  Also compare
// http://unicode.org/Public/emoji/1.0/emoji-data.txt
enum class FontFallbackPriority {
  // For regular non-symbols text,
  // normal text fallback in FontFallbackIterator
  kText,
  // For emoji in text presentation
  kEmojiText,
  // For emoji in text presentation with variation selector 15
  kEmojiTextWithVs,
  // For emoji in emoji presentation
  kEmojiEmoji,
  // For emoji in emoji presentation with variation selector 16
  kEmojiEmojiWithVs,
  kInvalid,

  // When adding values, ensure `kMaxEnumValue` is the largest value to store
  // (values that can be returned for non-empty inputs).
  kMaxEnumValue = kEmojiEmojiWithVs,
};

inline bool IsNonTextFallbackPriority(FontFallbackPriority fallback_priority) {
  return fallback_priority == FontFallbackPriority::kEmojiText ||
         fallback_priority == FontFallbackPriority::kEmojiEmoji ||
         fallback_priority == FontFallbackPriority::kEmojiTextWithVs ||
         fallback_priority == FontFallbackPriority::kEmojiEmojiWithVs;
}

inline bool HasVsFallbackPriority(FontFallbackPriority fallback_priority) {
  return fallback_priority == FontFallbackPriority::kEmojiTextWithVs ||
         fallback_priority == FontFallbackPriority::kEmojiEmojiWithVs;
}

inline bool IsEmojiPresentationEmoji(FontFallbackPriority fallback_priority) {
  return fallback_priority == FontFallbackPriority::kEmojiEmoji ||
         fallback_priority == FontFallbackPriority::kEmojiEmojiWithVs;
}

inline bool IsTextPresentationEmoji(FontFallbackPriority fallback_priority) {
  return fallback_priority == FontFallbackPriority::kEmojiText ||
         fallback_priority == FontFallbackPriority::kEmojiTextWithVs;
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_FALLBACK_PRIORITY_H_
