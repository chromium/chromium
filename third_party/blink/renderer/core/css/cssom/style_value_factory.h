// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_STYLE_VALUE_FACTORY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_STYLE_VALUE_FACTORY_H_

#include "third_party/blink/renderer/bindings/core/v8/css_style_value_or_string.h"
#include "third_party/blink/renderer/core/css/cssom/css_style_value.h"
#include "third_party/blink/renderer/core/css_property_names.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"

namespace blink {

class CSSParserContext;
class CSSProperty;
class CSSValue;
class ExecutionContext;
class PropertyRegistration;

class CORE_EXPORT StyleValueFactory {
  STATIC_ONLY(StyleValueFactory);

 public:
  static CSSStyleValueVector FromString(
      CSSPropertyID,
      const AtomicString& custom_property_name,
      const PropertyRegistration*,
      const String&,
      const CSSParserContext*);
  static CSSStyleValue* CssValueToStyleValue(
      CSSPropertyID,
      const AtomicString& custom_property_name,
      const CSSValue&);
  static CSSStyleValueVector CssValueToStyleValueVector(
      CSSPropertyID,
      const AtomicString& custom_property_name,
      const CSSValue&);
  // Returns an empty vector on error conditions.
  static CSSStyleValueVector CoerceStyleValuesOrStrings(
      const CSSProperty& property,
      const AtomicString& custom_property_name,
      const PropertyRegistration*,
      const HeapVector<CSSStyleValueOrString>& values,
      const ExecutionContext&);
  // If you don't have complex CSS properties, use this one.
  static CSSStyleValueVector CssValueToStyleValueVector(const CSSValue&);
};

}  // namespace blink

#endif
