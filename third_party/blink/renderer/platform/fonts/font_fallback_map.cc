// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/font_fallback_map.h"

#include "third_party/blink/renderer/platform/fonts/font_selector.h"

namespace blink {

void FontFallbackMap::Trace(Visitor* visitor) const {
  visitor->Trace(font_selector_);
  FontCacheClient::Trace(visitor);
  FontSelectorClient::Trace(visitor);
}

FontFallbackMap::~FontFallbackMap() {
  AutoLockForParallelTextShaping guard(lock_);
  InvalidateAll();
}

scoped_refptr<FontFallbackList> FontFallbackMap::Get(
    const FontDescription& font_description) {
  recordreplay::Assert("[RUN-1219-1708] FontFallbackMap::Get %d #0",
    record_replay_id_);
  AutoLockForParallelTextShaping guard(lock_);
  auto iter = fallback_list_for_description_.find(font_description);
  if (iter != fallback_list_for_description_.end()) {
    DCHECK(iter->value->IsValid());
    return iter->value;
  }
  recordreplay::Assert("[RUN-1219-1708] FontFallbackMap::Get %d #1",
    record_replay_id_);
  auto add_result = fallback_list_for_description_.insert(
      font_description, FontFallbackList::Create(*this));
  recordreplay::Assert("[RUN-1219-1708] FontFallbackMap::Get %d #2",
    record_replay_id_);
  return add_result.stored_value->value;
}

void FontFallbackMap::Remove(const FontDescription& font_description) {
  recordreplay::Assert("[RUN-1219-1718] FontFallbackMap::Remove #0 %d %u",
    record_replay_id_, StringHash::GetHash(font_description.ToString()));
    
  AutoLockForParallelTextShaping guard(lock_);
  auto iter = fallback_list_for_description_.find(font_description);
  DCHECK_NE(iter, fallback_list_for_description_.end());
  DCHECK(iter->value->IsValid());
  DCHECK(iter->value->HasOneRef());
  fallback_list_for_description_.erase(iter);
}

void FontFallbackMap::InvalidateAll() {
  recordreplay::Assert("[RUN-1219-1718] FontFallbackMap::InvalidateAll %d",
    record_replay_id_);
  lock_.AssertAcquired();
  for (auto& entry : fallback_list_for_description_)
    entry.value->MarkInvalid();
  fallback_list_for_description_.clear();
}

template <typename Predicate>
void FontFallbackMap::InvalidateInternal(Predicate predicate) {
  recordreplay::Assert("[RUN-1219-1718] FontFallbackMap::InvalidateInternal %d #0",
    record_replay_id_);
  lock_.AssertAcquired();
  Vector<FontDescription> invalidated;
  for (auto& entry : fallback_list_for_description_) {
    if (predicate(*entry.value)) {
      recordreplay::Assert("[RUN-1219-1718] FontFallbackMap::InvalidateInternal #1 %d %s",
        record_replay_id_,
        entry.key.ToString().Utf8().data());
      invalidated.push_back(entry.key);
      entry.value->MarkInvalid();
    }
  }
  fallback_list_for_description_.RemoveAll(invalidated);
}

void FontFallbackMap::FontsNeedUpdate(FontSelector*,
                                      FontInvalidationReason reason) {
  recordreplay::Assert(
    "[RUN-1219-1728] FontFallbackMap::FontNeedsUpdate %d",
    this->RecordReplayId()
  );
  AutoLockForParallelTextShaping guard(lock_);
  switch (reason) {
    case FontInvalidationReason::kFontFaceLoaded:
      InvalidateInternal([](const FontFallbackList& fallback_list) {
        return fallback_list.HasLoadingFallback();
      });
      break;
    case FontInvalidationReason::kFontFaceDeleted:
      InvalidateInternal([](const FontFallbackList& fallback_list) {
        return fallback_list.HasCustomFont();
      });
      break;
    default:
      InvalidateAll();
  }
}

void FontFallbackMap::FontCacheInvalidated() {
  recordreplay::Assert("[RUN-1219-1718] FontFallbackMap::FontCacheInvalidated %d",
    record_replay_id_);
  AutoLockForParallelTextShaping guard(lock_);
  InvalidateAll();
}

}  // namespace blink
