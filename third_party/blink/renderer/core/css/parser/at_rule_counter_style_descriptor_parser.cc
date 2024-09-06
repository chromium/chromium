// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/at_rule_descriptor_parser.h"

#include "third_party/blink/renderer/core/css/css_string_value.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/css_value_pair.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"

namespace blink {

namespace {

CSSValue* ConsumeCounterStyleSymbol(CSSParserTokenStream& stream,
                                    const CSSParserContext& context) {
  // <symbol> = <string> | <image> | <custom-ident>
  if (CSSValue* string = css_parsing_utils::ConsumeString(stream)) {
    return string;
  }
  if (RuntimeEnabledFeatures::CSSAtRuleCounterStyleImageSymbolsEnabled()) {
    if (CSSValue* image = css_parsing_utils::ConsumeImage(stream, context)) {
      return image;
    }
  }
  if (CSSCustomIdentValue* custom_ident =
          css_parsing_utils::ConsumeCustomIdent(stream, context)) {
    return custom_ident;
  }
  return nullptr;
}

CSSValue* ConsumeCounterStyleSystem(CSSParserTokenStream& stream,
                                    const CSSParserContext& context) {
  // Syntax: cyclic | numeric | alphabetic | symbolic | additive |
  // [ fixed <integer>? ] | [ extends <counter-style-name> ]
  if (CSSValue* ident = css_parsing_utils::ConsumeIdent<
          CSSValueID::kCyclic, CSSValueID::kSymbolic, CSSValueID::kAlphabetic,
          CSSValueID::kNumeric, CSSValueID::kAdditive>(stream)) {
    return ident;
  }

  if (CSSValue* ident =
          css_parsing_utils::ConsumeIdent<CSSValueID::kFixed>(stream)) {
    CSSValue* first_symbol_value =
        css_parsing_utils::ConsumeInteger(stream, context);
    if (!first_symbol_value) {
      first_symbol_value = CSSNumericLiteralValue::Create(
          1, CSSPrimitiveValue::UnitType::kInteger);
    }
    return MakeGarbageCollected<CSSValuePair>(
        ident, first_symbol_value, CSSValuePair::kKeepIdenticalValues);
  }

  if (CSSValue* ident =
          css_parsing_utils::ConsumeIdent<CSSValueID::kExtends>(stream)) {
    CSSValue* extended =
        css_parsing_utils::ConsumeCounterStyleName(stream, context);
    if (!extended) {
      return nullptr;
    }
    return MakeGarbageCollected<CSSValuePair>(
        ident, extended, CSSValuePair::kKeepIdenticalValues);
  }

  // Internal keywords for predefined counter styles that use special
  // algorithms. For example, 'simp-chinese-informal'.
  if (context.Mode() == kUASheetMode) {
    if (CSSValue* ident = css_parsing_utils::ConsumeIdent<
            CSSValueID::kInternalHebrew,
            CSSValueID::kInternalSimpChineseInformal,
            CSSValueID::kInternalSimpChineseFormal,
            CSSValueID::kInternalTradChineseInformal,
            CSSValueID::kInternalTradChineseFormal,
            CSSValueID::kInternalKoreanHangulFormal,
            CSSValueID::kInternalKoreanHanjaInformal,
            CSSValueID::kInternalKoreanHanjaFormal,
            CSSValueID::kInternalLowerArmenian,
            CSSValueID::kInternalUpperArmenian,
            CSSValueID::kInternalEthiopicNumeric>(stream)) {
      return ident;
    }
  }

  return nullptr;
}

CSSValue* ConsumeCounterStyleNegative(CSSParserTokenStream& stream,
                                      const CSSParserContext& context) {
  // Syntax: <symbol> <symbol>?
  CSSValue* prepend = ConsumeCounterStyleSymbol(stream, context);
  if (!prepend) {
    return nullptr;
  }
  if (stream.AtEnd()) {
    return prepend;
  }

  CSSValue* append = ConsumeCounterStyleSymbol(stream, context);
  if (!append || !stream.AtEnd()) {
    return nullptr;
  }

  return MakeGarbageCollected<CSSValuePair>(prepend, append,
                                            CSSValuePair::kKeepIdenticalValues);
}

CSSValue* ConsumeCounterStyleRangeBound(CSSParserTokenStream& stream,
                                        const CSSParserContext& context) {
  if (CSSValue* infinite =
          css_parsing_utils::ConsumeIdent<CSSValueID::kInfinite>(stream)) {
    return infinite;
  }
  if (CSSValue* integer = css_parsing_utils::ConsumeInteger(stream, context)) {
    return integer;
  }
  return nullptr;
}

CSSValue* ConsumeCounterStyleRange(CSSParserTokenStream& stream,
                                   const CSSParserContext& context) {
  // Syntax: [ [ <integer> | infinite ]{2} ]# | auto
  if (CSSValue* auto_value =
          css_parsing_utils::ConsumeIdent<CSSValueID::kAuto>(stream)) {
    return auto_value;
  }

  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  do {
    CSSValue* lower_bound = ConsumeCounterStyleRangeBound(stream, context);
    if (!lower_bound) {
      return nullptr;
    }
    CSSValue* upper_bound = ConsumeCounterStyleRangeBound(stream, context);
    if (!upper_bound) {
      return nullptr;
    }

    // If the lower bound of any stream is higher than the upper bound, the
    // entire descriptor is invalid and must be ignored.
    if (lower_bound->IsPrimitiveValue() && upper_bound->IsPrimitiveValue() &&
        To<CSSPrimitiveValue>(lower_bound)->GetIntValue() >
            To<CSSPrimitiveValue>(upper_bound)->GetIntValue()) {
      return nullptr;
    }

    list->Append(*MakeGarbageCollected<CSSValuePair>(
        lower_bound, upper_bound, CSSValuePair::kKeepIdenticalValues));
  } while (css_parsing_utils::ConsumeCommaIncludingWhitespace(stream));
  if (!stream.AtEnd() || !list->length()) {
    return nullptr;
  }
  return list;
}

CSSValue* ConsumeCounterStylePad(CSSParserTokenStream& stream,
                                 const CSSParserContext& context) {
  // Syntax: <integer [0,∞]> && <symbol>
  CSSValue* integer = nullptr;
  CSSValue* symbol = nullptr;
  while (!integer || !symbol) {
    if (!integer) {
      integer = css_parsing_utils::ConsumeInteger(stream, context, 0);
      if (integer) {
        continue;
      }
    }
    if (!symbol) {
      symbol = ConsumeCounterStyleSymbol(stream, context);
      if (symbol) {
        continue;
      }
    }
    return nullptr;
  }
  if (!stream.AtEnd()) {
    return nullptr;
  }

  return MakeGarbageCollected<CSSValuePair>(integer, symbol,
                                            CSSValuePair::kKeepIdenticalValues);
}

CSSValue* ConsumeCounterStyleSymbols(CSSParserTokenStream& stream,
                                     const CSSParserContext& context) {
  // Syntax: <symbol>+
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  while (!stream.AtEnd()) {
    CSSValue* symbol = ConsumeCounterStyleSymbol(stream, context);
    if (!symbol) {
      return nullptr;
    }
    list->Append(*symbol);
  }
  if (!list->length()) {
    return nullptr;
  }
  return list;
}

CSSValue* ConsumeCounterStyleAdditiveSymbols(CSSParserTokenStream& stream,
                                             const CSSParserContext& context) {
  // Syntax: [ <integer [0,∞]> && <symbol> ]#
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  CSSPrimitiveValue* last_integer = nullptr;
  do {
    CSSPrimitiveValue* integer = nullptr;
    CSSValue* symbol = nullptr;
    while (!integer || !symbol) {
      if (!integer) {
        integer = css_parsing_utils::ConsumeInteger(stream, context, 0);
        if (integer) {
          continue;
        }
      }
      if (!symbol) {
        symbol = ConsumeCounterStyleSymbol(stream, context);
        if (symbol) {
          continue;
        }
      }
      return nullptr;
    }

    if (last_integer) {
      // The additive tuples must be specified in order of strictly descending
      // weight; otherwise, the declaration is invalid and must be ignored.
      if (integer->GetIntValue() >= last_integer->GetIntValue()) {
        return nullptr;
      }
    }
    last_integer = integer;

    list->Append(*MakeGarbageCollected<CSSValuePair>(
        integer, symbol, CSSValuePair::kKeepIdenticalValues));
  } while (css_parsing_utils::ConsumeCommaIncludingWhitespace(stream));
  if (!stream.AtEnd() || !list->length()) {
    return nullptr;
  }
  return list;
}

CSSValue* ConsumeCounterStyleSpeakAs(CSSParserTokenStream& stream,
                                     const CSSParserContext& context) {
  // Syntax: auto | bullets | numbers | words | <counter-style-name>
  // We don't support spell-out now.
  if (CSSValue* ident = css_parsing_utils::ConsumeIdent<
          CSSValueID::kAuto, CSSValueID::kBullets, CSSValueID::kNumbers,
          CSSValueID::kWords>(stream)) {
    return ident;
  }
  if (CSSValue* name =
          css_parsing_utils::ConsumeCounterStyleName(stream, context)) {
    return name;
  }
  return nullptr;
}

}  // namespace

CSSValue* AtRuleDescriptorParser::ParseAtCounterStyleDescriptor(
    AtRuleDescriptorID id,
    CSSParserTokenStream& stream,
    const CSSParserContext& context) {
  CSSValue* parsed_value = nullptr;
  switch (id) {
    case AtRuleDescriptorID::System:
      stream.ConsumeWhitespace();
      parsed_value = ConsumeCounterStyleSystem(stream, context);
      break;
    case AtRuleDescriptorID::Negative:
      stream.ConsumeWhitespace();
      parsed_value = ConsumeCounterStyleNegative(stream, context);
      break;
    case AtRuleDescriptorID::Prefix:
    case AtRuleDescriptorID::Suffix:
      stream.ConsumeWhitespace();
      parsed_value = ConsumeCounterStyleSymbol(stream, context);
      break;
    case AtRuleDescriptorID::Range:
      stream.ConsumeWhitespace();
      parsed_value = ConsumeCounterStyleRange(stream, context);
      break;
    case AtRuleDescriptorID::Pad:
      stream.ConsumeWhitespace();
      parsed_value = ConsumeCounterStylePad(stream, context);
      break;
    case AtRuleDescriptorID::Fallback:
      stream.ConsumeWhitespace();
      parsed_value =
          css_parsing_utils::ConsumeCounterStyleName(stream, context);
      break;
    case AtRuleDescriptorID::Symbols:
      stream.ConsumeWhitespace();
      parsed_value = ConsumeCounterStyleSymbols(stream, context);
      break;
    case AtRuleDescriptorID::AdditiveSymbols:
      stream.ConsumeWhitespace();
      parsed_value = ConsumeCounterStyleAdditiveSymbols(stream, context);
      break;
    case AtRuleDescriptorID::SpeakAs:
      stream.ConsumeWhitespace();
      parsed_value = ConsumeCounterStyleSpeakAs(stream, context);
      break;
    default:
      break;
  }

  if (!parsed_value || !stream.AtEnd()) {
    return nullptr;
  }

  return parsed_value;
}

}  // namespace blink
