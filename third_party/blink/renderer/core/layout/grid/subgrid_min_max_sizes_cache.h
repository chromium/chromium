// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_SUBGRID_MIN_MAX_SIZES_CACHE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_SUBGRID_MIN_MAX_SIZES_CACHE_H_

#include "third_party/blink/renderer/core/layout/min_max_sizes.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class SubgridMinMaxSizesCache
    : public GarbageCollected<SubgridMinMaxSizesCache> {
 public:
  SubgridMinMaxSizesCache() = delete;
  SubgridMinMaxSizesCache(const SubgridMinMaxSizesCache&) = delete;
  SubgridMinMaxSizesCache& operator=(const SubgridMinMaxSizesCache&) = delete;

  SubgridMinMaxSizesCache(MinMaxSizes&& min_max_sizes,
                          const GridLayoutData& layout_data)
      : opposite_axis_subgridded_tracks_(
            layout_data.OnlySubgriddedCollection()),
        cached_min_max_sizes_(std::move(min_max_sizes)) {}

  bool IsValidFor(const GridLayoutData& layout_data) const {
    return *layout_data.OnlySubgriddedCollection() ==
           *opposite_axis_subgridded_tracks_;
  }

  const MinMaxSizes& CachedMinMaxSizes() const { return cached_min_max_sizes_; }

  void Trace(Visitor* visitor) const {
    visitor->Trace(opposite_axis_subgridded_tracks_);
  }

 private:
  // The intrinsic sizes of a subgrid's standalone axis might change when the
  // subgridded tracks in the opposite axis change. We keep a copy of these
  // tracks to check if the cache is reusable with the new layout data.
  Member<const GridLayoutTrackCollection> opposite_axis_subgridded_tracks_;

  MinMaxSizes cached_min_max_sizes_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_SUBGRID_MIN_MAX_SIZES_CACHE_H_
