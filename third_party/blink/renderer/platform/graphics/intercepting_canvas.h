/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_INTERCEPTING_CANVAS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_INTERCEPTING_CANVAS_H_

#include "third_party/blink/renderer/platform/graphics/paint/paint_record.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"

namespace blink {

class InterceptingCanvasBase : public SkCanvas {
 public:
  template <typename DerivedCanvas>
  class CanvasInterceptorBase {
    STACK_ALLOCATED();

   protected:
    CanvasInterceptorBase(InterceptingCanvasBase* canvas) : canvas_(canvas) {
      ++canvas_->call_nesting_depth_;
    }

    ~CanvasInterceptorBase() {
      DCHECK_GT(canvas_->call_nesting_depth_, 0u);
      if (!--canvas_->call_nesting_depth_)
        canvas_->call_count_++;
    }

    DerivedCanvas* Canvas() { return static_cast<DerivedCanvas*>(canvas_); }
    bool TopLevelCall() const { return canvas_->CallNestingDepth() == 1; }
    InterceptingCanvasBase* canvas_;

   private:
    DISALLOW_COPY_AND_ASSIGN(CanvasInterceptorBase);
  };

  void ResetStepCount() { call_count_ = 0; }

 protected:
  explicit InterceptingCanvasBase(SkBitmap bitmap)
      : SkCanvas(bitmap), call_nesting_depth_(0), call_count_(0) {}
  InterceptingCanvasBase(int width, int height)
      : SkCanvas(width, height), call_nesting_depth_(0), call_count_(0) {}

  void UnrollDrawPicture(const SkPicture*,
                         const SkMatrix*,
                         const SkPaint*,
                         SkPicture::AbortCallback*);

  void onDrawPaint(const SkPaint&) override = 0;
  void onDrawPoints(PointMode,
                    size_t count,
                    const SkPoint pts[],
                    const SkPaint&) override = 0;
  void onDrawRect(const SkRect&, const SkPaint&) override = 0;
  void onDrawOval(const SkRect&, const SkPaint&) override = 0;
  void onDrawRRect(const SkRRect&, const SkPaint&) override = 0;
  void onDrawPath(const SkPath&, const SkPaint&) override = 0;
  void onDrawBitmap(const SkBitmap&,
                    SkScalar left,
                    SkScalar top,
                    const SkPaint*) override = 0;
  void onDrawBitmapRect(const SkBitmap&,
                        const SkRect* src,
                        const SkRect& dst,
                        const SkPaint*,
                        SrcRectConstraint) override = 0;
  void onDrawBitmapNine(const SkBitmap&,
                        const SkIRect& center,
                        const SkRect& dst,
                        const SkPaint*) override = 0;
  void onDrawImage(const SkImage*,
                   SkScalar,
                   SkScalar,
                   const SkPaint*) override = 0;
  void onDrawImageRect(const SkImage*,
                       const SkRect* src,
                       const SkRect& dst,
                       const SkPaint*,
                       SrcRectConstraint) override = 0;
  void onDrawVerticesObject(const SkVertices*,
                            SkBlendMode bmode,
                            const SkPaint&) override = 0;

  void onDrawDRRect(const SkRRect& outer,
                    const SkRRect& inner,
                    const SkPaint&) override = 0;
  void onDrawTextBlob(const SkTextBlob*,
                      SkScalar x,
                      SkScalar y,
                      const SkPaint&) override = 0;
  void onClipRect(const SkRect&, SkClipOp, ClipEdgeStyle) override = 0;
  void onClipRRect(const SkRRect&, SkClipOp, ClipEdgeStyle) override = 0;
  void onClipPath(const SkPath&, SkClipOp, ClipEdgeStyle) override = 0;
  void onClipRegion(const SkRegion&, SkClipOp) override = 0;
  void onDrawPicture(const SkPicture*,
                     const SkMatrix*,
                     const SkPaint*) override = 0;
  void didSetMatrix(const SkMatrix&) override = 0;
  void didConcat(const SkMatrix&) override = 0;
  void willSave() override = 0;
  SaveLayerStrategy getSaveLayerStrategy(const SaveLayerRec&) override = 0;
  void willRestore() override = 0;

  unsigned CallNestingDepth() const { return call_nesting_depth_; }
  unsigned CallCount() const { return call_count_; }

 private:
  unsigned call_nesting_depth_;
  unsigned call_count_;

  DISALLOW_COPY_AND_ASSIGN(InterceptingCanvasBase);
};

template <typename DerivedCanvas>
class CanvasInterceptor {};

template <typename DerivedCanvas,
          typename Interceptor = CanvasInterceptor<DerivedCanvas>>
class InterceptingCanvas : public InterceptingCanvasBase {
 protected:
  explicit InterceptingCanvas(SkBitmap bitmap)
      : InterceptingCanvasBase(bitmap) {}
  InterceptingCanvas(int width, int height)
      : InterceptingCanvasBase(width, height) {}

  void onDrawPaint(const SkPaint& paint) override {
    Interceptor interceptor(this);
    this->SkCanvas::onDrawPaint(paint);
  }

  void onDrawPoints(PointMode mode,
                    size_t count,
                    const SkPoint pts[],
                    const SkPaint& paint) override {
    Interceptor interceptor(this);
    this->SkCanvas::onDrawPoints(mode, count, pts, paint);
  }

  void onDrawRect(const SkRect& rect, const SkPaint& paint) override {
    Interceptor interceptor(this);
    this->SkCanvas::onDrawRect(rect, paint);
  }

  void onDrawOval(const SkRect& rect, const SkPaint& paint) override {
    Interceptor interceptor(this);
    this->SkCanvas::onDrawOval(rect, paint);
  }

  void onDrawRRect(const SkRRect& rrect, const SkPaint& paint) override {
    Interceptor interceptor(this);
    this->SkCanvas::onDrawRRect(rrect, paint);
  }

  void onDrawPath(const SkPath& path, const SkPaint& paint) override {
    Interceptor interceptor(this);
    this->SkCanvas::onDrawPath(path, paint);
  }

  void onDrawBitmap(const SkBitmap& bitmap,
                    SkScalar left,
                    SkScalar top,
                    const SkPaint* paint) override {
    Interceptor interceptor(this);
    this->SkCanvas::onDrawBitmap(bitmap, left, top, paint);
  }

  void onDrawBitmapRect(const SkBitmap& bitmap,
                        const SkRect* src,
                        const SkRect& dst,
                        const SkPaint* paint,
                        SrcRectConstraint constraint) override {
    Interceptor interceptor(this);
    this->SkCanvas::onDrawBitmapRect(bitmap, src, dst, paint, constraint);
  }

  void onDrawBitmapNine(const SkBitmap& bitmap,
                        const SkIRect& center,
                        const SkRect& dst,
                        const SkPaint* paint) override {
    Interceptor interceptor(this);
    this->SkCanvas::onDrawBitmapNine(bitmap, center, dst, paint);
  }

  void onDrawImage(const SkImage* image,
                   SkScalar x,
                   SkScalar y,
                   const SkPaint* paint) override {
    Interceptor interceptor(this);
    this->SkCanvas::onDrawImage(image, x, y, paint);
  }

  void onDrawImageRect(const SkImage* image,
                       const SkRect* src,
                       const SkRect& dst,
                       const SkPaint* paint,
                       SrcRectConstraint constraint) override {
    Interceptor interceptor(this);
    this->SkCanvas::onDrawImageRect(image, src, dst, paint, constraint);
  }

  void onDrawVerticesObject(const SkVertices* vertices,
                            SkBlendMode bmode,
                            const SkPaint& paint) override {
    Interceptor interceptor(this);
    this->SkCanvas::onDrawVerticesObject(vertices, bmode, paint);
  }

  void onDrawDRRect(const SkRRect& outer,
                    const SkRRect& inner,
                    const SkPaint& paint) override {
    Interceptor interceptor(this);
    this->SkCanvas::onDrawDRRect(outer, inner, paint);
  }

  void onDrawTextBlob(const SkTextBlob* blob,
                      SkScalar x,
                      SkScalar y,
                      const SkPaint& paint) override {
    Interceptor interceptor(this);
    this->SkCanvas::onDrawTextBlob(blob, x, y, paint);
  }

  void onClipRect(const SkRect& rect,
                  SkClipOp op,
                  ClipEdgeStyle edge_style) override {
    Interceptor interceptor(this);
    this->SkCanvas::onClipRect(rect, op, edge_style);
  }

  void onClipRRect(const SkRRect& rrect,
                   SkClipOp op,
                   ClipEdgeStyle edge_style) override {
    Interceptor interceptor(this);
    this->SkCanvas::onClipRRect(rrect, op, edge_style);
  }

  void onClipPath(const SkPath& path,
                  SkClipOp op,
                  ClipEdgeStyle edge_style) override {
    Interceptor interceptor(this);
    this->SkCanvas::onClipPath(path, op, edge_style);
  }

  void onClipRegion(const SkRegion& region, SkClipOp op) override {
    Interceptor interceptor(this);
    this->SkCanvas::onClipRegion(region, op);
  }

  void onDrawPicture(const SkPicture* picture,
                     const SkMatrix* matrix,
                     const SkPaint* paint) override {
    this->UnrollDrawPicture(picture, matrix, paint, nullptr);
  }

  void didSetMatrix(const SkMatrix& matrix) override {
    Interceptor interceptor(this);
    this->SkCanvas::didSetMatrix(matrix);
  }

  void didConcat(const SkMatrix& matrix) override {
    Interceptor interceptor(this);
    this->SkCanvas::didConcat(matrix);
  }

  void willSave() override {
    Interceptor interceptor(this);
    this->SkCanvas::willSave();
  }

  SkCanvas::SaveLayerStrategy getSaveLayerStrategy(
      const SaveLayerRec& rec) override {
    Interceptor interceptor(this);
    return this->SkCanvas::getSaveLayerStrategy(rec);
  }

  void willRestore() override {
    Interceptor interceptor(this);
    this->SkCanvas::willRestore();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_INTERCEPTING_CANVAS_H_
