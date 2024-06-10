// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/grid/layout_grid.h"

#include "third_party/blink/renderer/core/layout/grid/subgrid_min_max_sizes_cache.h"
#include "third_party/blink/renderer/core/layout/layout_result.h"

namespace blink {

LayoutGrid::LayoutGrid(Element* element) : LayoutBlock(element) {}

void LayoutGrid::Trace(Visitor* visitor) const {
  visitor->Trace(cached_subgrid_min_max_sizes_);
  LayoutBlock::Trace(visitor);
}

void LayoutGrid::AddChild(LayoutObject* new_child, LayoutObject* before_child) {
  NOT_DESTROYED();
  LayoutBlock::AddChild(new_child, before_child);

  // Out-of-flow grid items don't impact placement.
  if (!new_child->IsOutOfFlowPositioned())
    SetGridPlacementDirty(true);
}

void LayoutGrid::RemoveChild(LayoutObject* child) {
  NOT_DESTROYED();
  LayoutBlock::RemoveChild(child);

  // Out-of-flow grid items don't impact placement.
  if (!child->IsOutOfFlowPositioned())
    SetGridPlacementDirty(true);
}

namespace {

bool ExplicitGridDidResize(const ComputedStyle& new_style,
                           const ComputedStyle& old_style) {
  const auto& old_ng_columns_track_list =
      old_style.GridTemplateColumns().track_list;
  const auto& new_ng_columns_track_list =
      new_style.GridTemplateColumns().track_list;
  const auto& old_ng_rows_track_list = old_style.GridTemplateRows().track_list;
  const auto& new_ng_rows_track_list = new_style.GridTemplateRows().track_list;

  return old_ng_columns_track_list.TrackCountWithoutAutoRepeat() !=
             new_ng_columns_track_list.TrackCountWithoutAutoRepeat() ||
         old_ng_rows_track_list.TrackCountWithoutAutoRepeat() !=
             new_ng_rows_track_list.TrackCountWithoutAutoRepeat() ||
         old_ng_columns_track_list.AutoRepeatTrackCount() !=
             new_ng_columns_track_list.AutoRepeatTrackCount() ||
         old_ng_rows_track_list.AutoRepeatTrackCount() !=
             new_ng_rows_track_list.AutoRepeatTrackCount();
}

bool NamedGridLinesDefinitionDidChange(const ComputedStyle& new_style,
                                       const ComputedStyle& old_style) {
  return new_style.GridTemplateRows().named_grid_lines !=
             old_style.GridTemplateRows().named_grid_lines ||
         new_style.GridTemplateColumns().named_grid_lines !=
             old_style.GridTemplateColumns().named_grid_lines;
}

}  // namespace

void LayoutGrid::StyleDidChange(StyleDifference diff,
                                const ComputedStyle* old_style) {
  NOT_DESTROYED();
  LayoutBlock::StyleDidChange(diff, old_style);
  if (!old_style)
    return;

  const auto& new_style = StyleRef();
  const auto& new_grid_columns_track_list =
      new_style.GridTemplateColumns().track_list;
  const auto& new_grid_rows_track_list =
      new_style.GridTemplateRows().track_list;

  if (new_grid_columns_track_list !=
          old_style->GridTemplateColumns().track_list ||
      new_grid_rows_track_list != old_style->GridTemplateRows().track_list ||
      new_style.GridAutoColumns() != old_style->GridAutoColumns() ||
      new_style.GridAutoRows() != old_style->GridAutoRows() ||
      new_style.GetGridAutoFlow() != old_style->GetGridAutoFlow()) {
    SetGridPlacementDirty(true);
  }

  if (ExplicitGridDidResize(new_style, *old_style) ||
      NamedGridLinesDefinitionDidChange(new_style, *old_style) ||
      !base::ValuesEquivalent(new_style.GridTemplateAreas(),
                              old_style->GridTemplateAreas()) ||
      (diff.NeedsLayout() &&
       (new_grid_columns_track_list.AutoRepeatTrackCount() ||
        new_grid_rows_track_list.AutoRepeatTrackCount()))) {
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
  return **cached_subgrid_min_max_sizes_;
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
  // Retrieve the layout data from the last fragment as it has the most
  // up-to-date grid geometry.
  const wtf_size_t fragment_count = PhysicalFragmentCount();
  if (fragment_count == 0)
    return nullptr;
  return GetLayoutResult(fragment_count - 1)->GetGridLayoutData();
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
  const auto* grid_layout_data = LayoutData();
  if (!grid_layout_data)
    return LayoutUnit();

  return (track_direction == kForColumns)
             ? grid_layout_data->Columns().GutterSize()
             : grid_layout_data->Rows().GutterSize();
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
  Vector<LayoutUnit, 1> track_sizes;
  const auto* grid_layout_data = LayoutData();
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

Vector<LayoutUnit> LayoutGrid::RowPositions() const {
  NOT_DESTROYED();
  return ComputeExpandedPositions(kForRows);
}

Vector<LayoutUnit> LayoutGrid::ColumnPositions() const {
  NOT_DESTROYED();
  return ComputeExpandedPositions(kForColumns);
}

Vector<LayoutUnit> LayoutGrid::ComputeTrackSizeRepeaterForRange(
    const GridLayoutTrackCollection& track_collection,
    wtf_size_t range_index) const {
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

Vector<LayoutUnit> LayoutGrid::ComputeExpandedPositions(
    GridTrackSizingDirection track_direction) const {
  Vector<LayoutUnit> expanded_positions;
  const auto* grid_layout_data = LayoutData();
  if (!grid_layout_data)
    return expanded_positions;

  const auto& track_collection = (track_direction == kForColumns)
                                     ? grid_layout_data->Columns()
                                     : grid_layout_data->Rows();

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
