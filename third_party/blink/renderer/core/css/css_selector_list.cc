/*
 * Copyright (C) 2008, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/css/css_selector_list.h"

#include <memory>
#include "third_party/blink/renderer/core/css/css_selector.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

CSSSelectorList* CSSSelectorList::Empty() {
  CSSSelectorList* list =
      MakeGarbageCollected<CSSSelectorList>(base::PassKey<CSSSelectorList>());
  new (list->first_selector_) CSSSelector();
  list->first_selector_[0].SetMatch(CSSSelector::kInvalidList);
  DCHECK(!list->IsValid());
  return list;
}

CSSSelectorList* CSSSelectorList::Copy() const {
  if (!IsValid()) {
    return CSSSelectorList::Empty();
  }

  unsigned length = ComputeLength();
  DCHECK(length);
  CSSSelectorList* list = MakeGarbageCollected<CSSSelectorList>(
      AdditionalBytes(sizeof(CSSSelector) * (length - 1)),
      base::PassKey<CSSSelectorList>());
  for (unsigned i = 0; i < length; ++i) {
    new (&list->first_selector_[i]) CSSSelector(first_selector_[i]);
  }

  return list;
}

HeapVector<CSSSelector> CSSSelectorList::Copy(
    const CSSSelector* selector_list) {
  HeapVector<CSSSelector> selectors;
  for (const CSSSelector* selector = selector_list; selector;
       selector = selector->IsLastInSelectorList() ? nullptr : (selector + 1)) {
    selectors.push_back(*selector);
  }
  return selectors;
}

void CSSSelectorList::AdoptSelectorVector(
    base::span<CSSSelector> selector_vector,
    CSSSelector* selector_array) {
  std::uninitialized_move(selector_vector.begin(), selector_vector.end(),
                          selector_array);
  selector_array[selector_vector.size() - 1].SetLastInSelectorList(true);
}

CSSSelectorList* CSSSelectorList::AdoptSelectorVector(
    base::span<CSSSelector> selector_vector) {
  if (selector_vector.empty()) {
    return CSSSelectorList::Empty();
  }

  CSSSelectorList* list = MakeGarbageCollected<CSSSelectorList>(
      AdditionalBytes(sizeof(CSSSelector) * (selector_vector.size() - 1)),
      base::PassKey<CSSSelectorList>());
  AdoptSelectorVector(selector_vector, list->first_selector_);
  return list;
}

unsigned CSSSelectorList::ComputeLength() const {
  if (!IsValid()) {
    return 0;
  }
  const CSSSelector* current = First();
  while (!current->IsLastInSelectorList()) {
    ++current;
  }
  return SelectorIndex(*current) + 1;
}

unsigned CSSSelectorList::MaximumSpecificity() const {
  unsigned specificity = 0;

  for (const CSSSelector* s = First(); s; s = Next(*s)) {
    specificity = std::max(specificity, s->Specificity());
  }

  return specificity;
}

void CSSSelectorList::Reparent(CSSSelector* selector_list,
                               StyleRule* new_parent) {
  DCHECK(selector_list);
  CSSSelector* current = selector_list;
  do {
    current->Reparent(new_parent);
  } while (!(current++)->IsLastInSelectorList());
}

String CSSSelectorList::SelectorsText(const CSSSelector* first) {
  StringBuilder result;

  for (const CSSSelector* s = first; s; s = Next(*s)) {
    if (s != first) {
      result.Append(", ");
    }
    result.Append(s->SelectorText());
  }

  return result.ReleaseString();
}

void CSSSelectorList::Trace(Visitor* visitor) const {
  if (!IsValid()) {
    return;
  }

  for (int i = 0;; ++i) {
    visitor->Trace(first_selector_[i]);
    if (first_selector_[i].IsLastInSelectorList()) {
      break;
    }
  }
}

}  // namespace blink
