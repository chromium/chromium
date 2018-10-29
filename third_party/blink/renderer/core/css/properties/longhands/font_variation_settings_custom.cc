// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/font_variation_settings.h"

#include "third_party/blink/renderer/core/css/css_font_variation_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {
namespace {

cssvalue::CSSFontVariationValue* ConsumeFontVariationTag(
    CSSParserTokenRange& range) {
  // Feature tag name consists of 4-letter characters.
  static const wtf_size_t kTagNameLength = 4;

  const CSSParserToken& token = range.ConsumeIncludingWhitespace();
  // Feature tag name comes first
  if (token.GetType() != kStringToken)
    return nullptr;
  if (token.Value().length() != kTagNameLength)
    return nullptr;
  AtomicString tag = token.Value().ToAtomicString();
  for (wtf_size_t i = 0; i < kTagNameLength; ++i) {
    // Limits the range of characters to 0x20-0x7E, following the tag name rules
    // defined in the OpenType specification.
    UChar character = tag[i];
    if (character < 0x20 || character > 0x7E)
      return nullptr;
  }

  double tag_value = 0;
  if (!CSSPropertyParserHelpers::ConsumeNumberRaw(range, tag_value))
    return nullptr;
  return cssvalue::CSSFontVariationValue::Create(tag,
                                                 clampTo<float>(tag_value));
}

}  // namespace
namespace CSSLonghand {

const CSSValue* FontVariationSettings::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueNormal)
    return CSSPropertyParserHelpers::ConsumeIdent(range);
  CSSValueList* variation_settings = CSSValueList::CreateCommaSeparated();
  do {
    cssvalue::CSSFontVariationValue* font_variation_value =
        ConsumeFontVariationTag(range);
    if (!font_variation_value)
      return nullptr;
    variation_settings->Append(*font_variation_value);
  } while (CSSPropertyParserHelpers::ConsumeCommaIncludingWhitespace(range));
  return variation_settings;
}

const CSSValue* FontVariationSettings::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node* styled_node,
    bool allow_visited_style) const {
  const blink::FontVariationSettings* variation_settings =
      style.GetFontDescription().VariationSettings();
  if (!variation_settings || !variation_settings->size())
    return CSSIdentifierValue::Create(CSSValueNormal);
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  for (wtf_size_t i = 0; i < variation_settings->size(); ++i) {
    const FontVariationAxis& variation_axis = variation_settings->at(i);
    cssvalue::CSSFontVariationValue* variation_value =
        cssvalue::CSSFontVariationValue::Create(variation_axis.Tag(),
                                                variation_axis.Value());
    list->Append(*variation_value);
  }
  return list;
}

}  // namespace CSSLonghand
}  // namespace blink
