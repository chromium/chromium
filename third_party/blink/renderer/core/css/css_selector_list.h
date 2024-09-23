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

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_SELECTOR_LIST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_SELECTOR_LIST_H_

#include "base/types/pass_key.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_selector.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

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
// Use CSSSelector::NextSimpleSelector() and
// CSSSelector::IsLastInComplexSelector() to traverse through each sequence of
// simple selectors, from .c3 to #ident; from span to .c2; from div to .c1
//
// StyleRule stores its selectors in an identical memory layout,
// but not as part of a CSSSelectorList (see its class comments).
// It reuses many of the exposed static member functions from CSSSelectorList
// to provide a subset of its API.
class CORE_EXPORT CSSSelectorList : public GarbageCollected<CSSSelectorList> {
 public:
  // Constructs an empty selector list, for which IsValid() returns false.
  // TODO(sesse): Consider making this a singleton.
  static CSSSelectorList* Empty();

  // Do not call; for Empty() and AdoptSelectorVector() only.
  explicit CSSSelectorList(base::PassKey<CSSSelectorList>) {}

  CSSSelectorList(CSSSelectorList&& o) {
    memcpy(this, o.first_selector_, ComputeLength() * sizeof(CSSSelector));
  }
  ~CSSSelectorList() = default;

  static CSSSelectorList* AdoptSelectorVector(
      base::span<CSSSelector> selector_vector);
  static void AdoptSelectorVector(base::span<CSSSelector> selector_vector,
                                  CSSSelector* selector_array);

  CSSSelectorList* Copy() const;
  static HeapVector<CSSSelector> Copy(const CSSSelector* selector_list);

  bool IsValid() const {
    return first_selector_[0].Match() != CSSSelector::kInvalidList;
  }
  const CSSSelector* First() const {
    return IsValid() ? first_selector_ : nullptr;
  }
  static const CSSSelector* Next(const CSSSelector&);
  static CSSSelector* Next(CSSSelector&);

  // The CSS selector represents a single sequence of simple selectors.
  bool HasOneSelector() const { return IsValid() && !Next(*first_selector_); }
  const CSSSelector& SelectorAt(wtf_size_t index) const {
    DCHECK(IsValid());
    return first_selector_[index];
  }

  wtf_size_t SelectorIndex(const CSSSelector& selector) const {
    DCHECK(IsValid());
    return static_cast<wtf_size_t>(&selector - first_selector_);
  }

  wtf_size_t IndexOfNextSelectorAfter(wtf_size_t index) const {
    const CSSSelector& current = SelectorAt(index);
    const CSSSelector* next = Next(current);
    if (!next) {
      return kNotFound;
    }
    return SelectorIndex(*next);
  }

  String SelectorsText() const { return SelectorsText(First()); }
  static String SelectorsText(const CSSSelector* first);

  // Selector lists don't know their length, computing it is O(n) and should be
  // avoided when possible. Instead iterate from first() and using next().
  unsigned ComputeLength() const;

  // Return the specificity of the selector with the highest specificity.
  unsigned MaximumSpecificity() const;

  // See CSSSelector::Reparent.
  static void Reparent(CSSSelector* selector_list, StyleRule* new_parent);

  void Reparent(StyleRule* new_parent) {
    CSSSelectorList::Reparent(first_selector_, new_parent);
  }

  CSSSelectorList(const CSSSelectorList&) = delete;
  CSSSelectorList& operator=(const CSSSelectorList&) = delete;

  void Trace(Visitor* visitor) const;

 private:
  // All of the remaining CSSSelector objects are allocated on
  // AdditionalBytes, and thus live immediately after this object. The length
  // is not stored explicitly anywhere: End of a multipart selector is
  // indicated by is_last_in_complexlector_ bit in the last item. End of the
  // array is indicated by is_last_in_selector_list_ bit in the last item.
  CSSSelector first_selector_[1];
};

inline const CSSSelector* CSSSelectorList::Next(const CSSSelector& current) {
  return Next(const_cast<CSSSelector&>(current));
}

inline CSSSelector* CSSSelectorList::Next(CSSSelector& current) {
  // Skip subparts of compound selectors.
  CSSSelector* last = &current;
  while (!last->IsLastInComplexSelector()) {
    last++;
  }
  return last->IsLastInSelectorList() ? nullptr : last + 1;
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_SELECTOR_LIST_H_
