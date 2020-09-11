/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_VALUE_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/style/data_equivalency.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class Document;
class Length;

class CORE_EXPORT CSSValue : public GarbageCollected<CSSValue> {
 public:
  template <typename T>
  static void* AllocateObject(size_t size) {
    ThreadState* state =
        ThreadStateFor<ThreadingTrait<CSSValue>::kAffinity>::GetState();
    const char* type_name = "blink::CSSValue";
    return state->Heap().AllocateOnArenaIndex(
        state, size, BlinkGC::kCSSValueArenaIndex,
        GCInfoTrait<GCInfoFoldedType<CSSValue>>::Index(), type_name);
  }

  // TODO(sashab): Remove this method and move logic to the caller.
  static CSSValue* Create(const Length& value, float zoom);

  String CssText() const;

  bool IsNumericLiteralValue() const {
    return class_type_ == kNumericLiteralClass;
  }
  bool IsMathFunctionValue() const { return class_type_ == kMathFunctionClass; }
  bool IsPrimitiveValue() const {
    return IsNumericLiteralValue() || IsMathFunctionValue();
  }
  bool IsIdentifierValue() const { return class_type_ == kIdentifierClass; }
  bool IsValuePair() const { return class_type_ == kValuePairClass; }
  bool IsValueList() const { return class_type_ >= kValueListClass; }

  bool IsBaseValueList() const { return class_type_ == kValueListClass; }

  bool IsBasicShapeValue() const {
    return class_type_ >= kBasicShapeCircleClass &&
           class_type_ <= kBasicShapeInsetClass;
  }
  bool IsBasicShapeCircleValue() const {
    return class_type_ == kBasicShapeCircleClass;
  }
  bool IsBasicShapeEllipseValue() const {
    return class_type_ == kBasicShapeEllipseClass;
  }
  bool IsBasicShapePolygonValue() const {
    return class_type_ == kBasicShapePolygonClass;
  }
  bool IsBasicShapeInsetValue() const {
    return class_type_ == kBasicShapeInsetClass;
  }

  bool IsBorderImageSliceValue() const {
    return class_type_ == kBorderImageSliceClass;
  }
  bool IsColorValue() const { return class_type_ == kColorClass; }
  bool IsCounterValue() const { return class_type_ == kCounterClass; }
  bool IsCursorImageValue() const { return class_type_ == kCursorImageClass; }
  bool IsCrossfadeValue() const { return class_type_ == kCrossfadeClass; }
  bool IsPaintValue() const { return class_type_ == kPaintClass; }
  bool IsFontFeatureValue() const { return class_type_ == kFontFeatureClass; }
  bool IsFontFamilyValue() const { return class_type_ == kFontFamilyClass; }
  bool IsFontFaceSrcValue() const { return class_type_ == kFontFaceSrcClass; }
  bool IsFontStyleRangeValue() const {
    return class_type_ == kFontStyleRangeClass;
  }
  bool IsFontVariationValue() const {
    return class_type_ == kFontVariationClass;
  }
  bool IsFunctionValue() const { return class_type_ == kFunctionClass; }
  bool IsCustomIdentValue() const { return class_type_ == kCustomIdentClass; }
  bool IsImageGeneratorValue() const {
    return class_type_ >= kCrossfadeClass && class_type_ <= kConicGradientClass;
  }
  bool IsGradientValue() const {
    return class_type_ >= kLinearGradientClass &&
           class_type_ <= kConicGradientClass;
  }
  bool IsImageSetValue() const { return class_type_ == kImageSetClass; }
  bool IsImageValue() const { return class_type_ == kImageClass; }
  bool IsInheritedValue() const { return class_type_ == kInheritedClass; }
  bool IsInitialValue() const { return class_type_ == kInitialClass; }
  bool IsUnsetValue() const { return class_type_ == kUnsetClass; }
  bool IsRevertValue() const { return class_type_ == kRevertClass; }
  bool IsCSSWideKeyword() const {
    return class_type_ >= kInheritedClass && class_type_ <= kRevertClass;
  }
  bool IsLayoutFunctionValue() const {
    return class_type_ == kLayoutFunctionClass;
  }
  bool IsLinearGradientValue() const {
    return class_type_ == kLinearGradientClass;
  }
  bool IsPathValue() const { return class_type_ == kPathClass; }
  bool IsQuadValue() const { return class_type_ == kQuadClass; }
  bool IsRayValue() const { return class_type_ == kRayClass; }
  bool IsRadialGradientValue() const {
    return class_type_ == kRadialGradientClass;
  }
  bool IsConicGradientValue() const {
    return class_type_ == kConicGradientClass;
  }
  bool IsReflectValue() const { return class_type_ == kReflectClass; }
  bool IsShadowValue() const { return class_type_ == kShadowClass; }
  bool IsStringValue() const { return class_type_ == kStringClass; }
  bool IsURIValue() const { return class_type_ == kURIClass; }
  bool IsCubicBezierTimingFunctionValue() const {
    return class_type_ == kCubicBezierTimingFunctionClass;
  }
  bool IsStepsTimingFunctionValue() const {
    return class_type_ == kStepsTimingFunctionClass;
  }
  bool IsGridTemplateAreasValue() const {
    return class_type_ == kGridTemplateAreasClass;
  }
  bool IsContentDistributionValue() const {
    return class_type_ == kCSSContentDistributionClass;
  }
  bool IsUnicodeRangeValue() const { return class_type_ == kUnicodeRangeClass; }
  bool IsGridLineNamesValue() const {
    return class_type_ == kGridLineNamesClass;
  }
  bool IsCustomPropertyDeclaration() const {
    return class_type_ == kCustomPropertyDeclarationClass;
  }
  bool IsVariableReferenceValue() const {
    return class_type_ == kVariableReferenceClass;
  }
  bool IsGridAutoRepeatValue() const {
    return class_type_ == kGridAutoRepeatClass;
  }
  bool IsGridIntegerRepeatValue() const {
    return class_type_ == kGridIntegerRepeatClass;
  }
  bool IsPendingSubstitutionValue() const {
    return class_type_ == kPendingSubstitutionValueClass;
  }
  bool IsInvalidVariableValue() const {
    return class_type_ == kInvalidVariableValueClass;
  }
  bool IsAxisValue() const { return class_type_ == kAxisClass; }
  bool IsShorthandWrapperValue() const {
    return class_type_ == kKeyframeShorthandClass;
  }
  bool IsInitialColorValue() const {
    return class_type_ == kInitialColorValueClass;
  }
  bool IsLightDarkValuePair() const {
    return class_type_ == kLightDarkValuePairClass;
  }
  bool IsIdSelectorValue() const { return class_type_ == kIdSelectorClass; }
  bool IsElementOffsetValue() const {
    return class_type_ == kElementOffsetClass;
  }

  bool HasFailedOrCanceledSubresources() const;
  bool MayContainUrl() const;
  void ReResolveUrl(const Document&) const;

  bool operator==(const CSSValue&) const;
  bool operator!=(const CSSValue& o) const { return !(*this == o); }

  void FinalizeGarbageCollectedObject();
  void TraceAfterDispatch(blink::Visitor* visitor) const {}
  void Trace(Visitor*) const;

  // ~CSSValue should be public, because non-public ~CSSValue causes C2248
  // error: 'blink::CSSValue::~CSSValue' : cannot access protected member
  // declared in class 'blink::CSSValue' when compiling
  // 'source\wtf\refcounted.h' by using msvc.
  ~CSSValue() = default;

 protected:
  enum ClassType {
    kNumericLiteralClass,
    kMathFunctionClass,
    kIdentifierClass,
    kColorClass,
    kCounterClass,
    kQuadClass,
    kCustomIdentClass,
    kStringClass,
    kURIClass,
    kValuePairClass,
    kLightDarkValuePairClass,
    kIdSelectorClass,
    kElementOffsetClass,

    // Basic shape classes.
    // TODO(sashab): Represent these as a single subclass, BasicShapeClass.
    kBasicShapeCircleClass,
    kBasicShapeEllipseClass,
    kBasicShapePolygonClass,
    kBasicShapeInsetClass,

    // Image classes.
    kImageClass,
    kCursorImageClass,

    // Image generator classes.
    kCrossfadeClass,
    kPaintClass,
    kLinearGradientClass,
    kRadialGradientClass,
    kConicGradientClass,

    // Timing function classes.
    kCubicBezierTimingFunctionClass,
    kStepsTimingFunctionClass,

    // Other class types.
    kBorderImageSliceClass,
    kFontFeatureClass,
    kFontFaceSrcClass,
    kFontFamilyClass,
    kFontStyleRangeClass,
    kFontVariationClass,

    kInheritedClass,
    kInitialClass,
    kUnsetClass,
    kRevertClass,

    kReflectClass,
    kShadowClass,
    kUnicodeRangeClass,
    kGridTemplateAreasClass,
    kPathClass,
    kRayClass,
    kVariableReferenceClass,
    kCustomPropertyDeclarationClass,
    kPendingSubstitutionValueClass,
    kInvalidVariableValueClass,
    kLayoutFunctionClass,

    kCSSContentDistributionClass,

    kKeyframeShorthandClass,
    kInitialColorValueClass,

    // List class types must appear after ValueListClass.
    kValueListClass,
    kFunctionClass,
    kImageSetClass,
    kGridLineNamesClass,
    kGridAutoRepeatClass,
    kGridIntegerRepeatClass,
    kAxisClass,
    // Do not append non-list class types here.
  };

  static const size_t kValueListSeparatorBits = 2;
  enum ValueListSeparator { kSpaceSeparator, kCommaSeparator, kSlashSeparator };

  ClassType GetClassType() const { return static_cast<ClassType>(class_type_); }

  explicit CSSValue(ClassType class_type)
      : numeric_literal_unit_type_(0),
        is_non_negative_math_function_(false),
        value_list_separator_(kSpaceSeparator),
        allows_negative_percentage_reference_(false),
        class_type_(class_type) {}

  // NOTE: This class is non-virtual for memory and performance reasons.
  // Don't go making it virtual again unless you know exactly what you're doing!

 protected:
  // The bits in this section are only used by specific subclasses but kept here
  // to maximize struct packing.
  // The bits are ordered and split into groups to such that from the
  // perspective of each subclass, each field is a separate memory location.
  // Using NOLINT here allows to use uint8_t as bitfield type which reduces
  // size of CSSValue from 4 bytes to 3 bytes.

  // CSSNumericLiteralValue bits:
  // This field hold CSSPrimitiveValue::UnitType.
  uint8_t numeric_literal_unit_type_ : 7;  // NOLINT

  // CSSMathFunctionValue:
  uint8_t is_non_negative_math_function_ : 1;  // NOLINT

  // Force a new memory location. This will make TSAN treat the 2 fields above
  // this line as a separate memory location than the 2 fields below it.
  char : 0;

  // CSSNumericLiteralValue bits:
  uint8_t value_list_separator_ : kValueListSeparatorBits;  // NOLINT

  // CSSMathFunctionValue:
  uint8_t allows_negative_percentage_reference_ : 1;  // NOLINT

 private:
  const uint8_t class_type_;  // ClassType
};

template <typename CSSValueType, wtf_size_t inlineCapacity>
inline bool CompareCSSValueVector(
    const HeapVector<Member<CSSValueType>, inlineCapacity>& first_vector,
    const HeapVector<Member<CSSValueType>, inlineCapacity>& second_vector) {
  wtf_size_t size = first_vector.size();
  if (size != second_vector.size()) {
    return false;
  }

  for (wtf_size_t i = 0; i < size; i++) {
    if (!DataEquivalent(first_vector[i], second_vector[i])) {
      return false;
    }
  }
  return true;
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_VALUE_H_
