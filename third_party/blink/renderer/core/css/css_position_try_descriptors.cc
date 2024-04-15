// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_position_try_descriptors.h"

#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"

namespace blink {

CSSPositionTryDescriptors::CSSPositionTryDescriptors(
    MutableCSSPropertyValueSet& set,
    CSSRule* rule)
    : StyleRuleCSSStyleDeclaration(set, rule) {}

bool CSSPositionTryDescriptors::IsPropertyValid(
    CSSPropertyID property_id) const {
  if (property_id == CSSPropertyID::kVariable) {
    return false;
  }
  return CSSProperty::Get(property_id).IsValidForPositionTry();
}

void CSSPositionTryDescriptors::Trace(Visitor* visitor) const {
  StyleRuleCSSStyleDeclaration::Trace(visitor);
}

String CSSPositionTryDescriptors::Get(CSSPropertyID property_id) {
  return GetPropertyValueInternal(property_id);
}

void CSSPositionTryDescriptors::Set(const ExecutionContext* execution_context,
                                    CSSPropertyID property_id,
                                    const String& value,
                                    ExceptionState& exception_state) {
  const SecureContextMode mode = execution_context
                                     ? execution_context->GetSecureContextMode()
                                     : SecureContextMode::kInsecureContext;
  SetPropertyInternal(property_id, /* custom_property_name */ g_null_atom,
                      value, /* important */ false, mode, exception_state);
}

}  // namespace blink
