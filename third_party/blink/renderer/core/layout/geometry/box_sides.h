// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GEOMETRY_BOX_SIDES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GEOMETRY_BOX_SIDES_H_

#include "third_party/blink/renderer/platform/text/writing_mode.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// Presence of box sides (e.g. borders) for a fragment, in the logical
// coordinate space. Line and / or block fragmentation may cause a layout box to
// be split into multiple lines, columns, etc., and, as long as
// 'box-decoration-break' is 'slice' (i.e. the initial value), the leading
// border should only be painted at the first fragment, and the trailing border
// should only be painted at the last one.
struct LogicalBoxSides {
  STACK_ALLOCATED();

 public:
  bool block_start = true;
  bool line_right = true;
  bool block_end = true;
  bool line_left = true;

  LogicalBoxSides() = default;
  LogicalBoxSides(bool block_start,
                  bool line_right,
                  bool block_end,
                  bool line_left)
      : block_start(block_start),
        line_right(line_right),
        block_end(block_end),
        line_left(line_left) {}
};

// Presence of box sides (e.g. borders) for a fragment, in the physical
// coordinate space.
struct PhysicalBoxSides {
  STACK_ALLOCATED();

 public:
  bool top = true;
  bool right = true;
  bool bottom = true;
  bool left = true;

  PhysicalBoxSides() = default;
  PhysicalBoxSides(bool top, bool right, bool bottom, bool left)
      : top(top), right(right), bottom(bottom), left(left) {}
  PhysicalBoxSides(LogicalBoxSides logical, WritingMode writing_mode) {
    // The values sideways-lr and sideways-rl are not supported by the engine,
    // although they're part of the WritingMode enum.
    DCHECK(writing_mode != WritingMode::kSidewaysLr);
    DCHECK(writing_mode != WritingMode::kSidewaysRl);

    if (writing_mode == WritingMode::kHorizontalTb) {
      top = logical.block_start;
      right = logical.line_right;
      bottom = logical.block_end;
      left = logical.line_left;
    } else {
      top = logical.line_left;
      bottom = logical.line_right;
      if (writing_mode == WritingMode::kVerticalRl) {
        right = logical.block_start;
        left = logical.block_end;
      } else {
        DCHECK_EQ(writing_mode, WritingMode::kVerticalLr);
        right = logical.block_end;
        left = logical.block_start;
      }
    }
  }

  bool IsEmpty() const { return !top && !right && !bottom && !left; }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GEOMETRY_BOX_SIDES_H_
