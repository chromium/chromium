// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/grid/layout_grid.h"

#include "third_party/blink/renderer/core/layout/break_token_algorithm_data.h"
#include "third_party/blink/renderer/core/layout/fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/layout_result.h"

namespace blink {

LayoutGrid::LayoutGrid(Element* element) : LayoutBlock(element) {}

void LayoutGrid::MarkGridDirty() {
  NOT_DESTROYED();
  SetGridPlacementDirty(true);
}

void LayoutGrid::AddChild(LayoutObject* new_child, LayoutObject* before_child) {
  NOT_DESTROYED();
  LayoutBlock::AddChild(new_child, before_child);

  // Counter-intuitively, adding/removing a "position:absolute" child or
  // similar *can* make the placement dirty as the OOF may cause an anonymous
  // child to be split (or merged).
  MarkGridDirty();
}

void LayoutGrid::RemoveChild(LayoutObject* child) {
  NOT_DESTROYED();
  LayoutBlock::RemoveChild(child);

  MarkGridDirty();
}

namespace {

// Returns true if the placement-affecting inputs for a single track direction
// differ between `old_style` and `new_style`.
bool GridPlacementInputsDidChangeInDirection(
    const ComputedStyle& new_style,
    const ComputedStyle& old_style,
    const StyleDifference& diff,
    GridTrackSizingDirection track_direction) {
  const bool is_for_columns = (track_direction == kForColumns);
  const auto& new_template = is_for_columns ? new_style.GridTemplateColumns()
                                            : new_style.GridTemplateRows();
  const auto& new_track_list = new_template.GetTrackList();

  // A full layout may resolve a different number of `auto-fit`/`auto-fill`
  // repetitions, which changes the explicit grid and therefore placement.
  if (diff.NeedsFullLayout() && new_track_list.AutoRepeatTrackCount()) {
    return true;
  }

  const auto& old_template = is_for_columns ? old_style.GridTemplateColumns()
                                            : old_style.GridTemplateRows();
  const auto& old_track_list = old_template.GetTrackList();

  // A resize of the explicit grid or a change in the number of auto-repeat
  // tracks changes how items are placed.
  if (new_track_list.TrackCountWithoutAutoRepeat() !=
          old_track_list.TrackCountWithoutAutoRepeat() ||
      new_track_list.AutoRepeatTrackCount() !=
          old_track_list.AutoRepeatTrackCount()) {
    return true;
  }

  if (new_track_list != old_track_list) {
    return true;
  }

  // Named lines provide targets that items can be placed against by name.
  if (new_template.GetNamedGridLines() != old_template.GetNamedGridLines()) {
    return true;
  }

  // The implicit (auto) track definitions can change how items are placed into
  // the implicit grid.
  const auto& new_auto_tracks =
      is_for_columns ? new_style.GridAutoColumns() : new_style.GridAutoRows();
  const auto& old_auto_tracks =
      is_for_columns ? old_style.GridAutoColumns() : old_style.GridAutoRows();
  return new_auto_tracks != old_auto_tracks;
}

}  // namespace

// static
bool LayoutGrid::GridPlacementInputsDidChange(
    const ComputedStyle& new_style,
    const ComputedStyle& old_style,
    const StyleDifference& diff,
    std::optional<GridTrackSizingDirection> track_direction) {
  // The auto-placement flow and template areas can change how items are placed.
  if (new_style.GetGridAutoFlow() != old_style.GetGridAutoFlow() ||
      !base::ValuesEquivalent(new_style.GridTemplateAreas(),
                              old_style.GridTemplateAreas())) {
    return true;
  }

  // If no track direction is specified, check both directions.
  if ((!track_direction || *track_direction == kForColumns) &&
      GridPlacementInputsDidChangeInDirection(new_style, old_style, diff,
                                              kForColumns)) {
    return true;
  }
  if ((!track_direction || *track_direction == kForRows) &&
      GridPlacementInputsDidChangeInDirection(new_style, old_style, diff,
                                              kForRows)) {
    return true;
  }
  return false;
}

void LayoutGrid::StyleDidChange(
    StyleDifference diff,
    const ComputedStyle* old_style,
    const ComputedStyle& new_style,
    const StyleChangeContext& style_change_context) {
  NOT_DESTROYED();
  LayoutBlock::StyleDidChange(diff, old_style, new_style, style_change_context);
  if (!old_style)
    return;

  if (GridPlacementInputsDidChange(new_style, *old_style, diff)) {
    SetGridPlacementDirty(true);
  }
}

bool LayoutGrid::HasCachedPlacementData() const {
  return cached_placement_data_ && !IsGridPlacementDirty();
}

const GridPlacementData& LayoutGrid::CachedPlacementData() const {
  DCHECK(HasCachedPlacementData());
  return *cached_placement_data_;
}

void LayoutGrid::SetCachedPlacementData(GridPlacementData&& placement_data) {
  cached_placement_data_ = std::move(placement_data);
  SetGridPlacementDirty(false);
}

bool LayoutGrid::HasCachedSubgridMinMaxSizes() const {
  return cached_subgrid_min_max_sizes_ && !IsSubgridMinMaxSizesCacheDirty();
}

const MinMaxSizes& LayoutGrid::CachedSubgridMinMaxSizes() const {
  DCHECK(HasCachedSubgridMinMaxSizes());
  return cached_subgrid_min_max_sizes_->CachedMinMaxSizes();
}

void LayoutGrid::SetSubgridMinMaxSizesCache(MinMaxSizes&& min_max_sizes,
                                            const GridLayoutData& layout_data) {
  cached_subgrid_min_max_sizes_ = MakeGarbageCollected<SubgridMinMaxSizesCache>(
      std::move(min_max_sizes), layout_data);
  SetSubgridMinMaxSizesCacheDirty(false);
}

bool LayoutGrid::ShouldInvalidateSubgridMinMaxSizesCacheFor(
    const GridLayoutData& layout_data) const {
  return HasCachedSubgridMinMaxSizes() &&
         !cached_subgrid_min_max_sizes_->IsValidFor(layout_data);
}

const GridLayoutData* LayoutGrid::LayoutData() const {
  return GetGridLayoutDataFromFragments(this);
}

wtf_size_t LayoutGrid::StitchedRowGapIndex(
    const PhysicalBoxFragment& fragment,
    wtf_size_t gap_index,
    std::optional<wtf_size_t> line_index) const {
  NOT_DESTROYED();
  // This should only be reached when painting gap decorations in a fragmented
  // context.
  CHECK(!fragment.IsOnlyForNode());
  const auto* previous_break_token = FindPreviousBreakToken(fragment);

  // The first fragment has no previous break token, so the stitched index is
  // just `gap_index`.
  if (!previous_break_token) {
    return gap_index;
  }
  return previous_break_token->TokenData()->GetFirstUnprocessedRowGapIndex(
             line_index) +
         gap_index;
}

// static
const GridLayoutData* LayoutGrid::GetGridLayoutDataFromFragments(
    const LayoutBlock* layout_block) {
  CHECK(layout_block);
  // Retrieve the layout data from the last fragment as it has the most
  // up-to-date grid geometry.
  const wtf_size_t fragment_count = layout_block->PhysicalFragmentCount();
  if (fragment_count == 0)
    return nullptr;
  return layout_block->GetLayoutResult(fragment_count - 1)->GetGridLayoutData();
}

// static
LayoutUnit LayoutGrid::ComputeGridGap(
    const GridLayoutData* grid_layout_data,
    GridTrackSizingDirection track_direction) {
  if (!grid_layout_data) {
    return LayoutUnit();
  }

  return (track_direction == kForColumns)
             ? grid_layout_data->Columns().GutterSize()
             : grid_layout_data->Rows().GutterSize();
}

wtf_size_t LayoutGrid::AutoRepeatCountForDirection(
    GridTrackSizingDirection track_direction) const {
  NOT_DESTROYED();
  if (!HasCachedPlacementData())
    return 0;
  return cached_placement_data_->AutoRepeatTrackCount(track_direction);
}

wtf_size_t LayoutGrid::ExplicitGridStartForDirection(
    GridTrackSizingDirection track_direction) const {
  NOT_DESTROYED();
  if (!HasCachedPlacementData())
    return 0;
  return cached_placement_data_->StartOffset(track_direction);
}

wtf_size_t LayoutGrid::ExplicitGridEndForDirection(
    GridTrackSizingDirection track_direction) const {
  NOT_DESTROYED();
  if (!HasCachedPlacementData())
    return 0;

  return base::checked_cast<wtf_size_t>(
      ExplicitGridStartForDirection(track_direction) +
      cached_placement_data_->ExplicitGridTrackCount(track_direction));
}

LayoutUnit LayoutGrid::GridGap(GridTrackSizingDirection track_direction) const {
  NOT_DESTROYED();
  return ComputeGridGap(LayoutData(), track_direction);
}

LayoutUnit LayoutGrid::GridItemOffset(
    GridTrackSizingDirection track_direction) const {
  NOT_DESTROYED();
  // Distribution offset is baked into the gutter_size in GridNG.
  return LayoutUnit();
}

Vector<LayoutUnit, 1> LayoutGrid::TrackSizesForComputedStyle(
    GridTrackSizingDirection track_direction) const {
  NOT_DESTROYED();
  return CollectTrackSizesForComputedStyle(LayoutData(), track_direction);
}

// static
Vector<LayoutUnit, 1> LayoutGrid::CollectTrackSizesForComputedStyle(
    const GridLayoutData* grid_layout_data,
    GridTrackSizingDirection track_direction) {
  Vector<LayoutUnit, 1> track_sizes;
  if (!grid_layout_data)
    return track_sizes;

  const auto& track_collection = (track_direction == kForColumns)
                                     ? grid_layout_data->Columns()
                                     : grid_layout_data->Rows();

  // |EndLineOfImplicitGrid| is equivalent to the total track count.
  track_sizes.ReserveInitialCapacity(std::min<wtf_size_t>(
      track_collection.EndLineOfImplicitGrid(), kGridMaxTracks));

  const wtf_size_t range_count = track_collection.RangeCount();
  for (wtf_size_t i = 0; i < range_count; ++i) {
    auto track_sizes_in_range =
        ComputeTrackSizeRepeaterForRange(track_collection, i);

    const wtf_size_t range_track_count = track_collection.RangeTrackCount(i);
    for (wtf_size_t j = 0; j < range_track_count; ++j) {
      track_sizes.emplace_back(
          track_sizes_in_range[j % track_sizes_in_range.size()]);

      // Respect total track count limit.
      DCHECK(track_sizes.size() <= kGridMaxTracks);
      if (track_sizes.size() == kGridMaxTracks)
        return track_sizes;
    }
  }
  return track_sizes;
}

Vector<LayoutUnit> LayoutGrid::GridTrackPositions(
    GridTrackSizingDirection track_direction) const {
  NOT_DESTROYED();
  return ComputeExpandedPositions(track_direction == kForColumns
                                      ? LayoutData()->Columns()
                                      : LayoutData()->Rows());
}

// static
Vector<LayoutUnit> LayoutGrid::ComputeTrackSizeRepeaterForRange(
    const GridLayoutTrackCollection& track_collection,
    wtf_size_t range_index) {
  const wtf_size_t range_set_count =
      track_collection.RangeSetCount(range_index);

  if (!range_set_count)
    return {LayoutUnit()};

  Vector<LayoutUnit> track_sizes;
  track_sizes.ReserveInitialCapacity(range_set_count);

  const wtf_size_t begin_set_index =
      track_collection.RangeBeginSetIndex(range_index);
  const wtf_size_t end_set_index = begin_set_index + range_set_count;

  for (wtf_size_t i = begin_set_index; i < end_set_index; ++i) {
    LayoutUnit set_size =
        track_collection.GetSetOffset(i + 1) - track_collection.GetSetOffset(i);
    const wtf_size_t set_track_count = track_collection.GetSetTrackCount(i);

    DCHECK_GE(set_size, 0);
    set_size = (set_size - track_collection.GutterSize() * set_track_count)
                   .ClampNegativeToZero();

    // Once we have determined the size of the set, we can find the size of a
    // given track by dividing the |set_size| by the |set_track_count|.
    // In some situations, this will leave a remainder, but rather than try to
    // distribute the space unequally between tracks, discard it to prefer equal
    // length tracks.
    DCHECK_GT(set_track_count, 0u);
    track_sizes.emplace_back(set_size / set_track_count);
  }
  return track_sizes;
}

// static
Vector<LayoutUnit> LayoutGrid::ComputeExpandedPositions(
    const GridLayoutTrackCollection& track_collection) {
  Vector<LayoutUnit> expanded_positions;

  // |EndLineOfImplicitGrid| is equivalent to the total track count.
  expanded_positions.ReserveInitialCapacity(std::min<wtf_size_t>(
      track_collection.EndLineOfImplicitGrid() + 1, kGridMaxTracks + 1));

  auto current_offset = track_collection.GetSetOffset(0);
  expanded_positions.emplace_back(current_offset);

  auto last_applied_gutter_size = LayoutUnit();
  auto BuildExpandedPositions = [&]() {
    const wtf_size_t range_count = track_collection.RangeCount();

    for (wtf_size_t i = 0; i < range_count; ++i) {
      auto track_sizes_in_range =
          ComputeTrackSizeRepeaterForRange(track_collection, i);
      last_applied_gutter_size = track_collection.RangeSetCount(i)
                                     ? track_collection.GutterSize()
                                     : LayoutUnit();

      const wtf_size_t range_track_count = track_collection.RangeTrackCount(i);
      for (wtf_size_t j = 0; j < range_track_count; ++j) {
        current_offset +=
            track_sizes_in_range[j % track_sizes_in_range.size()] +
            last_applied_gutter_size;
        expanded_positions.emplace_back(current_offset);

        // Respect total track count limit, don't forget to account for the
        // initial offset.
        DCHECK(expanded_positions.size() <= kGridMaxTracks + 1);
        if (expanded_positions.size() == kGridMaxTracks + 1)
          return;
      }
    }
  };

  BuildExpandedPositions();
  expanded_positions.back() -= last_applied_gutter_size;
  return expanded_positions;
}

}  // namespace blink
