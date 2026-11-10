// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_FLEX_GAP_GEOMETRY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_FLEX_GAP_GEOMETRY_H_

#include <memory>
#include <optional>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/gap/gap_geometry.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class CORE_EXPORT FlexGapGeometry final : public GapGeometry {
 public:
  FlexGapGeometry() : GapGeometry(ContainerType::kFlex) {}

  LayoutUnit GetCrossGapSizeForLine(wtf_size_t line_index) const {
    CHECK_GT(cross_gap_sizes_.size(), line_index);
    return cross_gap_sizes_[line_index];
  }

  // Resizes the per-line cross gap sizes to `count` lines. Flex needs per-line
  // sizes because content distribution can give each line a different
  // effective gap size.
  void ResizeCrossGapSizes(wtf_size_t count) { cross_gap_sizes_.resize(count); }

  void SetCrossGapSizeForLine(wtf_size_t line_index, LayoutUnit size) {
    CHECK_LT(line_index, cross_gap_sizes_.size());
    cross_gap_sizes_[line_index] = size;
  }

  void AddFragmentedCrossGapDecorationIndex(wtf_size_t decoration_value_index,
                                            wtf_size_t gap_slot_count) {
    CHECK_LT(decoration_value_index, gap_slot_count);
    if (!stored_cross_gap_decoration_values_) {
      stored_cross_gap_decoration_values_ =
          std::make_unique<StoredCrossGapDecorationValues>();
      stored_cross_gap_decoration_values_->gap_slot_count = gap_slot_count;
    }
    CHECK_EQ(stored_cross_gap_decoration_values_->gap_slot_count,
             gap_slot_count);
    stored_cross_gap_decoration_values_->value_indices.push_back(
        decoration_value_index);
    CHECK_EQ(stored_cross_gap_decoration_values_->value_indices.size(),
             CrossGapCount());
  }

 private:
  // Compares the per-line cross gap sizes and the stored decoration value
  // indices of fragmented cross gaps.
  bool HasEqualSubclassFields(const GapGeometry& other) const override;

  // Each cross gap belongs to the flex line that contains it.
  bool CrossGapsHaveOwners() const override { return true; }

  // Layout stores the assignment only when this flex container is fragmented
  // and reverses gap placement (`wrap-reverse` or a reversed `flex-direction`).
  const StoredCrossGapDecorationValues* GetStoredCrossGapDecorationValues()
      const override {
    return stored_cross_gap_decoration_values_.get();
  }

  // Returns the largest per-line cross gap size, or `std::nullopt` if this
  // container has no flex lines with stored sizes.
  std::optional<LayoutUnit> MaxCrossGapSize() const override;

  // Flex lines can have different effective gap sizes because of content
  // distribution. Returns the cross gap size of the flex line on the side of
  // `intersection`: the line before the main gap at `main_gap_index` if
  // `intersection` is above that main gap, otherwise the line after it.
  std::optional<LayoutUnit> CrossGapSizeAtMainGapIntersection(
      wtf_size_t main_gap_index,
      const GapIntersection& intersection) const override {
    return GetCrossGapSizeForLine(
        intersection.IsAboveMainGap() ? main_gap_index : main_gap_index + 1);
  }

  // Fills `intersections` for a flex main gap at `gap_index`, including the
  // content edges and:
  // 1. Cross gaps that appear before the main gap
  // 2. Cross gaps that appear after the main gap
  void GenerateMainIntersectionList(
      GridTrackSizingDirection direction,
      wtf_size_t gap_index,
      RuleVisibilityItems rule_visibility,
      GapSegmentStateCursor& cursor,
      Vector<GapIntersection>& intersections) const override;

  // Fills `intersections` for a flex cross gap at `gap_index`, which includes:
  // 1. The gap's start offset
  // 2. Its computed end offset (either a main gap or the container's
  // content-end edge)
  // `cross_gap_owner_index` is the index of the flex line that owns this cross
  // gap. It is also the index of the main gap where this cross gap ends, or
  // `GetMainGaps().size()` when the gap ends at the container content edge.
  void GenerateCrossIntersectionList(
      GridTrackSizingDirection direction,
      wtf_size_t gap_index,
      std::optional<wtf_size_t> cross_gap_owner_index,
      GapSegmentStateCursor& cursor,
      Vector<GapIntersection>& intersections) const override;

  // Computes the end offset for a flex cross gap at `cross_gap_index`. The end
  // offset is either:
  // - The container's content end which occurs when the cross gap is at last
  // line, or
  // - The offset of the main gap where this cross gap ends (tracked by
  // `main_gap_index`) which occurs when the cross gap occurs on any
  // line but the last.
  LayoutUnit ComputeEndOffsetForCrossGap(GridTrackSizingDirection direction,
                                         bool cross_gap_is_at_end,
                                         wtf_size_t main_gap_index) const;

  // Per-line effective gap sizes (`gap` property + content distribution space)
  // for flex containers. Column flex uses absolute flex-line indices, while row
  // flex uses fragment-relative line indices.
  Vector<LayoutUnit> cross_gap_sizes_;

  // Selects the color, style, and width for each `CrossGap` in this fragmented
  // flex geometry. See `AddFragmentedCrossGapDecorationIndex()`.
  std::unique_ptr<StoredCrossGapDecorationValues>
      stored_cross_gap_decoration_values_;
};

template <>
struct DowncastTraits<FlexGapGeometry> {
  static bool AllowFrom(const GapGeometry& gap_geometry) {
    return gap_geometry.GetContainerType() == GapGeometry::ContainerType::kFlex;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_FLEX_GAP_GEOMETRY_H_
