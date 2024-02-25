// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/style_property_map_read_only_main_thread.h"

#include "third_party/blink/renderer/core/css/css_property_name.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_unparsed_declaration_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/cssom/css_style_value.h"
#include "third_party/blink/renderer/core/css/cssom/css_unparsed_value.h"
#include "third_party/blink/renderer/core/css/cssom/css_unsupported_style_value.h"
#include "third_party/blink/renderer/core/css/cssom/style_value_factory.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/property_registration.h"
#include "third_party/blink/renderer/core/css/property_registry.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

namespace {

class StylePropertyMapIterationSource final
    : public PairSyncIterable<StylePropertyMapReadOnly>::IterationSource {
 public:
  explicit StylePropertyMapIterationSource(
      HeapVector<StylePropertyMapReadOnlyMainThread::StylePropertyMapEntry>
          values)
      : index_(0), values_(values) {}

  bool FetchNextItem(ScriptState*,
                     String& key,
                     CSSStyleValueVector& value,
                     ExceptionState&) override {
    if (index_ >= values_.size()) {
      return false;
    }

    const StylePropertyMapReadOnlyMainThread::StylePropertyMapEntry& pair =
        values_.at(index_++);
    key = pair.first;
    value = pair.second;
    return true;
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(values_);
    PairSyncIterable<StylePropertyMapReadOnly>::IterationSource::Trace(visitor);
  }

 private:
  wtf_size_t index_;
  const HeapVector<StylePropertyMapReadOnlyMainThread::StylePropertyMapEntry>
      values_;
};

}  // namespace

CSSStyleValue* StylePropertyMapReadOnlyMainThread::get(
    const ExecutionContext* execution_context,
    const String& property_name,
    ExceptionState& exception_state) const {
  std::optional<CSSPropertyName> name =
      CSSPropertyName::From(execution_context, property_name);

  if (!name) {
    exception_state.ThrowTypeError("Invalid propertyName: " + property_name);
    return nullptr;
  }

  if (CSSProperty::IsShorthand(*name)) {
    return GetShorthandProperty(*name);
  }

  const CSSValue* value = (name->IsCustomProperty())
                              ? GetCustomProperty(name->ToAtomicString())
                              : GetProperty(name->Id());
  if (!value) {
    return nullptr;
  }

  // Custom properties count as repeated whenever we have a CSSValueList.
  if (CSSProperty::IsRepeated(*name) ||
      (name->IsCustomProperty() && value->IsValueList())) {
    CSSStyleValueVector values =
        StyleValueFactory::CssValueToStyleValueVector(*name, *value);
    return values.empty() ? nullptr : values[0];
  }

  return StyleValueFactory::CssValueToStyleValue(*name, *value);
}

CSSStyleValueVector StylePropertyMapReadOnlyMainThread::getAll(
    const ExecutionContext* execution_context,
    const String& property_name,
    ExceptionState& exception_state) const {
  std::optional<CSSPropertyName> name =
      CSSPropertyName::From(execution_context, property_name);

  if (!name) {
    exception_state.ThrowTypeError("Invalid propertyName: " + property_name);
    return CSSStyleValueVector();
  }

  if (CSSProperty::IsShorthand(*name)) {
    CSSStyleValueVector values;
    if (CSSStyleValue* value = GetShorthandProperty(*name)) {
      values.push_back(value);
    }
    return values;
  }

  const CSSValue* value = (name->IsCustomProperty())
                              ? GetCustomProperty(name->ToAtomicString())
                              : GetProperty(name->Id());
  if (!value) {
    return CSSStyleValueVector();
  }

  return StyleValueFactory::CssValueToStyleValueVector(*name, *value);
}

bool StylePropertyMapReadOnlyMainThread::has(
    const ExecutionContext* execution_context,
    const String& property_name,
    ExceptionState& exception_state) const {
  return !getAll(execution_context, property_name, exception_state).empty();
}

StylePropertyMapReadOnlyMainThread::IterationSource*
StylePropertyMapReadOnlyMainThread::CreateIterationSource(
    ScriptState* script_state,
    ExceptionState&) {
  HeapVector<StylePropertyMapReadOnlyMainThread::StylePropertyMapEntry> result;

  ForEachProperty([&result](const CSSPropertyName& name,
                            const CSSValue& value) {
    auto values = StyleValueFactory::CssValueToStyleValueVector(name, value);
    result.emplace_back(name.ToAtomicString(), std::move(values));
  });

  return MakeGarbageCollected<StylePropertyMapIterationSource>(result);
}

CSSStyleValue* StylePropertyMapReadOnlyMainThread::GetShorthandProperty(
    const CSSPropertyName& name) const {
  DCHECK(CSSProperty::IsShorthand(name));
  const CSSProperty& property = CSSProperty::Get(name.Id());
  const auto serialization = SerializationForShorthand(property);
  if (serialization.empty()) {
    return nullptr;
  }
  return MakeGarbageCollected<CSSUnsupportedStyleValue>(
      CSSPropertyName(property.PropertyID()), serialization);
}

}  // namespace blink
