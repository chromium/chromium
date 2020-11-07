// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/at_rule_descriptor_parser.h"

#include "third_party/blink/renderer/core/css/css_string_value.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_range.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"

namespace blink {

namespace {

CSSValue* ConsumeCounterStyleName(CSSParserTokenRange& range) {
  // TODO(crbug.com/687225): Implement.
  return nullptr;
}

CSSValue* ConsumeCounterStyleSymbol(CSSParserTokenRange& range) {
  // TODO(crbug.com/687225): Implement.
  return nullptr;
}

CSSValue* ConsumeCounterStyleSystem(CSSParserTokenRange& range) {
  // TODO(crbug.com/687225): Implement.
  return nullptr;
}

CSSValue* ConsumeCounterStyleNegative(CSSParserTokenRange& range) {
  // TODO(crbug.com/687225): Implement.
  return nullptr;
}

CSSValue* ConsumeCounterStyleRange(CSSParserTokenRange& range) {
  // TODO(crbug.com/687225): Implement.
  return nullptr;
}

CSSValue* ConsumeCounterStylePad(CSSParserTokenRange& range) {
  // TODO(crbug.com/687225): Implement.
  return nullptr;
}

CSSValue* ConsumeCounterStyleSymbols(CSSParserTokenRange& range) {
  // TODO(crbug.com/687225): Implement.
  return nullptr;
}

CSSValue* ConsumeCounterStyleAdditiveSymbols(CSSParserTokenRange& range) {
  // TODO(crbug.com/687225): Implement.
  return nullptr;
}

CSSValue* ConsumeCounterStyleSpeakAs(CSSParserTokenRange& range) {
  // TODO(crbug.com/687225): Implement.
  return nullptr;
}

}  // namespace

CSSValue* AtRuleDescriptorParser::ParseAtCounterStyleDescriptor(
    AtRuleDescriptorID id,
    CSSParserTokenRange& range,
    const CSSParserContext& context) {
  DCHECK(RuntimeEnabledFeatures::CSSAtRuleCounterStyleEnabled());

  CSSValue* parsed_value = nullptr;
  switch (id) {
    case AtRuleDescriptorID::System:
      range.ConsumeWhitespace();
      parsed_value = ConsumeCounterStyleSystem(range);
      break;
    case AtRuleDescriptorID::Negative:
      range.ConsumeWhitespace();
      parsed_value = ConsumeCounterStyleNegative(range);
      break;
    case AtRuleDescriptorID::Prefix:
    case AtRuleDescriptorID::Suffix:
      range.ConsumeWhitespace();
      parsed_value = ConsumeCounterStyleSymbol(range);
      break;
    case AtRuleDescriptorID::Range:
      range.ConsumeWhitespace();
      parsed_value = ConsumeCounterStyleRange(range);
      break;
    case AtRuleDescriptorID::Pad:
      range.ConsumeWhitespace();
      parsed_value = ConsumeCounterStylePad(range);
      break;
    case AtRuleDescriptorID::Fallback:
      range.ConsumeWhitespace();
      parsed_value = ConsumeCounterStyleName(range);
      break;
    case AtRuleDescriptorID::Symbols:
      range.ConsumeWhitespace();
      parsed_value = ConsumeCounterStyleSymbols(range);
      break;
    case AtRuleDescriptorID::AdditiveSymbols:
      range.ConsumeWhitespace();
      parsed_value = ConsumeCounterStyleAdditiveSymbols(range);
      break;
    case AtRuleDescriptorID::SpeakAs:
      range.ConsumeWhitespace();
      parsed_value = ConsumeCounterStyleSpeakAs(range);
      break;
    default:
      break;
  }

  if (!parsed_value || !range.AtEnd())
    return nullptr;

  return parsed_value;
}

}  // namespace blink
