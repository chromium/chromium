/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2006, 2007, 2010 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/html/html_li_element.h"

#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/layout_tree_builder_traversal.h"
#include "third_party/blink/renderer/core/html/list_item_ordinal.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html_names.h"

namespace blink {

HTMLLIElement::HTMLLIElement(Document& document)
    : HTMLElement(html_names::kLiTag, document) {}

bool HTMLLIElement::IsPresentationAttribute(const QualifiedName& name) const {
  if (name == html_names::kTypeAttr)
    return true;
  return HTMLElement::IsPresentationAttribute(name);
}

CSSValueID ListTypeToCSSValueID(const AtomicString& value) {
  if (value == "a")
    return CSSValueID::kLowerAlpha;
  if (value == "A")
    return CSSValueID::kUpperAlpha;
  if (value == "i")
    return CSSValueID::kLowerRoman;
  if (value == "I")
    return CSSValueID::kUpperRoman;
  if (value == "1")
    return CSSValueID::kDecimal;
  if (DeprecatedEqualIgnoringCase(value, "disc"))
    return CSSValueID::kDisc;
  if (DeprecatedEqualIgnoringCase(value, "circle"))
    return CSSValueID::kCircle;
  if (DeprecatedEqualIgnoringCase(value, "square"))
    return CSSValueID::kSquare;
  if (DeprecatedEqualIgnoringCase(value, "none"))
    return CSSValueID::kNone;
  return CSSValueID::kInvalid;
}

void HTMLLIElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableCSSPropertyValueSet* style) {
  if (name == html_names::kTypeAttr) {
    CSSValueID type_value = ListTypeToCSSValueID(value);
    if (IsValidCSSValueID(type_value)) {
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kListStyleType, type_value);
    }
  } else {
    HTMLElement::CollectStyleForPresentationAttribute(name, value, style);
  }
}

void HTMLLIElement::ParseAttribute(const AttributeModificationParams& params) {
  if (params.name == html_names::kValueAttr) {
    if (ListItemOrdinal* ordinal = ListItemOrdinal::Get(*this))
      ParseValue(params.new_value, ordinal);
  } else {
    HTMLElement::ParseAttribute(params);
  }
}

void HTMLLIElement::AttachLayoutTree(AttachContext& context) {
  HTMLElement::AttachLayoutTree(context);

  if (ListItemOrdinal* ordinal = ListItemOrdinal::Get(*this)) {
    DCHECK(!GetDocument().ChildNeedsDistributionRecalc());

    // Find the enclosing list node.
    Element* list_node = nullptr;
    Element* current = this;
    while (!list_node) {
      current = LayoutTreeBuilderTraversal::ParentElement(*current);
      if (!current)
        break;
      if (IsA<HTMLUListElement>(*current) || IsA<HTMLOListElement>(*current))
        list_node = current;
    }

    // If we are not in a list, tell the layoutObject so it can position us
    // inside.  We don't want to change our style to say "inside" since that
    // would affect nested nodes.
    if (!list_node)
      ordinal->SetNotInList(true, *this);

    ParseValue(FastGetAttribute(html_names::kValueAttr), ordinal);
  }
}

void HTMLLIElement::ParseValue(const AtomicString& value,
                               ListItemOrdinal* ordinal) {
  DCHECK(ListItemOrdinal::IsListItem(*this));

  int requested_value = 0;
  if (ParseHTMLInteger(value, requested_value))
    ordinal->SetExplicitValue(requested_value, *this);
  else
    ordinal->ClearExplicitValue(*this);
}

}  // namespace blink
