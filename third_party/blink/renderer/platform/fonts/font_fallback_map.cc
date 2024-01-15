// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/font_fallback_map.h"

#include "third_party/blink/renderer/platform/fonts/font_selector.h"

namespace blink {

void FontFallbackMap::Trace(Visitor* visitor) const {
  visitor->Trace(font_selector_);
  visitor->Trace(fallback_list_for_description_);
  FontCacheClient::Trace(visitor);
  FontSelectorClient::Trace(visitor);
}

FontFallbackList* FontFallbackMap::Get(
    const FontDescription& font_description) {
  auto add_result =
      fallback_list_for_description_.insert(font_description, nullptr);
  if (add_result.is_new_entry) {
    add_result.stored_value->value =
        MakeGarbageCollected<FontFallbackList>(font_selector_);
  }
  return add_result.stored_value->value;
}

void FontFallbackMap::InvalidateAll() {
  for (auto& entry : fallback_list_for_description_)
    entry.value->MarkInvalid();
  fallback_list_for_description_.clear();
}

template <typename Predicate>
void FontFallbackMap::InvalidateInternal(Predicate predicate) {
  Vector<FontDescription> invalidated;
  for (auto& entry : fallback_list_for_description_) {
    if (predicate(*entry.value)) {
      invalidated.push_back(entry.key);
      entry.value->MarkInvalid();
    }
  }
  fallback_list_for_description_.RemoveAll(invalidated);
}

void FontFallbackMap::FontsNeedUpdate(FontSelector*,
                                      FontInvalidationReason reason) {
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
  InvalidateAll();
}

}  // namespace blink
