// Copyright 2018 the Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/prepopulated_computed_style_property_map.h"

#include "third_party/blink/renderer/core/css/computed_style_css_value_mapping.h"
#include "third_party/blink/renderer/core/css/css_custom_property_declaration.h"
#include "third_party/blink/renderer/core/css/css_variable_data.h"
#include "third_party/blink/renderer/core/css/cssom/computed_style_property_map.h"
#include "third_party/blink/renderer/core/css/cssom/css_unparsed_value.h"
#include "third_party/blink/renderer/core/css/properties/css_property_ref.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

PrepopulatedComputedStylePropertyMap::PrepopulatedComputedStylePropertyMap(
    const Document& document,
    const ComputedStyle& style,
    const Vector<CSSPropertyID>& native_properties,
    const Vector<AtomicString>& custom_properties)
    : StylePropertyMapReadOnlyMainThread() {
  // NOTE: This may over-reserve as shorthand properties will get dropped from
  // being in the map.
  native_values_.ReserveCapacityForSize(native_properties.size());
  custom_values_.ReserveCapacityForSize(custom_properties.size());

  for (const auto& property_id : native_properties) {
    // Silently drop shorthand properties.
    DCHECK_NE(property_id, CSSPropertyID::kInvalid);
    if (CSSProperty::Get(property_id).IsShorthand())
      continue;

    UpdateNativeProperty(style, property_id);
  }

  for (const auto& property_name : custom_properties) {
    UpdateCustomProperty(document, style, property_name);
  }
}

unsigned PrepopulatedComputedStylePropertyMap::size() const {
  return native_values_.size() + custom_values_.size();
}

void PrepopulatedComputedStylePropertyMap::UpdateStyle(
    const Document& document,
    const ComputedStyle& style) {
  for (const auto& property_id : native_values_.Keys()) {
    DCHECK_NE(property_id, CSSPropertyID::kInvalid);
    UpdateNativeProperty(style, property_id);
  }

  for (const auto& property_name : custom_values_.Keys()) {
    UpdateCustomProperty(document, style, property_name);
  }
}

void PrepopulatedComputedStylePropertyMap::UpdateNativeProperty(
    const ComputedStyle& style,
    CSSPropertyID property_id) {
  native_values_.Set(property_id, CSSProperty::Get(property_id)
                                      .CSSValueFromComputedStyle(
                                          style, /* layout_object */ nullptr,
                                          /* allow_visited_style */ false));
}

void PrepopulatedComputedStylePropertyMap::UpdateCustomProperty(
    const Document& document,
    const ComputedStyle& style,
    const AtomicString& property_name) {
  CSSPropertyRef ref(property_name, document);
  const CSSValue* value = ref.GetProperty().CSSValueFromComputedStyle(
      style, /* layout_object */ nullptr,
      /* allow_visited_style */ false);
  if (!value)
    value = CSSUnparsedValue::Create()->ToCSSValue();

  custom_values_.Set(property_name, value);
}

const CSSValue* PrepopulatedComputedStylePropertyMap::GetProperty(
    CSSPropertyID property_id) const {
  return native_values_.at(property_id);
}

const CSSValue* PrepopulatedComputedStylePropertyMap::GetCustomProperty(
    AtomicString property_name) const {
  return custom_values_.at(property_name);
}

void PrepopulatedComputedStylePropertyMap::ForEachProperty(
    const IterationCallback& callback) {
  // Have to sort by all properties by code point, so we have to store
  // them in a buffer first.
  HeapVector<std::pair<CSSPropertyName, Member<const CSSValue>>> values;

  for (const auto& entry : native_values_) {
    DCHECK(entry.value);
    values.emplace_back(CSSPropertyName(entry.key), entry.value);
  }

  for (const auto& entry : custom_values_) {
    DCHECK(entry.value);
    values.emplace_back(CSSPropertyName(entry.key), entry.value);
  }

  std::sort(values.begin(), values.end(), [](const auto& a, const auto& b) {
    return ComputedStylePropertyMap::ComparePropertyNames(a.first, b.first);
  });

  for (const auto& value : values)
    callback(value.first, *value.second);
}

String PrepopulatedComputedStylePropertyMap::SerializationForShorthand(
    const CSSProperty&) const {
  // TODO(816722): Shorthands not yet supported for this style map.
  NOTREACHED();
  return "";
}

void PrepopulatedComputedStylePropertyMap::Trace(blink::Visitor* visitor) {
  visitor->Trace(native_values_);
  visitor->Trace(custom_values_);
  StylePropertyMapReadOnlyMainThread::Trace(visitor);
}

}  // namespace blink
