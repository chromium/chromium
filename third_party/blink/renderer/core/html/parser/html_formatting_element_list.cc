/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/html/parser/html_formatting_element_list.h"

#ifndef NDEBUG
#include <stdio.h>
#endif

namespace blink {

// Biblically, Noah's Ark only had room for two of each animal, but in the
// Book of Hixie (aka
// http://www.whatwg.org/specs/web-apps/current-work/multipage/parsing.html#list-of-active-formatting-elements),
// Noah's Ark of Formatting Elements can fit three of each element.
static const size_t kNoahsArkCapacity = 3;

HTMLFormattingElementList::HTMLFormattingElementList() = default;

HTMLFormattingElementList::~HTMLFormattingElementList() = default;

Element* HTMLFormattingElementList::ClosestElementInScopeWithName(
    const AtomicString& target_name) {
  for (wtf_size_t i = 1; i <= entries_.size(); ++i) {
    const Entry& entry = entries_[entries_.size() - i];
    if (entry.IsMarker())
      return nullptr;
    if (entry.StackItem()->MatchesHTMLTag(target_name))
      return entry.GetElement();
  }
  return nullptr;
}

bool HTMLFormattingElementList::Contains(Element* element) {
  return !!Find(element);
}

HTMLFormattingElementList::Entry* HTMLFormattingElementList::Find(
    Element* element) {
  wtf_size_t index = entries_.ReverseFind(element);
  if (index != kNotFound) {
    // This is somewhat of a hack, and is why this method can't be const.
    return &entries_[index];
  }
  return nullptr;
}

HTMLFormattingElementList::Bookmark HTMLFormattingElementList::BookmarkFor(
    Element* element) {
  wtf_size_t index = entries_.ReverseFind(element);
  DCHECK_NE(index, kNotFound);
  return Bookmark(&at(index));
}

void HTMLFormattingElementList::SwapTo(Element* old_element,
                                       HTMLStackItem* new_item,
                                       const Bookmark& bookmark) {
  DCHECK(Contains(old_element));
  DCHECK(!Contains(new_item->GetElement()));
  if (!bookmark.HasBeenMoved()) {
    DCHECK(bookmark.Mark()->GetElement() == old_element);
    bookmark.Mark()->ReplaceElement(new_item);
    return;
  }
  size_t index = bookmark.Mark() - First();
  SECURITY_DCHECK(index < size());
  entries_.insert(static_cast<wtf_size_t>(index + 1), new_item);
  Remove(old_element);
}

void HTMLFormattingElementList::Append(HTMLStackItem* item) {
  EnsureNoahsArkCondition(item);
  entries_.push_back(item);
}

void HTMLFormattingElementList::Remove(Element* element) {
  wtf_size_t index = entries_.ReverseFind(element);
  if (index != kNotFound)
    entries_.EraseAt(index);
}

void HTMLFormattingElementList::AppendMarker() {
  entries_.push_back(Entry::kMarkerEntry);
}

void HTMLFormattingElementList::ClearToLastMarker() {
  // http://www.whatwg.org/specs/web-apps/current-work/multipage/parsing.html#clear-the-list-of-active-formatting-elements-up-to-the-last-marker
  while (entries_.size()) {
    bool should_stop = entries_.back().IsMarker();
    entries_.pop_back();
    if (should_stop)
      break;
  }
}

void HTMLFormattingElementList::TryToEnsureNoahsArkConditionQuickly(
    HTMLStackItem* new_item,
    HeapVector<Member<HTMLStackItem>>& remaining_candidates) {
  DCHECK(remaining_candidates.IsEmpty());

  if (entries_.size() < kNoahsArkCapacity)
    return;

  // Use a vector with inline capacity to avoid a malloc in the common case of a
  // quickly ensuring the condition.
  HeapVector<Member<HTMLStackItem>, 10> candidates;

  wtf_size_t new_item_attribute_count = new_item->Attributes().size();

  for (wtf_size_t i = entries_.size(); i;) {
    --i;
    Entry& entry = entries_[i];
    if (entry.IsMarker())
      break;

    // Quickly reject obviously non-matching candidates.
    HTMLStackItem* candidate = entry.StackItem();
    if (new_item->LocalName() != candidate->LocalName() ||
        new_item->NamespaceURI() != candidate->NamespaceURI())
      continue;
    if (candidate->Attributes().size() != new_item_attribute_count)
      continue;

    candidates.push_back(candidate);
  }

  // There's room for the new element in the ark. There's no need to copy out
  // the remainingCandidates.
  if (candidates.size() < kNoahsArkCapacity)
    return;

  remaining_candidates.AppendVector(candidates);
}

void HTMLFormattingElementList::EnsureNoahsArkCondition(
    HTMLStackItem* new_item) {
  HeapVector<Member<HTMLStackItem>> candidates;
  TryToEnsureNoahsArkConditionQuickly(new_item, candidates);
  if (candidates.IsEmpty())
    return;

  // We pre-allocate and re-use this second vector to save one malloc per
  // attribute that we verify.
  HeapVector<Member<HTMLStackItem>> remaining_candidates;
  remaining_candidates.ReserveInitialCapacity(candidates.size());

  for (const auto& attribute : new_item->Attributes()) {
    for (const auto& candidate : candidates) {
      // These properties should already have been checked by
      // tryToEnsureNoahsArkConditionQuickly.
      DCHECK_EQ(new_item->Attributes().size(), candidate->Attributes().size());
      DCHECK_EQ(new_item->LocalName(), candidate->LocalName());
      DCHECK_EQ(new_item->NamespaceURI(), candidate->NamespaceURI());

      Attribute* candidate_attribute =
          candidate->GetAttributeItem(attribute.GetName());
      if (candidate_attribute &&
          candidate_attribute->Value() == attribute.Value())
        remaining_candidates.push_back(candidate);
    }

    if (remaining_candidates.size() < kNoahsArkCapacity)
      return;

    candidates.swap(remaining_candidates);
    remaining_candidates.Shrink(0);
  }

  // Inductively, we shouldn't spin this loop very many times. It's possible,
  // however, that we wil spin the loop more than once because of how the
  // formatting element list gets permuted.
  for (wtf_size_t i = kNoahsArkCapacity - 1; i < candidates.size(); ++i)
    Remove(candidates[i]->GetElement());
}

#ifndef NDEBUG

void HTMLFormattingElementList::Show() {
  for (wtf_size_t i = 1; i <= entries_.size(); ++i) {
    const Entry& entry = entries_[entries_.size() - i];
    if (entry.IsMarker())
      LOG(INFO) << "marker";
    else
      LOG(INFO) << *entry.GetElement();
  }
}

#endif

}  // namespace blink
