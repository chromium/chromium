// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_menu_item_radio_element.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/html/forms/html_field_set_element.h"
#include "third_party/blink/renderer/core/html/html_menu_bar_element.h"
#include "third_party/blink/renderer/core/html/html_menu_list_element.h"
#include "third_party/blink/renderer/core/html_names.h"

namespace blink {

HTMLMenuItemRadioElement::HTMLMenuItemRadioElement(Document& document)
    : HTMLMenuItemCheckableElementBase(html_names::kMenuitemradioTag,
                                       document) {}

HTMLMenuItemRadioElement::~HTMLMenuItemRadioElement() = default;

Element* HTMLMenuItemRadioElement::RadioGroupAncestor() const {
  for (Node& ancestor : NodeTraversal::AncestorsOf(*this)) {
    if (auto* element = DynamicTo<Element>(ancestor)) {
      if (IsA<HTMLFieldSetElement>(*element) ||
          IsA<HTMLMenuListElement>(*element) ||
          IsA<HTMLMenuBarElement>(*element)) {
        return element;
      }
    }
  }
  return nullptr;
}

void HTMLMenuItemRadioElement::UpdateForExclusivity() {
  Element* group = RadioGroupAncestor();
  if (!group) {
    return;
  }

  for (HTMLMenuItemRadioElement& radio :
       Traversal<HTMLMenuItemRadioElement>::DescendantsOf(*group)) {
    if (&radio != this && radio.RadioGroupAncestor() == group) {
      radio.setChecked(false, CheckedBehavior::kNoEvents);
    }
  }
}

}  // namespace blink
