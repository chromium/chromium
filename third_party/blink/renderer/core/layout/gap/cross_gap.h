// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_CROSS_GAP_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_CROSS_GAP_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_offset.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// This is used to hold the range [start, end] of which cross gaps come "before"
// and "after" the `MainGap` associated with this `CrossGap`. Where `start` and
// `end` describe the index range within the `cross_gaps_` vector of the
// `GapGeometry` where a given `CrossGap` is stored.
class CrossGapRange {
 public:
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
  void Increment(wtf_size_t cross_gap_index) {
    if (!start_index_.has_value()) {
      start_index_ = cross_gap_index;

      // Both start at the same index, but subsequent calls will increment the
      // end index.
      end_index_ = cross_gap_index;
    } else {
      CHECK(end_index_.has_value());
      CHECK_GT(cross_gap_index, *end_index_);
      CHECK_GT(cross_gap_index, *start_index_);
      end_index_ = cross_gap_index;
    }
  }

  String ToString() const {
    String str;
    str = "(" +
          (start_index_.has_value() ? String::Number(*start_index_) : "null") +
          " --> " +
          (end_index_.has_value() ? String::Number(*end_index_) : "null") + ")";
    return str;
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

  CrossGap(LogicalOffset offset) : gap_logical_start_offset_(offset) {}
  CrossGap(LogicalOffset offset, EdgeIntersectionState state)
      : gap_logical_start_offset_(offset), edge_state_(state) {}

  LogicalOffset GetGapStartOffset() const { return gap_logical_start_offset_; }

  String ToString(bool verbose = false) const {
    if (verbose) {
      String edge_state;
      if (StartsAtEdge()) {
        edge_state = "kStart";
      } else if (EndsAtEdge()) {
        edge_state = "kEnd";
      } else {
        edge_state = "kNone";
      }
      return String("CrossStartOffset(") +
             gap_logical_start_offset_.inline_offset.ToString() + ", " +
             gap_logical_start_offset_.block_offset.ToString() + "); " +
             "EdgeState: " + edge_state + ";";
    }

    return String("CrossStartOffset(") +
           gap_logical_start_offset_.inline_offset.ToString() + ", " +
           gap_logical_start_offset_.block_offset.ToString() + ")";
  }

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

 private:
  LogicalOffset gap_logical_start_offset_;

  EdgeIntersectionState edge_state_ = EdgeIntersectionState::kNone;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_CROSS_GAP_H_
