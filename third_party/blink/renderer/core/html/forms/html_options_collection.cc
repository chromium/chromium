/*
 * Copyright (C) 2006, 2011, 2012 Apple Computer, Inc.
 * Copyright (C) 2014 Samsung Electronics. All rights reserved.
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

#include "third_party/blink/renderer/core/html/forms/html_options_collection.h"

#include "third_party/blink/renderer/bindings/core/v8/html_element_or_long.h"
#include "third_party/blink/renderer/bindings/core/v8/html_option_element_or_html_opt_group_element.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

HTMLOptionsCollection::HTMLOptionsCollection(ContainerNode& select)
    : HTMLCollection(select, kSelectOptions, kDoesNotOverrideItemAfter) {
  DCHECK(IsA<HTMLSelectElement>(select));
}

HTMLOptionsCollection::HTMLOptionsCollection(ContainerNode& select,
                                             CollectionType type)
    : HTMLOptionsCollection(select) {
  DCHECK_EQ(type, kSelectOptions);
}

void HTMLOptionsCollection::SupportedPropertyNames(Vector<String>& names) {
  // As per
  // http://www.whatwg.org/specs/web-apps/current-work/multipage/common-dom-interfaces.html#htmloptionscollection:
  // The supported property names consist of the non-empty values of all the id
  // and name attributes of all the elements represented by the collection, in
  // tree order, ignoring later duplicates, with the id of an element preceding
  // its name if it contributes both, they differ from each other, and neither
  // is the duplicate of an earlier entry.
  HashSet<AtomicString> existing_names;
  unsigned length = this->length();
  for (unsigned i = 0; i < length; ++i) {
    Element* element = item(i);
    DCHECK(element);
    const AtomicString& id_attribute = element->GetIdAttribute();
    if (!id_attribute.IsEmpty()) {
      HashSet<AtomicString>::AddResult add_result =
          existing_names.insert(id_attribute);
      if (add_result.is_new_entry)
        names.push_back(id_attribute);
    }
    const AtomicString& name_attribute = element->GetNameAttribute();
    if (!name_attribute.IsEmpty()) {
      HashSet<AtomicString>::AddResult add_result =
          existing_names.insert(name_attribute);
      if (add_result.is_new_entry)
        names.push_back(name_attribute);
    }
  }
}

void HTMLOptionsCollection::add(
    const HTMLOptionElementOrHTMLOptGroupElement& element,
    const HTMLElementOrLong& before,
    ExceptionState& exception_state) {
  To<HTMLSelectElement>(ownerNode()).add(element, before, exception_state);
}

void HTMLOptionsCollection::remove(int index) {
  To<HTMLSelectElement>(ownerNode()).remove(index);
}

int HTMLOptionsCollection::selectedIndex() const {
  return To<HTMLSelectElement>(ownerNode()).selectedIndex();
}

void HTMLOptionsCollection::setSelectedIndex(int index) {
  To<HTMLSelectElement>(ownerNode()).setSelectedIndex(index);
}

void HTMLOptionsCollection::setLength(unsigned length,
                                      ExceptionState& exception_state) {
  To<HTMLSelectElement>(ownerNode()).setLength(length, exception_state);
}

bool HTMLOptionsCollection::AnonymousIndexedSetter(
    unsigned index,
    HTMLOptionElement* value,
    ExceptionState& exception_state) {
  auto& base = To<HTMLSelectElement>(ownerNode());
  if (!value) {  // undefined or null
    base.remove(index);
    return true;
  }
  base.SetOption(index, value, exception_state);
  return true;
}

}  // namespace blink
