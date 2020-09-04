/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_STYLE_BUILDER_CONVERTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_STYLE_BUILDER_CONVERTER_H_

#include "cc/input/scroll_snap_data.h"
#include "third_party/blink/renderer/core/css/css_function_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_string_value.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/css_value_pair.h"
#include "third_party/blink/renderer/core/css/css_variable_data.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/grid_area.h"
#include "third_party/blink/renderer/core/style/grid_positions_resolver.h"
#include "third_party/blink/renderer/core/style/named_grid_lines_map.h"
#include "third_party/blink/renderer/core/style/ordered_named_grid_lines.h"
#include "third_party/blink/renderer/core/style/quotes_data.h"
#include "third_party/blink/renderer/core/style/shadow_list.h"
#include "third_party/blink/renderer/core/style/style_offset_rotation.h"
#include "third_party/blink/renderer/core/style/style_reflection.h"
#include "third_party/blink/renderer/core/style/svg_computed_style_defs.h"
#include "third_party/blink/renderer/core/style/transform_origin.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/geometry/length_size.h"
#include "third_party/blink/renderer/platform/graphics/image_orientation.h"
#include "third_party/blink/renderer/platform/text/tab_size.h"
#include "third_party/blink/renderer/platform/transforms/rotation.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"

namespace blink {

class ClipPathOperation;
class CSSToLengthConversionData;
class Font;
class FontBuilder;
class RotateTransformOperation;
class ScaleTransformOperation;
class StyleAutoColor;
class StylePath;
class StyleResolverState;
class StyleSVGResource;
class TextSizeAdjust;
class TranslateTransformOperation;
class UnzoomedLength;

class StyleBuilderConverterBase {
  STATIC_ONLY(StyleBuilderConverterBase);

 public:
  static FontSelectionValue ConvertFontStretch(const CSSValue&);
  static FontSelectionValue ConvertFontStyle(const CSSValue&);
  static FontSelectionValue ConvertFontWeight(const CSSValue&,
                                              FontSelectionValue);
  static FontDescription::FontVariantCaps ConvertFontVariantCaps(
      const CSSValue&);
  static FontDescription::FamilyDescription ConvertFontFamily(
      const CSSValue&,
      FontBuilder*,
      const Document* document_for_count);
  static FontDescription::Size ConvertFontSize(
      const CSSValue&,
      const CSSToLengthConversionData&,
      FontDescription::Size parent_size);
};

// Note that we assume the parser only allows valid CSSValue types.
class StyleBuilderConverter {
  STATIC_ONLY(StyleBuilderConverter);

 public:
  static scoped_refptr<StyleReflection> ConvertBoxReflect(StyleResolverState&,
                                                          const CSSValue&);
  static Color ConvertColor(StyleResolverState&,
                            const CSSValue&,
                            bool for_visited_link = false);
  template <typename T>
  static T ConvertComputedLength(StyleResolverState&, const CSSValue&);
  static LengthBox ConvertClip(StyleResolverState&, const CSSValue&);
  static scoped_refptr<ClipPathOperation> ConvertClipPath(StyleResolverState&,
                                                          const CSSValue&);
  static scoped_refptr<StyleSVGResource> ConvertElementReference(
      StyleResolverState&,
      const CSSValue&);
  static FilterOperations ConvertFilterOperations(StyleResolverState&,
                                                  const CSSValue&);
  static FilterOperations ConvertOffscreenFilterOperations(const CSSValue&,
                                                           const Font&);
  template <typename T>
  static T ConvertFlags(StyleResolverState&, const CSSValue&);
  static FontDescription::FamilyDescription ConvertFontFamily(
      StyleResolverState&,
      const CSSValue&);
  static scoped_refptr<FontFeatureSettings> ConvertFontFeatureSettings(
      StyleResolverState&,
      const CSSValue&);
  static scoped_refptr<FontVariationSettings> ConvertFontVariationSettings(
      const StyleResolverState&,
      const CSSValue&);
  static FontDescription::Size ConvertFontSize(StyleResolverState&,
                                               const CSSValue&);
  static float ConvertFontSizeAdjust(StyleResolverState&, const CSSValue&);

  static FontSelectionValue ConvertFontStretch(StyleResolverState&,
                                               const CSSValue&);
  static FontSelectionValue ConvertFontStyle(StyleResolverState&,
                                             const CSSValue&);
  static FontSelectionValue ConvertFontWeight(StyleResolverState&,
                                              const CSSValue&);

  static FontDescription::FontVariantCaps ConvertFontVariantCaps(
      StyleResolverState&,
      const CSSValue&);
  static FontDescription::VariantLigatures ConvertFontVariantLigatures(
      StyleResolverState&,
      const CSSValue&);
  static FontVariantNumeric ConvertFontVariantNumeric(StyleResolverState&,
                                                      const CSSValue&);
  static FontVariantEastAsian ConvertFontVariantEastAsian(StyleResolverState&,
                                                          const CSSValue&);
  static StyleSelfAlignmentData ConvertSelfOrDefaultAlignmentData(
      StyleResolverState&,
      const CSSValue&);
  static StyleContentAlignmentData ConvertContentAlignmentData(
      StyleResolverState&,
      const CSSValue&);
  static GridAutoFlow ConvertGridAutoFlow(StyleResolverState&, const CSSValue&);
  static GridPosition ConvertGridPosition(StyleResolverState&, const CSSValue&);
  static GridTrackSize ConvertGridTrackSize(StyleResolverState&,
                                            const CSSValue&);
  static GridTrackList ConvertGridTrackSizeList(StyleResolverState&,
                                                const CSSValue&);
  template <typename T>
  static T ConvertLineWidth(StyleResolverState&, const CSSValue&);
  static float ConvertBorderWidth(StyleResolverState&, const CSSValue&);
  static LayoutUnit ConvertLayoutUnit(StyleResolverState&, const CSSValue&);
  static base::Optional<Length> ConvertGapLength(const StyleResolverState&,
                                                 const CSSValue&);
  static Length ConvertLength(const StyleResolverState&, const CSSValue&);
  static UnzoomedLength ConvertUnzoomedLength(const StyleResolverState&,
                                              const CSSValue&);
  static float ConvertZoom(const StyleResolverState&, const CSSValue&);
  static Length ConvertLengthOrAuto(const StyleResolverState&, const CSSValue&);
  static Length ConvertLengthSizing(StyleResolverState&, const CSSValue&);
  static Length ConvertLengthMaxSizing(StyleResolverState&, const CSSValue&);
  static TabSize ConvertLengthOrTabSpaces(StyleResolverState&, const CSSValue&);
  static Length ConvertLineHeight(StyleResolverState&, const CSSValue&);
  static float ConvertNumberOrPercentage(StyleResolverState&, const CSSValue&);
  static float ConvertAlpha(StyleResolverState&,
                            const CSSValue&);  // clamps to [0,1]
  static StyleOffsetRotation ConvertOffsetRotate(StyleResolverState&,
                                                 const CSSValue&);
  static LengthPoint ConvertPosition(StyleResolverState&, const CSSValue&);
  static LengthPoint ConvertPositionOrAuto(StyleResolverState&,
                                           const CSSValue&);
  static float ConvertPerspective(StyleResolverState&, const CSSValue&);
  static Length ConvertQuirkyLength(StyleResolverState&, const CSSValue&);
  static scoped_refptr<QuotesData> ConvertQuotes(StyleResolverState&,
                                                 const CSSValue&);
  static LengthSize ConvertRadius(StyleResolverState&, const CSSValue&);
  static EPaintOrder ConvertPaintOrder(StyleResolverState&, const CSSValue&);
  static ShadowData ConvertShadow(const CSSToLengthConversionData&,
                                  StyleResolverState*,
                                  const CSSValue&);
  static scoped_refptr<ShadowList> ConvertShadowList(StyleResolverState&,
                                                     const CSSValue&);
  static ShapeValue* ConvertShapeValue(StyleResolverState&, const CSSValue&);
  static float ConvertSpacing(StyleResolverState&, const CSSValue&);
  template <CSSValueID IdForNone>
  static AtomicString ConvertString(StyleResolverState&, const CSSValue&);
  static scoped_refptr<SVGDashArray> ConvertStrokeDasharray(StyleResolverState&,
                                                            const CSSValue&);
  static StyleColor ConvertStyleColor(StyleResolverState&,
                                      const CSSValue&,
                                      bool for_visited_link = false);
  static StyleAutoColor ConvertStyleAutoColor(StyleResolverState&,
                                              const CSSValue&,
                                              bool for_visited_link = false);
  static SVGPaint ConvertSVGPaint(StyleResolverState&, const CSSValue&);
  static TextDecorationThickness ConvertTextDecorationThickness(
      StyleResolverState&,
      const CSSValue&);
  static TextEmphasisPosition ConvertTextTextEmphasisPosition(
      StyleResolverState&,
      const CSSValue&);
  static float ConvertTextStrokeWidth(StyleResolverState&, const CSSValue&);
  static TextSizeAdjust ConvertTextSizeAdjust(StyleResolverState&,
                                              const CSSValue&);
  static TextUnderlinePosition ConvertTextUnderlinePosition(
      StyleResolverState& state,
      const CSSValue& value);
  static Length ConvertTextUnderlineOffset(StyleResolverState& state,
                                           const CSSValue& value);
  static TransformOperations ConvertTransformOperations(StyleResolverState&,
                                                        const CSSValue&);
  static TransformOrigin ConvertTransformOrigin(StyleResolverState&,
                                                const CSSValue&);

  static void ConvertGridTrackList(
      const CSSValue&,
      GridTrackList&,
      NamedGridLinesMap&,
      OrderedNamedGridLines&,
      Vector<GridTrackSize>& auto_repeat_track_sizes,
      NamedGridLinesMap&,
      OrderedNamedGridLines&,
      size_t& auto_repeat_insertion_point,
      AutoRepeatType&,
      StyleResolverState&);
  static void CreateImplicitNamedGridLinesFromGridArea(
      const NamedGridAreaMap&,
      NamedGridLinesMap&,
      GridTrackSizingDirection);

  static cc::ScrollSnapType ConvertSnapType(StyleResolverState&,
                                            const CSSValue&);
  static cc::ScrollSnapAlign ConvertSnapAlign(StyleResolverState&,
                                              const CSSValue&);
  static scoped_refptr<TranslateTransformOperation> ConvertTranslate(
      StyleResolverState&,
      const CSSValue&);
  static scoped_refptr<RotateTransformOperation> ConvertRotate(
      StyleResolverState&,
      const CSSValue&);
  static scoped_refptr<ScaleTransformOperation> ConvertScale(
      StyleResolverState&,
      const CSSValue&);
  static RespectImageOrientationEnum ConvertImageOrientation(
      StyleResolverState&,
      const CSSValue&);
  static scoped_refptr<StylePath> ConvertPathOrNone(StyleResolverState&,
                                                    const CSSValue&);
  static scoped_refptr<BasicShape> ConvertOffsetPath(StyleResolverState&,
                                                     const CSSValue&);
  static StyleOffsetRotation ConvertOffsetRotate(const CSSValue&);
  template <CSSValueID cssValueFor0, CSSValueID cssValueFor100>
  static Length ConvertPositionLength(StyleResolverState&, const CSSValue&);
  static Rotation ConvertRotation(const CSSValue&);

  static const CSSValue& ConvertRegisteredPropertyInitialValue(const Document&,
                                                               const CSSValue&);
  static const CSSValue& ConvertRegisteredPropertyValue(
      const StyleResolverState&,
      const CSSValue&,
      const String& base_url,
      const WTF::TextEncoding&);

  static scoped_refptr<CSSVariableData> ConvertRegisteredPropertyVariableData(
      const CSSValue&,
      bool is_animation_tainted);

  static LengthSize ConvertIntrinsicSize(StyleResolverState&, const CSSValue&);

  static base::Optional<IntSize> ConvertAspectRatio(StyleResolverState&,
                                                    const CSSValue&);

  static bool ConvertInternalEmptyLineHeight(StyleResolverState& state,
                                             const CSSValue& value);

  static AtomicString ConvertPage(StyleResolverState&, const CSSValue&);

  static RubyPosition ConvertRubyPosition(StyleResolverState& state,
                                          const CSSValue& value);

  static ScrollbarGutter ConvertScrollbarGutter(StyleResolverState& state,
                                                const CSSValue& value);

  static void CountSystemColorComputeToSelfUsage(
      const StyleResolverState& state);
};

template <typename T>
T StyleBuilderConverter::ConvertComputedLength(StyleResolverState& state,
                                               const CSSValue& value) {
  return To<CSSPrimitiveValue>(value).ComputeLength<T>(
      state.CssToLengthConversionData());
}

template <typename T>
T StyleBuilderConverter::ConvertFlags(StyleResolverState& state,
                                      const CSSValue& value) {
  T flags = static_cast<T>(0);
  auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (identifier_value && identifier_value->GetValueID() == CSSValueID::kNone)
    return flags;
  for (auto& flag_value : To<CSSValueList>(value))
    flags |= To<CSSIdentifierValue>(*flag_value).ConvertTo<T>();
  return flags;
}

template <typename T>
T StyleBuilderConverter::ConvertLineWidth(StyleResolverState& state,
                                          const CSSValue& value) {
  double result = 0;
  if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    switch (identifier_value->GetValueID()) {
      case CSSValueID::kThin:
        result = 1;
        break;
      case CSSValueID::kMedium:
        result = 3;
        break;
      case CSSValueID::kThick:
        result = 5;
        break;
      default:
        NOTREACHED();
        break;
    }
    result = state.CssToLengthConversionData().ZoomedComputedPixels(
        result, CSSPrimitiveValue::UnitType::kPixels);
  } else {
    result = To<CSSPrimitiveValue>(value).ComputeLength<double>(
        state.CssToLengthConversionData());
  }
  // TODO(crbug.com/485650, crbug.com/382483): We are moving to use the full
  // page zoom implementation to handle high-dpi.  In that case specyfing a
  // border-width of less than 1px would result in a border that is one device
  // pixel thick.  With this change that would instead be rounded up to 2
  // device pixels.  Consider clamping it to device pixels or zoom adjusted CSS
  // pixels instead of raw CSS pixels.
  double zoomed_result = state.StyleRef().EffectiveZoom() * result;
  if (zoomed_result > 0.0 && zoomed_result < 1.0)
    return 1.0;
  return clampTo<T>(RoundForImpreciseConversion<T>(result),
                    defaultMinimumForClamp<T>(), defaultMaximumForClamp<T>());
}

template <CSSValueID cssValueFor0, CSSValueID cssValueFor100>
Length StyleBuilderConverter::ConvertPositionLength(StyleResolverState& state,
                                                    const CSSValue& value) {
  if (const auto* pair = DynamicTo<CSSValuePair>(value)) {
    Length length = StyleBuilderConverter::ConvertLength(state, pair->Second());
    if (To<CSSIdentifierValue>(pair->First()).GetValueID() == cssValueFor0)
      return length;
    DCHECK_EQ(To<CSSIdentifierValue>(pair->First()).GetValueID(),
              cssValueFor100);
    return length.SubtractFromOneHundredPercent();
  }

  if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    switch (identifier_value->GetValueID()) {
      case cssValueFor0:
        return Length::Percent(0);
      case cssValueFor100:
        return Length::Percent(100);
      case CSSValueID::kCenter:
        return Length::Percent(50);
      default:
        NOTREACHED();
    }
  }

  return StyleBuilderConverter::ConvertLength(state,
                                              To<CSSPrimitiveValue>(value));
}

template <CSSValueID IdForNone>
AtomicString StyleBuilderConverter::ConvertString(StyleResolverState&,
                                                  const CSSValue& value) {
  if (auto* string_value = DynamicTo<CSSStringValue>(value))
    return AtomicString(string_value->Value());
  DCHECK_EQ(To<CSSIdentifierValue>(value).GetValueID(), IdForNone);
  return g_null_atom;
}

}  // namespace blink

#endif
