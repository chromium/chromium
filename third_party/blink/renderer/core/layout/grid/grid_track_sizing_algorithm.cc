// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "third_party/blink/renderer/core/layout/grid/grid_track_sizing_algorithm.h"

#include "third_party/blink/renderer/core/layout/geometry/box_strut.h"
#include "third_party/blink/renderer/core/layout/grid/grid_item.h"
#include "third_party/blink/renderer/core/layout/grid/grid_track_collection.h"

namespace blink {

namespace {

using GridItemDataPtrVector = Vector<GridItemData*, 16>;

}  // namespace

// static
void GridTrackSizingAlgorithm::CacheGridItemsProperties(
    const GridSizingTrackCollection& track_collection,
    GridItems* grid_items) {
  DCHECK(grid_items);

  GridItemDataPtrVector grid_items_spanning_multiple_ranges;
  const auto track_direction = track_collection.Direction();

  for (auto& grid_item : grid_items->IncludeSubgriddedItems()) {
    if (!grid_item.MustCachePlacementIndices(track_direction)) {
      continue;
    }

    const auto& range_indices = grid_item.RangeIndices(track_direction);
    auto& track_span_properties = (track_direction == kForColumns)
                                      ? grid_item.column_span_properties
                                      : grid_item.row_span_properties;

    grid_item.ComputeSetIndices(track_collection);
    track_span_properties.Reset();

    // If a grid item spans only one range, then we can just cache the track
    // span properties directly. On the contrary, if a grid item spans multiple
    // tracks, it is added to `grid_items_spanning_multiple_ranges` as we need
    // to do more work to cache its track span properties.
    //
    // TODO(layout-dev): Investigate applying this concept to spans > 1.
    if (range_indices.begin == range_indices.end) {
      track_span_properties =
          track_collection.RangeProperties(range_indices.begin);
    } else {
      grid_items_spanning_multiple_ranges.emplace_back(&grid_item);
    }
  }

  if (grid_items_spanning_multiple_ranges.empty()) {
    return;
  }

  auto CompareGridItemsByStartLine =
      [track_direction](GridItemData* lhs, GridItemData* rhs) -> bool {
    return lhs->StartLine(track_direction) < rhs->StartLine(track_direction);
  };
  std::sort(grid_items_spanning_multiple_ranges.begin(),
            grid_items_spanning_multiple_ranges.end(),
            CompareGridItemsByStartLine);

  auto CacheGridItemsSpanningMultipleRangesProperty =
      [&](TrackSpanProperties::PropertyId property) {
        // At this point we have the remaining grid items sorted by start line
        // in the respective direction; this is important since we'll process
        // both, the ranges in the track collection and the grid items,
        // incrementally.
        wtf_size_t current_range_index = 0;
        const wtf_size_t range_count = track_collection.RangeCount();

        for (auto* grid_item : grid_items_spanning_multiple_ranges) {
          // We want to find the first range in the collection that:
          //   - Spans tracks located AFTER the start line of the current grid
          //   item; this can be done by checking that the last track number of
          //   the current range is NOT less than the current grid item's start
          //   line. Furthermore, since grid items are sorted by start line, if
          //   at any point a range is located BEFORE the current grid item's
          //   start line, the same range will also be located BEFORE any
          //   subsequent item's start line.
          //   - Contains a track that fulfills the specified property.
          while (current_range_index < range_count &&
                 (track_collection.RangeEndLine(current_range_index) <=
                      grid_item->StartLine(track_direction) ||
                  !track_collection.RangeProperties(current_range_index)
                       .HasProperty(property))) {
            ++current_range_index;
          }

          // Since we discarded every range in the track collection, any
          // following grid item cannot fulfill the property.
          if (current_range_index == range_count) {
            break;
          }

          // Notice that, from the way we build the ranges of a track collection
          // (see `GridRangeBuilder::EnsureTrackCoverage`), any given range
          // must either be completely contained or excluded from a grid item's
          // span. Thus, if the current range's last track is also located
          // BEFORE the item's end line, then this range, including a track that
          // fulfills the specified property, is completely contained within
          // this item's boundaries. Otherwise, this and every subsequent range
          // are excluded from the grid item's span, meaning that such item
          // cannot satisfy the property we are looking for.
          if (track_collection.RangeEndLine(current_range_index) <=
              grid_item->EndLine(track_direction)) {
            grid_item->SetTrackSpanProperty(property, track_direction);
          }
        }
      };

  CacheGridItemsSpanningMultipleRangesProperty(
      TrackSpanProperties::kHasFlexibleTrack);
  CacheGridItemsSpanningMultipleRangesProperty(
      TrackSpanProperties::kHasIntrinsicTrack);
  CacheGridItemsSpanningMultipleRangesProperty(
      TrackSpanProperties::kHasAutoMinimumTrack);
  CacheGridItemsSpanningMultipleRangesProperty(
      TrackSpanProperties::kHasFixedMinimumTrack);
  CacheGridItemsSpanningMultipleRangesProperty(
      TrackSpanProperties::kHasFixedMaximumTrack);
}

// static
LayoutUnit GridTrackSizingAlgorithm::CalculateGutterSize(
    const ComputedStyle& container_style,
    const LogicalSize& container_available_size,
    GridTrackSizingDirection track_direction,
    LayoutUnit parent_gutter_size) {
  const bool is_for_columns = track_direction == kForColumns;
  const auto& gutter_size =
      is_for_columns ? container_style.ColumnGap() : container_style.RowGap();

  if (!gutter_size) {
    // No specified gutter size means we must use the "normal" gap behavior:
    //   - For standalone grids `parent_gutter_size` will default to zero.
    //   - For subgrids we must provide the parent grid's gutter size.
    return parent_gutter_size;
  }

  return MinimumValueForLength(
      *gutter_size, (is_for_columns ? container_available_size.inline_size
                                    : container_available_size.block_size)
                        .ClampIndefiniteToZero());
}

// static
GridTrackSizingAlgorithm::FirstSetGeometry
GridTrackSizingAlgorithm::ComputeFirstSetGeometry(
    const GridSizingTrackCollection& track_collection,
    const ComputedStyle& container_style,
    const LogicalSize& container_available_size,
    const BoxStrut& container_border_scrollbar_padding) {
  const bool is_for_columns = track_collection.Direction() == kForColumns;

  const auto& available_size = is_for_columns
                                   ? container_available_size.inline_size
                                   : container_available_size.block_size;

  // The default alignment, perform adjustments on top of this.
  FirstSetGeometry geometry{
      track_collection.GutterSize(),
      is_for_columns ? container_border_scrollbar_padding.inline_start
                     : container_border_scrollbar_padding.block_start};

  // If we have an indefinite `available_size` we can't perform any alignment.
  if (available_size == kIndefiniteSize) {
    return geometry;
  }

  const auto& content_alignment = is_for_columns
                                      ? container_style.JustifyContent()
                                      : container_style.AlignContent();

  // Determining the free space is typically unnecessary, i.e., if there is
  // default alignment. Only compute this on-demand.
  auto FreeSpace = [&]() -> LayoutUnit {
    const auto free_space = available_size - track_collection.TotalTrackSize();

    // If overflow is 'safe', make sure we don't overflow the 'start' edge
    // (potentially causing some data loss as the overflow is unreachable).
    return (content_alignment.Overflow() == OverflowAlignment::kSafe)
               ? free_space.ClampNegativeToZero()
               : free_space;
  };

  // TODO(ikilpatrick): 'space-between', 'space-around', and 'space-evenly' all
  // divide by the free-space, and may have a non-zero modulo. Investigate if
  // this should be distributed between the tracks.
  switch (content_alignment.Distribution()) {
    case ContentDistributionType::kSpaceBetween: {
      // Default behavior for 'space-between' is to start align content.
      const wtf_size_t track_count = track_collection.NonCollapsedTrackCount();
      const LayoutUnit free_space = FreeSpace();
      if (track_count < 2 || free_space < LayoutUnit()) {
        return geometry;
      }

      geometry.gutter_size += free_space / (track_count - 1);
      return geometry;
    }
    case ContentDistributionType::kSpaceAround: {
      // Default behavior for 'space-around' is to safe center content.
      const wtf_size_t track_count = track_collection.NonCollapsedTrackCount();
      const LayoutUnit free_space = FreeSpace();
      if (free_space < LayoutUnit()) {
        return geometry;
      }
      if (track_count < 1) {
        geometry.start_offset += free_space / 2;
        return geometry;
      }

      LayoutUnit track_space = free_space / track_count;
      geometry.start_offset += track_space / 2;
      geometry.gutter_size += track_space;
      return geometry;
    }
    case ContentDistributionType::kSpaceEvenly: {
      // Default behavior for 'space-evenly' is to safe center content.
      const wtf_size_t track_count = track_collection.NonCollapsedTrackCount();
      const LayoutUnit free_space = FreeSpace();
      if (free_space < LayoutUnit()) {
        return geometry;
      }

      LayoutUnit track_space = free_space / (track_count + 1);
      geometry.start_offset += track_space;
      geometry.gutter_size += track_space;
      return geometry;
    }
    case ContentDistributionType::kStretch:
    case ContentDistributionType::kDefault:
      break;
  }

  switch (content_alignment.GetPosition()) {
    case ContentPosition::kLeft: {
      DCHECK(is_for_columns);
      if (IsLtr(container_style.Direction())) {
        return geometry;
      }

      geometry.start_offset += FreeSpace();
      return geometry;
    }
    case ContentPosition::kRight: {
      DCHECK(is_for_columns);
      if (IsRtl(container_style.Direction())) {
        return geometry;
      }

      geometry.start_offset += FreeSpace();
      return geometry;
    }
    case ContentPosition::kCenter: {
      geometry.start_offset += FreeSpace() / 2;
      return geometry;
    }
    case ContentPosition::kEnd:
    case ContentPosition::kFlexEnd: {
      geometry.start_offset += FreeSpace();
      return geometry;
    }
    case ContentPosition::kStart:
    case ContentPosition::kFlexStart:
    case ContentPosition::kNormal:
    case ContentPosition::kBaseline:
    case ContentPosition::kLastBaseline:
      return geometry;
  }
}

}  // namespace blink
