// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_CSS_UNPARSED_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_CSS_UNPARSED_VALUE_H_

#include "base/macros.h"
#include "third_party/blink/renderer/bindings/core/v8/string_or_css_variable_reference_value.h"
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
  static CSSUnparsedValue* Create(
      const HeapVector<CSSUnparsedSegment>& tokens) {
    return MakeGarbageCollected<CSSUnparsedValue>(tokens);
  }

  // Blink-internal constructor
  static CSSUnparsedValue* Create() {
    return Create(HeapVector<CSSUnparsedSegment>());
  }
  static CSSUnparsedValue* FromCSSValue(const CSSVariableReferenceValue&);
  static CSSUnparsedValue* FromCSSValue(const CSSCustomPropertyDeclaration&);
  static CSSUnparsedValue* FromCSSVariableData(const CSSVariableData&);
  static CSSUnparsedValue* FromString(const String& string) {
    HeapVector<CSSUnparsedSegment> tokens;
    tokens.push_back(CSSUnparsedSegment::FromString(string));
    return Create(tokens);
  }

  CSSUnparsedValue(const HeapVector<CSSUnparsedSegment>& tokens)
      : CSSStyleValue(), tokens_(tokens) {}

  const CSSValue* ToCSSValue() const override;

  StyleValueType GetType() const override { return kUnparsedType; }

  CSSUnparsedSegment AnonymousIndexedGetter(unsigned, ExceptionState&) const;
  bool AnonymousIndexedSetter(unsigned,
                              const CSSUnparsedSegment&,
                              ExceptionState&);

  wtf_size_t length() const { return tokens_.size(); }

  void Trace(Visitor* visitor) override {
    visitor->Trace(tokens_);
    CSSStyleValue::Trace(visitor);
  }

  String ToString() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(CSSVariableReferenceValueTest, MixedList);

  HeapVector<CSSUnparsedSegment> tokens_;
  DISALLOW_COPY_AND_ASSIGN(CSSUnparsedValue);
};

template <>
struct DowncastTraits<CSSUnparsedValue> {
  static bool AllowFrom(const CSSStyleValue& value) {
    return value.GetType() == CSSStyleValue::StyleValueType::kUnparsedType;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_CSS_UNPARSED_VALUE_H_
