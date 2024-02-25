/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2003, 2006, 2010 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/html/html_quote_element.h"

#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"

namespace blink {

HTMLQuoteElement::HTMLQuoteElement(const QualifiedName& tag_name,
                                   Document& document)
    : HTMLElement(tag_name, document) {
  DCHECK(HasTagName(html_names::kQTag) ||
         HasTagName(html_names::kBlockquoteTag));
}

void HTMLQuoteElement::AdjustPseudoStyleLocale(
    ComputedStyleBuilder& pseudo_style_builder) {
  // For quote, pseudo elements should use parent locale. We need to change the
  // pseudo_style before QuoteContentData::CreateLayoutObject, where the
  // computed style is a const. Having the change here ensures correct pseudo
  // locale is rendered after style changes.
  // https://github.com/w3c/csswg-drafts/issues/5478
  FontDescription font_description = pseudo_style_builder.GetFontDescription();
  Element* parent = this->ParentOrShadowHostElement();
  if (parent) {
    font_description.SetLocale(
        LayoutLocale::Get(parent->ComputeInheritedLanguage()));
  } else {
    font_description.SetLocale(&LayoutLocale::GetDefault());
  }
  pseudo_style_builder.SetFontDescription(font_description);
}

bool HTMLQuoteElement::IsURLAttribute(const Attribute& attribute) const {
  return attribute.GetName() == html_names::kCiteAttr ||
         HTMLElement::IsURLAttribute(attribute);
}

bool HTMLQuoteElement::HasLegalLinkAttribute(const QualifiedName& name) const {
  return name == html_names::kCiteAttr ||
         HTMLElement::HasLegalLinkAttribute(name);
}

}  // namespace blink
