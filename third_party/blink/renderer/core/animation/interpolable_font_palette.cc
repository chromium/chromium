// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/interpolable_font_palette.h"
#include <memory>
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/animation/interpolable_color.h"
#include "third_party/blink/renderer/core/animation/interpolable_value.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/platform/fonts/font_palette.h"

namespace blink {

InterpolableFontPalette::InterpolableFontPalette(
    scoped_refptr<const FontPalette> font_palette)
    : font_palette_(font_palette) {
  DCHECK(font_palette);
}

// static
InterpolableFontPalette* InterpolableFontPalette::Create(
    scoped_refptr<const FontPalette> font_palette) {
  return MakeGarbageCollected<InterpolableFontPalette>(font_palette);
}

scoped_refptr<const FontPalette> InterpolableFontPalette::GetFontPalette()
    const {
  return font_palette_;
}

InterpolableFontPalette* InterpolableFontPalette::RawClone() const {
  return MakeGarbageCollected<InterpolableFontPalette>(font_palette_);
}

InterpolableFontPalette* InterpolableFontPalette::RawCloneAndZero() const {
  return MakeGarbageCollected<InterpolableFontPalette>(FontPalette::Create());
}

bool InterpolableFontPalette::Equals(const InterpolableValue& other) const {
  const InterpolableFontPalette& other_palette =
      To<InterpolableFontPalette>(other);
  return *font_palette_ == *other_palette.font_palette_;
}

void InterpolableFontPalette::AssertCanInterpolateWith(
    const InterpolableValue& other) const {
  DCHECK(other.IsFontPalette());
}

void InterpolableFontPalette::Interpolate(const InterpolableValue& to,
                                          const double progress,
                                          InterpolableValue& result) const {
  const InterpolableFontPalette& to_palette = To<InterpolableFontPalette>(to);
  InterpolableFontPalette& result_palette = To<InterpolableFontPalette>(result);

  // Percentages are required to be in the range 0% to 100% for palette-mix()
  // function, since the color-mix() function supports percentages only from
  // that range, compare
  // https://drafts.csswg.org/css-color-5/#color-mix-percent-norm.
  double normalized_progress = ClampTo<double>(progress, 0.0, 1.0);

  if (normalized_progress == 0 ||
      *font_palette_.get() == *to_palette.font_palette_.get()) {
    result_palette.font_palette_ = font_palette_;
  } else if (normalized_progress == 1) {
    result_palette.font_palette_ = to_palette.font_palette_;
  } else {
    FontPalette::NonNormalizedPercentages percentages =
        FontPalette::ComputeEndpointPercentagesFromNormalized(
            normalized_progress);
    // Since there is no way for user to specify which color space should be
    // used for interpolation, it defaults to Oklab.
    // https://www.w3.org/TR/css-color-4/#interpolation-space
    result_palette.font_palette_ = FontPalette::Mix(
        font_palette_, to_palette.font_palette_, percentages.start,
        percentages.end, normalized_progress, 1.0, Color::ColorSpace::kOklab,
        std::nullopt);
  }
}

}  // namespace blink
