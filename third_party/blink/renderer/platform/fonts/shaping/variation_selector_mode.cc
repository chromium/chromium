// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/variation_selector_mode.h"

namespace blink {

bool ShouldIgnoreVariationSelector(VariationSelectorMode mode) {
  return mode == kIgnoreVariationSelector;
}

bool UseFontVariantEmojiVariationSelector(VariationSelectorMode mode) {
  return mode == kForceVariationSelector15 ||
         mode == kForceVariationSelector16 ||
         mode == kUseUnicodeDefaultPresentation;
}

VariationSelectorMode GetVariationSelectorModeFromFontVariantEmoji(
    FontVariantEmoji font_variant_emoji) {
  switch (font_variant_emoji) {
    case kNormalVariantEmoji:
      return kUseSpecifiedVariationSelector;
    case kTextVariantEmoji:
      return kForceVariationSelector15;
    case kEmojiVariantEmoji:
      return kForceVariationSelector16;
    case kUnicodeVariantEmoji:
      return kUseUnicodeDefaultPresentation;
  }
  NOTREACHED();
}

}  // namespace blink
