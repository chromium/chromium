// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_contrast_color_value.h"

#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink::cssvalue {

bool CSSContrastColorValue::Equals(const CSSContrastColorValue& other) const {
  return base::ValuesEquivalent(color_, other.color_);
}

String CSSContrastColorValue::CustomCSSText() const {
  StringBuilder result;
  result.Append("contrast-color(");
  result.Append(color_->CssText());
  result.Append(')');
  return result.ReleaseString();
}

void CSSContrastColorValue::TraceAfterDispatch(blink::Visitor* visitor) const {
  visitor->Trace(color_);
  CSSValue::TraceAfterDispatch(visitor);
}

}  // namespace blink::cssvalue
