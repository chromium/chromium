// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_CANVAS_RECORDING_CONTEXT_2D_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_CANVAS_RECORDING_CONTEXT_2D_H_

#include "base/memory/scoped_refptr.h"
#include "cc/paint/paint_canvas.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_path.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_rendering_context_2d_state.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

// CanvasRecordingContext2D implements the canvas_recording_context_2d.idl, a 2D
// context API that records PaintOps. This class holds the canvas recording API
// that is common to all three context types (2D, OffscreenCanvas and
// PaintCanvas). It's an abstract base class cannot be instantiated directly.
class MODULES_EXPORT CanvasRecordingContext2D : public CanvasPath {
 public:
  CanvasRecordingContext2D(const CanvasRecordingContext2D&) = delete;
  CanvasRecordingContext2D& operator=(const CanvasRecordingContext2D&) = delete;

  // Functions implementing the CanvasShadowStyles interface.
  virtual double shadowOffsetX() const;
  virtual void setShadowOffsetX(double);

  virtual double shadowOffsetY() const;
  virtual void setShadowOffsetY(double);

  virtual double shadowBlur() const;
  virtual void setShadowBlur(double);

  void Trace(Visitor*) const override;

 protected:
  CanvasRecordingContext2D();
  ALWAYS_INLINE CanvasRenderingContext2DState& GetState() const {
    return *state_stack_.back();
  }

  HeapVector<Member<CanvasRenderingContext2DState>> state_stack_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_CANVAS_RECORDING_CONTEXT_2D_H_
