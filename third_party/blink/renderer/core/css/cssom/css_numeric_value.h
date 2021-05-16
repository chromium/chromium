// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_CSS_NUMERIC_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_CSS_NUMERIC_VALUE_H_

#include "third_party/blink/renderer/bindings/core/v8/double_or_css_numeric_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_typedefs.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/cssom/css_numeric_sum_value.h"
#include "third_party/blink/renderer/core/css/cssom/css_numeric_value_type.h"
#include "third_party/blink/renderer/core/css/cssom/css_style_value.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class CSSMathExpressionNode;
class CSSMathSum;
class CSSNumericType;
class CSSNumericValue;
class CSSUnitValue;
class ExceptionState;

using CSSNumberish = DoubleOrCSSNumericValue;
using CSSNumericValueVector = HeapVector<Member<CSSNumericValue>>;

class CORE_EXPORT CSSNumericValue : public CSSStyleValue {
  DEFINE_WRAPPERTYPEINFO();

 public:
  CSSNumericValue(const CSSNumericValue&) = delete;
  CSSNumericValue& operator=(const CSSNumericValue&) = delete;

  static CSSNumericValue* parse(const String& css_text, ExceptionState&);
  // Blink-internal ways of creating CSSNumericValues.
  static CSSNumericValue* FromCSSValue(const CSSPrimitiveValue&);
#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  // https://drafts.css-houdini.org/css-typed-om/#rectify-a-numberish-value
  static CSSNumericValue* FromNumberish(const V8CSSNumberish* value);
  // https://drafts.css-houdini.org/css-typed-om/#rectify-a-percentish-value
  static CSSNumericValue* FromPercentish(const V8CSSNumberish* value);
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  static CSSNumericValue* FromNumberish(const CSSNumberish& value);
  static CSSNumericValue* FromPercentish(const CSSNumberish& value);
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)

  // Methods defined in the IDL.
#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  CSSNumericValue* add(const HeapVector<Member<V8CSSNumberish>>& numberishes,
                       ExceptionState& exception_state);
  CSSNumericValue* sub(const HeapVector<Member<V8CSSNumberish>>& numberishes,
                       ExceptionState& exception_state);
  CSSNumericValue* mul(const HeapVector<Member<V8CSSNumberish>>& numberishes,
                       ExceptionState& exception_state);
  CSSNumericValue* div(const HeapVector<Member<V8CSSNumberish>>& numberishes,
                       ExceptionState& exception_state);
  CSSNumericValue* min(const HeapVector<Member<V8CSSNumberish>>& numberishes,
                       ExceptionState& exception_state);
  CSSNumericValue* max(const HeapVector<Member<V8CSSNumberish>>& numberishes,
                       ExceptionState& exception_state);
  bool equals(const HeapVector<Member<V8CSSNumberish>>& numberishes);
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  CSSNumericValue* add(const HeapVector<CSSNumberish>&, ExceptionState&);
  CSSNumericValue* sub(const HeapVector<CSSNumberish>&, ExceptionState&);
  CSSNumericValue* mul(const HeapVector<CSSNumberish>&, ExceptionState&);
  CSSNumericValue* div(const HeapVector<CSSNumberish>&, ExceptionState&);
  CSSNumericValue* min(const HeapVector<CSSNumberish>&, ExceptionState&);
  CSSNumericValue* max(const HeapVector<CSSNumberish>&, ExceptionState&);
  bool equals(const HeapVector<CSSNumberish>&);
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)

  // Converts between compatible types, as defined in the IDL.
  CSSUnitValue* to(const String&, ExceptionState&);
  CSSMathSum* toSum(const Vector<String>&, ExceptionState&);

  CSSNumericType* type() const;

  String toString() const final;

  // Internal methods.
  // Arithmetic
  virtual CSSNumericValue* Negate();
  virtual CSSNumericValue* Invert();

  // Converts between compatible types.
  CSSUnitValue* to(CSSPrimitiveValue::UnitType) const;
  virtual bool IsUnitValue() const = 0;
  virtual absl::optional<CSSNumericSumValue> SumValue() const = 0;

  virtual bool Equals(const CSSNumericValue&) const = 0;
  const CSSNumericValueType& Type() const { return type_; }

  virtual CSSMathExpressionNode* ToCalcExpressionNode() const = 0;

  enum class Nested : bool { kYes, kNo };
  enum class ParenLess : bool { kYes, kNo };
  virtual void BuildCSSText(Nested, ParenLess, StringBuilder&) const = 0;

 protected:
  static bool IsValidUnit(CSSPrimitiveValue::UnitType);
  static CSSPrimitiveValue::UnitType UnitFromName(const String& name);

  CSSNumericValue(const CSSNumericValueType& type) : type_(type) {}

 private:
  CSSNumericValueType type_;
};

#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
CSSNumericValueVector CSSNumberishesToNumericValues(
    const HeapVector<Member<V8CSSNumberish>>& values);
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
CSSNumericValueVector CSSNumberishesToNumericValues(
    const HeapVector<CSSNumberish>&);
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_CSS_NUMERIC_VALUE_H_
