// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_STYLE_VALUE_FACTORY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_STYLE_VALUE_FACTORY_H_

#include "third_party/blink/renderer/bindings/core/v8/css_style_value_or_string.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/cssom/css_style_value.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class CSSParserContext;
class CSSProperty;
class CSSPropertyName;
class CSSValue;
class ExecutionContext;

class CORE_EXPORT StyleValueFactory {
  STATIC_ONLY(StyleValueFactory);

 public:
  static CSSStyleValueVector FromString(
      CSSPropertyID,
      const AtomicString& custom_property_name,
      const String&,
      const CSSParserContext*);
  static CSSStyleValue* CssValueToStyleValue(const CSSPropertyName&,
                                             const CSSValue&);
  static CSSStyleValueVector CssValueToStyleValueVector(const CSSPropertyName&,
                                                        const CSSValue&);
  // Returns an empty vector on error conditions.
  static CSSStyleValueVector CoerceStyleValuesOrStrings(
      const CSSProperty& property,
      const AtomicString& custom_property_name,
      const HeapVector<CSSStyleValueOrString>& values,
      const ExecutionContext&);
  // Reify a CSSStyleValue without the context of a CSS property. For most
  // CSSValues, this will result in a CSSUnsupportedStyleValue. Note that the
  // CSSUnsupportedStyleValue returned from this function (unlike regular
  // CSSUnsupportedStyleValues) do not have an associated CSS property,
  // which means that any attempt to StylePropertyMap.set/setAll such values
  // will always fail. Therefore, this function should only be used in
  // situations where declared and inline style objects [1] are not accessible,
  // such as paint worklets.
  //
  // [1] https://www.w3.org/TR/css-typed-om-1/#declared-stylepropertymap-objects
  static CSSStyleValueVector CssValueToStyleValueVector(const CSSValue&);
};

}  // namespace blink

#endif
