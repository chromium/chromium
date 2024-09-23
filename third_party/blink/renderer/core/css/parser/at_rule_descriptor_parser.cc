// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/at_rule_descriptor_parser.h"

#include "third_party/blink/renderer/core/css/css_font_face_src_value.h"
#include "third_party/blink/renderer/core/css/css_string_value.h"
#include "third_party/blink/renderer/core/css/css_unicode_range_value.h"
#include "third_party/blink/renderer/core/css/css_unparsed_declaration_value.h"
#include "third_party/blink/renderer/core/css/css_unset_value.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/css_value_pair.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_mode.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/parser/css_variable_parser.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"

namespace blink {

namespace {

CSSValue* ConsumeFontVariantList(CSSParserTokenStream& stream) {
  CSSValueList* values = CSSValueList::CreateCommaSeparated();
  do {
    if (stream.Peek().Id() == CSSValueID::kAll) {
      // FIXME: CSSPropertyParser::ParseFontVariant() implements
      // the old css3 draft:
      // http://www.w3.org/TR/2002/WD-css3-webfonts-20020802/#font-variant
      // 'all' is only allowed in @font-face and with no other values.
      if (values->length()) {
        return nullptr;
      }
      return css_parsing_utils::ConsumeIdent(stream);
    }
    CSSIdentifierValue* font_variant =
        css_parsing_utils::ConsumeFontVariantCSS21(stream);
    if (font_variant) {
      values->Append(*font_variant);
    }
  } while (css_parsing_utils::ConsumeCommaIncludingWhitespace(stream));

  if (values->length()) {
    return values;
  }

  return nullptr;
}

CSSIdentifierValue* ConsumeFontDisplay(CSSParserTokenStream& stream) {
  return css_parsing_utils::ConsumeIdent<
      CSSValueID::kAuto, CSSValueID::kBlock, CSSValueID::kSwap,
      CSSValueID::kFallback, CSSValueID::kOptional>(stream);
}

CSSValueList* ConsumeFontFaceUnicodeRange(CSSParserTokenStream& stream) {
  CSSValueList* values = CSSValueList::CreateCommaSeparated();

  do {
    CSSParserToken token = stream.Peek();
    if (token.GetType() != kUnicodeRangeToken) {
      return nullptr;
    }
    stream.ConsumeIncludingWhitespace();  // kUnicodeRangeToken

    UChar32 start = token.UnicodeRangeStart();
    UChar32 end = token.UnicodeRangeEnd();
    if (start > end || end > 0x10FFFF) {
      return nullptr;
    }
    values->Append(
        *MakeGarbageCollected<cssvalue::CSSUnicodeRangeValue>(start, end));
  } while (css_parsing_utils::ConsumeCommaIncludingWhitespace(stream));

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
      NOTREACHED_IN_MIGRATION();
      return CSSFontFaceSrcValue::FontTechnology::kTechnologyUnknown;
  }
}

CSSValue* ConsumeFontFaceSrcURI(CSSParserTokenStream& stream,
                                const CSSParserContext& context) {
  cssvalue::CSSURIValue* src_value =
      css_parsing_utils::ConsumeUrl(stream, context);
  if (!src_value) {
    return nullptr;
  }
  auto* uri_value =
      CSSFontFaceSrcValue::Create(src_value, context.JavascriptWorld());

  // After the url() it's either the end of the src: line, or a comma
  // for the next url() or format().
  if (!stream.AtEnd() &&
      stream.Peek().GetType() != CSSParserTokenType::kCommaToken &&
      (stream.Peek().GetType() != CSSParserTokenType::kFunctionToken ||
       (stream.Peek().FunctionId() != CSSValueID::kFormat &&
        stream.Peek().FunctionId() != CSSValueID::kTech))) {
    return nullptr;
  }

  if (stream.Peek().FunctionId() == CSSValueID::kFormat) {
    {
      CSSParserTokenStream::BlockGuard guard(stream);
      stream.ConsumeWhitespace();
      CSSParserTokenType peek_type = stream.Peek().GetType();
      if (peek_type != kIdentToken && peek_type != kStringToken) {
        return nullptr;
      }

      String sanitized_format;

      if (peek_type == kIdentToken) {
        CSSIdentifierValue* font_format =
            css_parsing_utils::ConsumeFontFormatIdent(stream);
        if (!font_format) {
          return nullptr;
        }
        sanitized_format = font_format->CssText();
      }

      if (peek_type == kStringToken) {
        sanitized_format = css_parsing_utils::ConsumeString(stream)->Value();
      }

      if (IsSupportedFontFormat(sanitized_format)) {
        uri_value->SetFormat(sanitized_format);
      } else {
        return nullptr;
      }

      stream.ConsumeWhitespace();

      // After one argument to the format function, there shouldn't be anything
      // else, for example not a comma.
      if (!stream.AtEnd()) {
        return nullptr;
      }
    }
    stream.ConsumeWhitespace();
  }

  if (stream.Peek().FunctionId() == CSSValueID::kTech) {
    {
      CSSParserTokenStream::BlockGuard guard(stream);
      stream.ConsumeWhitespace();

      // One or more tech args expected.
      if (stream.AtEnd()) {
        return nullptr;
      }

      do {
        CSSIdentifierValue* technology_value =
            css_parsing_utils::ConsumeFontTechIdent(stream);
        if (!technology_value) {
          return nullptr;
        }
        if (!stream.AtEnd() &&
            stream.Peek().GetType() != CSSParserTokenType::kCommaToken) {
          return nullptr;
        }
        if (css_parsing_utils::IsSupportedKeywordTech(
                technology_value->GetValueID())) {
          uri_value->AppendTechnology(
              ValueIDToTechnology(technology_value->GetValueID()));
        } else {
          return nullptr;
        }
      } while (css_parsing_utils::ConsumeCommaIncludingWhitespace(stream));
    }
    stream.ConsumeWhitespace();
  }

  return uri_value;
}

CSSValue* ConsumeFontFaceSrcLocal(CSSParserTokenStream& stream,
                                  const CSSParserContext& context) {
  CSSParserTokenStream::BlockGuard guard(stream);
  stream.ConsumeWhitespace();
  if (stream.Peek().GetType() == kStringToken) {
    const CSSParserToken& arg = stream.ConsumeIncludingWhitespace();
    if (!stream.AtEnd()) {
      return nullptr;
    }
    return CSSFontFaceSrcValue::CreateLocal(arg.Value().ToString());
  }
  if (stream.Peek().GetType() == kIdentToken) {
    String family_name = css_parsing_utils::ConcatenateFamilyName(stream);
    if (!stream.AtEnd()) {
      return nullptr;
    }
    if (family_name.empty()) {
      return nullptr;
    }
    return CSSFontFaceSrcValue::CreateLocal(family_name);
  }
  return nullptr;
}

CSSValue* ConsumeFontFaceSrcSkipToComma(
    CSSValue* parse_function(CSSParserTokenStream&, const CSSParserContext&),
    CSSParserTokenStream& stream,
    const CSSParserContext& context) {
  CSSValue* parse_result = parse_function(stream, context);
  stream.ConsumeWhitespace();
  if (parse_result && (stream.AtEnd() || stream.Peek().GetType() ==
                                             CSSParserTokenType::kCommaToken)) {
    return parse_result;
  }

  stream.SkipUntilPeekedTypeIs<CSSParserTokenType::kCommaToken>();
  return nullptr;
}

CSSValueList* ConsumeFontFaceSrc(CSSParserTokenStream& stream,
                                 const CSSParserContext& context) {
  CSSValueList* values = CSSValueList::CreateCommaSeparated();

  stream.ConsumeWhitespace();
  do {
    const CSSParserToken& token = stream.Peek();
    CSSValue* parsed_value = nullptr;
    if (token.FunctionId() == CSSValueID::kLocal) {
      parsed_value = ConsumeFontFaceSrcSkipToComma(ConsumeFontFaceSrcLocal,
                                                   stream, context);
    } else {
      parsed_value =
          ConsumeFontFaceSrcSkipToComma(ConsumeFontFaceSrcURI, stream, context);
    }
    if (parsed_value) {
      values->Append(*parsed_value);
    }
  } while (css_parsing_utils::ConsumeCommaIncludingWhitespace(stream));

  return values->length() ? values : nullptr;
}

CSSValue* ConsumeDescriptor(StyleRule::RuleType rule_type,
                            AtRuleDescriptorID id,
                            CSSParserTokenStream& stream,
                            const CSSParserContext& context) {
  using Parser = AtRuleDescriptorParser;

  switch (rule_type) {
    case StyleRule::kFontFace:
      return Parser::ParseFontFaceDescriptor(id, stream, context);
    case StyleRule::kFontPaletteValues:
      return Parser::ParseAtFontPaletteValuesDescriptor(id, stream, context);
    case StyleRule::kProperty:
      return Parser::ParseAtPropertyDescriptor(id, stream, context);
    case StyleRule::kCounterStyle:
      return Parser::ParseAtCounterStyleDescriptor(id, stream, context);
    case StyleRule::kViewTransition:
      return Parser::ParseAtViewTransitionDescriptor(id, stream, context);
    case StyleRule::kCharset:
    case StyleRule::kContainer:
    case StyleRule::kStyle:
    case StyleRule::kImport:
    case StyleRule::kMedia:
    case StyleRule::kPage:
    case StyleRule::kPageMargin:
    case StyleRule::kKeyframes:
    case StyleRule::kKeyframe:
    case StyleRule::kFontFeatureValues:
    case StyleRule::kFontFeature:
    case StyleRule::kLayerBlock:
    case StyleRule::kLayerStatement:
    case StyleRule::kNestedDeclarations:
    case StyleRule::kNamespace:
    case StyleRule::kScope:
    case StyleRule::kSupports:
    case StyleRule::kStartingStyle:
    case StyleRule::kFunction:
    case StyleRule::kMixin:
    case StyleRule::kApplyMixin:
    case StyleRule::kPositionTry:
      // TODO(andruud): Handle other descriptor types here.
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }
}

CSSValue* ConsumeFontMetricOverride(CSSParserTokenStream& stream,
                                    const CSSParserContext& context) {
  if (CSSIdentifierValue* normal =
          css_parsing_utils::ConsumeIdent<CSSValueID::kNormal>(stream)) {
    return normal;
  }
  return css_parsing_utils::ConsumePercent(
      stream, context, CSSPrimitiveValue::ValueRange::kNonNegative);
}

}  // namespace

CSSValue* AtRuleDescriptorParser::ParseFontFaceDescriptor(
    AtRuleDescriptorID id,
    CSSParserTokenStream& stream,
    const CSSParserContext& context) {
  CSSValue* parsed_value = nullptr;
  stream.ConsumeWhitespace();
  switch (id) {
    case AtRuleDescriptorID::FontFamily:
      // In order to avoid confusion, <family-name> does not accept unquoted
      // <generic-family> keywords and general CSS keywords.
      // ConsumeGenericFamily will take care of excluding the former while the
      // ConsumeFamilyName will take care of excluding the latter.
      // See https://drafts.csswg.org/css-fonts/#family-name-syntax,
      if (css_parsing_utils::ConsumeGenericFamily(stream)) {
        return nullptr;
      }
      parsed_value = css_parsing_utils::ConsumeFamilyName(stream);
      break;
    case AtRuleDescriptorID::Src:  // This is a list of urls or local
                                   // references.
      parsed_value = ConsumeFontFaceSrc(stream, context);
      break;
    case AtRuleDescriptorID::UnicodeRange: {
      CSSParserTokenStream::EnableUnicodeRanges enable(stream, true);
      parsed_value = ConsumeFontFaceUnicodeRange(stream);
      break;
    }
    case AtRuleDescriptorID::FontDisplay:
      parsed_value = ConsumeFontDisplay(stream);
      break;
    case AtRuleDescriptorID::FontStretch: {
      CSSParserContext::ParserModeOverridingScope scope(context,
                                                        kCSSFontFaceRuleMode);
      parsed_value = css_parsing_utils::ConsumeFontStretch(stream, context);
      break;
    }
    case AtRuleDescriptorID::FontStyle: {
      CSSParserContext::ParserModeOverridingScope scope(context,
                                                        kCSSFontFaceRuleMode);
      parsed_value = css_parsing_utils::ConsumeFontStyle(stream, context);
      break;
    }
    case AtRuleDescriptorID::FontVariant:
      parsed_value = ConsumeFontVariantList(stream);
      break;
    case AtRuleDescriptorID::FontWeight: {
      CSSParserContext::ParserModeOverridingScope scope(context,
                                                        kCSSFontFaceRuleMode);
      parsed_value = css_parsing_utils::ConsumeFontWeight(stream, context);
      break;
    }
    case AtRuleDescriptorID::FontFeatureSettings:
      parsed_value =
          css_parsing_utils::ConsumeFontFeatureSettings(stream, context);
      break;
    case AtRuleDescriptorID::AscentOverride:
    case AtRuleDescriptorID::DescentOverride:
    case AtRuleDescriptorID::LineGapOverride:
      parsed_value = ConsumeFontMetricOverride(stream, context);
      break;
    case AtRuleDescriptorID::SizeAdjust:
      parsed_value = css_parsing_utils::ConsumePercent(
          stream, context, CSSPrimitiveValue::ValueRange::kNonNegative);
      break;
    default:
      break;
  }

  if (!parsed_value || !stream.AtEnd()) {
    return nullptr;
  }

  return parsed_value;
}

CSSValue* AtRuleDescriptorParser::ParseFontFaceDescriptor(
    AtRuleDescriptorID id,
    StringView string,
    const CSSParserContext& context) {
  CSSParserTokenStream stream(string);
  return ParseFontFaceDescriptor(id, stream, context);
}

CSSValue* AtRuleDescriptorParser::ParseFontFaceDeclaration(
    CSSParserTokenStream& stream,
    const CSSParserContext& context) {
  DCHECK_EQ(stream.Peek().GetType(), kIdentToken);
  const CSSParserToken& token = stream.ConsumeIncludingWhitespace();
  AtRuleDescriptorID id = token.ParseAsAtRuleDescriptorID();

  if (stream.Consume().GetType() != kColonToken) {
    return nullptr;  // Parse error
  }

  return ParseFontFaceDescriptor(id, stream, context);
}

CSSValue* AtRuleDescriptorParser::ParseAtPropertyDescriptor(
    AtRuleDescriptorID id,
    CSSParserTokenStream& stream,
    const CSSParserContext& context) {
  CSSValue* parsed_value = nullptr;
  switch (id) {
    case AtRuleDescriptorID::Syntax:
      stream.ConsumeWhitespace();
      parsed_value = css_parsing_utils::ConsumeString(stream);
      break;
    case AtRuleDescriptorID::InitialValue: {
      bool important_ignored;
      CSSVariableData* variable_data =
          CSSVariableParser::ConsumeUnparsedDeclaration(
              stream, /*allow_important_annotation=*/false,
              /*is_animation_tainted=*/false,
              /*must_contain_variable_reference=*/false,
              /*restricted_value=*/false, /*comma_ends_declaration=*/false,
              important_ignored, context.GetExecutionContext());
      if (variable_data) {
        return MakeGarbageCollected<CSSUnparsedDeclarationValue>(variable_data,
                                                                 &context);
      } else {
        return nullptr;
      }
    }
    case AtRuleDescriptorID::Inherits:
      stream.ConsumeWhitespace();
      parsed_value =
          css_parsing_utils::ConsumeIdent<CSSValueID::kTrue,
                                          CSSValueID::kFalse>(stream);
      break;
    default:
      break;
  }

  if (!parsed_value || !stream.AtEnd()) {
    stream.SkipUntilPeekedTypeIs();  // For the inspector.
    return nullptr;
  }

  return parsed_value;
}

CSSValue* AtRuleDescriptorParser::ParseAtViewTransitionDescriptor(
    AtRuleDescriptorID id,
    CSSParserTokenStream& stream,
    const CSSParserContext& context) {
  CSSValue* parsed_value = nullptr;
  switch (id) {
    case AtRuleDescriptorID::Navigation:
      stream.ConsumeWhitespace();
      parsed_value =
          css_parsing_utils::ConsumeIdent<CSSValueID::kAuto, CSSValueID::kNone>(
              stream);
      break;
    case AtRuleDescriptorID::Types: {
      CSSValueList* types = CSSValueList::CreateSpaceSeparated();
      parsed_value = types;
      while (!stream.AtEnd()) {
        stream.ConsumeWhitespace();
        if (stream.Peek().Id() == CSSValueID::kNone) {
          return nullptr;
        }
        CSSCustomIdentValue* ident =
            css_parsing_utils::ConsumeCustomIdent(stream, context);
        if (!ident || ident->Value().StartsWith("-ua-")) {
          return nullptr;
        }
        types->Append(*ident);
      }
      break;
    }
    default:
      break;
  }

  if (!parsed_value || !stream.AtEnd()) {
    return nullptr;
  }

  return parsed_value;
}

bool AtRuleDescriptorParser::ParseAtRule(
    StyleRule::RuleType rule_type,
    AtRuleDescriptorID id,
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    HeapVector<CSSPropertyValue, 64>& parsed_descriptors) {
  CSSValue* result = ConsumeDescriptor(rule_type, id, stream, context);

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
