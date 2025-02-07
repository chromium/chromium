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
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"

namespace blink {

class DOMMatrix;
class DOMMatrixInit;
class HTMLCanvasElement;
class V8ImageSmoothingQuality;
class V8UnionCanvasFilterOrString;

// CanvasRecordingContext2D implements the canvas_recording_context_2d.idl, a 2D
// context API that records PaintOps. This class holds the canvas recording API
// that is common to all three context types (2D, OffscreenCanvas and
// PaintCanvas). It's an abstract base class cannot be instantiated directly.
class MODULES_EXPORT CanvasRecordingContext2D : public CanvasPath {
 public:
  CanvasRecordingContext2D(const CanvasRecordingContext2D&) = delete;
  CanvasRecordingContext2D& operator=(const CanvasRecordingContext2D&) = delete;

  // Functions implementing the CanvasTransform Interface.
  void scale(double sx, double sy);
  void rotate(double angle_in_radians);
  void translate(double tx, double ty);
  void transform(double m11,
                 double m12,
                 double m21,
                 double m22,
                 double dx,
                 double dy);
  void setTransform(double m11,
                    double m12,
                    double m21,
                    double m22,
                    double dx,
                    double dy);
  void setTransform(DOMMatrixInit*, ExceptionState&);
  virtual DOMMatrix* getTransform();
  virtual void resetTransform();

  // Functions implementing the CanvasCompositing interface.
  // Alpha value that goes from 0 to 1.
  double globalAlpha() const;
  void setGlobalAlpha(double);

  String globalCompositeOperation() const;
  void setGlobalCompositeOperation(const String&);

  const V8UnionCanvasFilterOrString* filter() const;
  void setFilter(ScriptState*, const V8UnionCanvasFilterOrString* input);

  // Functions implementing the CanvasImageSmoothing interface.
  bool imageSmoothingEnabled() const;
  void setImageSmoothingEnabled(bool);
  V8ImageSmoothingQuality imageSmoothingQuality() const;
  void setImageSmoothingQuality(const V8ImageSmoothingQuality&);

  // Functions implementing the CanvasShadowStyles interface.
  virtual double shadowOffsetX() const;
  virtual void setShadowOffsetX(double);

  virtual double shadowOffsetY() const;
  virtual void setShadowOffsetY(double);

  virtual double shadowBlur() const;
  virtual void setShadowBlur(double);

  virtual cc::PaintCanvas* GetOrCreatePaintCanvas() = 0;
  void Trace(Visitor*) const override;

 protected:
  CanvasRecordingContext2D();
  virtual HTMLCanvasElement* HostAsHTMLCanvasElement() const;

  // Helper functions for Filter.
  virtual void SnapshotStateForFilter() {}

  ALWAYS_INLINE CanvasRenderingContext2DState& GetState() const {
    return *state_stack_.back();
  }

  HeapVector<Member<CanvasRenderingContext2DState>> state_stack_;

 private:
  void SetTransform(const AffineTransform&);
};

ALWAYS_INLINE void CanvasRecordingContext2D::SetTransform(
    const AffineTransform& matrix) {
  GetState().SetTransform(matrix);
  SetIsTransformInvertible(matrix.IsInvertible());
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_CANVAS_RECORDING_CONTEXT_2D_H_
