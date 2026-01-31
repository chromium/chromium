// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_IMAGEBITMAP_IMAGE_BITMAP_RENDERING_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_IMAGEBITMAP_IMAGE_BITMAP_RENDERING_CONTEXT_H_

#include "base/byte_size.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_typedefs.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context_factory.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "ui/gfx/geometry/point_f.h"

namespace cc {
class Layer;
}

namespace blink {

class ExceptionState;
class ExecutionContext;
class ImageBitmap;
class ImageLayerBridge;
class V8UnionHTMLCanvasElementOrOffscreenCanvas;

class MODULES_EXPORT ImageBitmapRenderingContext final
    : public ScriptWrappable,
      public CanvasRenderingContext {
  DEFINE_WRAPPERTYPEINFO();

 public:
  class Factory : public CanvasRenderingContextFactory {
   public:
    Factory() = default;

    Factory(const Factory&) = delete;
    Factory& operator=(const Factory&) = delete;

    ~Factory() override = default;

    CanvasRenderingContext* Create(
        ExecutionContext*,
        CanvasRenderingContextHost*,
        const CanvasContextCreationAttributesCore&) override;
    CanvasRenderingContext::CanvasRenderingAPI GetRenderingAPI()
        const override {
      return CanvasRenderingContext::CanvasRenderingAPI::kBitmaprenderer;
    }
  };

  ImageBitmapRenderingContext(CanvasRenderingContextHost*,
                              const CanvasContextCreationAttributesCore&);

  void Trace(Visitor*) const override;

  V8UnionHTMLCanvasElementOrOffscreenCanvas* getHTMLOrOffscreenCanvas() const;

  void PageVisibilityChanged() override {}
  bool isContextLost() const override { return false; }
  // If SetImage receives a null imagebitmap, it will Reset the internal bitmap
  // to a black and transparent bitmap.
  void SetImage(ImageBitmap*);
  scoped_refptr<StaticBitmapImage> GetImage() final;

  void SetUV(const gfx::PointF& left_top, const gfx::PointF& right_bottom);

  SkAlphaType GetAlphaType() const override { return kPremul_SkAlphaType; }
  viz::SharedImageFormat GetSharedImageFormat() const override {
    return GetN32FormatForCanvas();
  }
  gfx::ColorSpace GetColorSpace() const override {
    return gfx::ColorSpace::CreateSRGB();
  }
  bool IsComposited() const final { return true; }
  bool PushFrame() override;

  cc::Layer* CcLayer() const final;
  // TODO(junov): handle lost contexts when content is GPU-backed
  void LoseContext(LostContextMode) override {}

  void Reset() override;

  base::ByteSize AllocatedBufferSize() const override;

  void Stop() override;

  scoped_refptr<StaticBitmapImage> PaintRenderingResultsToSnapshot(
      SourceDrawingBuffer source_buffer) override;

  bool IsPaintable() const final;

  // Script API
  void transferFromImageBitmap(ImageBitmap*, ExceptionState&);

  // CanvasRenderingContext implementation
  ImageBitmap* TransferToImageBitmap(ScriptState*, ExceptionState&) override;

  V8RenderingContext* AsV8RenderingContext() final;
  V8OffscreenRenderingContext* AsV8OffscreenRenderingContext() final;

  ~ImageBitmapRenderingContext() override;

 private:
  void Dispose() override;

  // This function resets the internal image resource to a image of the same
  // size than the original, with the same properties, but completely black.
  // This is used to follow the standard regarding transferToBitmap
  scoped_refptr<StaticBitmapImage> GetImageAndResetInternal();

  CanvasResourceProviderSharedImage*
  GetOrCreateResourceProviderForOffscreenCanvas();
  void ResetInternalBitmapToBlackTransparent(int width, int height);

  Member<ImageLayerBridge> image_layer_bridge_;
  std::unique_ptr<CanvasResourceProviderSharedImage>
      resource_provider_for_offscreen_canvas_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_IMAGEBITMAP_IMAGE_BITMAP_RENDERING_CONTEXT_H_
