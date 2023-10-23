// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/at_rule_descriptor_parser.h"

#include "third_party/blink/renderer/core/css/css_custom_property_declaration.h"
#include "third_party/blink/renderer/core/css/css_font_face_src_value.h"
#include "third_party/blink/renderer/core/css/css_string_value.h"
#include "third_party/blink/renderer/core/css/css_unicode_range_value.h"
#include "third_party/blink/renderer/core/css/css_unset_value.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/css_value_pair.h"
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
      if (values->length()) {
        return nullptr;
      }
      return css_parsing_utils::ConsumeIdent(range);
    }
    CSSIdentifierValue* font_variant =
        css_parsing_utils::ConsumeFontVariantCSS21(range);
    if (font_variant) {
      values->Append(*font_variant);
    }
  } while (css_parsing_utils::ConsumeCommaIncludingWhitespace(range));

  if (values->length()) {
    return values;
  }

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
    if (token.GetType() != kUnicodeRangeToken) {
      return nullptr;
    }

    UChar32 start = token.UnicodeRangeStart();
    UChar32 end = token.UnicodeRangeEnd();
    if (start > end) {
      return nullptr;
    }
    values->Append(
        *MakeGarbageCollected<cssvalue::CSSUnicodeRangeValue>(start, end));
  } while (css_parsing_utils::ConsumeCommaIncludingWhitespace(range));

  return values;
}

bool IsSupportedFontFormat(String font_format) {
  return css_parsing_utils::IsSupportedKeywordFormat(
             css_parsing_utils::FontFormatToId(font_format)) ||
         EqualIgnoringASCIICase(font_format, "woff-variations") ||
         EqualIgnoringASCIICase(font_format, "truetype-variations") ||
         EqualIgnoringASCIICase(font_format, "opentype-variations") ||
         EqualIgnoringASCIICase(font_format, "woff2-variations");
}

CSSFontFaceSrcValue::FontTechnology ValueIDToTechnology(CSSValueID valueID) {
  switch (valueID) {
    case CSSValueID::kFeaturesAat:
      return CSSFontFaceSrcValue::FontTechnology::kTechnologyFeaturesAAT;
    case CSSValueID::kFeaturesOpentype:
      return CSSFontFaceSrcValue::FontTechnology::kTechnologyFeaturesOT;
    case CSSValueID::kVariations:
      return CSSFontFaceSrcValue::FontTechnology::kTechnologyVariations;
    case CSSValueID::kPalettes:
      return CSSFontFaceSrcValue::FontTechnology::kTechnologyPalettes;
    case CSSValueID::kColorCOLRv0:
      return CSSFontFaceSrcValue::FontTechnology::kTechnologyCOLRv0;
    case CSSValueID::kColorCOLRv1:
      return CSSFontFaceSrcValue::FontTechnology::kTechnologyCOLRv1;
    case CSSValueID::kColorCBDT:
      return CSSFontFaceSrcValue::FontTechnology::kTechnologyCDBT;
    case CSSValueID::kColorSbix:
      return CSSFontFaceSrcValue::FontTechnology::kTechnologySBIX;
    default:
      NOTREACHED();
      return CSSFontFaceSrcValue::FontTechnology::kTechnologyUnknown;
  }
}

CSSValue* ConsumeFontFaceSrcURI(CSSParserTokenRange& range,
                                const CSSParserContext& context) {
  StringView url_string =
      css_parsing_utils::ConsumeUrlAsStringView(range, context);
  if (url_string.IsNull()) {
    return nullptr;
  }
  AtomicString url = url_string.ToAtomicString();
  CSSFontFaceSrcValue* uri_value(CSSFontFaceSrcValue::Create(
      CSSUrlData(url, context.CompleteNonEmptyURL(url)), context.GetReferrer(),
      context.JavascriptWorld(),
      context.IsOriginClean() ? OriginClean::kTrue : OriginClean::kFalse,
      context.IsAdRelated()));

  // After the url() it's either the end of the src: line, or a comma
  // for the next url() or format().
  if (!range.AtEnd() &&
      range.Peek().GetType() != CSSParserTokenType::kCommaToken &&
      (range.Peek().GetType() != CSSParserTokenType::kFunctionToken ||
       (range.Peek().FunctionId() != CSSValueID::kFormat &&
        range.Peek().FunctionId() != CSSValueID::kTech))) {
    return nullptr;
  }

  if (range.Peek().FunctionId() == CSSValueID::kFormat) {
    CSSParserTokenRange format_args = css_parsing_utils::ConsumeFunction(range);
    CSSParserTokenType peek_type = format_args.Peek().GetType();
    if (peek_type != kIdentToken && peek_type != kStringToken) {
      return nullptr;
    }

    String sanitized_format;

    if (peek_type == kIdentToken) {
      CSSIdentifierValue* font_format =
          css_parsing_utils::ConsumeFontFormatIdent(format_args);
      if (!font_format) {
        return nullptr;
      }
      sanitized_format = font_format->CssText();
    }

    if (peek_type == kStringToken) {
      sanitized_format = css_parsing_utils::ConsumeString(format_args)->Value();
    }

    if (IsSupportedFontFormat(sanitized_format)) {
      uri_value->SetFormat(sanitized_format);
    } else {
      return nullptr;
    }

    format_args.ConsumeWhitespace();

    // After one argument to the format function, there shouldn't be anything
    // else, for example not a comma.
    if (!format_args.AtEnd()) {
      return nullptr;
    }
  }

  if (range.Peek().FunctionId() == CSSValueID::kTech) {
    CSSParserTokenRange tech_args = css_parsing_utils::ConsumeFunction(range);

    // One or more tech args expected.
    if (tech_args.AtEnd()) {
      return nullptr;
    }

    do {
      CSSIdentifierValue* technology_value =
          css_parsing_utils::ConsumeFontTechIdent(tech_args);
      if (!technology_value) {
        return nullptr;
      }
      if (!tech_args.AtEnd() &&
          tech_args.Peek().GetType() != CSSParserTokenType::kCommaToken) {
        return nullptr;
      }
      if (css_parsing_utils::IsSupportedKeywordTech(
              technology_value->GetValueID())) {
        uri_value->AppendTechnology(
            ValueIDToTechnology(technology_value->GetValueID()));
      } else {
        return nullptr;
      }
    } while (css_parsing_utils::ConsumeCommaIncludingWhitespace(tech_args));
  }

  return uri_value;
}

CSSValue* ConsumeFontFaceSrcLocal(CSSParserTokenRange& range,
                                  const CSSParserContext& context) {
  CSSParserTokenRange args = css_parsing_utils::ConsumeFunction(range);
  if (args.Peek().GetType() == kStringToken) {
    const CSSParserToken& arg = args.ConsumeIncludingWhitespace();
    if (!args.AtEnd()) {
      return nullptr;
    }
    return CSSFontFaceSrcValue::CreateLocal(
        arg.Value().ToString(), context.JavascriptWorld(),
        context.IsOriginClean() ? OriginClean::kTrue : OriginClean::kFalse,
        context.IsAdRelated());
  }
  if (args.Peek().GetType() == kIdentToken) {
    String family_name = css_parsing_utils::ConcatenateFamilyName(args);
    if (!args.AtEnd()) {
      return nullptr;
    }
    if (family_name.empty()) {
      return nullptr;
    }
    return CSSFontFaceSrcValue::CreateLocal(
        family_name, context.JavascriptWorld(),
        context.IsOriginClean() ? OriginClean::kTrue : OriginClean::kFalse,
        context.IsAdRelated());
  }
  return nullptr;
}

CSSValue* ConsumeFontFaceSrcSkipToComma(
    CSSValue* parse_function(CSSParserTokenRange&, const CSSParserContext&),
    CSSParserTokenRange& range,
    const CSSParserContext& context) {
  CSSValue* parse_result = parse_function(range, context);
  range.ConsumeWhitespace();
  if (parse_result && (range.AtEnd() || range.Peek().GetType() ==
                                            CSSParserTokenType::kCommaToken)) {
    return parse_result;
  }

  while (!range.AtEnd() &&
         range.Peek().GetType() != CSSParserTokenType::kCommaToken) {
    range.Consume();
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
    if (token.FunctionId() == CSSValueID::kLocal) {
      parsed_value = ConsumeFontFaceSrcSkipToComma(ConsumeFontFaceSrcLocal,
                                                   range, context);
    } else {
      parsed_value =
          ConsumeFontFaceSrcSkipToComma(ConsumeFontFaceSrcURI, range, context);
    }
    if (parsed_value) {
      values->Append(*parsed_value);
    }
  } while (css_parsing_utils::ConsumeCommaIncludingWhitespace(range));

  return values->length() ? values : nullptr;
}

CSSValue* ConsumeDescriptor(StyleRule::RuleType rule_type,
                            AtRuleDescriptorID id,
                            const CSSTokenizedValue& tokenized_value,
                            const CSSParserContext& context) {
  using Parser = AtRuleDescriptorParser;
  CSSParserTokenRange range = tokenized_value.range;

  switch (rule_type) {
    case StyleRule::kFontFace:
      return Parser::ParseFontFaceDescriptor(id, tokenized_value, context);
    case StyleRule::kFontPaletteValues:
      return Parser::ParseAtFontPaletteValuesDescriptor(id, range, context);
    case StyleRule::kProperty:
      return Parser::ParseAtPropertyDescriptor(id, tokenized_value, context);
    case StyleRule::kCounterStyle:
      return Parser::ParseAtCounterStyleDescriptor(id, range, context);
    case StyleRule::kViewTransitions:
      return Parser::ParseAtViewTransitionsDescriptor(id, range, context);
    case StyleRule::kCharset:
    case StyleRule::kContainer:
    case StyleRule::kStyle:
    case StyleRule::kImport:
    case StyleRule::kMedia:
    case StyleRule::kPage:
    case StyleRule::kKeyframes:
    case StyleRule::kKeyframe:
    case StyleRule::kFontFeatureValues:
    case StyleRule::kFontFeature:
    case StyleRule::kLayerBlock:
    case StyleRule::kLayerStatement:
    case StyleRule::kNamespace:
    case StyleRule::kScope:
    case StyleRule::kSupports:
    case StyleRule::kPositionFallback:
    case StyleRule::kTry:
    case StyleRule::kStartingStyle:
      // TODO(andruud): Handle other descriptor types here.
      NOTREACHED();
      return nullptr;
  }
}

CSSValue* ConsumeFontMetricOverride(CSSParserTokenRange& range,
                                    const CSSParserContext& context) {
  if (CSSIdentifierValue* normal =
          css_parsing_utils::ConsumeIdent<CSSValueID::kNormal>(range)) {
    return normal;
  }
  return css_parsing_utils::ConsumePercent(
      range, context, CSSPrimitiveValue::ValueRange::kNonNegative);
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
      // In order to avoid confusion, <family-name> does not accept unquoted
      // <generic-family> keywords and general CSS keywords.
      // ConsumeGenericFamily will take care of excluding the former while the
      // ConsumeFamilyName will take care of excluding the latter.
      // See https://drafts.csswg.org/css-fonts/#family-name-syntax,
      if (css_parsing_utils::ConsumeGenericFamily(range)) {
        return nullptr;
      }
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
    case AtRuleDescriptorID::SizeAdjust:
      parsed_value = css_parsing_utils::ConsumePercent(
          range, context, CSSPrimitiveValue::ValueRange::kNonNegative);
      break;
    default:
      break;
  }

  if (!parsed_value || !range.AtEnd()) {
    return nullptr;
  }

  return parsed_value;
}

CSSValue* AtRuleDescriptorParser::ParseFontFaceDescriptor(
    AtRuleDescriptorID id,
    const String& string,
    const CSSParserContext& context) {
  CSSTokenizer tokenizer(string);
  Vector<CSSParserToken, 32> tokens;

  if (id == AtRuleDescriptorID::UnicodeRange) {
    tokens = tokenizer.TokenizeToEOFWithUnicodeRanges();
  } else {
    tokens = tokenizer.TokenizeToEOF();
  }
  CSSParserTokenRange range = CSSParserTokenRange(tokens);
  return ParseFontFaceDescriptor(id, range, context);
}

CSSValue* AtRuleDescriptorParser::ParseFontFaceDescriptor(
    AtRuleDescriptorID id,
    const CSSTokenizedValue& tokenized_value,
    const CSSParserContext& context) {
  CSSParserTokenRange range = tokenized_value.range;

  if (id == AtRuleDescriptorID::UnicodeRange) {
    CSSTokenizer tokenizer(tokenized_value.text);
    Vector<CSSParserToken, 32> tokens =
        tokenizer.TokenizeToEOFWithUnicodeRanges();
    range = CSSParserTokenRange(tokens);
    return ParseFontFaceDescriptor(id, range, context);
  }
  return ParseFontFaceDescriptor(id, range, context);
}

CSSValue* AtRuleDescriptorParser::ParseFontFaceDeclaration(
    CSSParserTokenRange& range,
    const CSSParserContext& context) {
  DCHECK_EQ(range.Peek().GetType(), kIdentToken);
  const CSSParserToken& token = range.ConsumeIncludingWhitespace();
  AtRuleDescriptorID id = token.ParseAsAtRuleDescriptorID();

  if (range.Consume().GetType() != kColonToken) {
    return nullptr;  // Parse error
  }

  return ParseFontFaceDescriptor(id, range, context);
}

CSSValue* AtRuleDescriptorParser::ParseAtPropertyDescriptor(
    AtRuleDescriptorID id,
    const CSSTokenizedValue& tokenized_value,
    const CSSParserContext& context) {
  CSSValue* parsed_value = nullptr;
  CSSParserTokenRange range = tokenized_value.range;
  switch (id) {
    case AtRuleDescriptorID::Syntax:
      range.ConsumeWhitespace();
      parsed_value = css_parsing_utils::ConsumeString(range);
      break;
    case AtRuleDescriptorID::InitialValue: {
      // Note that we must retain leading whitespace here.
      return CSSVariableParser::ParseDeclarationValue(
          tokenized_value, false /* is_animation_tainted */, context);
    }
    case AtRuleDescriptorID::Inherits:
      range.ConsumeWhitespace();
      parsed_value = css_parsing_utils::ConsumeIdent<CSSValueID::kTrue,
                                                     CSSValueID::kFalse>(range);
      break;
    default:
      break;
  }

  if (!parsed_value || !range.AtEnd()) {
    return nullptr;
  }

  return parsed_value;
}

CSSValue* AtRuleDescriptorParser::ParseAtViewTransitionsDescriptor(
    AtRuleDescriptorID id,
    CSSParserTokenRange& range,
    const CSSParserContext& context) {
  CSSValue* parsed_value = nullptr;
  switch (id) {
    case AtRuleDescriptorID::NavigationTrigger:
      range.ConsumeWhitespace();
      parsed_value =
          css_parsing_utils::ConsumeIdent<CSSValueID::kCrossDocumentSameOrigin,
                                          CSSValueID::kNone>(range);
      break;
    default:
      break;
  }

  if (!parsed_value || !range.AtEnd()) {
    return nullptr;
  }

  return parsed_value;
}

bool AtRuleDescriptorParser::ParseAtRule(
    StyleRule::RuleType rule_type,
    AtRuleDescriptorID id,
    const CSSTokenizedValue& tokenized_value,
    const CSSParserContext& context,
    HeapVector<CSSPropertyValue, 64>& parsed_descriptors) {
  CSSValue* result = ConsumeDescriptor(rule_type, id, tokenized_value, context);

  if (!result) {
    return false;
  }
  // Convert to CSSPropertyID for legacy compatibility,
  // TODO(crbug.com/752745): Refactor CSSParserImpl to avoid using
  // the CSSPropertyID.
  CSSPropertyID equivalent_property_id = AtRuleDescriptorIDAsCSSPropertyID(id);
  parsed_descriptors.push_back(
      CSSPropertyValue(CSSPropertyName(equivalent_property_id), *result));
  context.Count(context.Mode(), equivalent_property_id);
  return true;
}

}  // namespace blink
