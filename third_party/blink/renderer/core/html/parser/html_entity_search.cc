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

#include "third_party/blink/renderer/core/html/parser/html_entity_search.h"

#include "third_party/blink/renderer/core/html/parser/html_entity_table.h"

namespace blink {

static const HTMLEntityTableEntry* Halfway(const HTMLEntityTableEntry* left,
                                           const HTMLEntityTableEntry* right) {
  return &left[(right - left) / 2];
}

HTMLEntitySearch::HTMLEntitySearch()
    : current_length_(0),
      most_recent_match_(nullptr),
      first_(HTMLEntityTable::FirstEntry()),
      last_(HTMLEntityTable::LastEntry()) {}

HTMLEntitySearch::CompareResult HTMLEntitySearch::Compare(
    const HTMLEntityTableEntry* entry,
    UChar next_character) const {
  if (entry->length < current_length_ + 1)
    return kBefore;
  const LChar* entity_string = HTMLEntityTable::EntityString(*entry);
  UChar entry_next_character = entity_string[current_length_];
  if (entry_next_character == next_character)
    return kPrefix;
  return entry_next_character < next_character ? kBefore : kAfter;
}

const HTMLEntityTableEntry* HTMLEntitySearch::FindFirst(
    UChar next_character) const {
  const HTMLEntityTableEntry* left = first_;
  const HTMLEntityTableEntry* right = last_;
  if (left == right)
    return left;
  CompareResult result = Compare(left, next_character);
  if (result == kPrefix)
    return left;
  if (result == kAfter)
    return right;
  while (left + 1 < right) {
    const HTMLEntityTableEntry* probe = Halfway(left, right);
    result = Compare(probe, next_character);
    if (result == kBefore)
      left = probe;
    else {
      DCHECK(result == kAfter || result == kPrefix);
      right = probe;
    }
  }
  DCHECK_EQ(left + 1, right);
  return right;
}

const HTMLEntityTableEntry* HTMLEntitySearch::FindLast(
    UChar next_character) const {
  const HTMLEntityTableEntry* left = first_;
  const HTMLEntityTableEntry* right = last_;
  if (left == right)
    return right;
  CompareResult result = Compare(right, next_character);
  if (result == kPrefix)
    return right;
  if (result == kBefore)
    return left;
  while (left + 1 < right) {
    const HTMLEntityTableEntry* probe = Halfway(left, right);
    result = Compare(probe, next_character);
    if (result == kAfter)
      right = probe;
    else {
      DCHECK(result == kBefore || result == kPrefix);
      left = probe;
    }
  }
  DCHECK_EQ(left + 1, right);
  return left;
}

void HTMLEntitySearch::Advance(UChar next_character) {
  DCHECK(IsEntityPrefix());
  if (!current_length_) {
    first_ = HTMLEntityTable::FirstEntryStartingWith(next_character);
    last_ = HTMLEntityTable::LastEntryStartingWith(next_character);
    if (!first_ || !last_)
      return Fail();
  } else {
    first_ = FindFirst(next_character);
    last_ = FindLast(next_character);
    if (first_ == last_ && Compare(first_, next_character) != kPrefix)
      return Fail();
  }
  ++current_length_;
  if (first_->length != current_length_) {
    return;
  }
  most_recent_match_ = first_;
}

}  // namespace blink
