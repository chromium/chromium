// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/unique_font_selector.h"

#include "third_party/blink/renderer/core/html/canvas/canvas_font_cache.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/font_selector.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

UniqueFontSelector::UniqueFontSelector(FontSelector* base_selector)
    : base_selector_(base_selector) {
  if (base_selector_) {
    memory_pressure_listener_registration_.emplace(
        FROM_HERE, base::MemoryPressureListenerTag::kUniqueFontSelector, this);
  }
}

void UniqueFontSelector::Trace(Visitor* visitor) const {
  visitor->Trace(base_selector_);
  visitor->Trace(font_cache_);
}

void UniqueFontSelector::Dispose() {
  if (memory_pressure_listener_registration_) {
    memory_pressure_listener_registration_->Dispose();
  }
}

const Font* UniqueFontSelector::FindOrCreateFont(
    const FontDescription& description) {
  const Font* font;
  {
    auto add_result = font_cache_.insert(description, CacheValue());
    if (!add_result.is_new_entry) {
      auto it =
          lru_list_.MakeIterator(add_result.stored_value->value.list_index);
      it->generation = frame_generation_;
      lru_list_.MoveTo(it, lru_list_.cbegin());
      return add_result.stored_value->value.font;
    }
    font = MakeGarbageCollected<Font>(description, base_selector_);
    add_result.stored_value->value.font = font;
    lru_list_.push_front(LruListKey{description, frame_generation_});
    add_result.stored_value->value.list_index = lru_list_.begin().GetIndex();
  }

  wtf_size_t max_size = CanvasFontCache::MaxFonts();
  while (lru_list_.size() > max_size) {
    auto& value = lru_list_.back();
    // Allow the cache size to exceed MaxFonts() within the same frame.
    if (value.generation == frame_generation_) {
      // However, it should not exceed MaxFonts() * 2.
      if (!RuntimeEnabledFeatures::CanvasTextTexImage2DFixEnabled() ||
          lru_list_.size() <= max_size * 2) {
        break;
      }
    }
    font_cache_.erase(value.description);
    lru_list_.pop_back();
  }

  DCHECK_EQ(font_cache_.size(), lru_list_.size());
  return font;
}

void UniqueFontSelector::DidSwitchFrame() {
  ++frame_generation_;
}

void UniqueFontSelector::RegisterForInvalidationCallbacks(
    FontSelectorClient* client) {
  if (base_selector_ != nullptr) {
    base_selector_->RegisterForInvalidationCallbacks(client);
  }
}

void UniqueFontSelector::OnMemoryPressure(
    base::MemoryPressureLevel memory_pressure_level) {
  if (memory_pressure_level == base::MEMORY_PRESSURE_LEVEL_CRITICAL) {
    font_cache_.clear();
    lru_list_.clear();
  }
}

void UniqueFontSelector::CacheValue::Trace(Visitor* visitor) const {
  visitor->Trace(font);
}

}  // namespace blink
