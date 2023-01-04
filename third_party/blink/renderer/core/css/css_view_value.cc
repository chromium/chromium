// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_view_value.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {
namespace cssvalue {

CSSViewValue::CSSViewValue(const CSSValue* axis, const CSSValue* inset)
    : CSSValue(kViewClass), axis_(axis), inset_(inset) {}

String CSSViewValue::CustomCSSText() const {
  StringBuilder result;
  result.Append("view(");
  if (axis_) {
    result.Append(axis_->CssText());
  }
  if (inset_) {
    if (axis_) {
      result.Append(' ');
    }
    result.Append(inset_->CssText());
  }
  result.Append(")");
  return result.ReleaseString();
}

bool CSSViewValue::Equals(const CSSViewValue& other) const {
  return base::ValuesEquivalent(axis_, other.axis_) &&
         base::ValuesEquivalent(inset_, other.inset_);
}

void CSSViewValue::TraceAfterDispatch(blink::Visitor* visitor) const {
  CSSValue::TraceAfterDispatch(visitor);
  visitor->Trace(axis_);
  visitor->Trace(inset_);
}

}  // namespace cssvalue
}  // namespace blink
