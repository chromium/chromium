// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_SCOPED_CANVAS_SUBTREE_ID_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_SCOPED_CANVAS_SUBTREE_ID_H_

#include "third_party/blink/renderer/platform/graphics/compositor_element_id.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class ScopedCanvasSubtreeId final {
  STACK_ALLOCATED();

 public:
  explicit ScopedCanvasSubtreeId(PaintController& paint_controller,
                                 CompositorElementId id)
      : paint_controller_(paint_controller),
        previous_canvas_subtree_id_(paint_controller.CanvasSubtreeId()) {
    paint_controller.SetCanvasSubtreeId(id);
  }
  ScopedCanvasSubtreeId(const ScopedCanvasSubtreeId&) = delete;
  ScopedCanvasSubtreeId& operator=(const ScopedCanvasSubtreeId&) = delete;
  ~ScopedCanvasSubtreeId() {
    paint_controller_.SetCanvasSubtreeId(previous_canvas_subtree_id_);
  }

 private:
  PaintController& paint_controller_;
  CompositorElementId previous_canvas_subtree_id_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_SCOPED_CANVAS_SUBTREE_ID_H_
