// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_GAP_INTERSECTION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_GAP_INTERSECTION_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"

namespace blink {

// Represents the overlap state of an intersection point within a gap in an
// unaligned layout mode. For example, in a flex main gap when cross gaps from
// adjacent flex lines are merged, some intersection points mark the boundaries
// of "overlap windows" i.e. regions where a cross gap from one side of the
// main gap overlaps with another cross gap on the other side.
//
// "kWindowOpen" marks where an overlap window begins and "kWindowClose" marks
// where it ends along the main axis.
enum class OverlapWindowState {
  kNone,         // Not part of an overlap window.
  kWindowOpen,   // Start of an overlap window.
  kWindowClose,  // End of an overlap window.
};

// Extra state carried by a `GapIntersection` in unaligned layout modes (e.g.
// flexbox). In these modes, items across adjacent lines are not always aligned,
// so cross gaps from different sides of a main gap may have different sizes and
// overlap non-uniformly. These overlapping regions are called "overlap
// windows". The `OverlapWindowState` marks the open and close boundaries of
// these windows and is used to generate the right intersection start and end
// points for painting.
//
// The `is_above_main_gap` flag tracks which side of the main gap (above or
// below) the intersection originated from. This flag is used even for
// non-overlapping intersections so that per-line gap sizes can be resolved at
// paint time.
struct ExtraIntersectionState {
  // Tracks which side of the main gap this intersection originated from.
  bool is_above_main_gap = false;

  // Marks the open/close boundaries of overlap windows. `kNone` when the
  // intersection is not part of an overlap window.
  OverlapWindowState overlap_state = OverlapWindowState::kNone;
};

// Stores the offset of a gap intersection, along with optional
// `ExtraIntersectionState` for unaligned layout modes. For grid and multicol
// containers, gaps are aligned and all cross gaps have the same size, so
// `extra_state_` is absent.
class CORE_EXPORT GapIntersection {
 public:
  GapIntersection() = default;

  explicit GapIntersection(LayoutUnit offset) : offset_(offset) {}

  GapIntersection(LayoutUnit offset,
                  OverlapWindowState state,
                  bool is_above_main_gap)
      : offset_(offset),
        extra_state_(ExtraIntersectionState{is_above_main_gap, state}) {}

  GapIntersection(LayoutUnit offset, bool is_above_main_gap)
      : offset_(offset),
        extra_state_(ExtraIntersectionState{is_above_main_gap}) {}

  LayoutUnit GetOffset() const { return offset_; }

  bool HasOverlapState() const {
    return extra_state_.has_value() &&
           extra_state_->overlap_state != OverlapWindowState::kNone;
  }

  OverlapWindowState GetOverlapState() const {
    CHECK(HasOverlapState());
    return extra_state_->overlap_state;
  }

  bool IsOverlapWindowOpen() const {
    return HasOverlapState() &&
           extra_state_->overlap_state == OverlapWindowState::kWindowOpen;
  }

  bool IsOverlapWindowClose() const {
    return HasOverlapState() &&
           extra_state_->overlap_state == OverlapWindowState::kWindowClose;
  }

  bool IsAboveMainGap() const {
    CHECK(extra_state_.has_value());
    return extra_state_->is_above_main_gap;
  }

  void SetOffset(LayoutUnit offset) { offset_ = offset; }

  void SetOverlapState(OverlapWindowState state) {
    CHECK(extra_state_.has_value());
    extra_state_->overlap_state = state;
  }

  void SetIsAboveMainGap(bool is_above) {
    CHECK(extra_state_.has_value());
    extra_state_->is_above_main_gap = is_above;
  }

  void ResetOverlapState() {
    CHECK(extra_state_.has_value());
    extra_state_->overlap_state = OverlapWindowState::kNone;
  }

 private:
  LayoutUnit offset_;

  // Present only for unaligned layout modes (e.g. flex). Tracks which side of
  // the main gap this intersection originated from and optional overlap window
  // state. Absent for aligned modes such as grid and multicol.
  std::optional<ExtraIntersectionState> extra_state_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_GAP_INTERSECTION_H_
