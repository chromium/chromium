/**
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.
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

#include "third_party/blink/renderer/core/html/html_table_part_element.h"

#include "third_party/blink/renderer/core/css/css_image_value.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/html_table_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/weborigin/referrer.h"

namespace blink {

bool HTMLTablePartElement::IsPresentationAttribute(
    const QualifiedName& name) const {
  if (name == html_names::kBgcolorAttr || name == html_names::kBackgroundAttr ||
      name == html_names::kValignAttr || name == html_names::kAlignAttr ||
      name == html_names::kHeightAttr)
    return true;
  return HTMLElement::IsPresentationAttribute(name);
}

void HTMLTablePartElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableCSSPropertyValueSet* style) {
  if (name == html_names::kBgcolorAttr) {
    AddHTMLColorToStyle(style, CSSPropertyID::kBackgroundColor, value);
  } else if (name == html_names::kBackgroundAttr) {
    String url = StripLeadingAndTrailingHTMLSpaces(value);
    if (!url.IsEmpty()) {
      UseCounter::Count(
          GetDocument(),
          WebFeature::kHTMLTableElementPresentationAttributeBackground);
      CSSImageValue* image_value =
          CSSImageValue::Create(url, GetDocument().CompleteURL(url),
                                Referrer(GetDocument().OutgoingReferrer(),
                                         GetDocument().GetReferrerPolicy()),
                                OriginClean::kTrue);
      style->SetProperty(
          CSSPropertyValue(GetCSSPropertyBackgroundImage(), *image_value));
    }
  } else if (name == html_names::kValignAttr) {
    if (DeprecatedEqualIgnoringCase(value, "top")) {
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kVerticalAlign, CSSValueID::kTop);
    } else if (DeprecatedEqualIgnoringCase(value, "middle")) {
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kVerticalAlign, CSSValueID::kMiddle);
    } else if (DeprecatedEqualIgnoringCase(value, "bottom")) {
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kVerticalAlign, CSSValueID::kBottom);
    } else if (DeprecatedEqualIgnoringCase(value, "baseline")) {
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kVerticalAlign, CSSValueID::kBaseline);
    } else {
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kVerticalAlign, value);
    }
  } else if (name == html_names::kAlignAttr) {
    if (DeprecatedEqualIgnoringCase(value, "middle") ||
        DeprecatedEqualIgnoringCase(value, "center")) {
      AddPropertyToPresentationAttributeStyle(style, CSSPropertyID::kTextAlign,
                                              CSSValueID::kWebkitCenter);
    } else if (DeprecatedEqualIgnoringCase(value, "absmiddle")) {
      AddPropertyToPresentationAttributeStyle(style, CSSPropertyID::kTextAlign,
                                              CSSValueID::kCenter);
    } else if (DeprecatedEqualIgnoringCase(value, "left")) {
      AddPropertyToPresentationAttributeStyle(style, CSSPropertyID::kTextAlign,
                                              CSSValueID::kWebkitLeft);
    } else if (DeprecatedEqualIgnoringCase(value, "right")) {
      AddPropertyToPresentationAttributeStyle(style, CSSPropertyID::kTextAlign,
                                              CSSValueID::kWebkitRight);
    } else {
      AddPropertyToPresentationAttributeStyle(style, CSSPropertyID::kTextAlign,
                                              value);
    }
  } else if (name == html_names::kHeightAttr) {
    if (!value.IsEmpty())
      AddHTMLLengthToStyle(style, CSSPropertyID::kHeight, value);
  } else {
    HTMLElement::CollectStyleForPresentationAttribute(name, value, style);
  }
}

HTMLTableElement* HTMLTablePartElement::FindParentTable() const {
  ContainerNode* parent = FlatTreeTraversal::Parent(*this);
  while (parent && !IsA<HTMLTableElement>(*parent))
    parent = FlatTreeTraversal::Parent(*parent);
  return To<HTMLTableElement>(parent);
}

}  // namespace blink
