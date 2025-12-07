// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_dynamic_range_limit_mix_value.h"

#include "base/memory/values_equivalent.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink::cssvalue {

bool CSSDynamicRangeLimitMixValue::Equals(
    const CSSDynamicRangeLimitMixValue& other) const {
  if (limits_.size() != other.limits_.size()) {
    return false;
  }
  CHECK(limits_.size() == other.percentages_.size());
  for (size_t i = 0; i < limits_.size(); ++i) {
    if (!base::ValuesEquivalent(limits_[i], other.limits_[i]) ||
        !base::ValuesEquivalent(percentages_[i], other.percentages_[i])) {
      return false;
    }
  }
  return true;
}

String CSSDynamicRangeLimitMixValue::CustomCSSText() const {
  StringBuilder result;
  result.Append("dynamic-range-limit-mix(");
  for (size_t i = 0; i < limits_.size(); ++i) {
    result.Append(limits_[i]->CssText());
    result.Append(" ");
    result.Append(percentages_[i]->CssText());
    if (i != limits_.size() - 1) {
      result.Append(", ");
    }
  }
  result.Append(")");
  return result.ReleaseString();
}

void CSSDynamicRangeLimitMixValue::TraceAfterDispatch(
    blink::Visitor* visitor) const {
  visitor->Trace(limits_);
  visitor->Trace(percentages_);
  CSSValue::TraceAfterDispatch(visitor);
}

}  // namespace blink::cssvalue
