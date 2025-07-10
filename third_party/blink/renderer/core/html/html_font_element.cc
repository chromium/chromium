/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2003, 2006, 2008, 2010 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/html/html_font_element.h"

#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/css_value_pool.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/wtf/text/character_visitor.h"
#include "third_party/blink/renderer/platform/wtf/text/parsing_utilities.h"
#include "third_party/blink/renderer/platform/wtf/text/string_to_number.h"

namespace blink {

HTMLFontElement::HTMLFontElement(Document& document)
    : HTMLElement(html_names::kFontTag, document) {}

// http://www.whatwg.org/specs/web-apps/current-work/multipage/rendering.html#fonts-and-colors
template <typename CharacterType>
static std::optional<int> ParseFontSize(
    base::span<const CharacterType> characters) {
  // Step 1
  // Step 2
  // Step 3
  size_t position = SkipWhile<CharacterType, IsHTMLSpace>(characters, 0);

  // Step 4
  if (position == characters.size()) {
    return std::nullopt;
  }
  DCHECK_LT(position, characters.size());

  // Step 5
  enum { kRelativePlus, kRelativeMinus, kAbsolute } mode;

  switch (characters[position]) {
    case '+':
      mode = kRelativePlus;
      ++position;
      break;
    case '-':
      mode = kRelativeMinus;
      ++position;
      break;
    default:
      mode = kAbsolute;
      break;
  }

  // Step 6
  const size_t digits_start = position;
  position = SkipWhile<CharacterType, IsASCIIDigit>(characters, position);

  // Step 7
  if (digits_start == position)
    return std::nullopt;

  // Step 8
  int value = CharactersToInt(
      characters.subspan(digits_start,
                         static_cast<size_t>(position - digits_start)),
      NumberParsingOptions(), nullptr);

  // Step 9
  if (mode == kRelativePlus) {
    value = base::CheckAdd(value, 3).ValueOrDefault(value);
  } else if (mode == kRelativeMinus) {
    value = base::CheckSub(3, value).ValueOrDefault(value);
  }

  // Step 10
  if (value > 7)
    value = 7;

  // Step 11
  if (value < 1)
    value = 1;

  return value;
}

static std::optional<int> ParseFontSize(const String& input) {
  if (input.empty()) {
    return std::nullopt;
  }
  return VisitCharacters(input,
                         [](auto chars) { return ParseFontSize(chars); });
}

static const CSSValueList* CreateFontFaceValueWithPool(
    const AtomicString& string,
    SecureContextMode secure_context_mode) {
  CSSValuePool::FontFaceValueCache::AddResult entry =
      CssValuePool().GetFontFaceCacheEntry(string);
  if (!entry.stored_value->value) {
    const CSSValue* parsed_value = CSSParser::ParseSingleValue(
        CSSPropertyID::kFontFamily, string,
        StrictCSSParserContext(secure_context_mode));
    if (auto* parsed_value_list = DynamicTo<CSSValueList>(parsed_value))
      entry.stored_value->value = parsed_value_list;
  }
  return entry.stored_value->value.Get();
}

std::optional<CSSValueID> HTMLFontElement::CssValueFromFontSizeNumber(
    const String& s) {
  std::optional<int> num = ParseFontSize(s);
  if (!num) {
    return std::nullopt;
  }
  switch (*num) {
    case 1:
      // FIXME: The spec says that we're supposed to use CSSValueID::kXxSmall
      // here.
      return CSSValueID::kXSmall;
    case 2:
      return CSSValueID::kSmall;
    case 3:
      return CSSValueID::kMedium;
    case 4:
      return CSSValueID::kLarge;
    case 5:
      return CSSValueID::kXLarge;
    case 6:
      return CSSValueID::kXxLarge;
    case 7:
      return CSSValueID::kXxxLarge;
    default:
      NOTREACHED();
  }
}

bool HTMLFontElement::IsPresentationAttribute(const QualifiedName& name) const {
  if (name == html_names::kSizeAttr || name == html_names::kColorAttr ||
      name == html_names::kFaceAttr)
    return true;
  return HTMLElement::IsPresentationAttribute(name);
}

void HTMLFontElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    HeapVector<CSSPropertyValue, 8>& style) {
  if (name == html_names::kSizeAttr) {
    if (std::optional<CSSValueID> size_keyword =
            CssValueFromFontSizeNumber(value)) {
      AddPropertyToPresentationAttributeStyle(style, CSSPropertyID::kFontSize,
                                              *size_keyword);
    }
  } else if (name == html_names::kColorAttr) {
    AddHTMLColorToStyle(style, CSSPropertyID::kColor, value);
  } else if (name == html_names::kFaceAttr && !value.empty()) {
    if (const CSSValueList* font_face_value = CreateFontFaceValueWithPool(
            value, GetExecutionContext()->GetSecureContextMode())) {
      style.emplace_back(CSSPropertyName(CSSPropertyID::kFontFamily),
                         *font_face_value);
    }
  } else {
    HTMLElement::CollectStyleForPresentationAttribute(name, value, style);
  }
}

}  // namespace blink
