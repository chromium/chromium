// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/css_style_value.h"

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"
#include "third_party/blink/renderer/core/css/cssom/style_value_factory.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

class PropertyRegistration;

namespace {

CSSStyleValueVector ParseCSSStyleValue(
    const ExecutionContext* execution_context,
    const String& property_name,
    const String& value,
    ExceptionState& exception_state) {
  const CSSPropertyID property_id = cssPropertyID(property_name);

  if (property_id == CSSPropertyID::kInvalid) {
    exception_state.ThrowTypeError("Invalid property name");
    return CSSStyleValueVector();
  }

  AtomicString custom_property_name = property_id == CSSPropertyID::kVariable
                                          ? AtomicString(property_name)
                                          : g_null_atom;
  const auto style_values = StyleValueFactory::FromString(
      property_id, custom_property_name, value,
      MakeGarbageCollected<CSSParserContext>(*execution_context));
  if (style_values.IsEmpty()) {
    exception_state.ThrowTypeError("The value provided ('" + value +
                                   "') could not be parsed as a '" +
                                   property_name + "'.");
    return CSSStyleValueVector();
  }

  return style_values;
}

}  // namespace

CSSStyleValue* CSSStyleValue::parse(const ExecutionContext* execution_context,
                                    const String& property_name,
                                    const String& value,
                                    ExceptionState& exception_state) {
  CSSStyleValueVector style_value_vector = ParseCSSStyleValue(
      execution_context, property_name, value, exception_state);
  return style_value_vector.IsEmpty() ? nullptr : style_value_vector[0];
}

CSSStyleValueVector CSSStyleValue::parseAll(
    const ExecutionContext* execution_context,
    const String& property_name,
    const String& value,
    ExceptionState& exception_state) {
  return ParseCSSStyleValue(execution_context, property_name, value,
                            exception_state);
}

String CSSStyleValue::toString() const {
  const CSSValue* result = ToCSSValue();
  DCHECK(result);
  return result->CssText();
}

}  // namespace blink
