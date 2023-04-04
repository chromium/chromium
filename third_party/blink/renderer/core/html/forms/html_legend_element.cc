/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2010 Apple Inc. All rights reserved.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
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

#include "third_party/blink/renderer/core/html/forms/html_legend_element.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/forms/html_field_set_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

HTMLLegendElement::HTMLLegendElement(Document& document)
    : HTMLElement(html_names::kLegendTag, document) {}

HTMLFormElement* HTMLLegendElement::form() const {
  // According to the specification, If the legend has a fieldset element as
  // its parent, then the form attribute must return the same value as the
  // form attribute on that fieldset element. Otherwise, it must return null.
  if (auto* fieldset = DynamicTo<HTMLFieldSetElement>(parentNode()))
    return fieldset->formOwner();
  return nullptr;
}

void HTMLLegendElement::DetachLayoutTree(bool performing_reattach) {
  LayoutObject* object = GetLayoutObject();
  if (!performing_reattach && object && object->IsRenderedLegend())
    object->Parent()->GetNode()->SetForceReattachLayoutTree();
  HTMLElement::DetachLayoutTree(performing_reattach);
}

LayoutObject* HTMLLegendElement::CreateLayoutObject(
    const ComputedStyle& style) {
  // Count text-align property which does not mapped from 'align' content
  // attribute. See crbug.com/880822 and |HTMLElement::
  // CollectStyleForPresentationAttribute()|.
  bool should_count;
  const AtomicString& align_value =
      FastGetAttribute(html_names::kAlignAttr).LowerASCII();
  switch (style.GetTextAlign()) {
    case ETextAlign::kLeft:
      should_count = align_value != "left";
      break;
    case ETextAlign::kRight:
      should_count = align_value != "right";
      break;
    case ETextAlign::kCenter:
      should_count = (align_value != "center" && align_value != "middle");
      break;
    default:
      should_count = (align_value == "left" || align_value == "right" ||
                      align_value == "center" || align_value == "middle");
      break;
  }
  if (should_count)
    UseCounter::Count(GetDocument(), WebFeature::kTextAlignSpecifiedToLegend);

  return HTMLElement::CreateLayoutObject(style);
}

}  // namespace blink
