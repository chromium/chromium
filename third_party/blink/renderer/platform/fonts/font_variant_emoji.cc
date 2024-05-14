// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/font_variant_emoji.h"

#include "base/notreached.h"

namespace blink {

String ToString(FontVariantEmoji variant_emoji) {
  switch (variant_emoji) {
    case FontVariantEmoji::kNormalVariantEmoji:
      return "Normal";
    case FontVariantEmoji::kTextVariantEmoji:
      return "Text";
    case FontVariantEmoji::kEmojiVariantEmoji:
      return "Emoji";
    case FontVariantEmoji::kUnicodeVariantEmoji:
      return "Unicode";
  }
  NOTREACHED_IN_MIGRATION();
  return "Unknown";
}

}  // namespace blink
