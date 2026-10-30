// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_FLEX_GAP_GEOMETRY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_FLEX_GAP_GEOMETRY_H_

#include <optional>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/gap/gap_geometry.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class CORE_EXPORT FlexGapGeometry final : public GapGeometry {
 public:
  FlexGapGeometry() : GapGeometry(ContainerType::kFlex) {}

  LayoutUnit GetFlexCrossGapSize(wtf_size_t line_index) const {
    CHECK(flex_cross_gap_sizes_.has_value());
    CHECK_GT(flex_cross_gap_sizes_->size(), line_index);
    return (*flex_cross_gap_sizes_)[line_index];
  }

  // Per-line main axis gap sizes for flex containers. This is needed because
  // different lines in a flex container can have different effective gap sizes
  // due to content distribution space.
  void ResizeFlexCrossGapSizes(wtf_size_t count) {
    if (!flex_cross_gap_sizes_.has_value()) {
      flex_cross_gap_sizes_.emplace();
    }
    flex_cross_gap_sizes_->resize(count);
  }

  void SetFlexCrossGapSize(wtf_size_t index, LayoutUnit size) {
    CHECK(flex_cross_gap_sizes_.has_value());
    CHECK_LT(index, flex_cross_gap_sizes_->size());
    (*flex_cross_gap_sizes_)[index] = size;
  }

  void AddFragmentedFlexCrossGapDecorationIndex(
      wtf_size_t decoration_value_index,
      wtf_size_t gap_slot_count) {
    CHECK_LT(decoration_value_index, gap_slot_count);
    if (fragmented_flex_cross_gap_decoration_indices_.empty()) {
      fragmented_flex_cross_gap_count_ = gap_slot_count;
    }
    CHECK_EQ(fragmented_flex_cross_gap_count_, gap_slot_count);
    fragmented_flex_cross_gap_decoration_indices_.push_back(
        decoration_value_index);
    CHECK_EQ(fragmented_flex_cross_gap_decoration_indices_.size(),
             CrossGapCount());
  }

 private:
  // TODO(javiercon): Remove this friend declaration when the methods that
  // `GapGeometry` calls can be made virtual.
  friend class GapGeometry;

  bool HasEqualSubclassFields(const FlexGapGeometry& other) const {
    return flex_cross_gap_sizes_ == other.flex_cross_gap_sizes_ &&
           fragmented_flex_cross_gap_decoration_indices_ ==
               other.fragmented_flex_cross_gap_decoration_indices_ &&
           fragmented_flex_cross_gap_count_ ==
               other.fragmented_flex_cross_gap_count_;
  }

  bool HasFragmentedFlexCrossGapDecorationIndices() const {
    return !fragmented_flex_cross_gap_decoration_indices_.empty();
  }

  wtf_size_t FragmentedFlexCrossGapCount() const {
    CHECK(!fragmented_flex_cross_gap_decoration_indices_.empty());
    return fragmented_flex_cross_gap_count_;
  }

  // Returns the largest per-line gap size, or `std::nullopt` if no per-line
  // sizes are stored.
  std::optional<LayoutUnit> MaxFlexCrossGapSize() const;

  // Returns the saved value index for a fragmented flex `CrossGap`.
  wtf_size_t FragmentedFlexCrossGapDecorationValueIndexAt(
      wtf_size_t cross_gap_index) const {
    CHECK_LT(cross_gap_index,
             fragmented_flex_cross_gap_decoration_indices_.size());
    return fragmented_flex_cross_gap_decoration_indices_[cross_gap_index];
  }

  // Fills `intersections` for a flex main gap at `gap_index`, including the
  // content edges and:
  // 1. Cross gaps that appear before the main gap
  // 2. Cross gaps that appear after the main gap
  void GenerateMainIntersectionListForFlex(
      GridTrackSizingDirection direction,
      wtf_size_t gap_index,
      Vector<GapIntersection>& intersections,
      GapSegmentStateCursor& cursor) const;

  // Fills `intersections` for a flex cross gap at `gap_index`, which includes:
  // 1. The gap's start offset
  // 2. Its computed end offset (either a main gap or the container's
  // content-end edge)
  // `main_gap_index` is the index of the main gap where this cross gap ends.
  void GenerateCrossIntersectionListForFlex(
      GridTrackSizingDirection direction,
      wtf_size_t gap_index,
      Vector<GapIntersection>& intersections,
      GapSegmentStateCursor& cursor,
      wtf_size_t main_gap_index) const;

  // Computes the end offset for a flex cross gap at `cross_gap_index`. The end
  // offset is either:
  // - The container's content end which occurs when the cross gap is at last
  // line, or
  // - The offset of the main gap where this cross gap ends (tracked by
  // `main_gap_index`) which occurs when the cross gap occurs on any
  // line but the last.
  LayoutUnit ComputeEndOffsetForFlexCrossGap(GridTrackSizingDirection direction,
                                             bool cross_gap_is_at_end,
                                             wtf_size_t main_gap_index) const;

  // Per-line effective gap sizes (`gap` property + content distribution space)
  // for flex containers. Column flex uses absolute flex-line indices, while row
  // flex uses fragment-relative line indices.
  std::optional<Vector<LayoutUnit>> flex_cross_gap_sizes_;

  // Index used to select the color, style, and width for each `CrossGap` in
  // this fragmented flex geometry.
  Vector<wtf_size_t> fragmented_flex_cross_gap_decoration_indices_;
  wtf_size_t fragmented_flex_cross_gap_count_ = 0;
};

template <>
struct DowncastTraits<FlexGapGeometry> {
  static bool AllowFrom(const GapGeometry& gap_geometry) {
    return gap_geometry.GetContainerType() == GapGeometry::ContainerType::kFlex;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_FLEX_GAP_GEOMETRY_H_
