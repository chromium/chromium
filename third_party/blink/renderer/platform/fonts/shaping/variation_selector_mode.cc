// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/variation_selector_mode.h"

namespace blink {

bool ShouldIgnoreVariationSelector(VariationSelectorMode mode) {
  return mode == VariationSelectorMode::kIgnore;
}

bool UseFontVariantEmojiVariationSelector(VariationSelectorMode mode) {
  return mode == VariationSelectorMode::kForce15 ||
         mode == VariationSelectorMode::kForce16 ||
         mode == VariationSelectorMode::kUseUnicodeDefaultPresentation;
}

VariationSelectorMode GetVariationSelectorModeFromFontVariantEmoji(
    FontVariantEmoji font_variant_emoji) {
  switch (font_variant_emoji) {
    case FontVariantEmoji::kNormal:
      return VariationSelectorMode::kUseSpecified;
    case FontVariantEmoji::kText:
      return VariationSelectorMode::kForce15;
    case FontVariantEmoji::kEmoji:
      return VariationSelectorMode::kForce16;
    case FontVariantEmoji::kUnicode:
      return VariationSelectorMode::kUseUnicodeDefaultPresentation;
  }
  NOTREACHED();
}

}  // namespace blink
