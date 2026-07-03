// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_IMAGEBITMAP_IMAGE_BITMAP_RENDERING_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_IMAGEBITMAP_IMAGE_BITMAP_RENDERING_CONTEXT_H_

#include "base/byte_size.h"
#include "base/memory/scoped_refptr.h"
#include "cc/layers/texture_layer_client.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_typedefs.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context_factory.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/union_base.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "ui/gfx/geometry/point_f.h"

namespace cc {
class Layer;
class TextureLayer;
}

namespace gpu {
struct SyncToken;
class ClientSharedImage;
}  // namespace gpu

namespace blink {

class CanvasNon2DResourceProvider;
class ExceptionState;
class ExecutionContext;
class ImageBitmap;
class WebGraphicsSharedImageInterfaceProvider;
class V8UnionHTMLCanvasElementOrOffscreenCanvas;
class WebGraphicsContext3DProviderWrapper;

class MODULES_EXPORT ImageBitmapRenderingContext final
    : public ScriptWrappable,
      public CanvasRenderingContext,
      public cc::TextureLayerClient {
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

  static scoped_refptr<StaticBitmapImage> MakeAccelerated(
      const scoped_refptr<StaticBitmapImage>& source,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>
          context_provider_wrapper);

  void Trace(Visitor*) const override;

  bindings::OptimizedReturnProxy<V8UnionHTMLCanvasElementOrOffscreenCanvas>
  getHTMLOrOffscreenCanvas(ScriptState*) const;

  void PageVisibilityChanged() override {}
  bool isContextLost() const override { return false; }
  // If SetImage receives a null imagebitmap, it will Reset the internal bitmap
  // to a black and transparent bitmap.
  void SetImage(ImageBitmap*);
  scoped_refptr<StaticBitmapImage> GetImage() final;

  void SetUV(const gfx::PointF& left_top, const gfx::PointF& right_bottom);

  // TODO(https://crbug.com/40206688): This should reflect the opacity of the
  // ImageBitmap.
  bool IsOpaque() const override { return false; }
  bool IsComposited() const final { return true; }
  scoped_refptr<CanvasResource> GetResourceForPushFrame(
      bool& should_call_push_frame) override;

  cc::Layer* CcLayer() const final;

  // cc::TextureLayerClient implementation.
  bool PrepareTransferableResource(
      viz::TransferableResource* out_resource,
      viz::ReleaseCallback* out_release_callback) override;

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

  void ResetInternalBitmapToBlackTransparent(int width, int height);

  void SetImageInternal(scoped_refptr<StaticBitmapImage>);
  void ResourceReleasedGpu(scoped_refptr<StaticBitmapImage>,
                           const gpu::SyncToken&,
                           bool lost_resource);

  struct SoftwareResource {
    SoftwareResource();
    SoftwareResource(SoftwareResource&& other);
    SoftwareResource& operator=(SoftwareResource&& other);

    scoped_refptr<gpu::ClientSharedImage> shared_image;
    gpu::SyncToken sync_token;
    base::WeakPtr<blink::WebGraphicsSharedImageInterfaceProvider> sii_provider;
  };

  SoftwareResource CreateOrRecycleSoftwareResource(
      const gfx::Size& size,
      const gfx::ColorSpace& color_space);

  void ResourceReleasedSoftware(SoftwareResource resource,
                                const gpu::SyncToken&,
                                bool lost_resource);

  Vector<SoftwareResource> recycled_software_resources_;

  scoped_refptr<StaticBitmapImage> image_;
  bool disposed_ = false;
  bool has_presented_since_last_set_image_ = false;
  bool is_opaque_ = false;
  scoped_refptr<cc::TextureLayer> layer_;
  std::unique_ptr<CanvasNon2DResourceProvider>
      resource_provider_for_offscreen_canvas_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_IMAGEBITMAP_IMAGE_BITMAP_RENDERING_CONTEXT_H_
