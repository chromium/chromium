// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/computed_style_property_map.h"

#include "third_party/blink/renderer/core/css/computed_style_css_value_mapping.h"
#include "third_party/blink/renderer/core/css/css_custom_property_declaration.h"
#include "third_party/blink/renderer/core/css/css_function_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_variable_data.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/css/properties/css_property_ref.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

unsigned int ComputedStylePropertyMap::size() const {
  const ComputedStyle* style = UpdateStyle();
  if (!style)
    return 0;

  DCHECK(StyledNode());
  const Document& document = StyledNode()->GetDocument();
  return CSSComputedStyleDeclaration::ComputableProperties(
             StyledNode()->GetExecutionContext())
             .size() +
         ComputedStyleCSSValueMapping::GetVariables(
             *style, document.GetPropertyRegistry())
             .size();
}

bool ComputedStylePropertyMap::ComparePropertyNames(
    const CSSPropertyName& name_a,
    const CSSPropertyName& name_b) {
  AtomicString a = name_a.ToAtomicString();
  AtomicString b = name_b.ToAtomicString();
  if (a.StartsWith("--"))
    return b.StartsWith("--") && WTF::CodeUnitCompareLessThan(a, b);
  if (a.StartsWith("-")) {
    return b.StartsWith("--") ||
           (b.StartsWith("-") && WTF::CodeUnitCompareLessThan(a, b));
  }
  return b.StartsWith("-") || WTF::CodeUnitCompareLessThan(a, b);
}

Node* ComputedStylePropertyMap::StyledNode() const {
  DCHECK(node_);
  if (!pseudo_id_)
    return node_;
  if (auto* element_node = DynamicTo<Element>(node_.Get())) {
    if (PseudoElement* element = element_node->GetPseudoElement(pseudo_id_)) {
      return element;
    }
  }
  return nullptr;
}

const ComputedStyle* ComputedStylePropertyMap::UpdateStyle() const {
  Node* node = StyledNode();
  if (!node || !node->InActiveDocument())
    return nullptr;

  // Update style before getting the value for the property
  // This could cause the node to be blown away. This code is copied from
  // CSSComputedStyleDeclaration::GetPropertyCSSValue.
  node->GetDocument().UpdateStyleAndLayoutTreeForNode(node);
  node = StyledNode();
  if (!node)
    return nullptr;
  // This is copied from CSSComputedStyleDeclaration::computeComputedStyle().
  // PseudoIdNone must be used if node() is a PseudoElement.
  const ComputedStyle* style = node->EnsureComputedStyle(
      node->IsPseudoElement() ? kPseudoIdNone : pseudo_id_);
  node = StyledNode();
  if (!node || !node->InActiveDocument() || !style)
    return nullptr;
  return style;
}

const CSSValue* ComputedStylePropertyMap::GetProperty(
    CSSPropertyID property_id) const {
  const ComputedStyle* style = UpdateStyle();
  if (!style)
    return nullptr;

  return ComputedStyleUtils::ComputedPropertyValue(
      CSSProperty::Get(property_id), *style);
}

const CSSValue* ComputedStylePropertyMap::GetCustomProperty(
    const AtomicString& property_name) const {
  const ComputedStyle* style = UpdateStyle();
  if (!style)
    return nullptr;
  CSSPropertyRef ref(property_name, node_->GetDocument());
  return ref.GetProperty().CSSValueFromComputedStyle(
      *style, nullptr /* layout_object */, false /* allow_visited_style */);
}

void ComputedStylePropertyMap::ForEachProperty(IterationFunction visitor) {
  const ComputedStyle* style = UpdateStyle();
  if (!style)
    return;

  DCHECK(StyledNode());
  const Document& document = StyledNode()->GetDocument();
  // Have to sort by all properties by code point, so we have to store
  // them in a buffer first.
  HeapVector<std::pair<CSSPropertyName, Member<const CSSValue>>> values;
  for (const CSSProperty* property :
       CSSComputedStyleDeclaration::ComputableProperties(
           StyledNode()->GetExecutionContext())) {
    DCHECK(property);
    DCHECK(!property->IDEquals(CSSPropertyID::kVariable));
    const CSSValue* value = property->CSSValueFromComputedStyle(
        *style, nullptr /* layout_object */, false);
    if (value)
      values.emplace_back(CSSPropertyName(property->PropertyID()), value);
  }

  const PropertyRegistry* registry = document.GetPropertyRegistry();

  for (const auto& name_value :
       ComputedStyleCSSValueMapping::GetVariables(*style, registry)) {
    values.emplace_back(CSSPropertyName(name_value.key), name_value.value);
  }

  std::sort(values.begin(), values.end(), [](const auto& a, const auto& b) {
    return ComparePropertyNames(a.first, b.first);
  });

  for (const auto& value : values)
    visitor(value.first, *value.second);
}

String ComputedStylePropertyMap::SerializationForShorthand(
    const CSSProperty& property) const {
  DCHECK(property.IsShorthand());
  const ComputedStyle* style = UpdateStyle();
  if (!style)
    return "";

  if (const CSSValue* value = property.CSSValueFromComputedStyle(
          *style, nullptr /* layout_object */, false)) {
    return value->CssText();
  }

  NOTREACHED();
  return "";
}

}  // namespace blink
