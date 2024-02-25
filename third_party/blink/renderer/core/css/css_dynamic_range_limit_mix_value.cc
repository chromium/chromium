// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_dynamic_range_limit_mix_value.h"

#include "base/memory/values_equivalent.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink::cssvalue {

bool CSSDynamicRangeLimitMixValue::Equals(
    const CSSDynamicRangeLimitMixValue& other) const {
  return base::ValuesEquivalent(limit1_, other.limit1_) &&
         base::ValuesEquivalent(limit2_, other.limit2_) &&
         base::ValuesEquivalent(percentage_, other.percentage_);
}

String CSSDynamicRangeLimitMixValue::CustomCSSText() const {
  StringBuilder result;
  result.Append("dynamic-range-limit-mix(");
  result.Append(limit1_->CssText());
  result.Append(", ");
  result.Append(limit2_->CssText());
  result.Append(", ");
  result.Append(percentage_->CssText());
  result.Append(")");

  return result.ReleaseString();
}

void CSSDynamicRangeLimitMixValue::TraceAfterDispatch(
    blink::Visitor* visitor) const {
  visitor->Trace(limit1_);
  visitor->Trace(limit2_);
  visitor->Trace(percentage_);
  CSSValue::TraceAfterDispatch(visitor);
}

}  // namespace blink::cssvalue
