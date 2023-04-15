// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_scroll_value.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {
namespace cssvalue {

CSSScrollValue::CSSScrollValue(const CSSValue* scroller, const CSSValue* axis)
    : CSSValue(kScrollClass), scroller_(scroller), axis_(axis) {}

String CSSScrollValue::CustomCSSText() const {
  StringBuilder result;
  result.Append("scroll(");
  if (scroller_) {
    result.Append(scroller_->CssText());
  }
  if (axis_) {
    if (scroller_) {
      result.Append(' ');
    }
    result.Append(axis_->CssText());
  }
  result.Append(")");
  return result.ReleaseString();
}

bool CSSScrollValue::Equals(const CSSScrollValue& other) const {
  return base::ValuesEquivalent(scroller_, other.scroller_) &&
         base::ValuesEquivalent(axis_, other.axis_);
}

void CSSScrollValue::TraceAfterDispatch(blink::Visitor* visitor) const {
  CSSValue::TraceAfterDispatch(visitor);
  visitor->Trace(scroller_);
  visitor->Trace(axis_);
}

}  // namespace cssvalue
}  // namespace blink
