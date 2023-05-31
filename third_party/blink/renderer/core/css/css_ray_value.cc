// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_ray_value.h"

#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {
namespace cssvalue {

CSSRayValue::CSSRayValue(const CSSPrimitiveValue& angle,
                         const CSSIdentifierValue& size,
                         const CSSIdentifierValue* contain,
                         const CSSValue* center_x_,
                         const CSSValue* center_y_)
    : CSSValue(kRayClass),
      angle_(&angle),
      size_(&size),
      contain_(contain),
      center_x_(center_x_),
      center_y_(center_y_) {}

String CSSRayValue::CustomCSSText() const {
  StringBuilder result;
  result.Append("ray(");
  result.Append(angle_->CssText());
  if (size_->GetValueID() != CSSValueID::kClosestSide) {
    result.Append(' ');
    result.Append(size_->CssText());
  }
  if (contain_) {
    result.Append(' ');
    result.Append(contain_->CssText());
  }
  if (center_x_) {
    result.Append(" at ");
    result.Append(center_x_->CssText());
    result.Append(' ');
    result.Append(center_y_->CssText());
  }
  result.Append(')');
  return result.ReleaseString();
}

bool CSSRayValue::Equals(const CSSRayValue& other) const {
  return base::ValuesEquivalent(angle_, other.angle_) &&
         base::ValuesEquivalent(size_, other.size_) &&
         base::ValuesEquivalent(contain_, other.contain_) &&
         base::ValuesEquivalent(center_x_, other.center_x_) &&
         base::ValuesEquivalent(center_y_, other.center_y_);
}

void CSSRayValue::TraceAfterDispatch(blink::Visitor* visitor) const {
  visitor->Trace(angle_);
  visitor->Trace(size_);
  visitor->Trace(contain_);
  visitor->Trace(center_x_);
  visitor->Trace(center_y_);
  CSSValue::TraceAfterDispatch(visitor);
}

}  // namespace cssvalue
}  // namespace blink
