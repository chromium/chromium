/*
 * Copyright (C) 2011 Andreas Kling (kling@webkit.org)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "third_party/blink/renderer/core/css/css_value.h"

#include "third_party/blink/renderer/core/css/css_axis_value.h"
#include "third_party/blink/renderer/core/css/css_basic_shape_values.h"
#include "third_party/blink/renderer/core/css/css_border_image_slice_value.h"
#include "third_party/blink/renderer/core/css/css_color_value.h"
#include "third_party/blink/renderer/core/css/css_content_distribution_value.h"
#include "third_party/blink/renderer/core/css/css_counter_value.h"
#include "third_party/blink/renderer/core/css/css_crossfade_value.h"
#include "third_party/blink/renderer/core/css/css_cursor_image_value.h"
#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"
#include "third_party/blink/renderer/core/css/css_custom_property_declaration.h"
#include "third_party/blink/renderer/core/css/css_font_face_src_value.h"
#include "third_party/blink/renderer/core/css/css_font_family_value.h"
#include "third_party/blink/renderer/core/css/css_font_feature_value.h"
#include "third_party/blink/renderer/core/css/css_font_style_range_value.h"
#include "third_party/blink/renderer/core/css/css_font_variation_value.h"
#include "third_party/blink/renderer/core/css/css_function_value.h"
#include "third_party/blink/renderer/core/css/css_gradient_value.h"
#include "third_party/blink/renderer/core/css/css_grid_auto_repeat_value.h"
#include "third_party/blink/renderer/core/css/css_grid_integer_repeat_value.h"
#include "third_party/blink/renderer/core/css/css_grid_line_names_value.h"
#include "third_party/blink/renderer/core/css/css_grid_template_areas_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_image_set_value.h"
#include "third_party/blink/renderer/core/css/css_image_value.h"
#include "third_party/blink/renderer/core/css/css_inherited_value.h"
#include "third_party/blink/renderer/core/css/css_initial_value.h"
#include "third_party/blink/renderer/core/css/css_invalid_variable_value.h"
#include "third_party/blink/renderer/core/css/css_keyframe_shorthand_value.h"
#include "third_party/blink/renderer/core/css/css_layout_function_value.h"
#include "third_party/blink/renderer/core/css/css_light_dark_color_pair.h"
#include "third_party/blink/renderer/core/css/css_math_function_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_paint_value.h"
#include "third_party/blink/renderer/core/css/css_path_value.h"
#include "third_party/blink/renderer/core/css/css_pending_interpolation_value.h"
#include "third_party/blink/renderer/core/css/css_pending_substitution_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_quad_value.h"
#include "third_party/blink/renderer/core/css/css_ray_value.h"
#include "third_party/blink/renderer/core/css/css_reflect_value.h"
#include "third_party/blink/renderer/core/css/css_shadow_value.h"
#include "third_party/blink/renderer/core/css/css_string_value.h"
#include "third_party/blink/renderer/core/css/css_timing_function_value.h"
#include "third_party/blink/renderer/core/css/css_unicode_range_value.h"
#include "third_party/blink/renderer/core/css/css_unset_value.h"
#include "third_party/blink/renderer/core/css/css_uri_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/css_value_pair.h"
#include "third_party/blink/renderer/core/css/css_variable_reference_value.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"

namespace blink {

struct SameSizeAsCSSValue final : public GarbageCollected<SameSizeAsCSSValue> {
  uint32_t bitfields;
};
ASSERT_SIZE(CSSValue, SameSizeAsCSSValue);

CSSValue* CSSValue::Create(const Length& value, float zoom) {
  switch (value.GetType()) {
    case Length::kAuto:
    case Length::kMinContent:
    case Length::kMaxContent:
    case Length::kFillAvailable:
    case Length::kFitContent:
    case Length::kExtendToZoom:
      return CSSIdentifierValue::Create(value);
    case Length::kPercent:
    case Length::kFixed:
    case Length::kCalculated:
      return CSSPrimitiveValue::CreateFromLength(value, zoom);
    case Length::kDeviceWidth:
    case Length::kDeviceHeight:
    case Length::kMaxSizeNone:
      NOTREACHED();
      break;
  }
  return nullptr;
}

bool CSSValue::HasFailedOrCanceledSubresources() const {
  if (IsValueList())
    return To<CSSValueList>(this)->HasFailedOrCanceledSubresources();
  if (GetClassType() == kFontFaceSrcClass)
    return To<CSSFontFaceSrcValue>(this)->HasFailedOrCanceledSubresources();
  if (GetClassType() == kImageClass)
    return To<CSSImageValue>(this)->HasFailedOrCanceledSubresources();
  if (GetClassType() == kCrossfadeClass) {
    return To<cssvalue::CSSCrossfadeValue>(this)
        ->HasFailedOrCanceledSubresources();
  }
  if (GetClassType() == kImageSetClass)
    return To<CSSImageSetValue>(this)->HasFailedOrCanceledSubresources();

  return false;
}

bool CSSValue::MayContainUrl() const {
  if (IsValueList())
    return To<CSSValueList>(*this).MayContainUrl();
  return IsImageValue() || IsURIValue();
}

void CSSValue::ReResolveUrl(const Document& document) const {
  // TODO(fs): Should handle all values that can contain URLs.
  if (IsImageValue()) {
    To<CSSImageValue>(*this).ReResolveURL(document);
    return;
  }
  if (IsURIValue()) {
    To<cssvalue::CSSURIValue>(*this).ReResolveUrl(document);
    return;
  }
  if (IsValueList()) {
    To<CSSValueList>(*this).ReResolveUrl(document);
    return;
  }
}

template <class ChildClassType>
inline static bool CompareCSSValues(const CSSValue& first,
                                    const CSSValue& second) {
  return static_cast<const ChildClassType&>(first).Equals(
      static_cast<const ChildClassType&>(second));
}

bool CSSValue::operator==(const CSSValue& other) const {
  if (class_type_ == other.class_type_) {
    switch (GetClassType()) {
      case kAxisClass:
        return CompareCSSValues<cssvalue::CSSAxisValue>(*this, other);
      case kBasicShapeCircleClass:
        return CompareCSSValues<cssvalue::CSSBasicShapeCircleValue>(*this,
                                                                    other);
      case kBasicShapeEllipseClass:
        return CompareCSSValues<cssvalue::CSSBasicShapeEllipseValue>(*this,
                                                                     other);
      case kBasicShapePolygonClass:
        return CompareCSSValues<cssvalue::CSSBasicShapePolygonValue>(*this,
                                                                     other);
      case kBasicShapeInsetClass:
        return CompareCSSValues<cssvalue::CSSBasicShapeInsetValue>(*this,
                                                                   other);
      case kBorderImageSliceClass:
        return CompareCSSValues<cssvalue::CSSBorderImageSliceValue>(*this,
                                                                    other);
      case kColorClass:
        return CompareCSSValues<cssvalue::CSSColorValue>(*this, other);
      case kCounterClass:
        return CompareCSSValues<cssvalue::CSSCounterValue>(*this, other);
      case kCursorImageClass:
        return CompareCSSValues<cssvalue::CSSCursorImageValue>(*this, other);
      case kFontFaceSrcClass:
        return CompareCSSValues<CSSFontFaceSrcValue>(*this, other);
      case kFontFamilyClass:
        return CompareCSSValues<CSSFontFamilyValue>(*this, other);
      case kFontFeatureClass:
        return CompareCSSValues<cssvalue::CSSFontFeatureValue>(*this, other);
      case kFontStyleRangeClass:
        return CompareCSSValues<cssvalue::CSSFontStyleRangeValue>(*this, other);
      case kFontVariationClass:
        return CompareCSSValues<cssvalue::CSSFontVariationValue>(*this, other);
      case kFunctionClass:
        return CompareCSSValues<CSSFunctionValue>(*this, other);
      case kLayoutFunctionClass:
        return CompareCSSValues<cssvalue::CSSLayoutFunctionValue>(*this, other);
      case kLinearGradientClass:
        return CompareCSSValues<cssvalue::CSSLinearGradientValue>(*this, other);
      case kRadialGradientClass:
        return CompareCSSValues<cssvalue::CSSRadialGradientValue>(*this, other);
      case kConicGradientClass:
        return CompareCSSValues<cssvalue::CSSConicGradientValue>(*this, other);
      case kCrossfadeClass:
        return CompareCSSValues<cssvalue::CSSCrossfadeValue>(*this, other);
      case kPaintClass:
        return CompareCSSValues<CSSPaintValue>(*this, other);
      case kCustomIdentClass:
        return CompareCSSValues<CSSCustomIdentValue>(*this, other);
      case kImageClass:
        return CompareCSSValues<CSSImageValue>(*this, other);
      case kInheritedClass:
        return CompareCSSValues<CSSInheritedValue>(*this, other);
      case kInitialClass:
        return CompareCSSValues<CSSInitialValue>(*this, other);
      case kUnsetClass:
        return CompareCSSValues<cssvalue::CSSUnsetValue>(*this, other);
      case kGridAutoRepeatClass:
        return CompareCSSValues<cssvalue::CSSGridAutoRepeatValue>(*this, other);
      case kGridIntegerRepeatClass:
        return CompareCSSValues<cssvalue::CSSGridIntegerRepeatValue>(*this,
                                                                     other);
      case kGridLineNamesClass:
        return CompareCSSValues<cssvalue::CSSGridLineNamesValue>(*this, other);
      case kGridTemplateAreasClass:
        return CompareCSSValues<cssvalue::CSSGridTemplateAreasValue>(*this,
                                                                     other);
      case kPathClass:
        return CompareCSSValues<cssvalue::CSSPathValue>(*this, other);
      case kNumericLiteralClass:
        return CompareCSSValues<CSSNumericLiteralValue>(*this, other);
      case kMathFunctionClass:
        return CompareCSSValues<CSSMathFunctionValue>(*this, other);
      case kRayClass:
        return CompareCSSValues<cssvalue::CSSRayValue>(*this, other);
      case kIdentifierClass:
        return CompareCSSValues<CSSIdentifierValue>(*this, other);
      case kKeyframeShorthandClass:
        return CompareCSSValues<CSSKeyframeShorthandValue>(*this, other);
      case kQuadClass:
        return CompareCSSValues<CSSQuadValue>(*this, other);
      case kReflectClass:
        return CompareCSSValues<cssvalue::CSSReflectValue>(*this, other);
      case kShadowClass:
        return CompareCSSValues<CSSShadowValue>(*this, other);
      case kStringClass:
        return CompareCSSValues<CSSStringValue>(*this, other);
      case kCubicBezierTimingFunctionClass:
        return CompareCSSValues<cssvalue::CSSCubicBezierTimingFunctionValue>(
            *this, other);
      case kStepsTimingFunctionClass:
        return CompareCSSValues<cssvalue::CSSStepsTimingFunctionValue>(*this,
                                                                       other);
      case kUnicodeRangeClass:
        return CompareCSSValues<cssvalue::CSSUnicodeRangeValue>(*this, other);
      case kURIClass:
        return CompareCSSValues<cssvalue::CSSURIValue>(*this, other);
      case kValueListClass:
        return CompareCSSValues<CSSValueList>(*this, other);
      case kValuePairClass:
        return CompareCSSValues<CSSValuePair>(*this, other);
      case kImageSetClass:
        return CompareCSSValues<CSSImageSetValue>(*this, other);
      case kCSSContentDistributionClass:
        return CompareCSSValues<cssvalue::CSSContentDistributionValue>(*this,
                                                                       other);
      case kPendingInterpolationClass:
        return CompareCSSValues<cssvalue::CSSPendingInterpolationValue>(*this,
                                                                        other);
      case kCustomPropertyDeclarationClass:
        return CompareCSSValues<CSSCustomPropertyDeclaration>(*this, other);
      case kVariableReferenceClass:
        return CompareCSSValues<CSSVariableReferenceValue>(*this, other);
      case kPendingSubstitutionValueClass:
        return CompareCSSValues<cssvalue::CSSPendingSubstitutionValue>(*this,
                                                                       other);
      case kInvalidVariableValueClass:
        return CompareCSSValues<CSSInvalidVariableValue>(*this, other);
      case kLightDarkColorPairClass:
        return CompareCSSValues<CSSLightDarkColorPair>(*this, other);
    }
    NOTREACHED();
    return false;
  }
  return false;
}

String CSSValue::CssText() const {
  switch (GetClassType()) {
    case kAxisClass:
      return To<cssvalue::CSSAxisValue>(this)->CustomCSSText();
    case kBasicShapeCircleClass:
      return To<cssvalue::CSSBasicShapeCircleValue>(this)->CustomCSSText();
    case kBasicShapeEllipseClass:
      return To<cssvalue::CSSBasicShapeEllipseValue>(this)->CustomCSSText();
    case kBasicShapePolygonClass:
      return To<cssvalue::CSSBasicShapePolygonValue>(this)->CustomCSSText();
    case kBasicShapeInsetClass:
      return To<cssvalue::CSSBasicShapeInsetValue>(this)->CustomCSSText();
    case kBorderImageSliceClass:
      return To<cssvalue::CSSBorderImageSliceValue>(this)->CustomCSSText();
    case kColorClass:
      return To<cssvalue::CSSColorValue>(this)->CustomCSSText();
    case kCounterClass:
      return To<cssvalue::CSSCounterValue>(this)->CustomCSSText();
    case kCursorImageClass:
      return To<cssvalue::CSSCursorImageValue>(this)->CustomCSSText();
    case kFontFaceSrcClass:
      return To<CSSFontFaceSrcValue>(this)->CustomCSSText();
    case kFontFamilyClass:
      return To<CSSFontFamilyValue>(this)->CustomCSSText();
    case kFontFeatureClass:
      return To<cssvalue::CSSFontFeatureValue>(this)->CustomCSSText();
    case kFontStyleRangeClass:
      return To<cssvalue::CSSFontStyleRangeValue>(this)->CustomCSSText();
    case kFontVariationClass:
      return To<cssvalue::CSSFontVariationValue>(this)->CustomCSSText();
    case kFunctionClass:
      return To<CSSFunctionValue>(this)->CustomCSSText();
    case kLayoutFunctionClass:
      return To<cssvalue::CSSLayoutFunctionValue>(this)->CustomCSSText();
    case kLinearGradientClass:
      return To<cssvalue::CSSLinearGradientValue>(this)->CustomCSSText();
    case kRadialGradientClass:
      return To<cssvalue::CSSRadialGradientValue>(this)->CustomCSSText();
    case kConicGradientClass:
      return To<cssvalue::CSSConicGradientValue>(this)->CustomCSSText();
    case kCrossfadeClass:
      return To<cssvalue::CSSCrossfadeValue>(this)->CustomCSSText();
    case kPaintClass:
      return To<CSSPaintValue>(this)->CustomCSSText();
    case kCustomIdentClass:
      return To<CSSCustomIdentValue>(this)->CustomCSSText();
    case kImageClass:
      return To<CSSImageValue>(this)->CustomCSSText();
    case kInheritedClass:
      return To<CSSInheritedValue>(this)->CustomCSSText();
    case kUnsetClass:
      return To<cssvalue::CSSUnsetValue>(this)->CustomCSSText();
    case kInitialClass:
      return To<CSSInitialValue>(this)->CustomCSSText();
    case kGridAutoRepeatClass:
      return To<cssvalue::CSSGridAutoRepeatValue>(this)->CustomCSSText();
    case kGridIntegerRepeatClass:
      return To<cssvalue::CSSGridIntegerRepeatValue>(this)->CustomCSSText();
    case kGridLineNamesClass:
      return To<cssvalue::CSSGridLineNamesValue>(this)->CustomCSSText();
    case kGridTemplateAreasClass:
      return To<cssvalue::CSSGridTemplateAreasValue>(this)->CustomCSSText();
    case kPathClass:
      return To<cssvalue::CSSPathValue>(this)->CustomCSSText();
    case kNumericLiteralClass:
      return To<CSSNumericLiteralValue>(this)->CustomCSSText();
    case kMathFunctionClass:
      return To<CSSMathFunctionValue>(this)->CustomCSSText();
    case kRayClass:
      return To<cssvalue::CSSRayValue>(this)->CustomCSSText();
    case kIdentifierClass:
      return To<CSSIdentifierValue>(this)->CustomCSSText();
    case kKeyframeShorthandClass:
      return To<CSSKeyframeShorthandValue>(this)->CustomCSSText();
    case kQuadClass:
      return To<CSSQuadValue>(this)->CustomCSSText();
    case kReflectClass:
      return To<cssvalue::CSSReflectValue>(this)->CustomCSSText();
    case kShadowClass:
      return To<CSSShadowValue>(this)->CustomCSSText();
    case kStringClass:
      return To<CSSStringValue>(this)->CustomCSSText();
    case kCubicBezierTimingFunctionClass:
      return To<cssvalue::CSSCubicBezierTimingFunctionValue>(this)
          ->CustomCSSText();
    case kStepsTimingFunctionClass:
      return To<cssvalue::CSSStepsTimingFunctionValue>(this)->CustomCSSText();
    case kUnicodeRangeClass:
      return To<cssvalue::CSSUnicodeRangeValue>(this)->CustomCSSText();
    case kURIClass:
      return To<cssvalue::CSSURIValue>(this)->CustomCSSText();
    case kValuePairClass:
      return To<CSSValuePair>(this)->CustomCSSText();
    case kValueListClass:
      return To<CSSValueList>(this)->CustomCSSText();
    case kImageSetClass:
      return To<CSSImageSetValue>(this)->CustomCSSText();
    case kCSSContentDistributionClass:
      return To<cssvalue::CSSContentDistributionValue>(this)->CustomCSSText();
    case kPendingInterpolationClass:
      return To<cssvalue::CSSPendingInterpolationValue>(this)->CustomCSSText();
    case kVariableReferenceClass:
      return To<CSSVariableReferenceValue>(this)->CustomCSSText();
    case kCustomPropertyDeclarationClass:
      return To<CSSCustomPropertyDeclaration>(this)->CustomCSSText();
    case kPendingSubstitutionValueClass:
      return To<cssvalue::CSSPendingSubstitutionValue>(this)->CustomCSSText();
    case kInvalidVariableValueClass:
      return To<CSSInvalidVariableValue>(this)->CustomCSSText();
    case kLightDarkColorPairClass:
      return To<CSSLightDarkColorPair>(this)->CustomCSSText();
  }
  NOTREACHED();
  return String();
}

void CSSValue::FinalizeGarbageCollectedObject() {
  switch (GetClassType()) {
    case kAxisClass:
      To<cssvalue::CSSAxisValue>(this)->~CSSAxisValue();
      return;
    case kBasicShapeCircleClass:
      To<cssvalue::CSSBasicShapeCircleValue>(this)->~CSSBasicShapeCircleValue();
      return;
    case kBasicShapeEllipseClass:
      To<cssvalue::CSSBasicShapeEllipseValue>(this)
          ->~CSSBasicShapeEllipseValue();
      return;
    case kBasicShapePolygonClass:
      To<cssvalue::CSSBasicShapePolygonValue>(this)
          ->~CSSBasicShapePolygonValue();
      return;
    case kBasicShapeInsetClass:
      To<cssvalue::CSSBasicShapeInsetValue>(this)->~CSSBasicShapeInsetValue();
      return;
    case kBorderImageSliceClass:
      To<cssvalue::CSSBorderImageSliceValue>(this)->~CSSBorderImageSliceValue();
      return;
    case kColorClass:
      To<cssvalue::CSSColorValue>(this)->~CSSColorValue();
      return;
    case kCounterClass:
      To<cssvalue::CSSCounterValue>(this)->~CSSCounterValue();
      return;
    case kCursorImageClass:
      To<cssvalue::CSSCursorImageValue>(this)->~CSSCursorImageValue();
      return;
    case kFontFaceSrcClass:
      To<CSSFontFaceSrcValue>(this)->~CSSFontFaceSrcValue();
      return;
    case kFontFamilyClass:
      To<CSSFontFamilyValue>(this)->~CSSFontFamilyValue();
      return;
    case kFontFeatureClass:
      To<cssvalue::CSSFontFeatureValue>(this)->~CSSFontFeatureValue();
      return;
    case kFontStyleRangeClass:
      To<cssvalue::CSSFontStyleRangeValue>(this)->~CSSFontStyleRangeValue();
      return;
    case kFontVariationClass:
      To<cssvalue::CSSFontVariationValue>(this)->~CSSFontVariationValue();
      return;
    case kFunctionClass:
      To<CSSFunctionValue>(this)->~CSSFunctionValue();
      return;
    case kLayoutFunctionClass:
      To<cssvalue::CSSLayoutFunctionValue>(this)->~CSSLayoutFunctionValue();
      return;
    case kLinearGradientClass:
      To<cssvalue::CSSLinearGradientValue>(this)->~CSSLinearGradientValue();
      return;
    case kRadialGradientClass:
      To<cssvalue::CSSRadialGradientValue>(this)->~CSSRadialGradientValue();
      return;
    case kConicGradientClass:
      To<cssvalue::CSSConicGradientValue>(this)->~CSSConicGradientValue();
      return;
    case kCrossfadeClass:
      To<cssvalue::CSSCrossfadeValue>(this)->~CSSCrossfadeValue();
      return;
    case kPaintClass:
      To<CSSPaintValue>(this)->~CSSPaintValue();
      return;
    case kCustomIdentClass:
      To<CSSCustomIdentValue>(this)->~CSSCustomIdentValue();
      return;
    case kImageClass:
      To<CSSImageValue>(this)->~CSSImageValue();
      return;
    case kInheritedClass:
      To<CSSInheritedValue>(this)->~CSSInheritedValue();
      return;
    case kInitialClass:
      To<CSSInitialValue>(this)->~CSSInitialValue();
      return;
    case kUnsetClass:
      To<cssvalue::CSSUnsetValue>(this)->~CSSUnsetValue();
      return;
    case kGridAutoRepeatClass:
      To<cssvalue::CSSGridAutoRepeatValue>(this)->~CSSGridAutoRepeatValue();
      return;
    case kGridIntegerRepeatClass:
      To<cssvalue::CSSGridIntegerRepeatValue>(this)
          ->~CSSGridIntegerRepeatValue();
      return;
    case kGridLineNamesClass:
      To<cssvalue::CSSGridLineNamesValue>(this)->~CSSGridLineNamesValue();
      return;
    case kGridTemplateAreasClass:
      To<cssvalue::CSSGridTemplateAreasValue>(this)
          ->~CSSGridTemplateAreasValue();
      return;
    case kPathClass:
      To<cssvalue::CSSPathValue>(this)->~CSSPathValue();
      return;
    case kNumericLiteralClass:
      To<CSSNumericLiteralValue>(this)->~CSSNumericLiteralValue();
      return;
    case kMathFunctionClass:
      To<CSSMathFunctionValue>(this)->~CSSMathFunctionValue();
      return;
    case kRayClass:
      To<cssvalue::CSSRayValue>(this)->~CSSRayValue();
      return;
    case kIdentifierClass:
      To<CSSIdentifierValue>(this)->~CSSIdentifierValue();
      return;
    case kKeyframeShorthandClass:
      To<CSSKeyframeShorthandValue>(this)->~CSSKeyframeShorthandValue();
      return;
    case kQuadClass:
      To<CSSQuadValue>(this)->~CSSQuadValue();
      return;
    case kReflectClass:
      To<cssvalue::CSSReflectValue>(this)->~CSSReflectValue();
      return;
    case kShadowClass:
      To<CSSShadowValue>(this)->~CSSShadowValue();
      return;
    case kStringClass:
      To<CSSStringValue>(this)->~CSSStringValue();
      return;
    case kCubicBezierTimingFunctionClass:
      To<cssvalue::CSSCubicBezierTimingFunctionValue>(this)
          ->~CSSCubicBezierTimingFunctionValue();
      return;
    case kStepsTimingFunctionClass:
      To<cssvalue::CSSStepsTimingFunctionValue>(this)
          ->~CSSStepsTimingFunctionValue();
      return;
    case kUnicodeRangeClass:
      To<cssvalue::CSSUnicodeRangeValue>(this)->~CSSUnicodeRangeValue();
      return;
    case kURIClass:
      To<cssvalue::CSSURIValue>(this)->~CSSURIValue();
      return;
    case kValueListClass:
      To<CSSValueList>(this)->~CSSValueList();
      return;
    case kValuePairClass:
      To<CSSValuePair>(this)->~CSSValuePair();
      return;
    case kImageSetClass:
      To<CSSImageSetValue>(this)->~CSSImageSetValue();
      return;
    case kCSSContentDistributionClass:
      To<cssvalue::CSSContentDistributionValue>(this)
          ->~CSSContentDistributionValue();
      return;
    case kPendingInterpolationClass:
      To<cssvalue::CSSPendingInterpolationValue>(this)
          ->~CSSPendingInterpolationValue();
      return;
    case kVariableReferenceClass:
      To<CSSVariableReferenceValue>(this)->~CSSVariableReferenceValue();
      return;
    case kCustomPropertyDeclarationClass:
      To<CSSCustomPropertyDeclaration>(this)->~CSSCustomPropertyDeclaration();
      return;
    case kPendingSubstitutionValueClass:
      To<cssvalue::CSSPendingSubstitutionValue>(this)
          ->~CSSPendingSubstitutionValue();
      return;
    case kInvalidVariableValueClass:
      To<CSSInvalidVariableValue>(this)->~CSSInvalidVariableValue();
      return;
    case kLightDarkColorPairClass:
      To<CSSLightDarkColorPair>(this)->~CSSLightDarkColorPair();
      return;
  }
  NOTREACHED();
}

void CSSValue::Trace(blink::Visitor* visitor) {
  switch (GetClassType()) {
    case kAxisClass:
      To<cssvalue::CSSAxisValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kBasicShapeCircleClass:
      To<cssvalue::CSSBasicShapeCircleValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kBasicShapeEllipseClass:
      To<cssvalue::CSSBasicShapeEllipseValue>(this)->TraceAfterDispatch(
          visitor);
      return;
    case kBasicShapePolygonClass:
      To<cssvalue::CSSBasicShapePolygonValue>(this)->TraceAfterDispatch(
          visitor);
      return;
    case kBasicShapeInsetClass:
      To<cssvalue::CSSBasicShapeInsetValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kBorderImageSliceClass:
      To<cssvalue::CSSBorderImageSliceValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kColorClass:
      To<cssvalue::CSSColorValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kCounterClass:
      To<cssvalue::CSSCounterValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kCursorImageClass:
      To<cssvalue::CSSCursorImageValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kFontFaceSrcClass:
      To<CSSFontFaceSrcValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kFontFamilyClass:
      To<CSSFontFamilyValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kFontFeatureClass:
      To<cssvalue::CSSFontFeatureValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kFontStyleRangeClass:
      To<cssvalue::CSSFontStyleRangeValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kFontVariationClass:
      To<cssvalue::CSSFontVariationValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kFunctionClass:
      To<CSSFunctionValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kLayoutFunctionClass:
      To<cssvalue::CSSLayoutFunctionValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kLinearGradientClass:
      To<cssvalue::CSSLinearGradientValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kRadialGradientClass:
      To<cssvalue::CSSRadialGradientValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kConicGradientClass:
      To<cssvalue::CSSConicGradientValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kCrossfadeClass:
      To<cssvalue::CSSCrossfadeValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kPaintClass:
      To<CSSPaintValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kCustomIdentClass:
      To<CSSCustomIdentValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kImageClass:
      To<CSSImageValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kInheritedClass:
      To<CSSInheritedValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kInitialClass:
      To<CSSInitialValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kUnsetClass:
      To<cssvalue::CSSUnsetValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kGridAutoRepeatClass:
      To<cssvalue::CSSGridAutoRepeatValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kGridIntegerRepeatClass:
      To<cssvalue::CSSGridIntegerRepeatValue>(this)->TraceAfterDispatch(
          visitor);
      return;
    case kGridLineNamesClass:
      To<cssvalue::CSSGridLineNamesValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kGridTemplateAreasClass:
      To<cssvalue::CSSGridTemplateAreasValue>(this)->TraceAfterDispatch(
          visitor);
      return;
    case kPathClass:
      To<cssvalue::CSSPathValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kNumericLiteralClass:
      To<CSSNumericLiteralValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kMathFunctionClass:
      To<CSSMathFunctionValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kRayClass:
      To<cssvalue::CSSRayValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kIdentifierClass:
      To<CSSIdentifierValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kKeyframeShorthandClass:
      To<CSSKeyframeShorthandValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kQuadClass:
      To<CSSQuadValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kReflectClass:
      To<cssvalue::CSSReflectValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kShadowClass:
      To<CSSShadowValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kStringClass:
      To<CSSStringValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kCubicBezierTimingFunctionClass:
      To<cssvalue::CSSCubicBezierTimingFunctionValue>(this)->TraceAfterDispatch(
          visitor);
      return;
    case kStepsTimingFunctionClass:
      To<cssvalue::CSSStepsTimingFunctionValue>(this)->TraceAfterDispatch(
          visitor);
      return;
    case kUnicodeRangeClass:
      To<cssvalue::CSSUnicodeRangeValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kURIClass:
      To<cssvalue::CSSURIValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kValueListClass:
      To<CSSValueList>(this)->TraceAfterDispatch(visitor);
      return;
    case kValuePairClass:
      To<CSSValuePair>(this)->TraceAfterDispatch(visitor);
      return;
    case kImageSetClass:
      To<CSSImageSetValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kCSSContentDistributionClass:
      To<cssvalue::CSSContentDistributionValue>(this)->TraceAfterDispatch(
          visitor);
      return;
    case kPendingInterpolationClass:
      To<cssvalue::CSSPendingInterpolationValue>(this)->TraceAfterDispatch(
          visitor);
      return;
    case kVariableReferenceClass:
      To<CSSVariableReferenceValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kCustomPropertyDeclarationClass:
      To<CSSCustomPropertyDeclaration>(this)->TraceAfterDispatch(visitor);
      return;
    case kPendingSubstitutionValueClass:
      To<cssvalue::CSSPendingSubstitutionValue>(this)->TraceAfterDispatch(
          visitor);
      return;
    case kInvalidVariableValueClass:
      To<CSSInvalidVariableValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kLightDarkColorPairClass:
      To<CSSLightDarkColorPair>(this)->TraceAfterDispatch(visitor);
      return;
  }
  NOTREACHED();
}

}  // namespace blink
