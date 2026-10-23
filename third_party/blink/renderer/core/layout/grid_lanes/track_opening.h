// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_LANES_TRACK_OPENING_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_LANES_TRACK_OPENING_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

struct GridItemData;
class GridLayoutSubtree;

using ItemIndexPath = Vector<wtf_size_t>;

// The start and end of a track opening along the stacking axis.
struct TrackOpeningCoordinates {
  DISALLOW_NEW();

 public:
  LayoutUnit Size() const { return end_position - start_position; }

  bool operator==(const TrackOpeningCoordinates&) const = default;

  LayoutUnit start_position;
  LayoutUnit end_position;
};

// Available alignment space of one item. It is shared by the item's alignment
// candidates in all of the lanes that the item spans, and is updated while the
// track openings change. It is the smallest opening directly below the item
// across those lanes, or zero if one of those lanes has no opening directly
// below the item.
class AlignmentSpace final : public GarbageCollected<AlignmentSpace> {
 public:
  // Updates the available alignment space with an opening directly below the
  // item, keeping the smallest size.
  void AccommodateNewOpeningSize(LayoutUnit opening_size) {
    alignment_space_ = std::min(alignment_space_, opening_size);
  }

  LayoutUnit Value() const {
    DCHECK_NE(alignment_space_, LayoutUnit::Max());
    return alignment_space_;
  }

  void Trace(Visitor*) const {}

 private:
  LayoutUnit alignment_space_ = LayoutUnit::Max();
};

// Holds per-item data needed for stacking-axis alignment adjustment. More on
// stacking-axis alignment here:
// https://drafts.csswg.org/css-grid-3/#stacking-self-alignment
struct CORE_EXPORT AlignmentCandidate {
  DISALLOW_NEW();

 public:
  AlignmentCandidate() = default;
  AlignmentCandidate(GridItemData* item,
                     AlignmentSpace* alignment_space,
                     LayoutUnit original_stacking_size,
                     wtf_size_t item_index,
                     GridLayoutSubtree* layout_subtree,
                     const ItemIndexPath* item_index_path);

  bool IsValid() const { return item != nullptr; }

  LayoutUnit AvailableAlignmentSpace() const {
    return alignment_space->Value();
  }

  bool HasAvailableAlignmentSpace() const {
    return AvailableAlignmentSpace() > LayoutUnit();
  }

  void Trace(Visitor* visitor) const;

  Member<GridItemData> item;
  // Shared by every copy of this candidate, one in each lane that the item
  // spans.
  Member<AlignmentSpace> alignment_space;
  // Without a lane graph, the single index identifies a child in the fragment
  // builder. With a lane graph, the path identifies the item in its start lane.
  ItemIndexPath item_index_path;
  // This is only needed for stretch aligned items as they will need to be
  // relaid out once we know the final alignment candidate for a given track
  // opening.
  Member<GridLayoutSubtree> layout_subtree;
  // The candidate's stacking axis size before any stretch alignment
  // adjustments.
  LayoutUnit original_stacking_size;
};

// Struct used to represent openings that occur in the tracks as a result of
// layouts with items of varying span sizes.
struct CORE_EXPORT TrackOpening {
  DISALLOW_NEW();

 public:
  TrackOpening() = default;
  TrackOpening(LayoutUnit start_position, LayoutUnit end_position)
      : coordinates{start_position, end_position} {}

  LayoutUnit Size() const { return coordinates.Size(); }

  // Returns whether this opening is empty for gap decorations. It is empty
  // unless the item above it could align into it. Requires alignment
  // candidates to be tracked and the track openings to be finalized.
  bool IsEmptyForGapDecorations() const {
    return !alignment_candidate.IsValid() ||
           !alignment_candidate.HasAvailableAlignmentSpace();
  }

  void Trace(Visitor* visitor) const;

  // The start and end of the opening in the stacking axis.
  TrackOpeningCoordinates coordinates;

  // The item directly above this opening, used for alignment in the stacking
  // axis.
  AlignmentCandidate alignment_candidate;

  // Path to the lane-specific item directly below this opening.
  ItemIndexPath item_below_index_path;
};

}  // namespace blink

WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(blink::TrackOpening)
WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(blink::AlignmentCandidate)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_LANES_TRACK_OPENING_H_
