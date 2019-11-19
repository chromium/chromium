// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/declared_style_property_map.h"

#include "third_party/blink/renderer/core/css/css_custom_property_declaration.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_style_rule.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/css_variable_reference_value.h"
#include "third_party/blink/renderer/core/css/style_property_serializer.h"
#include "third_party/blink/renderer/core/css/style_rule.h"

namespace blink {

DeclaredStylePropertyMap::DeclaredStylePropertyMap(CSSStyleRule* owner_rule)
    : StylePropertyMap(), owner_rule_(owner_rule) {}

unsigned int DeclaredStylePropertyMap::size() const {
  if (!GetStyleRule())
    return 0;
  return GetStyleRule()->Properties().PropertyCount();
}

const CSSValue* DeclaredStylePropertyMap::GetProperty(
    CSSPropertyID property_id) const {
  if (!GetStyleRule())
    return nullptr;
  return GetStyleRule()->Properties().GetPropertyCSSValue(property_id);
}

const CSSValue* DeclaredStylePropertyMap::GetCustomProperty(
    AtomicString property_name) const {
  if (!GetStyleRule())
    return nullptr;
  return GetStyleRule()->Properties().GetPropertyCSSValue(property_name);
}

void DeclaredStylePropertyMap::SetProperty(CSSPropertyID property_id,
                                           const CSSValue& value) {
  if (!GetStyleRule())
    return;
  CSSStyleSheet::RuleMutationScope mutation_scope(owner_rule_);
  GetStyleRule()->MutableProperties().SetProperty(property_id, value);
}

bool DeclaredStylePropertyMap::SetShorthandProperty(
    CSSPropertyID property_id,
    const String& value,
    SecureContextMode secure_context_mode) {
  DCHECK(CSSProperty::Get(property_id).IsShorthand());
  CSSStyleSheet::RuleMutationScope mutation_scope(owner_rule_);
  const auto result = GetStyleRule()->MutableProperties().SetProperty(
      property_id, value, false /* important */, secure_context_mode);
  return result.did_parse;
}

void DeclaredStylePropertyMap::SetCustomProperty(
    const AtomicString& property_name,
    const CSSValue& value) {
  if (!GetStyleRule())
    return;
  CSSStyleSheet::RuleMutationScope mutation_scope(owner_rule_);

  auto* variable_data =
      To<CSSVariableReferenceValue>(value).VariableDataValue();
  GetStyleRule()->MutableProperties().SetProperty(
      CSSPropertyID::kVariable,
      *MakeGarbageCollected<CSSCustomPropertyDeclaration>(property_name,
                                                          variable_data));
}

void DeclaredStylePropertyMap::RemoveProperty(CSSPropertyID property_id) {
  if (!GetStyleRule())
    return;
  CSSStyleSheet::RuleMutationScope mutation_scope(owner_rule_);
  GetStyleRule()->MutableProperties().RemoveProperty(property_id);
}

void DeclaredStylePropertyMap::RemoveCustomProperty(
    const AtomicString& property_name) {
  if (!GetStyleRule())
    return;
  CSSStyleSheet::RuleMutationScope mutation_scope(owner_rule_);
  GetStyleRule()->MutableProperties().RemoveProperty(property_name);
}

void DeclaredStylePropertyMap::RemoveAllProperties() {
  if (!GetStyleRule())
    return;
  CSSStyleSheet::RuleMutationScope mutation_scope(owner_rule_);
  GetStyleRule()->MutableProperties().Clear();
}

void DeclaredStylePropertyMap::ForEachProperty(
    const IterationCallback& callback) {
  if (!GetStyleRule())
    return;
  const CSSPropertyValueSet& declared_style_set = GetStyleRule()->Properties();
  for (unsigned i = 0; i < declared_style_set.PropertyCount(); i++) {
    const auto& property_reference = declared_style_set.PropertyAt(i);
    callback(property_reference.Name(), property_reference.Value());
  }
}

StyleRule* DeclaredStylePropertyMap::GetStyleRule() const {
  if (!owner_rule_)
    return nullptr;
  return owner_rule_->GetStyleRule();
}

String DeclaredStylePropertyMap::SerializationForShorthand(
    const CSSProperty& property) const {
  DCHECK(property.IsShorthand());
  if (StyleRule* style_rule = GetStyleRule()) {
    return StylePropertySerializer(style_rule->Properties())
        .SerializeShorthand(property.PropertyID());
  }

  return "";
}

}  // namespace blink
