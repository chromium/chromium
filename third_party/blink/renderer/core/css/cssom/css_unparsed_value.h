// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_CSS_UNPARSED_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_CSS_UNPARSED_VALUE_H_

#include "third_party/blink/renderer/bindings/core/v8/string_or_css_variable_reference_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_typedefs.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_cssvariablereferencevalue_string.h"
#include "third_party/blink/renderer/core/css/cssom/css_style_value.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class CSSVariableReferenceValue;
class CSSCustomPropertyDeclaration;
class CSSVariableData;
using CSSUnparsedSegment = StringOrCSSVariableReferenceValue;

class CORE_EXPORT CSSUnparsedValue final : public CSSStyleValue {
  DEFINE_WRAPPERTYPEINFO();

 public:
#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  static CSSUnparsedValue* Create(
      const HeapVector<Member<V8CSSUnparsedSegment>>& tokens) {
    return MakeGarbageCollected<CSSUnparsedValue>(tokens);
  }
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  static CSSUnparsedValue* Create(
      const HeapVector<CSSUnparsedSegment>& tokens) {
    return MakeGarbageCollected<CSSUnparsedValue>(tokens);
  }
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)

  // Blink-internal constructor
  static CSSUnparsedValue* Create() {
#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
    return Create(HeapVector<Member<V8CSSUnparsedSegment>>());
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
    return Create(HeapVector<CSSUnparsedSegment>());
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  }
  static CSSUnparsedValue* FromCSSValue(const CSSVariableReferenceValue&);
  static CSSUnparsedValue* FromCSSValue(const CSSCustomPropertyDeclaration&);
  static CSSUnparsedValue* FromCSSVariableData(const CSSVariableData&);
  static CSSUnparsedValue* FromString(const String& string) {
#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
    HeapVector<Member<V8CSSUnparsedSegment>> tokens;
    tokens.push_back(MakeGarbageCollected<V8CSSUnparsedSegment>(string));
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
    HeapVector<CSSUnparsedSegment> tokens;
    tokens.push_back(CSSUnparsedSegment::FromString(string));
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
    return Create(tokens);
  }

#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  explicit CSSUnparsedValue(
      const HeapVector<Member<V8CSSUnparsedSegment>>& tokens)
      : tokens_(tokens) {}
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  explicit CSSUnparsedValue(const HeapVector<CSSUnparsedSegment>& tokens)
      : tokens_(tokens) {}
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  CSSUnparsedValue(const CSSUnparsedValue&) = delete;
  CSSUnparsedValue& operator=(const CSSUnparsedValue&) = delete;

  const CSSValue* ToCSSValue() const override;

  StyleValueType GetType() const override { return kUnparsedType; }

#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  V8CSSUnparsedSegment* AnonymousIndexedGetter(
      uint32_t index,
      ExceptionState& exception_state) const;
  IndexedPropertySetterResult AnonymousIndexedSetter(
      uint32_t index,
      V8CSSUnparsedSegment* segment,
      ExceptionState& exception_state);
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  void AnonymousIndexedGetter(uint32_t index,
                              CSSUnparsedSegment& return_value,
                              ExceptionState& exception_state) const;
  IndexedPropertySetterResult AnonymousIndexedSetter(unsigned,
                                                     const CSSUnparsedSegment&,
                                                     ExceptionState&);
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)

  wtf_size_t length() const { return tokens_.size(); }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(tokens_);
    CSSStyleValue::Trace(visitor);
  }

  String ToString() const;

 private:
#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  HeapVector<Member<V8CSSUnparsedSegment>> tokens_;
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  HeapVector<CSSUnparsedSegment> tokens_;
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)

  FRIEND_TEST_ALL_PREFIXES(CSSVariableReferenceValueTest, MixedList);
};

template <>
struct DowncastTraits<CSSUnparsedValue> {
  static bool AllowFrom(const CSSStyleValue& value) {
    return value.GetType() == CSSStyleValue::StyleValueType::kUnparsedType;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_CSS_UNPARSED_VALUE_H_
