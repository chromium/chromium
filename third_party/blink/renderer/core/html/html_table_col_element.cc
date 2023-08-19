/*
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2010 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/html/html_table_col_element.h"

#include <algorithm>
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/html/html_table_cell_element.h"
#include "third_party/blink/renderer/core/html/html_table_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html/table_constants.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"

namespace blink {

HTMLTableColElement::HTMLTableColElement(const QualifiedName& tag_name,
                                         Document& document)
    : HTMLTablePartElement(tag_name, document), span_(kDefaultColSpan) {}

bool HTMLTableColElement::IsPresentationAttribute(
    const QualifiedName& name) const {
  if (name == html_names::kWidthAttr)
    return true;
  return HTMLTablePartElement::IsPresentationAttribute(name);
}

void HTMLTableColElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableCSSPropertyValueSet* style) {
  if (name == html_names::kWidthAttr)
    AddHTMLLengthToStyle(style, CSSPropertyID::kWidth, value);
  else
    HTMLTablePartElement::CollectStyleForPresentationAttribute(name, value,
                                                               style);
}

void HTMLTableColElement::ParseAttribute(
    const AttributeModificationParams& params) {
  if (params.name == html_names::kSpanAttr) {
    unsigned new_span = 0;
    if (!ParseHTMLClampedNonNegativeInteger(params.new_value, kMinColSpan,
                                            kMaxColSpan, new_span)) {
      new_span = kDefaultColSpan;
    }
    span_ = new_span;
    if (GetLayoutObject() && GetLayoutObject()->IsLayoutTableCol())
      GetLayoutObject()->UpdateFromElement();
  } else if (params.name == html_names::kWidthAttr) {
    if (!params.new_value.empty()) {
      if (GetLayoutObject() && GetLayoutObject()->IsLayoutTableCol()) {
        auto* col = To<LayoutBox>(GetLayoutObject());
        int new_width = Width().ToInt();
        if (new_width != col->Size().width) {
          col->SetNeedsLayoutAndIntrinsicWidthsRecalcAndFullPaintInvalidation(
              layout_invalidation_reason::kAttributeChanged);
        }
      }
    }
  } else {
    HTMLTablePartElement::ParseAttribute(params);
  }
}

const CSSPropertyValueSet*
HTMLTableColElement::AdditionalPresentationAttributeStyle() {
  if (!HasTagName(html_names::kColgroupTag))
    return nullptr;
  if (HTMLTableElement* table = FindParentTable())
    return table->AdditionalGroupStyle(false);
  return nullptr;
}

void HTMLTableColElement::setSpan(unsigned n) {
  SetUnsignedIntegralAttribute(html_names::kSpanAttr, n, kDefaultColSpan);
}

const AtomicString& HTMLTableColElement::Width() const {
  return FastGetAttribute(html_names::kWidthAttr);
}

}  // namespace blink
