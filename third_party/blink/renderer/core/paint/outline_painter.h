// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_OUTLINE_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_OUTLINE_PAINTER_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class ComputedStyle;
class GraphicsContext;
class Path;
struct PhysicalRect;

class OutlinePainter {
  STATIC_ONLY(OutlinePainter);

 public:
  static void PaintOutlineRects(GraphicsContext&,
                                const Vector<PhysicalRect>&,
                                const ComputedStyle&);

  static void PaintFocusRingPath(GraphicsContext&,
                                 const Path&,
                                 const ComputedStyle&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_OUTLINE_PAINTER_H_
