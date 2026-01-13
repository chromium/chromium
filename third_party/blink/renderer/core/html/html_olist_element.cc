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

#include "third_party/blink/renderer/core/html/html_olist_element.h"

#include "third_party/blink/renderer/core/css/css_counter_value.h"
#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/html/list_item_ordinal.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/keywords.h"
#include "third_party/blink/renderer/core/style/counter_directives.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

HTMLOListElement::HTMLOListElement(Document& document)
    : HTMLElement(html_names::kOlTag, document) {}

bool HTMLOListElement::IsPresentationAttribute(
    const QualifiedName& name) const {
  if (name == html_names::kTypeAttr || name == html_names::kReversedAttr ||
      name == html_names::kStartAttr) {
    return true;
  }
  return HTMLElement::IsPresentationAttribute(name);
}

void HTMLOListElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    HeapVector<CSSPropertyValue, 8>& style) {
  if (name == html_names::kTypeAttr) {
    if (value == "a") {
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kListStyleType,
          *MakeGarbageCollected<CSSCustomIdentValue>(keywords::kLowerAlpha));
    } else if (value == "A") {
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kListStyleType,
          *MakeGarbageCollected<CSSCustomIdentValue>(keywords::kUpperAlpha));
    } else if (value == "i") {
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kListStyleType,
          *MakeGarbageCollected<CSSCustomIdentValue>(keywords::kLowerRoman));
    } else if (value == "I") {
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kListStyleType,
          *MakeGarbageCollected<CSSCustomIdentValue>(keywords::kUpperRoman));
    } else if (value == "1") {
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kListStyleType,
          *MakeGarbageCollected<CSSCustomIdentValue>(keywords::kDecimal));
    }
  } else if (RuntimeEnabledFeatures::CSSListCounterAccountingEnabled() &&
             (name == html_names::kStartAttr ||
              name == html_names::kReversedAttr)) {
    // This logic depends on both attributes. To avoid duplicate processing,
    // if both are present, skip when processing the 'reversed' attribute.
    if (name == html_names::kReversedAttr && has_explicit_start_) {
      return;
    }

    // https://html.spec.whatwg.org/multipage/rendering.html#lists
    const bool is_reversed = FastHasAttribute(html_names::kReversedAttr);
    if (has_explicit_start_ || is_reversed) {
      const CSSCustomIdentValue* identifier =
          MakeGarbageCollected<CSSCustomIdentValue>(AtomicString("list-item"));
      const CSSNumericLiteralValue* literal_start_value =
          has_explicit_start_
              ? CSSNumericLiteralValue::Create(
                    // The counter-reset value must be offset by 1 because the
                    // list-item counter is incremented/decremented before the
                    // value is used for the first item.
                    static_cast<int64_t>(start_) + (is_reversed ? 1 : -1),
                    CSSPrimitiveValue::UnitType::kNumber)
              : nullptr;
      CSSValueList* list = CSSValueList::CreateSpaceSeparated();
      list->Append(*MakeGarbageCollected<cssvalue::CSSCounterValue>(
          *identifier, literal_start_value, static_cast<bool>(is_reversed)));
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kCounterReset, *list);
    }
  } else {
    HTMLElement::CollectStyleForPresentationAttribute(name, value, style);
  }
}

void HTMLOListElement::ParseAttribute(
    const AttributeModificationParams& params) {
  if (params.name == html_names::kStartAttr) {
    int parsed_start = 0;
    bool can_parse = ParseHTMLInteger(params.new_value, parsed_start);
    if (RuntimeEnabledFeatures::CSSListCounterAccountingEnabled() &&
        ((can_parse && has_explicit_start_ && start_ == parsed_start) ||
         (!can_parse && !has_explicit_start_))) {
      return;
    }
    int64_t old_initial_counter = InitialCounter();
    has_explicit_start_ = can_parse;
    start_ = can_parse ? parsed_start : 0xBADBEEF;
    if (!RuntimeEnabledFeatures::CSSListCounterAccountingEnabled() &&
        old_initial_counter == InitialCounter()) {
      return;
    }
    InvalidateItemValues();
  } else if (params.name == html_names::kReversedAttr) {
    bool reversed = !params.new_value.IsNull();
    if (RuntimeEnabledFeatures::CSSListCounterAccountingEnabled()) {
      if (reversed == FastHasAttribute(html_names::kReversedAttr)) {
        return;
      }
    } else {
      if (reversed == IsReversed()) {
        return;
      }
      is_reversed_ = reversed;
    }
    InvalidateItemValues();
  } else {
    HTMLElement::ParseAttribute(params);
  }
}

void HTMLOListElement::setStart(int start) {
  SetIntegralAttribute(html_names::kStartAttr, start);
}

void HTMLOListElement::InvalidateItemValues() {
  if (!GetLayoutObject())
    return;
  ListItemOrdinal::InvalidateAllItemsForOrderedList(this);
}

void HTMLOListElement::RecalculateInitialCounterForReversed() {
  DCHECK(!RuntimeEnabledFeatures::CSSListCounterAccountingEnabled() ||
         IsReversed());
  initial_counter_for_reversed_ =
      ListItemOrdinal::InitialCounterForReversedOrderedList(this);
  should_recalculate_initial_counter_ = false;
}

}  // namespace blink
