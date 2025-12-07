// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/declared_style_property_map.h"

#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_style_rule.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/css_unparsed_declaration_value.h"
#include "third_party/blink/renderer/core/css/style_property_serializer.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"

namespace blink {

DeclaredStylePropertyMap::DeclaredStylePropertyMap(CSSStyleRule* owner_rule)
    : StylePropertyMap(), owner_rule_(owner_rule) {}

unsigned int DeclaredStylePropertyMap::size() const {
  if (!GetStyleRule()) {
    return 0;
  }
  return GetStyleRule()->Properties().PropertyCount();
}

const CSSValue* DeclaredStylePropertyMap::GetProperty(
    CSSPropertyID property_id) const {
  if (!GetStyleRule()) {
    return nullptr;
  }
  return GetStyleRule()->Properties().GetPropertyCSSValue(property_id);
}

const CSSValue* DeclaredStylePropertyMap::GetCustomProperty(
    const AtomicString& property_name) const {
  if (!GetStyleRule()) {
    return nullptr;
  }
  return GetStyleRule()->Properties().GetPropertyCSSValue(property_name);
}

void DeclaredStylePropertyMap::SetProperty(CSSPropertyID property_id,
                                           const CSSValue& value) {
  DCHECK_NE(property_id, CSSPropertyID::kVariable);
  if (!GetStyleRule()) {
    return;
  }
  CSSStyleSheet::RuleMutationScope mutation_scope(owner_rule_);
  GetStyleRule()->MutableProperties().SetProperty(property_id, value);
  NotifyRuleMutation();
}

bool DeclaredStylePropertyMap::SetShorthandProperty(
    CSSPropertyID property_id,
    const String& value,
    SecureContextMode secure_context_mode) {
  DCHECK(CSSProperty::Get(property_id).IsShorthand());
  CSSStyleSheet::RuleMutationScope mutation_scope(owner_rule_);
  const auto result = GetStyleRule()->MutableProperties().ParseAndSetProperty(
      property_id, value, false /* important */, secure_context_mode);
  NotifyRuleMutation();
  return result != MutableCSSPropertyValueSet::kParseError;
}

void DeclaredStylePropertyMap::SetCustomProperty(
    const AtomicString& property_name,
    const CSSValue& value) {
  if (!GetStyleRule()) {
    return;
  }
  CSSStyleSheet::RuleMutationScope mutation_scope(owner_rule_);

  const auto& variable_value = To<CSSUnparsedDeclarationValue>(value);
  CSSVariableData* variable_data = variable_value.VariableDataValue();
  GetStyleRule()->MutableProperties().SetProperty(
      CSSPropertyName(property_name),
      *MakeGarbageCollected<CSSUnparsedDeclarationValue>(
          variable_data, variable_value.ParserContext()));
  NotifyRuleMutation();
}

void DeclaredStylePropertyMap::RemoveProperty(CSSPropertyID property_id) {
  if (!GetStyleRule()) {
    return;
  }
  CSSStyleSheet::RuleMutationScope mutation_scope(owner_rule_);
  GetStyleRule()->MutableProperties().RemoveProperty(property_id);
  NotifyRuleMutation();
}

void DeclaredStylePropertyMap::RemoveCustomProperty(
    const AtomicString& property_name) {
  if (!GetStyleRule()) {
    return;
  }
  CSSStyleSheet::RuleMutationScope mutation_scope(owner_rule_);
  GetStyleRule()->MutableProperties().RemoveProperty(property_name);
  NotifyRuleMutation();
}

void DeclaredStylePropertyMap::RemoveAllProperties() {
  if (!GetStyleRule()) {
    return;
  }
  CSSStyleSheet::RuleMutationScope mutation_scope(owner_rule_);
  GetStyleRule()->MutableProperties().Clear();
  NotifyRuleMutation();
}

void DeclaredStylePropertyMap::ForEachProperty(IterationFunction visitor) {
  if (!GetStyleRule()) {
    return;
  }
  const CSSPropertyValueSet& declared_style_set = GetStyleRule()->Properties();
  for (const CSSPropertyValue& property_reference :
       declared_style_set.Properties()) {
    visitor(property_reference.Name(), property_reference.Value());
  }
}

StyleRule* DeclaredStylePropertyMap::GetStyleRule() const {
  if (!owner_rule_) {
    return nullptr;
  }
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

void DeclaredStylePropertyMap::NotifyRuleMutation() {
  // Similar to StyleRuleCSSStyleDeclaration::DidMutate(),
  // except that owner_rule_ can never be anything but a CSSStyleRule.
  if (owner_rule_ && owner_rule_->parentStyleSheet()) {
    StyleSheetContents* parent_contents =
        owner_rule_->parentStyleSheet()->Contents();
    parent_contents->NotifyRuleChanged(GetStyleRule());
  }
}

}  // namespace blink
