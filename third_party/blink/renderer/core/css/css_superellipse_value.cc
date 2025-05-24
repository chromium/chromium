// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_superellipse_value.h"

#include <cmath>
#include <limits>

#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/style/superellipse.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink::cssvalue {

String CSSSuperellipseValue::CustomCSSText() const {
  StringBuilder result;
  result.Append("superellipse(");
  result.Append(param_->CssText());
  result.Append(')');
  return result.ReleaseString();
}

void CSSSuperellipseValue::TraceAfterDispatch(blink::Visitor* visitor) const {
  visitor->Trace(param_);
  CSSValue::TraceAfterDispatch(visitor);
}

}  // namespace blink::cssvalue
