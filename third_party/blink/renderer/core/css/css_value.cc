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
#include "third_party/blink/renderer/core/css/css_grid_line_names_value.h"
#include "third_party/blink/renderer/core/css/css_grid_template_areas_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_image_set_value.h"
#include "third_party/blink/renderer/core/css/css_image_value.h"
#include "third_party/blink/renderer/core/css/css_inherited_value.h"
#include "third_party/blink/renderer/core/css/css_initial_value.h"
#include "third_party/blink/renderer/core/css/css_layout_function_value.h"
#include "third_party/blink/renderer/core/css/css_paint_value.h"
#include "third_party/blink/renderer/core/css/css_path_value.h"
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

using namespace cssvalue;

struct SameSizeAsCSSValue
    : public GarbageCollectedFinalized<SameSizeAsCSSValue> {
  uint32_t bitfields;
};
ASSERT_SIZE(CSSValue, SameSizeAsCSSValue);

CSSValue* CSSValue::Create(const Length& value, float zoom) {
  switch (value.GetType()) {
    case kAuto:
    case kMinContent:
    case kMaxContent:
    case kFillAvailable:
    case kFitContent:
    case kExtendToZoom:
      return CSSIdentifierValue::Create(value);
    case kPercent:
    case kFixed:
    case kCalculated:
      return CSSPrimitiveValue::Create(value, zoom);
    case kDeviceWidth:
    case kDeviceHeight:
    case kMaxSizeNone:
      NOTREACHED();
      break;
  }
  return nullptr;
}

bool CSSValue::HasFailedOrCanceledSubresources() const {
  if (IsValueList())
    return ToCSSValueList(this)->HasFailedOrCanceledSubresources();
  if (GetClassType() == kFontFaceSrcClass)
    return ToCSSFontFaceSrcValue(this)->HasFailedOrCanceledSubresources();
  if (GetClassType() == kImageClass)
    return ToCSSImageValue(this)->HasFailedOrCanceledSubresources();
  if (GetClassType() == kCrossfadeClass)
    return ToCSSCrossfadeValue(this)->HasFailedOrCanceledSubresources();
  if (GetClassType() == kImageSetClass)
    return ToCSSImageSetValue(this)->HasFailedOrCanceledSubresources();

  return false;
}

bool CSSValue::MayContainUrl() const {
  if (IsValueList())
    return ToCSSValueList(*this).MayContainUrl();
  return IsImageValue() || IsURIValue();
}

void CSSValue::ReResolveUrl(const Document& document) const {
  // TODO(fs): Should handle all values that can contain URLs.
  if (IsImageValue()) {
    ToCSSImageValue(*this).ReResolveURL(document);
    return;
  }
  if (IsURIValue()) {
    ToCSSURIValue(*this).ReResolveUrl(document);
    return;
  }
  if (IsValueList()) {
    ToCSSValueList(*this).ReResolveUrl(document);
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
      case kBasicShapeCircleClass:
        return CompareCSSValues<CSSBasicShapeCircleValue>(*this, other);
      case kBasicShapeEllipseClass:
        return CompareCSSValues<CSSBasicShapeEllipseValue>(*this, other);
      case kBasicShapePolygonClass:
        return CompareCSSValues<CSSBasicShapePolygonValue>(*this, other);
      case kBasicShapeInsetClass:
        return CompareCSSValues<CSSBasicShapeInsetValue>(*this, other);
      case kBorderImageSliceClass:
        return CompareCSSValues<CSSBorderImageSliceValue>(*this, other);
      case kColorClass:
        return CompareCSSValues<CSSColorValue>(*this, other);
      case kCounterClass:
        return CompareCSSValues<CSSCounterValue>(*this, other);
      case kCursorImageClass:
        return CompareCSSValues<CSSCursorImageValue>(*this, other);
      case kFontFaceSrcClass:
        return CompareCSSValues<CSSFontFaceSrcValue>(*this, other);
      case kFontFamilyClass:
        return CompareCSSValues<CSSFontFamilyValue>(*this, other);
      case kFontFeatureClass:
        return CompareCSSValues<cssvalue::CSSFontFeatureValue>(*this, other);
      case kFontStyleRangeClass:
        return CompareCSSValues<CSSFontStyleRangeValue>(*this, other);
      case kFontVariationClass:
        return CompareCSSValues<CSSFontVariationValue>(*this, other);
      case kFunctionClass:
        return CompareCSSValues<CSSFunctionValue>(*this, other);
      case kLayoutFunctionClass:
        return CompareCSSValues<CSSLayoutFunctionValue>(*this, other);
      case kLinearGradientClass:
        return CompareCSSValues<CSSLinearGradientValue>(*this, other);
      case kRadialGradientClass:
        return CompareCSSValues<CSSRadialGradientValue>(*this, other);
      case kConicGradientClass:
        return CompareCSSValues<CSSConicGradientValue>(*this, other);
      case kCrossfadeClass:
        return CompareCSSValues<CSSCrossfadeValue>(*this, other);
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
        return CompareCSSValues<CSSUnsetValue>(*this, other);
      case kGridAutoRepeatClass:
        return CompareCSSValues<CSSGridAutoRepeatValue>(*this, other);
      case kGridLineNamesClass:
        return CompareCSSValues<CSSGridLineNamesValue>(*this, other);
      case kGridTemplateAreasClass:
        return CompareCSSValues<CSSGridTemplateAreasValue>(*this, other);
      case kPathClass:
        return CompareCSSValues<CSSPathValue>(*this, other);
      case kPrimitiveClass:
        return CompareCSSValues<CSSPrimitiveValue>(*this, other);
      case kRayClass:
        return CompareCSSValues<CSSRayValue>(*this, other);
      case kIdentifierClass:
        return CompareCSSValues<CSSIdentifierValue>(*this, other);
      case kQuadClass:
        return CompareCSSValues<CSSQuadValue>(*this, other);
      case kReflectClass:
        return CompareCSSValues<CSSReflectValue>(*this, other);
      case kShadowClass:
        return CompareCSSValues<CSSShadowValue>(*this, other);
      case kStringClass:
        return CompareCSSValues<CSSStringValue>(*this, other);
      case kCubicBezierTimingFunctionClass:
        return CompareCSSValues<CSSCubicBezierTimingFunctionValue>(*this,
                                                                   other);
      case kStepsTimingFunctionClass:
        return CompareCSSValues<CSSStepsTimingFunctionValue>(*this, other);
      case kFramesTimingFunctionClass:
        return CompareCSSValues<CSSFramesTimingFunctionValue>(*this, other);
      case kUnicodeRangeClass:
        return CompareCSSValues<CSSUnicodeRangeValue>(*this, other);
      case kURIClass:
        return CompareCSSValues<CSSURIValue>(*this, other);
      case kValueListClass:
        return CompareCSSValues<CSSValueList>(*this, other);
      case kValuePairClass:
        return CompareCSSValues<CSSValuePair>(*this, other);
      case kImageSetClass:
        return CompareCSSValues<CSSImageSetValue>(*this, other);
      case kCSSContentDistributionClass:
        return CompareCSSValues<CSSContentDistributionValue>(*this, other);
      case kCustomPropertyDeclarationClass:
        return CompareCSSValues<CSSCustomPropertyDeclaration>(*this, other);
      case kVariableReferenceClass:
        return CompareCSSValues<CSSVariableReferenceValue>(*this, other);
      case kPendingSubstitutionValueClass:
        return CompareCSSValues<CSSPendingSubstitutionValue>(*this, other);
    }
    NOTREACHED();
    return false;
  }
  return false;
}

String CSSValue::CssText() const {
  switch (GetClassType()) {
    case kBasicShapeCircleClass:
      return ToCSSBasicShapeCircleValue(this)->CustomCSSText();
    case kBasicShapeEllipseClass:
      return ToCSSBasicShapeEllipseValue(this)->CustomCSSText();
    case kBasicShapePolygonClass:
      return ToCSSBasicShapePolygonValue(this)->CustomCSSText();
    case kBasicShapeInsetClass:
      return ToCSSBasicShapeInsetValue(this)->CustomCSSText();
    case kBorderImageSliceClass:
      return ToCSSBorderImageSliceValue(this)->CustomCSSText();
    case kColorClass:
      return ToCSSColorValue(this)->CustomCSSText();
    case kCounterClass:
      return ToCSSCounterValue(this)->CustomCSSText();
    case kCursorImageClass:
      return ToCSSCursorImageValue(this)->CustomCSSText();
    case kFontFaceSrcClass:
      return ToCSSFontFaceSrcValue(this)->CustomCSSText();
    case kFontFamilyClass:
      return ToCSSFontFamilyValue(this)->CustomCSSText();
    case kFontFeatureClass:
      return ToCSSFontFeatureValue(this)->CustomCSSText();
    case kFontStyleRangeClass:
      return ToCSSFontStyleRangeValue(this)->CustomCSSText();
    case kFontVariationClass:
      return ToCSSFontVariationValue(this)->CustomCSSText();
    case kFunctionClass:
      return ToCSSFunctionValue(this)->CustomCSSText();
    case kLayoutFunctionClass:
      return ToCSSLayoutFunctionValue(this)->CustomCSSText();
    case kLinearGradientClass:
      return ToCSSLinearGradientValue(this)->CustomCSSText();
    case kRadialGradientClass:
      return ToCSSRadialGradientValue(this)->CustomCSSText();
    case kConicGradientClass:
      return ToCSSConicGradientValue(this)->CustomCSSText();
    case kCrossfadeClass:
      return ToCSSCrossfadeValue(this)->CustomCSSText();
    case kPaintClass:
      return ToCSSPaintValue(this)->CustomCSSText();
    case kCustomIdentClass:
      return ToCSSCustomIdentValue(this)->CustomCSSText();
    case kImageClass:
      return ToCSSImageValue(this)->CustomCSSText();
    case kInheritedClass:
      return ToCSSInheritedValue(this)->CustomCSSText();
    case kUnsetClass:
      return ToCSSUnsetValue(this)->CustomCSSText();
    case kInitialClass:
      return ToCSSInitialValue(this)->CustomCSSText();
    case kGridAutoRepeatClass:
      return ToCSSGridAutoRepeatValue(this)->CustomCSSText();
    case kGridLineNamesClass:
      return ToCSSGridLineNamesValue(this)->CustomCSSText();
    case kGridTemplateAreasClass:
      return ToCSSGridTemplateAreasValue(this)->CustomCSSText();
    case kPathClass:
      return ToCSSPathValue(this)->CustomCSSText();
    case kPrimitiveClass:
      return ToCSSPrimitiveValue(this)->CustomCSSText();
    case kRayClass:
      return ToCSSRayValue(this)->CustomCSSText();
    case kIdentifierClass:
      return ToCSSIdentifierValue(this)->CustomCSSText();
    case kQuadClass:
      return ToCSSQuadValue(this)->CustomCSSText();
    case kReflectClass:
      return ToCSSReflectValue(this)->CustomCSSText();
    case kShadowClass:
      return ToCSSShadowValue(this)->CustomCSSText();
    case kStringClass:
      return ToCSSStringValue(this)->CustomCSSText();
    case kCubicBezierTimingFunctionClass:
      return ToCSSCubicBezierTimingFunctionValue(this)->CustomCSSText();
    case kStepsTimingFunctionClass:
      return ToCSSStepsTimingFunctionValue(this)->CustomCSSText();
    case kFramesTimingFunctionClass:
      return ToCSSFramesTimingFunctionValue(this)->CustomCSSText();
    case kUnicodeRangeClass:
      return ToCSSUnicodeRangeValue(this)->CustomCSSText();
    case kURIClass:
      return ToCSSURIValue(this)->CustomCSSText();
    case kValuePairClass:
      return ToCSSValuePair(this)->CustomCSSText();
    case kValueListClass:
      return ToCSSValueList(this)->CustomCSSText();
    case kImageSetClass:
      return ToCSSImageSetValue(this)->CustomCSSText();
    case kCSSContentDistributionClass:
      return ToCSSContentDistributionValue(this)->CustomCSSText();
    case kVariableReferenceClass:
      return ToCSSVariableReferenceValue(this)->CustomCSSText();
    case kCustomPropertyDeclarationClass:
      return ToCSSCustomPropertyDeclaration(this)->CustomCSSText();
    case kPendingSubstitutionValueClass:
      return ToCSSPendingSubstitutionValue(this)->CustomCSSText();
  }
  NOTREACHED();
  return String();
}

void CSSValue::FinalizeGarbageCollectedObject() {
  switch (GetClassType()) {
    case kBasicShapeCircleClass:
      ToCSSBasicShapeCircleValue(this)->~CSSBasicShapeCircleValue();
      return;
    case kBasicShapeEllipseClass:
      ToCSSBasicShapeEllipseValue(this)->~CSSBasicShapeEllipseValue();
      return;
    case kBasicShapePolygonClass:
      ToCSSBasicShapePolygonValue(this)->~CSSBasicShapePolygonValue();
      return;
    case kBasicShapeInsetClass:
      ToCSSBasicShapeInsetValue(this)->~CSSBasicShapeInsetValue();
      return;
    case kBorderImageSliceClass:
      ToCSSBorderImageSliceValue(this)->~CSSBorderImageSliceValue();
      return;
    case kColorClass:
      ToCSSColorValue(this)->~CSSColorValue();
      return;
    case kCounterClass:
      ToCSSCounterValue(this)->~CSSCounterValue();
      return;
    case kCursorImageClass:
      ToCSSCursorImageValue(this)->~CSSCursorImageValue();
      return;
    case kFontFaceSrcClass:
      ToCSSFontFaceSrcValue(this)->~CSSFontFaceSrcValue();
      return;
    case kFontFamilyClass:
      ToCSSFontFamilyValue(this)->~CSSFontFamilyValue();
      return;
    case kFontFeatureClass:
      ToCSSFontFeatureValue(this)->~CSSFontFeatureValue();
      return;
    case kFontStyleRangeClass:
      ToCSSFontStyleRangeValue(this)->~CSSFontStyleRangeValue();
      return;
    case kFontVariationClass:
      ToCSSFontVariationValue(this)->~CSSFontVariationValue();
      return;
    case kFunctionClass:
      ToCSSFunctionValue(this)->~CSSFunctionValue();
      return;
    case kLayoutFunctionClass:
      ToCSSLayoutFunctionValue(this)->~CSSLayoutFunctionValue();
      return;
    case kLinearGradientClass:
      ToCSSLinearGradientValue(this)->~CSSLinearGradientValue();
      return;
    case kRadialGradientClass:
      ToCSSRadialGradientValue(this)->~CSSRadialGradientValue();
      return;
    case kConicGradientClass:
      ToCSSConicGradientValue(this)->~CSSConicGradientValue();
      return;
    case kCrossfadeClass:
      ToCSSCrossfadeValue(this)->~CSSCrossfadeValue();
      return;
    case kPaintClass:
      ToCSSPaintValue(this)->~CSSPaintValue();
      return;
    case kCustomIdentClass:
      ToCSSCustomIdentValue(this)->~CSSCustomIdentValue();
      return;
    case kImageClass:
      ToCSSImageValue(this)->~CSSImageValue();
      return;
    case kInheritedClass:
      ToCSSInheritedValue(this)->~CSSInheritedValue();
      return;
    case kInitialClass:
      ToCSSInitialValue(this)->~CSSInitialValue();
      return;
    case kUnsetClass:
      ToCSSUnsetValue(this)->~CSSUnsetValue();
      return;
    case kGridAutoRepeatClass:
      ToCSSGridAutoRepeatValue(this)->~CSSGridAutoRepeatValue();
      return;
    case kGridLineNamesClass:
      ToCSSGridLineNamesValue(this)->~CSSGridLineNamesValue();
      return;
    case kGridTemplateAreasClass:
      ToCSSGridTemplateAreasValue(this)->~CSSGridTemplateAreasValue();
      return;
    case kPathClass:
      ToCSSPathValue(this)->~CSSPathValue();
      return;
    case kPrimitiveClass:
      ToCSSPrimitiveValue(this)->~CSSPrimitiveValue();
      return;
    case kRayClass:
      ToCSSRayValue(this)->~CSSRayValue();
      return;
    case kIdentifierClass:
      ToCSSIdentifierValue(this)->~CSSIdentifierValue();
      return;
    case kQuadClass:
      ToCSSQuadValue(this)->~CSSQuadValue();
      return;
    case kReflectClass:
      ToCSSReflectValue(this)->~CSSReflectValue();
      return;
    case kShadowClass:
      ToCSSShadowValue(this)->~CSSShadowValue();
      return;
    case kStringClass:
      ToCSSStringValue(this)->~CSSStringValue();
      return;
    case kCubicBezierTimingFunctionClass:
      ToCSSCubicBezierTimingFunctionValue(this)
          ->~CSSCubicBezierTimingFunctionValue();
      return;
    case kStepsTimingFunctionClass:
      ToCSSStepsTimingFunctionValue(this)->~CSSStepsTimingFunctionValue();
      return;
    case kFramesTimingFunctionClass:
      ToCSSFramesTimingFunctionValue(this)->~CSSFramesTimingFunctionValue();
      return;
    case kUnicodeRangeClass:
      ToCSSUnicodeRangeValue(this)->~CSSUnicodeRangeValue();
      return;
    case kURIClass:
      ToCSSURIValue(this)->~CSSURIValue();
      return;
    case kValueListClass:
      ToCSSValueList(this)->~CSSValueList();
      return;
    case kValuePairClass:
      ToCSSValuePair(this)->~CSSValuePair();
      return;
    case kImageSetClass:
      ToCSSImageSetValue(this)->~CSSImageSetValue();
      return;
    case kCSSContentDistributionClass:
      ToCSSContentDistributionValue(this)->~CSSContentDistributionValue();
      return;
    case kVariableReferenceClass:
      ToCSSVariableReferenceValue(this)->~CSSVariableReferenceValue();
      return;
    case kCustomPropertyDeclarationClass:
      ToCSSCustomPropertyDeclaration(this)->~CSSCustomPropertyDeclaration();
      return;
    case kPendingSubstitutionValueClass:
      ToCSSPendingSubstitutionValue(this)->~CSSPendingSubstitutionValue();
      return;
  }
  NOTREACHED();
}

void CSSValue::Trace(blink::Visitor* visitor) {
  switch (GetClassType()) {
    case kBasicShapeCircleClass:
      ToCSSBasicShapeCircleValue(this)->TraceAfterDispatch(visitor);
      return;
    case kBasicShapeEllipseClass:
      ToCSSBasicShapeEllipseValue(this)->TraceAfterDispatch(visitor);
      return;
    case kBasicShapePolygonClass:
      ToCSSBasicShapePolygonValue(this)->TraceAfterDispatch(visitor);
      return;
    case kBasicShapeInsetClass:
      ToCSSBasicShapeInsetValue(this)->TraceAfterDispatch(visitor);
      return;
    case kBorderImageSliceClass:
      ToCSSBorderImageSliceValue(this)->TraceAfterDispatch(visitor);
      return;
    case kColorClass:
      ToCSSColorValue(this)->TraceAfterDispatch(visitor);
      return;
    case kCounterClass:
      ToCSSCounterValue(this)->TraceAfterDispatch(visitor);
      return;
    case kCursorImageClass:
      ToCSSCursorImageValue(this)->TraceAfterDispatch(visitor);
      return;
    case kFontFaceSrcClass:
      ToCSSFontFaceSrcValue(this)->TraceAfterDispatch(visitor);
      return;
    case kFontFamilyClass:
      ToCSSFontFamilyValue(this)->TraceAfterDispatch(visitor);
      return;
    case kFontFeatureClass:
      ToCSSFontFeatureValue(this)->TraceAfterDispatch(visitor);
      return;
    case kFontStyleRangeClass:
      ToCSSFontStyleRangeValue(this)->TraceAfterDispatch(visitor);
      return;
    case kFontVariationClass:
      ToCSSFontVariationValue(this)->TraceAfterDispatch(visitor);
      return;
    case kFunctionClass:
      ToCSSFunctionValue(this)->TraceAfterDispatch(visitor);
      return;
    case kLayoutFunctionClass:
      ToCSSLayoutFunctionValue(this)->TraceAfterDispatch(visitor);
      return;
    case kLinearGradientClass:
      ToCSSLinearGradientValue(this)->TraceAfterDispatch(visitor);
      return;
    case kRadialGradientClass:
      ToCSSRadialGradientValue(this)->TraceAfterDispatch(visitor);
      return;
    case kConicGradientClass:
      ToCSSConicGradientValue(this)->TraceAfterDispatch(visitor);
      return;
    case kCrossfadeClass:
      ToCSSCrossfadeValue(this)->TraceAfterDispatch(visitor);
      return;
    case kPaintClass:
      ToCSSPaintValue(this)->TraceAfterDispatch(visitor);
      return;
    case kCustomIdentClass:
      ToCSSCustomIdentValue(this)->TraceAfterDispatch(visitor);
      return;
    case kImageClass:
      ToCSSImageValue(this)->TraceAfterDispatch(visitor);
      return;
    case kInheritedClass:
      ToCSSInheritedValue(this)->TraceAfterDispatch(visitor);
      return;
    case kInitialClass:
      ToCSSInitialValue(this)->TraceAfterDispatch(visitor);
      return;
    case kUnsetClass:
      ToCSSUnsetValue(this)->TraceAfterDispatch(visitor);
      return;
    case kGridAutoRepeatClass:
      ToCSSGridAutoRepeatValue(this)->TraceAfterDispatch(visitor);
      return;
    case kGridLineNamesClass:
      ToCSSGridLineNamesValue(this)->TraceAfterDispatch(visitor);
      return;
    case kGridTemplateAreasClass:
      ToCSSGridTemplateAreasValue(this)->TraceAfterDispatch(visitor);
      return;
    case kPathClass:
      ToCSSPathValue(this)->TraceAfterDispatch(visitor);
      return;
    case kPrimitiveClass:
      ToCSSPrimitiveValue(this)->TraceAfterDispatch(visitor);
      return;
    case kRayClass:
      ToCSSRayValue(this)->TraceAfterDispatch(visitor);
      return;
    case kIdentifierClass:
      ToCSSIdentifierValue(this)->TraceAfterDispatch(visitor);
      return;
    case kQuadClass:
      ToCSSQuadValue(this)->TraceAfterDispatch(visitor);
      return;
    case kReflectClass:
      ToCSSReflectValue(this)->TraceAfterDispatch(visitor);
      return;
    case kShadowClass:
      ToCSSShadowValue(this)->TraceAfterDispatch(visitor);
      return;
    case kStringClass:
      ToCSSStringValue(this)->TraceAfterDispatch(visitor);
      return;
    case kCubicBezierTimingFunctionClass:
      ToCSSCubicBezierTimingFunctionValue(this)->TraceAfterDispatch(visitor);
      return;
    case kStepsTimingFunctionClass:
      ToCSSStepsTimingFunctionValue(this)->TraceAfterDispatch(visitor);
      return;
    case kFramesTimingFunctionClass:
      ToCSSFramesTimingFunctionValue(this)->TraceAfterDispatch(visitor);
      return;
    case kUnicodeRangeClass:
      ToCSSUnicodeRangeValue(this)->TraceAfterDispatch(visitor);
      return;
    case kURIClass:
      ToCSSURIValue(this)->TraceAfterDispatch(visitor);
      return;
    case kValueListClass:
      ToCSSValueList(this)->TraceAfterDispatch(visitor);
      return;
    case kValuePairClass:
      ToCSSValuePair(this)->TraceAfterDispatch(visitor);
      return;
    case kImageSetClass:
      ToCSSImageSetValue(this)->TraceAfterDispatch(visitor);
      return;
    case kCSSContentDistributionClass:
      ToCSSContentDistributionValue(this)->TraceAfterDispatch(visitor);
      return;
    case kVariableReferenceClass:
      ToCSSVariableReferenceValue(this)->TraceAfterDispatch(visitor);
      return;
    case kCustomPropertyDeclarationClass:
      ToCSSCustomPropertyDeclaration(this)->TraceAfterDispatch(visitor);
      return;
    case kPendingSubstitutionValueClass:
      ToCSSPendingSubstitutionValue(this)->TraceAfterDispatch(visitor);
      return;
  }
  NOTREACHED();
}

}  // namespace blink
