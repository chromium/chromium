/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_PRIMITIVE_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_PRIMITIVE_VALUE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css_property_names.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/platform/wtf/bit_vector.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"

namespace blink {

class CSSCalcValue;
class CSSToLengthConversionData;
class Length;

// Dimension calculations are imprecise, often resulting in values of e.g.
// 44.99998. We need to go ahead and round if we're really close to the next
// integer value.
template <typename T>
inline T RoundForImpreciseConversion(double value) {
  value += (value < 0) ? -0.01 : +0.01;
  return ((value > std::numeric_limits<T>::max()) ||
          (value < std::numeric_limits<T>::min()))
             ? 0
             : static_cast<T>(value);
}

template <>
inline float RoundForImpreciseConversion(double value) {
  double ceiled_value = ceil(value);
  double proximity_to_next_int = ceiled_value - value;
  if (proximity_to_next_int <= 0.01 && value > 0)
    return static_cast<float>(ceiled_value);
  if (proximity_to_next_int >= 0.99 && value < 0)
    return static_cast<float>(floor(value));
  return static_cast<float>(value);
}

// CSSPrimitiveValue stores numeric data types (e.g. 1, 10px, 4%) and calc()
// values (e.g. calc(3px + 2em)).
class CORE_EXPORT CSSPrimitiveValue : public CSSValue {
 public:
  // These units are iterated through, so be careful when adding or changing the
  // order.
  enum class UnitType {
    kUnknown,
    kNumber,
    kPercentage,
    // Length units
    kEms,
    kExs,
    kPixels,
    kCentimeters,
    kMillimeters,
    kInches,
    kPoints,
    kPicas,
    kQuarterMillimeters,
    kViewportWidth,
    kViewportHeight,
    kViewportMin,
    kViewportMax,
    kRems,
    kChs,
    kUserUnits,  // The SVG term for unitless lengths
    // Angle units
    kDegrees,
    kRadians,
    kGradians,
    kTurns,
    // Time units
    kMilliseconds,
    kSeconds,
    kHertz,
    kKilohertz,
    // Resolution
    kDotsPerPixel,
    kDotsPerInch,
    kDotsPerCentimeter,
    // Other units
    kFraction,
    kInteger,
    kCalc,
    kCalcPercentageWithNumber,
    kCalcPercentageWithLength,
    kCalcLengthWithNumber,
    kCalcPercentageWithLengthAndNumber,

    // This value is used to handle quirky margins in reflow roots (body, td,
    // and th) like WinIE. The basic idea is that a stylesheet can use the value
    // __qem (for quirky em) instead of em. When the quirky value is used, if
    // you're in quirks mode, the margin will collapse away inside a table cell.
    // This quirk is specified in the HTML spec but our impl is different.
    // TODO: Remove this. crbug.com/443952
    kQuirkyEms,
  };

  enum LengthUnitType {
    kUnitTypePixels = 0,
    kUnitTypePercentage,
    kUnitTypeFontSize,
    kUnitTypeFontXSize,
    kUnitTypeRootFontSize,
    kUnitTypeZeroCharacterWidth,
    kUnitTypeViewportWidth,
    kUnitTypeViewportHeight,
    kUnitTypeViewportMin,
    kUnitTypeViewportMax,

    // This value must come after the last length unit type to enable iteration
    // over the length unit types.
    kLengthUnitTypeCount,
  };

  struct CSSLengthArray {
    CSSLengthArray() : values(kLengthUnitTypeCount) {
      type_flags.Resize(kLengthUnitTypeCount);
    }

    Vector<double, CSSPrimitiveValue::kLengthUnitTypeCount> values;
    BitVector type_flags;
  };

  void AccumulateLengthArray(CSSLengthArray&, double multiplier = 1) const;

  enum UnitCategory {
    kUNumber,
    kUPercent,
    kULength,
    kUAngle,
    kUTime,
    kUFrequency,
    kUResolution,
    kUOther
  };
  static UnitCategory UnitTypeToUnitCategory(UnitType);
  static float ClampToCSSLengthRange(double);

  static bool IsAngle(UnitType unit) {
    return unit == UnitType::kDegrees || unit == UnitType::kRadians ||
           unit == UnitType::kGradians || unit == UnitType::kTurns;
  }
  bool IsAngle() const { return IsAngle(TypeWithCalcResolved()); }
  bool IsFontRelativeLength() const {
    return GetType() == UnitType::kQuirkyEms || GetType() == UnitType::kEms ||
           GetType() == UnitType::kExs || GetType() == UnitType::kRems ||
           GetType() == UnitType::kChs;
  }
  bool IsQuirkyEms() const { return GetType() == UnitType::kQuirkyEms; }
  bool IsViewportPercentageLength() const {
    return IsViewportPercentageLength(GetType());
  }
  static bool IsViewportPercentageLength(UnitType type) {
    return type >= UnitType::kViewportWidth && type <= UnitType::kViewportMax;
  }
  static bool IsLength(UnitType type) {
    return (type >= UnitType::kEms && type <= UnitType::kUserUnits) ||
           type == UnitType::kQuirkyEms;
  }
  static inline bool IsRelativeUnit(UnitType type) {
    return type == UnitType::kPercentage || type == UnitType::kEms ||
           type == UnitType::kExs || type == UnitType::kRems ||
           type == UnitType::kChs || IsViewportPercentageLength(type);
  }
  bool IsLength() const { return IsLength(TypeWithCalcResolved()); }
  bool IsNumber() const {
    return TypeWithCalcResolved() == UnitType::kNumber ||
           TypeWithCalcResolved() == UnitType::kInteger;
  }
  bool IsPercentage() const {
    return TypeWithCalcResolved() == UnitType::kPercentage;
  }
  bool IsPx() const { return TypeWithCalcResolved() == UnitType::kPixels; }
  static bool IsTime(UnitType unit) {
    return unit == UnitType::kSeconds || unit == UnitType::kMilliseconds;
  }
  bool IsTime() const { return IsTime(GetType()); }
  static bool IsFrequency(UnitType unit) {
    return unit == UnitType::kHertz || unit == UnitType::kKilohertz;
  }
  bool IsCalculated() const { return GetType() == UnitType::kCalc; }
  bool IsCalculatedPercentageWithNumber() const {
    return TypeWithCalcResolved() == UnitType::kCalcPercentageWithNumber;
  }
  bool IsCalculatedPercentageWithLength() const {
    return TypeWithCalcResolved() == UnitType::kCalcPercentageWithLength;
  }
  static bool IsResolution(UnitType type) {
    return type >= UnitType::kDotsPerPixel &&
           type <= UnitType::kDotsPerCentimeter;
  }
  static bool IsFlex(UnitType unit) { return unit == UnitType::kFraction; }
  bool IsFlex() const { return IsFlex(GetType()); }

  static CSSPrimitiveValue* Create(double value, UnitType);
  static CSSPrimitiveValue* Create(const Length& value, float zoom) {
    return new CSSPrimitiveValue(value, zoom);
  }

  // TODO(sashab): Remove this.
  template <typename T>
  static CSSPrimitiveValue* Create(T value) {
    return new CSSPrimitiveValue(value);
  }

  ~CSSPrimitiveValue();

  UnitType TypeWithCalcResolved() const;

  double ComputeDegrees() const;
  double ComputeSeconds() const;
  double ComputeDotsPerPixel() const;

  // Computes a length in pixels, resolving relative lengths
  template <typename T>
  T ComputeLength(const CSSToLengthConversionData&) const;

  // Converts to a Length (Fixed, Percent or Calculated)
  Length ConvertToLength(const CSSToLengthConversionData&) const;

  double GetDoubleValue() const;
  float GetFloatValue() const { return GetValue<float>(); }
  int GetIntValue() const { return GetValue<int>(); }
  template <typename T>
  inline T GetValue() const {
    return clampTo<T>(GetDoubleValue());
  }

  CSSCalcValue* CssCalcValue() const {
    DCHECK(IsCalculated());
    return value_.calc;
  }

  template <typename T>
  inline T ConvertTo() const;  // Defined in CSSPrimitiveValueMappings.h

  static const char* UnitTypeToString(UnitType);
  static UnitType StringToUnitType(StringView string) {
    if (string.Is8Bit())
      return StringToUnitType(string.Characters8(), string.length());
    return StringToUnitType(string.Characters16(), string.length());
  }

  String CustomCSSText() const;

  bool Equals(const CSSPrimitiveValue&) const;

  void TraceAfterDispatch(blink::Visitor*);

  static UnitType CanonicalUnitTypeForCategory(UnitCategory);
  static double ConversionToCanonicalUnitsScaleFactor(UnitType);

  // Returns true and populates lengthUnitType, if unitType is a length unit.
  // Otherwise, returns false.
  static bool UnitTypeToLengthUnitType(UnitType, LengthUnitType&);
  static UnitType LengthUnitTypeToUnitType(LengthUnitType);

 private:
  CSSPrimitiveValue(const Length&, float zoom);
  CSSPrimitiveValue(double, UnitType);

  template <typename T>
  CSSPrimitiveValue(T);  // Defined in CSSPrimitiveValueMappings.h

  template <typename T>
  CSSPrimitiveValue(T* val) : CSSValue(kPrimitiveClass) {
    Init(val);
  }

  static void Create(int);       // compile-time guard
  static void Create(unsigned);  // compile-time guard
  template <typename T>
  operator T*();  // compile-time guard

  // Code generated by css_primitive_value_unit_trie.cc.tmpl
  static UnitType StringToUnitType(const LChar*, unsigned length);
  static UnitType StringToUnitType(const UChar*, unsigned length);

  void Init(UnitType);
  void Init(const Length&);
  void Init(CSSCalcValue*);

  double ComputeLengthDouble(const CSSToLengthConversionData&) const;

  inline UnitType GetType() const {
    return static_cast<UnitType>(primitive_unit_type_);
  }

  union {
    double num;
    // FIXME: oilpan: Should be a member, but no support for members in unions.
    // Just trace the raw ptr for now.
    CSSCalcValue* calc;
  } value_;
};

using CSSLengthArray = CSSPrimitiveValue::CSSLengthArray;

DEFINE_CSS_VALUE_TYPE_CASTS(CSSPrimitiveValue, IsPrimitiveValue());

template <>
int CSSPrimitiveValue::ComputeLength(const CSSToLengthConversionData&) const;

template <>
Length CSSPrimitiveValue::ComputeLength(const CSSToLengthConversionData&) const;

template <>
unsigned CSSPrimitiveValue::ComputeLength(
    const CSSToLengthConversionData&) const;

template <>
short CSSPrimitiveValue::ComputeLength(const CSSToLengthConversionData&) const;

template <>
float CSSPrimitiveValue::ComputeLength(const CSSToLengthConversionData&) const;

template <>
double CSSPrimitiveValue::ComputeLength(const CSSToLengthConversionData&) const;
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_PRIMITIVE_VALUE_H_
