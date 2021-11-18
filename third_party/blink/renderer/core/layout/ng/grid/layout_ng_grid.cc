// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/grid/layout_ng_grid.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"

namespace blink {

LayoutNGGrid::LayoutNGGrid(Element* element)
    : LayoutNGMixin<LayoutBlock>(element) {
  if (element)
    GetDocument().IncLayoutGridCounterNG();
}

void LayoutNGGrid::UpdateBlockLayout(bool relayout_children) {
  if (IsOutOfFlowPositioned()) {
    UpdateOutOfFlowBlockLayout();
    return;
  }

  UpdateInFlowBlockLayout();
}

void LayoutNGGrid::AddChild(LayoutObject* new_child,
                            LayoutObject* before_child) {
  NOT_DESTROYED();
  LayoutBlock::AddChild(new_child, before_child);

  // Out-of-flow grid items do not impact grid placement.
  if (!new_child->IsOutOfFlowPositioned())
    SetGridPlacementDirty(true);
}

void LayoutNGGrid::RemoveChild(LayoutObject* child) {
  NOT_DESTROYED();
  LayoutBlock::RemoveChild(child);

  // Out-of-flow grid items do not impact grid placement.
  if (!child->IsOutOfFlowPositioned())
    SetGridPlacementDirty(true);
}

namespace {

using GridTrackListStyleFunc =
    const blink::GridTrackList& (blink::ComputedStyleBase::*)() const;
using GridAutoFlowStyleFunc =
    blink::GridAutoFlow (blink::ComputedStyle::*)() const;
using NamedGridLinesMapStyleFunc =
    const blink::NamedGridLinesMap& (blink::ComputedStyleBase::*)() const;
using WTFSizeTStyleFunc = WTF::wtf_size_t (blink::ComputedStyleBase::*)() const;

template <typename T>
bool StyleChanged(const ComputedStyle& new_style,
                  const ComputedStyle& old_style,
                  T style_func) {
  auto new_style_binding = WTF::Bind(style_func, WTF::Unretained(&new_style));
  auto old_style_binding = WTF::Bind(style_func, WTF::Unretained(&old_style));
  return std::move(new_style_binding).Run() !=
         std::move(old_style_binding).Run();
}

bool WTFSizeTChanged(wtf_size_t old_value, wtf_size_t new_value) {
  return old_value != new_value;
}

bool ExplicitGridDidResize(const ComputedStyle& new_style,
                           const ComputedStyle& old_style) {
  return WTFSizeTChanged(
             old_style.GridTemplateColumns().LegacyTrackList().size(),
             new_style.GridTemplateColumns().LegacyTrackList().size()) ||
         WTFSizeTChanged(
             old_style.GridTemplateRows().LegacyTrackList().size(),
             new_style.GridTemplateRows().LegacyTrackList().size()) ||
         WTFSizeTChanged(old_style.GridAutoRepeatColumns().size(),
                         new_style.GridAutoRepeatColumns().size()) ||
         WTFSizeTChanged(old_style.GridAutoRepeatRows().size(),
                         new_style.GridAutoRepeatRows().size() ||
                             StyleChanged<WTFSizeTStyleFunc>(
                                 new_style, old_style,
                                 &ComputedStyle::NamedGridAreaColumnCount) ||
                             StyleChanged<WTFSizeTStyleFunc>(
                                 new_style, old_style,
                                 &ComputedStyle::NamedGridAreaColumnCount) ||
                             StyleChanged<WTFSizeTStyleFunc>(
                                 new_style, old_style,
                                 &ComputedStyle::NamedGridAreaRowCount));
}

bool NamedGridLinesDefinitionDidChange(const ComputedStyle& new_style,
                                       const ComputedStyle& old_style) {
  return StyleChanged<NamedGridLinesMapStyleFunc>(
             new_style, old_style, &ComputedStyle::NamedGridRowLines) ||
         StyleChanged<NamedGridLinesMapStyleFunc>(
             new_style, old_style, &ComputedStyle::NamedGridColumnLines) ||
         StyleChanged<NamedGridLinesMapStyleFunc>(
             new_style, old_style, &ComputedStyle::ImplicitNamedGridRowLines) ||
         StyleChanged<NamedGridLinesMapStyleFunc>(
             new_style, old_style,
             &ComputedStyle::ImplicitNamedGridColumnLines);
}

}  // namespace

void LayoutNGGrid::StyleDidChange(StyleDifference diff,
                                  const ComputedStyle* old_style) {
  NOT_DESTROYED();
  LayoutBlock::StyleDidChange(diff, old_style);
  if (!old_style)
    return;

  const ComputedStyle& new_style = StyleRef();
  if (StyleChanged<GridTrackListStyleFunc>(
          new_style, *old_style, &ComputedStyle::GridTemplateColumns) ||
      StyleChanged<GridTrackListStyleFunc>(new_style, *old_style,
                                           &ComputedStyle::GridTemplateRows) ||
      StyleChanged<GridTrackListStyleFunc>(new_style, *old_style,
                                           &ComputedStyle::GridAutoColumns) ||
      StyleChanged<GridTrackListStyleFunc>(new_style, *old_style,
                                           &ComputedStyle::GridAutoRows) ||
      StyleChanged<GridAutoFlowStyleFunc>(new_style, *old_style,
                                          &ComputedStyle::GetGridAutoFlow)) {
    SetGridPlacementDirty(true);
  }

  if (ExplicitGridDidResize(new_style, *old_style) ||
      NamedGridLinesDefinitionDidChange(new_style, *old_style) ||
      (diff.NeedsLayout() && (StyleRef().GridAutoRepeatColumns().size() ||
                              StyleRef().GridAutoRepeatRows().size()))) {
    SetGridPlacementDirty(true);
  }
}

const LayoutNGGridInterface* LayoutNGGrid::ToLayoutNGGridInterface() const {
  NOT_DESTROYED();
  return this;
}

bool LayoutNGGrid::HasCachedPlacementData() const {
  return cached_placement_data_ && !IsGridPlacementDirty();
}

const NGGridPlacementData& LayoutNGGrid::CachedPlacementData() const {
  DCHECK(HasCachedPlacementData());
  return *cached_placement_data_;
}

void LayoutNGGrid::SetCachedPlacementData(
    NGGridPlacementData&& placement_data) {
  cached_placement_data_ = std::move(placement_data);
  SetGridPlacementDirty(false);
}

const NGGridLayoutData* LayoutNGGrid::GridLayoutData() const {
  const auto* cached_layout_result = GetCachedLayoutResult();
  return cached_layout_result ? cached_layout_result->GridLayoutData()
                              : nullptr;
}

wtf_size_t LayoutNGGrid::AutoRepeatCountForDirection(
    const GridTrackSizingDirection track_direction) const {
  NOT_DESTROYED();
  if (!HasCachedPlacementData())
    return 0;

  const bool is_for_columns = track_direction == kForColumns;
  const wtf_size_t auto_repeat_size =
      is_for_columns
          ? StyleRef().GridTemplateColumns().NGTrackList().AutoRepeatSize()
          : StyleRef().GridTemplateRows().NGTrackList().AutoRepeatSize();

  return auto_repeat_size *
         (is_for_columns ? cached_placement_data_->column_auto_repetitions
                         : cached_placement_data_->row_auto_repetitions);
}

wtf_size_t LayoutNGGrid::ExplicitGridStartForDirection(
    const GridTrackSizingDirection track_direction) const {
  NOT_DESTROYED();
  if (!HasCachedPlacementData())
    return 0;
  return (track_direction == kForColumns)
             ? cached_placement_data_->column_start_offset
             : cached_placement_data_->row_start_offset;
}

wtf_size_t LayoutNGGrid::ExplicitGridEndForDirection(
    const GridTrackSizingDirection track_direction) const {
  NOT_DESTROYED();
  const wtf_size_t start_offset =
      ExplicitGridStartForDirection(track_direction);

  return base::checked_cast<wtf_size_t>(
      start_offset +
      ((track_direction == kForColumns)
           ? GridPositionsResolver::ExplicitGridColumnCount(
                 StyleRef(), AutoRepeatCountForDirection(kForColumns))
           : GridPositionsResolver::ExplicitGridRowCount(
                 StyleRef(), AutoRepeatCountForDirection(kForRows))));
}

LayoutUnit LayoutNGGrid::GridGap(
    const GridTrackSizingDirection track_direction) const {
  NOT_DESTROYED();
  const auto* grid_layout_data = GridLayoutData();
  if (!grid_layout_data)
    return LayoutUnit();

  return (track_direction == kForColumns)
             ? grid_layout_data->column_geometry.gutter_size
             : grid_layout_data->row_geometry.gutter_size;
}

LayoutUnit LayoutNGGrid::GridItemOffset(
    const GridTrackSizingDirection track_direction) const {
  NOT_DESTROYED();
  // Distribution offset is baked into the gutter_size in GridNG.
  return LayoutUnit();
}

Vector<LayoutUnit, 1> LayoutNGGrid::TrackSizesForComputedStyle(
    const GridTrackSizingDirection track_direction) const {
  NOT_DESTROYED();
  Vector<LayoutUnit, 1> track_sizes;
  const auto* grid_layout_data = GridLayoutData();
  if (!grid_layout_data)
    return track_sizes;

  const auto& geometry = (track_direction == kForColumns)
                             ? grid_layout_data->column_geometry
                             : grid_layout_data->row_geometry;

  track_sizes.ReserveInitialCapacity(
      std::min<wtf_size_t>(geometry.track_count, kGridMaxTracks));

  for (const auto& range : geometry.ranges) {
    Vector<LayoutUnit> track_sizes_in_range =
        ComputeTrackSizeRepeaterForRange(geometry, range);
    for (wtf_size_t i = 0; i < range.track_count; ++i) {
      track_sizes.emplace_back(
          track_sizes_in_range[i % track_sizes_in_range.size()]);

      // Respect total track count limit.
      DCHECK(track_sizes.size() <= kGridMaxTracks);
      if (track_sizes.size() == kGridMaxTracks)
        return track_sizes;
    }
  }
  return track_sizes;
}

Vector<LayoutUnit> LayoutNGGrid::RowPositions() const {
  NOT_DESTROYED();
  return ComputeExpandedPositions(kForRows);
}

Vector<LayoutUnit> LayoutNGGrid::ColumnPositions() const {
  NOT_DESTROYED();
  return ComputeExpandedPositions(kForColumns);
}

Vector<LayoutUnit> LayoutNGGrid::ComputeTrackSizeRepeaterForRange(
    const NGGridLayoutData::TrackCollectionGeometry& geometry,
    const NGGridLayoutData::RangeData& range) const {
  if (range.IsCollapsed())
    return {LayoutUnit()};

  Vector<LayoutUnit> track_sizes;
  track_sizes.ReserveInitialCapacity(range.set_count);

  const wtf_size_t ending_set_index =
      range.starting_set_index + range.set_count;
  for (wtf_size_t set_index = range.starting_set_index;
       set_index < ending_set_index; ++set_index) {
    DCHECK_LT(set_index + 1, geometry.sets.size());

    // Set information is stored as offsets. To determine the size of a single
    // track in a given set, first determine the total size the set takes up by
    // finding the difference between the offsets.
    LayoutUnit set_size =
        geometry.sets[set_index + 1].offset - geometry.sets[set_index].offset;

    const wtf_size_t set_track_count = geometry.sets[set_index + 1].track_count;
    DCHECK_GT(set_track_count, 0u);

    // Once we have determined the size of the set, we can find the size of a
    // given track by dividing the |set_size| by the |set_track_count|.
    // In some situations, this will leave a remainder, but rather than try to
    // distribute the space unequally between tracks, discard it to prefer equal
    // length tracks.
    track_sizes.emplace_back((set_size / set_track_count) -
                             geometry.gutter_size);
  }
  return track_sizes;
}

Vector<LayoutUnit> LayoutNGGrid::ComputeExpandedPositions(
    const GridTrackSizingDirection track_direction) const {
  Vector<LayoutUnit> expanded_positions;
  const auto* grid_layout_data = GridLayoutData();
  if (!grid_layout_data)
    return expanded_positions;

  const auto& geometry = (track_direction == kForColumns)
                             ? grid_layout_data->column_geometry
                             : grid_layout_data->row_geometry;

  expanded_positions.ReserveInitialCapacity(
      std::min<wtf_size_t>(geometry.track_count + 1, kGridMaxTracks + 1));

  LayoutUnit current_offset = geometry.sets[0].offset;
  expanded_positions.emplace_back(current_offset);

  bool is_last_range_collapsed = true;
  auto BuildExpandedPositions = [&]() {
    for (const auto& range : geometry.ranges) {
      is_last_range_collapsed = range.IsCollapsed();
      Vector<LayoutUnit> track_sizes_in_range =
          ComputeTrackSizeRepeaterForRange(geometry, range);

      for (wtf_size_t i = 0; i < range.track_count; ++i) {
        current_offset +=
            track_sizes_in_range[i % track_sizes_in_range.size()] +
            (range.IsCollapsed() ? LayoutUnit() : geometry.gutter_size);
        expanded_positions.emplace_back(current_offset);

        // Respect total track count limit, don't forget to account for the
        // initial offset.
        DCHECK_LE(expanded_positions.size(),
                  static_cast<unsigned int>(kGridMaxTracks + 1));
        if (expanded_positions.size() == kGridMaxTracks + 1)
          return;
      }
    }
  };

  BuildExpandedPositions();
  if (!is_last_range_collapsed)
    expanded_positions.back() -= geometry.gutter_size;
  return expanded_positions;
}

}  // namespace blink
