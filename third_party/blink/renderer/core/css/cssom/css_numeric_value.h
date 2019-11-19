// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_CSS_NUMERIC_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_CSS_NUMERIC_VALUE_H_

#include "base/macros.h"
#include "third_party/blink/renderer/bindings/core/v8/double_or_css_numeric_value.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/cssom/css_numeric_sum_value.h"
#include "third_party/blink/renderer/core/css/cssom/css_numeric_type.h"
#include "third_party/blink/renderer/core/css/cssom/css_numeric_value_type.h"
#include "third_party/blink/renderer/core/css/cssom/css_style_value.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class CSSUnitValue;
class ExceptionState;
class CSSMathExpressionNode;

class CSSNumericValue;
class CSSMathSum;
using CSSNumberish = DoubleOrCSSNumericValue;
using CSSNumericValueVector = HeapVector<Member<CSSNumericValue>>;

class CORE_EXPORT CSSNumericValue : public CSSStyleValue {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static CSSNumericValue* parse(const String& css_text, ExceptionState&);
  // Blink-internal ways of creating CSSNumericValues.
  static CSSNumericValue* FromCSSValue(const CSSPrimitiveValue&);
  // https://drafts.css-houdini.org/css-typed-om/#rectify-a-numberish-value
  static CSSNumericValue* FromNumberish(const CSSNumberish& value);

  // Methods defined in the IDL.
  CSSNumericValue* add(const HeapVector<CSSNumberish>&, ExceptionState&);
  CSSNumericValue* sub(const HeapVector<CSSNumberish>&, ExceptionState&);
  CSSNumericValue* mul(const HeapVector<CSSNumberish>&, ExceptionState&);
  CSSNumericValue* div(const HeapVector<CSSNumberish>&, ExceptionState&);
  CSSNumericValue* min(const HeapVector<CSSNumberish>&, ExceptionState&);
  CSSNumericValue* max(const HeapVector<CSSNumberish>&, ExceptionState&);
  bool equals(const HeapVector<CSSNumberish>&);

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
  virtual base::Optional<CSSNumericSumValue> SumValue() const = 0;

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
  DISALLOW_COPY_AND_ASSIGN(CSSNumericValue);
};

CSSNumericValueVector CSSNumberishesToNumericValues(
    const HeapVector<CSSNumberish>&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_CSS_NUMERIC_VALUE_H_
