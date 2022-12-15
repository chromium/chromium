// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PROPERTIES_CSS_PARSING_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PROPERTIES_CSS_PARSING_UTILS_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_anchor_query_type.h"
#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"
#include "third_party/blink/renderer/core/css/css_function_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_mode.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_range.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/frame/web_feature_forward.h"
#include "third_party/blink/renderer/core/style/grid_area.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/member.h"

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
enum class AllowPathValue { kAllow, kForbid };
enum class AllowBasicShapeRectValue { kAllow, kForbid };
enum class AllowBasicShapeXYWHValue { kAllow, kForbid };
enum class DefaultFill { kFill, kNoFill };
enum class ParsingStyle { kLegacy, kNotLegacy };
enum class TrackListType {
  kGridAuto,
  kGridTemplate,
  kGridTemplateNoRepeat,
  kGridTemplateSubgrid
};
enum class UnitlessQuirk { kAllow, kForbid };
enum class AllowedColorKeywords { kAllowSystemColor, kNoSystemColor };

using ConsumeAnimationItemValue = CSSValue* (*)(CSSPropertyID,
                                                CSSParserTokenRange&,
                                                const CSSParserContext&,
                                                bool use_legacy_parsing);
using IsPositionKeyword = bool (*)(CSSValueID);

constexpr size_t kMaxNumAnimationLonghands = 10;

void Complete4Sides(CSSValue* side[4]);

// TODO(timloh): These should probably just be consumeComma and consumeSlash.
bool ConsumeCommaIncludingWhitespace(CSSParserTokenRange&);
bool ConsumeSlashIncludingWhitespace(CSSParserTokenRange&);
// consumeFunction expects the range starts with a FunctionToken.
CSSParserTokenRange ConsumeFunction(CSSParserTokenRange&);

// https://drafts.csswg.org/css-syntax/#typedef-any-value
//
// Consumes component values until it reaches a token that is not allowed
// for <any-value>.
CORE_EXPORT bool ConsumeAnyValue(CSSParserTokenRange&);

CSSPrimitiveValue* ConsumeInteger(
    CSSParserTokenRange&,
    const CSSParserContext&,
    double minimum_value = -std::numeric_limits<double>::max());
CSSPrimitiveValue* ConsumeIntegerOrNumberCalc(
    CSSParserTokenRange&,
    const CSSParserContext&,
    CSSPrimitiveValue::ValueRange = CSSPrimitiveValue::ValueRange::kInteger);
CSSPrimitiveValue* ConsumePositiveInteger(CSSParserTokenRange&,
                                          const CSSParserContext&);
bool ConsumeNumberRaw(CSSParserTokenRange&,
                      const CSSParserContext& context,
                      double& result);
CSSPrimitiveValue* ConsumeNumber(CSSParserTokenRange&,
                                 const CSSParserContext&,
                                 CSSPrimitiveValue::ValueRange);
CSSPrimitiveValue* ConsumeLength(CSSParserTokenRange&,
                                 const CSSParserContext&,
                                 CSSPrimitiveValue::ValueRange,
                                 UnitlessQuirk = UnitlessQuirk::kForbid);
CSSPrimitiveValue* ConsumePercent(CSSParserTokenRange&,
                                  const CSSParserContext&,
                                  CSSPrimitiveValue::ValueRange);

// Any percentages are converted to numbers.
CSSPrimitiveValue* ConsumeNumberOrPercent(CSSParserTokenRange&,
                                          const CSSParserContext&,
                                          CSSPrimitiveValue::ValueRange);

CSSPrimitiveValue* ConsumeAlphaValue(CSSParserTokenRange&,
                                     const CSSParserContext&);
CSSPrimitiveValue* ConsumeLengthOrPercent(
    CSSParserTokenRange&,
    const CSSParserContext&,
    CSSPrimitiveValue::ValueRange,
    UnitlessQuirk = UnitlessQuirk::kForbid,
    CSSAnchorQueryTypes = kCSSAnchorQueryTypesNone);
CSSPrimitiveValue* ConsumeSVGGeometryPropertyLength(
    CSSParserTokenRange&,
    const CSSParserContext&,
    CSSPrimitiveValue::ValueRange);

CORE_EXPORT CSSPrimitiveValue* ConsumeAngle(
    CSSParserTokenRange&,
    const CSSParserContext&,
    absl::optional<WebFeature> unitless_zero_feature);
CORE_EXPORT CSSPrimitiveValue* ConsumeAngle(
    CSSParserTokenRange&,
    const CSSParserContext&,
    absl::optional<WebFeature> unitless_zero_feature,
    double minimum_value,
    double maximum_value);
CSSPrimitiveValue* ConsumeHue(CSSParserTokenRange&,
                              const CSSParserContext&,
                              absl::optional<WebFeature> unitless_zero_feature);
CSSPrimitiveValue* ConsumeTime(CSSParserTokenRange&,
                               const CSSParserContext&,
                               CSSPrimitiveValue::ValueRange);
CSSPrimitiveValue* ConsumeResolution(CSSParserTokenRange&);
CSSValue* ConsumeRatio(CSSParserTokenRange&, const CSSParserContext&);
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
CSSCustomIdentValue* ConsumeDashedIdent(CSSParserTokenRange&,
                                        const CSSParserContext&);
CSSStringValue* ConsumeString(CSSParserTokenRange&);
StringView ConsumeUrlAsStringView(CSSParserTokenRange&,
                                  const CSSParserContext&);
cssvalue::CSSURIValue* ConsumeUrl(CSSParserTokenRange&,
                                  const CSSParserContext&);

CORE_EXPORT CSSValue* ConsumeColor(CSSParserTokenRange&,
                                   const CSSParserContext&,
                                   bool accept_quirky_colors = false,
                                   AllowedColorKeywords allowed_keywords =
                                       AllowedColorKeywords::kAllowSystemColor);

CSSValue* ConsumeLineWidth(CSSParserTokenRange&,
                           const CSSParserContext&,
                           UnitlessQuirk);

CSSValuePair* ConsumePosition(CSSParserTokenRange&,
                              const CSSParserContext&,
                              UnitlessQuirk,
                              absl::optional<WebFeature> three_value_position);
bool ConsumePosition(CSSParserTokenRange&,
                     const CSSParserContext&,
                     UnitlessQuirk,
                     absl::optional<WebFeature> three_value_position,
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

// Old syntax: auto | <length>
CSSValue* ConsumeIntrinsicSizeLonghandOld(CSSParserTokenRange&,
                                          const CSSParserContext&);
// New syntax: none | <length> | auto && <length>
CSSValue* ConsumeIntrinsicSizeLonghandNew(CSSParserTokenRange&,
                                          const CSSParserContext&);
// Picks between Old and New based on RuntimeEnabledFeatures
CSSValue* ConsumeIntrinsicSizeLonghand(CSSParserTokenRange&,
                                       const CSSParserContext&);

CSSIdentifierValue* ConsumeShapeBox(CSSParserTokenRange&);
CSSIdentifierValue* ConsumeVisualBox(CSSParserTokenRange&);

enum class IsImplicitProperty { kNotImplicit, kImplicit };

void AddProperty(CSSPropertyID resolved_property,
                 CSSPropertyID current_shorthand,
                 const CSSValue&,
                 bool important,
                 IsImplicitProperty,
                 HeapVector<CSSPropertyValue, 64>& properties);

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
    HeapVector<CSSPropertyValue, 64>& properties);

bool ConsumeShorthandVia4Longhands(
    const StylePropertyShorthand&,
    bool important,
    const CSSParserContext&,
    CSSParserTokenRange&,
    HeapVector<CSSPropertyValue, 64>& properties);

bool ConsumeShorthandGreedilyViaLonghands(
    const StylePropertyShorthand&,
    bool important,
    const CSSParserContext&,
    CSSParserTokenRange&,
    HeapVector<CSSPropertyValue, 64>& properties,
    bool use_initial_value_function = false);

void AddExpandedPropertyForValue(CSSPropertyID prop_id,
                                 const CSSValue&,
                                 bool,
                                 HeapVector<CSSPropertyValue, 64>& properties);

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
CORE_EXPORT bool IsDashedIdent(const CSSParserToken&);

CSSValue* ConsumeCSSWideKeyword(CSSParserTokenRange&);

// This function returns false for CSS-wide keywords, 'default', and any
// template parameters provided.
//
// https://drafts.csswg.org/css-values-4/#identifier-value
template <CSSValueID, CSSValueID...>
bool IsCustomIdent(CSSValueID);

// https://drafts.csswg.org/scroll-animations-1/#typedef-timeline-name
bool IsTimelineName(const CSSParserToken&);

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
CSSValue* ConsumeScrollFunction(CSSParserTokenRange&, const CSSParserContext&);
CSSValue* ConsumeViewFunction(CSSParserTokenRange&, const CSSParserContext&);
CSSValue* ConsumeAnimationTimeline(CSSParserTokenRange&,
                                   const CSSParserContext&);
CSSValue* ConsumeAnimationTimingFunction(CSSParserTokenRange&,
                                         const CSSParserContext&);
CSSValue* ConsumeAnimationDuration(CSSParserTokenRange&,
                                   const CSSParserContext&);
// https://drafts.csswg.org/scroll-animations-1/#typedef-timeline-range-name
CSSValue* ConsumeTimelineRangeName(CSSParserTokenRange&);
CSSValue* ConsumeTimelineRangeNameAndPercent(CSSParserTokenRange&,
                                             const CSSParserContext&);
CSSValue* ConsumeAnimationDelay(CSSParserTokenRange&, const CSSParserContext&);

bool ConsumeAnimationShorthand(
    const StylePropertyShorthand&,
    HeapVector<Member<CSSValueList>, kMaxNumAnimationLonghands>&,
    ConsumeAnimationItemValue,
    CSSParserTokenRange&,
    const CSSParserContext&,
    bool use_legacy_parsing);

CSSValue* ConsumeSingleTimelineAxis(CSSParserTokenRange&);
CSSValue* ConsumeSingleTimelineName(CSSParserTokenRange&,
                                    const CSSParserContext&);
CSSValue* ConsumeSingleTimelineInset(CSSParserTokenRange&,
                                     const CSSParserContext&);

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
                                    absl::optional<WebFeature> negative_size);
bool ParseBackgroundOrMask(bool,
                           CSSParserTokenRange&,
                           const CSSParserContext&,
                           const CSSParserLocalContext&,
                           HeapVector<CSSPropertyValue, 64>&);

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

CSSValue* ConsumeMathDepth(CSSParserTokenRange& range,
                           const CSSParserContext& context);

CSSValueList* ConsumeFontFamily(CSSParserTokenRange&);
CSSValue* ConsumeGenericFamily(CSSParserTokenRange&);
CSSValue* ConsumeFamilyName(CSSParserTokenRange&);
String ConcatenateFamilyName(CSSParserTokenRange&);
CSSIdentifierValue* ConsumeFontStretchKeywordOnly(CSSParserTokenRange&,
                                                  const CSSParserContext&);
CSSValue* ConsumeFontStretch(CSSParserTokenRange&, const CSSParserContext&);
CSSValue* ConsumeFontStyle(CSSParserTokenRange&, const CSSParserContext&);
CSSValue* ConsumeFontWeight(CSSParserTokenRange&, const CSSParserContext&);
CSSValue* ConsumeFontFeatureSettings(CSSParserTokenRange&,
                                     const CSSParserContext&);
cssvalue::CSSFontFeatureValue* ConsumeFontFeatureTag(CSSParserTokenRange&,
                                                     const CSSParserContext&);
CSSIdentifierValue* ConsumeFontVariantCSS21(CSSParserTokenRange&);
CSSIdentifierValue* ConsumeFontTechIdent(CSSParserTokenRange&);
CSSIdentifierValue* ConsumeFontFormatIdent(CSSParserTokenRange&);
CSSValueID FontFormatToId(String);
bool IsSupportedKeywordTech(CSSValueID keyword);
bool IsSupportedKeywordFormat(CSSValueID keyword);

CSSValue* ConsumeGridLine(CSSParserTokenRange&, const CSSParserContext&);
CSSValue* ConsumeGridTrackList(CSSParserTokenRange&,
                               const CSSParserContext&,
                               TrackListType);
bool ParseGridTemplateAreasRow(const WTF::String& grid_row_names,
                               NamedGridAreaMap&,
                               const wtf_size_t row_count,
                               wtf_size_t& column_count);
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
                                  const CSSValue*& template_rows,
                                  const CSSValue*& template_columns,
                                  const CSSValue*& template_areas);

CSSValue* ConsumeHyphenateLimitChars(CSSParserTokenRange&,
                                     const CSSParserContext&);

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
                                UnitlessQuirk,
                                CSSAnchorQueryTypes = kCSSAnchorQueryTypesNone);
CSSValue* ConsumeScrollPadding(CSSParserTokenRange&, const CSSParserContext&);
CSSValue* ConsumeOffsetPath(CSSParserTokenRange&, const CSSParserContext&);
CSSValue* ConsumePathOrNone(CSSParserTokenRange&);
CSSValue* ConsumeOffsetRotate(CSSParserTokenRange&, const CSSParserContext&);
CSSValue* ConsumeInitialLetter(CSSParserTokenRange&, const CSSParserContext&);

CSSValue* ConsumeBasicShape(
    CSSParserTokenRange&,
    const CSSParserContext&,
    AllowPathValue,
    AllowBasicShapeRectValue = AllowBasicShapeRectValue::kForbid,
    AllowBasicShapeXYWHValue = AllowBasicShapeXYWHValue::kForbid);
bool ConsumeRadii(CSSValue* horizontal_radii[4],
                  CSSValue* vertical_radii[4],
                  CSSParserTokenRange&,
                  const CSSParserContext&,
                  bool use_legacy_parsing);

CSSValue* ConsumeTextDecorationLine(CSSParserTokenRange&);

CSSValue* ConsumeToggleGroup(CSSParserTokenRange&, const CSSParserContext&);
CSSValue* ConsumeToggleSpecifier(CSSParserTokenRange&, const CSSParserContext&);
CSSValue* ConsumeToggleTrigger(CSSParserTokenRange&, const CSSParserContext&);

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
CSSValue* ConsumeSVGPaint(CSSParserTokenRange&, const CSSParserContext&);
CSSValue* ParseSpacing(CSSParserTokenRange&, const CSSParserContext&);

CSSValue* ConsumeSingleContainerName(CSSParserTokenRange&,
                                     const CSSParserContext&);
CSSValue* ConsumeContainerName(CSSParserTokenRange&, const CSSParserContext&);
CSSValue* ConsumeContainerType(CSSParserTokenRange&);

UnitlessQuirk UnitlessUnlessShorthand(const CSSParserLocalContext&);

// https://drafts.csswg.org/css-counter-styles-3/#typedef-counter-style-name
CSSCustomIdentValue* ConsumeCounterStyleName(CSSParserTokenRange&,
                                             const CSSParserContext&);
AtomicString ConsumeCounterStyleNameInPrelude(CSSParserTokenRange&,
                                              const CSSParserContext&);

// When parsing a counter style name, it should be ASCII lowercased if it's an
// ASCII case-insensitive match of any predefined counter style name.
bool ShouldLowerCaseCounterStyleNameOnParse(const AtomicString&,
                                            const CSSParserContext&);

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
  return ConsumeLengthOrPercent(range, context,
                                CSSPrimitiveValue::ValueRange::kAll);
}

inline bool AtIdent(const CSSParserToken& token, const char* ident) {
  return token.GetType() == kIdentToken &&
         EqualIgnoringASCIICase(token.Value(), ident);
}

template <typename T>
bool ConsumeIfIdent(T& range_or_stream, const char* ident) {
  if (!AtIdent(range_or_stream.Peek(), ident))
    return false;
  range_or_stream.ConsumeIncludingWhitespace();
  return true;
}

inline bool AtDelimiter(const CSSParserToken& token, UChar c) {
  return token.GetType() == kDelimiterToken && token.Delimiter() == c;
}

template <typename T>
bool ConsumeIfDelimiter(T& range_or_stream, UChar c) {
  if (!AtDelimiter(range_or_stream.Peek(), c))
    return false;
  range_or_stream.ConsumeIncludingWhitespace();
  return true;
}

}  // namespace css_parsing_utils
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PROPERTIES_CSS_PARSING_UTILS_H_
