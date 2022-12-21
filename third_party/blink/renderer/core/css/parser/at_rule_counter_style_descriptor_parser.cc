// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/at_rule_descriptor_parser.h"

#include "third_party/blink/renderer/core/css/css_string_value.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/css_value_pair.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_range.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"

namespace blink {

namespace {

CSSValue* ConsumeCounterStyleSymbol(CSSParserTokenRange& range,
                                    const CSSParserContext& context) {
  // <symbol> = <string> | <image> | <custom-ident>
  if (CSSValue* string = css_parsing_utils::ConsumeString(range)) {
    return string;
  }
  if (RuntimeEnabledFeatures::CSSAtRuleCounterStyleImageSymbolsEnabled()) {
    if (CSSValue* image = css_parsing_utils::ConsumeImage(range, context)) {
      return image;
    }
  }
  if (CSSCustomIdentValue* custom_ident =
          css_parsing_utils::ConsumeCustomIdent(range, context)) {
    return custom_ident;
  }
  return nullptr;
}

CSSValue* ConsumeCounterStyleSystem(CSSParserTokenRange& range,
                                    const CSSParserContext& context) {
  // Syntax: cyclic | numeric | alphabetic | symbolic | additive |
  // [ fixed <integer>? ] | [ extends <counter-style-name> ]
  if (CSSValue* ident = css_parsing_utils::ConsumeIdent<
          CSSValueID::kCyclic, CSSValueID::kSymbolic, CSSValueID::kAlphabetic,
          CSSValueID::kNumeric, CSSValueID::kAdditive>(range)) {
    return ident;
  }

  if (CSSValue* ident =
          css_parsing_utils::ConsumeIdent<CSSValueID::kFixed>(range)) {
    CSSValue* first_symbol_value =
        css_parsing_utils::ConsumeInteger(range, context);
    if (!first_symbol_value) {
      first_symbol_value = CSSNumericLiteralValue::Create(
          1, CSSPrimitiveValue::UnitType::kInteger);
    }
    return MakeGarbageCollected<CSSValuePair>(
        ident, first_symbol_value, CSSValuePair::kKeepIdenticalValues);
  }

  if (CSSValue* ident =
          css_parsing_utils::ConsumeIdent<CSSValueID::kExtends>(range)) {
    CSSValue* extended =
        css_parsing_utils::ConsumeCounterStyleName(range, context);
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
            CSSValueID::kInternalEthiopicNumeric>(range)) {
      return ident;
    }
  }

  return nullptr;
}

CSSValue* ConsumeCounterStyleNegative(CSSParserTokenRange& range,
                                      const CSSParserContext& context) {
  // Syntax: <symbol> <symbol>?
  CSSValue* prepend = ConsumeCounterStyleSymbol(range, context);
  if (!prepend) {
    return nullptr;
  }
  if (range.AtEnd()) {
    return prepend;
  }

  CSSValue* append = ConsumeCounterStyleSymbol(range, context);
  if (!append || !range.AtEnd()) {
    return nullptr;
  }

  return MakeGarbageCollected<CSSValuePair>(prepend, append,
                                            CSSValuePair::kKeepIdenticalValues);
}

CSSValue* ConsumeCounterStyleRangeBound(CSSParserTokenRange& range,
                                        const CSSParserContext& context) {
  if (CSSValue* infinite =
          css_parsing_utils::ConsumeIdent<CSSValueID::kInfinite>(range)) {
    return infinite;
  }
  if (CSSValue* integer = css_parsing_utils::ConsumeInteger(range, context)) {
    return integer;
  }
  return nullptr;
}

CSSValue* ConsumeCounterStyleRange(CSSParserTokenRange& range,
                                   const CSSParserContext& context) {
  // Syntax: [ [ <integer> | infinite ]{2} ]# | auto
  if (CSSValue* auto_value =
          css_parsing_utils::ConsumeIdent<CSSValueID::kAuto>(range)) {
    return auto_value;
  }

  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  do {
    CSSValue* lower_bound = ConsumeCounterStyleRangeBound(range, context);
    if (!lower_bound) {
      return nullptr;
    }
    CSSValue* upper_bound = ConsumeCounterStyleRangeBound(range, context);
    if (!upper_bound) {
      return nullptr;
    }

    // If the lower bound of any range is higher than the upper bound, the
    // entire descriptor is invalid and must be ignored.
    if (lower_bound->IsPrimitiveValue() && upper_bound->IsPrimitiveValue() &&
        To<CSSPrimitiveValue>(lower_bound)->GetIntValue() >
            To<CSSPrimitiveValue>(upper_bound)->GetIntValue()) {
      return nullptr;
    }

    list->Append(*MakeGarbageCollected<CSSValuePair>(
        lower_bound, upper_bound, CSSValuePair::kKeepIdenticalValues));
  } while (css_parsing_utils::ConsumeCommaIncludingWhitespace(range));
  if (!range.AtEnd() || !list->length()) {
    return nullptr;
  }
  return list;
}

CSSValue* ConsumeCounterStylePad(CSSParserTokenRange& range,
                                 const CSSParserContext& context) {
  // Syntax: <integer [0,∞]> && <symbol>
  CSSValue* integer = nullptr;
  CSSValue* symbol = nullptr;
  while (!integer || !symbol) {
    if (!integer) {
      integer = css_parsing_utils::ConsumeInteger(range, context, 0);
      if (integer) {
        continue;
      }
    }
    if (!symbol) {
      symbol = ConsumeCounterStyleSymbol(range, context);
      if (symbol) {
        continue;
      }
    }
    return nullptr;
  }
  if (!range.AtEnd()) {
    return nullptr;
  }

  return MakeGarbageCollected<CSSValuePair>(integer, symbol,
                                            CSSValuePair::kKeepIdenticalValues);
}

CSSValue* ConsumeCounterStyleSymbols(CSSParserTokenRange& range,
                                     const CSSParserContext& context) {
  // Syntax: <symbol>+
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  while (!range.AtEnd()) {
    CSSValue* symbol = ConsumeCounterStyleSymbol(range, context);
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

CSSValue* ConsumeCounterStyleAdditiveSymbols(CSSParserTokenRange& range,
                                             const CSSParserContext& context) {
  // Syntax: [ <integer [0,∞]> && <symbol> ]#
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  CSSPrimitiveValue* last_integer = nullptr;
  do {
    CSSPrimitiveValue* integer = nullptr;
    CSSValue* symbol = nullptr;
    while (!integer || !symbol) {
      if (!integer) {
        integer = css_parsing_utils::ConsumeInteger(range, context, 0);
        if (integer) {
          continue;
        }
      }
      if (!symbol) {
        symbol = ConsumeCounterStyleSymbol(range, context);
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
  } while (css_parsing_utils::ConsumeCommaIncludingWhitespace(range));
  if (!range.AtEnd() || !list->length()) {
    return nullptr;
  }
  return list;
}

CSSValue* ConsumeCounterStyleSpeakAs(CSSParserTokenRange& range,
                                     const CSSParserContext& context) {
  // Syntax: auto | bullets | numbers | words | <counter-style-name>
  // We don't support spell-out now.
  if (CSSValue* ident = css_parsing_utils::ConsumeIdent<
          CSSValueID::kAuto, CSSValueID::kBullets, CSSValueID::kNumbers,
          CSSValueID::kWords>(range)) {
    return ident;
  }
  if (CSSValue* name =
          css_parsing_utils::ConsumeCounterStyleName(range, context)) {
    return name;
  }
  return nullptr;
}

}  // namespace

CSSValue* AtRuleDescriptorParser::ParseAtCounterStyleDescriptor(
    AtRuleDescriptorID id,
    CSSParserTokenRange& range,
    const CSSParserContext& context) {
  CSSValue* parsed_value = nullptr;
  switch (id) {
    case AtRuleDescriptorID::System:
      range.ConsumeWhitespace();
      parsed_value = ConsumeCounterStyleSystem(range, context);
      break;
    case AtRuleDescriptorID::Negative:
      range.ConsumeWhitespace();
      parsed_value = ConsumeCounterStyleNegative(range, context);
      break;
    case AtRuleDescriptorID::Prefix:
    case AtRuleDescriptorID::Suffix:
      range.ConsumeWhitespace();
      parsed_value = ConsumeCounterStyleSymbol(range, context);
      break;
    case AtRuleDescriptorID::Range:
      range.ConsumeWhitespace();
      parsed_value = ConsumeCounterStyleRange(range, context);
      break;
    case AtRuleDescriptorID::Pad:
      range.ConsumeWhitespace();
      parsed_value = ConsumeCounterStylePad(range, context);
      break;
    case AtRuleDescriptorID::Fallback:
      range.ConsumeWhitespace();
      parsed_value = css_parsing_utils::ConsumeCounterStyleName(range, context);
      break;
    case AtRuleDescriptorID::Symbols:
      range.ConsumeWhitespace();
      parsed_value = ConsumeCounterStyleSymbols(range, context);
      break;
    case AtRuleDescriptorID::AdditiveSymbols:
      range.ConsumeWhitespace();
      parsed_value = ConsumeCounterStyleAdditiveSymbols(range, context);
      break;
    case AtRuleDescriptorID::SpeakAs:
      range.ConsumeWhitespace();
      parsed_value = ConsumeCounterStyleSpeakAs(range, context);
      break;
    default:
      break;
  }

  if (!parsed_value || !range.AtEnd()) {
    return nullptr;
  }

  return parsed_value;
}

}  // namespace blink
