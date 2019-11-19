// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GEOMETRY_NG_BORDER_EDGES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GEOMETRY_NG_BORDER_EDGES_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"

namespace blink {

// Which border edges should be painted. Due to fragmentation one or more may
// be skipped.
struct CORE_EXPORT NGBorderEdges {
  unsigned block_start : 1;
  unsigned line_right : 1;
  unsigned block_end : 1;
  unsigned line_left : 1;

  NGBorderEdges()
      : block_start(true), line_right(true), block_end(true), line_left(true) {}
  NGBorderEdges(bool block_start,
                bool line_right,
                bool block_end,
                bool line_left)
      : block_start(block_start),
        line_right(line_right),
        block_end(block_end),
        line_left(line_left) {}

  enum Physical {
    kTop = 1,
    kRight = 2,
    kBottom = 4,
    kLeft = 8,
    kAll = kTop | kRight | kBottom | kLeft
  };
  static NGBorderEdges FromPhysical(unsigned physical_edges,
                                    WritingMode writing_mode) {
    if (writing_mode == WritingMode::kHorizontalTb) {
      return NGBorderEdges(physical_edges & kTop, physical_edges & kRight,
                           physical_edges & kBottom, physical_edges & kLeft);
    }
    if (writing_mode != WritingMode::kSidewaysLr) {
      return NGBorderEdges(physical_edges & kRight, physical_edges & kBottom,
                           physical_edges & kLeft, physical_edges & kTop);
    }
    return NGBorderEdges(physical_edges & kLeft, physical_edges & kTop,
                         physical_edges & kRight, physical_edges & kBottom);
  }
  unsigned ToPhysical(WritingMode writing_mode) const {
    if (writing_mode == WritingMode::kHorizontalTb) {
      return (block_start ? kTop : 0) | (line_right ? kRight : 0) |
             (block_end ? kBottom : 0) | (line_left ? kLeft : 0);
    }
    if (writing_mode != WritingMode::kSidewaysLr) {
      return (block_start ? kRight : 0) | (line_right ? kBottom : 0) |
             (block_end ? kLeft : 0) | (line_left ? kTop : 0);
    }
    return (block_start ? kLeft : 0) | (line_right ? kTop : 0) |
           (block_end ? kRight : 0) | (line_left ? kBottom : 0);
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GEOMETRY_NG_BORDER_EDGES_H_
