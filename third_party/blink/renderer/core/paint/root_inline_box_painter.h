// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_ROOT_INLINE_BOX_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_ROOT_INLINE_BOX_PAINTER_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class LayoutUnit;
class RootInlineBox;
struct PaintInfo;
struct PhysicalOffset;

class RootInlineBoxPainter {
  STACK_ALLOCATED();

 public:
  RootInlineBoxPainter(const RootInlineBox& root_inline_box)
      : root_inline_box_(root_inline_box) {}

  void Paint(const PaintInfo&,
             const PhysicalOffset&,
             LayoutUnit line_top,
             LayoutUnit line_bottom);

 private:
  void PaintEllipsisBox(const PaintInfo&,
                        const PhysicalOffset& paint_offset,
                        LayoutUnit line_top,
                        LayoutUnit line_bottom) const;

  const RootInlineBox& root_inline_box_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_ROOT_INLINE_BOX_PAINTER_H_
