// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/inline_style_property_map.h"

#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/css_unparsed_declaration_value.h"
#include "third_party/blink/renderer/core/css/style_attribute_mutation_scope.h"
#include "third_party/blink/renderer/core/css/style_property_serializer.h"
#include "third_party/blink/renderer/core/dom/document.h"

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
  StyleAttributeMutationScope mutation_scope(owner_element_.Get());
  owner_element_->EnsureMutableInlineStyle().SetProperty(property_id, value);
  owner_element_->NotifyInlineStyleMutation();
  owner_element_->InvalidateStyleAttribute(false);
  mutation_scope.DidInvalidateStyleAttr();
  mutation_scope.EnqueueMutationRecord();
}

bool InlineStylePropertyMap::SetShorthandProperty(
    CSSPropertyID property_id,
    const String& value,
    SecureContextMode secure_context_mode) {
  DCHECK(CSSProperty::Get(property_id).IsShorthand());
  StyleAttributeMutationScope mutation_scope(owner_element_.Get());
  const auto result =
      owner_element_->EnsureMutableInlineStyle().ParseAndSetProperty(
          property_id, value, false /* important */, secure_context_mode,
          owner_element_->GetDocument().ElementSheet().Contents());
  if (result == MutableCSSPropertyValueSet::kParseError) {
    return false;
  }
  owner_element_->NotifyInlineStyleMutation();
  owner_element_->InvalidateStyleAttribute(false);
  mutation_scope.DidInvalidateStyleAttr();
  mutation_scope.EnqueueMutationRecord();
  return true;
}

void InlineStylePropertyMap::SetCustomProperty(
    const AtomicString& property_name,
    const CSSValue& value) {
  DCHECK(value.IsUnparsedDeclaration());
  const auto& variable_value = To<CSSUnparsedDeclarationValue>(value);
  CSSVariableData* variable_data = variable_value.VariableDataValue();
  StyleAttributeMutationScope mutation_scope(owner_element_.Get());
  owner_element_->EnsureMutableInlineStyle().SetProperty(
      CSSPropertyName(property_name),
      *MakeGarbageCollected<CSSUnparsedDeclarationValue>(
          variable_data, variable_value.ParserContext()));
  owner_element_->NotifyInlineStyleMutation();
  owner_element_->InvalidateStyleAttribute(false);
  mutation_scope.DidInvalidateStyleAttr();
  mutation_scope.EnqueueMutationRecord();
}

void InlineStylePropertyMap::RemoveProperty(CSSPropertyID property_id) {
  StyleAttributeMutationScope mutation_scope(owner_element_.Get());
  bool did_change =
      owner_element_->EnsureMutableInlineStyle().RemoveProperty(property_id);
  if (!did_change) {
    return;
  }
  owner_element_->ClearMutableInlineStyleIfEmpty();
  owner_element_->InvalidateStyleAttribute(false);
  mutation_scope.DidInvalidateStyleAttr();
  mutation_scope.EnqueueMutationRecord();
}

void InlineStylePropertyMap::RemoveCustomProperty(
    const AtomicString& property_name) {
  StyleAttributeMutationScope mutation_scope(owner_element_.Get());
  bool did_change =
      owner_element_->EnsureMutableInlineStyle().RemoveProperty(property_name);
  if (!did_change) {
    return;
  }
  owner_element_->ClearMutableInlineStyleIfEmpty();
  owner_element_->InvalidateStyleAttribute(false);
  mutation_scope.DidInvalidateStyleAttr();
  mutation_scope.EnqueueMutationRecord();
}

void InlineStylePropertyMap::RemoveAllProperties() {
  StyleAttributeMutationScope mutation_scope(owner_element_.Get());
  if (!owner_element_->InlineStyle()) {
    return;
  }
  owner_element_->EnsureMutableInlineStyle().Clear();
  owner_element_->ClearMutableInlineStyleIfEmpty();
  owner_element_->InvalidateStyleAttribute(false);
  mutation_scope.DidInvalidateStyleAttr();
  mutation_scope.EnqueueMutationRecord();
}

void InlineStylePropertyMap::ForEachProperty(IterationFunction visitor) {
  CSSPropertyValueSet& inline_style_set =
      owner_element_->EnsureMutableInlineStyle();
  for (const CSSPropertyValue& property : inline_style_set.Properties()) {
    visitor(property.Name(), property.Value());
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
