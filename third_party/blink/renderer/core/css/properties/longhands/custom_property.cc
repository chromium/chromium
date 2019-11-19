// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/custom_property.h"

#include "third_party/blink/renderer/core/css/css_custom_property_declaration.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_local_context.h"
#include "third_party/blink/renderer/core/css/parser/css_variable_parser.h"
#include "third_party/blink/renderer/core/css/property_registration.h"
#include "third_party/blink/renderer/core/css/property_registry.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder_converter.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

CustomProperty::CustomProperty(const AtomicString& name,
                               const Document& document)
    : CustomProperty(name, PropertyRegistration::From(&document, name)) {}

CustomProperty::CustomProperty(const AtomicString& name,
                               const PropertyRegistry* registry)
    : CustomProperty(name, registry ? registry->Registration(name) : nullptr) {}

CustomProperty::CustomProperty(const AtomicString& name,
                               const PropertyRegistration* registration)
    : Variable(!registration || registration->Inherits()),
      name_(name),
      registration_(registration) {}

const AtomicString& CustomProperty::GetPropertyNameAtomicString() const {
  return name_;
}

CSSPropertyName CustomProperty::GetCSSPropertyName() const {
  return CSSPropertyName(name_);
}

void CustomProperty::ApplyInitial(StyleResolverState& state) const {
  bool is_inherited_property = IsInherited();

  if (!registration_) {
    state.Style()->SetVariableData(name_, nullptr, is_inherited_property);
    return;
  }

  state.Style()->SetVariableData(name_, registration_->InitialVariableData(),
                                 is_inherited_property);
  state.Style()->SetVariableValue(name_, registration_->Initial(),
                                  is_inherited_property);
}

void CustomProperty::ApplyInherit(StyleResolverState& state) const {
  bool is_inherited_property = IsInherited();

  CSSVariableData* parent_data =
      state.ParentStyle()->GetVariableData(name_, is_inherited_property);

  state.Style()->SetVariableData(name_, parent_data, is_inherited_property);

  if (registration_) {
    const CSSValue* parent_value = state.ParentStyle()->GetVariableValue(name_);
    state.Style()->SetVariableValue(name_, parent_value, is_inherited_property);
  }
}

void CustomProperty::ApplyValue(StyleResolverState& state,
                                const CSSValue& value) const {
  if (value.IsInvalidVariableValue()) {
    DCHECK(RuntimeEnabledFeatures::CSSCascadeEnabled());
    state.Style()->SetVariableData(name_, nullptr, IsInherited());
    if (registration_)
      state.Style()->SetVariableValue(name_, nullptr, IsInherited());
    return;
  }

  const auto& declaration = To<CSSCustomPropertyDeclaration>(value);

  bool is_inherited_property = IsInherited();
  bool initial = declaration.IsInitial(is_inherited_property);
  bool inherit = declaration.IsInherit(is_inherited_property);
  DCHECK(!(initial && inherit));

  // TODO(andruud): Use regular initial/inherit dispatch in StyleBuilder
  //                once custom properties are Ribbonized.
  if (initial) {
    ApplyInitial(state);
  } else if (inherit) {
    ApplyInherit(state);
  } else {
    if (!RuntimeEnabledFeatures::CSSCascadeEnabled()) {
      state.Style()->SetVariableData(name_, declaration.Value(),
                                     is_inherited_property);
      if (registration_)
        state.Style()->SetVariableValue(name_, nullptr, is_inherited_property);
      return;
    }

    scoped_refptr<CSSVariableData> data = declaration.Value();
    DCHECK(!data->NeedsVariableResolution());

    state.Style()->SetVariableData(name_, data, is_inherited_property);

    if (registration_) {
      // TODO(andruud): Store CSSParserContext on CSSCustomPropertyDeclaration
      // and use that.
      const CSSParserContext* context =
          StrictCSSParserContext(state.GetDocument().GetSecureContextMode());
      auto mode = CSSParserLocalContext::VariableMode::kTyped;
      auto local_context = CSSParserLocalContext().WithVariableMode(mode);
      CSSParserTokenRange range = data->TokenRange();
      const CSSValue* registered_value =
          ParseSingleValue(range, *context, local_context);
      if (!registered_value) {
        if (is_inherited_property)
          ApplyInherit(state);
        else
          ApplyInitial(state);
        return;
      }

      registered_value = &StyleBuilderConverter::ConvertRegisteredPropertyValue(
          state, *registered_value, data->BaseURL(), data->Charset());
      data = StyleBuilderConverter::ConvertRegisteredPropertyVariableData(
          *registered_value, data->IsAnimationTainted());

      state.Style()->SetVariableData(name_, data, is_inherited_property);
      state.Style()->SetVariableValue(name_, registered_value,
                                      is_inherited_property);
    }
  }
}

const CSSValue* CustomProperty::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  using VariableMode = CSSParserLocalContext::VariableMode;

  switch (local_context.GetVariableMode()) {
    case VariableMode::kTyped:
      return ParseTyped(range, context, local_context);
    case VariableMode::kUntyped:
      return ParseUntyped(range, context, local_context);
    case VariableMode::kValidatedUntyped:
      if (registration_ && !ParseTyped(range, context, local_context))
        return nullptr;
      return ParseUntyped(range, context, local_context);
  }
}

const CSSValue* CustomProperty::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    bool allow_visited_style) const {
  if (registration_) {
    const CSSValue* value = style.GetVariableValue(name_, IsInherited());
    if (value)
      return value;
    // If we don't have CSSValue for this registered property, it means that
    // that the property was not registered at the time |style| was calculated,
    // hence we proceed with unregistered behavior.
  }

  CSSVariableData* data = style.GetVariableData(name_, IsInherited());

  if (!data)
    return nullptr;

  return MakeGarbageCollected<CSSCustomPropertyDeclaration>(name_, data);
}

const CSSValue* CustomProperty::ParseUntyped(
    CSSParserTokenRange range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return CSSVariableParser::ParseDeclarationValue(
      name_, range, local_context.IsAnimationTainted(), context);
}

const CSSValue* CustomProperty::ParseTyped(
    CSSParserTokenRange range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  if (!registration_)
    return ParseUntyped(range, context, local_context);
  return registration_->Syntax().Parse(range, &context,
                                       local_context.IsAnimationTainted());
}

}  // namespace blink
