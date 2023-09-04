// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/html_listbox_element.h"

#include "third_party/blink/renderer/core/dom/popover_data.h"

namespace blink {

HTMLListboxElement::HTMLListboxElement(Document& document)
    : HTMLElement(html_names::kListboxTag, document) {
  CHECK(RuntimeEnabledFeatures::HTMLSelectListElementEnabled());
}

Node::InsertionNotificationRequest HTMLListboxElement::InsertedInto(
    ContainerNode& parent) {
  if (IsA<HTMLSelectListElement>(parent)) {
    EnsurePopoverData()->setType(PopoverValueType::kAuto);
  }
  return HTMLElement::InsertedInto(parent);
}

void HTMLListboxElement::RemovedFrom(ContainerNode& insertion_point) {
  HTMLElement::RemovedFrom(insertion_point);

  // Clean up the popover data we set in InsertedInto. If this listbox is still
  // considered selectlist-associated, then UpdatePopoverAttribute will early
  // out.
  UpdatePopoverAttribute(FastGetAttribute(html_names::kPopoverAttr));
}

// static
bool HTMLListboxElement::IsSelectlistAssociated(const Element* node) {
  return IsA<HTMLListboxElement>(node) &&
         IsA<HTMLSelectListElement>(node->parentNode());
}

}  // namespace blink
