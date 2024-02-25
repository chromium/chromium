// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/inline_style_property_map.h"

#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_unparsed_declaration_value.h"
#include "third_party/blink/renderer/core/css/style_property_serializer.h"

namespace blink {

unsigned int InlineStylePropertyMap::size() const {
  const CSSPropertyValueSet* inline_style = owner_element_->InlineStyle();
  return inline_style ? inline_style->PropertyCount() : 0;
}

const CSSValue* InlineStylePropertyMap::GetProperty(
    CSSPropertyID property_id) const {
  const CSSPropertyValueSet* inline_style = owner_element_->InlineStyle();
  return inline_style ? inline_style->GetPropertyCSSValue(property_id)
                      : nullptr;
}

const CSSValue* InlineStylePropertyMap::GetCustomProperty(
    const AtomicString& property_name) const {
  const CSSPropertyValueSet* inline_style = owner_element_->InlineStyle();
  return inline_style ? inline_style->GetPropertyCSSValue(property_name)
                      : nullptr;
}

void InlineStylePropertyMap::SetProperty(CSSPropertyID property_id,
                                         const CSSValue& value) {
  DCHECK_NE(property_id, CSSPropertyID::kVariable);
  owner_element_->SetInlineStyleProperty(property_id, value);
  owner_element_->NotifyInlineStyleMutation();
}

bool InlineStylePropertyMap::SetShorthandProperty(
    CSSPropertyID property_id,
    const String& value,
    SecureContextMode secure_context_mode) {
  DCHECK(CSSProperty::Get(property_id).IsShorthand());
  const auto result =
      owner_element_->EnsureMutableInlineStyle().ParseAndSetProperty(
          property_id, value, false /* important */, secure_context_mode);
  return result != MutableCSSPropertyValueSet::kParseError;
}

void InlineStylePropertyMap::SetCustomProperty(
    const AtomicString& property_name,
    const CSSValue& value) {
  DCHECK(value.IsUnparsedDeclaration());
  const auto& variable_value = To<CSSUnparsedDeclarationValue>(value);
  CSSVariableData* variable_data = variable_value.VariableDataValue();
  owner_element_->SetInlineStyleProperty(
      CSSPropertyName(property_name),
      *MakeGarbageCollected<CSSUnparsedDeclarationValue>(
          variable_data, variable_value.ParserContext()));
  owner_element_->NotifyInlineStyleMutation();
}

void InlineStylePropertyMap::RemoveProperty(CSSPropertyID property_id) {
  owner_element_->RemoveInlineStyleProperty(property_id);
}

void InlineStylePropertyMap::RemoveCustomProperty(
    const AtomicString& property_name) {
  owner_element_->RemoveInlineStyleProperty(property_name);
}

void InlineStylePropertyMap::RemoveAllProperties() {
  owner_element_->RemoveAllInlineStyleProperties();
}

void InlineStylePropertyMap::ForEachProperty(IterationFunction visitor) {
  CSSPropertyValueSet& inline_style_set =
      owner_element_->EnsureMutableInlineStyle();
  for (unsigned i = 0; i < inline_style_set.PropertyCount(); i++) {
    const auto& property_reference = inline_style_set.PropertyAt(i);
    visitor(property_reference.Name(), property_reference.Value());
  }
}

String InlineStylePropertyMap::SerializationForShorthand(
    const CSSProperty& property) const {
  DCHECK(property.IsShorthand());
  if (const CSSPropertyValueSet* inline_style = owner_element_->InlineStyle()) {
    return StylePropertySerializer(*inline_style)
        .SerializeShorthand(property.PropertyID());
  }
  return "";
}

}  // namespace blink
