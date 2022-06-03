// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_FRAME_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_FRAME_PAINTER_H_

#include "third_party/blink/renderer/core/paint/paint_phase.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class CullRect;
class GraphicsContext;
class LocalFrameView;

class FramePainter {
  STACK_ALLOCATED();

 public:
  explicit FramePainter(const LocalFrameView& frame_view)
      : frame_view_(&frame_view) {}
  FramePainter(const FramePainter&) = delete;
  FramePainter& operator=(const FramePainter&) = delete;

  void Paint(GraphicsContext&, const GlobalPaintFlags, const CullRect&);
  void PaintContents(GraphicsContext&, const GlobalPaintFlags, const CullRect&);

 private:
  const LocalFrameView& GetFrameView();

  const LocalFrameView* frame_view_;
  static bool in_paint_contents_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_FRAME_PAINTER_H_
