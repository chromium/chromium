// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_alternate_value.h"
#include "base/memory/values_equivalent.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink::cssvalue {

CSSAlternateValue::CSSAlternateValue(const CSSFunctionValue& function,
                                     const CSSValueList& alias_list)
    : CSSValue(kAlternateClass), function_(function), aliases_(alias_list) {}

String CSSAlternateValue::CustomCSSText() const {
  StringBuilder builder;
  builder.Append(getValueName(function_->FunctionType()));
  builder.Append('(');
  builder.Append(aliases_->CssText());
  builder.Append(')');
  return builder.ReleaseString();
}

bool CSSAlternateValue::Equals(const CSSAlternateValue& other) const {
  return base::ValuesEquivalent(function_, other.function_) &&
         base::ValuesEquivalent(aliases_, other.aliases_);
}

}  // namespace blink::cssvalue
