/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_GRID_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_GRID_H_

#include <memory>
#include "third_party/blink/renderer/core/layout/grid.h"
#include "third_party/blink/renderer/core/layout/grid_layout_utils.h"
#include "third_party/blink/renderer/core/layout/grid_track_sizing_algorithm.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/order_iterator.h"
#include "third_party/blink/renderer/core/style/grid_positions_resolver.h"

namespace blink {

struct GridArea;
struct GridSpan;

struct ContentAlignmentData {
 public:
  ContentAlignmentData() = default;
  ContentAlignmentData(const ContentAlignmentData&) = delete;
  ContentAlignmentData& operator=(const ContentAlignmentData&) = delete;
  bool IsValid() { return position_offset >= 0 && distribution_offset >= 0; }

  LayoutUnit position_offset = LayoutUnit(-1);
  LayoutUnit distribution_offset = LayoutUnit(-1);
};

enum GridAxisPosition { kGridAxisStart, kGridAxisEnd, kGridAxisCenter };

class LayoutGrid final : public LayoutBlock {
 public:
  explicit LayoutGrid(Element*);
  ~LayoutGrid() override;

  static LayoutGrid* CreateAnonymous(Document*);
  const char* GetName() const override { return "LayoutGrid"; }

  void UpdateBlockLayout(bool relayout_children) override;

  void DirtyGrid();

  Vector<LayoutUnit> TrackSizesForComputedStyle(GridTrackSizingDirection) const;

  const Vector<LayoutUnit>& ColumnPositions() const {
    DCHECK(!grid_->NeedsItemsPlacement());
    return column_positions_;
  }

  const Vector<LayoutUnit>& RowPositions() const {
    DCHECK(!grid_->NeedsItemsPlacement());
    return row_positions_;
  }

  // TODO(svillar): rename this method as this does not return a
  // GridCell but its contents.
  const GridItemList& GetGridCell(int row, int column) const {
    SECURITY_DCHECK(!grid_->NeedsItemsPlacement());
    return grid_->Cell(row, column);
  }

  size_t AutoRepeatCountForDirection(GridTrackSizingDirection direction) const {
    return grid_->AutoRepeatTracks(direction);
  }

  size_t ExplicitGridStartForDirection(
      GridTrackSizingDirection direction) const {
    return grid_->ExplicitGridStart(direction);
  }

  LayoutUnit TranslateRTLCoordinate(LayoutUnit) const;

  LayoutUnit TranslateOutOfFlowRTLCoordinate(const LayoutBox&,
                                             LayoutUnit) const;

  // TODO(svillar): We need these for the GridTrackSizingAlgorithm. Let's figure
  // it out how to remove this dependency.
  LayoutUnit GuttersSize(const Grid&,
                         GridTrackSizingDirection,
                         size_t start_line,
                         size_t span,
                         base::Optional<LayoutUnit> available_size) const;
  bool CachedHasDefiniteLogicalHeight() const;
  bool IsBaselineAlignmentForChild(const LayoutBox& child) const;
  bool IsBaselineAlignmentForChild(const LayoutBox& child, GridAxis) const;

  StyleSelfAlignmentData SelfAlignmentForChild(
      GridAxis,
      const LayoutBox& child,
      const ComputedStyle* = nullptr) const;

  LayoutUnit GridGap(GridTrackSizingDirection) const;
  LayoutUnit GridItemOffset(GridTrackSizingDirection) const;

  void UpdateGridAreaLogicalSize(LayoutBox&, LayoutSize) const;

  StyleContentAlignmentData ContentAlignment(GridTrackSizingDirection) const;

  size_t ExplicitGridEndForDirection(GridTrackSizingDirection) const;

  // Exposed for testing *ONLY*.
  Grid* InternalGrid() const { return grid_.get(); }

 protected:
  ItemPosition SelfAlignmentNormalBehavior(
      const LayoutBox* child = nullptr) const override {
    DCHECK(child);
    return child->IsLayoutReplaced() ? ItemPosition::kStart
                                     : ItemPosition::kStretch;
  }

 private:
  bool IsOfType(LayoutObjectType type) const override {
    return type == kLayoutObjectLayoutGrid || LayoutBlock::IsOfType(type);
  }
  MinMaxSizes ComputeIntrinsicLogicalWidths() const override;

  void AddChild(LayoutObject* new_child,
                LayoutObject* before_child = nullptr) override;
  void RemoveChild(LayoutObject*) override;

  bool SelfAlignmentChangedSize(GridAxis,
                                const ComputedStyle& old_style,
                                const ComputedStyle& new_style,
                                const LayoutBox&) const;
  bool DefaultAlignmentChangedSize(GridAxis,
                                   const ComputedStyle& old_style,
                                   const ComputedStyle& new_style) const;
  void StyleDidChange(StyleDifference, const ComputedStyle*) override;

  bool ExplicitGridDidResize(const ComputedStyle&) const;
  bool NamedGridLinesDefinitionDidChange(const ComputedStyle&) const;

  size_t ComputeAutoRepeatTracksCount(
      GridTrackSizingDirection,
      base::Optional<LayoutUnit> available_size) const;
  size_t ClampAutoRepeatTracks(GridTrackSizingDirection,
                               size_t auto_repeat_tracks) const;

  std::unique_ptr<OrderedTrackIndexSet> ComputeEmptyTracksForAutoRepeat(
      Grid&,
      GridTrackSizingDirection) const;

  void PerformGridItemsPreLayout(const GridTrackSizingAlgorithm&) const;

  void PlaceItemsOnGrid(
      GridTrackSizingAlgorithm&,
      base::Optional<LayoutUnit> available_logical_width) const;
  void PopulateExplicitGridAndOrderIterator(Grid&) const;
  std::unique_ptr<GridArea> CreateEmptyGridAreaAtSpecifiedPositionsOutsideGrid(
      const Grid&,
      const LayoutBox&,
      GridTrackSizingDirection,
      const GridSpan& specified_positions) const;
  void PlaceSpecifiedMajorAxisItemsOnGrid(Grid&,
                                          const Vector<LayoutBox*>&) const;
  void PlaceAutoMajorAxisItemsOnGrid(Grid&, const Vector<LayoutBox*>&) const;
  void PlaceAutoMajorAxisItemOnGrid(
      Grid&,
      LayoutBox&,
      std::pair<size_t, size_t>& auto_placement_cursor) const;
  GridTrackSizingDirection AutoPlacementMajorAxisDirection() const;
  GridTrackSizingDirection AutoPlacementMinorAxisDirection() const;

  base::Optional<LayoutUnit> OverrideIntrinsicContentLogicalSize(
      GridTrackSizingDirection) const;

  void ComputeTrackSizesForIndefiniteSize(GridTrackSizingAlgorithm&,
                                          GridTrackSizingDirection) const;
  void ComputeTrackSizesForDefiniteSize(GridTrackSizingDirection,
                                        LayoutUnit free_space);

  void RepeatTracksSizingIfNeeded(LayoutUnit available_space_for_columns,
                                  LayoutUnit available_space_for_rows);

  void LayoutGridItems();
  void PrepareChildForPositionedLayout(LayoutBox&);
  bool HasStaticPositionForChild(const LayoutBox&,
                                 GridTrackSizingDirection) const;
  void LayoutPositionedObjects(
      bool relayout_children,
      PositionedLayoutBehavior = kDefaultLayout) override;
  void PopulateGridPositionsForDirection(GridTrackSizingDirection);

  LayoutUnit ResolveAutoStartGridPosition(GridTrackSizingDirection) const;
  LayoutUnit ResolveAutoEndGridPosition(GridTrackSizingDirection) const;
  LayoutUnit LogicalOffsetForOutOfFlowChild(const LayoutBox&,
                                            GridTrackSizingDirection,
                                            LayoutUnit) const;
  LayoutUnit GridAreaBreadthForOutOfFlowChild(const LayoutBox&,
                                              GridTrackSizingDirection);
  void GridAreaPositionForOutOfFlowChild(const LayoutBox&,
                                         GridTrackSizingDirection,
                                         LayoutUnit& start,
                                         LayoutUnit& end) const;
  void GridAreaPositionForInFlowChild(const LayoutBox&,
                                      GridTrackSizingDirection,
                                      LayoutUnit& start,
                                      LayoutUnit& end) const;
  void GridAreaPositionForChild(const LayoutBox&,
                                GridTrackSizingDirection,
                                LayoutUnit& start,
                                LayoutUnit& end) const;

  GridAxisPosition ColumnAxisPositionForChild(const LayoutBox&) const;
  GridAxisPosition RowAxisPositionForChild(const LayoutBox&) const;
  LayoutUnit RowAxisOffsetForChild(const LayoutBox&) const;
  LayoutUnit ColumnAxisOffsetForChild(const LayoutBox&) const;
  void ComputeContentPositionAndDistributionOffset(
      GridTrackSizingDirection,
      const LayoutUnit& available_free_space,
      unsigned number_of_grid_tracks);
  LayoutPoint GridAreaLogicalPosition(const GridArea&) const;
  void SetLogicalPositionForChild(LayoutBox&) const;
  void SetLogicalOffsetForChild(LayoutBox&, GridTrackSizingDirection) const;
  LayoutUnit LogicalOffsetForChild(const LayoutBox&,
                                   GridTrackSizingDirection) const;

  LayoutUnit GridAreaBreadthForChildIncludingAlignmentOffsets(
      const LayoutBox&,
      GridTrackSizingDirection) const;

  void PaintChildren(const PaintInfo&,
                     const PhysicalOffset& paint_offset) const override;

  LayoutUnit AvailableAlignmentSpaceForChildBeforeStretching(
      LayoutUnit grid_area_breadth_for_child,
      const LayoutBox&) const;
  StyleSelfAlignmentData JustifySelfForChild(
      const LayoutBox&,
      const ComputedStyle* = nullptr) const;
  StyleSelfAlignmentData AlignSelfForChild(
      const LayoutBox&,
      const ComputedStyle* = nullptr) const;
  StyleSelfAlignmentData DefaultAlignment(GridAxis, const ComputedStyle&) const;
  bool DefaultAlignmentIsStretchOrNormal(GridAxis, const ComputedStyle&) const;
  void ApplyStretchAlignmentToChildIfNeeded(LayoutBox&);
  bool HasAutoSizeInColumnAxis(const LayoutBox& child) const {
    return IsHorizontalWritingMode() ? child.StyleRef().Height().IsAuto()
                                     : child.StyleRef().Width().IsAuto();
  }
  bool HasAutoSizeInRowAxis(const LayoutBox& child) const {
    return IsHorizontalWritingMode() ? child.StyleRef().Width().IsAuto()
                                     : child.StyleRef().Height().IsAuto();
  }
  bool AllowedToStretchChildAlongColumnAxis(const LayoutBox& child) const {
    return AlignSelfForChild(child).GetPosition() == ItemPosition::kStretch &&
           HasAutoSizeInColumnAxis(child) && !HasAutoMarginsInColumnAxis(child);
  }
  bool AllowedToStretchChildAlongRowAxis(const LayoutBox& child) const {
    return JustifySelfForChild(child).GetPosition() == ItemPosition::kStretch &&
           HasAutoSizeInRowAxis(child) && !HasAutoMarginsInRowAxis(child);
  }
  bool HasAutoMarginsInColumnAxis(const LayoutBox&) const;
  bool HasAutoMarginsInRowAxis(const LayoutBox&) const;
  void UpdateAutoMarginsInColumnAxisIfNeeded(LayoutBox&);
  void UpdateAutoMarginsInRowAxisIfNeeded(LayoutBox&);

  LayoutUnit BaselinePosition(
      FontBaseline,
      bool first_line,
      LineDirectionMode,
      LinePositionMode = kPositionOnContainingLine) const override;
  LayoutUnit FirstLineBoxBaseline() const override;
  LayoutUnit InlineBlockBaseline(LineDirectionMode) const override;

  LayoutUnit ColumnAxisBaselineOffsetForChild(const LayoutBox&) const;
  LayoutUnit RowAxisBaselineOffsetForChild(const LayoutBox&) const;

  LayoutUnit GridGap(GridTrackSizingDirection,
                     base::Optional<LayoutUnit> available_size) const;

  size_t GridItemSpan(const LayoutBox&, GridTrackSizingDirection);

  size_t NonCollapsedTracks(GridTrackSizingDirection) const;
  size_t NumTracks(GridTrackSizingDirection, const Grid&) const;

  static LayoutUnit OverrideContainingBlockContentSizeForChild(
      const LayoutBox& child,
      GridTrackSizingDirection);
  static LayoutUnit SynthesizedBaselineFromBorderBox(const LayoutBox&,
                                                     LineDirectionMode);
  static const StyleContentAlignmentData& ContentAlignmentNormalBehavior();

  std::unique_ptr<Grid> grid_;
  GridTrackSizingAlgorithm track_sizing_algorithm_;

  Vector<LayoutUnit> row_positions_;
  Vector<LayoutUnit> column_positions_;
  ContentAlignmentData offset_between_columns_;
  ContentAlignmentData offset_between_rows_;

  typedef HashMap<const LayoutBox*, base::Optional<size_t>>
      OutOfFlowPositionsMap;
  OutOfFlowPositionsMap column_of_positioned_item_;
  OutOfFlowPositionsMap row_of_positioned_item_;

  bool has_any_orthogonal_item_{false};
  bool baseline_items_cached_{false};
  base::Optional<bool> has_definite_logical_height_;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutGrid, IsLayoutGrid());

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_GRID_H_
