// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_progress_value.h"

#include "base/memory/values_equivalent.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink::cssvalue {

CSSProgressValue::CSSProgressValue(const CSSValue& progress,
                                   const CSSValue* easing_function)
    : CSSValue(kProgressClass),
      progress_(&progress),
      easing_function_(easing_function) {}

String CSSProgressValue::CustomCSSText() const {
  StringBuilder result;
  result.Append(progress_->CssText());
  if (easing_function_) {
    result.Append(" by ");
    result.Append(easing_function_->CssText());
  }
  return result.ReleaseString();
}

bool CSSProgressValue::Equals(const CSSProgressValue& other) const {
  return base::ValuesEquivalent(progress_, other.progress_) &&
         base::ValuesEquivalent(easing_function_, other.easing_function_);
}

void CSSProgressValue::TraceAfterDispatch(blink::Visitor* visitor) const {
  visitor->Trace(progress_);
  visitor->Trace(easing_function_);
  CSSValue::TraceAfterDispatch(visitor);
}

}  // namespace blink::cssvalue
