// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_palette_mix_value.h"
#include "base/memory/values_equivalent.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink::cssvalue {

bool CSSPaletteMixValue::Equals(const CSSPaletteMixValue& other) const {
  return base::ValuesEquivalent(palette1_, other.palette1_) &&
         base::ValuesEquivalent(palette2_, other.palette2_) &&
         base::ValuesEquivalent(percentage1_, other.percentage1_) &&
         base::ValuesEquivalent(percentage2_, other.percentage2_) &&
         color_interpolation_space_ == other.color_interpolation_space_ &&
         hue_interpolation_method_ == other.hue_interpolation_method_;
}

String CSSPaletteMixValue::CustomCSSText() const {
  StringBuilder result;
  result.Append("palette-mix(in ");
  result.Append(Color::SerializeInterpolationSpace(color_interpolation_space_,
                                                   hue_interpolation_method_));
  result.Append(", ");
  result.Append(palette1_->CssText());
  if (percentage1_) {
    result.Append(" ");
    result.Append(percentage1_->CssText());
  }
  result.Append(", ");
  result.Append(palette2_->CssText());
  if (percentage2_) {
    result.Append(" ");
    result.Append(percentage2_->CssText());
  }
  result.Append(")");

  return result.ReleaseString();
}

void CSSPaletteMixValue::TraceAfterDispatch(blink::Visitor* visitor) const {
  visitor->Trace(palette1_);
  visitor->Trace(palette2_);
  visitor->Trace(percentage1_);
  visitor->Trace(percentage2_);
  CSSValue::TraceAfterDispatch(visitor);
}

}  // namespace blink::cssvalue
