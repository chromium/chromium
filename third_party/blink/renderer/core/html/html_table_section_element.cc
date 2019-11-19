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

#include "third_party/blink/renderer/core/html/html_table_section_element.h"

#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/node_lists_node_data.h"
#include "third_party/blink/renderer/core/html/html_collection.h"
#include "third_party/blink/renderer/core/html/html_table_element.h"
#include "third_party/blink/renderer/core/html/html_table_row_element.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

HTMLTableSectionElement::HTMLTableSectionElement(const QualifiedName& tag_name,
                                                 Document& document)
    : HTMLTablePartElement(tag_name, document) {}

const CSSPropertyValueSet*
HTMLTableSectionElement::AdditionalPresentationAttributeStyle() {
  if (HTMLTableElement* table = FindParentTable())
    return table->AdditionalGroupStyle(true);
  return nullptr;
}

// these functions are rather slow, since we need to get the row at
// the index... but they aren't used during usual HTML parsing anyway
HTMLElement* HTMLTableSectionElement::insertRow(
    int index,
    ExceptionState& exception_state) {
  HTMLCollection* children = rows();
  int num_rows = children ? static_cast<int>(children->length()) : 0;
  if (index < -1 || index > num_rows) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        "The provided index (" + String::Number(index) +
            " is outside the range [-1, " + String::Number(num_rows) + "].");
    return nullptr;
  }

  auto* row = MakeGarbageCollected<HTMLTableRowElement>(GetDocument());
  if (num_rows == index || index == -1)
    AppendChild(row, exception_state);
  else
    InsertBefore(row, children->item(index), exception_state);
  return row;
}

void HTMLTableSectionElement::deleteRow(int index,
                                        ExceptionState& exception_state) {
  HTMLCollection* children = rows();
  int num_rows = children ? (int)children->length() : 0;
  if (index == -1) {
    if (!num_rows)
      return;
    index = num_rows - 1;
  }
  if (index >= 0 && index < num_rows) {
    Element* row = children->item(index);
    HTMLElement::RemoveChild(row, exception_state);
  } else {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        "The provided index (" + String::Number(index) +
            " is outside the range [-1, " + String::Number(num_rows) + "].");
  }
}

HTMLCollection* HTMLTableSectionElement::rows() {
  return EnsureCachedCollection<HTMLCollection>(kTSectionRows);
}

}  // namespace blink
