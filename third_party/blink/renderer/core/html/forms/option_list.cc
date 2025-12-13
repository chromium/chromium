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
  // This function returns any <option> descendant of select_.

  Element* current;
  if (previous) {
    current = ElementTraversal::NextSkippingChildren(*previous, &select_);
  } else {
    current = ElementTraversal::FirstChild(select_);
  }
  while (current) {
    if (auto* option = DynamicTo<HTMLOptionElement>(current)) {
      current_ = option;
      return;
    }
    if (HTMLSelectElement::ShouldIgnoreDescendantsForOptionTraversals(
            current)) {
      current = ElementTraversal::NextSkippingChildren(*current, &select_);
    } else if (auto* optgroup = DynamicTo<HTMLOptGroupElement>(current)) {
      // optgroup->OwnerSelectElement() might be null because this method may
      // be called before InsertedInto is called on the optgroup. Like the
      // same check for option elements above, we have to skip DCHECKs inside
      // the call to OwnerSelectElement.
      // TODO(crbug.com/398887837): Remove the skip_check parameter.
      if (optgroup->OwnerSelectElement(/*skip_check=*/true) == select_ ||
          HTMLSelectElement::AssociatedSelectAndOptgroup(*optgroup).first ==
              select_) {
        current = ElementTraversal::Next(*current, &select_);
      } else {
        // Don't track elements inside nested <optgroup>s.
        current = ElementTraversal::NextSkippingChildren(*current, &select_);
      }
    } else {
      current = ElementTraversal::Next(*current, &select_);
    }
  }
  current_ = nullptr;
}

void OptionListIterator::Retreat(HTMLOptionElement* next) {
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

      if (current == select_) {
        current = nullptr;
      } else if (HTMLSelectElement::ShouldIgnoreDescendantsForOptionTraversals(
                     current)) {
        current = ElementTraversal::PreviousAbsoluteSibling(*current, &select_);
      } else if (auto* optgroup = DynamicTo<HTMLOptGroupElement>(current)) {
        // optgroup->OwnerSelectElement() might be null because this method may
        // be called before InsertedInto is called on the optgroup.
        if (optgroup->OwnerSelectElement() == select_ ||
            HTMLSelectElement::AssociatedSelectAndOptgroup(*optgroup).first ==
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

}  // namespace blink
