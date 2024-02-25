// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/measure_cache.h"

#include "third_party/blink/renderer/core/layout/geometry/fragment_geometry.h"
#include "third_party/blink/renderer/core/layout/layout_result.h"
#include "third_party/blink/renderer/core/layout/layout_utils.h"

namespace blink {

const LayoutResult* MeasureCache::Find(
    const BlockNode& node,
    const ConstraintSpace& new_space,
    std::optional<FragmentGeometry>* fragment_geometry) {
  for (auto it = cache_.rbegin(); it != cache_.rend(); ++it) {
    const auto* result = it->Get();
    if (CalculateSizeBasedLayoutCacheStatus(node, nullptr, *result, new_space,
                                            fragment_geometry) !=
        LayoutCacheStatus::kHit) {
      continue;
    }

    if (it == cache_.rbegin()) {
      return result;
    }

    // Shift this result to the back of the cache.
    cache_.EraseAt(static_cast<wtf_size_t>(std::distance(it, cache_.rend())) -
                   1u);
    cache_.emplace_back(result);
    return result;
  }

  return nullptr;
}

void MeasureCache::Add(const LayoutResult* result) {
  if (cache_.size() == kMaxCacheEntries) {
    cache_.EraseAt(0);
  }
  cache_.push_back(result);
}

void MeasureCache::Clear() {
  InvalidateItems();
  cache_.resize(0);
}

void MeasureCache::LayoutObjectWillBeDestroyed() {
  for (auto& entry : cache_) {
    entry->GetPhysicalFragment().LayoutObjectWillBeDestroyed();
  }
}

void MeasureCache::InvalidateItems() {
  for (auto& entry : cache_) {
    LayoutBox::InvalidateItems(*entry);
  }
}

void MeasureCache::SetFragmentChildrenInvalid(const LayoutResult* except) {
  for (auto& entry : cache_) {
    if (entry != except) {
      entry->GetMutableForLayoutBoxCachedResults().SetFragmentChildrenInvalid();
    }
  }
}

const LayoutResult* MeasureCache::GetLastForTesting() const {
  return cache_.empty() ? nullptr : cache_.back().Get();
}

}  // namespace blink
