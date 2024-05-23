// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_SUBGRID_MIN_MAX_SIZES_CACHE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_SUBGRID_MIN_MAX_SIZES_CACHE_H_

#include "third_party/blink/renderer/core/layout/min_max_sizes.h"

namespace blink {

class SubgridMinMaxSizesCache
    : public GarbageCollected<SubgridMinMaxSizesCache> {
 public:
  SubgridMinMaxSizesCache() = delete;
  SubgridMinMaxSizesCache(const SubgridMinMaxSizesCache&) = delete;
  SubgridMinMaxSizesCache& operator=(const SubgridMinMaxSizesCache&) = delete;

  explicit SubgridMinMaxSizesCache(MinMaxSizes&& min_max_sizes)
      : cached_min_max_sizes_(std::move(min_max_sizes)) {}

  const MinMaxSizes& operator*() const { return cached_min_max_sizes_; }

  void Trace(Visitor*) const {}

 private:
  MinMaxSizes cached_min_max_sizes_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_SUBGRID_MIN_MAX_SIZES_CACHE_H_
