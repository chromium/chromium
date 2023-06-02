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
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

InterpolableFontPalette::InterpolableFontPalette(
    scoped_refptr<FontPalette> font_palette)
    : font_palette_(font_palette) {
  DCHECK(font_palette);
}

// static
std::unique_ptr<InterpolableFontPalette> InterpolableFontPalette::Create(
    scoped_refptr<FontPalette> font_palette) {
  return std::make_unique<InterpolableFontPalette>(font_palette);
}

scoped_refptr<FontPalette> InterpolableFontPalette::GetFontPalette() const {
  return font_palette_;
}

InterpolableFontPalette* InterpolableFontPalette::RawClone() const {
  return new InterpolableFontPalette(font_palette_);
}

InterpolableFontPalette* InterpolableFontPalette::RawCloneAndZero() const {
  return new InterpolableFontPalette(FontPalette::Create());
}

bool InterpolableFontPalette::Equals(const InterpolableValue& other) const {
  const InterpolableFontPalette& other_palette =
      To<InterpolableFontPalette>(other);
  return *font_palette_ == *other_palette.font_palette_;
}

void InterpolableFontPalette::Scale(double scale) {
  font_palette_ = FontPalette::Scale(font_palette_, scale);
}

void InterpolableFontPalette::Add(const InterpolableValue& other) {
  const InterpolableFontPalette& other_interpolable_palette =
      To<InterpolableFontPalette>(other);
  scoped_refptr<FontPalette> other_palette =
      other_interpolable_palette.font_palette_;
  font_palette_ = FontPalette::Add(font_palette_, other_palette);
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

  if (progress == 0 ||
      *font_palette_.get() == *to_palette.font_palette_.get()) {
    result_palette.font_palette_ = font_palette_;
  } else if (progress == 1) {
    result_palette.font_palette_ = to_palette.font_palette_;
  } else {
    result_palette.font_palette_ =
        FontPalette::Mix(font_palette_, to_palette.font_palette_, progress);
  }
}

}  // namespace blink
