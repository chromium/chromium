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
                         const CSSIdentifierValue* contain)
    : CSSValue(kRayClass), angle_(&angle), size_(&size), contain_(contain) {}

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
  result.Append(')');
  return result.ReleaseString();
}

bool CSSRayValue::Equals(const CSSRayValue& other) const {
  return base::ValuesEquivalent(angle_, other.angle_) &&
         base::ValuesEquivalent(size_, other.size_) &&
         base::ValuesEquivalent(contain_, other.contain_);
}

void CSSRayValue::TraceAfterDispatch(blink::Visitor* visitor) const {
  visitor->Trace(angle_);
  visitor->Trace(size_);
  visitor->Trace(contain_);
  CSSValue::TraceAfterDispatch(visitor);
}

}  // namespace cssvalue
}  // namespace blink
