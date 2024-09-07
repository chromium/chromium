// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/computed_style_property_map.h"

#include "third_party/blink/renderer/core/css/computed_style_css_value_mapping.h"
#include "third_party/blink/renderer/core/css/css_function_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_unparsed_declaration_value.h"
#include "third_party/blink/renderer/core/css/css_variable_data.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/css/properties/css_property_ref.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

unsigned int ComputedStylePropertyMap::size() const {
  const ComputedStyle* style = UpdateStyle();
  if (!style) {
    return 0;
  }

  DCHECK(StyledElement());
  const Document& document = StyledElement()->GetDocument();
  return CSSComputedStyleDeclaration::ComputableProperties(
             StyledElement()->GetExecutionContext())
             .size() +
         ComputedStyleCSSValueMapping::GetVariables(
             *style, document.GetPropertyRegistry(),
             CSSValuePhase::kComputedValue)
             .size();
}

bool ComputedStylePropertyMap::ComparePropertyNames(
    const CSSPropertyName& name_a,
    const CSSPropertyName& name_b) {
  AtomicString a = name_a.ToAtomicString();
  AtomicString b = name_b.ToAtomicString();
  if (a.StartsWith("--")) {
    return b.StartsWith("--") && WTF::CodeUnitCompareLessThan(a, b);
  }
  if (a.StartsWith("-")) {
    return b.StartsWith("--") ||
           (b.StartsWith("-") && WTF::CodeUnitCompareLessThan(a, b));
  }
  return b.StartsWith("-") || WTF::CodeUnitCompareLessThan(a, b);
}

Element* ComputedStylePropertyMap::StyledElement() const {
  DCHECK(element_);
  if (!pseudo_id_) {
    return element_.Get();
  }
  if (PseudoElement* pseudo_element = element_->GetPseudoElement(pseudo_id_)) {
    return pseudo_element;
  }
  return nullptr;
}

const ComputedStyle* ComputedStylePropertyMap::UpdateStyle() const {
  Element* element = StyledElement();
  if (!element || !element->InActiveDocument()) {
    return nullptr;
  }

  // Update style before getting the value for the property
  // This could cause the element to be blown away. This code is copied from
  // CSSComputedStyleDeclaration::GetPropertyCSSValue.
  element->GetDocument().UpdateStyleAndLayoutTreeForElement(
      element, DocumentUpdateReason::kComputedStyle);
  element = StyledElement();
  if (!element) {
    return nullptr;
  }
  // This is copied from CSSComputedStyleDeclaration::computeComputedStyle().
  // PseudoIdNone must be used if node() is a PseudoElement.
  const ComputedStyle* style = element->EnsureComputedStyle(
      element->IsPseudoElement() ? kPseudoIdNone : pseudo_id_);
  element = StyledElement();
  if (!element || !element->InActiveDocument() || !style) {
    return nullptr;
  }
  return style;
}

const CSSValue* ComputedStylePropertyMap::GetProperty(
    CSSPropertyID property_id) const {
  const ComputedStyle* style = UpdateStyle();
  if (!style) {
    return nullptr;
  }

  return ComputedStyleUtils::ComputedPropertyValue(
      CSSProperty::Get(property_id), *style);
}

const CSSValue* ComputedStylePropertyMap::GetCustomProperty(
    const AtomicString& property_name) const {
  const ComputedStyle* style = UpdateStyle();
  if (!style) {
    return nullptr;
  }
  CSSPropertyRef ref(property_name, element_->GetDocument());
  return ref.GetProperty().CSSValueFromComputedStyle(
      *style, nullptr /* layout_object */, false /* allow_visited_style */,
      CSSValuePhase::kComputedValue);
}

void ComputedStylePropertyMap::ForEachProperty(IterationFunction visitor) {
  const ComputedStyle* style = UpdateStyle();
  if (!style) {
    return;
  }

  DCHECK(StyledElement());
  const Document& document = StyledElement()->GetDocument();
  // Have to sort by all properties by code point, so we have to store
  // them in a buffer first.
  HeapVector<std::pair<CSSPropertyName, Member<const CSSValue>>> values;
  for (const CSSProperty* property :
       CSSComputedStyleDeclaration::ComputableProperties(
           StyledElement()->GetExecutionContext())) {
    DCHECK(property);
    DCHECK(!property->IDEquals(CSSPropertyID::kVariable));
    const CSSValue* value = property->CSSValueFromComputedStyle(
        *style, nullptr /* layout_object */, false,
        CSSValuePhase::kComputedValue);
    if (value) {
      values.emplace_back(CSSPropertyName(property->PropertyID()), value);
    }
  }

  const PropertyRegistry* registry = document.GetPropertyRegistry();

  for (const auto& name_value : ComputedStyleCSSValueMapping::GetVariables(
           *style, registry, CSSValuePhase::kComputedValue)) {
    values.emplace_back(CSSPropertyName(name_value.key), name_value.value);
  }

  std::sort(values.begin(), values.end(), [](const auto& a, const auto& b) {
    return ComparePropertyNames(a.first, b.first);
  });

  for (const auto& value : values) {
    visitor(value.first, *value.second);
  }
}

String ComputedStylePropertyMap::SerializationForShorthand(
    const CSSProperty& property) const {
  DCHECK(property.IsShorthand());
  const ComputedStyle* style = UpdateStyle();
  if (!style) {
    return "";
  }

  if (const CSSValue* value = property.CSSValueFromComputedStyle(
          *style, nullptr /* layout_object */, false,
          CSSValuePhase::kComputedValue)) {
    return value->CssText();
  }

  return "";
}

}  // namespace blink
