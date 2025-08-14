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
struct CrossGapRange {
  wtf_size_t start_index;
  wtf_size_t end_index;
};

// Represents any gap that intersects a `MainGap`. For example, in a row-based
// flex container, the `MainGap` would represent the gaps between flex lines,
// while the `CrossGaps` would represent the gaps between flex items in the same
// line. In Grid, we use the row gaps as our `MainGap`s and column gaps as
// `CrossGap`s. See third_party/blink/renderer/core/layout/gap/README.md for
// more information.
class CORE_EXPORT CrossGap {
 public:
  CrossGap(LogicalOffset offset) : gap_logical_start_offset_(offset) {}

  LogicalOffset GetGapStartOffset() const { return gap_logical_start_offset_; }

  blink::String ToString(bool verbose = false) const {
    return blink::String("(") +
           gap_logical_start_offset_.inline_offset.ToString() + ", " +
           gap_logical_start_offset_.block_offset.ToString() + ")";
  }

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

  void SetEdgeIntersectionState(EdgeIntersectionState state) {
    edge_state_ = state;
  }

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
