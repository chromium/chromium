// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_TEST_MOCK_PAINT_CANVAS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_TEST_MOCK_PAINT_CANVAS_H_

#include "cc/paint/paint_canvas.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_flags.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_image.h"

namespace cc {
class SkottieWrapper;
}  // namespace cc

namespace blink {

class MockPaintCanvas : public cc::PaintCanvas {
 public:
  MOCK_CONST_METHOD0(imageInfo, SkImageInfo());
  MOCK_METHOD3(accessTopLayerPixels,
               void*(SkImageInfo* info, size_t* rowBytes, SkIPoint* origin));
  MOCK_METHOD0(flush, void());
  MOCK_METHOD0(save, int());
  MOCK_METHOD2(saveLayer, int(const SkRect* bounds, const PaintFlags* flags));
  MOCK_METHOD2(saveLayerAlpha, int(const SkRect* bounds, uint8_t alpha));
  MOCK_METHOD0(restore, void());
  MOCK_CONST_METHOD0(getSaveCount, int());
  MOCK_METHOD1(restoreToCount, void(int save_count));
  MOCK_METHOD2(translate, void(SkScalar dx, SkScalar dy));
  MOCK_METHOD2(scale, void(SkScalar sx, SkScalar sy));
  MOCK_METHOD1(rotate, void(SkScalar degrees));
  MOCK_METHOD1(concat, void(const SkMatrix& matrix));
  MOCK_METHOD1(setMatrix, void(const SkMatrix& matrix));
  MOCK_METHOD3(clipRect,
               void(const SkRect& rect, SkClipOp op, bool do_anti_alias));
  MOCK_METHOD3(clipRRect,
               void(const SkRRect& rrect, SkClipOp op, bool do_anti_alias));
  MOCK_METHOD3(clipPath,
               void(const SkPath& path, SkClipOp op, bool do_anti_alias));
  MOCK_CONST_METHOD0(getLocalClipBounds, SkRect());
  MOCK_CONST_METHOD1(getLocalClipBounds, bool(SkRect* bounds));
  MOCK_CONST_METHOD0(getDeviceClipBounds, SkIRect());
  MOCK_CONST_METHOD1(getDeviceClipBounds, bool(SkIRect* bounds));
  MOCK_METHOD2(drawColor, void(SkColor color, SkBlendMode mode));
  MOCK_METHOD1(clear, void(SkColor color));
  MOCK_METHOD5(drawLine,
               void(SkScalar x0,
                    SkScalar y0,
                    SkScalar x1,
                    SkScalar y1,
                    const PaintFlags& flags));
  MOCK_METHOD2(drawRect, void(const SkRect& rect, const PaintFlags& flags));
  MOCK_METHOD2(drawIRect, void(const SkIRect& rect, const PaintFlags& flags));
  MOCK_METHOD2(drawOval, void(const SkRect& oval, const PaintFlags& flags));
  MOCK_METHOD2(drawRRect, void(const SkRRect& rrect, const PaintFlags& flags));
  MOCK_METHOD3(drawDRRect,
               void(const SkRRect& outer,
                    const SkRRect& inner,
                    const PaintFlags& flags));
  MOCK_METHOD4(drawRoundRect,
               void(const SkRect& rect,
                    SkScalar rx,
                    SkScalar ry,
                    const PaintFlags& flags));
  MOCK_METHOD2(drawPath, void(const SkPath& path, const PaintFlags& flags));
  MOCK_METHOD4(drawImage,
               void(const PaintImage& image,
                    SkScalar left,
                    SkScalar top,
                    const PaintFlags* flags));
  MOCK_METHOD5(drawImageRect,
               void(const PaintImage& image,
                    const SkRect& src,
                    const SkRect& dst,
                    const PaintFlags* flags,
                    SrcRectConstraint constraint));
  MOCK_METHOD3(drawSkottie,
               void(scoped_refptr<cc::SkottieWrapper> skottie,
                    const SkRect& dst,
                    float t));
  MOCK_METHOD4(drawBitmap,
               void(const SkBitmap& bitmap,
                    SkScalar left,
                    SkScalar top,
                    const PaintFlags* flags));
  MOCK_METHOD4(
      drawTextBlob,
      void(sk_sp<SkTextBlob>, SkScalar x, SkScalar y, const PaintFlags& flags));
  MOCK_METHOD5(drawTextBlob,
               void(sk_sp<SkTextBlob>,
                    SkScalar x,
                    SkScalar y,
                    cc::NodeId node_id,
                    const PaintFlags& flags));

  MOCK_METHOD1(drawPicture, void(sk_sp<const PaintRecord> record));
  MOCK_CONST_METHOD0(isClipEmpty, bool());
  MOCK_CONST_METHOD0(isClipRect, bool());
  MOCK_CONST_METHOD0(getTotalMatrix, const SkMatrix&());

  MOCK_METHOD3(Annotate,
               void(AnnotationType type,
                    const SkRect& rect,
                    sk_sp<SkData> data));
  MOCK_METHOD0(GetPrintingMetafile, printing::MetafileSkia*());
  MOCK_METHOD1(SetPrintingMetafile, void(printing::MetafileSkia*));
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_TEST_MOCK_PAINT_CANVAS_H_
