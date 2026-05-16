// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_OPTION_LIST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_OPTION_LIST_H_

#include "base/check_op.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class HTMLSelectElement;
class HTMLOptionElement;
class HTMLMenuOwnerElement;
class HTMLMenuItemElement;

template <typename OwnerType, typename ItemType>
class ElementListIterator final {
  STACK_ALLOCATED();

 public:
  enum class IteratorStartingPoint {
    kStart,
    kEnd,
    kLast,
  };

  explicit ElementListIterator(
      const OwnerType& owner,
      IteratorStartingPoint starting_point = IteratorStartingPoint::kStart)
      : owner_(owner), current_(nullptr) {
    switch (starting_point) {
      case IteratorStartingPoint::kStart:
        Advance(nullptr);
        break;
      case IteratorStartingPoint::kLast:
        Retreat(nullptr);
        break;
      case IteratorStartingPoint::kEnd:
        break;
    }
  }
  ItemType& operator*() {
    DCHECK(current_);
    return *current_;
  }
  ItemType* operator->() { return current_; }
  ElementListIterator<OwnerType, ItemType>& operator++() {
    if (current_) {
      Advance(current_);
    }
    return *this;
  }
  ElementListIterator<OwnerType, ItemType>& operator--() {
    if (current_) {
      Retreat(current_);
    }
    return *this;
  }
  operator bool() const { return current_; }
  bool operator==(const ElementListIterator<OwnerType, ItemType>& other) const {
    return current_ == other.current_;
  }
  ElementListIterator<OwnerType, ItemType>& operator=(
      const ElementListIterator<OwnerType, ItemType>& other) {
    CHECK_EQ(&owner_, &other.owner_);
    current_ = other.current_;
    return *this;
  }

 private:
  void Advance(ItemType* current);
  void Retreat(ItemType* current);

  const OwnerType& owner_;
  ItemType* current_;  // nullptr means we reached the end.
};

template <typename OwnerType, typename ItemType>
class ElementList final {
  STACK_ALLOCATED();

 public:
  explicit ElementList(const OwnerType& owner) : owner_(owner) {}
  using Iterator = ElementListIterator<OwnerType, ItemType>;
  Iterator begin() {
    return Iterator(
        owner_, ElementListIterator<OwnerType,
                                    ItemType>::IteratorStartingPoint::kStart);
  }
  Iterator end() {
    return Iterator(
        owner_,
        ElementListIterator<OwnerType, ItemType>::IteratorStartingPoint::kEnd);
  }
  Iterator last() {
    return Iterator(
        owner_,
        ElementListIterator<OwnerType, ItemType>::IteratorStartingPoint::kLast);
  }
  bool Empty() {
    return !Iterator(
        owner_, ElementListIterator<OwnerType,
                                    ItemType>::IteratorStartingPoint::kStart);
  }
  unsigned size() const;
  ItemType& at(unsigned index) {
    Iterator it = begin();
    for (unsigned i = 0; i < index; i++) {
      ++it;
    }
    return *it;
  }

  typedef bool (*ElementMatchingPredicate)(ItemType& element);
  ItemType* FindNextElement(ItemType& element,
                            ElementMatchingPredicate predicate,
                            bool inclusive = false,
                            bool wrap = false) {
    return FindElement(element, predicate, /*forward*/ true, inclusive, wrap);
  }
  ItemType* FindPreviousElement(ItemType& element,
                                ElementMatchingPredicate predicate,
                                bool inclusive = false,
                                bool wrap = false) {
    return FindElement(element, predicate, /*forward*/ false, inclusive, wrap);
  }

  ItemType* NextFocusableElement(ItemType& element,
                                 bool inclusive = false,
                                 bool wrap = false);
  ItemType* PreviousFocusableElement(ItemType& element,
                                     bool inclusive = false,
                                     bool wrap = false);

 private:
  ItemType* FindElement(ItemType& element,
                        ElementMatchingPredicate predicate,
                        bool forward,
                        bool inclusive,
                        bool wrap = false);

  const OwnerType& owner_;
};

// Defining the template implementation in the cc file instead of this header
// should reduce compiled object size.
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    ElementListIterator<HTMLSelectElement, HTMLOptionElement>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    ElementList<HTMLSelectElement, HTMLOptionElement>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    ElementListIterator<HTMLMenuOwnerElement, HTMLMenuItemElement>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    ElementList<HTMLMenuOwnerElement, HTMLMenuItemElement>;

// OptionList class is a lightweight version of HTMLOptionsCollection.
using OptionList = ElementList<HTMLSelectElement, HTMLOptionElement>;
using MenuItemList = ElementList<HTMLMenuOwnerElement, HTMLMenuItemElement>;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_OPTION_LIST_H_
