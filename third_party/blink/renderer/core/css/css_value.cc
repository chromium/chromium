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

#include "third_party/blink/renderer/core/css/css_alternate_value.h"
#include "third_party/blink/renderer/core/css/css_appearance_auto_base_select_value_pair.h"
#include "third_party/blink/renderer/core/css/css_attr_value_tainting.h"
#include "third_party/blink/renderer/core/css/css_axis_value.h"
#include "third_party/blink/renderer/core/css/css_basic_shape_values.h"
#include "third_party/blink/renderer/core/css/css_border_image_slice_value.h"
#include "third_party/blink/renderer/core/css/css_bracketed_value_list.h"
#include "third_party/blink/renderer/core/css/css_color.h"
#include "third_party/blink/renderer/core/css/css_color_mix_value.h"
#include "third_party/blink/renderer/core/css/css_content_distribution_value.h"
#include "third_party/blink/renderer/core/css/css_counter_value.h"
#include "third_party/blink/renderer/core/css/css_crossfade_value.h"
#include "third_party/blink/renderer/core/css/css_cursor_image_value.h"
#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"
#include "third_party/blink/renderer/core/css/css_cyclic_variable_value.h"
#include "third_party/blink/renderer/core/css/css_dynamic_range_limit_mix_value.h"
#include "third_party/blink/renderer/core/css/css_flip_revert_value.h"
#include "third_party/blink/renderer/core/css/css_font_face_src_value.h"
#include "third_party/blink/renderer/core/css/css_font_family_value.h"
#include "third_party/blink/renderer/core/css/css_font_feature_value.h"
#include "third_party/blink/renderer/core/css/css_font_style_range_value.h"
#include "third_party/blink/renderer/core/css/css_font_variation_value.h"
#include "third_party/blink/renderer/core/css/css_function_value.h"
#include "third_party/blink/renderer/core/css/css_gradient_value.h"
#include "third_party/blink/renderer/core/css/css_grid_auto_repeat_value.h"
#include "third_party/blink/renderer/core/css/css_grid_integer_repeat_value.h"
#include "third_party/blink/renderer/core/css/css_grid_template_areas_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_image_set_option_value.h"
#include "third_party/blink/renderer/core/css/css_image_set_type_value.h"
#include "third_party/blink/renderer/core/css/css_image_set_value.h"
#include "third_party/blink/renderer/core/css/css_image_value.h"
#include "third_party/blink/renderer/core/css/css_inherited_value.h"
#include "third_party/blink/renderer/core/css/css_initial_color_value.h"
#include "third_party/blink/renderer/core/css/css_initial_value.h"
#include "third_party/blink/renderer/core/css/css_invalid_variable_value.h"
#include "third_party/blink/renderer/core/css/css_keyframe_shorthand_value.h"
#include "third_party/blink/renderer/core/css/css_layout_function_value.h"
#include "third_party/blink/renderer/core/css/css_light_dark_value_pair.h"
#include "third_party/blink/renderer/core/css/css_math_function_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_paint_value.h"
#include "third_party/blink/renderer/core/css/css_palette_mix_value.h"
#include "third_party/blink/renderer/core/css/css_path_value.h"
#include "third_party/blink/renderer/core/css/css_pending_substitution_value.h"
#include "third_party/blink/renderer/core/css/css_pending_system_font_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_quad_value.h"
#include "third_party/blink/renderer/core/css/css_ratio_value.h"
#include "third_party/blink/renderer/core/css/css_ray_value.h"
#include "third_party/blink/renderer/core/css/css_reflect_value.h"
#include "third_party/blink/renderer/core/css/css_relative_color_value.h"
#include "third_party/blink/renderer/core/css/css_repeat_style_value.h"
#include "third_party/blink/renderer/core/css/css_repeat_value.h"
#include "third_party/blink/renderer/core/css/css_revert_layer_value.h"
#include "third_party/blink/renderer/core/css/css_revert_value.h"
#include "third_party/blink/renderer/core/css/css_scoped_keyword_value.h"
#include "third_party/blink/renderer/core/css/css_scroll_value.h"
#include "third_party/blink/renderer/core/css/css_shadow_value.h"
#include "third_party/blink/renderer/core/css/css_string_value.h"
#include "third_party/blink/renderer/core/css/css_timing_function_value.h"
#include "third_party/blink/renderer/core/css/css_unicode_range_value.h"
#include "third_party/blink/renderer/core/css/css_unparsed_declaration_value.h"
#include "third_party/blink/renderer/core/css/css_unset_value.h"
#include "third_party/blink/renderer/core/css/css_uri_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/css_value_pair.h"
#include "third_party/blink/renderer/core/css/css_view_value.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"

namespace blink {

struct SameSizeAsCSSValue final : public GarbageCollected<SameSizeAsCSSValue> {
  char bitfields[sizeof(uint32_t)];
};
ASSERT_SIZE(CSSValue, SameSizeAsCSSValue);

CSSValue* CSSValue::Create(const Length& value, float zoom) {
  switch (value.GetType()) {
    case Length::kAuto:
    case Length::kMinContent:
    case Length::kMaxContent:
    case Length::kStretch:
    case Length::kFitContent:
    case Length::kContent:
    case Length::kExtendToZoom:
      return CSSIdentifierValue::Create(value);
    case Length::kPercent:
    case Length::kFixed:
    case Length::kCalculated:
    case Length::kFlex:
      return CSSPrimitiveValue::CreateFromLength(value, zoom);
    case Length::kDeviceWidth:
    case Length::kDeviceHeight:
    case Length::kMinIntrinsic:
    case Length::kNone:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  return nullptr;
}

bool CSSValue::HasFailedOrCanceledSubresources() const {
  if (IsValueList()) {
    return To<CSSValueList>(this)->HasFailedOrCanceledSubresources();
  }
  if (GetClassType() == kFontFaceSrcClass) {
    return To<CSSFontFaceSrcValue>(this)->HasFailedOrCanceledSubresources();
  }
  if (GetClassType() == kImageClass) {
    return To<CSSImageValue>(this)->HasFailedOrCanceledSubresources();
  }
  if (GetClassType() == kCrossfadeClass) {
    return To<cssvalue::CSSCrossfadeValue>(this)
        ->HasFailedOrCanceledSubresources();
  }
  if (GetClassType() == kImageSetClass) {
    return To<CSSImageSetValue>(this)->HasFailedOrCanceledSubresources();
  }

  return false;
}

bool CSSValue::MayContainUrl() const {
  if (IsValueList()) {
    return To<CSSValueList>(*this).MayContainUrl();
  }
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
  if (attr_tainted_ != other.attr_tainted_) {
    return false;
  }
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
      case kBasicShapeRectClass:
        return CompareCSSValues<cssvalue::CSSBasicShapeRectValue>(*this, other);
      case kBasicShapeXYWHClass:
        return CompareCSSValues<cssvalue::CSSBasicShapeXYWHValue>(*this, other);
      case kBorderImageSliceClass:
        return CompareCSSValues<cssvalue::CSSBorderImageSliceValue>(*this,
                                                                    other);
      case kColorClass:
        return CompareCSSValues<cssvalue::CSSColor>(*this, other);
      case kColorMixClass:
        return CompareCSSValues<cssvalue::CSSColorMixValue>(*this, other);
      case kCounterClass:
        return CompareCSSValues<cssvalue::CSSCounterValue>(*this, other);
      case kCursorImageClass:
        return CompareCSSValues<cssvalue::CSSCursorImageValue>(*this, other);
      case kDynamicRangeLimitMixClass:
        return CompareCSSValues<cssvalue::CSSDynamicRangeLimitMixValue>(*this,
                                                                        other);
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
      case kAlternateClass:
        return CompareCSSValues<cssvalue::CSSAlternateValue>(*this, other);
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
      case kConstantGradientClass:
        return CompareCSSValues<cssvalue::CSSConstantGradientValue>(*this,
                                                                    other);
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
      case kRevertClass:
        return CompareCSSValues<cssvalue::CSSRevertValue>(*this, other);
      case kRevertLayerClass:
        return CompareCSSValues<cssvalue::CSSRevertLayerValue>(*this, other);
      case kGridAutoRepeatClass:
        return CompareCSSValues<cssvalue::CSSGridAutoRepeatValue>(*this, other);
      case kGridIntegerRepeatClass:
        return CompareCSSValues<cssvalue::CSSGridIntegerRepeatValue>(*this,
                                                                     other);
      case kGridLineNamesClass:
        return CompareCSSValues<cssvalue::CSSBracketedValueList>(*this, other);
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
      case kScopedKeywordClass:
        return CompareCSSValues<cssvalue::CSSScopedKeywordValue>(*this, other);
      case kKeyframeShorthandClass:
        return CompareCSSValues<CSSKeyframeShorthandValue>(*this, other);
      case kInitialColorValueClass:
        return CompareCSSValues<CSSInitialColorValue>(*this, other);
      case kQuadClass:
        return CompareCSSValues<CSSQuadValue>(*this, other);
      case kReflectClass:
        return CompareCSSValues<cssvalue::CSSReflectValue>(*this, other);
      case kShadowClass:
        return CompareCSSValues<CSSShadowValue>(*this, other);
      case kStringClass:
        return CompareCSSValues<CSSStringValue>(*this, other);
      case kLinearTimingFunctionClass:
        return CompareCSSValues<cssvalue::CSSLinearTimingFunctionValue>(*this,
                                                                        other);
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
      case kImageSetTypeClass:
        return CompareCSSValues<CSSImageSetTypeValue>(*this, other);
      case kImageSetOptionClass:
        return CompareCSSValues<CSSImageSetOptionValue>(*this, other);
      case kImageSetClass:
        return CompareCSSValues<CSSImageSetValue>(*this, other);
      case kCSSContentDistributionClass:
        return CompareCSSValues<cssvalue::CSSContentDistributionValue>(*this,
                                                                       other);
      case kUnparsedDeclarationClass:
        return CompareCSSValues<CSSUnparsedDeclarationValue>(*this, other);
      case kPendingSubstitutionValueClass:
        return CompareCSSValues<cssvalue::CSSPendingSubstitutionValue>(*this,
                                                                       other);
      case kPendingSystemFontValueClass:
        return CompareCSSValues<cssvalue::CSSPendingSystemFontValue>(*this,
                                                                     other);
      case kInvalidVariableValueClass:
        return CompareCSSValues<CSSInvalidVariableValue>(*this, other);
      case kCyclicVariableValueClass:
        return CompareCSSValues<CSSCyclicVariableValue>(*this, other);
      case kFlipRevertClass:
        return CompareCSSValues<cssvalue::CSSFlipRevertValue>(*this, other);
      case kLightDarkValuePairClass:
        return CompareCSSValues<CSSLightDarkValuePair>(*this, other);
      case kAppearanceAutoBaseSelectValuePairClass:
        return CompareCSSValues<CSSAppearanceAutoBaseSelectValuePair>(*this,
                                                                      other);
      case kScrollClass:
        return CompareCSSValues<cssvalue::CSSScrollValue>(*this, other);
      case kViewClass:
        return CompareCSSValues<cssvalue::CSSViewValue>(*this, other);
      case kRatioClass:
        return CompareCSSValues<cssvalue::CSSRatioValue>(*this, other);
      case kPaletteMixClass:
        return CompareCSSValues<cssvalue::CSSPaletteMixValue>(*this, other);
      case kRepeatStyleClass:
        return CompareCSSValues<CSSRepeatStyleValue>(*this, other);
      case kRelativeColorClass:
        return CompareCSSValues<cssvalue::CSSRelativeColorValue>(*this, other);
      case kRepeatClass:
        return CompareCSSValues<cssvalue::CSSRepeatValue>(*this, other);
    }
    NOTREACHED_IN_MIGRATION();
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
    case kBasicShapeRectClass:
      return To<cssvalue::CSSBasicShapeRectValue>(this)->CustomCSSText();
    case kBasicShapeXYWHClass:
      return To<cssvalue::CSSBasicShapeXYWHValue>(this)->CustomCSSText();
    case kBorderImageSliceClass:
      return To<cssvalue::CSSBorderImageSliceValue>(this)->CustomCSSText();
    case kColorClass:
      return To<cssvalue::CSSColor>(this)->CustomCSSText();
    case kColorMixClass:
      return To<cssvalue::CSSColorMixValue>(this)->CustomCSSText();
    case kCounterClass:
      return To<cssvalue::CSSCounterValue>(this)->CustomCSSText();
    case kCursorImageClass:
      return To<cssvalue::CSSCursorImageValue>(this)->CustomCSSText();
    case kDynamicRangeLimitMixClass:
      return To<cssvalue::CSSDynamicRangeLimitMixValue>(this)->CustomCSSText();
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
    case kAlternateClass:
      return To<cssvalue::CSSAlternateValue>(this)->CustomCSSText();
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
    case kConstantGradientClass:
      return To<cssvalue::CSSConstantGradientValue>(this)->CustomCSSText();
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
    case kRevertClass:
      return To<cssvalue::CSSRevertValue>(this)->CustomCSSText();
    case kRevertLayerClass:
      return To<cssvalue::CSSRevertLayerValue>(this)->CustomCSSText();
    case kInitialClass:
      return To<CSSInitialValue>(this)->CustomCSSText();
    case kGridAutoRepeatClass:
      return To<cssvalue::CSSGridAutoRepeatValue>(this)->CustomCSSText();
    case kGridIntegerRepeatClass:
      return To<cssvalue::CSSGridIntegerRepeatValue>(this)->CustomCSSText();
    case kGridLineNamesClass:
      return To<cssvalue::CSSBracketedValueList>(this)->CustomCSSText();
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
    case kScopedKeywordClass:
      return To<cssvalue::CSSScopedKeywordValue>(this)->CustomCSSText();
    case kKeyframeShorthandClass:
      return To<CSSKeyframeShorthandValue>(this)->CustomCSSText();
    case kInitialColorValueClass:
      return To<CSSInitialColorValue>(this)->CustomCSSText();
    case kQuadClass:
      return To<CSSQuadValue>(this)->CustomCSSText();
    case kReflectClass:
      return To<cssvalue::CSSReflectValue>(this)->CustomCSSText();
    case kShadowClass:
      return To<CSSShadowValue>(this)->CustomCSSText();
    case kStringClass:
      return To<CSSStringValue>(this)->CustomCSSText();
    case kLinearTimingFunctionClass:
      return To<cssvalue::CSSLinearTimingFunctionValue>(this)->CustomCSSText();
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
    case kImageSetTypeClass:
      return To<CSSImageSetTypeValue>(this)->CustomCSSText();
    case kImageSetOptionClass:
      return To<CSSImageSetOptionValue>(this)->CustomCSSText();
    case kImageSetClass:
      return To<CSSImageSetValue>(this)->CustomCSSText();
    case kCSSContentDistributionClass:
      return To<cssvalue::CSSContentDistributionValue>(this)->CustomCSSText();
    case kUnparsedDeclarationClass:
      return To<CSSUnparsedDeclarationValue>(this)->CustomCSSText();
    case kPendingSubstitutionValueClass:
      return To<cssvalue::CSSPendingSubstitutionValue>(this)->CustomCSSText();
    case kPendingSystemFontValueClass:
      return To<cssvalue::CSSPendingSystemFontValue>(this)->CustomCSSText();
    case kInvalidVariableValueClass:
      return To<CSSInvalidVariableValue>(this)->CustomCSSText();
    case kCyclicVariableValueClass:
      return To<CSSCyclicVariableValue>(this)->CustomCSSText();
    case kFlipRevertClass:
      return To<cssvalue::CSSFlipRevertValue>(this)->CustomCSSText();
    case kLightDarkValuePairClass:
      return To<CSSLightDarkValuePair>(this)->CustomCSSText();
    case kAppearanceAutoBaseSelectValuePairClass:
      return To<CSSAppearanceAutoBaseSelectValuePair>(this)->CustomCSSText();
    case kScrollClass:
      return To<cssvalue::CSSScrollValue>(this)->CustomCSSText();
    case kViewClass:
      return To<cssvalue::CSSViewValue>(this)->CustomCSSText();
    case kRatioClass:
      return To<cssvalue::CSSRatioValue>(this)->CustomCSSText();
    case kPaletteMixClass:
      return To<cssvalue::CSSPaletteMixValue>(this)->CustomCSSText();
    case kRepeatStyleClass:
      return To<CSSRepeatStyleValue>(this)->CustomCSSText();
    case kRelativeColorClass:
      return To<cssvalue::CSSRelativeColorValue>(this)->CustomCSSText();
    case kRepeatClass:
      return To<cssvalue::CSSRepeatValue>(this)->CustomCSSText();
  }
  NOTREACHED_IN_MIGRATION();
  return String();
}

const CSSValue* CSSValue::UntaintedCopy() const {
  if (const auto* v = DynamicTo<CSSValueList>(this)) {
    return v->UntaintedCopy();
  }
  if (const auto* v = DynamicTo<CSSStringValue>(this)) {
    return v->UntaintedCopy();
  }
  return this;
}

const CSSValue& CSSValue::PopulateWithTreeScope(
    const TreeScope* tree_scope) const {
  switch (GetClassType()) {
    case kScopedKeywordClass:
      return To<cssvalue::CSSScopedKeywordValue>(this)->PopulateWithTreeScope(
          tree_scope);
    case kCounterClass:
      return To<cssvalue::CSSCounterValue>(this)->PopulateWithTreeScope(
          tree_scope);
    case kCustomIdentClass:
      return To<CSSCustomIdentValue>(this)->PopulateWithTreeScope(tree_scope);
    case kMathFunctionClass:
      return To<CSSMathFunctionValue>(this)->PopulateWithTreeScope(tree_scope);
    case kValueListClass:
      return To<CSSValueList>(this)->PopulateWithTreeScope(tree_scope);
    default:
      NOTREACHED_IN_MIGRATION();
      return *this;
  }
}

void CSSValue::Trace(Visitor* visitor) const {
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
    case kBasicShapeRectClass:
      To<cssvalue::CSSBasicShapeRectValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kBasicShapeXYWHClass:
      To<cssvalue::CSSBasicShapeXYWHValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kBorderImageSliceClass:
      To<cssvalue::CSSBorderImageSliceValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kColorClass:
      To<cssvalue::CSSColor>(this)->TraceAfterDispatch(visitor);
      return;
    case kColorMixClass:
      To<cssvalue::CSSColorMixValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kCounterClass:
      To<cssvalue::CSSCounterValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kCursorImageClass:
      To<cssvalue::CSSCursorImageValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kDynamicRangeLimitMixClass:
      To<cssvalue::CSSDynamicRangeLimitMixValue>(this)->TraceAfterDispatch(
          visitor);
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
    case kAlternateClass:
      To<cssvalue::CSSAlternateValue>(this)->TraceAfterDispatch(visitor);
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
    case kConstantGradientClass:
      To<cssvalue::CSSConstantGradientValue>(this)->TraceAfterDispatch(visitor);
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
    case kRevertClass:
      To<cssvalue::CSSRevertValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kRevertLayerClass:
      To<cssvalue::CSSRevertLayerValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kGridAutoRepeatClass:
      To<cssvalue::CSSGridAutoRepeatValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kGridIntegerRepeatClass:
      To<cssvalue::CSSGridIntegerRepeatValue>(this)->TraceAfterDispatch(
          visitor);
      return;
    case kGridLineNamesClass:
      To<cssvalue::CSSBracketedValueList>(this)->TraceAfterDispatch(visitor);
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
    case kScopedKeywordClass:
      To<cssvalue::CSSScopedKeywordValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kKeyframeShorthandClass:
      To<CSSKeyframeShorthandValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kInitialColorValueClass:
      To<CSSInitialColorValue>(this)->TraceAfterDispatch(visitor);
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
    case kLinearTimingFunctionClass:
      To<cssvalue::CSSLinearTimingFunctionValue>(this)->TraceAfterDispatch(
          visitor);
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
    case kImageSetTypeClass:
      To<CSSImageSetTypeValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kImageSetOptionClass:
      To<CSSImageSetOptionValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kImageSetClass:
      To<CSSImageSetValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kCSSContentDistributionClass:
      To<cssvalue::CSSContentDistributionValue>(this)->TraceAfterDispatch(
          visitor);
      return;
    case kUnparsedDeclarationClass:
      To<CSSUnparsedDeclarationValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kPendingSubstitutionValueClass:
      To<cssvalue::CSSPendingSubstitutionValue>(this)->TraceAfterDispatch(
          visitor);
      return;
    case kPendingSystemFontValueClass:
      To<cssvalue::CSSPendingSystemFontValue>(this)->TraceAfterDispatch(
          visitor);
      return;
    case kInvalidVariableValueClass:
      To<CSSInvalidVariableValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kCyclicVariableValueClass:
      To<CSSCyclicVariableValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kFlipRevertClass:
      To<cssvalue::CSSFlipRevertValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kLightDarkValuePairClass:
      To<CSSLightDarkValuePair>(this)->TraceAfterDispatch(visitor);
      return;
    case kAppearanceAutoBaseSelectValuePairClass:
      To<CSSAppearanceAutoBaseSelectValuePair>(this)->TraceAfterDispatch(
          visitor);
      return;
    case kScrollClass:
      To<cssvalue::CSSScrollValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kViewClass:
      To<cssvalue::CSSViewValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kRatioClass:
      To<cssvalue::CSSRatioValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kPaletteMixClass:
      To<cssvalue::CSSPaletteMixValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kRepeatStyleClass:
      To<CSSRepeatStyleValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kRelativeColorClass:
      To<cssvalue::CSSRelativeColorValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kRepeatClass:
      To<cssvalue::CSSRepeatValue>(this)->TraceAfterDispatch(visitor);
      return;
  }
  NOTREACHED_IN_MIGRATION();
}

#if DCHECK_IS_ON()
String CSSValue::ClassTypeToString() const {
  switch (GetClassType()) {
    case kNumericLiteralClass:
      return "NumericLiteralClass";
    case kMathFunctionClass:
      return "MathFunctionClass";
    case kIdentifierClass:
      return "IdentifierClass";
    case kScopedKeywordClass:
      return "ScopedKeywordClass";
    case kColorClass:
      return "ColorClass";
    case kColorMixClass:
      return "ColorMixClass";
    case kCounterClass:
      return "CounterClass";
    case kQuadClass:
      return "QuadClass";
    case kCustomIdentClass:
      return "CustomIdentClass";
    case kStringClass:
      return "StringClass";
    case kURIClass:
      return "URIClass";
    case kValuePairClass:
      return "ValuePairClass";
    case kLightDarkValuePairClass:
      return "LightDarkValuePairClass";
    case kAppearanceAutoBaseSelectValuePairClass:
      return "AppearanceAutoBaseSelectValuePairClass";
    case kScrollClass:
      return "ScrollClass";
    case kViewClass:
      return "ViewClass";
    case kRatioClass:
      return "RatioClass";
    case kBasicShapeCircleClass:
      return "BasicShapeCircleClass";
    case kBasicShapeEllipseClass:
      return "BasicShapeEllipseClass";
    case kBasicShapePolygonClass:
      return "BasicShapePolygonClass";
    case kBasicShapeInsetClass:
      return "BasicShapeInsetClass";
    case kBasicShapeRectClass:
      return "BasicShapeRectClass";
    case kBasicShapeXYWHClass:
      return "BasicShapeXYWHClass";
    case kImageClass:
      return "ImageClass";
    case kCursorImageClass:
      return "CursorImageClass";
    case kCrossfadeClass:
      return "CrossfadeClass";
    case kPaintClass:
      return "PaintClass";
    case kLinearGradientClass:
      return "LinearGradientClass";
    case kRadialGradientClass:
      return "RadialGradientClass";
    case kConicGradientClass:
      return "ConicGradientClass";
    case kConstantGradientClass:
      return "ConstantGradientClass";
    case kLinearTimingFunctionClass:
      return "LinearTimingFunctionClass";
    case kCubicBezierTimingFunctionClass:
      return "CubicBezierTimingFunctionClass";
    case kStepsTimingFunctionClass:
      return "StepsTimingFunctionClass";
    case kBorderImageSliceClass:
      return "BorderImageSliceClass";
    case kFontFeatureClass:
      return "FontFeatureClass";
    case kFontFaceSrcClass:
      return "FontFaceSrcClass";
    case kFontFamilyClass:
      return "FontFamilyClass";
    case kFontStyleRangeClass:
      return "FontStyleRangeClass";
    case kFontVariationClass:
      return "FontVariationClass";
    case kAlternateClass:
      return "AlternateClass";
    case kInheritedClass:
      return "InheritedClass";
    case kInitialClass:
      return "InitialClass";
    case kUnsetClass:
      return "UnsetClass";
    case kRevertClass:
      return "RevertClass";
    case kRevertLayerClass:
      return "RevertLayerClass";
    case kReflectClass:
      return "ReflectClass";
    case kShadowClass:
      return "ShadowClass";
    case kUnicodeRangeClass:
      return "UnicodeRangeClass";
    case kGridTemplateAreasClass:
      return "GridTemplateAreasClass";
    case kPathClass:
      return "PathClass";
    case kRayClass:
      return "RayClass";
    case kUnparsedDeclarationClass:
      return "UnparsedDeclarationClass";
    case kPendingSubstitutionValueClass:
      return "PendingSubstitutionValueClass";
    case kPendingSystemFontValueClass:
      return "PendingSystemFontValueClass";
    case kInvalidVariableValueClass:
      return "InvalidVariableValueClass";
    case kCyclicVariableValueClass:
      return "CyclicVariableValueClass";
    case kFlipRevertClass:
      return "FlipRevertClass";
    case kLayoutFunctionClass:
      return "LayoutFunctionClass";
    case kCSSContentDistributionClass:
      return "CSSContentDistributionClass";
    case kKeyframeShorthandClass:
      return "KeyframeShorthandClass";
    case kInitialColorValueClass:
      return "InitialColorValueClass";
    case kImageSetOptionClass:
      return "ImageSetOptionClass";
    case kImageSetTypeClass:
      return "ImageSetTypeClass";
    case kValueListClass:
      return "ValueListClass";
    case kFunctionClass:
      return "FunctionClass";
    case kImageSetClass:
      return "ImageSetClass";
    case kGridLineNamesClass:
      return "GridLineNamesClass";
    case kGridAutoRepeatClass:
      return "GridAutoRepeatClass";
    case kGridIntegerRepeatClass:
      return "GridIntegerRepeatClass";
    case kRepeatClass:
      return "RepeatClass";
    case kAxisClass:
      return "AxisClass";
    default:
      NOTREACHED_IN_MIGRATION();
      return "Unknown ClassType";
  }
}
#endif

}  // namespace blink
