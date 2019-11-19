// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_REPLACED_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_REPLACED_PAINTER_H_

#include "third_party/blink/renderer/platform/geometry/layout_point.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

struct PaintInfo;
class ScopedPaintState;
class LayoutReplaced;

class ReplacedPainter {
  STACK_ALLOCATED();

 public:
  ReplacedPainter(const LayoutReplaced& layout_replaced)
      : layout_replaced_(layout_replaced) {}

  void Paint(const PaintInfo&);

  bool ShouldPaint(const ScopedPaintState&) const;

 private:
  bool ShouldPaintBoxDecorationBackground(const PaintInfo&);

  const LayoutReplaced& layout_replaced_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_REPLACED_PAINTER_H_
