// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/slot_scoped_traversal.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"

namespace blink {

HTMLSlotElement* SlotScopedTraversal::FindScopeOwnerSlot(
    const Element& current) {
  if (Element* nearest_inclusive_ancestor_assigned_to_slot =
          SlotScopedTraversal::NearestInclusiveAncestorAssignedToSlot(current))
    return nearest_inclusive_ancestor_assigned_to_slot->AssignedSlot();
  return nullptr;
}

Element* SlotScopedTraversal::NearestInclusiveAncestorAssignedToSlot(
    const Element& current) {
  Element* element = const_cast<Element*>(&current);
  for (; element; element = element->parentElement()) {
    if (element->AssignedSlot())
      break;
  }
  return element;
}

}  // namespace blink
