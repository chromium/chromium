// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGStaticPosition_h
#define NGStaticPosition_h

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_physical_offset.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"

namespace blink {

// Represents static position of an out of flow descendant.
struct CORE_EXPORT NGStaticPosition {
  enum Type { kTopLeft, kTopRight, kBottomLeft, kBottomRight };

  Type type;  // Logical corner that corresponds to physical top left.
  NGPhysicalOffset offset;

  // Creates a position with proper type wrt writing mode and direction.
  // It expects physical offset of inline_start/block_start vertex.
  static NGStaticPosition Create(WritingMode, TextDirection, NGPhysicalOffset);

  // Left/Right/TopPosition functions map static position to inset of
  // left/right/top edge wrt container space.
  // The function arguments are required to solve the equation:
  // contaner_size = left + margin_left + width + margin_right + right
  LayoutUnit LeftInset(LayoutUnit container_size,
                       LayoutUnit width,
                       LayoutUnit margin_left,
                       LayoutUnit margin_right) const;
  LayoutUnit RightInset(LayoutUnit container_size,
                        LayoutUnit width,
                        LayoutUnit margin_left,
                        LayoutUnit margin_right) const;
  LayoutUnit TopInset(LayoutUnit container_size,
                      LayoutUnit height,
                      LayoutUnit margin_top,
                      LayoutUnit margin_bottom) const;
  LayoutUnit BottomInset(LayoutUnit container_size,
                         LayoutUnit height,
                         LayoutUnit margin_top,
                         LayoutUnit margin_bottom) const;

  LayoutUnit Left() const;
  LayoutUnit Right() const;
  LayoutUnit Top() const;
  LayoutUnit Bottom() const;

  bool HasTop() const { return type == kTopLeft || type == kTopRight; }
  bool HasLeft() const { return type == kTopLeft || type == kBottomLeft; }
};

}  // namespace blink

#endif  // NGStaticPosition_h
