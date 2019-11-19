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
class HTMLCanvasElementOrOffscreenCanvas;

class MODULES_EXPORT ImageBitmapRenderingContextBase
    : public CanvasRenderingContext {
 public:
  ImageBitmapRenderingContextBase(CanvasRenderingContextHost*,
                                  const CanvasContextCreationAttributesCore&);
  ~ImageBitmapRenderingContextBase() override;

  void Trace(blink::Visitor*) override;

  // TODO(juanmihd): Remove this method crbug.com/941579
  HTMLCanvasElement* canvas() const {
    if (Host()->IsOffscreenCanvas())
      return nullptr;
    return static_cast<HTMLCanvasElement*>(Host());
  }

  bool CanCreateCanvas2dResourceProvider() const;
  void getHTMLOrOffscreenCanvas(HTMLCanvasElementOrOffscreenCanvas&) const;

  void SetIsHidden(bool) override {}
  bool isContextLost() const override { return false; }
  void SetImage(ImageBitmap*);
  // The acceleration hint here is ignored as GetImage(AccelerationHint) only
  // calls to image_layer_bridge->GetImage(), without giving it a hint
  scoped_refptr<StaticBitmapImage> GetImage(AccelerationHint) final;
  // This function resets the internal image resource to a image of the same
  // size than the original, with the same properties, but completely black.
  // This is used to follow the standard regarding transferToBitmap
  scoped_refptr<StaticBitmapImage> GetImageAndResetInternal();
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
};

}  // namespace blink

#endif
