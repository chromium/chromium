// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_INLINE_USED_FONT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_INLINE_USED_FONT_H_

#include "third_party/blink/renderer/platform/fonts/font.h"

namespace blink {

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

  const SimpleFontData* PrimaryFont() const { return font_->PrimaryFont(); }
  // Returns ascent of this font, scaled by `text_fit_scaling_factor_`.
  float FloatAscent() const;
  // Returns ascent of this font, scaled by `text_fit_scaling_factor_`.
  LayoutUnit FixedAscent() const { return LayoutUnit(FloatAscent()); }

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
