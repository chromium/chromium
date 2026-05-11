// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_INLINE_USED_FONT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_INLINE_USED_FONT_H_

#include "third_party/blink/renderer/platform/fonts/font.h"
#include "ui/gfx/geometry/rect_f.h"

namespace blink {

struct TextFragmentPaintInfo;

// Represents a font along with an additional scaling factor applied during
// painting.
//
// This class encapsulates a `blink::Font` and a scaling factor to unify font
// handling across features that require scaling:
//
// - SVG text: `font_` is a pre-scaled font, and `text_fit_scaling_factor_`
//   is always 1.0.
// - CSS `text-fit`: `font_` is the base font used for text shaping. The
//   text is painted after applying the `text_fit_scaling_factor_`.
// - Standard text: `font_` matches the `Font` in the `ComputedStyle`, and
//   `text_fit_scaling_factor_` is 1.0.
class UsedFont {
  DISALLOW_NEW();

 public:
  UsedFont(const Font& font, float scaling_factor)
      : font_(font), text_fit_scaling_factor_(scaling_factor) {}
  void Trace(Visitor* visitor) const { visitor->Trace(font_); }

  float ScalingFactor() const { return text_fit_scaling_factor_; }
  const SimpleFontData* PrimaryFont() const { return font_->PrimaryFont(); }
  // Returns the computed font-size. `text_fit_scaling_factor` doesn't
  // affect it.
  float ComputedSize() const {
    return font_->GetFontDescription().ComputedSize();
  }
  // Returns the used font-size. `text_fit_scaling_factor` affects it.
  float UsedSize() const { return ComputedSize() * text_fit_scaling_factor_; }
  // Returns ascent of this font, scaled by `text_fit_scaling_factor_`.
  float FloatAscent() const {
    if (const auto* font_data = PrimaryFont()) [[likely]] {
      return font_data->GetFontMetrics().FloatAscent() *
             text_fit_scaling_factor_;
    }
    return 0.0f;
  }
  // Returns ascent of this font, scaled by `text_fit_scaling_factor_`.
  LayoutUnit FixedAscent() const { return LayoutUnit(FloatAscent()); }

  // Get the underline thickness from the font if available, scaled by
  // `text_fit_scaling_factor_`.
  std::optional<float> UnderlineThickness() const;

  // Return the ink bounds scaled by `text_fit_scaling_factor_`.
  gfx::RectF TextInkBounds(const TextFragmentPaintInfo& text_info) const {
    gfx::RectF bounds = font_->TextInkBounds(text_info);
    if (text_fit_scaling_factor_ != 1.0f) {
      bounds.Scale(text_fit_scaling_factor_);
    }
    return bounds;
  }

 private:
  // Data members are not `const` because this class is a part of DecoratingBox,
  // which is stored in a Vector.

  // Font used for shaping. This is not null.
  Member<const Font> font_;
  // Paint-time scaling factor.
  float text_fit_scaling_factor_;
};

}  // namespace blink

WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(blink::UsedFont)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_INLINE_USED_FONT_H_
