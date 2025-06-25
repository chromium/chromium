// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/imagebitmap/image_bitmap_rendering_context_base.h"

#include "base/metrics/histogram_functions.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_htmlcanvaselement_offscreencanvas.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/image_layer_bridge.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {

ImageBitmapRenderingContextBase::ImageBitmapRenderingContextBase(
    CanvasRenderingContextHost* host,
    const CanvasContextCreationAttributesCore& attrs)
    : CanvasRenderingContext(host, attrs, CanvasRenderingAPI::kBitmaprenderer),
      image_layer_bridge_(MakeGarbageCollected<ImageLayerBridge>(
          attrs.alpha ? kNonOpaque : kOpaque)) {
  host->InitializeLayerWithCSSProperties(image_layer_bridge_->CcLayer());
}

ImageBitmapRenderingContextBase::~ImageBitmapRenderingContextBase() = default;

V8UnionHTMLCanvasElementOrOffscreenCanvas*
ImageBitmapRenderingContextBase::getHTMLOrOffscreenCanvas() const {
  if (Host()->IsOffscreenCanvas()) {
    return MakeGarbageCollected<V8UnionHTMLCanvasElementOrOffscreenCanvas>(
        static_cast<OffscreenCanvas*>(Host()));
  }
  return MakeGarbageCollected<V8UnionHTMLCanvasElementOrOffscreenCanvas>(
      static_cast<HTMLCanvasElement*>(Host()));
}

void ImageBitmapRenderingContextBase::Reset() {
  CHECK(Host());
  CHECK(Host()->IsOffscreenCanvas());
  resource_provider_.reset();
  Host()->DiscardResources();
}

void ImageBitmapRenderingContextBase::Stop() {
  image_layer_bridge_->Dispose();
}

scoped_refptr<StaticBitmapImage>
ImageBitmapRenderingContextBase::PaintRenderingResultsToSnapshot(
    SourceDrawingBuffer source_buffer,
    FlushReason reason) {
  return GetImage(reason);
}

void ImageBitmapRenderingContextBase::Dispose() {
  Stop();
  resource_provider_.reset();
  CanvasRenderingContext::Dispose();
}

void ImageBitmapRenderingContextBase::ResetInternalBitmapToBlackTransparent(
    int width,
    int height) {
  SkBitmap black_bitmap;
  if (black_bitmap.tryAllocN32Pixels(width, height)) {
    black_bitmap.eraseARGB(0, 0, 0, 0);
    auto image = SkImages::RasterFromBitmap(black_bitmap);
    if (image) {
      image_layer_bridge_->SetImage(
          UnacceleratedStaticBitmapImage::Create(image));
    }
  }
}

void ImageBitmapRenderingContextBase::SetImage(ImageBitmap* image_bitmap) {
  DCHECK(!image_bitmap || !image_bitmap->IsNeutered());

  // According to the standard TransferFromImageBitmap(null) has to reset the
  // internal bitmap and create a black transparent one.
  if (image_bitmap)
    image_layer_bridge_->SetImage(image_bitmap->BitmapImage());
  else
    ResetInternalBitmapToBlackTransparent(Host()->width(), Host()->height());

  DidDraw(CanvasPerformanceMonitor::DrawType::kOther);

  if (image_bitmap)
    image_bitmap->close();
}

scoped_refptr<StaticBitmapImage> ImageBitmapRenderingContextBase::GetImage(
    FlushReason) {
  return image_layer_bridge_->GetImage();
}

scoped_refptr<StaticBitmapImage>
ImageBitmapRenderingContextBase::GetImageAndResetInternal() {
  if (!image_layer_bridge_->GetImage())
    return nullptr;
  scoped_refptr<StaticBitmapImage> copy_image = image_layer_bridge_->GetImage();

  ResetInternalBitmapToBlackTransparent(copy_image->width(),
                                        copy_image->height());

  return copy_image;
}

void ImageBitmapRenderingContextBase::SetUV(const gfx::PointF& left_top,
                                            const gfx::PointF& right_bottom) {
  image_layer_bridge_->SetUV(left_top, right_bottom);
}

cc::Layer* ImageBitmapRenderingContextBase::CcLayer() const {
  return image_layer_bridge_->CcLayer();
}

bool ImageBitmapRenderingContextBase::IsPaintable() const {
  return !!image_layer_bridge_->GetImage();
}

void ImageBitmapRenderingContextBase::Trace(Visitor* visitor) const {
  visitor->Trace(image_layer_bridge_);
  ScriptWrappable::Trace(visitor);
  CanvasRenderingContext::Trace(visitor);
}

CanvasResourceProvider* ImageBitmapRenderingContextBase::
    GetOrCreateResourceProviderForOffscreenCanvas() {
  CHECK(Host()->IsOffscreenCanvas());
  if (isContextLost() && !IsContextBeingRestored()) {
    return nullptr;
  }

  if (CanvasResourceProvider* provider = resource_provider_.get()) {
    if (!provider->IsValid()) {
      // The canvas context is not lost but the provider is invalid. This
      // happens if the GPU process dies in the middle of a render task. The
      // canvas is notified of GPU context losses via the `NotifyGpuContextLost`
      // callback and restoration happens in `TryRestoreContextEvent`. Both
      // callbacks are executed in their own separate task. If the GPU context
      // goes invalid in the middle of a render task, the canvas won't
      // immediately know about it and canvas APIs will continue using the
      // provider that is now invalid. We can early return here, trying to
      // re-create the provider right away would just fail. We need to let
      // `TryRestoreContextEvent` wait for the GPU process to up again.
      return nullptr;
    }
    return provider;
  }

  if (!Host()->IsValidImageSize() && !Host()->Size().IsEmpty()) {
    LoseContext(CanvasRenderingContext::kInvalidCanvasSize);
    return nullptr;
  }

  std::unique_ptr<CanvasResourceProvider> provider;
  gfx::Size surface_size(Host()->width(), Host()->height());
  const SkAlphaType alpha_type = GetAlphaType();
  const viz::SharedImageFormat format = GetSharedImageFormat();
  const gfx::ColorSpace color_space = GetColorSpace();
  if (SharedGpuContext::IsGpuCompositingEnabled()) {
    provider = CanvasResourceProvider::CreateSharedImageProvider(
        Host()->Size(), format, alpha_type, color_space,
        CanvasResourceProvider::ShouldInitialize::kCallClear,
        SharedGpuContext::ContextProviderWrapper(), RasterMode::kGPU,
        gpu::SHARED_IMAGE_USAGE_DISPLAY_READ, Host());
  } else if (static_cast<OffscreenCanvas*>(Host())->HasPlaceholderCanvas()) {
    base::WeakPtr<CanvasResourceDispatcher> dispatcher_weakptr =
        Host()->GetOrCreateResourceDispatcher()->GetWeakPtr();
    provider =
        CanvasResourceProvider::CreateSharedImageProviderForSoftwareCompositor(
            Host()->Size(), format, alpha_type, color_space,
            CanvasResourceProvider::ShouldInitialize::kCallClear,
            SharedGpuContext::SharedImageInterfaceProvider(), Host());
  }

  if (!provider) {
    // Last resort fallback is to use the bitmap provider. Using this
    // path is normal for software-rendered OffscreenCanvases that have no
    // placeholder canvas. If there is a placeholder, its content will not be
    // visible on screen, but at least readbacks will work. Failure to create
    // another type of resource prover above is a sign that the graphics
    // pipeline is in a bad state (e.g. gpu process crashed, out of memory)
    provider = CanvasResourceProvider::CreateBitmapProvider(
        Host()->Size(), format, alpha_type, color_space,
        CanvasResourceProvider::ShouldInitialize::kCallClear, Host());
  }

  resource_provider_ = std::move(provider);
  Host()->UpdateMemoryUsage();

  if (resource_provider_.get() && resource_provider_.get()->IsValid()) {
    // todo(crbug.com/1064363)  Add a separate UMA for Offscreen Canvas usage
    // and understand if the if (ResourceProvider() &&
    // ResourceProvider()->IsValid()) is really needed.
    base::UmaHistogramBoolean("Blink.Canvas.ResourceProviderIsAccelerated",
                              resource_provider_.get()->IsAccelerated());
    base::UmaHistogramEnumeration("Blink.Canvas.ResourceProviderType",
                                  resource_provider_.get()->GetType());
    Host()->DidDraw();
  }
  return resource_provider_.get();
}

bool ImageBitmapRenderingContextBase::IsAccelerated() const {
  auto* resource_provider = resource_provider_.get();
  return resource_provider ? resource_provider->IsAccelerated()
                           : Host()->ShouldTryToUseGpuRaster();
}

bool ImageBitmapRenderingContextBase::PushFrame() {
  DCHECK(Host());
  DCHECK(Host()->IsOffscreenCanvas());
  if (!GetOrCreateResourceProviderForOffscreenCanvas()) {
    return false;
  }

  scoped_refptr<StaticBitmapImage> image = image_layer_bridge_->GetImage();
  if (!image) {
    return false;
  }
  cc::PaintFlags paint_flags;
  paint_flags.setBlendMode(SkBlendMode::kSrc);
  resource_provider_->Canvas().drawImage(image->PaintImageForCurrentFrame(), 0,
                                         0, SkSamplingOptions(), &paint_flags);
  scoped_refptr<CanvasResource> resource =
      resource_provider_->ProduceCanvasResource(FlushReason::kNon2DCanvas);
  Host()->PushFrame(
      std::move(resource),
      SkIRect::MakeWH(image_layer_bridge_->GetImage()->Size().width(),
                      image_layer_bridge_->GetImage()->Size().height()));
  return true;
}

}  // namespace blink
