// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_GAP_INTERSECTION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_GAP_INTERSECTION_H_

#include <optional>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"

namespace blink {

// Represents the overlap state of an intersection point within a gap in an
// unaligned layout mode. For example, in a flex main gap when cross gaps from
// adjacent flex lines are merged, some intersection points mark the boundaries
// of "overlap windows" i.e. regions where a cross gap from one side of the
// main gap overlaps with another cross gap on the other side.
//
// "Open" and "Close" indicate where an overlap window begins and ends along
// the main axis. "Above" and "Below" refer to which side of the main gap has
// the item causing the overlap.
enum class OverlapWindowState {
  kWindowOpenAbove,   // Start of overlap window, item is above the main gap.
  kWindowOpenBelow,   // Start of overlap window, item is below the main gap.
  kWindowCloseAbove,  // End of overlap window, item is above the main gap.
  kWindowCloseBelow,  // End of overlap window, item is below the main gap.
};

// Stores the offset of a gap intersection, along with an optional
// `OverlapWindowState`. In layout modes like flexbox, where items across
// adjacent lines are not always aligned, cross gaps from different sides of a
// main gap may overlap non-uniformly. These overlapping regions are called
// "overlap windows". The `OverlapWindowState` marks the open and close
// boundaries of these windows and is used to generate the right intersection
// start and end points for painting. For grid and multicol containers, items
// are aligned and gaps overlap uniformly, so the overlap state is always absent
// (std::nullopt).
class CORE_EXPORT GapIntersection {
 public:
  GapIntersection() = default;

  explicit GapIntersection(LayoutUnit offset) : offset_(offset) {}

  GapIntersection(LayoutUnit offset, OverlapWindowState state)
      : offset_(offset), overlap_state_(state) {}

  LayoutUnit GetOffset() const { return offset_; }

  bool HasOverlapState() const { return overlap_state_.has_value(); }

  OverlapWindowState GetOverlapState() const {
    CHECK(overlap_state_.has_value());
    return *overlap_state_;
  }

  bool IsOverlapWindowOpen() const {
    return overlap_state_.has_value() &&
           (*overlap_state_ == OverlapWindowState::kWindowOpenBelow ||
            *overlap_state_ == OverlapWindowState::kWindowOpenAbove);
  }

  bool IsOverlapWindowClose() const {
    return overlap_state_.has_value() &&
           (*overlap_state_ == OverlapWindowState::kWindowCloseBelow ||
            *overlap_state_ == OverlapWindowState::kWindowCloseAbove);
  }

  bool IsOverlapWindowAbove() const {
    return overlap_state_.has_value() &&
           (*overlap_state_ == OverlapWindowState::kWindowOpenAbove ||
            *overlap_state_ == OverlapWindowState::kWindowCloseAbove);
  }

  bool IsOverlapWindowBelow() const {
    return overlap_state_.has_value() &&
           (*overlap_state_ == OverlapWindowState::kWindowOpenBelow ||
            *overlap_state_ == OverlapWindowState::kWindowCloseBelow);
  }

  void SetOffset(LayoutUnit offset) { offset_ = offset; }

  void SetOverlapState(OverlapWindowState state) { overlap_state_ = state; }

  void ResetOverlapState() { overlap_state_.reset(); }

 private:
  LayoutUnit offset_;
  // Absent (std::nullopt) in layout modes where gaps align uniformly, such as
  // grid and multicol. Set only in modes like flexbox where overlap windows
  // need to be tracked.
  std::optional<OverlapWindowState> overlap_state_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_GAP_INTERSECTION_H_
