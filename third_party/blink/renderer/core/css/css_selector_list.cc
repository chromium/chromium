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

#include "third_party/blink/renderer/core/css/css_selector_list.h"

#include <memory>
#include "third_party/blink/renderer/core/css/parser/css_parser_selector.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

CSSSelectorList CSSSelectorList::Copy() const {
  CSSSelectorList list;

  if (!IsValid()) {
    DCHECK(!list.IsValid());
    return list;
  }

  unsigned length = ComputeLength();
  DCHECK(length);
  list.selector_array_ = std::make_unique<CSSSelector[]>(length);
  for (unsigned i = 0; i < length; ++i)
    new (&list.selector_array_[i]) CSSSelector(selector_array_[i]);

  return list;
}

template <bool UseArena>
size_t CSSSelectorList::FlattenedSize(
    const CSSSelectorVector<UseArena>& selector_vector) {
  size_t flattened_size = 0;
  for (const auto& selector_ptr : selector_vector) {
    for (CSSParserSelector<UseArena>* selector = selector_ptr.get(); selector;
         selector = selector->TagHistory())
      ++flattened_size;
  }
  DCHECK(flattened_size);
  return flattened_size;
}

template <bool UseArena>
void CSSSelectorList::AdoptSelectorVector(
    CSSSelectorVector<UseArena>& selector_vector,
    CSSSelector* selector_array,
    size_t flattened_size) {
  DCHECK_EQ(flattened_size, FlattenedSize<UseArena>(selector_vector));
  wtf_size_t array_index = 0;
  for (const auto& selector_ptr : selector_vector) {
    CSSParserSelector<UseArena>* current = selector_ptr.get();
    while (current) {
      // Move item from the parser selector vector into selector_array_ without
      // invoking destructor (Ugh.) The CSSSelector is allocated on Arena,
      // so we do not need to actually free it.
      CSSSelector* current_selector = current->ReleaseSelector().release();
      memcpy(&selector_array[array_index], current_selector,
             sizeof(CSSSelector));
      if constexpr (!UseArena) {
        WTF::Partitions::FastFree(current_selector);
      }

      current = current->TagHistory();
      DCHECK(!selector_array[array_index].IsLastInSelectorList());
      if (current)
        selector_array[array_index].SetLastInTagHistory(false);
      ++array_index;
    }
    DCHECK(selector_array[array_index - 1].IsLastInTagHistory());
  }
  DCHECK_EQ(flattened_size, array_index);
  selector_array[array_index - 1].SetLastInSelectorList(true);
  selector_vector.clear();
}

template <bool UseArena>
CSSSelectorList CSSSelectorList::AdoptSelectorVector(
    CSSSelectorVector<UseArena>& selector_vector) {
  if (selector_vector.IsEmpty()) {
    return {};
  }

  size_t flattened_size = FlattenedSize<UseArena>(selector_vector);

  CSSSelectorList list;
  list.selector_array_ = std::make_unique<CSSSelector[]>(flattened_size);
  AdoptSelectorVector<UseArena>(selector_vector, list.selector_array_.get(),
                                flattened_size);
  return list;
}

unsigned CSSSelectorList::ComputeLength() const {
  if (!selector_array_)
    return 0;
  const CSSSelector* current = First();
  while (!current->IsLastInSelectorList())
    ++current;
  return SelectorIndex(*current) + 1;
}

unsigned CSSSelectorList::MaximumSpecificity() const {
  unsigned specificity = 0;

  for (const CSSSelector* s = First(); s; s = Next(*s))
    specificity = std::max(specificity, s->Specificity());

  return specificity;
}

String CSSSelectorList::SelectorsText(const CSSSelector* first) {
  StringBuilder result;

  for (const CSSSelector* s = first; s; s = Next(*s)) {
    if (s != first)
      result.Append(", ");
    result.Append(s->SelectorText());
  }

  return result.ReleaseString();
}

// Explicit instantiation of member functions visible from other compilation
// units.
template CORE_EXPORT size_t CSSSelectorList::FlattenedSize<false>(
    const CSSSelectorVector<false>& selector_vector);

template CORE_EXPORT CSSSelectorList
CSSSelectorList::AdoptSelectorVector<false>(
    CSSSelectorVector<false>& selector_vector);

template CORE_EXPORT void CSSSelectorList::AdoptSelectorVector<false>(
    CSSSelectorVector<false>& selector_vector,
    CSSSelector* selector_array,
    size_t flattened_size);

template CORE_EXPORT size_t CSSSelectorList::FlattenedSize<true>(
    const CSSSelectorVector<true>& selector_vector);

template CORE_EXPORT CSSSelectorList CSSSelectorList::AdoptSelectorVector<true>(
    CSSSelectorVector<true>& selector_vector);

template CORE_EXPORT void CSSSelectorList::AdoptSelectorVector<true>(
    CSSSelectorVector<true>& selector_vector,
    CSSSelector* selector_array,
    size_t flattened_size);

}  // namespace blink
