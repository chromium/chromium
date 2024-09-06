// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/at_rule_descriptor_parser.h"

#include "third_party/blink/renderer/core/css/css_string_value.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/css_value_pair.h"
#include "third_party/blink/renderer/core/css/parser/at_rule_descriptors.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"

namespace blink {

namespace {

CSSValue* ConsumeFontFamily(CSSParserTokenStream& stream,
                            const CSSParserContext& context) {
  return css_parsing_utils::ConsumeNonGenericFamilyNameList(stream);
}

CSSValue* ConsumeBasePalette(CSSParserTokenStream& stream,
                             const CSSParserContext& context) {
  if (CSSValue* ident =
          css_parsing_utils::ConsumeIdent<CSSValueID::kLight,
                                          CSSValueID::kDark>(stream)) {
    return ident;
  }

  return css_parsing_utils::ConsumeInteger(stream, context, 0);
}

CSSValue* ConsumeColorOverride(CSSParserTokenStream& stream,
                               const CSSParserContext& context) {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  do {
    CSSValue* color_index =
        css_parsing_utils::ConsumeInteger(stream, context, 0);
    if (!color_index) {
      return nullptr;
    }
    stream.ConsumeWhitespace();
    CSSValue* color = css_parsing_utils::ConsumeAbsoluteColor(stream, context);
    if (!color) {
      return nullptr;
    }
    CSSIdentifierValue* color_identifier = DynamicTo<CSSIdentifierValue>(color);
    if (color_identifier &&
        color_identifier->GetValueID() == CSSValueID::kCurrentcolor) {
      return nullptr;
    }
    list->Append(*MakeGarbageCollected<CSSValuePair>(
        color_index, color, CSSValuePair::kKeepIdenticalValues));
  } while (css_parsing_utils::ConsumeCommaIncludingWhitespace(stream));
  if (!stream.AtEnd() || !list->length()) {
    return nullptr;
  }

  return list;
}

}  // namespace

CSSValue* AtRuleDescriptorParser::ParseAtFontPaletteValuesDescriptor(
    AtRuleDescriptorID id,
    CSSParserTokenStream& stream,
    const CSSParserContext& context) {
  CSSValue* parsed_value = nullptr;

  switch (id) {
    case AtRuleDescriptorID::FontFamily:
      stream.ConsumeWhitespace();
      parsed_value = ConsumeFontFamily(stream, context);
      break;
    case AtRuleDescriptorID::BasePalette:
      stream.ConsumeWhitespace();
      parsed_value = ConsumeBasePalette(stream, context);
      break;
    case AtRuleDescriptorID::OverrideColors:
      stream.ConsumeWhitespace();
      parsed_value = ConsumeColorOverride(stream, context);
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
