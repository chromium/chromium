// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PROPERTIES_CSS_PARSING_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PROPERTIES_CSS_PARSING_UTILS_H_

#include "base/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"
#include "third_party/blink/renderer/core/css/css_function_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_mode.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_range.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/frame/web_feature_forward.h"
#include "third_party/blink/renderer/core/style/grid_area.h"
#include "third_party/blink/renderer/platform/geometry/length.h"  // For ValueRange
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

namespace cssvalue {
class CSSFontFeatureValue;
class CSSURIValue;
}  // namespace cssvalue
class CSSIdentifierValue;
class CSSParserContext;
class CSSParserLocalContext;
class CSSPropertyValue;
class CSSShadowValue;
class CSSStringValue;
class CSSValue;
class CSSValueList;
class CSSValuePair;
class StylePropertyShorthand;

// "Consume" functions, when successful, should consume all the relevant tokens
// as well as any trailing whitespace. When the start of the range doesn't
// match the type we're looking for, the range should not be modified.
namespace css_parsing_utils {

enum class AllowInsetAndSpread { kAllow, kForbid };
enum class AllowTextValue { kAllow, kForbid };
enum class DefaultFill { kFill, kNoFill };
enum class ParsingStyle { kLegacy, kNotLegacy };
enum class TrackListType { kGridTemplate, kGridTemplateNoRepeat, kGridAuto };
enum class UnitlessQuirk { kAllow, kForbid };

using ConsumeAnimationItemValue = CSSValue* (*)(CSSPropertyID,
                                                CSSParserTokenRange&,
                                                const CSSParserContext&,
                                                bool use_legacy_parsing);
using IsPositionKeyword = bool (*)(CSSValueID);

constexpr size_t kMaxNumAnimationLonghands = 9;

void Complete4Sides(CSSValue* side[4]);

// TODO(timloh): These should probably just be consumeComma and consumeSlash.
bool ConsumeCommaIncludingWhitespace(CSSParserTokenRange&);
bool ConsumeSlashIncludingWhitespace(CSSParserTokenRange&);
// consumeFunction expects the range starts with a FunctionToken.
CSSParserTokenRange ConsumeFunction(CSSParserTokenRange&);

CSSPrimitiveValue* ConsumeInteger(
    CSSParserTokenRange&,
    const CSSParserContext&,
    double minimum_value = -std::numeric_limits<double>::max());
CSSPrimitiveValue* ConsumeIntegerOrNumberCalc(CSSParserTokenRange&,
                                              const CSSParserContext&);
CSSPrimitiveValue* ConsumePositiveInteger(CSSParserTokenRange&,
                                          const CSSParserContext&);
bool ConsumeNumberRaw(CSSParserTokenRange&,
                      const CSSParserContext& context,
                      double& result);
CSSPrimitiveValue* ConsumeNumber(CSSParserTokenRange&,
                                 const CSSParserContext&,
                                 ValueRange);
CSSPrimitiveValue* ConsumeLength(CSSParserTokenRange&,
                                 const CSSParserContext&,
                                 ValueRange,
                                 UnitlessQuirk = UnitlessQuirk::kForbid);
CSSPrimitiveValue* ConsumePercent(CSSParserTokenRange&,
                                  const CSSParserContext&,
                                  ValueRange);
CSSPrimitiveValue* ConsumeAlphaValue(CSSParserTokenRange&,
                                     const CSSParserContext&);
CSSPrimitiveValue* ConsumeLengthOrPercent(
    CSSParserTokenRange&,
    const CSSParserContext&,
    ValueRange,
    UnitlessQuirk = UnitlessQuirk::kForbid);
CSSPrimitiveValue* ConsumeSVGGeometryPropertyLength(CSSParserTokenRange&,
                                                    const CSSParserContext&,
                                                    ValueRange);

CSSPrimitiveValue* ConsumeAngle(
    CSSParserTokenRange&,
    const CSSParserContext&,
    base::Optional<WebFeature> unitless_zero_feature);
CSSPrimitiveValue* ConsumeAngle(
    CSSParserTokenRange&,
    const CSSParserContext&,
    base::Optional<WebFeature> unitless_zero_feature,
    double minimum_value,
    double maximum_value);
CSSPrimitiveValue* ConsumeTime(CSSParserTokenRange&,
                               const CSSParserContext&,
                               ValueRange);
CSSPrimitiveValue* ConsumeResolution(CSSParserTokenRange&);

CSSIdentifierValue* ConsumeIdent(CSSParserTokenRange&);
CSSIdentifierValue* ConsumeIdentRange(CSSParserTokenRange&,
                                      CSSValueID lower,
                                      CSSValueID upper);
template <CSSValueID, CSSValueID...>
inline bool IdentMatches(CSSValueID id);
template <CSSValueID... allowedIdents>
CSSIdentifierValue* ConsumeIdent(CSSParserTokenRange&);

CSSCustomIdentValue* ConsumeCustomIdent(CSSParserTokenRange&,
                                        const CSSParserContext&);
CSSStringValue* ConsumeString(CSSParserTokenRange&);
StringView ConsumeUrlAsStringView(CSSParserTokenRange&,
                                  const CSSParserContext&);
cssvalue::CSSURIValue* ConsumeUrl(CSSParserTokenRange&,
                                  const CSSParserContext&);
CSSValue* ConsumeSelectorFunction(CSSParserTokenRange&);
CORE_EXPORT CSSValue* ConsumeIdSelector(CSSParserTokenRange&);

CSSValue* ConsumeColor(CSSParserTokenRange&,
                       const CSSParserContext&,
                       bool accept_quirky_colors = false);

CSSValue* ConsumeLineWidth(CSSParserTokenRange&,
                           const CSSParserContext&,
                           UnitlessQuirk);

CSSValuePair* ConsumePosition(CSSParserTokenRange&,
                              const CSSParserContext&,
                              UnitlessQuirk,
                              base::Optional<WebFeature> three_value_position);
bool ConsumePosition(CSSParserTokenRange&,
                     const CSSParserContext&,
                     UnitlessQuirk,
                     base::Optional<WebFeature> three_value_position,
                     CSSValue*& result_x,
                     CSSValue*& result_y);
bool ConsumeOneOrTwoValuedPosition(CSSParserTokenRange&,
                                   const CSSParserContext&,
                                   UnitlessQuirk,
                                   CSSValue*& result_x,
                                   CSSValue*& result_y);
bool ConsumeBorderShorthand(CSSParserTokenRange&,
                            const CSSParserContext&,
                            const CSSValue*& result_width,
                            const CSSValue*& result_style,
                            const CSSValue*& result_color);

enum class ConsumeGeneratedImagePolicy { kAllow, kForbid };

CSSValue* ConsumeImage(
    CSSParserTokenRange&,
    const CSSParserContext&,
    ConsumeGeneratedImagePolicy = ConsumeGeneratedImagePolicy::kAllow);
CSSValue* ConsumeImageOrNone(CSSParserTokenRange&, const CSSParserContext&);

CSSValue* ConsumeAxis(CSSParserTokenRange&, const CSSParserContext& context);

CSSIdentifierValue* ConsumeShapeBox(CSSParserTokenRange&);

enum class IsImplicitProperty { kNotImplicit, kImplicit };

void AddProperty(CSSPropertyID resolved_property,
                 CSSPropertyID current_shorthand,
                 const CSSValue&,
                 bool important,
                 IsImplicitProperty,
                 HeapVector<CSSPropertyValue, 256>& properties);

void CountKeywordOnlyPropertyUsage(CSSPropertyID,
                                   const CSSParserContext&,
                                   CSSValueID);

const CSSValue* ParseLonghand(CSSPropertyID unresolved_property,
                              CSSPropertyID current_shorthand,
                              const CSSParserContext&,
                              CSSParserTokenRange&);

bool ConsumeShorthandVia2Longhands(
    const StylePropertyShorthand&,
    bool important,
    const CSSParserContext&,
    CSSParserTokenRange&,
    HeapVector<CSSPropertyValue, 256>& properties);

bool ConsumeShorthandVia4Longhands(
    const StylePropertyShorthand&,
    bool important,
    const CSSParserContext&,
    CSSParserTokenRange&,
    HeapVector<CSSPropertyValue, 256>& properties);

bool ConsumeShorthandGreedilyViaLonghands(
    const StylePropertyShorthand&,
    bool important,
    const CSSParserContext&,
    CSSParserTokenRange&,
    HeapVector<CSSPropertyValue, 256>& properties);

void AddExpandedPropertyForValue(CSSPropertyID prop_id,
                                 const CSSValue&,
                                 bool,
                                 HeapVector<CSSPropertyValue, 256>& properties);

CSSValue* ConsumeTransformValue(CSSParserTokenRange&, const CSSParserContext&);
CSSValue* ConsumeTransformList(CSSParserTokenRange&, const CSSParserContext&);
CSSValue* ConsumeFilterFunctionList(CSSParserTokenRange&,
                                    const CSSParserContext&);

bool IsBaselineKeyword(CSSValueID id);
bool IsSelfPositionKeyword(CSSValueID);
bool IsSelfPositionOrLeftOrRightKeyword(CSSValueID);
bool IsContentPositionKeyword(CSSValueID);
bool IsContentPositionOrLeftOrRightKeyword(CSSValueID);
CORE_EXPORT bool IsCSSWideKeyword(CSSValueID);
CORE_EXPORT bool IsCSSWideKeyword(StringView);
bool IsRevertKeyword(StringView);
bool IsDefaultKeyword(StringView);
bool IsHashIdentifier(const CSSParserToken&);

// This function returns false for CSS-wide keywords, 'default', and any
// template parameters provided.
//
// https://drafts.csswg.org/css-values-4/#identifier-value
template <CSSValueID, CSSValueID...>
bool IsCustomIdent(CSSValueID);

// https://drafts.csswg.org/scroll-animations-1/#typedef-timeline-name
bool IsTimelineName(const CSSParserToken&);

CSSValue* ConsumeScrollOffset(CSSParserTokenRange&, const CSSParserContext&);
CSSValue* ConsumeElementOffset(CSSParserTokenRange&, const CSSParserContext&);
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

CSSValue* ConsumeAnimationIterationCount(CSSParserTokenRange&,
                                         const CSSParserContext&);
CSSValue* ConsumeAnimationName(CSSParserTokenRange&,
                               const CSSParserContext&,
                               bool allow_quoted_name);
CSSValue* ConsumeAnimationTimeline(CSSParserTokenRange&,
                                   const CSSParserContext&);
CSSValue* ConsumeAnimationTimingFunction(CSSParserTokenRange&,
                                         const CSSParserContext&);
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
bool ConsumeBackgroundPosition(CSSParserTokenRange&,
                               const CSSParserContext&,
                               UnitlessQuirk,
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
CSSValue* ConsumeBorderImageSlice(CSSParserTokenRange&,
                                  const CSSParserContext&,
                                  DefaultFill);
CSSValue* ConsumeBorderImageWidth(CSSParserTokenRange&,
                                  const CSSParserContext&);
CSSValue* ConsumeBorderImageOutset(CSSParserTokenRange&,
                                   const CSSParserContext&);

CSSValue* ParseBorderRadiusCorner(CSSParserTokenRange&,
                                  const CSSParserContext&);
CSSValue* ParseBorderWidthSide(CSSParserTokenRange&,
                               const CSSParserContext&,
                               const CSSParserLocalContext&);

CSSValue* ConsumeShadow(CSSParserTokenRange&,
                        const CSSParserContext&,
                        AllowInsetAndSpread);
CSSShadowValue* ParseSingleShadow(CSSParserTokenRange&,
                                  const CSSParserContext&,
                                  AllowInsetAndSpread);

CSSValue* ConsumeColumnCount(CSSParserTokenRange&, const CSSParserContext&);
CSSValue* ConsumeColumnWidth(CSSParserTokenRange&, const CSSParserContext&);
bool ConsumeColumnWidthOrCount(CSSParserTokenRange&,
                               const CSSParserContext&,
                               CSSValue*&,
                               CSSValue*&);
CSSValue* ConsumeGapLength(CSSParserTokenRange&, const CSSParserContext&);

CSSValue* ConsumeCounter(CSSParserTokenRange&, const CSSParserContext&, int);

CSSValue* ConsumeFontSize(CSSParserTokenRange&,
                          const CSSParserContext&,
                          UnitlessQuirk = UnitlessQuirk::kForbid);

CSSValue* ConsumeLineHeight(CSSParserTokenRange&, const CSSParserContext&);

CSSValueList* ConsumeFontFamily(CSSParserTokenRange&);
CSSValue* ConsumeGenericFamily(CSSParserTokenRange&);
CSSValue* ConsumeFamilyName(CSSParserTokenRange&);
String ConcatenateFamilyName(CSSParserTokenRange&);
CSSIdentifierValue* ConsumeFontStretchKeywordOnly(CSSParserTokenRange&);
CSSValue* ConsumeFontStretch(CSSParserTokenRange&, const CSSParserContext&);
CSSValue* ConsumeFontStyle(CSSParserTokenRange&, const CSSParserContext&);
CSSValue* ConsumeFontWeight(CSSParserTokenRange&, const CSSParserContext&);
CSSValue* ConsumeFontFeatureSettings(CSSParserTokenRange&,
                                     const CSSParserContext&);
cssvalue::CSSFontFeatureValue* ConsumeFontFeatureTag(CSSParserTokenRange&,
                                                     const CSSParserContext&);
CSSIdentifierValue* ConsumeFontVariantCSS21(CSSParserTokenRange&);

CSSValue* ConsumeGridLine(CSSParserTokenRange&, const CSSParserContext&);
CSSValue* ConsumeGridTrackList(CSSParserTokenRange&,
                               const CSSParserContext&,
                               TrackListType);
bool ParseGridTemplateAreasRow(const WTF::String& grid_row_names,
                               NamedGridAreaMap&,
                               const size_t row_count,
                               size_t& column_count);
CSSValue* ConsumeGridTemplatesRowsOrColumns(CSSParserTokenRange&,
                                            const CSSParserContext&);
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

CSSValue* ConsumeMaxWidthOrHeight(CSSParserTokenRange&,
                                  const CSSParserContext&,
                                  UnitlessQuirk = UnitlessQuirk::kForbid);
CSSValue* ConsumeWidthOrHeight(CSSParserTokenRange&,
                               const CSSParserContext&,
                               UnitlessQuirk = UnitlessQuirk::kForbid);

CSSValue* ConsumeMarginOrOffset(CSSParserTokenRange&,
                                const CSSParserContext&,
                                UnitlessQuirk);
CSSValue* ConsumeScrollPadding(CSSParserTokenRange&, const CSSParserContext&);
CSSValue* ConsumeOffsetPath(CSSParserTokenRange&, const CSSParserContext&);
CSSValue* ConsumePathOrNone(CSSParserTokenRange&);
CSSValue* ConsumeOffsetRotate(CSSParserTokenRange&, const CSSParserContext&);

CSSValue* ConsumeBasicShape(CSSParserTokenRange&, const CSSParserContext&);
bool ConsumeRadii(CSSValue* horizontal_radii[4],
                  CSSValue* vertical_radii[4],
                  CSSParserTokenRange&,
                  const CSSParserContext&,
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
                             const CSSParserContext&,
                             UnitlessQuirk);
CSSValue* ParsePaintStroke(CSSParserTokenRange&, const CSSParserContext&);
CSSValue* ParseSpacing(CSSParserTokenRange&, const CSSParserContext&);

UnitlessQuirk UnitlessUnlessShorthand(const CSSParserLocalContext&);

// Template implementations are at the bottom of the file for readability.

template <typename... emptyBaseCase>
inline bool IdentMatches(CSSValueID id) {
  return false;
}
template <CSSValueID head, CSSValueID... tail>
inline bool IdentMatches(CSSValueID id) {
  return id == head || IdentMatches<tail...>(id);
}

template <typename...>
bool IsCustomIdent(CSSValueID id) {
  return !IsCSSWideKeyword(id) && id != CSSValueID::kDefault;
}

template <CSSValueID head, CSSValueID... tail>
bool IsCustomIdent(CSSValueID id) {
  return id != head && IsCustomIdent<tail...>(id);
}

template <CSSValueID... names>
CSSIdentifierValue* ConsumeIdent(CSSParserTokenRange& range) {
  if (range.Peek().GetType() != kIdentToken ||
      !IdentMatches<names...>(range.Peek().Id()))
    return nullptr;
  return CSSIdentifierValue::Create(range.ConsumeIncludingWhitespace().Id());
}

// ConsumeCommaSeparatedList takes a callback function to call on each item in
// the list, followed by the arguments to pass to this callback.
// The first argument to the callback must be the CSSParserTokenRange
template <typename Func, typename... Args>
CSSValueList* ConsumeCommaSeparatedList(Func callback,
                                        CSSParserTokenRange& range,
                                        Args&&... args) {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  do {
    CSSValue* value = callback(range, std::forward<Args>(args)...);
    if (!value)
      return nullptr;
    list->Append(*value);
  } while (ConsumeCommaIncludingWhitespace(range));
  DCHECK(list->length());
  return list;
}

template <CSSValueID start, CSSValueID end>
CSSValue* ConsumePositionLonghand(CSSParserTokenRange& range,
                                  const CSSParserContext& context) {
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
  return ConsumeLengthOrPercent(range, context, kValueRangeAll);
}

}  // namespace css_parsing_utils
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PROPERTIES_CSS_PARSING_UTILS_H_
