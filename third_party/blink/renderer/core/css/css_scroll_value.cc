// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_scroll_value.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {
namespace cssvalue {

CSSScrollValue::CSSScrollValue(const CSSValue* axis, const CSSValue* scroller)
    : CSSValue(kScrollClass), axis_(axis), scroller_(scroller) {}

String CSSScrollValue::CustomCSSText() const {
  StringBuilder result;
  result.Append("scroll(");
  if (axis_)
    result.Append(axis_->CssText());
  if (scroller_) {
    if (axis_)
      result.Append(' ');
    result.Append(scroller_->CssText());
  }
  result.Append(")");
  return result.ReleaseString();
}

bool CSSScrollValue::Equals(const CSSScrollValue& other) const {
  return base::ValuesEquivalent(axis_, other.axis_) &&
         base::ValuesEquivalent(scroller_, other.scroller_);
}

void CSSScrollValue::TraceAfterDispatch(blink::Visitor* visitor) const {
  CSSValue::TraceAfterDispatch(visitor);
  visitor->Trace(axis_);
  visitor->Trace(scroller_);
}

}  // namespace cssvalue
}  // namespace blink
