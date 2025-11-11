// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_CROSS_GAP_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_CROSS_GAP_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/gap/gap_utils.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_offset.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// This is used to hold the range [start, end] of which cross gaps come "before"
// and "after" the `MainGap` associated with this `CrossGap`. Where `start` and
// `end` describe the index range within the `cross_gaps_` vector of the
// `GapGeometry` where a given `CrossGap` is stored.
class CORE_EXPORT CrossGapRange {
 public:
  CrossGapRange(wtf_size_t start, wtf_size_t end)
      : start_index_(start), end_index_(end) {}

  CrossGapRange() = default;

  bool IsValid() const {
    return start_index_.has_value() && end_index_.has_value();
  }

  wtf_size_t Start() const {
    CHECK(start_index_.has_value());
    return *start_index_;
  }

  wtf_size_t End() const {
    CHECK(end_index_.has_value());
    return *end_index_;
  }

  // Increments the range. `cross_gap_index` is the index of the cross gap being
  // processed.
  void Increment(wtf_size_t cross_gap_index);

  String ToString() const;

  bool operator==(const CrossGapRange& other) const {
    return start_index_ == other.start_index_ && end_index_ == other.end_index_;
  }

 private:
  std::optional<wtf_size_t> start_index_;
  std::optional<wtf_size_t> end_index_;
};

// Represents any gap that intersects a `MainGap`. For example, in a row-based
// flex container, the `MainGap` would represent the gaps between flex lines,
// while the `CrossGaps` would represent the gaps between flex items in the same
// line. In Grid, we use the row gaps as our `MainGap`s and column gaps as
// `CrossGap`s. See third_party/blink/renderer/core/layout/gap/README.md for
// more information.
class CORE_EXPORT CrossGap {
 public:
  // Represents whether/how the gap borders the edge of the container. This
  // state is used by the paint code in order to paint correctly with the outset
  // property, as this property can result in different behavior at the edges.
  // This is also useful for the paint code to know whether to paint to the
  // middle of a gap or to the end of the content.
  enum EdgeIntersectionState : uint8_t {
    kNone = 0,
    kStart = 1,
    kEnd = 2,
    kBoth = 3,
  };

  CrossGap(LogicalOffset offset) : gap_logical_offset_(offset) {}
  CrossGap(LogicalOffset offset, EdgeIntersectionState state)
      : gap_logical_offset_(offset), edge_state_(state) {}

  LogicalOffset GetGapOffset() const { return gap_logical_offset_; }

  String ToString(bool verbose = false) const;

  void SetEdgeIntersectionState(EdgeIntersectionState state) {
    edge_state_ = state;
  }
  EdgeIntersectionState GetEdgeIntersectionState() const { return edge_state_; }

  bool StartsAtEdge() const {
    return edge_state_ == kStart || edge_state_ == kBoth;
  }
  bool EndsAtEdge() const {
    return edge_state_ == kEnd || edge_state_ == kBoth;
  }
  bool GapIntersectsContainerEdge() const { return edge_state_ != kNone; }

  bool HasGapSegmentStateRanges() const {
    return gap_segment_state_ranges_.has_value();
  }

  const GapSegmentStateRanges& GetGapSegmentStateRanges() const {
    CHECK(gap_segment_state_ranges_.has_value());
    return gap_segment_state_ranges_.value();
  }

  void AddGapSegmentStateRange(
      const GapSegmentStateRange& gap_segment_state_range);

  static void UpdateCrossGapRangeEdgeState(
      Vector<CrossGap>& cross_gaps,
      wtf_size_t start_index,
      wtf_size_t end_index,
      CrossGap::EdgeIntersectionState new_state);

 private:
  LogicalOffset gap_logical_offset_;

  EdgeIntersectionState edge_state_ = EdgeIntersectionState::kNone;

  // If present, holds slices of this cross gap, each with a `GapSegmentState`
  // (Blocked / Empty). A cross gap usually spans range [1, N) in one piece, but
  // the presence of spanning items or empty cells can break it into multiple
  // state-specific sub‑ranges.
  std::optional<GapSegmentStateRanges> gap_segment_state_ranges_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_CROSS_GAP_H_
