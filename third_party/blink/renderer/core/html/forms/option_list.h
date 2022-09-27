// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_OPTION_LIST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_OPTION_LIST_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class HTMLSelectElement;
class HTMLOptionElement;

class CORE_EXPORT OptionListIterator final {
  STACK_ALLOCATED();

 public:
  explicit OptionListIterator(const HTMLSelectElement* select)
      : select_(select), current_(nullptr) {
    if (select_)
      Advance(nullptr);
  }
  HTMLOptionElement* operator*() { return current_; }
  void operator++() {
    if (current_)
      Advance(current_);
  }
  bool operator==(const OptionListIterator& other) const {
    return current_ == other.current_;
  }
  bool operator!=(const OptionListIterator& other) const {
    return !(*this == other);
  }

 private:
  void Advance(HTMLOptionElement* current);

  const HTMLSelectElement* select_;
  HTMLOptionElement* current_;  // nullptr means we reached to the end.
};

// OptionList class is a lightweight version of HTMLOptionsCollection.
class OptionList final {
  STACK_ALLOCATED();

 public:
  explicit OptionList(const HTMLSelectElement& select) : select_(&select) {}
  using Iterator = OptionListIterator;
  Iterator begin() { return Iterator(select_); }
  Iterator end() { return Iterator(nullptr); }

 private:
  const HTMLSelectElement* select_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_OPTION_LIST_H_
