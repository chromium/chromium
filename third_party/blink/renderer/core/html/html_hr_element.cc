/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2010 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/html/html_hr_element.h"

#include "third_party/blink/renderer/core/css/css_color_value.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/html/forms/html_opt_group_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html_names.h"

namespace blink {

HTMLHRElement::HTMLHRElement(Document& document)
    : HTMLElement(html_names::kHrTag, document) {}

bool HTMLHRElement::IsPresentationAttribute(const QualifiedName& name) const {
  if (name == html_names::kAlignAttr || name == html_names::kWidthAttr ||
      name == html_names::kColorAttr || name == html_names::kNoshadeAttr ||
      name == html_names::kSizeAttr)
    return true;
  return HTMLElement::IsPresentationAttribute(name);
}

void HTMLHRElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableCSSPropertyValueSet* style) {
  if (name == html_names::kAlignAttr) {
    if (DeprecatedEqualIgnoringCase(value, "left")) {
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kMarginLeft, 0,
          CSSPrimitiveValue::UnitType::kPixels);
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kMarginRight, CSSValueID::kAuto);
    } else if (DeprecatedEqualIgnoringCase(value, "right")) {
      AddPropertyToPresentationAttributeStyle(style, CSSPropertyID::kMarginLeft,
                                              CSSValueID::kAuto);
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kMarginRight, 0,
          CSSPrimitiveValue::UnitType::kPixels);
    } else {
      AddPropertyToPresentationAttributeStyle(style, CSSPropertyID::kMarginLeft,
                                              CSSValueID::kAuto);
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kMarginRight, CSSValueID::kAuto);
    }
  } else if (name == html_names::kWidthAttr) {
    bool ok;
    int v = value.ToInt(&ok);
    if (ok && !v) {
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kWidth, 1,
          CSSPrimitiveValue::UnitType::kPixels);
    } else {
      AddHTMLLengthToStyle(style, CSSPropertyID::kWidth, value);
    }
  } else if (name == html_names::kColorAttr) {
    AddPropertyToPresentationAttributeStyle(style, CSSPropertyID::kBorderStyle,
                                            CSSValueID::kSolid);
    AddHTMLColorToStyle(style, CSSPropertyID::kBorderColor, value);
    AddHTMLColorToStyle(style, CSSPropertyID::kBackgroundColor, value);
  } else if (name == html_names::kNoshadeAttr) {
    if (!FastHasAttribute(html_names::kColorAttr)) {
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kBorderStyle, CSSValueID::kSolid);

      const cssvalue::CSSColorValue& dark_gray_value =
          *cssvalue::CSSColorValue::Create(Color::kDarkGray);
      style->SetProperty(CSSPropertyID::kBorderColor, dark_gray_value);
      style->SetProperty(CSSPropertyID::kBackgroundColor, dark_gray_value);
    }
  } else if (name == html_names::kSizeAttr) {
    int size = value.ToInt();
    if (size <= 1) {
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kBorderBottomWidth, 0,
          CSSPrimitiveValue::UnitType::kPixels);
    } else {
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kHeight, size - 2,
          CSSPrimitiveValue::UnitType::kPixels);
    }
  } else {
    HTMLElement::CollectStyleForPresentationAttribute(name, value, style);
  }
}

HTMLSelectElement* HTMLHRElement::OwnerSelectElement() const {
  if (!parentNode())
    return nullptr;
  if (auto* select = DynamicTo<HTMLSelectElement>(*parentNode()))
    return select;
  if (!IsA<HTMLOptGroupElement>(*parentNode()))
    return nullptr;
  return DynamicTo<HTMLSelectElement>(parentNode()->parentNode());
}

Node::InsertionNotificationRequest HTMLHRElement::InsertedInto(
    ContainerNode& insertion_point) {
  HTMLElement::InsertedInto(insertion_point);
  if (HTMLSelectElement* select = OwnerSelectElement()) {
    if (&insertion_point == select ||
        (IsA<HTMLOptGroupElement>(insertion_point) &&
         insertion_point.parentNode() == select))
      select->HrInsertedOrRemoved(*this);
  }
  return kInsertionDone;
}

void HTMLHRElement::RemovedFrom(ContainerNode& insertion_point) {
  if (auto* select = DynamicTo<HTMLSelectElement>(insertion_point)) {
    if (!parentNode() || IsA<HTMLOptGroupElement>(*parentNode()))
      select->HrInsertedOrRemoved(*this);
  } else if (IsA<HTMLOptGroupElement>(insertion_point)) {
    Node* parent = insertion_point.parentNode();
    select = DynamicTo<HTMLSelectElement>(parent);
    if (select)
      select->HrInsertedOrRemoved(*this);
  }
  HTMLElement::RemovedFrom(insertion_point);
}

}  // namespace blink
