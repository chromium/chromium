// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_GAP_GEOMETRY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_GAP_GEOMETRY_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/gap/gap_intersection.h"
#include "third_party/blink/renderer/core/layout/gap/main_gap.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_offset.h"
#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/style/grid_enums.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class ComputedStyle;
class PhysicalBoxFragment;

// This bitmask indicates whether an intersection is blocked due to the presence
// of a spanning item in one or both directions. When considering column gaps,
// `kBlockedBefore` means the intersection is blocked by a spanning item
// upwards and `kBlockedAfter` means it is blocked downwards. When
// considering row gaps, `kBlockedBefore` means the intersection is blocked by a
// spanning item to the left and `kBlockedAfter` means it is blocked to the
// right.
class CORE_EXPORT BlockedStatus {
 public:
  enum BlockStatusId : unsigned {
    kNone = 0,
    kBlockedBefore = 1 << 0,
    kBlockedAfter = 1 << 1,
  };

  inline bool HasBlockedStatus(BlockStatusId status) const {
    return (status_ & status) != 0;
  }
  inline void SetBlockedStatus(BlockStatusId status) { status_ |= status; }

  inline bool operator&(BlockStatusId status) const {
    return HasBlockedStatus(status);
  }
  inline BlockedStatus& operator|=(BlockStatusId status) {
    SetBlockedStatus(status);
    return *this;
  }

 private:
  wtf_size_t status_{kNone};
};

using MainGaps = Vector<MainGap>;
using CrossGaps = Vector<CrossGap>;

// Gap geometry is used to determine gap locations for the purpose of painting
// gap decorations.
//
// See third_party/blink/renderer/core/layout/gap/README.md for more.
class CORE_EXPORT GapGeometry : public GarbageCollected<GapGeometry> {
 public:
  enum ContainerType {
    kGrid,
    kFlex,
    kMultiColumn,
  };

  explicit GapGeometry(ContainerType container_type)
      : container_type_(container_type) {}

  // This copy-esque constructor allows creating a new GapGeometry
  // instance based on an existing one, while replacing the main gaps and
  // content block offsets. This is useful for fragmentation where most states
  // remain the same, but the content block offsets and main gaps may differ.
  GapGeometry(const GapGeometry& other,
              MainGaps&& new_main_gaps,
              LayoutUnit new_content_block_start,
              LayoutUnit new_content_block_end)
      : inline_gap_size_(other.inline_gap_size_),
        block_gap_size_(other.block_gap_size_),
        container_type_(other.container_type_),
        main_gaps_(std::move(new_main_gaps)),
        cross_gaps_(other.cross_gaps_),
        flex_cross_gap_sizes_(other.flex_cross_gap_sizes_),
        content_inline_start_(other.content_inline_start_),
        content_inline_end_(other.content_inline_end_),
        content_block_start_(new_content_block_start),
        content_block_end_(new_content_block_end),
        main_direction_(other.main_direction_),
        main_gap_running_index_(other.main_gap_running_index_) {}

  void Trace(Visitor* visitor) const {}

  bool operator==(const GapGeometry& other) const {
    return inline_gap_size_ == other.inline_gap_size_ &&
           block_gap_size_ == other.block_gap_size_ &&
           container_type_ == other.container_type_ &&
           main_gaps_ == other.main_gaps_ && cross_gaps_ == other.cross_gaps_ &&
           content_inline_start_ == other.content_inline_start_ &&
           content_inline_end_ == other.content_inline_end_ &&
           content_block_start_ == other.content_block_start_ &&
           content_block_end_ == other.content_block_end_ &&
           main_direction_ == other.main_direction_;
  }

  // Per-side outward extension (in logical space) caused by negative gap
  // decoration insets pushing decorations past the content box edges.
  struct GapDecorationInkOutsets {
    LayoutUnit inline_start;
    LayoutUnit inline_end;
    LayoutUnit block_start;
    LayoutUnit block_end;

    LayoutUnit InlineOutsetThickness() const {
      return inline_start + inline_end;
    }
    LayoutUnit BlockOutsetThickness() const { return block_start + block_end; }
  };

  // Computes the physical bounding rect for gap decorations ink overflow.
  // `inline_thickness` / `block_thickness` account for the rule width.
  // `outsets` accounts for negative insets that push decorations past the
  // content box edges.
  PhysicalRect ComputeInkOverflowForGaps(
      WritingDirectionMode writing_direction,
      const PhysicalSize& container_size,
      LayoutUnit inline_thickness,
      LayoutUnit block_thickness,
      const GapDecorationInkOutsets& outsets) const;

  // Returns the gap size perpendicular to a rule running in `direction`
  // (the "crossing" gap for that rule). Used as the percentage basis for
  // junction gap-decoration insets.
  //
  // For flex containers, per-line cross gap sizes can differ due to content
  // distribution; this returns the maximum across all lines as a conservative
  // bound (for ink overflow computation).
  LayoutUnit GetCrossingGapSize(GridTrackSizingDirection direction) const;

  ContainerType GetContainerType() const { return container_type_; }

  // Returns true when row gaps span multiple fragments.
  bool HasRowGapFragmentation(const PhysicalBoxFragment& box_fragment,
                              bool is_main) const;

  void SetInlineGapSize(LayoutUnit size) { inline_gap_size_ = size; }
  LayoutUnit GetInlineGapSize() const { return inline_gap_size_; }

  void SetBlockGapSize(LayoutUnit size) { block_gap_size_ = size; }
  LayoutUnit GetBlockGapSize() const { return block_gap_size_; }

  LayoutUnit GetFlexCrossGapSize(wtf_size_t line_index) const {
    CHECK(flex_cross_gap_sizes_.has_value());
    CHECK_GT(flex_cross_gap_sizes_->size(), line_index);
    return (*flex_cross_gap_sizes_)[line_index];
  }

  void SetContentInlineOffsets(LayoutUnit start_offset, LayoutUnit end_offset);
  LayoutUnit GetContentInlineStart() const { return content_inline_start_; }
  LayoutUnit GetContentInlineEnd() const { return content_inline_end_; }

  void SetContentBlockOffsets(LayoutUnit start_offset, LayoutUnit end_offset);
  LayoutUnit GetContentBlockStart() const { return content_block_start_; }
  LayoutUnit GetContentBlockEnd() const { return content_block_end_; }

  void ReserveMainGaps(wtf_size_t capacity) {
    main_gaps_.ReserveInitialCapacity(capacity);
  }

  void ReserveCrossGaps(wtf_size_t capacity) {
    cross_gaps_.ReserveInitialCapacity(capacity);
  }

  MainGap& AddMainGap(LayoutUnit offset,
                      SpannerMainGapType type = SpannerMainGapType::kNone) {
    main_gaps_.emplace_back(offset, type);
    return main_gaps_.back();
  }

  CrossGap& AddCrossGap(LogicalOffset offset) {
    cross_gaps_.emplace_back(offset);
    return cross_gaps_.back();
  }

  CrossGap& AddCrossGap(LogicalOffset offset,
                        CrossGap::EdgeIntersectionState state) {
    cross_gaps_.emplace_back(offset, state);
    return cross_gaps_.back();
  }

  void RemoveLastMainGap() {
    CHECK(!main_gaps_.empty());
    main_gaps_.pop_back();
  }

  MainGap& MainGapAt(wtf_size_t index) {
    CHECK_LT(index, main_gaps_.size());
    return main_gaps_[index];
  }

  CrossGap& CrossGapAt(wtf_size_t index) {
    CHECK_LT(index, cross_gaps_.size());
    return cross_gaps_[index];
  }

  wtf_size_t MainGapCount() const { return main_gaps_.size(); }
  wtf_size_t CrossGapCount() const { return cross_gaps_.size(); }

  // Per-line main axis gap sizes for flex containers. This is needed because
  // different lines in a flex container can have different effective gap sizes
  // due to content distribution space.
  void AddFlexCrossGapSize(LayoutUnit size) {
    if (!flex_cross_gap_sizes_.has_value()) {
      flex_cross_gap_sizes_.emplace();
    }
    flex_cross_gap_sizes_->push_back(size);
  }

  wtf_size_t GetFlexCrossGapSizeCount() const {
    return flex_cross_gap_sizes_.has_value() ? flex_cross_gap_sizes_->size()
                                             : 0;
  }

  void Finalize() {
    main_gap_running_index_ = 0;
    while (main_gap_running_index_ < main_gaps_.size() &&
           !main_gaps_[main_gap_running_index_].HasCrossGapsBefore()) {
      ++main_gap_running_index_;
    }
  }

  void SetMainDirection(GridTrackSizingDirection direction) {
    main_direction_ = direction;
  }

  GridTrackSizingDirection GetMainDirection() const { return main_direction_; }

  bool IsMainDirection(GridTrackSizingDirection direction) const {
    return main_direction_ == direction;
  }

  const Vector<MainGap>& GetMainGaps() const { return main_gaps_; }

  const Vector<CrossGap>& GetCrossGaps() const { return cross_gaps_; }

  // Returns the center offset of the gap at the specified `gap_index` in the
  // given `direction` (main or cross axis). For the main axis, it returns the
  // offset directly. For the cross axis, it returns either the inline or block
  // offset depending on the direction (columns or rows).
  LayoutUnit GetGapCenterOffset(GridTrackSizingDirection direction,
                                wtf_size_t gap_index) const;

  // Gap Decorations are painted relative to intersection points within a gap.
  // This method fills `intersections` with the ordered intersection offsets for
  // the gap at `gap_index`. The general pattern is: container content-start ->
  // MainxCross intersections -> container content-end. The middle intersections
  // depend on the container type and direction.
  //
  // We reset `intersections` (Shrink(0)) before populating, so the buffer's
  // capacity is preserved through loop iterations. This makes reuse across a
  // gap loop allocation-free after the first call.
  void GenerateIntersectionListForGap(
      GridTrackSizingDirection direction,
      wtf_size_t gap_index,
      Vector<GapIntersection>& intersections) const;

  // Determines whether the intersection at `intersection_index` within
  // `gap_index` lies on the container boundary. Typically, the first and last
  // intersections are at the container edge, but for flex cross gaps, we must
  // first check if the gap itself is an edge gap before deciding whether the
  // first or last intersection is at the container edge.
  bool IsIntersectionAtContainerEdge(
      wtf_size_t gap_index,
      wtf_size_t intersection_index,
      wtf_size_t intersection_count,
      bool is_main_gap,
      const Vector<GapIntersection>& intersections) const;

  // Returns true if the intersection should be treated as a "cap" for
  // decoration purposes (i.e. an endpoint with no visible crossing decoration
  // to join with). An intersection is a cap if it is either:
  // - At the container boundary (per IsIntersectionAtContainerEdge), or
  // - A dangling interior endpoint with no visible crossing decoration
  //   (https://github.com/w3c/csswg-drafts/issues/13697).
  // Otherwise the intersection is a "junction" (it has a crossing decoration
  // that the main decoration joins with). `gap_index` is the index of the gap
  // itself and `intersection_index` is the index of the intersection point
  // within that gap.
  bool IsCapIntersection(GridTrackSizingDirection cross_direction,
                         wtf_size_t gap_index,
                         wtf_size_t intersection_index,
                         bool is_main_gap,
                         RuleVisibilityItems rule_visibility,
                         RuleVisibilityItems cross_rule_visibility,
                         const Vector<GapIntersection>& intersections) const;

  // Returns the cross-direction decoration width at the given intersection,
  // used by `overlap-join` to determine how far the decoration should extend.
  // Cap intersections have no crossing decoration, so they return 0.
  // `is_cap_intersection` indicates whether the intersection is a cap (at a
  // container edge or a dangling interior endpoint with no visible crossing
  // decoration; https://github.com/w3c/csswg-drafts/issues/13697).
  LayoutUnit GetCrossDecorationWidthForIntersection(
      wtf_size_t gap_index,
      wtf_size_t intersection_index,
      bool is_main_gap,
      const Vector<GapIntersection>& intersections,
      bool is_cap_intersection,
      const Vector<int>& cross_decoration_widths) const;

  // Returns the base width used to resolve percentage inset values at the
  // intersection located at `intersection_index`. Cap intersections return 0.
  // For most junction intersections, this is the cross width at that point
  // (via `GetCrossWidthForIntersection()`). For flex main-direction overlap
  // intersections, this instead returns the overlap window size. Takes
  // `intersections` list because logic here depends on neighboring entries to
  // detect overlaps.
  LayoutUnit GetMaxInsetWidth(
      GridTrackSizingDirection track_direction,
      wtf_size_t gap_index,
      wtf_size_t intersection_index,
      bool is_main_gap,
      const Vector<GapIntersection>& intersections) const;

  // Returns the cross gap width at the intersection located at
  // `intersection_index`. Returns 0 for cap intersections. For junction
  // intersections in grid and multicol, returns the cross gutter width. For
  // flex main-direction intersections, returns the per-line cross gap size.
  // Takes `intersections` list because logic here depends on neighboring
  // entries to identify cap and spanner-adjacent intersections.
  LayoutUnit GetCrossWidthForIntersection(
      GridTrackSizingDirection track_direction,
      wtf_size_t gap_index,
      wtf_size_t intersection_index,
      bool is_main_gap,
      const Vector<GapIntersection>& intersections) const;

  // Returns the `GapSegmentState` for the intersection at `secondary_index`
  // within the gap at `primary_index` in the `track_direction`.
  GapSegmentState GetIntersectionGapSegmentState(
      GridTrackSizingDirection track_direction,
      wtf_size_t primary_index,
      wtf_size_t secondary_index) const;

  const GapSegmentStateRanges* GetGapSegmentStateRangesForGap(
      GridTrackSizingDirection track_direction,
      wtf_size_t gap_index) const;

  // Determines if a given track at `secondary_index` is covered for gap at
  // `primary_index`. For the given `track_direction`, this function looks up
  // any spanners associated with the gap at `primary_index`. If no spanners
  // exist, the track is uncovered. Otherwise, it determines if
  // `secondary_index` falls within any of the gap spanner ranges, indicating
  // that the track is covered by a spanning item.
  bool IsTrackCovered(GridTrackSizingDirection track_direction,
                      wtf_size_t primary_index,
                      wtf_size_t secondary_index) const;

  // Adjusts ranges of cross gaps for this GapGeometry such that they are
  // fragmentation-aware and relative to the current fragment.
  // `last_track_in_previous_fragment` points to the last track that has been
  // fully processed in the previous fragment. `first_track_in_next_fragment`
  // points to the first track that has not been fully processed in the current
  // fragment. If a track starts in the current fragment but continues to
  // subsequent fragments, it is considered "unprocessed".
  void AdjustCrossGapsRangesForFragmentation(
      wtf_size_t last_track_in_previous_fragment,
      wtf_size_t first_track_in_next_fragment,
      Vector<wtf_size_t>& column_gaps_segment_ranges_start_indices);

  // Determines the blocked status of a specific intersection within a grid.
  // `primary_index` represents the gap index along the track direction and
  // `secondary_index` identifies the specific intersection within that gap.
  BlockedStatus GetIntersectionBlockedStatus(
      GridTrackSizingDirection track_direction,
      wtf_size_t primary_index,
      wtf_size_t secondary_index,
      const Vector<GapIntersection>& intersections) const;

  // Derives the blocked status for an intersection from precomputed
  // gap segment states, without querying the ranges.
  static BlockedStatus BlockedStatusFromGapStates(
      const Vector<GapIntersection>& intersections,
      wtf_size_t index);

  blink::String ToString(bool verbose = false) const;

  bool IsMultiColSpanner(wtf_size_t gap_index,
                         GridTrackSizingDirection direction = kForRows) const;

  // `is_cap_intersection` indicates whether the intersection is a cap (at a
  // container edge or a dangling interior endpoint with no visible crossing
  // decoration; https://github.com/w3c/csswg-drafts/issues/13697).
  LayoutUnit ComputeInsetEnd(const ComputedStyle& style,
                             wtf_size_t gap_index,
                             wtf_size_t intersection_index,
                             const Vector<GapIntersection>& intersections,
                             bool is_cap_intersection,
                             bool is_column_gap,
                             bool is_main,
                             bool has_joining_decoration,
                             LayoutUnit cross_gap_width,
                             LayoutUnit cross_decoration_width) const;

  // `is_cap_intersection` indicates whether the intersection is a cap (at a
  // container edge or a dangling interior endpoint with no visible crossing
  // decoration; https://github.com/w3c/csswg-drafts/issues/13697).
  LayoutUnit ComputeInsetStart(const ComputedStyle& style,
                               wtf_size_t gap_index,
                               wtf_size_t intersection_index,
                               const Vector<GapIntersection>& intersections,
                               bool is_cap_intersection,
                               bool is_column_gap,
                               bool is_main,
                               bool has_joining_decoration,
                               LayoutUnit cross_gap_width,
                               LayoutUnit cross_decoration_width) const;

 private:
  // Fills `intersections` for a main gap at `gap_index`. The list includes:
  // - container content start
  // - Intersections with cross gaps (container-specific)
  // - container content end.
  // All offsets are in increasing order along `direction`.
  void GenerateMainIntersectionList(
      GridTrackSizingDirection direction,
      wtf_size_t gap_index,
      Vector<GapIntersection>& intersections) const;

  // Fills `intersections` for a flex main gap at `gap_index`, which includes:
  // 1. Cross gaps that appear before the main gap
  // 2. Cross gaps that appear after the main gap
  void GenerateMainIntersectionListForFlex(
      GridTrackSizingDirection direction,
      wtf_size_t gap_index,
      Vector<GapIntersection>& intersections) const;

  // Fills `intersections` for a cross gap at `gap_index`. For grid containers,
  // this includes the container content edges and every main gap offset. For
  // flex containers, it includes the cross-gap start offset and its computed
  // end offset.
  void GenerateCrossIntersectionList(
      GridTrackSizingDirection direction,
      wtf_size_t gap_index,
      Vector<GapIntersection>& intersections) const;

  // Fills `intersections` for a grid cross gap at `gap_index`, which includes:
  // 1. The content-start edge
  // 2. The start offset of every main gap
  // 3. The content-end edge
  void GenerateCrossIntersectionListForGrid(
      GridTrackSizingDirection direction,
      Vector<GapIntersection>& intersections,
      GapSegmentStateCursor& cursor) const;

  // Fills `intersections` for a flex cross gap at `gap_index`, which includes:
  // 1. The gap's start offset
  // 2. Its computed end offset (either a main gap or the container's
  // content-end edge)
  void GenerateCrossIntersectionListForFlex(
      GridTrackSizingDirection direction,
      wtf_size_t gap_index,
      Vector<GapIntersection>& intersections,
      GapSegmentStateCursor& cursor) const;

  // Fills `intersections` for a multicol cross gap at `gap_index`, which
  // includes:
  // 1. The start block offset of the cross gap.
  // 2. The offset of any main gaps that intersect this cross gap.
  void GenerateCrossIntersectionListForMulticol(
      GridTrackSizingDirection direction,
      wtf_size_t gap_index,
      Vector<GapIntersection>& intersections,
      GapSegmentStateCursor& cursor) const;

  // Computes the end offset for a flex or multicol cross gap at
  // `cross_gap_index`. The end offset is either:
  // - The container's content end which occurs when the cross gap is at last
  // line, or
  // - The offset of the main gap where this cross gap ends (tracked by
  // `main_gap_running_index_`) which occurs when the cross gap occurs on any
  // line but the last.
  LayoutUnit ComputeEndOffsetForFlexCrossGap(wtf_size_t cross_gap_index,
                                             GridTrackSizingDirection direction,
                                             bool cross_gap_is_at_end) const;

  // For `overlap-join`, computes the inset so the main-direction decoration
  // extends to meet the far edge of the cross-direction decoration at each
  // junction intersection. When `has_joining_decoration` is false (e.g. at cap
  // intersections or when the adjacent segment is not visible), the inset is 0.
  // At junction intersections the inset is negative, pulling the endpoint
  // outward to the far edge of the cross decoration width.
  LayoutUnit ComputeOverlapJoinInset(bool has_joining_decoration,
                                     bool is_main,
                                     LayoutUnit cross_gap_width,
                                     LayoutUnit cross_decoration_width) const;

  // In flex it refers to the gap between flex items, and in grid it
  // refers to the column gutter size.
  LayoutUnit inline_gap_size_;
  // In flex it refers to the gap between flex lines, and in grid it
  // refers to the row gutter size.
  LayoutUnit block_gap_size_;

  ContainerType container_type_;

  // TODO(crbug.com/436140061): These properties are being used to implement the
  // optimized version of GapDecorations. Once the optimized version is
  // implemented, we can remove all the other unused properties from the old
  // version.
  // See third_party/blink/renderer/core/layout/gap/README.md for more
  // information.

  MainGaps main_gaps_;
  CrossGaps cross_gaps_;

  // Per-line effective gap sizes (`gap` property + content distribution space)
  // for flex containers. Each flex line corresponds to one entry in this
  // vector, indexed by fragment-relative line index. Every line has a
  // corresponding entry.
  std::optional<Vector<LayoutUnit>> flex_cross_gap_sizes_;

  // These represent the offsets of the content where the gaps begin and end.
  // We use separate LayoutUnits instead of LogicalOffsets, since these are more
  // like "ranges" rather than points since we care about how "long" the content
  // is in each direction, not the exact coordinate of where it starts.
  LayoutUnit content_inline_start_;
  LayoutUnit content_inline_end_;
  LayoutUnit content_block_start_;
  LayoutUnit content_block_end_;

  // TODO(samomekarajr): Consider making this type a display agnostic type that
  // uses inline/block rather than rows/columns.
  GridTrackSizingDirection main_direction_ = kForRows;

  // In flex, cross gaps (except those at the last flex line) terminate
  // at a main gap. Main gaps already track their adjacent cross gaps (before
  // and after). The `main_gap_running_index_` tracks which main gap a sequence
  // of cross gaps belongs to. This allows us to determine the correct end
  // offset for cross gaps in flex.
  //
  // This is made be mutable because GapGeometry is treated as const during
  // Paint, but `ComputeEndOffsetForFlexCrossGap()` (called at paint time)
  // updates this index as part of its calculation. Making this mutable allows
  // us to maintain necessary state without breaking const-correctness for the
  // overall GapGeometry object.
  //
  // TODO(samomekarajr): Explore removing this in favour of having this state
  // live at the parent paint call and passing in as an input/output param.
  mutable wtf_size_t main_gap_running_index_ = kNotFound;

  // For multicol containers, this set tracks which intersection indices are
  // considered to be spanner-adjacent "edges". These intersections are
  // adjacent to spanner main gaps and need to be treated as edge
  // intersections so that insets are applied correctly.
  mutable HashSet<wtf_size_t> multicol_spanner_adjacent_intersections_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_GAP_GEOMETRY_H_
