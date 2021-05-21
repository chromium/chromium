// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_IMAGEBITMAP_IMAGE_BITMAP_RENDERING_CONTEXT_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_IMAGEBITMAP_IMAGE_BITMAP_RENDERING_CONTEXT_BASE_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context_factory.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/geometry/float_point.h"

namespace cc {
class Layer;
}

namespace blink {

class ImageBitmap;
class ImageLayerBridge;
class V8UnionHTMLCanvasElementOrOffscreenCanvas;

class MODULES_EXPORT ImageBitmapRenderingContextBase
    : public CanvasRenderingContext {
 public:
  ImageBitmapRenderingContextBase(CanvasRenderingContextHost*,
                                  const CanvasContextCreationAttributesCore&);
  ~ImageBitmapRenderingContextBase() override;

  void Trace(Visitor*) const override;

  // TODO(juanmihd): Remove this method crbug.com/941579
  HTMLCanvasElement* canvas() const {
    if (Host()->IsOffscreenCanvas())
      return nullptr;
    return static_cast<HTMLCanvasElement*>(Host());
  }

  bool CanCreateCanvas2dResourceProvider() const;
  V8UnionHTMLCanvasElementOrOffscreenCanvas* getHTMLOrOffscreenCanvas() const;

  void SetIsInHiddenPage(bool) override {}
  void SetIsBeingDisplayed(bool) override {}
  bool isContextLost() const override { return false; }
  // If SetImage receives a null imagebitmap, it will Reset the internal bitmap
  // to a black and transparent bitmap.
  void SetImage(ImageBitmap*);
  scoped_refptr<StaticBitmapImage> GetImage() final;

  void SetUV(const FloatPoint& left_top, const FloatPoint& right_bottom);
  bool IsComposited() const final { return true; }
  bool IsAccelerated() const final;
  bool PushFrame() override;

  bool IsOriginTopLeft() const override;

  cc::Layer* CcLayer() const final;
  // TODO(junov): handle lost contexts when content is GPU-backed
  void LoseContext(LostContextMode) override {}

  void Stop() override;

  bool IsPaintable() const final;

 protected:
  Member<ImageLayerBridge> image_layer_bridge_;

  // This function resets the internal image resource to a image of the same
  // size than the original, with the same properties, but completely black.
  // This is used to follow the standard regarding transferToBitmap
  scoped_refptr<StaticBitmapImage> GetImageAndResetInternal();

 private:
  void ResetInternalBitmapToBlackTransparent(int width, int height);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_IMAGEBITMAP_IMAGE_BITMAP_RENDERING_CONTEXT_BASE_H_
