// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/option_list.h"

#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/html/forms/html_data_list_element.h"
#include "third_party/blink/renderer/core/html/forms/html_opt_group_element.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"

namespace blink {

void OptionListIterator::Advance(HTMLOptionElement* previous) {
  // This function returns only
  // - An OPTION child of select_, or
  // - An OPTION child of an OPTGROUP child of select_.
  // - An OPTION descendant of select_ if SelectParserRelaxation is enabled.

  Element* current;
  if (previous) {
    DCHECK_EQ(previous->OwnerSelectElement(), select_);
    current = ElementTraversal::NextSkippingChildren(*previous, &select_);
  } else {
    current = ElementTraversal::FirstChild(select_);
  }
  while (current) {
    if (auto* option = DynamicTo<HTMLOptionElement>(current)) {
      current_ = option;
      return;
    }
    if (RuntimeEnabledFeatures::SelectParserRelaxationEnabled()) {
      if (IsA<HTMLSelectElement>(current)) {
        current = ElementTraversal::NextSkippingChildren(*current, &select_);
      } else {
        current = ElementTraversal::Next(*current, &select_);
      }
    } else {
      DCHECK(!RuntimeEnabledFeatures::CustomizableSelectEnabled());
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
    current = ElementTraversal::PreviousAbsoluteSibling(*next, &select_);
  } else {
    current = ElementTraversal::LastChild(select_);
  }

  while (current) {
    if (auto* option = DynamicTo<HTMLOptionElement>(current)) {
      current_ = option;
      return;
    }

    if (RuntimeEnabledFeatures::SelectParserRelaxationEnabled()) {
      if (IsA<HTMLSelectElement>(current)) {
        current = ElementTraversal::PreviousAbsoluteSibling(*next, &select_);
      } else {
        current = ElementTraversal::Previous(*current, &select_);
      }
    } else {
      DCHECK(!RuntimeEnabledFeatures::CustomizableSelectEnabled());
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

HTMLOptionElement* OptionList::NextMatchingOption(
    HTMLOptionElement& option,
    OptionMatchingPredicate matching,
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
    if (matching(*option_list_iterator)) {
      return &*option_list_iterator;
    }
  }
}

}  // namespace blink
