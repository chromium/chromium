// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_repeat_value.h"

#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink::cssvalue {

String CSSRepeatValue::CustomCSSText() const {
  StringBuilder result;
  result.Append("repeat(");
  repetitions_ ? result.Append(repetitions_->CssText())
               : result.Append(getValueName(CSSValueID::kAuto));
  result.Append(", ");
  result.Append(values_->CustomCSSText());
  result.Append(')');
  return result.ReleaseString();
}

const CSSPrimitiveValue* CSSRepeatValue::Repetitions() const {
  CHECK(repetitions_);
  return repetitions_.Get();
}

bool CSSRepeatValue::IsAutoRepeatValue() const {
  return !repetitions_;
}

const CSSValueList& CSSRepeatValue::Values() const {
  return *values_.Get();
}

void CSSRepeatValue::TraceAfterDispatch(blink::Visitor* visitor) const {
  visitor->Trace(repetitions_);
  visitor->Trace(values_);

  CSSValue::TraceAfterDispatch(visitor);
}

bool CSSRepeatValue::Equals(const CSSRepeatValue& other) const {
  return repetitions_ == other.repetitions_ && values_ == other.values_;
}

}  // namespace blink::cssvalue
