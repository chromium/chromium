// Copyright 2016 the chromium authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/style_property_map_read_only.h"

#include "third_party/blink/renderer/core/css/css_custom_property_declaration.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/css_variable_reference_value.h"
#include "third_party/blink/renderer/core/css/cssom/css_style_value.h"
#include "third_party/blink/renderer/core/css/cssom/css_unparsed_value.h"
#include "third_party/blink/renderer/core/css/cssom/css_unsupported_style_value.h"
#include "third_party/blink/renderer/core/css/cssom/style_value_factory.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/property_registration.h"
#include "third_party/blink/renderer/core/css/property_registry.h"
#include "third_party/blink/renderer/core/css_property_names.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

namespace {

class StylePropertyMapIterationSource final
    : public PairIterable<String, CSSStyleValueVector>::IterationSource {
 public:
  explicit StylePropertyMapIterationSource(
      HeapVector<StylePropertyMapReadOnly::StylePropertyMapEntry> values)
      : index_(0), values_(values) {}

  bool Next(ScriptState*,
            String& key,
            CSSStyleValueVector& value,
            ExceptionState&) override {
    if (index_ >= values_.size())
      return false;

    const StylePropertyMapReadOnly::StylePropertyMapEntry& pair =
        values_.at(index_++);
    key = pair.first;
    value = pair.second;
    return true;
  }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(values_);
    PairIterable<String, CSSStyleValueVector>::IterationSource::Trace(visitor);
  }

 private:
  wtf_size_t index_;
  const HeapVector<StylePropertyMapReadOnly::StylePropertyMapEntry> values_;
};

}  // namespace

CSSStyleValue* StylePropertyMapReadOnly::get(
    const ExecutionContext* execution_context,
    const String& property_name,
    ExceptionState& exception_state) {
  const CSSPropertyID property_id = cssPropertyID(property_name);
  if (property_id == CSSPropertyInvalid) {
    exception_state.ThrowTypeError("Invalid propertyName: " + property_name);
    return nullptr;
  }

  DCHECK(isValidCSSPropertyID(property_id));
  const CSSProperty& property = CSSProperty::Get(property_id);
  if (property.IsShorthand())
    return GetShorthandProperty(property);

  AtomicString custom_property_name = property_id == CSSPropertyVariable
                                          ? AtomicString(property_name)
                                          : g_null_atom;

  const CSSValue* value =
      (property_id == CSSPropertyVariable)
          ? GetCustomProperty(*execution_context, custom_property_name)
          : GetProperty(property_id);
  if (!value)
    return nullptr;

  // Custom properties count as repeated whenever we have a CSSValueList.
  if (property.IsRepeated() ||
      (property_id == CSSPropertyVariable && value->IsValueList())) {
    CSSStyleValueVector values = StyleValueFactory::CssValueToStyleValueVector(
        property_id, custom_property_name, *value);
    return values.IsEmpty() ? nullptr : values[0];
  }

  return StyleValueFactory::CssValueToStyleValue(property_id,
                                                 custom_property_name, *value);
}

CSSStyleValueVector StylePropertyMapReadOnly::getAll(
    const ExecutionContext* execution_context,
    const String& property_name,
    ExceptionState& exception_state) {
  CSSPropertyID property_id = cssPropertyID(property_name);
  if (property_id == CSSPropertyInvalid) {
    exception_state.ThrowTypeError("Invalid propertyName: " + property_name);
    return CSSStyleValueVector();
  }

  DCHECK(isValidCSSPropertyID(property_id));
  const CSSProperty& property = CSSProperty::Get(property_id);
  if (property.IsShorthand()) {
    CSSStyleValueVector values;
    if (CSSStyleValue* value = GetShorthandProperty(property))
      values.push_back(value);
    return values;
  }

  AtomicString custom_property_name = property_id == CSSPropertyVariable
                                          ? AtomicString(property_name)
                                          : g_null_atom;

  const CSSValue* value =
      (property_id == CSSPropertyVariable)
          ? GetCustomProperty(*execution_context, custom_property_name)
          : GetProperty(property_id);
  if (!value)
    return CSSStyleValueVector();

  return StyleValueFactory::CssValueToStyleValueVector(
      property_id, custom_property_name, *value);
}

bool StylePropertyMapReadOnly::has(const ExecutionContext* execution_context,
                                   const String& property_name,
                                   ExceptionState& exception_state) {
  return !getAll(execution_context, property_name, exception_state).IsEmpty();
}

const CSSValue* StylePropertyMapReadOnly::GetCustomProperty(
    const ExecutionContext& execution_context,
    const AtomicString& property_name) {
  const CSSValue* value = GetCustomProperty(property_name);

  const auto* document = DynamicTo<Document>(execution_context);
  if (!document)
    return value;

  return PropertyRegistry::ParseIfRegistered(*document, property_name, value);
}

StylePropertyMapReadOnly::IterationSource*
StylePropertyMapReadOnly::StartIteration(ScriptState* script_state,
                                         ExceptionState&) {
  HeapVector<StylePropertyMapReadOnly::StylePropertyMapEntry> result;

  const ExecutionContext& execution_context =
      *ExecutionContext::From(script_state);

  ForEachProperty([&result, &execution_context](const String& property_name,
                                                const CSSValue& css_value) {
    const auto property_id = cssPropertyID(property_name);

    const CSSValue* value = &css_value;
    AtomicString custom_property_name = g_null_atom;

    if (property_id == CSSPropertyVariable) {
      custom_property_name = AtomicString(property_name);

      const auto* document = DynamicTo<Document>(execution_context);
      if (document) {
        value = PropertyRegistry::ParseIfRegistered(
            *document, custom_property_name, value);
      }
    }

    auto values = StyleValueFactory::CssValueToStyleValueVector(
        property_id, custom_property_name, *value);
    result.emplace_back(property_name, std::move(values));
  });

  return new StylePropertyMapIterationSource(result);
}

CSSStyleValue* StylePropertyMapReadOnly::GetShorthandProperty(
    const CSSProperty& property) {
  DCHECK(property.IsShorthand());
  const auto serialization = SerializationForShorthand(property);
  if (serialization.IsEmpty())
    return nullptr;
  return CSSUnsupportedStyleValue::Create(property.PropertyID(), g_null_atom,
                                          serialization);
}

}  // namespace blink
