// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PROPERTIES_COMPUTED_STYLE_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PROPERTIES_COMPUTED_STYLE_UTILS_H_

#include "cc/input/scroll_snap_data.h"
#include "third_party/blink/renderer/core/css/css_border_image_slice_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/css_value_pair.h"
#include "third_party/blink/renderer/core/css/zoom_adjusted_pixel_value.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class CSSNumericLiteralValue;
class CSSStyleValue;
class CSSValue;
class ComputedStyle;
class StyleColor;
class StylePropertyShorthand;

class ComputedStyleUtils {
  STATIC_ONLY(ComputedStyleUtils);

 public:
  inline static CSSValue* ZoomAdjustedPixelValueOrAuto(
      const Length& length,
      const ComputedStyle& style) {
    if (length.IsAuto())
      return CSSIdentifierValue::Create(CSSValueID::kAuto);
    return ZoomAdjustedPixelValue(length.Value(), style);
  }

  static CSSValue* CurrentColorOrValidColor(const ComputedStyle&,
                                            const StyleColor&);
  static const blink::Color BorderSideColor(const ComputedStyle&,
                                            const StyleColor&,
                                            EBorderStyle,
                                            bool visited_link);
  static CSSValue* ZoomAdjustedPixelValueForLength(const Length&,
                                                   const ComputedStyle&);
  static const CSSValue* BackgroundImageOrWebkitMaskImage(
      const ComputedStyle&,
      bool allow_visited_style,
      const FillLayer&);
  static const CSSValue* ValueForFillSize(const FillSize&,
                                          const ComputedStyle&);
  static const CSSValue* BackgroundImageOrWebkitMaskSize(const ComputedStyle&,
                                                         const FillLayer&);
  static const CSSValueList* CreatePositionListForLayer(const CSSProperty&,
                                                        const FillLayer&,
                                                        const ComputedStyle&);
  static const CSSValue* ValueForFillRepeat(EFillRepeat x_repeat,
                                            EFillRepeat y_repeat);
  static const CSSValueList* ValuesForBackgroundShorthand(
      const ComputedStyle&,
      const LayoutObject*,
      bool allow_visited_style);
  static const CSSValue* BackgroundRepeatOrWebkitMaskRepeat(const FillLayer*);
  static const CSSValue* BackgroundPositionOrWebkitMaskPosition(
      const CSSProperty&,
      const ComputedStyle&,
      const FillLayer*);
  static const CSSValue* BackgroundPositionXOrWebkitMaskPositionX(
      const ComputedStyle&,
      const FillLayer*);
  static const CSSValue* BackgroundPositionYOrWebkitMaskPositionY(
      const ComputedStyle&,
      const FillLayer*);
  static cssvalue::CSSBorderImageSliceValue* ValueForNinePieceImageSlice(
      const NinePieceImage&);
  static CSSQuadValue* ValueForNinePieceImageQuad(const BorderImageLengthBox&,
                                                  const ComputedStyle&);
  static CSSValue* ValueForNinePieceImageRepeat(const NinePieceImage&);
  static CSSValue* ValueForNinePieceImage(const NinePieceImage&,
                                          const ComputedStyle&,
                                          bool allow_visited_style);
  static CSSValue* ValueForReflection(const StyleReflection*,
                                      const ComputedStyle&,
                                      bool allow_visited_style);
  static CSSValue* ValueForPosition(const LengthPoint& position,
                                    const ComputedStyle&);

  static CSSValue* ValueForOffset(const ComputedStyle&,
                                  const LayoutObject*,
                                  bool allow_visited_style);
  static CSSValue* MinWidthOrMinHeightAuto(const ComputedStyle&);
  static CSSValue* ValueForPositionOffset(const ComputedStyle&,
                                          const CSSProperty&,
                                          const LayoutObject*);
  static CSSValueList* ValueForItemPositionWithOverflowAlignment(
      const StyleSelfAlignmentData&);
  static CSSValueList*
  ValueForContentPositionAndDistributionWithOverflowAlignment(
      const StyleContentAlignmentData&);
  static CSSValue* ValueForLineHeight(const ComputedStyle&);
  static CSSValueList* ValueForFontFamily(const ComputedStyle&);
  static CSSPrimitiveValue* ValueForFontSize(const ComputedStyle&);
  static CSSPrimitiveValue* ValueForFontStretch(const ComputedStyle&);
  static CSSValue* ValueForFontStyle(const ComputedStyle&);
  static CSSNumericLiteralValue* ValueForFontWeight(const ComputedStyle&);
  static CSSIdentifierValue* ValueForFontVariantCaps(const ComputedStyle&);
  static CSSValue* ValueForFontVariantLigatures(const ComputedStyle&);
  static CSSValue* ValueForFontVariantNumeric(const ComputedStyle&);
  static CSSValue* ValueForFont(const ComputedStyle&);
  static CSSValue* ValueForFontVariantEastAsian(const ComputedStyle&);
  static CSSValue* SpecifiedValueForGridTrackSize(const GridTrackSize&,
                                                  const ComputedStyle&);
  static CSSValue* ValueForGridTrackSizeList(GridTrackSizingDirection,
                                             const ComputedStyle&);
  static CSSValue* ValueForGridTrackList(GridTrackSizingDirection,
                                         const LayoutObject*,
                                         const ComputedStyle&);
  static CSSValue* ValueForGridPosition(const GridPosition&);
  static FloatSize UsedBoxSize(const LayoutObject&);
  static CSSValue* RenderTextDecorationFlagsToCSSValue(TextDecoration);
  static CSSValue* ValueForTextDecorationStyle(ETextDecorationStyle);
  static CSSValue* ValueForTextDecorationSkipInk(ETextDecorationSkipInk);
  static CSSValue* TouchActionFlagsToCSSValue(TouchAction);
  static CSSValue* ValueForWillChange(const Vector<CSSPropertyID>&,
                                      bool will_change_contents,
                                      bool will_change_scroll_position);
  static CSSValue* ValueForAnimationDelay(const CSSTimingData*);
  static CSSValue* ValueForAnimationDirection(Timing::PlaybackDirection);
  static CSSValue* ValueForAnimationDuration(const CSSTimingData*);
  static CSSValue* ValueForAnimationFillMode(Timing::FillMode);
  static CSSValue* ValueForAnimationIterationCount(double iteration_count);
  static CSSValue* ValueForAnimationPlayState(EAnimPlayState);
  static CSSValue* CreateTimingFunctionValue(const TimingFunction*);
  static CSSValue* ValueForAnimationTimingFunction(const CSSTimingData*);
  static CSSValueList* ValuesForBorderRadiusCorner(const LengthSize&,
                                                   const ComputedStyle&);
  static const CSSValue& ValueForBorderRadiusCorner(const LengthSize&,
                                                    const ComputedStyle&);
  // TODO(fs): For some properties ('transform') we use the pixel snapped
  // border-box as the reference box. In other cases ('transform-origin') we use
  // the "unsnapped" border-box. Maybe use the same (the "unsnapped") in both
  // cases?
  enum UsePixelSnappedBox {
    kDontUsePixelSnappedBox,
    kUsePixelSnappedBox,
  };
  static FloatRect ReferenceBoxForTransform(
      const LayoutObject&,
      UsePixelSnappedBox = kUsePixelSnappedBox);
  static CSSValue* ComputedTransform(const LayoutObject*, const ComputedStyle&);
  static CSSValue* CreateTransitionPropertyValue(
      const CSSTransitionData::TransitionProperty&);
  static CSSValue* ValueForTransitionProperty(const CSSTransitionData*);
  static CSSValue* ValueForContentData(const ComputedStyle&,
                                       bool allow_visited_style);
  static CSSValue* ValueForCounterDirectives(const ComputedStyle&,
                                             bool is_increment);
  static CSSValue* ValueForShape(const ComputedStyle&,
                                 bool allow_visited_style,
                                 ShapeValue*);
  static CSSValueList* ValueForBorderRadiusShorthand(const ComputedStyle&);
  static CSSValue* StrokeDashArrayToCSSValueList(const SVGDashArray&,
                                                 const ComputedStyle&);
  static CSSValue* AdjustSVGPaintForCurrentColor(const SVGPaint&, const Color&);
  static CSSValue* ValueForSVGResource(const StyleSVGResource*);
  static CSSValue* ValueForShadowData(const ShadowData&,
                                      const ComputedStyle&,
                                      bool use_spread);
  static CSSValue* ValueForShadowList(const ShadowList*,
                                      const ComputedStyle&,
                                      bool use_spread);
  static CSSValue* ValueForFilter(const ComputedStyle&,
                                  const FilterOperations&);
  static CSSValue* ValueForScrollSnapType(const cc::ScrollSnapType&,
                                          const ComputedStyle&);
  static CSSValue* ValueForScrollSnapAlign(const cc::ScrollSnapAlign&,
                                           const ComputedStyle&);
  static CSSValue* ValueForPageBreakBetween(EBreakBetween);
  static CSSValue* ValueForWebkitColumnBreakBetween(EBreakBetween);
  static CSSValue* ValueForPageBreakInside(EBreakInside);
  static CSSValue* ValueForWebkitColumnBreakInside(EBreakInside);
  static bool WidthOrHeightShouldReturnUsedValue(const LayoutObject*);
  static CSSValueList* ValuesForShorthandProperty(const StylePropertyShorthand&,
                                                  const ComputedStyle&,
                                                  const LayoutObject*,
                                                  bool allow_visited_style);
  static CSSValuePair* ValuesForGapShorthand(const StylePropertyShorthand&,
                                             const ComputedStyle&,
                                             const LayoutObject*,
                                             bool allow_visited_style);
  static CSSValueList* ValuesForGridShorthand(const StylePropertyShorthand&,
                                              const ComputedStyle&,
                                              const LayoutObject*,
                                              bool allow_visited_style);
  static CSSValueList* ValuesForSidesShorthand(const StylePropertyShorthand&,
                                               const ComputedStyle&,
                                               const LayoutObject*,
                                               bool allow_visited_style);
  static CSSValuePair* ValuesForInlineBlockShorthand(
      const StylePropertyShorthand&,
      const ComputedStyle&,
      const LayoutObject*,
      bool allow_visited_style);
  static CSSValuePair* ValuesForPlaceShorthand(const StylePropertyShorthand&,
                                               const ComputedStyle&,
                                               const LayoutObject*,
                                               bool allow_visited_style);
  static CSSValue* ValuesForFontVariantProperty(const ComputedStyle&,
                                                const LayoutObject*,
                                                bool allow_visited_style);
  static CSSValue* ScrollCustomizationFlagsToCSSValue(
      scroll_customization::ScrollDirection);
  static CSSValue* ValueForGapLength(const GapLength&, const ComputedStyle&);
  static std::unique_ptr<CrossThreadStyleValue>
  CrossThreadStyleValueFromCSSStyleValue(CSSStyleValue* style_value);

  static CSSValuePair* ValuesForIntrinsicSizeShorthand(
      const StylePropertyShorthand&,
      const ComputedStyle&,
      const LayoutObject*,
      bool allow_visited_style);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PROPERTIES_COMPUTED_STYLE_UTILS_H_
