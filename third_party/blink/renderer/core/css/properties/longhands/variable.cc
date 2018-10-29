// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/variable.h"

#include "third_party/blink/renderer/core/css/css_custom_property_declaration.h"
#include "third_party/blink/renderer/core/css/property_registration.h"
#include "third_party/blink/renderer/core/css/property_registry.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

namespace {

void ApplyInitialValue(StyleResolverState& state,
                       const AtomicString& name,
                       const PropertyRegistration* registration) {
  bool is_inherited_property = !registration || registration->Inherits();
  state.Style()->RemoveVariable(name, is_inherited_property);
}

void ApplyInheritValue(StyleResolverState& state,
                       const AtomicString& name,
                       const PropertyRegistration* registration) {
  bool is_inherited_property = !registration || registration->Inherits();
  state.Style()->RemoveVariable(name, is_inherited_property);

  CSSVariableData* parent_value =
      state.ParentStyle()->GetVariable(name, is_inherited_property);

  if (!parent_value)
    return;

  state.Style()->SetVariable(name, parent_value, is_inherited_property);

  if (registration) {
    const CSSValue* parent_css_value =
        parent_value ? state.ParentStyle()->GetRegisteredVariable(
                           name, is_inherited_property)
                     : nullptr;
    state.Style()->SetRegisteredVariable(name, parent_css_value,
                                         is_inherited_property);
  }
}

}  // namespace

void Variable::ApplyValue(StyleResolverState& state,
                          const CSSValue& value) const {
  const CSSCustomPropertyDeclaration& declaration =
      ToCSSCustomPropertyDeclaration(value);
  const AtomicString& name = declaration.GetName();
  const PropertyRegistration* registration = nullptr;
  const PropertyRegistry* registry = state.GetDocument().GetPropertyRegistry();
  if (registry)
    registration = registry->Registration(name);

  bool is_inherited_property = !registration || registration->Inherits();
  bool initial = declaration.IsInitial(is_inherited_property);
  bool inherit = declaration.IsInherit(is_inherited_property);
  DCHECK(!(initial && inherit));

  // TODO(andruud): Use regular initial/inherit dispatch in StyleBuilder
  //                once custom properties are Ribbonized.
  if (initial) {
    ApplyInitialValue(state, name, registration);
  } else if (inherit) {
    ApplyInheritValue(state, name, registration);
  } else {
    state.Style()->SetVariable(name, declaration.Value(),
                               is_inherited_property);
    if (registration) {
      state.Style()->SetRegisteredVariable(name, nullptr,
                                           is_inherited_property);
    }
  }
}

}  // namespace blink
