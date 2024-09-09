// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/font_palette.h"

#include "third_party/blink/renderer/platform/wtf/hash_functions.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/skia/include/core/SkFontArguments.h"

namespace blink {

namespace {

// This converts -0.0 to 0.0, so that they have the same hash value. This
// ensures that equal FontDescription have the same hash value.
float NormalizeSign(float number) {
  if (number == 0.0) [[unlikely]] {
    return 0.0;
  }
  return number;
}

}  // namespace

unsigned FontPalette::GetHash() const {
  unsigned computed_hash = 0;
  WTF::AddIntToHash(computed_hash, palette_keyword_);

  if (palette_keyword_ == kInterpolablePalette) {
    WTF::AddFloatToHash(computed_hash, NormalizeSign(percentages_.start));
    WTF::AddFloatToHash(computed_hash, NormalizeSign(percentages_.end));
    WTF::AddFloatToHash(computed_hash, NormalizeSign(normalized_percentage_));
    WTF::AddFloatToHash(computed_hash, NormalizeSign(alpha_multiplier_));
    WTF::AddIntToHash(computed_hash,
                      static_cast<uint8_t>(color_interpolation_space_));
    if (hue_interpolation_method_.has_value()) {
      WTF::AddIntToHash(computed_hash,
                        static_cast<uint8_t>(*hue_interpolation_method_));
    }

    WTF::AddIntToHash(computed_hash, start_->GetHash());
    WTF::AddIntToHash(computed_hash, end_->GetHash());
  }

  if (palette_keyword_ != kCustomPalette)
    return computed_hash;

  WTF::AddIntToHash(computed_hash, WTF::GetHash(palette_values_name_));
  WTF::AddIntToHash(computed_hash, match_font_family_.empty()
                                       ? 0
                                       : WTF::GetHash(match_font_family_));
  WTF::AddIntToHash(computed_hash, base_palette_.type);
  WTF::AddIntToHash(computed_hash, base_palette_.index);

  for (auto& override_entry : palette_overrides_) {
    WTF::AddIntToHash(computed_hash, override_entry.index);
  }
  return computed_hash;
}

String FontPalette::ToString() const {
  switch (palette_keyword_) {
    case kNormalPalette:
      return "normal";
    case kLightPalette:
      return "light";
    case kDarkPalette:
      return "dark";
    case kCustomPalette:
      return palette_values_name_.GetString();
    case kInterpolablePalette:
      StringBuilder builder;
      builder.Append("palette-mix(in ");
      if (hue_interpolation_method_.has_value()) {
        builder.Append(Color::SerializeInterpolationSpace(
            color_interpolation_space_, *hue_interpolation_method_));
      } else {
        builder.Append(
            Color::SerializeInterpolationSpace(color_interpolation_space_));
      }
      builder.Append(", ");
      builder.Append(start_->ToString());
      builder.Append(", ");
      builder.Append(end_->ToString());
      DCHECK(normalized_percentage_);
      builder.Append(" ");
      double normalized_percentage = normalized_percentage_ * 100;
      builder.AppendNumber(normalized_percentage);
      builder.Append("%)");
      return builder.ToString();
  }
}

bool FontPalette::operator==(const FontPalette& other) const {
  if (IsInterpolablePalette() != other.IsInterpolablePalette()) {
    return false;
  }
  if (IsInterpolablePalette() && other.IsInterpolablePalette()) {
    return *start_.get() == *other.start_.get() &&
           *end_.get() == *other.end_.get() &&
           percentages_ == other.percentages_ &&
           normalized_percentage_ == other.normalized_percentage_ &&
           alpha_multiplier_ == other.alpha_multiplier_ &&
           color_interpolation_space_ == other.color_interpolation_space_ &&
           hue_interpolation_method_ == other.hue_interpolation_method_;
  }
  return palette_keyword_ == other.palette_keyword_ &&
         palette_values_name_ == other.palette_values_name_ &&
         match_font_family_ == other.match_font_family_ &&
         base_palette_ == other.base_palette_ &&
         palette_overrides_ == other.palette_overrides_;
}

}  // namespace blink
