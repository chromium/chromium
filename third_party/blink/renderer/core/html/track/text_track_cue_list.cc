/*
 * Copyright (C) 2011 Google Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/html/track/text_track_cue_list.h"

#include <algorithm>

#include "base/numerics/safe_conversions.h"

namespace blink {

TextTrackCueList::TextTrackCueList() : first_invalid_index_(0) {}

wtf_size_t TextTrackCueList::length() const {
  return list_.size();
}

TextTrackCue* TextTrackCueList::AnonymousIndexedGetter(wtf_size_t index) const {
  if (index < list_.size())
    return list_[index].Get();
  return nullptr;
}

TextTrackCue* TextTrackCueList::getCueById(const AtomicString& id) const {
  for (const auto& cue : list_) {
    if (cue->id() == id)
      return cue.Get();
  }
  return nullptr;
}

void TextTrackCueList::CollectActiveCues(TextTrackCueList& active_cues) const {
  active_cues.Clear();
  for (auto& cue : list_) {
    if (cue->IsActive())
      active_cues.Add(cue);
  }
}

bool TextTrackCueList::Add(TextTrackCue* cue) {
  // Maintain text track cue order:
  // https://html.spec.whatwg.org/C/#text-track-cue-order
  wtf_size_t index = FindInsertionIndex(cue);

  // FIXME: The cue should not exist in the list in the first place.
  if (!list_.empty() && (index > 0) && (list_[index - 1].Get() == cue))
    return false;

  list_.insert(index, cue);
  InvalidateCueIndex(index);
  return true;
}

static bool CueIsBefore(const TextTrackCue* cue, TextTrackCue* other_cue) {
  if (cue->startTime() < other_cue->startTime())
    return true;

  return cue->startTime() == other_cue->startTime() &&
         cue->endTime() > other_cue->endTime();
}

wtf_size_t TextTrackCueList::FindInsertionIndex(
    const TextTrackCue* cue_to_insert) const {
  auto it =
      std::upper_bound(list_.begin(), list_.end(), cue_to_insert, CueIsBefore);
  wtf_size_t index = base::checked_cast<wtf_size_t>(it - list_.begin());
  SECURITY_DCHECK(index <= list_.size());
  return index;
}

bool TextTrackCueList::Remove(TextTrackCue* cue) {
  wtf_size_t index = list_.Find(cue);
  if (index == kNotFound)
    return false;

  list_.EraseAt(index);
  InvalidateCueIndex(index);
  cue->InvalidateCueIndex();
  return true;
}

void TextTrackCueList::RemoveAll() {
  if (list_.empty())
    return;

  first_invalid_index_ = 0;
  for (auto& cue : list_)
    cue->InvalidateCueIndex();
  Clear();
}

void TextTrackCueList::UpdateCueIndex(TextTrackCue* cue) {
  if (!Remove(cue))
    return;
  Add(cue);
}

void TextTrackCueList::Clear() {
  list_.clear();
}

void TextTrackCueList::InvalidateCueIndex(wtf_size_t index) {
  // Store the smallest (first) index that we know has a cue that does not
  // meet the criteria:
  //   cueIndex(list[index-1]) + 1 == cueIndex(list[index]) [index > 0]
  // This is a stronger requirement than we need, but it's easier to maintain.
  // We can then check if a cue's index is valid by comparing it with
  // |first_invalid_index_| - if it's strictly less it is valid.
  first_invalid_index_ = std::min(first_invalid_index_, index);
}

void TextTrackCueList::ValidateCueIndexes() {
  // Compute new index values for the cues starting at
  // |first_invalid_index_|. If said index is beyond the end of the list, no
  // cues will need to be updated.
  for (wtf_size_t i = first_invalid_index_; i < list_.size(); ++i)
    list_[i]->UpdateCueIndex(i);
  first_invalid_index_ = list_.size();
}

void TextTrackCueList::Trace(Visitor* visitor) const {
  visitor->Trace(list_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
