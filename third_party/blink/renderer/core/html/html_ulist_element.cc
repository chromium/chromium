/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 *
 */

#include "third_party/blink/renderer/core/html/html_ulist_element.h"

#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/keywords.h"

namespace blink {

HTMLUListElement::HTMLUListElement(Document& document)
    : HTMLElement(html_names::kUlTag, document) {}

bool HTMLUListElement::IsPresentationAttribute(
    const QualifiedName& name) const {
  if (name == html_names::kTypeAttr)
    return true;
  return HTMLElement::IsPresentationAttribute(name);
}

void HTMLUListElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableCSSPropertyValueSet* style) {
  if (name == html_names::kTypeAttr) {
    if (EqualIgnoringASCIICase(value, keywords::kDisc)) {
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kListStyleType,
          *MakeGarbageCollected<CSSCustomIdentValue>(keywords::kDisc));
    } else if (EqualIgnoringASCIICase(value, keywords::kCircle)) {
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kListStyleType,
          *MakeGarbageCollected<CSSCustomIdentValue>(keywords::kCircle));
    } else if (EqualIgnoringASCIICase(value, keywords::kSquare)) {
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kListStyleType,
          *MakeGarbageCollected<CSSCustomIdentValue>(keywords::kSquare));
    } else if (EqualIgnoringASCIICase(value, "none")) {
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kListStyleType, CSSValueID::kNone);
    }
  } else {
    HTMLElement::CollectStyleForPresentationAttribute(name, value, style);
  }
}

}  // namespace blink
