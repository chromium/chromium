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
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace {
// CSSSelector is one of the top types that consume renderer memory,
// so instead of using the |WTF_HEAP_PROFILER_TYPE_NAME| macro in the
// allocations below, pass this type name constant to allow profiling
// in official builds.
const char kCSSSelectorTypeName[] = "blink::CSSSelector";
}

namespace blink {

CSSSelectorList CSSSelectorList::Copy() const {
  CSSSelectorList list;

  unsigned length = this->ComputeLength();
  list.selector_array_ =
      reinterpret_cast<CSSSelector*>(WTF::Partitions::FastMalloc(
          WTF::Partitions::ComputeAllocationSize(length, sizeof(CSSSelector)),
          kCSSSelectorTypeName));
  for (unsigned i = 0; i < length; ++i)
    new (&list.selector_array_[i]) CSSSelector(selector_array_[i]);

  return list;
}

CSSSelectorList CSSSelectorList::ConcatenateListExpansion(
    const CSSSelectorList& expanded,
    const CSSSelectorList& original) {
  unsigned expanded_length = expanded.ComputeLength();
  unsigned original_length = original.ComputeLength();
  unsigned total_length = expanded_length + original_length;

  CSSSelectorList list;
  list.selector_array_ = reinterpret_cast<CSSSelector*>(
      WTF::Partitions::FastMalloc(WTF::Partitions::ComputeAllocationSize(
                                      total_length, sizeof(CSSSelector)),
                                  kCSSSelectorTypeName));

  unsigned list_index = 0;
  for (unsigned i = 0; i < expanded_length; ++i) {
    new (&list.selector_array_[list_index])
        CSSSelector(expanded.selector_array_[i]);
    ++list_index;
  }
  DCHECK(list.selector_array_[list_index - 1].IsLastInOriginalList());
  DCHECK(list.selector_array_[list_index - 1].IsLastInSelectorList());
  list.selector_array_[list_index - 1].SetLastInSelectorList(false);
  for (unsigned i = 0; i < original_length; ++i) {
    new (&list.selector_array_[list_index])
        CSSSelector(original.selector_array_[i]);
    ++list_index;
  }
  DCHECK(list.selector_array_[list_index - 1].IsLastInOriginalList());
  DCHECK(list.selector_array_[list_index - 1].IsLastInSelectorList());
  return list;
}

Vector<const CSSSelector*> SelectorBoundaries(const CSSSelectorList& list) {
  Vector<const CSSSelector*> result;
  for (const CSSSelector* s = list.First(); s; s = list.Next(*s)) {
    result.push_back(s);
  }
  result.push_back(list.First() + list.ComputeLength());
  return result;
}

void AddToList(CSSSelector*& destination,
               const CSSSelector* begin,
               const CSSSelector* end) {
  for (const CSSSelector* current = begin; current != end; ++current) {
    new (destination) CSSSelector(*current);
    destination->SetLastInSelectorList(false);
    destination->SetLastInOriginalList(false);
    destination++;
  }
}

void AddToList(CSSSelector*& destination,
               const CSSSelector* begin,
               const CSSSelector* end,
               const CSSSelector* selector_to_expand) {
  for (const CSSSelector* current = begin; current != end; ++current) {
    new (destination) CSSSelector(*current);
    DCHECK_EQ(current + 1 == end, current->IsLastInTagHistory());
    if (current->IsLastInTagHistory()) {
      destination->SetRelation(selector_to_expand->Relation());
      if (!selector_to_expand->IsLastInTagHistory())
        destination->SetLastInTagHistory(false);
    }
    if (selector_to_expand->GetPseudoType() == CSSSelector::kPseudoWhere ||
        selector_to_expand->IgnoreSpecificity())
      destination->SetIgnoreSpecificity(true);
    destination->SetLastInSelectorList(false);
    destination->SetLastInOriginalList(false);
    destination++;
  }
}

CSSSelectorList CSSSelectorList::ExpandedFirstPseudoClass() const {
  DCHECK(this->RequiresExpansion());
  unsigned original_length = this->ComputeLength();
  Vector<const CSSSelector*> selector_boundaries = SelectorBoundaries(*this);

  size_t begin = 0;
  CSSSelectorList transformed = this->Copy();
  while (!selector_boundaries[begin]->HasPseudoIs() &&
         !selector_boundaries[begin]->HasPseudoWhere())
    ++begin;

  const CSSSelector* selector_to_expand_begin = selector_boundaries[begin];
  const CSSSelector* selector_to_expand_end = selector_boundaries[begin + 1];
  unsigned selector_to_expand_length =
      static_cast<unsigned>(selector_to_expand_end - selector_to_expand_begin);

  const CSSSelector* simple_selector = selector_to_expand_begin;
  while (simple_selector->GetPseudoType() != CSSSelector::kPseudoIs &&
         simple_selector->GetPseudoType() != CSSSelector::kPseudoWhere) {
    simple_selector = simple_selector->TagHistory();
  }

  unsigned inner_selector_length =
      simple_selector->SelectorList()->ComputeLength();
  Vector<const CSSSelector*> selector_arg_boundaries =
      SelectorBoundaries(*simple_selector->SelectorList());

  wtf_size_t num_args =
      SafeCast<wtf_size_t>(selector_arg_boundaries.size()) - 1;
  unsigned other_selectors_length = original_length - selector_to_expand_length;

  wtf_size_t expanded_selector_list_length =
      (selector_to_expand_length - 1) * num_args + inner_selector_length +
      other_selectors_length;

  // Do not perform expansion if the selector list size is too large to create
  // RuleData
  if (expanded_selector_list_length > 8192)
    return CSSSelectorList();

  CSSSelectorList list;
  list.selector_array_ =
      reinterpret_cast<CSSSelector*>(WTF::Partitions::FastMalloc(
          WTF::Partitions::ComputeAllocationSize(expanded_selector_list_length,
                                                 sizeof(CSSSelector)),
          kCSSSelectorTypeName));

  CSSSelector* destination = list.selector_array_;

  AddToList(destination, selector_boundaries[0], selector_to_expand_begin);
  for (wtf_size_t i = 0; i < num_args; ++i) {
    AddToList(destination, selector_to_expand_begin, simple_selector);
    AddToList(destination, selector_arg_boundaries[i],
              selector_arg_boundaries[i + 1], simple_selector);
    AddToList(destination, simple_selector + 1, selector_to_expand_end);
  }
  AddToList(destination, selector_to_expand_end, selector_boundaries.back());

  DCHECK(destination == list.selector_array_ + expanded_selector_list_length);

  list.selector_array_[expanded_selector_list_length - 1].SetLastInOriginalList(
      true);
  list.selector_array_[expanded_selector_list_length - 1].SetLastInSelectorList(
      true);

  return list;
}

CSSSelectorList CSSSelectorList::TransformForListExpansion() {
  DCHECK_GT(this->ComputeLength(), 0u);
  DCHECK(
      this->selector_array_[this->ComputeLength() - 1].IsLastInOriginalList());
  DCHECK(this->RequiresExpansion());

  // Append the expanded form of matches to the original selector list
  CSSSelectorList transformed = this->Copy();
  do {
    transformed = transformed.ExpandedFirstPseudoClass();
  } while (transformed.RequiresExpansion());

  if (transformed.ComputeLength() == 0)
    return CSSSelectorList();
  return CSSSelectorList::ConcatenateListExpansion(transformed, *this);
}

bool CSSSelectorList::HasPseudoIs() const {
  for (const CSSSelector* s = FirstForCSSOM(); s; s = Next(*s)) {
    if (s->HasPseudoIs())
      return true;
  }
  return false;
}

bool CSSSelectorList::HasPseudoWhere() const {
  for (const CSSSelector* s = FirstForCSSOM(); s; s = Next(*s)) {
    if (s->HasPseudoWhere())
      return true;
  }
  return false;
}

bool CSSSelectorList::RequiresExpansion() const {
  for (const CSSSelector* s = FirstForCSSOM(); s; s = Next(*s)) {
    if (s->HasPseudoIs() || s->HasPseudoWhere())
      return true;
  }
  return false;
}

CSSSelectorList CSSSelectorList::AdoptSelectorVector(
    Vector<std::unique_ptr<CSSParserSelector>>& selector_vector) {
  size_t flattened_size = 0;
  for (wtf_size_t i = 0; i < selector_vector.size(); ++i) {
    for (CSSParserSelector* selector = selector_vector[i].get(); selector;
         selector = selector->TagHistory())
      ++flattened_size;
  }
  DCHECK(flattened_size);

  CSSSelectorList list;
  list.selector_array_ = reinterpret_cast<CSSSelector*>(
      WTF::Partitions::FastMalloc(WTF::Partitions::ComputeAllocationSize(
                                      flattened_size, sizeof(CSSSelector)),
                                  kCSSSelectorTypeName));
  wtf_size_t array_index = 0;
  for (wtf_size_t i = 0; i < selector_vector.size(); ++i) {
    CSSParserSelector* current = selector_vector[i].get();
    while (current) {
      // Move item from the parser selector vector into selector_array_ without
      // invoking destructor (Ugh.)
      CSSSelector* current_selector = current->ReleaseSelector().release();
      memcpy(&list.selector_array_[array_index], current_selector,
             sizeof(CSSSelector));
      WTF::Partitions::FastFree(current_selector);

      current = current->TagHistory();
      DCHECK(!list.selector_array_[array_index].IsLastInSelectorList());
      if (current)
        list.selector_array_[array_index].SetLastInTagHistory(false);
      ++array_index;
    }
    DCHECK(list.selector_array_[array_index - 1].IsLastInTagHistory());
  }
  DCHECK_EQ(flattened_size, array_index);
  list.selector_array_[array_index - 1].SetLastInSelectorList(true);
  list.selector_array_[array_index - 1].SetLastInOriginalList(true);
  selector_vector.clear();

  return list;
}

const CSSSelector* CSSSelectorList::FirstForCSSOM() const {
  const CSSSelector* s = this->First();
  if (!s)
    return nullptr;
  while (this->Next(*s))
    s = this->Next(*s);
  if (this->NextInFullList(*s))
    return this->NextInFullList(*s);
  return this->First();
}

unsigned CSSSelectorList::ComputeLength() const {
  if (!selector_array_)
    return 0;
  CSSSelector* current = selector_array_;
  while (!current->IsLastInSelectorList())
    ++current;
  return static_cast<unsigned>(current - selector_array_) + 1;
}

void CSSSelectorList::DeleteSelectors() {
  DCHECK(selector_array_);

  bool finished = false;
  for (CSSSelector* s = selector_array_; !finished; ++s) {
    finished = s->IsLastInSelectorList();
    s->~CSSSelector();
  }

  WTF::Partitions::FastFree(selector_array_);
}

String CSSSelectorList::SelectorsText() const {
  StringBuilder result;

  for (const CSSSelector* s = FirstForCSSOM(); s; s = Next(*s)) {
    if (s != FirstForCSSOM())
      result.Append(", ");
    result.Append(s->SelectorText());
  }

  return result.ToString();
}

}  // namespace blink
