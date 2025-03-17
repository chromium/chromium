// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_function_descriptors.h"

#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"

namespace blink {

CSSFunctionDescriptors::CSSFunctionDescriptors(MutableCSSPropertyValueSet& set,
                                               CSSRule* rule)
    : StyleRuleCSSStyleDeclaration(set, rule) {}

bool CSSFunctionDescriptors::IsPropertyValid(CSSPropertyID property_id) const {
  switch (property_id) {
    case CSSPropertyID::kVariable:
    case CSSPropertyID::kResult:
      return true;
    default:
      return false;
  }
}

void CSSFunctionDescriptors::Trace(Visitor* visitor) const {
  StyleRuleCSSStyleDeclaration::Trace(visitor);
}

String CSSFunctionDescriptors::result() {
  return GetPropertyValueInternal(CSSPropertyID::kResult);
}

void CSSFunctionDescriptors::setResult(
    const ExecutionContext* execution_context,
    const String& value,
    ExceptionState& exception_state) {
  const SecureContextMode mode = execution_context
                                     ? execution_context->GetSecureContextMode()
                                     : SecureContextMode::kInsecureContext;
  SetPropertyInternal(CSSPropertyID::kResult,
                      /*custom_property_name=*/g_null_atom, value,
                      /*important=*/false, mode, exception_state);
}

}  // namespace blink
