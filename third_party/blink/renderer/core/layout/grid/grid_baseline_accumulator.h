// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_GRID_BASELINE_ACCUMULATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_GRID_BASELINE_ACCUMULATOR_H_

#include <optional>

#include "base/memory/stack_allocated.h"
#include "third_party/blink/renderer/core/layout/grid/grid_item.h"
#include "third_party/blink/renderer/core/layout/grid/grid_layout_utils.h"
#include "third_party/blink/renderer/core/layout/logical_box_fragment.h"
#include "third_party/blink/renderer/core/style/grid_area.h"
#include "third_party/blink/renderer/platform/fonts/font_baseline.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"

namespace blink {

// Determining the grid's baseline is prioritized based on grid order (as
// opposed to DOM order). The baseline of the grid is determined by the first
// grid item with baseline alignment in the first row. If no items have
// baseline alignment, fall back to the first item in row-major order.
class GridBaselineAccumulator : public BaselineAccumulator {
  STACK_ALLOCATED();

 public:
  explicit GridBaselineAccumulator(FontBaseline font_baseline)
      : font_baseline_(font_baseline) {}

  void Accumulate(const GridItemData& grid_item,
                  const LogicalBoxFragment& fragment,
                  const LayoutUnit block_offset,
                  LayoutUnit item_stacking_position) override {
    Accumulate(grid_item, fragment, block_offset);
  }

  void Accumulate(const GridItemData& grid_item,
                  const LogicalBoxFragment& fragment,
                  const LayoutUnit block_offset) {
    auto StartsBefore = [](const GridArea& a, const GridArea& b) -> bool {
      // Me need to check for `IsTranslatedDefinite` everywhere because this is
      // also used with grid-lanes which only has one grid axis.
      if (a.rows.IsTranslatedDefinite() && b.rows.IsTranslatedDefinite()) {
        if (a.rows.StartLine() < b.rows.StartLine()) {
          return true;
        }
        if (a.rows.StartLine() > b.rows.StartLine()) {
          return false;
        }
      }
      if (a.columns.IsTranslatedDefinite() &&
          b.columns.IsTranslatedDefinite()) {
        return a.columns.StartLine() < b.columns.StartLine();
      }
      return false;
    };
    auto EndsAfter = [](const GridArea& a, const GridArea& b) -> bool {
      if (a.rows.IsTranslatedDefinite() && b.rows.IsTranslatedDefinite()) {
        if (a.rows.EndLine() > b.rows.EndLine()) {
          return true;
        }
        if (a.rows.EndLine() < b.rows.EndLine()) {
          return false;
        }
      }

      if (a.columns.IsTranslatedDefinite() &&
          b.columns.IsTranslatedDefinite()) {
        // Use greater-or-equal to prefer the "last" grid-item.
        return a.columns.EndLine() >= b.columns.EndLine();
      }
      return false;
    };

    if (!first_fallback_baseline_ ||
        StartsBefore(grid_item.resolved_position,
                     first_fallback_baseline_->resolved_position)) {
      first_fallback_baseline_.emplace(
          grid_item.resolved_position,
          block_offset + fragment.FirstBaselineOrSynthesize(font_baseline_));
    }

    if (!last_fallback_baseline_ ||
        EndsAfter(grid_item.resolved_position,
                  last_fallback_baseline_->resolved_position)) {
      last_fallback_baseline_.emplace(
          grid_item.resolved_position,
          block_offset + fragment.LastBaselineOrSynthesize(font_baseline_));
    }

    // Keep track of the first/last set which has content.
    const auto& set_indices = grid_item.SetIndices(kForRows);
    if (first_set_index_ == kNotFound || set_indices.begin < first_set_index_) {
      first_set_index_ = set_indices.begin;
    }
    if (last_set_index_ == kNotFound || set_indices.end - 1 > last_set_index_) {
      last_set_index_ = set_indices.end - 1;
    }
  }

  void AccumulateRows(const GridLayoutTrackCollection& rows) {
    for (wtf_size_t i = 0; i < rows.GetSetCount(); ++i) {
      LayoutUnit set_offset = rows.GetSetOffset(i);
      LayoutUnit major_baseline = rows.MajorBaseline(i);
      if (major_baseline != LayoutUnit::Min()) {
        LayoutUnit baseline_offset = set_offset + major_baseline;
        if (!first_major_baseline_) {
          first_major_baseline_.emplace(i, baseline_offset);
        }
        last_major_baseline_.emplace(i, baseline_offset);
      }

      LayoutUnit minor_baseline = rows.MinorBaseline(i);
      if (minor_baseline != LayoutUnit::Min()) {
        LayoutUnit baseline_offset =
            set_offset + rows.CalculateSetSpanSize(i, i + 1) - minor_baseline;
        if (!first_minor_baseline_) {
          first_minor_baseline_.emplace(i, baseline_offset);
        }
        last_minor_baseline_.emplace(i, baseline_offset);
      }
    }
  }

  std::optional<LayoutUnit> FirstBaseline() const override {
    if (first_major_baseline_ &&
        first_major_baseline_->set_index == first_set_index_) {
      return first_major_baseline_->baseline;
    }
    if (first_minor_baseline_ &&
        first_minor_baseline_->set_index == first_set_index_) {
      return first_minor_baseline_->baseline;
    }
    if (first_fallback_baseline_) {
      return first_fallback_baseline_->baseline;
    }
    return std::nullopt;
  }

  std::optional<LayoutUnit> LastBaseline() const override {
    if (last_minor_baseline_ &&
        last_minor_baseline_->set_index == last_set_index_) {
      return last_minor_baseline_->baseline;
    }
    if (last_major_baseline_ &&
        last_major_baseline_->set_index == last_set_index_) {
      return last_major_baseline_->baseline;
    }
    if (last_fallback_baseline_) {
      return last_fallback_baseline_->baseline;
    }
    return std::nullopt;
  }

 private:
  struct SetIndexAndBaseline {
    SetIndexAndBaseline(wtf_size_t set_index, LayoutUnit baseline)
        : set_index(set_index), baseline(baseline) {}
    wtf_size_t set_index;
    LayoutUnit baseline;
  };
  struct PositionAndBaseline {
    PositionAndBaseline(const GridArea& resolved_position, LayoutUnit baseline)
        : resolved_position(resolved_position), baseline(baseline) {}
    GridArea resolved_position;
    LayoutUnit baseline;
  };

  FontBaseline font_baseline_;
  wtf_size_t first_set_index_ = kNotFound;
  wtf_size_t last_set_index_ = kNotFound;

  std::optional<SetIndexAndBaseline> first_major_baseline_;
  std::optional<SetIndexAndBaseline> first_minor_baseline_;
  std::optional<PositionAndBaseline> first_fallback_baseline_;

  std::optional<SetIndexAndBaseline> last_major_baseline_;
  std::optional<SetIndexAndBaseline> last_minor_baseline_;
  std::optional<PositionAndBaseline> last_fallback_baseline_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_GRID_BASELINE_ACCUMULATOR_H_
