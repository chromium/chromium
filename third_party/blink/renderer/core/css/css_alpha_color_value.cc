// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_alpha_color_value.h"

#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink::cssvalue {

bool CSSAlphaColorValue::Equals(const CSSAlphaColorValue& other) const {
  return base::ValuesEquivalent(origin_color_, other.origin_color_) &&
         base::ValuesEquivalent(alpha_, other.alpha_);
}

String CSSAlphaColorValue::CustomCSSText() const {
  StringBuilder result;
  result.Append("alpha(from ");
  result.Append(origin_color_->CssText());
  if (alpha_ != nullptr) {
    result.Append(" / ");
    result.Append(alpha_->CssText());
  }
  result.Append(')');
  return result.ReleaseString();
}

void CSSAlphaColorValue::TraceAfterDispatch(blink::Visitor* visitor) const {
  visitor->Trace(origin_color_);
  visitor->Trace(alpha_);
  CSSValue::TraceAfterDispatch(visitor);
}

}  // namespace blink::cssvalue
