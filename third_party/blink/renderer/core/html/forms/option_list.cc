// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/option_list.h"

#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/html/forms/html_data_list_element.h"
#include "third_party/blink/renderer/core/html/forms/html_opt_group_element.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/html_hr_element.h"

namespace blink {

void OptionListIterator::Advance(HTMLOptionElement* previous) {
  // This function returns only
  // - An OPTION child of select_, or
  // - An OPTION child of an OPTGROUP child of select_.
  // - An OPTION descendant of select_ if SelectParserRelaxation is enabled.

  Element* current;
  if (previous) {
    if (HTMLSelectElement::SelectParserRelaxationEnabled(&select_) &&
        !previous->OwnerSelectElement(/*skip_check=*/true)) {
      // In some cases, an OptionList is created and used for a select element
      // before its descendant option elements had InsertedInto called on
      // them, such as constructing fragments in Element::setInnerHTML. When
      // these options aren't notified like this, they won't have the correct
      // value for OwnerSelectElement yet. We can update it to the correct
      // value here.
      // TODO(crbug.com/398887837): Remove this.
      previous->SetOwnerSelectElement(const_cast<HTMLSelectElement*>(&select_));
    } else {
      DCHECK_EQ(previous->OwnerSelectElement(), select_);
    }
    current = ElementTraversal::NextSkippingChildren(*previous, &select_);
  } else {
    current = ElementTraversal::FirstChild(select_);
  }
  while (current) {
    if (auto* option = DynamicTo<HTMLOptionElement>(current)) {
      current_ = option;
      return;
    }
    if (HTMLSelectElement::SelectParserRelaxationEnabled(&select_)) {
      if (IsA<HTMLSelectElement>(current) || IsA<HTMLHRElement>(current)) {
        current = ElementTraversal::NextSkippingChildren(*current, &select_);
      } else if (auto* optgroup = DynamicTo<HTMLOptGroupElement>(current)) {
        // optgroup->OwnerSelectElement() might be null because this method may
        // be called before InsertedInto is called on the optgroup. Like the
        // same check for option elements above, we have to skip DCHECKs inside
        // the call to OwnerSelectElement.
        // TODO(crbug.com/398887837): Remove the skip_check parameter.
        if (optgroup->OwnerSelectElement(/*skip_check=*/true) == select_ ||
            HTMLSelectElement::NearestAncestorSelectNoNesting(*optgroup) ==
                select_) {
          current = ElementTraversal::Next(*current, &select_);
        } else {
          // Don't track elements inside nested <optgroup>s.
          current = ElementTraversal::NextSkippingChildren(*current, &select_);
        }
      } else {
        current = ElementTraversal::Next(*current, &select_);
      }
    } else {
      DCHECK(!HTMLSelectElement::CustomizableSelectEnabled(&select_));
      if (IsA<HTMLOptGroupElement>(current) &&
          current->parentNode() == &select_) {
        if ((current_ = Traversal<HTMLOptionElement>::FirstChild(*current))) {
          return;
        }
      }
      current = ElementTraversal::NextSkippingChildren(*current, &select_);
    }
  }
  current_ = nullptr;
}

void OptionListIterator::Retreat(HTMLOptionElement* next) {
  // This function returns only
  // - An OPTION child of select_, or
  // - An OPTION child of an OPTGROUP child of select_.
  // - An OPTION descendant of select_ if SelectParserRelaxation is enabled.

  Element* current;
  if (next) {
    DCHECK_EQ(next->OwnerSelectElement(), select_);
    current = ElementTraversal::Previous(*next, &select_);
  } else {
    current = ElementTraversal::LastChild(select_);
  }

  while (current) {
    if (auto* option = DynamicTo<HTMLOptionElement>(current)) {
      current_ = option;
      return;
    }

    if (HTMLSelectElement::SelectParserRelaxationEnabled(&select_)) {
      if (current == select_) {
        current = nullptr;
      } else if (IsA<HTMLSelectElement>(current) ||
                 IsA<HTMLHRElement>(current)) {
        current = ElementTraversal::PreviousAbsoluteSibling(*current, &select_);
      } else if (auto* optgroup = DynamicTo<HTMLOptGroupElement>(current)) {
        // optgroup->OwnerSelectElement() might be null because this method may
        // be called before InsertedInto is called on the optgroup.
        if (optgroup->OwnerSelectElement() == select_ ||
            HTMLSelectElement::NearestAncestorSelectNoNesting(*optgroup) ==
                select_) {
          current = ElementTraversal::Previous(*current, &select_);
        } else {
          // Don't track elements inside nested <optgroup>s.
          current =
              ElementTraversal::PreviousAbsoluteSibling(*current, &select_);
        }
      } else {
        current = ElementTraversal::Previous(*current, &select_);
      }
    } else {
      DCHECK(!HTMLSelectElement::CustomizableSelectEnabled(&select_));
      if (IsA<HTMLOptGroupElement>(current) &&
          current->parentNode() == &select_) {
        if ((current_ = Traversal<HTMLOptionElement>::LastChild(*current))) {
          return;
        }
      }
      current = ElementTraversal::PreviousAbsoluteSibling(*next, &select_);
    }
  }

  current_ = nullptr;
}

unsigned OptionList::size() const {
  unsigned count = 0;
  auto iterator = Iterator(select_, OptionListIterator::StartingPoint::kStart);
  while (iterator) {
    ++count;
    ++iterator;
  }
  return count;
}

HTMLOptionElement* OptionList::FindFocusableOption(HTMLOptionElement& option,
                                                   bool forward,
                                                   bool inclusive) {
  DCHECK_EQ(option.OwnerSelectElement(), select_);
  DCHECK(!Empty());
  OptionListIterator option_list_iterator = begin();
  while (option_list_iterator && *option_list_iterator != option) {
    ++option_list_iterator;
  }
  CHECK_EQ(*option_list_iterator, option);
  while (true) {
    if (!inclusive) {
      if (forward) {
        ++option_list_iterator;
      } else {
        --option_list_iterator;
      }
    }
    if (!option_list_iterator) {
      return nullptr;
    }
    inclusive = false;
    if (option_list_iterator->IsFocusable()) {
      return &*option_list_iterator;
    }
  }
}

HTMLOptionElement* OptionList::FirstKeyboardFocusableOption() {
  if (Empty()) {
    return nullptr;
  }
  for (OptionListIterator it = begin(); it; ++it) {
    if (it->IsKeyboardFocusableSlow(Element::UpdateBehavior::kStyleAndLayout)) {
      return &*it;
    }
  }
  return nullptr;
}

}  // namespace blink
