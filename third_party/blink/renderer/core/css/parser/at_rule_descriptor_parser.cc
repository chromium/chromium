// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/at_rule_descriptor_parser.h"

#include "third_party/blink/renderer/core/css/css_custom_property_declaration.h"
#include "third_party/blink/renderer/core/css/css_font_face_src_value.h"
#include "third_party/blink/renderer/core/css/css_id_selector_value.h"
#include "third_party/blink/renderer/core/css/css_string_value.h"
#include "third_party/blink/renderer/core/css/css_unicode_range_value.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_mode.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_range.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/parser/css_variable_parser.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"

namespace blink {

namespace {

CSSValue* ConsumeFontVariantList(CSSParserTokenRange& range) {
  CSSValueList* values = CSSValueList::CreateCommaSeparated();
  do {
    if (range.Peek().Id() == CSSValueID::kAll) {
      // FIXME: CSSPropertyParser::ParseFontVariant() implements
      // the old css3 draft:
      // http://www.w3.org/TR/2002/WD-css3-webfonts-20020802/#font-variant
      // 'all' is only allowed in @font-face and with no other values.
      if (values->length())
        return nullptr;
      return css_parsing_utils::ConsumeIdent(range);
    }
    CSSIdentifierValue* font_variant =
        css_parsing_utils::ConsumeFontVariantCSS21(range);
    if (font_variant)
      values->Append(*font_variant);
  } while (css_parsing_utils::ConsumeCommaIncludingWhitespace(range));

  if (values->length())
    return values;

  return nullptr;
}

CSSIdentifierValue* ConsumeFontDisplay(CSSParserTokenRange& range) {
  return css_parsing_utils::ConsumeIdent<
      CSSValueID::kAuto, CSSValueID::kBlock, CSSValueID::kSwap,
      CSSValueID::kFallback, CSSValueID::kOptional>(range);
}

CSSValueList* ConsumeFontFaceUnicodeRange(CSSParserTokenRange& range) {
  CSSValueList* values = CSSValueList::CreateCommaSeparated();

  do {
    const CSSParserToken& token = range.ConsumeIncludingWhitespace();
    if (token.GetType() != kUnicodeRangeToken)
      return nullptr;

    UChar32 start = token.UnicodeRangeStart();
    UChar32 end = token.UnicodeRangeEnd();
    if (start > end)
      return nullptr;
    values->Append(
        *MakeGarbageCollected<cssvalue::CSSUnicodeRangeValue>(start, end));
  } while (css_parsing_utils::ConsumeCommaIncludingWhitespace(range));

  return values;
}

CSSValue* ConsumeFontFaceSrcURI(CSSParserTokenRange& range,
                                const CSSParserContext& context) {
  String url =
      css_parsing_utils::ConsumeUrlAsStringView(range, context).ToString();
  if (url.IsNull())
    return nullptr;
  CSSFontFaceSrcValue* uri_value(CSSFontFaceSrcValue::Create(
      url, context.CompleteURL(url), context.GetReferrer(),
      context.JavascriptWorld(),
      context.IsOriginClean() ? OriginClean::kTrue : OriginClean::kFalse,
      context.IsAdRelated()));

  if (range.Peek().FunctionId() != CSSValueID::kFormat)
    return uri_value;

  // FIXME: https://drafts.csswg.org/css-fonts says that format() contains a
  // comma-separated list of strings, but CSSFontFaceSrcValue stores only one
  // format. Allowing one format for now.
  CSSParserTokenRange args = css_parsing_utils::ConsumeFunction(range);
  const CSSParserToken& arg = args.ConsumeIncludingWhitespace();
  if ((arg.GetType() != kStringToken) || !args.AtEnd())
    return nullptr;
  uri_value->SetFormat(arg.Value().ToString());
  return uri_value;
}

CSSValue* ConsumeFontFaceSrcLocal(CSSParserTokenRange& range,
                                  const CSSParserContext& context) {
  CSSParserTokenRange args = css_parsing_utils::ConsumeFunction(range);
  if (args.Peek().GetType() == kStringToken) {
    const CSSParserToken& arg = args.ConsumeIncludingWhitespace();
    if (!args.AtEnd())
      return nullptr;
    return CSSFontFaceSrcValue::CreateLocal(
        arg.Value().ToString(), context.JavascriptWorld(),
        context.IsOriginClean() ? OriginClean::kTrue : OriginClean::kFalse,
        context.IsAdRelated());
  }
  if (args.Peek().GetType() == kIdentToken) {
    String family_name = css_parsing_utils::ConcatenateFamilyName(args);
    if (!args.AtEnd())
      return nullptr;
    return CSSFontFaceSrcValue::CreateLocal(
        family_name, context.JavascriptWorld(),
        context.IsOriginClean() ? OriginClean::kTrue : OriginClean::kFalse,
        context.IsAdRelated());
  }
  return nullptr;
}

CSSValueList* ConsumeFontFaceSrc(CSSParserTokenRange& range,
                                 const CSSParserContext& context) {
  CSSValueList* values = CSSValueList::CreateCommaSeparated();

  range.ConsumeWhitespace();
  do {
    const CSSParserToken& token = range.Peek();
    CSSValue* parsed_value = nullptr;
    if (token.FunctionId() == CSSValueID::kLocal)
      parsed_value = ConsumeFontFaceSrcLocal(range, context);
    else
      parsed_value = ConsumeFontFaceSrcURI(range, context);
    if (!parsed_value)
      return nullptr;
    values->Append(*parsed_value);
  } while (css_parsing_utils::ConsumeCommaIncludingWhitespace(range));
  return values;
}

CSSValue* ConsumeScrollTimelineSource(CSSParserTokenRange& range) {
  if (auto* selector_function =
          css_parsing_utils::ConsumeSelectorFunction(range)) {
    return selector_function;
  }
  return css_parsing_utils::ConsumeIdent<CSSValueID::kAuto, CSSValueID::kNone>(
      range);
}

CSSValue* ConsumeScrollTimelineOrientation(CSSParserTokenRange& range) {
  return css_parsing_utils::ConsumeIdent<
      CSSValueID::kAuto, CSSValueID::kBlock, CSSValueID::kInline,
      CSSValueID::kHorizontal, CSSValueID::kVertical>(range);
}

CSSValue* ConsumeTimeRange(CSSParserTokenRange& range,
                           const CSSParserContext& context) {
  if (auto* value = css_parsing_utils::ConsumeIdent<CSSValueID::kAuto>(range))
    return value;
  return css_parsing_utils::ConsumeTime(range, context, kValueRangeAll);
}

CSSValue* ConsumeDescriptor(StyleRule::RuleType rule_type,
                            AtRuleDescriptorID id,
                            CSSParserTokenRange& range,
                            const CSSParserContext& context) {
  using Parser = AtRuleDescriptorParser;

  switch (rule_type) {
    case StyleRule::kFontFace:
      return Parser::ParseFontFaceDescriptor(id, range, context);
    case StyleRule::kProperty:
      return Parser::ParseAtPropertyDescriptor(id, range, context);
    case StyleRule::kScrollTimeline:
      return Parser::ParseAtScrollTimelineDescriptor(id, range, context);
    case StyleRule::kCharset:
    case StyleRule::kStyle:
    case StyleRule::kImport:
    case StyleRule::kMedia:
    case StyleRule::kPage:
    case StyleRule::kKeyframes:
    case StyleRule::kKeyframe:
    case StyleRule::kNamespace:
    case StyleRule::kSupports:
    case StyleRule::kViewport:
      // TODO(andruud): Handle other descriptor types here.
      NOTREACHED();
      return nullptr;
  }
}

CSSValue* ConsumeFontMetricOverride(CSSParserTokenRange& range,
                                    const CSSParserContext& context) {
  if (!RuntimeEnabledFeatures::CSSFontMetricsOverrideEnabled())
    return nullptr;
  if (CSSIdentifierValue* normal =
          css_parsing_utils::ConsumeIdent<CSSValueID::kNormal>(range)) {
    return normal;
  }
  return css_parsing_utils::ConsumePercent(range, context,
                                           kValueRangeNonNegative);
}

CSSValue* ConsumeAdvanceOverride(CSSParserTokenRange& range,
                                 const CSSParserContext& context) {
  if (!RuntimeEnabledFeatures::CSSFontMetricsOverrideEnabled())
    return nullptr;
  return css_parsing_utils::ConsumeNumber(range, context, kValueRangeAll);
}

}  // namespace

CSSValue* AtRuleDescriptorParser::ParseFontFaceDescriptor(
    AtRuleDescriptorID id,
    CSSParserTokenRange& range,
    const CSSParserContext& context) {
  CSSValue* parsed_value = nullptr;
  range.ConsumeWhitespace();
  switch (id) {
    case AtRuleDescriptorID::FontFamily:
      if (css_parsing_utils::ConsumeGenericFamily(range))
        return nullptr;
      parsed_value = css_parsing_utils::ConsumeFamilyName(range);
      break;
    case AtRuleDescriptorID::Src:  // This is a list of urls or local
                                   // references.
      parsed_value = ConsumeFontFaceSrc(range, context);
      break;
    case AtRuleDescriptorID::UnicodeRange:
      parsed_value = ConsumeFontFaceUnicodeRange(range);
      break;
    case AtRuleDescriptorID::FontDisplay:
      parsed_value = ConsumeFontDisplay(range);
      break;
    case AtRuleDescriptorID::FontStretch: {
      CSSParserContext::ParserModeOverridingScope scope(context,
                                                        kCSSFontFaceRuleMode);
      parsed_value = css_parsing_utils::ConsumeFontStretch(range, context);
      break;
    }
    case AtRuleDescriptorID::FontStyle: {
      CSSParserContext::ParserModeOverridingScope scope(context,
                                                        kCSSFontFaceRuleMode);
      parsed_value = css_parsing_utils::ConsumeFontStyle(range, context);
      break;
    }
    case AtRuleDescriptorID::FontVariant:
      parsed_value = ConsumeFontVariantList(range);
      break;
    case AtRuleDescriptorID::FontWeight: {
      CSSParserContext::ParserModeOverridingScope scope(context,
                                                        kCSSFontFaceRuleMode);
      parsed_value = css_parsing_utils::ConsumeFontWeight(range, context);
      break;
    }
    case AtRuleDescriptorID::FontFeatureSettings:
      parsed_value =
          css_parsing_utils::ConsumeFontFeatureSettings(range, context);
      break;
    case AtRuleDescriptorID::AscentOverride:
    case AtRuleDescriptorID::DescentOverride:
    case AtRuleDescriptorID::LineGapOverride:
      parsed_value = ConsumeFontMetricOverride(range, context);
      break;
    case AtRuleDescriptorID::AdvanceOverride:
      parsed_value = ConsumeAdvanceOverride(range, context);
      break;
    default:
      break;
  }

  if (!parsed_value || !range.AtEnd())
    return nullptr;

  return parsed_value;
}

CSSValue* AtRuleDescriptorParser::ParseFontFaceDescriptor(
    AtRuleDescriptorID id,
    const String& string,
    const CSSParserContext& context) {
  CSSTokenizer tokenizer(string);
  Vector<CSSParserToken, 32> tokens = tokenizer.TokenizeToEOF();
  CSSParserTokenRange range = CSSParserTokenRange(tokens);
  return ParseFontFaceDescriptor(id, range, context);
}

CSSValue* AtRuleDescriptorParser::ParseFontFaceDeclaration(
    CSSParserTokenRange& range,
    const CSSParserContext& context) {
  DCHECK_EQ(range.Peek().GetType(), kIdentToken);
  const CSSParserToken& token = range.ConsumeIncludingWhitespace();
  AtRuleDescriptorID id = token.ParseAsAtRuleDescriptorID();

  if (range.Consume().GetType() != kColonToken)
    return nullptr;  // Parse error

  return ParseFontFaceDescriptor(id, range, context);
}

CSSValue* AtRuleDescriptorParser::ParseAtPropertyDescriptor(
    AtRuleDescriptorID id,
    CSSParserTokenRange& range,
    const CSSParserContext& context) {
  CSSValue* parsed_value = nullptr;
  switch (id) {
    case AtRuleDescriptorID::Syntax:
      range.ConsumeWhitespace();
      parsed_value = css_parsing_utils::ConsumeString(range);
      break;
    case AtRuleDescriptorID::InitialValue: {
      // Note that we must retain leading whitespace here.
      return CSSVariableParser::ParseDeclarationValue(
          g_null_atom, range, false /* is_animation_tainted */, context);
    }
    case AtRuleDescriptorID::Inherits:
      range.ConsumeWhitespace();
      parsed_value = css_parsing_utils::ConsumeIdent<CSSValueID::kTrue,
                                                     CSSValueID::kFalse>(range);
      break;
    default:
      break;
  }

  if (!parsed_value || !range.AtEnd())
    return nullptr;

  return parsed_value;
}

CSSValue* AtRuleDescriptorParser::ParseAtScrollTimelineDescriptor(
    AtRuleDescriptorID id,
    CSSParserTokenRange& range,
    const CSSParserContext& context) {
  CSSValue* parsed_value = nullptr;

  range.ConsumeWhitespace();

  switch (id) {
    case AtRuleDescriptorID::Source:
      parsed_value = ConsumeScrollTimelineSource(range);
      break;
    case AtRuleDescriptorID::Orientation:
      parsed_value = ConsumeScrollTimelineOrientation(range);
      break;
    case AtRuleDescriptorID::Start:
    case AtRuleDescriptorID::End:
      parsed_value = css_parsing_utils::ConsumeScrollOffset(range, context);
      break;
    case AtRuleDescriptorID::TimeRange:
      parsed_value = ConsumeTimeRange(range, context);
      break;
    default:
      break;
  }

  if (!parsed_value || !range.AtEnd())
    return nullptr;

  return parsed_value;
}

bool AtRuleDescriptorParser::ParseAtRule(
    StyleRule::RuleType rule_type,
    AtRuleDescriptorID id,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    HeapVector<CSSPropertyValue, 256>& parsed_descriptors) {
  CSSValue* result = ConsumeDescriptor(rule_type, id, range, context);

  if (!result)
    return false;
  // Convert to CSSPropertyID for legacy compatibility,
  // TODO(crbug.com/752745): Refactor CSSParserImpl to avoid using
  // the CSSPropertyID.
  CSSPropertyID equivalent_property_id = AtRuleDescriptorIDAsCSSPropertyID(id);
  parsed_descriptors.push_back(
      CSSPropertyValue(CSSPropertyName(equivalent_property_id), *result));
  return true;
}

}  // namespace blink
