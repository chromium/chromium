// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_INLINE_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_INLINE_PAINTER_H_

#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

struct PaintInfo;
class LayoutInline;

class InlinePainter {
  STACK_ALLOCATED();

 public:
  InlinePainter(const LayoutInline& layout_inline)
      : layout_inline_(layout_inline) {}

  void Paint(const PaintInfo&);

 private:
  const LayoutInline& layout_inline_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_INLINE_PAINTER_H_
