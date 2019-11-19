// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_PROPERTY_PARSER_HELPERS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_PROPERTY_PARSER_HELPERS_H_

#include "base/optional.h"
#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"
#include "third_party/blink/renderer/core/css/css_function_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_mode.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_range.h"
#include "third_party/blink/renderer/core/frame/web_feature_forward.h"
#include "third_party/blink/renderer/platform/geometry/length.h"  // For ValueRange
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class CSSParserContext;
class CSSPropertyValue;
class CSSStringValue;
class CSSValuePair;
class StylePropertyShorthand;

namespace cssvalue {

class CSSURIValue;

}

// When these functions are successful, they will consume all the relevant
// tokens from the range and also consume any whitespace which follows. When
// the start of the range doesn't match the type we're looking for, the range
// will not be modified.
namespace css_property_parser_helpers {

void Complete4Sides(CSSValue* side[4]);

// TODO(timloh): These should probably just be consumeComma and consumeSlash.
bool ConsumeCommaIncludingWhitespace(CSSParserTokenRange&);
bool ConsumeSlashIncludingWhitespace(CSSParserTokenRange&);
// consumeFunction expects the range starts with a FunctionToken.
CSSParserTokenRange ConsumeFunction(CSSParserTokenRange&);

enum class UnitlessQuirk { kAllow, kForbid };

CSSPrimitiveValue* ConsumeInteger(
    CSSParserTokenRange&,
    double minimum_value = -std::numeric_limits<double>::max());
CSSPrimitiveValue* ConsumeIntegerOrNumberCalc(CSSParserTokenRange&);
CSSPrimitiveValue* ConsumePositiveInteger(CSSParserTokenRange&);
bool ConsumeNumberRaw(CSSParserTokenRange&, double& result);
CSSPrimitiveValue* ConsumeNumber(CSSParserTokenRange&, ValueRange);
CSSPrimitiveValue* ConsumeLength(CSSParserTokenRange&,
                                 CSSParserMode,
                                 ValueRange,
                                 UnitlessQuirk = UnitlessQuirk::kForbid);
CSSPrimitiveValue* ConsumePercent(CSSParserTokenRange&, ValueRange);
CSSPrimitiveValue* ConsumeAlphaValue(CSSParserTokenRange&);
CSSPrimitiveValue* ConsumeLengthOrPercent(
    CSSParserTokenRange&,
    CSSParserMode,
    ValueRange,
    UnitlessQuirk = UnitlessQuirk::kForbid);
CSSPrimitiveValue* ConsumeSVGGeometryPropertyLength(CSSParserTokenRange&,
                                                    const CSSParserContext&,
                                                    ValueRange);

CSSPrimitiveValue* ConsumeAngle(
    CSSParserTokenRange&,
    const CSSParserContext*,
    base::Optional<WebFeature> unitless_zero_feature);
CSSPrimitiveValue* ConsumeAngle(
    CSSParserTokenRange&,
    const CSSParserContext*,
    base::Optional<WebFeature> unitless_zero_feature,
    double minimum_value,
    double maximum_value);
CSSPrimitiveValue* ConsumeTime(CSSParserTokenRange&, ValueRange);
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
                                  const CSSParserContext*);
cssvalue::CSSURIValue* ConsumeUrl(CSSParserTokenRange&,
                                  const CSSParserContext*);

CSSValue* ConsumeColor(CSSParserTokenRange&,
                       CSSParserMode,
                       bool accept_quirky_colors = false);

CSSValue* ConsumeLineWidth(CSSParserTokenRange&, CSSParserMode, UnitlessQuirk);

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
                                   CSSParserMode,
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
    const CSSParserContext*,
    ConsumeGeneratedImagePolicy = ConsumeGeneratedImagePolicy::kAllow);
CSSValue* ConsumeImageOrNone(CSSParserTokenRange&, const CSSParserContext*);

CSSValue* ConsumeAxis(CSSParserTokenRange&);

bool IsCSSWideKeyword(StringView);
bool IsRevertKeyword(StringView);
bool IsDefaultKeyword(StringView);

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

// Template implementations are at the bottom of the file for readability.

template <typename... emptyBaseCase>
inline bool IdentMatches(CSSValueID id) {
  return false;
}
template <CSSValueID head, CSSValueID... tail>
inline bool IdentMatches(CSSValueID id) {
  return id == head || IdentMatches<tail...>(id);
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

CSSValue* ConsumeTransformValue(CSSParserTokenRange&, const CSSParserContext&);
CSSValue* ConsumeTransformList(CSSParserTokenRange&, const CSSParserContext&);
CSSValue* ConsumeFilterFunctionList(CSSParserTokenRange&,
                                    const CSSParserContext&);

}  // namespace css_property_parser_helpers

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_PROPERTY_PARSER_HELPERS_H_
