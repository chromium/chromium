// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "third_party/blink/renderer/core/layout/grid/grid_track_sizing_algorithm.h"

#include "third_party/blink/renderer/core/layout/geometry/box_strut.h"
#include "third_party/blink/renderer/core/layout/grid/grid_item.h"
#include "third_party/blink/renderer/core/layout/grid/grid_track_collection.h"

namespace blink {

namespace {

using ClampedFloat = base::ClampedNumeric<float>;
using GridItemDataPtrVector = HeapVector<Member<GridItemData>, 16>;
using GridSetPtrVector = Vector<GridSet*, 16>;

constexpr float kFloatEpsilon = std::numeric_limits<float>::epsilon();

}  // namespace

// static
void GridTrackSizingAlgorithm::CacheGridItemsProperties(
    const GridLayoutTrackCollection& track_collection,
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
    track_span_properties.ResetType();

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

        for (auto& grid_item : grid_items_spanning_multiple_ranges) {
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

void GridTrackSizingAlgorithm::ComputeUsedTrackSizes(
    const ContributionSizeFunctionRef& contribution_size,
    GridSizingTrackCollection* track_collection,
    GridItems* grid_items,
    bool needs_intrinsic_track_size) const {
  DCHECK(track_collection);
  DCHECK(grid_items);

  // 1. Initialize each track's base size and growth limit.
  // This step is done in `GridSizingTrackCollection::InitializeSets`, which
  // should have already been called to generate the track collection sets.

  // 2. Resolve intrinsic track sizing functions to absolute lengths.
  if (track_collection->HasIntrinsicTrack()) {
    ResolveIntrinsicTrackSizes(contribution_size, track_collection, grid_items);
  }

  // If we are currently calculating the size of intrinsic tracks in an
  // auto repeat(), there is no need to perform the remaining track
  // sizing, since we will need to run another pass with the actual
  // size for the track(s).
  if (needs_intrinsic_track_size) {
    return;
  }

  // If any track still has an infinite growth limit (i.e. it had no items
  // placed in it), set its growth limit to its base size before maximizing.
  track_collection->SetIndefiniteGrowthLimitsToBaseSize();

  // 3. If the free space is positive, distribute it equally to the base sizes
  // of all tracks, freezing tracks as they reach their growth limits (and
  // continuing to grow the unfrozen tracks as needed).
  MaximizeTracks(track_collection);

  // 4. This step sizes flexible tracks using the largest value it can assign to
  // an 'fr' without exceeding the available space.
  if (track_collection->HasFlexibleTrack()) {
    ExpandFlexibleTracks(contribution_size, track_collection, grid_items);
  }

  // 5. Stretch tracks with an 'auto' max track sizing function.
  StretchAutoTracks(track_collection);
}

// Helpers for the track sizing algorithm.
namespace {

LayoutUnit DefiniteGrowthLimit(const GridSet& set) {
  LayoutUnit growth_limit = set.GrowthLimit();
  // For infinite growth limits, substitute the track’s base size.
  return (growth_limit == kIndefiniteSize) ? set.BaseSize() : growth_limit;
}

// Returns the corresponding size to be increased by accommodating a grid item's
// contribution; for intrinsic min track sizing functions, return the base size.
// For intrinsic max track sizing functions, return the growth limit.
LayoutUnit AffectedSizeForContribution(
    const GridSet& set,
    GridItemContributionType contribution_type) {
  switch (contribution_type) {
    case GridItemContributionType::kForIntrinsicMinimums:
    case GridItemContributionType::kForContentBasedMinimums:
    case GridItemContributionType::kForMaxContentMinimums:
      return set.BaseSize();
    case GridItemContributionType::kForIntrinsicMaximums:
    case GridItemContributionType::kForMaxContentMaximums:
      return DefiniteGrowthLimit(set);
    case GridItemContributionType::kForFreeSpace:
      NOTREACHED();
  }
}

void GrowAffectedSizeByPlannedIncrease(
    GridItemContributionType contribution_type,
    GridSet* set) {
  DCHECK(set);

  set->is_infinitely_growable = false;
  const LayoutUnit planned_increase = set->planned_increase;

  // Only grow sets that accommodated a grid item.
  if (planned_increase == kIndefiniteSize) {
    return;
  }

  switch (contribution_type) {
    case GridItemContributionType::kForIntrinsicMinimums:
    case GridItemContributionType::kForContentBasedMinimums:
    case GridItemContributionType::kForMaxContentMinimums:
      set->IncreaseBaseSize(set->BaseSize() + planned_increase);
      return;
    case GridItemContributionType::kForIntrinsicMaximums:
      // Mark any tracks whose growth limit changed from infinite to finite in
      // this step as infinitely growable for the next step.
      set->is_infinitely_growable = set->GrowthLimit() == kIndefiniteSize;
      set->IncreaseGrowthLimit(DefiniteGrowthLimit(*set) + planned_increase);
      return;
    case GridItemContributionType::kForMaxContentMaximums:
      set->IncreaseGrowthLimit(DefiniteGrowthLimit(*set) + planned_increase);
      return;
    case GridItemContributionType::kForFreeSpace:
      NOTREACHED();
  }
}

// Returns true if a set should increase its used size according to the steps in
// https://drafts.csswg.org/css-grid-2/#algo-spanning-items; false otherwise.
bool IsContributionAppliedToSet(const GridSet& set,
                                GridItemContributionType contribution_type) {
  switch (contribution_type) {
    case GridItemContributionType::kForIntrinsicMinimums:
      return set.track_size.HasIntrinsicMinTrackBreadth();
    case GridItemContributionType::kForContentBasedMinimums:
      return set.track_size.HasMinOrMaxContentMinTrackBreadth();
    case GridItemContributionType::kForMaxContentMinimums:
      // TODO(ethavar): Check if the grid container is being sized under a
      // 'max-content' constraint to consider 'auto' min track sizing functions,
      // see https://drafts.csswg.org/css-grid-2/#track-size-max-content-min.
      return set.track_size.HasMaxContentMinTrackBreadth();
    case GridItemContributionType::kForIntrinsicMaximums:
      return set.track_size.HasIntrinsicMaxTrackBreadth();
    case GridItemContributionType::kForMaxContentMaximums:
      return set.track_size.HasMaxContentOrAutoMaxTrackBreadth();
    case GridItemContributionType::kForFreeSpace:
      return true;
  }
}

// https://drafts.csswg.org/css-grid-2/#extra-space
// Returns true if a set's used size should be consider to grow beyond its limit
// (see the "Distribute space beyond limits" section); otherwise, false.
// Note that we will deliberately return false in cases where we don't have a
// collection of tracks different than "all affected tracks".
bool ShouldUsedSizeGrowBeyondLimit(const GridSet& set,
                                   GridItemContributionType contribution_type) {
  switch (contribution_type) {
    case GridItemContributionType::kForIntrinsicMinimums:
    case GridItemContributionType::kForContentBasedMinimums:
      return set.track_size.HasIntrinsicMaxTrackBreadth();
    case GridItemContributionType::kForMaxContentMinimums:
      return set.track_size.HasMaxContentOrAutoMaxTrackBreadth();
    case GridItemContributionType::kForIntrinsicMaximums:
    case GridItemContributionType::kForMaxContentMaximums:
    case GridItemContributionType::kForFreeSpace:
      return false;
  }
}

bool IsDistributionForGrowthLimits(GridItemContributionType contribution_type) {
  switch (contribution_type) {
    case GridItemContributionType::kForIntrinsicMinimums:
    case GridItemContributionType::kForContentBasedMinimums:
    case GridItemContributionType::kForMaxContentMinimums:
    case GridItemContributionType::kForFreeSpace:
      return false;
    case GridItemContributionType::kForIntrinsicMaximums:
    case GridItemContributionType::kForMaxContentMaximums:
      return true;
  }
}

enum class InfinitelyGrowableBehavior { kEnforce, kIgnore };

// We define growth potential = limit - affected size; for base sizes, the limit
// is its growth limit. For growth limits, the limit is infinity if it is marked
// as "infinitely growable", and equal to the growth limit otherwise.
LayoutUnit GrowthPotentialForSet(
    const GridSet& set,
    GridItemContributionType contribution_type,
    InfinitelyGrowableBehavior infinitely_growable_behavior =
        InfinitelyGrowableBehavior::kEnforce) {
  switch (contribution_type) {
    case GridItemContributionType::kForIntrinsicMinimums:
    case GridItemContributionType::kForContentBasedMinimums:
    case GridItemContributionType::kForMaxContentMinimums: {
      LayoutUnit growth_limit = set.GrowthLimit();
      if (growth_limit == kIndefiniteSize) {
        return kIndefiniteSize;
      }

      LayoutUnit increased_base_size =
          set.BaseSize() + set.item_incurred_increase;
      DCHECK_LE(increased_base_size, growth_limit);
      return growth_limit - increased_base_size;
    }
    case GridItemContributionType::kForIntrinsicMaximums:
    case GridItemContributionType::kForMaxContentMaximums: {
      if (infinitely_growable_behavior ==
              InfinitelyGrowableBehavior::kEnforce &&
          set.GrowthLimit() != kIndefiniteSize && !set.is_infinitely_growable) {
        // For growth limits, the potential is infinite if its value is infinite
        // too or if the set is marked as infinitely growable; otherwise, zero.
        return LayoutUnit();
      }

      DCHECK(set.fit_content_limit >= 0 ||
             set.fit_content_limit == kIndefiniteSize);

      // The max track sizing function of a 'fit-content' track is treated as
      // 'max-content' until it reaches the limit specified as the 'fit-content'
      // argument, after which it is treated as having a fixed sizing function
      // of that argument (with a growth potential of zero).
      if (set.fit_content_limit != kIndefiniteSize) {
        LayoutUnit growth_potential = set.fit_content_limit -
                                      DefiniteGrowthLimit(set) -
                                      set.item_incurred_increase;
        return growth_potential.ClampNegativeToZero();
      }
      // Otherwise, this set has infinite growth potential.
      return kIndefiniteSize;
    }
    case GridItemContributionType::kForFreeSpace: {
      LayoutUnit growth_limit = set.GrowthLimit();
      DCHECK_NE(growth_limit, kIndefiniteSize);
      return growth_limit - set.BaseSize();
    }
  }
}

template <typename T>
bool AreEqual(T a, T b) {
  return a == b;
}

template <>
bool AreEqual<float>(float a, float b) {
  return std::abs(a - b) < kFloatEpsilon;
}

// Follow the definitions from https://drafts.csswg.org/css-grid-2/#extra-space;
// notice that this method replaces the notion of "tracks" with "sets".
template <bool is_equal_distribution>
void DistributeExtraSpaceToSets(LayoutUnit extra_space,
                                float flex_factor_sum,
                                GridItemContributionType contribution_type,
                                GridSetPtrVector* sets_to_grow,
                                GridSetPtrVector* sets_to_grow_beyond_limit) {
  DCHECK(sets_to_grow);

  if (extra_space == kIndefiniteSize) {
    // Infinite extra space should only happen when distributing free space at
    // the maximize tracks step; in such case, we can simplify this method by
    // "filling" every track base size up to their growth limit.
    DCHECK_EQ(contribution_type, GridItemContributionType::kForFreeSpace);
    for (auto* set : *sets_to_grow) {
      set->item_incurred_increase =
          GrowthPotentialForSet(*set, contribution_type);
    }
    return;
  }

  DCHECK_GT(extra_space, 0);
#if DCHECK_IS_ON()
  if (IsDistributionForGrowthLimits(contribution_type)) {
    DCHECK_EQ(sets_to_grow, sets_to_grow_beyond_limit);
  }
#endif

  wtf_size_t growable_track_count = 0;
  for (auto* set : *sets_to_grow) {
    set->item_incurred_increase = LayoutUnit();

    // From the first note in https://drafts.csswg.org/css-grid-2/#extra-space:
    //   If the affected size was a growth limit and the track is not marked
    //   "infinitely growable", then each item-incurred increase will be zero.
    //
    // When distributing space to growth limits, we need to increase each track
    // up to its 'fit-content' limit. However, because of the note above, first
    // we should only grow tracks marked as "infinitely growable" up to limits
    // and then grow all affected tracks beyond limits.
    //
    // We can correctly resolve every scenario by doing a single sort of
    // `sets_to_grow`, purposely ignoring the "infinitely growable" flag, then
    // filtering out sets that won't take a share of the extra space at each
    // step; for base sizes this is not required, but if there are no tracks
    // with growth potential > 0, we can optimize by not sorting the sets.
    if (GrowthPotentialForSet(*set, contribution_type)) {
      growable_track_count += set->track_count;
    }
  }

  using ShareRatioType =
      typename std::conditional<is_equal_distribution, wtf_size_t, float>::type;
  DCHECK(is_equal_distribution ||
         !AreEqual<ShareRatioType>(flex_factor_sum, 0));
  ShareRatioType share_ratio_sum =
      is_equal_distribution ? growable_track_count : flex_factor_sum;
  const bool is_flex_factor_sum_overflowing_limits =
      share_ratio_sum >= std::numeric_limits<wtf_size_t>::max();

  // We will sort the tracks by growth potential in non-decreasing order to
  // distribute space up to limits; notice that if we start distributing space
  // equally among all tracks we will eventually reach the limit of a track or
  // run out of space to distribute. If the former scenario happens, it should
  // be easy to see that the group of tracks that will reach its limit first
  // will be that with the least growth potential. Otherwise, if tracks in such
  // group does not reach their limit, every upcoming track with greater growth
  // potential must be able to increase its size by the same amount.
  if (growable_track_count ||
      IsDistributionForGrowthLimits(contribution_type)) {
    auto CompareSetsByGrowthPotential =
        [contribution_type](const GridSet* lhs, const GridSet* rhs) {
          auto growth_potential_lhs = GrowthPotentialForSet(
              *lhs, contribution_type, InfinitelyGrowableBehavior::kIgnore);
          auto growth_potential_rhs = GrowthPotentialForSet(
              *rhs, contribution_type, InfinitelyGrowableBehavior::kIgnore);

          if (growth_potential_lhs == kIndefiniteSize ||
              growth_potential_rhs == kIndefiniteSize) {
            // At this point we know that there is at least one set with
            // infinite growth potential; if `a` has a definite value, then `b`
            // must have infinite growth potential, and thus, `a` < `b`.
            return growth_potential_lhs != kIndefiniteSize;
          }
          // Straightforward comparison of definite growth potentials.
          return growth_potential_lhs < growth_potential_rhs;
        };

    // Only sort for equal distributions; since the growth potential of any
    // flexible set is infinite, they don't require comparing.
    if (AreEqual<float>(flex_factor_sum, 0)) {
      DCHECK(is_equal_distribution);
      std::sort(sets_to_grow->begin(), sets_to_grow->end(),
                CompareSetsByGrowthPotential);
    }
  }

  auto ExtraSpaceShare = [&](const GridSet& set,
                             LayoutUnit growth_potential) -> LayoutUnit {
    DCHECK(growth_potential >= 0 || growth_potential == kIndefiniteSize);

    // If this set won't take a share of the extra space, e.g. has zero growth
    // potential, exit so that this set is filtered out of `share_ratio_sum`.
    if (!growth_potential) {
      return LayoutUnit();
    }

    wtf_size_t set_track_count = set.track_count;
    DCHECK_LE(set_track_count, growable_track_count);

    ShareRatioType set_share_ratio =
        is_equal_distribution ? set_track_count : set.FlexFactor();

    // Since `share_ratio_sum` can be greater than the wtf_size_t limit, cap the
    // value of `set_share_ratio` to prevent overflows.
    if (set_share_ratio > share_ratio_sum) {
      DCHECK(is_flex_factor_sum_overflowing_limits);
      set_share_ratio = share_ratio_sum;
    }

    LayoutUnit extra_space_share;
    if (AreEqual(set_share_ratio, share_ratio_sum)) {
      // If this set's share ratio and the remaining ratio sum are the same, it
      // means that this set will receive all of the remaining space. Hence, we
      // can optimize a little by directly using the extra space as this set's
      // share and break early by decreasing the remaining growable track count
      // to 0 (even if there are further growable tracks, since the share ratio
      // sum will be reduced to 0, their space share will also be 0).
      set_track_count = growable_track_count;
      extra_space_share = extra_space;
    } else {
      DCHECK(!AreEqual<ShareRatioType>(share_ratio_sum, 0));
      DCHECK_LT(set_share_ratio, share_ratio_sum);

      extra_space_share = LayoutUnit::FromRawValue(
          (extra_space.RawValue() * set_share_ratio) / share_ratio_sum);
    }

    if (growth_potential != kIndefiniteSize) {
      extra_space_share = std::min(extra_space_share, growth_potential);
    }
    DCHECK_LE(extra_space_share, extra_space);

    growable_track_count -= set_track_count;
    share_ratio_sum -= set_share_ratio;
    extra_space -= extra_space_share;
    return extra_space_share;
  };

  // Distribute space up to limits:
  //   - For base sizes, grow the base size up to the growth limit.
  //   - For growth limits, the only case where a growth limit should grow at
  //   this step is when its set has already been marked "infinitely growable".
  //   Increase the growth limit up to the 'fit-content' argument (if any); note
  //   that these arguments could prevent this step to fulfill the entirety of
  //   the extra space and further distribution would be needed.
  for (auto* set : *sets_to_grow) {
    // Break early if there are no further tracks to grow.
    if (!growable_track_count) {
      break;
    }
    set->item_incurred_increase =
        ExtraSpaceShare(*set, GrowthPotentialForSet(*set, contribution_type));
  }

  // Distribute space beyond limits:
  //   - For base sizes, every affected track can grow indefinitely.
  //   - For growth limits, grow tracks up to their 'fit-content' argument.
  if (sets_to_grow_beyond_limit && extra_space) {
#if DCHECK_IS_ON()
    // We expect `sets_to_grow_beyond_limit` to be ordered by growth potential
    // for the following section of the algorithm to work.
    //
    // For base sizes, since going beyond limits should only happen after we
    // grow every track up to their growth limits, it should be easy to see that
    // every growth potential is now zero, so they're already ordered.
    //
    // Now let's consider growth limits: we forced the sets to be sorted by
    // growth potential ignoring the "infinitely growable" flag, meaning that
    // ultimately they will be sorted by remaining space to their 'fit-content'
    // parameter (if it exists, infinite otherwise). If we ended up here, we
    // must have filled the sets marked as "infinitely growable" up to their
    // 'fit-content' parameter; therefore, if we only consider sets with
    // remaining space to their 'fit-content' limit in the following
    // distribution step, they should still be ordered.
    LayoutUnit previous_growable_potential;
    for (auto* set : *sets_to_grow_beyond_limit) {
      LayoutUnit growth_potential = GrowthPotentialForSet(
          *set, contribution_type, InfinitelyGrowableBehavior::kIgnore);
      if (growth_potential) {
        if (previous_growable_potential == kIndefiniteSize) {
          DCHECK_EQ(growth_potential, kIndefiniteSize);
        } else {
          DCHECK(growth_potential >= previous_growable_potential ||
                 growth_potential == kIndefiniteSize);
        }
        previous_growable_potential = growth_potential;
      }
    }
#endif

    auto BeyondLimitsGrowthPotential =
        [contribution_type](const GridSet& set) -> LayoutUnit {
      // For growth limits, ignore the "infinitely growable" flag and grow all
      // affected tracks up to their 'fit-content' argument (note that
      // `GrowthPotentialForSet` already accounts for it).
      return !IsDistributionForGrowthLimits(contribution_type)
                 ? kIndefiniteSize
                 : GrowthPotentialForSet(set, contribution_type,
                                         InfinitelyGrowableBehavior::kIgnore);
    };

    // If we reached this point, we must have exhausted every growable track up
    // to their limits, meaning `growable_track_count` should be 0 and we need
    // to recompute it considering their 'fit-content' limits instead.
    DCHECK_EQ(growable_track_count, 0u);

    for (auto* set : *sets_to_grow_beyond_limit) {
      if (BeyondLimitsGrowthPotential(*set)) {
        growable_track_count += set->track_count;
      }
    }

    // In `IncreaseTrackSizesToAccommodateGridItems` we guaranteed that, when
    // dealing with flexible tracks, there shouldn't be any set to grow beyond
    // limits. Thus, the only way to reach the section below is when we are
    // distributing space equally among sets.
    DCHECK(is_equal_distribution);
    share_ratio_sum = growable_track_count;

    for (auto* set : *sets_to_grow_beyond_limit) {
      // Break early if there are no further tracks to grow.
      if (!growable_track_count) {
        break;
      }
      set->item_incurred_increase +=
          ExtraSpaceShare(*set, BeyondLimitsGrowthPotential(*set));
    }
  }
}

void DistributeExtraSpaceToSetsEqually(
    LayoutUnit extra_space,
    GridItemContributionType contribution_type,
    GridSetPtrVector* sets_to_grow,
    GridSetPtrVector* sets_to_grow_beyond_limit = nullptr) {
  DistributeExtraSpaceToSets</*is_equal_distribution=*/true>(
      extra_space, /*flex_factor_sum=*/0, contribution_type, sets_to_grow,
      sets_to_grow_beyond_limit);
}

void DistributeExtraSpaceToWeightedSets(
    LayoutUnit extra_space,
    float flex_factor_sum,
    GridItemContributionType contribution_type,
    GridSetPtrVector* sets_to_grow) {
  DistributeExtraSpaceToSets</*is_equal_distribution=*/false>(
      extra_space, flex_factor_sum, contribution_type, sets_to_grow,
      /*sets_to_grow_beyond_limit=*/nullptr);
}

}  // namespace

void GridTrackSizingAlgorithm::IncreaseTrackSizesToAccommodateGridItems(
    const ContributionSizeFunctionRef& contribution_size,
    base::span<Member<GridItemData>>::iterator group_begin,
    base::span<Member<GridItemData>>::iterator group_end,
    GridItemContributionType contribution_type,
    bool is_group_spanning_flex_track,
    GridSizingTrackCollection* track_collection) const {
  DCHECK(track_collection);

  for (auto set_iterator = track_collection->GetSetIterator();
       !set_iterator.IsAtEnd(); set_iterator.MoveToNextSet()) {
    set_iterator.CurrentSet().planned_increase = kIndefiniteSize;
  }

  GridSetPtrVector sets_to_grow;
  GridSetPtrVector sets_to_grow_beyond_limit;
  const auto track_direction = track_collection->Direction();

  while (group_begin != group_end) {
    auto& grid_item = **group_begin++;
    DCHECK(grid_item.IsSpanningIntrinsicTrack(track_direction));

    sets_to_grow.Shrink(0);
    sets_to_grow_beyond_limit.Shrink(0);

    ClampedFloat flex_factor_sum = 0;
    auto spanned_tracks_size = track_collection->GutterSize() *
                               (grid_item.SpanSize(track_direction) - 1);

    for (auto set_iterator = grid_item.SetIterator(track_collection);
         !set_iterator.IsAtEnd(); set_iterator.MoveToNextSet()) {
      auto& current_set = set_iterator.CurrentSet();

      spanned_tracks_size +=
          AffectedSizeForContribution(current_set, contribution_type);

      if (is_group_spanning_flex_track &&
          !current_set.track_size.HasFlexMaxTrackBreadth()) {
        // From https://drafts.csswg.org/css-grid-2/#algo-spanning-flex-items:
        //   Distributing space only to flexible tracks (i.e. treating all other
        //   tracks as having a fixed sizing function).
        continue;
      }

      if (IsContributionAppliedToSet(current_set, contribution_type)) {
        if (current_set.planned_increase == kIndefiniteSize) {
          current_set.planned_increase = LayoutUnit();
        }

        if (is_group_spanning_flex_track) {
          flex_factor_sum += current_set.FlexFactor();
        }

        sets_to_grow.push_back(&current_set);
        if (ShouldUsedSizeGrowBeyondLimit(current_set, contribution_type)) {
          sets_to_grow_beyond_limit.push_back(&current_set);
        }
      }
    }

    if (sets_to_grow.empty()) {
      continue;
    }

    // Subtract the corresponding size (base size or growth limit) of every
    // spanned track from the grid item's size contribution to find the item's
    // remaining size contribution. For infinite growth limits, substitute with
    // the track's base size. This is the space to distribute, floor it at zero.
    const auto extra_space =
        (contribution_size(contribution_type, &grid_item) - spanned_tracks_size)
            .ClampNegativeToZero();

    if (!extra_space) {
      continue;
    }

    // From https://drafts.csswg.org/css-grid-2/#algo-spanning-flex-items:
    //   If the sum of the flexible sizing functions of all flexible tracks
    //   spanned by the item is greater than zero, distributing space to such
    //   tracks according to the ratios of their flexible sizing functions
    //   rather than distributing space equally.
    if (!is_group_spanning_flex_track || AreEqual<float>(flex_factor_sum, 0)) {
      DistributeExtraSpaceToSetsEqually(
          extra_space, contribution_type, &sets_to_grow,
          sets_to_grow_beyond_limit.empty() ? &sets_to_grow
                                            : &sets_to_grow_beyond_limit);
    } else {
      // 'fr' units are only allowed as a maximum in track definitions, meaning
      // that no set has an intrinsic max track sizing function that would allow
      // it to grow beyond limits (see `ShouldUsedSizeGrowBeyondLimit`).
      DCHECK(sets_to_grow_beyond_limit.empty());
      DistributeExtraSpaceToWeightedSets(extra_space, flex_factor_sum,
                                         contribution_type, &sets_to_grow);
    }

    // For each affected track, if the track's item-incurred increase is larger
    // than its planned increase, set the planned increase to that value.
    for (auto* set : sets_to_grow) {
      DCHECK_NE(set->item_incurred_increase, kIndefiniteSize);
      DCHECK_NE(set->planned_increase, kIndefiniteSize);
      set->planned_increase =
          std::max(set->item_incurred_increase, set->planned_increase);
    }
  }

  for (auto set_iterator = track_collection->GetSetIterator();
       !set_iterator.IsAtEnd(); set_iterator.MoveToNextSet()) {
    GrowAffectedSizeByPlannedIncrease(contribution_type,
                                      &set_iterator.CurrentSet());
  }
}

// https://drafts.csswg.org/css-grid-2/#algo-content
void GridTrackSizingAlgorithm::ResolveIntrinsicTrackSizes(
    const ContributionSizeFunctionRef& contribution_size,
    GridSizingTrackCollection* track_collection,
    GridItems* grid_items) const {
  DCHECK(track_collection);
  DCHECK(grid_items);

  GridItemDataPtrVector reordered_grid_items;
  const auto track_direction = track_collection->Direction();
  reordered_grid_items.ReserveInitialCapacity(grid_items->Size());

  for (auto& grid_item : grid_items->IncludeSubgriddedItems()) {
    if (grid_item.IsSpanningIntrinsicTrack(track_direction) &&
        grid_item.IsConsideredForSizing(track_direction)) {
      reordered_grid_items.emplace_back(&grid_item);
    }
  }

  // Reorder grid items to process them as follows:
  //   - First, consider items spanning a single non-flexible track.
  //   - Next, consider items with span size of 2 not spanning a flexible track.
  //   - Repeat incrementally for items with greater span sizes until all items
  //   not spanning a flexible track have been considered.
  //   - Finally, consider all items spanning a flexible track.
  auto CompareGridItemsForIntrinsicTrackResolution =
      [track_direction](GridItemData* lhs, GridItemData* rhs) -> bool {
    if (lhs->IsSpanningFlexibleTrack(track_direction) ||
        rhs->IsSpanningFlexibleTrack(track_direction)) {
      // Ignore span sizes if one of the items spans a track with a flexible
      // sizing function; items not spanning such tracks should come first.
      return !lhs->IsSpanningFlexibleTrack(track_direction);
    }
    return lhs->SpanSize(track_direction) < rhs->SpanSize(track_direction);
  };
  std::sort(reordered_grid_items.begin(), reordered_grid_items.end(),
            CompareGridItemsForIntrinsicTrackResolution);

  auto current_group_begin = base::span(reordered_grid_items).begin();
  const auto reordered_grid_items_end = base::span(reordered_grid_items).end();

  // First, process the items that don't span a flexible track.
  while (current_group_begin != reordered_grid_items_end &&
         !(*current_group_begin)->IsSpanningFlexibleTrack(track_direction)) {
    // Each iteration considers all items with the same span size.
    const auto current_group_span_size =
        (*current_group_begin)->SpanSize(track_direction);
    auto current_group_end = current_group_begin;

    do {
      DCHECK(!(*current_group_end)->IsSpanningFlexibleTrack(track_direction));
      ++current_group_end;
    } while (current_group_end != reordered_grid_items_end &&
             !(*current_group_end)->IsSpanningFlexibleTrack(track_direction) &&
             (*current_group_end)->SpanSize(track_direction) ==
                 current_group_span_size);

    IncreaseTrackSizesToAccommodateGridItems(
        contribution_size, current_group_begin, current_group_end,
        GridItemContributionType::kForIntrinsicMinimums,
        /*is_group_spanning_flex_track=*/false, track_collection);
    IncreaseTrackSizesToAccommodateGridItems(
        contribution_size, current_group_begin, current_group_end,
        GridItemContributionType::kForContentBasedMinimums,
        /*is_group_spanning_flex_track=*/false, track_collection);
    IncreaseTrackSizesToAccommodateGridItems(
        contribution_size, current_group_begin, current_group_end,
        GridItemContributionType::kForMaxContentMinimums,
        /*is_group_spanning_flex_track=*/false, track_collection);
    IncreaseTrackSizesToAccommodateGridItems(
        contribution_size, current_group_begin, current_group_end,
        GridItemContributionType::kForIntrinsicMaximums,
        /*is_group_spanning_flex_track=*/false, track_collection);
    IncreaseTrackSizesToAccommodateGridItems(
        contribution_size, current_group_begin, current_group_end,
        GridItemContributionType::kForMaxContentMaximums,
        /*is_group_spanning_flex_track=*/false, track_collection);

    // Move to the next group with greater span size.
    current_group_begin = current_group_end;
  }

  // From https://drafts.csswg.org/css-grid-2/#algo-spanning-flex-items:
  //   Increase sizes to accommodate spanning items crossing flexible tracks:
  //   Next, repeat the previous step instead considering (together, rather than
  //   grouped by span size) all items that do span a track with a flexible
  //   sizing function...
#if DCHECK_IS_ON()
  // Every grid item of the remaining group should span a flexible track.
  for (auto it = current_group_begin; it != reordered_grid_items_end; ++it) {
    DCHECK((*it)->IsSpanningFlexibleTrack(track_direction));
  }
#endif

  // Now, process items spanning flexible tracks (if any).
  if (current_group_begin != reordered_grid_items_end) {
    // We can safely skip contributions for maximums since a <flex> definition
    // does not have an intrinsic max track sizing function.
    IncreaseTrackSizesToAccommodateGridItems(
        contribution_size, current_group_begin, reordered_grid_items_end,
        GridItemContributionType::kForIntrinsicMinimums,
        /*is_group_spanning_flex_track=*/true, track_collection);
    IncreaseTrackSizesToAccommodateGridItems(
        contribution_size, current_group_begin, reordered_grid_items_end,
        GridItemContributionType::kForContentBasedMinimums,
        /*is_group_spanning_flex_track=*/true, track_collection);
    IncreaseTrackSizesToAccommodateGridItems(
        contribution_size, current_group_begin, reordered_grid_items_end,
        GridItemContributionType::kForMaxContentMinimums,
        /*is_group_spanning_flex_track=*/true, track_collection);
  }
}

// https://drafts.csswg.org/css-grid-2/#algo-grow-tracks
void GridTrackSizingAlgorithm::MaximizeTracks(
    GridSizingTrackCollection* track_collection) const {
  DCHECK(track_collection);

  const auto free_space = DetermineFreeSpace(*track_collection);
  if (!free_space) {
    return;
  }

  GridSetPtrVector sets_to_grow;
  sets_to_grow.ReserveInitialCapacity(track_collection->GetSetCount());
  for (auto set_iterator = track_collection->GetSetIterator();
       !set_iterator.IsAtEnd(); set_iterator.MoveToNextSet()) {
    sets_to_grow.push_back(&set_iterator.CurrentSet());
  }

  DistributeExtraSpaceToSetsEqually(
      free_space, GridItemContributionType::kForFreeSpace, &sets_to_grow);

  for (auto* set : sets_to_grow) {
    set->IncreaseBaseSize(set->BaseSize() + set->item_incurred_increase);
  }

  // TODO(ethavar): If this would cause the grid to be larger than the grid
  // container’s inner size as limited by its 'max-width/height', then redo this
  // step, treating the available grid space as equal to the grid container’s
  // inner size when it’s sized to its 'max-width/height'.
}

// https://drafts.csswg.org/css-grid-2/#algo-stretch
void GridTrackSizingAlgorithm::StretchAutoTracks(
    GridSizingTrackCollection* track_collection) const {
  DCHECK(track_collection);

  const bool is_for_columns = track_collection->Direction() == kForColumns;
  const auto& content_alignment =
      is_for_columns ? columns_alignment_ : rows_alignment_;

  // Stretching 'auto' tracks should only occur if the container has a 'stretch'
  // (or default) content distribution in the respective axis.
  if (content_alignment.Distribution() != ContentDistributionType::kStretch &&
      (content_alignment.Distribution() != ContentDistributionType::kDefault ||
       content_alignment.GetPosition() != ContentPosition::kNormal)) {
    return;
  }

  // Expand tracks that have an 'auto' max track sizing function by dividing any
  // remaining positive, definite free space equally amongst them.
  GridSetPtrVector sets_to_grow;
  for (auto set_iterator = track_collection->GetSetIterator();
       !set_iterator.IsAtEnd(); set_iterator.MoveToNextSet()) {
    auto& set = set_iterator.CurrentSet();
    if (set.track_size.HasAutoMaxTrackBreadth() &&
        !set.track_size.IsFitContent()) {
      sets_to_grow.push_back(&set);
    }
  }

  if (sets_to_grow.empty()) {
    return;
  }

  // If free space is indefinite, but the grid container has a definite min
  // size, use that size to calculate the free space for this step instead.
  auto free_space = DetermineFreeSpace(*track_collection);
  if (free_space == kIndefiniteSize) {
    free_space = is_for_columns ? min_available_size_.inline_size
                                : min_available_size_.block_size;

    DCHECK_NE(free_space, kIndefiniteSize);
    free_space -= track_collection->TotalTrackSize();
  }

  if (free_space <= 0) {
    return;
  }

  DistributeExtraSpaceToSetsEqually(free_space,
                                    GridItemContributionType::kForFreeSpace,
                                    &sets_to_grow, &sets_to_grow);
  for (auto* set : sets_to_grow) {
    set->IncreaseBaseSize(set->BaseSize() + set->item_incurred_increase);
  }
}

// https://drafts.csswg.org/css-grid-2/#algo-flex-tracks
void GridTrackSizingAlgorithm::ExpandFlexibleTracks(
    const ContributionSizeFunctionRef& contribution_size,
    GridSizingTrackCollection* track_collection,
    GridItems* grid_items) const {
  const auto free_space = DetermineFreeSpace(*track_collection);

  // If the free space is zero or if sizing the grid container under a
  // min-content constraint, the used flex fraction is zero.
  if (!free_space) {
    return;
  }

  // https://drafts.csswg.org/css-grid-2/#algo-find-fr-size
  GridSetPtrVector flexible_sets;
  auto FindFrSize = [&](GridSizingTrackCollection::SetIterator set_iterator,
                        LayoutUnit leftover_space) -> float {
    ClampedFloat flex_factor_sum = 0;
    wtf_size_t total_track_count = 0;
    flexible_sets.Shrink(0);

    while (!set_iterator.IsAtEnd()) {
      auto& set = set_iterator.CurrentSet();

      if (set.track_size.HasFlexMaxTrackBreadth() &&
          !AreEqual<float>(set.FlexFactor(), 0)) {
        flex_factor_sum += set.FlexFactor();
        flexible_sets.push_back(&set);
      } else {
        leftover_space -= set.BaseSize();
      }

      total_track_count += set.track_count;
      set_iterator.MoveToNextSet();
    }

    // Remove the gutters between spanned tracks.
    leftover_space -= track_collection->GutterSize() * (total_track_count - 1);

    if (leftover_space < 0 || flexible_sets.empty()) {
      return 0;
    }

    // From css-grid-2 spec: "If the product of the hypothetical fr size and
    // a flexible track’s flex factor is less than the track’s base size,
    // restart this algorithm treating all such tracks as inflexible."
    //
    // We will process the same algorithm a bit different; since we define the
    // hypothetical fr size as the leftover space divided by the flex factor
    // sum, we can reinterpret the statement above as follows:
    //
    //   (leftover space / flex factor sum) * flexible set's flex factor <
    //       flexible set's base size
    //
    // Reordering the terms of such expression we get:
    //
    //   leftover space / flex factor sum <
    //       flexible set's base size / flexible set's flex factor
    //
    // The term on the right is constant for every flexible set, while the term
    // on the left changes whenever we restart the algorithm treating some of
    // those sets as inflexible. Note that, if the expression above is false for
    // a given set, any other set with a lesser (base size / flex factor) ratio
    // will also fail such expression.
    //
    // Based on this observation, we can process the sets in non-increasing
    // ratio, when the current set does not fulfill the expression, no further
    // set will fulfill it either (and we can return the hypothetical fr size).
    // Otherwise, determine which sets should be treated as inflexible, exclude
    // them from the leftover space and flex factor sum computation, and keep
    // checking the condition for sets with lesser ratios.
    auto CompareSetsByBaseSizeFlexFactorRatio = [](GridSet* lhs,
                                                   GridSet* rhs) -> bool {
      // Avoid divisions by reordering the terms of the comparison.
      return lhs->BaseSize().RawValue() * rhs->FlexFactor() >
             rhs->BaseSize().RawValue() * lhs->FlexFactor();
    };
    std::sort(flexible_sets.begin(), flexible_sets.end(),
              CompareSetsByBaseSizeFlexFactorRatio);

    auto current_set = base::span(flexible_sets).begin();
    while (leftover_space > 0 && current_set != flexible_sets.end()) {
      flex_factor_sum = base::ClampMax(flex_factor_sum, 1);

      auto next_set = current_set;
      while (next_set != flexible_sets.end() &&
             (*next_set)->FlexFactor() * leftover_space.RawValue() <
                 (*next_set)->BaseSize().RawValue() * flex_factor_sum) {
        ++next_set;
      }

      // Any upcoming flexible set will receive a share of free space of at
      // least their base size; return the current hypothetical fr size.
      if (current_set == next_set) {
        DCHECK(!AreEqual<float>(flex_factor_sum, 0));
        return leftover_space.RawValue() / flex_factor_sum;
      }

      // Otherwise, treat all those sets that does not receive a share of free
      // space of at least their base size as inflexible, effectively excluding
      // them from the leftover space and flex factor sum computation.
      for (auto it = current_set; it != next_set; ++it) {
        flex_factor_sum -= (*it)->FlexFactor();
        leftover_space -= (*it)->BaseSize();
      }
      current_set = next_set;
    }
    return 0;
  };

  float fr_size = 0;
  const auto track_direction = track_collection->Direction();

  if (free_space != kIndefiniteSize) {
    // Otherwise, if the free space is a definite length, the used flex fraction
    // is the result of finding the size of an fr using all of the grid tracks
    // and a space to fill of the available grid space.
    fr_size = FindFrSize(track_collection->GetSetIterator(),
                         (track_direction == kForColumns)
                             ? available_size_.inline_size
                             : available_size_.block_size);
  } else {
    // Otherwise, if the free space is an indefinite length, the used flex
    // fraction is the maximum of:
    //   - For each grid item that crosses a flexible track, the result of
    //   finding the size of an fr using all the grid tracks that the item
    //   crosses and a space to fill of the item’s max-content contribution.
    for (auto& grid_item : grid_items->IncludeSubgriddedItems()) {
      if (!grid_item.IsSpanningFlexibleTrack(track_direction) ||
          !grid_item.IsConsideredForSizing(track_direction)) {
        continue;
      }

      const auto item_contribution_size = contribution_size(
          GridItemContributionType::kForMaxContentMaximums, &grid_item);
      fr_size =
          std::max(fr_size, FindFrSize(grid_item.SetIterator(track_collection),
                                       item_contribution_size));
    }

    //   - For each flexible track, if the flexible track’s flex factor is
    //   greater than one, the result of dividing the track’s base size by its
    //   flex factor; otherwise, the track’s base size.
    for (auto set_iterator = track_collection->GetConstSetIterator();
         !set_iterator.IsAtEnd(); set_iterator.MoveToNextSet()) {
      auto& set = set_iterator.CurrentSet();

      if (!set.track_size.HasFlexMaxTrackBreadth()) {
        continue;
      }

      DCHECK_GT(set.track_count, 0u);
      float set_flex_factor = base::ClampMax(set.FlexFactor(), set.track_count);
      fr_size = std::max(set.BaseSize().RawValue() / set_flex_factor, fr_size);
    }
  }

  // Notice that the fr size multiplied by a set's flex factor can result in a
  // non-integer size; since we floor the expanded size to fit in a LayoutUnit,
  // when multiple sets lose the fractional part of the computation we may not
  // distribute the entire free space. We fix this issue by accumulating the
  // leftover fractional part from every flexible set.
  float leftover_size = 0;

  for (auto set_iterator = track_collection->GetSetIterator();
       !set_iterator.IsAtEnd(); set_iterator.MoveToNextSet()) {
    auto& set = set_iterator.CurrentSet();

    if (!set.track_size.HasFlexMaxTrackBreadth()) {
      continue;
    }

    const ClampedFloat fr_share = fr_size * set.FlexFactor() + leftover_size;
    // Add an epsilon to round up values very close to the next integer.
    const auto expanded_size =
        LayoutUnit::FromRawValue(fr_share + kFloatEpsilon);

    if (!expanded_size.MightBeSaturated() && expanded_size >= set.BaseSize()) {
      set.IncreaseBaseSize(expanded_size);
      // The epsilon added above might make |expanded_size| greater than
      // |fr_share|, in that case avoid a negative leftover by flooring to 0.
      leftover_size = base::ClampMax(fr_share - expanded_size.RawValue(), 0);
    }
  }

  // TODO(ethavar): If using this flex fraction would cause the grid to be
  // smaller than the grid container’s min-width/height (or larger than the grid
  // container’s max-width/height), then redo this step, treating the free space
  // as definite and the available grid space as equal to the grid container’s
  // inner size when it’s sized to its min-width/height (max-width/height).
}

// TODO(ikilpatrick): Determine if other uses of this method need to respect
// `min_available_size_` similar to `StretchAutoTracks`.
LayoutUnit GridTrackSizingAlgorithm::DetermineFreeSpace(
    const GridSizingTrackCollection& track_collection) const {
  const bool is_for_columns = track_collection.Direction() == kForColumns;

  // https://drafts.csswg.org/css-sizing-3/#auto-box-sizes: both min-content and
  // max-content block sizes are the size of the content after layout.
  switch (is_for_columns ? sizing_constraint_ : SizingConstraint::kLayout) {
    case SizingConstraint::kLayout: {
      auto free_space = is_for_columns ? available_size_.inline_size
                                       : available_size_.block_size;

      if (free_space != kIndefiniteSize) {
        // If tracks consume more space than the grid container has available,
        // clamp the free space to zero as there's no more room left to grow.
        free_space = (free_space - track_collection.TotalTrackSize())
                         .ClampNegativeToZero();
      }
      return free_space;
    }
    case SizingConstraint::kMaxContent:
      // If sizing under a max-content constraint, the free space is infinite.
      return kIndefiniteSize;
    case SizingConstraint::kMinContent:
      // If sizing under a min-content constraint, the free space is zero.
      return LayoutUnit();
  }
}

}  // namespace blink
