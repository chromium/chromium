/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_SELECTOR_LIST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_SELECTOR_LIST_H_

#include <memory>
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_selector.h"

namespace blink {

class CSSParserSelector;

// This class represents a CSS selector, i.e. a pattern of one or more
// simple selectors. https://www.w3.org/TR/css3-selectors/

// More specifically, a CSS selector is a chain of one or more sequences
// of simple selectors separated by combinators.
//
// For example, "div.c1 > span.c2 + .c3#ident" is represented as a
// CSSSelectorList that owns six CSSSelector instances.
//
// The simple selectors are stored in memory in the following order:
// .c3, #ident, span, .c2, div, .c1
// (See CSSSelector.h for more information.)
//
// First() and Next() can be used to traverse from right to left through
// the chain of sequences: .c3#ident then span.c2 then div.c1
//
// SelectorAt and IndexOfNextSelectorAfter provide an equivalent API:
// size_t index = 0;
// do {
//   const CSSSelector& sequence = selectorList.SelectorAt(index);
//   ...
//   index = IndexOfNextSelectorAfter(index);
// } while (index != kNotFound);
//
// Use CSSSelector::TagHistory() and CSSSelector::IsLastInTagHistory()
// to traverse through each sequence of simple selectors,
// from .c3 to #ident; from span to .c2; from div to .c1
class CORE_EXPORT CSSSelectorList {
  USING_FAST_MALLOC(CSSSelectorList);

 public:
  CSSSelectorList() : selector_array_(nullptr) {}

  CSSSelectorList(CSSSelectorList&& o) : selector_array_(o.selector_array_) {
    o.selector_array_ = nullptr;
  }

  CSSSelectorList& operator=(CSSSelectorList&& o) {
    DCHECK(this != &o);
    DeleteSelectorsIfNeeded();
    selector_array_ = o.selector_array_;
    o.selector_array_ = nullptr;
    return *this;
  }

  ~CSSSelectorList() { DeleteSelectorsIfNeeded(); }

  static CSSSelectorList AdoptSelectorVector(
      Vector<std::unique_ptr<CSSParserSelector>>& selector_vector);
  CSSSelectorList Copy() const;

  bool IsValid() const { return !!selector_array_; }
  const CSSSelector* First() const { return selector_array_; }
  const CSSSelector* FirstForCSSOM() const;
  static const CSSSelector* Next(const CSSSelector&);
  static const CSSSelector* NextInFullList(const CSSSelector&);

  // The CSS selector represents a single sequence of simple selectors.
  bool HasOneSelector() const {
    return selector_array_ && !Next(*selector_array_);
  }
  const CSSSelector& SelectorAt(wtf_size_t index) const {
    return selector_array_[index];
  }

  wtf_size_t SelectorIndex(const CSSSelector& selector) const {
    return static_cast<wtf_size_t>(&selector - selector_array_);
  }

  wtf_size_t IndexOfNextSelectorAfter(wtf_size_t index) const {
    const CSSSelector& current = SelectorAt(index);
    const CSSSelector* next = this->Next(current);
    if (!next)
      return kNotFound;
    return SelectorIndex(*next);
  }

  String SelectorsText() const;

  // Selector lists don't know their length, computing it is O(n) and should be
  // avoided when possible. Instead iterate from first() and using next().
  unsigned ComputeLength() const;

 private:
  void DeleteSelectorsIfNeeded() {
    if (selector_array_)
      DeleteSelectors();
  }
  void DeleteSelectors();

  CSSSelectorList(const CSSSelectorList&) = delete;
  CSSSelectorList& operator=(const CSSSelectorList&) = delete;

  // End of a multipart selector is indicated by is_last_in_tag_history_ bit in
  // the last item. End of the array is indicated by is_last_in_selector_list_
  // bit in the last item.
  CSSSelector* selector_array_;
};

inline const CSSSelector* CSSSelectorList::Next(const CSSSelector& current) {
  // Skip subparts of compound selectors.
  const CSSSelector* last = &current;
  while (!last->IsLastInTagHistory())
    last++;
  return last->IsLastInOriginalList() ? nullptr : last + 1;
}

inline const CSSSelector* CSSSelectorList::NextInFullList(
    const CSSSelector& current) {
  // Skip subparts of compound selectors.
  const CSSSelector* last = &current;
  while (!last->IsLastInTagHistory())
    last++;
  return last->IsLastInSelectorList() ? nullptr : last + 1;
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_SELECTOR_LIST_H_
