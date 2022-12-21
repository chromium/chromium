// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_layout_function_value.h"

#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {
namespace cssvalue {

CSSLayoutFunctionValue::CSSLayoutFunctionValue(CSSCustomIdentValue* name,
                                               bool is_inline)
    : CSSValue(kLayoutFunctionClass), name_(name), is_inline_(is_inline) {}

String CSSLayoutFunctionValue::CustomCSSText() const {
  StringBuilder result;
  if (is_inline_) {
    result.Append("inline-");
  }
  result.Append("layout(");
  result.Append(name_->CustomCSSText());
  result.Append(')');
  return result.ReleaseString();
}

AtomicString CSSLayoutFunctionValue::GetName() const {
  return name_->Value();
}

bool CSSLayoutFunctionValue::Equals(const CSSLayoutFunctionValue& other) const {
  return GetName() == other.GetName() && IsInline() == other.IsInline();
}

void CSSLayoutFunctionValue::TraceAfterDispatch(blink::Visitor* visitor) const {
  visitor->Trace(name_);
  CSSValue::TraceAfterDispatch(visitor);
}

}  // namespace cssvalue
}  // namespace blink
