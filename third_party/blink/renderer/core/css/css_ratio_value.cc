// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_ratio_value.h"
#include "base/memory/values_equivalent.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {
namespace cssvalue {

CSSRatioValue::CSSRatioValue(const CSSPrimitiveValue& first,
                             const CSSPrimitiveValue& second)
    : CSSValue(kRatioClass), first_(&first), second_(&second) {}

String CSSRatioValue::CustomCSSText() const {
  StringBuilder builder;
  builder.Append(first_->CssText());
  builder.Append(" / ");
  builder.Append(second_->CssText());
  return builder.ReleaseString();
}

bool CSSRatioValue::Equals(const CSSRatioValue& other) const {
  return base::ValuesEquivalent(first_, other.first_) &&
         base::ValuesEquivalent(second_, other.second_);
}

}  // namespace cssvalue
}  // namespace blink
