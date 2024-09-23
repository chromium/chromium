/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/svg/svg_length_tear_off.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/core/svg/svg_length_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

namespace {

inline bool IsValidLengthUnit(CSSPrimitiveValue::UnitType unit) {
  return unit == CSSPrimitiveValue::UnitType::kNumber ||
         unit == CSSPrimitiveValue::UnitType::kPercentage ||
         unit == CSSPrimitiveValue::UnitType::kEms ||
         unit == CSSPrimitiveValue::UnitType::kExs ||
         unit == CSSPrimitiveValue::UnitType::kPixels ||
         unit == CSSPrimitiveValue::UnitType::kCentimeters ||
         unit == CSSPrimitiveValue::UnitType::kMillimeters ||
         unit == CSSPrimitiveValue::UnitType::kInches ||
         unit == CSSPrimitiveValue::UnitType::kPoints ||
         unit == CSSPrimitiveValue::UnitType::kPicas;
}

inline bool IsValidLengthUnit(uint16_t type) {
  return IsValidLengthUnit(static_cast<CSSPrimitiveValue::UnitType>(type));
}

inline CSSPrimitiveValue::UnitType ToCSSUnitType(uint16_t type) {
  DCHECK(IsValidLengthUnit(type));
  if (type == SVGLengthTearOff::kSvgLengthtypeNumber)
    return CSSPrimitiveValue::UnitType::kUserUnits;
  return static_cast<CSSPrimitiveValue::UnitType>(type);
}

inline uint16_t ToInterfaceConstant(CSSPrimitiveValue::UnitType type) {
  switch (type) {
    case CSSPrimitiveValue::UnitType::kUnknown:
      return SVGLengthTearOff::kSvgLengthtypeUnknown;
    case CSSPrimitiveValue::UnitType::kUserUnits:
      return SVGLengthTearOff::kSvgLengthtypeNumber;
    case CSSPrimitiveValue::UnitType::kPercentage:
      return SVGLengthTearOff::kSvgLengthtypePercentage;
    case CSSPrimitiveValue::UnitType::kEms:
      return SVGLengthTearOff::kSvgLengthtypeEms;
    case CSSPrimitiveValue::UnitType::kExs:
      return SVGLengthTearOff::kSvgLengthtypeExs;
    case CSSPrimitiveValue::UnitType::kPixels:
      return SVGLengthTearOff::kSvgLengthtypePx;
    case CSSPrimitiveValue::UnitType::kCentimeters:
      return SVGLengthTearOff::kSvgLengthtypeCm;
    case CSSPrimitiveValue::UnitType::kMillimeters:
      return SVGLengthTearOff::kSvgLengthtypeMm;
    case CSSPrimitiveValue::UnitType::kInches:
      return SVGLengthTearOff::kSvgLengthtypeIn;
    case CSSPrimitiveValue::UnitType::kPoints:
      return SVGLengthTearOff::kSvgLengthtypePt;
    case CSSPrimitiveValue::UnitType::kPicas:
      return SVGLengthTearOff::kSvgLengthtypePc;
    default:
      return SVGLengthTearOff::kSvgLengthtypeUnknown;
  }
}

bool HasExposedLengthUnit(const SVGLength& length) {
  if (length.IsCalculated())
    return false;

  CSSPrimitiveValue::UnitType unit = length.NumericLiteralType();
  return IsValidLengthUnit(unit) ||
         unit == CSSPrimitiveValue::UnitType::kUnknown ||
         unit == CSSPrimitiveValue::UnitType::kUserUnits;
}

bool EnsureResolvable(SVGElement* context_element, bool needs_layout) {
  if (!context_element || !context_element->isConnected()) {
    return false;
  }
  Document& document = context_element->GetDocument();
  if (needs_layout) {
    document.UpdateStyleAndLayoutForNode(context_element,
                                         DocumentUpdateReason::kJavaScript);
  } else {
    document.UpdateStyleAndLayoutTreeForElement(
        context_element, DocumentUpdateReason::kJavaScript);
  }
  return true;
}

bool EnsureResolvable(const SVGLength& length, SVGElement* context_element) {
  if (!length.IsRelative()) {
    return true;
  }
  const bool needs_layout = length.IsPercentage() || length.IsCalculated();
  return EnsureResolvable(context_element, needs_layout);
}

bool EnsureResolvable(const SVGLength& length,
                      CSSPrimitiveValue::UnitType other_unit_type,
                      SVGElement* context_element) {
  if (!length.IsRelative() &&
      !CSSPrimitiveValue::IsRelativeUnit(other_unit_type)) {
    return true;
  }
  const bool needs_layout =
      length.IsPercentage() || length.IsCalculated() ||
      other_unit_type == CSSPrimitiveValue::UnitType::kPercentage;
  return EnsureResolvable(context_element, needs_layout);
}

void ThrowUnresolvableRelativeLength(ExceptionState& exception_state) {
  exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                    "Could not resolve relative length.");
}

}  // namespace

uint16_t SVGLengthTearOff::unitType() {
  return HasExposedLengthUnit(*Target())
             ? ToInterfaceConstant(Target()->NumericLiteralType())
             : kSvgLengthtypeUnknown;
}

SVGLengthMode SVGLengthTearOff::UnitMode() {
  return Target()->UnitMode();
}

float SVGLengthTearOff::value(ExceptionState& exception_state) {
  SVGElement* context_element = ContextElement();
  if (!EnsureResolvable(*Target(), context_element)) {
    ThrowUnresolvableRelativeLength(exception_state);
    return 0;
  }
  SVGLengthContext length_context(context_element);
  return Target()->Value(length_context);
}

void SVGLengthTearOff::setValue(float value, ExceptionState& exception_state) {
  if (IsImmutable()) {
    ThrowReadOnly(exception_state);
    return;
  }
  if (Target()->IsCalculated() || Target()->HasContainerRelativeUnits()) {
    Target()->SetValueAsNumber(value);
  } else {
    SVGElement* context_element = ContextElement();
    if (!EnsureResolvable(*Target(), context_element)) {
      ThrowUnresolvableRelativeLength(exception_state);
      return;
    }
    SVGLengthContext length_context(context_element);
    Target()->SetValueInSpecifiedUnits(length_context.ConvertValueFromUserUnits(
        value, Target()->UnitMode(), Target()->NumericLiteralType()));
  }
  CommitChange(SVGPropertyCommitReason::kUpdated);
}

float SVGLengthTearOff::valueInSpecifiedUnits() {
  if (Target()->IsCalculated())
    return 0;
  return Target()->ValueInSpecifiedUnits();
}

void SVGLengthTearOff::setValueInSpecifiedUnits(
    float value,
    ExceptionState& exception_state) {
  if (IsImmutable()) {
    ThrowReadOnly(exception_state);
    return;
  }
  if (Target()->IsCalculated())
    Target()->SetValueAsNumber(value);
  else
    Target()->SetValueInSpecifiedUnits(value);
  CommitChange(SVGPropertyCommitReason::kUpdated);
}

String SVGLengthTearOff::valueAsString() {
  return Target()->ValueAsString();
}

void SVGLengthTearOff::setValueAsString(const String& str,
                                        ExceptionState& exception_state) {
  if (IsImmutable()) {
    ThrowReadOnly(exception_state);
    return;
  }
  SVGParsingError status = Target()->SetValueAsString(str);
  if (status != SVGParseStatus::kNoError) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "The value provided ('" + str + "') is invalid.");
    return;
  }
  CommitChange(SVGPropertyCommitReason::kUpdated);
}

void SVGLengthTearOff::newValueSpecifiedUnits(uint16_t unit_type,
                                              float value_in_specified_units,
                                              ExceptionState& exception_state) {
  if (IsImmutable()) {
    ThrowReadOnly(exception_state);
    return;
  }
  if (!IsValidLengthUnit(unit_type)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "Cannot set value with unknown or invalid units (" +
            String::Number(unit_type) + ").");
    return;
  }
  Target()->NewValueSpecifiedUnits(ToCSSUnitType(unit_type),
                                   value_in_specified_units);
  CommitChange(SVGPropertyCommitReason::kUpdated);
}

void SVGLengthTearOff::convertToSpecifiedUnits(
    uint16_t unit_type,
    ExceptionState& exception_state) {
  if (IsImmutable()) {
    ThrowReadOnly(exception_state);
    return;
  }
  if (!IsValidLengthUnit(unit_type)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "Cannot convert to unknown or invalid units (" +
            String::Number(unit_type) + ").");
    return;
  }
  SVGElement* context_element = ContextElement();
  if (!EnsureResolvable(*Target(), ToCSSUnitType(unit_type), context_element)) {
    ThrowUnresolvableRelativeLength(exception_state);
    return;
  }
  SVGLengthContext length_context(context_element);
  Target()->ConvertToSpecifiedUnits(ToCSSUnitType(unit_type), length_context);
  CommitChange(SVGPropertyCommitReason::kUpdated);
}

SVGLengthTearOff::SVGLengthTearOff(SVGLength* target,
                                   SVGAnimatedPropertyBase* binding,
                                   PropertyIsAnimValType property_is_anim_val)
    : SVGPropertyTearOff<SVGLength>(target, binding, property_is_anim_val) {}

SVGLengthTearOff* SVGLengthTearOff::CreateDetached() {
  return MakeGarbageCollected<SVGLengthTearOff>(
      MakeGarbageCollected<SVGLength>(), nullptr, kPropertyIsNotAnimVal);
}

}  // namespace blink
