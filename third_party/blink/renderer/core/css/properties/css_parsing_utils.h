// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PROPERTIES_CSS_PARSING_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PROPERTIES_CSS_PARSING_UTILS_H_

#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_mode.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_range.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/style/grid_area.h"

namespace blink {

namespace cssvalue {
class CSSFontFeatureValue;
}  // namespace cssvalue
class CSSIdentifierValue;
class CSSParserContext;
class CSSParserLocalContext;
class CSSShadowValue;
class CSSValue;
class CSSValueList;
class StylePropertyShorthand;

namespace css_parsing_utils {

enum class AllowInsetAndSpread { kAllow, kForbid };
enum class AllowTextValue { kAllow, kForbid };
enum class DefaultFill { kFill, kNoFill };
enum class ParsingStyle { kLegacy, kNotLegacy };
enum class TrackListType { kGridTemplate, kGridTemplateNoRepeat, kGridAuto };

using ConsumeAnimationItemValue = CSSValue* (*)(CSSPropertyID,
                                                CSSParserTokenRange&,
                                                const CSSParserContext&,
                                                bool use_legacy_parsing);
using IsPositionKeyword = bool (*)(CSSValueID);

constexpr size_t kMaxNumAnimationLonghands = 8;

bool IsBaselineKeyword(CSSValueID id);
bool IsSelfPositionKeyword(CSSValueID);
bool IsSelfPositionOrLeftOrRightKeyword(CSSValueID);
bool IsContentPositionKeyword(CSSValueID);
bool IsContentPositionOrLeftOrRightKeyword(CSSValueID);

CSSValue* ConsumeScrollOffset(CSSParserTokenRange&);
CSSValue* ConsumeSelfPositionOverflowPosition(CSSParserTokenRange&,
                                              IsPositionKeyword);
CSSValue* ConsumeSimplifiedDefaultPosition(CSSParserTokenRange&,
                                           IsPositionKeyword);
CSSValue* ConsumeSimplifiedSelfPosition(CSSParserTokenRange&,
                                        IsPositionKeyword);
CSSValue* ConsumeContentDistributionOverflowPosition(CSSParserTokenRange&,
                                                     IsPositionKeyword);
CSSValue* ConsumeSimplifiedContentPosition(CSSParserTokenRange&,
                                           IsPositionKeyword);

CSSValue* ConsumeAnimationIterationCount(CSSParserTokenRange&);
CSSValue* ConsumeAnimationName(CSSParserTokenRange&,
                               const CSSParserContext&,
                               bool allow_quoted_name);
CSSValue* ConsumeAnimationTimingFunction(CSSParserTokenRange&);
bool ConsumeAnimationShorthand(
    const StylePropertyShorthand&,
    HeapVector<Member<CSSValueList>, kMaxNumAnimationLonghands>&,
    ConsumeAnimationItemValue,
    CSSParserTokenRange&,
    const CSSParserContext&,
    bool use_legacy_parsing);

void AddBackgroundValue(CSSValue*& list, CSSValue*);
CSSValue* ConsumeBackgroundAttachment(CSSParserTokenRange&);
CSSValue* ConsumeBackgroundBlendMode(CSSParserTokenRange&);
CSSValue* ConsumeBackgroundBox(CSSParserTokenRange&);
CSSValue* ConsumeBackgroundComposite(CSSParserTokenRange&);
CSSValue* ConsumeMaskSourceType(CSSParserTokenRange&);
bool ConsumeBackgroundPosition(CSSParserTokenRange&,
                               const CSSParserContext&,
                               css_property_parser_helpers::UnitlessQuirk,
                               CSSValue*& result_x,
                               CSSValue*& result_y);
CSSValue* ConsumePrefixedBackgroundBox(CSSParserTokenRange&, AllowTextValue);
CSSValue* ParseBackgroundBox(CSSParserTokenRange&,
                             const CSSParserLocalContext&,
                             AllowTextValue alias_allow_text_value);
CSSValue* ParseBackgroundOrMaskSize(CSSParserTokenRange&,
                                    const CSSParserContext&,
                                    const CSSParserLocalContext&,
                                    base::Optional<WebFeature> negative_size);
bool ParseBackgroundOrMask(bool,
                           CSSParserTokenRange&,
                           const CSSParserContext&,
                           const CSSParserLocalContext&,
                           HeapVector<CSSPropertyValue, 256>&);

bool ConsumeRepeatStyleComponent(CSSParserTokenRange&,
                                 CSSValue*& value1,
                                 CSSValue*& value2,
                                 bool& implicit);
bool ConsumeRepeatStyle(CSSParserTokenRange&,
                        CSSValue*& result_x,
                        CSSValue*& result_y,
                        bool& implicit);

CSSValue* ConsumeWebkitBorderImage(CSSParserTokenRange&,
                                   const CSSParserContext&);
bool ConsumeBorderImageComponents(CSSParserTokenRange&,
                                  const CSSParserContext&,
                                  CSSValue*& source,
                                  CSSValue*& slice,
                                  CSSValue*& width,
                                  CSSValue*& outset,
                                  CSSValue*& repeat,
                                  DefaultFill);
CSSValue* ConsumeBorderImageRepeat(CSSParserTokenRange&);
CSSValue* ConsumeBorderImageSlice(CSSParserTokenRange&, DefaultFill);
CSSValue* ConsumeBorderImageWidth(CSSParserTokenRange&);
CSSValue* ConsumeBorderImageOutset(CSSParserTokenRange&);

CSSValue* ParseBorderRadiusCorner(CSSParserTokenRange&,
                                  const CSSParserContext&);
CSSValue* ParseBorderWidthSide(CSSParserTokenRange&,
                               const CSSParserContext&,
                               const CSSParserLocalContext&);

CSSValue* ConsumeShadow(CSSParserTokenRange&,
                        CSSParserMode,
                        AllowInsetAndSpread);
CSSShadowValue* ParseSingleShadow(CSSParserTokenRange&,
                                  CSSParserMode,
                                  AllowInsetAndSpread);

CSSValue* ConsumeColumnCount(CSSParserTokenRange&);
CSSValue* ConsumeColumnWidth(CSSParserTokenRange&);
bool ConsumeColumnWidthOrCount(CSSParserTokenRange&, CSSValue*&, CSSValue*&);
CSSValue* ConsumeGapLength(CSSParserTokenRange&, const CSSParserContext&);

CSSValue* ConsumeCounter(CSSParserTokenRange&, const CSSParserContext&, int);

CSSValue* ConsumeFontSize(
    CSSParserTokenRange&,
    const CSSParserContext&,
    css_property_parser_helpers::UnitlessQuirk =
        css_property_parser_helpers::UnitlessQuirk::kForbid);

CSSValue* ConsumeLineHeight(CSSParserTokenRange&, CSSParserMode);

CSSValueList* ConsumeFontFamily(CSSParserTokenRange&);
CSSValue* ConsumeGenericFamily(CSSParserTokenRange&);
CSSValue* ConsumeFamilyName(CSSParserTokenRange&);
String ConcatenateFamilyName(CSSParserTokenRange&);
CSSIdentifierValue* ConsumeFontStretchKeywordOnly(CSSParserTokenRange&);
CSSValue* ConsumeFontStretch(CSSParserTokenRange&, const CSSParserMode&);
CSSValue* ConsumeFontStyle(CSSParserTokenRange&, const CSSParserMode&);
CSSValue* ConsumeFontWeight(CSSParserTokenRange&, const CSSParserMode&);
CSSValue* ConsumeFontFeatureSettings(CSSParserTokenRange&);
cssvalue::CSSFontFeatureValue* ConsumeFontFeatureTag(CSSParserTokenRange&);
CSSIdentifierValue* ConsumeFontVariantCSS21(CSSParserTokenRange&);

CSSValue* ConsumeGridLine(CSSParserTokenRange&, const CSSParserContext&);
CSSValue* ConsumeGridTrackList(CSSParserTokenRange&,
                               const CSSParserContext&,
                               CSSParserMode,
                               TrackListType);
bool ParseGridTemplateAreasRow(const WTF::String& grid_row_names,
                               NamedGridAreaMap&,
                               const size_t row_count,
                               size_t& column_count);
CSSValue* ConsumeGridTemplatesRowsOrColumns(CSSParserTokenRange&,
                                            const CSSParserContext&,
                                            CSSParserMode);
bool ConsumeGridItemPositionShorthand(bool important,
                                      CSSParserTokenRange&,
                                      const CSSParserContext&,
                                      CSSValue*& start_value,
                                      CSSValue*& end_value);
bool ConsumeGridTemplateShorthand(bool important,
                                  CSSParserTokenRange&,
                                  const CSSParserContext&,
                                  CSSValue*& template_rows,
                                  CSSValue*& template_columns,
                                  CSSValue*& template_areas);

// The fragmentation spec says that page-break-(after|before|inside) are to be
// treated as shorthands for their break-(after|before|inside) counterparts.
// We'll do the same for the non-standard properties
// -webkit-column-break-(after|before|inside).
bool ConsumeFromPageBreakBetween(CSSParserTokenRange&, CSSValueID&);
bool ConsumeFromColumnBreakBetween(CSSParserTokenRange&, CSSValueID&);
bool ConsumeFromColumnOrPageBreakInside(CSSParserTokenRange&, CSSValueID&);

CSSValue* ConsumeMaxWidthOrHeight(
    CSSParserTokenRange&,
    const CSSParserContext&,
    css_property_parser_helpers::UnitlessQuirk =
        css_property_parser_helpers::UnitlessQuirk::kForbid);
CSSValue* ConsumeWidthOrHeight(
    CSSParserTokenRange&,
    const CSSParserContext&,
    css_property_parser_helpers::UnitlessQuirk =
        css_property_parser_helpers::UnitlessQuirk::kForbid);

CSSValue* ConsumeMarginOrOffset(CSSParserTokenRange&,
                                CSSParserMode,
                                css_property_parser_helpers::UnitlessQuirk);
CSSValue* ConsumeScrollPadding(CSSParserTokenRange&);
CSSValue* ConsumeOffsetPath(CSSParserTokenRange&, const CSSParserContext&);
CSSValue* ConsumePathOrNone(CSSParserTokenRange&);
CSSValue* ConsumeOffsetRotate(CSSParserTokenRange&, const CSSParserContext&);

CSSValue* ConsumeBasicShape(CSSParserTokenRange&, const CSSParserContext&);
bool ConsumeRadii(CSSValue* horizontal_radii[4],
                  CSSValue* vertical_radii[4],
                  CSSParserTokenRange&,
                  CSSParserMode,
                  bool use_legacy_parsing);

CSSValue* ConsumeTextDecorationLine(CSSParserTokenRange&);

CSSValue* ConsumeTransformValue(CSSParserTokenRange&,
                                const CSSParserContext&,
                                bool use_legacy_parsing);
CSSValue* ConsumeTransformList(CSSParserTokenRange&,
                               const CSSParserContext&,
                               const CSSParserLocalContext&);
CSSValue* ConsumeTransitionProperty(CSSParserTokenRange&,
                                    const CSSParserContext&);
bool IsValidPropertyList(const CSSValueList&);

CSSValue* ConsumeBorderColorSide(CSSParserTokenRange&,
                                 const CSSParserContext&,
                                 const CSSParserLocalContext&);
CSSValue* ConsumeBorderWidth(CSSParserTokenRange&,
                             CSSParserMode,
                             css_property_parser_helpers::UnitlessQuirk);
CSSValue* ParsePaintStroke(CSSParserTokenRange&, const CSSParserContext&);
CSSValue* ParseSpacing(CSSParserTokenRange&, const CSSParserContext&);

css_property_parser_helpers::UnitlessQuirk UnitlessUnlessShorthand(
    const CSSParserLocalContext&);

template <CSSValueID start, CSSValueID end>
CSSValue* ConsumePositionLonghand(CSSParserTokenRange& range,
                                  CSSParserMode css_parser_mode) {
  if (range.Peek().GetType() == kIdentToken) {
    CSSValueID id = range.Peek().Id();
    int percent;
    if (id == start)
      percent = 0;
    else if (id == CSSValueID::kCenter)
      percent = 50;
    else if (id == end)
      percent = 100;
    else
      return nullptr;
    range.ConsumeIncludingWhitespace();
    return CSSNumericLiteralValue::Create(
        percent, CSSPrimitiveValue::UnitType::kPercentage);
  }
  return css_property_parser_helpers::ConsumeLengthOrPercent(
      range, css_parser_mode, kValueRangeAll);
}

CSSValue* ConsumeIntrinsicLength(CSSParserTokenRange&, const CSSParserContext&);

}  // namespace css_parsing_utils
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PROPERTIES_CSS_PARSING_UTILS_H_
