// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/unique_font_selector.h"

#include "base/feature_list.h"
#include "base/memory_coordinator/memory_coordinator_features.h"
#include "base/memory_coordinator/traits.h"
#include "base/memory_coordinator/utils.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_font_cache.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/font_selector.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

namespace {

constexpr base::MemoryConsumerTraits kUniqueFontSelectorTraits{
    // Bounded by CanvasFontCache::MaxFonts(), keeping footprint under 10MB.
    base::MemoryConsumerTraits::EstimatedMemoryUsage::kSmall,
    // Iterates hash map and list to erase keys.
    base::MemoryConsumerTraits::ReleaseMemoryCost::kRequiresTraversal,
    // Evicted elements can be reconstructed.
    base::MemoryConsumerTraits::InformationRetention::kLossless,
    // Synchronous capacity trimming.
    base::MemoryConsumerTraits::ExecutionType::kSynchronous,
    // Re-instantiating a Font from a description needs minimal CPU overhead.
    base::MemoryConsumerTraits::RecreateMemoryCost::kCheap,
    // Holds references managed by Blink Oilpan GC.
    base::MemoryConsumerTraits::ReleaseGCReferences::kYes};

}  // namespace

UniqueFontSelector::UniqueFontSelector(FontSelector* base_selector)
    : base_selector_(base_selector),
      current_max_fonts_(CanvasFontCache::MaxFonts()) {
  if (base_selector_) {
    memory_consumer_registration_.emplace(
        "UniqueFontSelector", kUniqueFontSelectorTraits, this,
        MemoryConsumerRegistration::CheckUnregister::kDisabled,
        MemoryConsumerRegistration::CheckRegistryExists::kDisabled);
  }
}

void UniqueFontSelector::Trace(Visitor* visitor) const {
  visitor->Trace(base_selector_);
  visitor->Trace(font_cache_);
}

void UniqueFontSelector::Dispose() {
  if (memory_consumer_registration_) {
    memory_consumer_registration_->Dispose();
    memory_consumer_registration_.reset();
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

  // We might have exceeded the size limit of the cache.
  EvictExcessEntries();

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

void UniqueFontSelector::OnUpdateMemoryLimit() {
  if (base::FeatureList::IsEnabled(base::kStatefulMemoryPressure)) {
    unsigned target_max = static_cast<unsigned>(CanvasFontCache::MaxFonts() *
                                                memory_limit_ratio());
    current_max_fonts_ =
        std::max(static_cast<unsigned>(lru_list_.size()), target_max);
  }
}

void UniqueFontSelector::OnReleaseMemory() {
  if (base::FeatureList::IsEnabled(base::kStatefulMemoryPressure)) {
    current_max_fonts_ = static_cast<unsigned>(CanvasFontCache::MaxFonts() *
                                               memory_limit_ratio());
    EvictExcessEntries();
  } else if (memory_limit() <= base::kCriticalMemoryPressureThreshold) {
    font_cache_.clear();
    lru_list_.clear();
  }
}

unsigned UniqueFontSelector::GetCurrentMaxFonts() const {
  return current_max_fonts_;
}

void UniqueFontSelector::EvictExcessEntries() {
  wtf_size_t max_size = GetCurrentMaxFonts();
  while (lru_list_.size() > max_size) {
    auto& value = lru_list_.back();
    // Allow the cache size to exceed `max_size` within the same frame.
    if (value.generation == frame_generation_) {
      // However, it should not exceed `max_size` * 2.
      if (lru_list_.size() <= max_size * 2) {
        break;
      }
    }
    font_cache_.erase(value.description);
    lru_list_.pop_back();
  }

  DCHECK_EQ(font_cache_.size(), lru_list_.size());
}

void UniqueFontSelector::CacheValue::Trace(Visitor* visitor) const {
  visitor->Trace(font);
}

}  // namespace blink
