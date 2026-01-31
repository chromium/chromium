// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/imagebitmap/image_bitmap_rendering_context.h"

#include <utility>

#include "base/metrics/histogram_functions.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_htmlcanvaselement_offscreencanvas.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_offscreen_rendering_context.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_rendering_context.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/image_layer_bridge.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {

ImageBitmapRenderingContext::ImageBitmapRenderingContext(
    CanvasRenderingContextHost* host,
    const CanvasContextCreationAttributesCore& attrs)
    : CanvasRenderingContext(host, attrs, CanvasRenderingAPI::kBitmaprenderer),
      image_layer_bridge_(MakeGarbageCollected<ImageLayerBridge>(
          attrs.alpha ? kNonOpaque : kOpaque)) {
  host->InitializeLayerWithCSSProperties(image_layer_bridge_->CcLayer());
}

ImageBitmapRenderingContext::~ImageBitmapRenderingContext() = default;

V8UnionHTMLCanvasElementOrOffscreenCanvas*
ImageBitmapRenderingContext::getHTMLOrOffscreenCanvas() const {
  if (Host()->IsOffscreenCanvas()) {
    return MakeGarbageCollected<V8UnionHTMLCanvasElementOrOffscreenCanvas>(
        static_cast<OffscreenCanvas*>(Host()));
  }
  return MakeGarbageCollected<V8UnionHTMLCanvasElementOrOffscreenCanvas>(
      static_cast<HTMLCanvasElement*>(Host()));
}

void ImageBitmapRenderingContext::Reset() {
  CHECK(Host());
  CHECK(Host()->IsOffscreenCanvas());
  resource_provider_for_offscreen_canvas_.reset();
  Host()->DiscardResources();
}

base::ByteSize ImageBitmapRenderingContext::AllocatedBufferSize() const {
  if (!IsPaintable()) {
    return base::ByteSize();
  }
  base::ByteSize result =
      image_layer_bridge_->GetImage()->EstimatedSizeInBytes();
  if (resource_provider_for_offscreen_canvas_) {
    result += resource_provider_for_offscreen_canvas_->EstimatedSizeInBytes();
  }
  return result;
}

void ImageBitmapRenderingContext::Stop() {
  image_layer_bridge_->Dispose();
}

scoped_refptr<StaticBitmapImage>
ImageBitmapRenderingContext::PaintRenderingResultsToSnapshot(
    SourceDrawingBuffer source_buffer) {
  return GetImage();
}

void ImageBitmapRenderingContext::Dispose() {
  Stop();
  resource_provider_for_offscreen_canvas_.reset();
  CanvasRenderingContext::Dispose();
}

void ImageBitmapRenderingContext::ResetInternalBitmapToBlackTransparent(
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

void ImageBitmapRenderingContext::SetImage(ImageBitmap* image_bitmap) {
  DCHECK(!image_bitmap || !image_bitmap->IsNeutered());

  // According to the standard TransferFromImageBitmap(null) has to reset the
  // internal bitmap and create a black transparent one.
  if (image_bitmap) {
    image_layer_bridge_->SetImage(image_bitmap->BitmapImage());
  } else {
    ResetInternalBitmapToBlackTransparent(Host()->width(), Host()->height());
  }

  DidDraw(CanvasPerformanceMonitor::DrawType::kOther);

  if (image_bitmap) {
    image_bitmap->close();
  }
  Host()->UpdateMemoryUsage();
}

scoped_refptr<StaticBitmapImage> ImageBitmapRenderingContext::GetImage() {
  return image_layer_bridge_->GetImage();
}

scoped_refptr<StaticBitmapImage>
ImageBitmapRenderingContext::GetImageAndResetInternal() {
  if (!image_layer_bridge_->GetImage()) {
    return nullptr;
  }
  scoped_refptr<StaticBitmapImage> copy_image = image_layer_bridge_->GetImage();

  ResetInternalBitmapToBlackTransparent(copy_image->width(),
                                        copy_image->height());

  return copy_image;
}

void ImageBitmapRenderingContext::SetUV(const gfx::PointF& left_top,
                                        const gfx::PointF& right_bottom) {
  image_layer_bridge_->SetUV(left_top, right_bottom);
}

cc::Layer* ImageBitmapRenderingContext::CcLayer() const {
  return image_layer_bridge_->CcLayer();
}

bool ImageBitmapRenderingContext::IsPaintable() const {
  return !!image_layer_bridge_->GetImage();
}

void ImageBitmapRenderingContext::Trace(Visitor* visitor) const {
  visitor->Trace(image_layer_bridge_);
  ScriptWrappable::Trace(visitor);
  CanvasRenderingContext::Trace(visitor);
}

CanvasResourceProviderSharedImage*
ImageBitmapRenderingContext::GetOrCreateResourceProviderForOffscreenCanvas() {
  CHECK(Host()->IsOffscreenCanvas());
  if (isContextLost() && !IsContextBeingRestored()) {
    return nullptr;
  }

  if (CanvasResourceProviderSharedImage* provider =
          resource_provider_for_offscreen_canvas_.get()) {
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

  std::unique_ptr<CanvasResourceProviderSharedImage> provider;
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

  resource_provider_for_offscreen_canvas_ = std::move(provider);
  Host()->UpdateMemoryUsage();

  if (resource_provider_for_offscreen_canvas_.get() &&
      resource_provider_for_offscreen_canvas_.get()->IsValid()) {
    // todo(crbug.com/1064363)  Add a separate UMA for Offscreen Canvas usage
    // and understand if the if (ResourceProvider() &&
    // ResourceProvider()->IsValid()) is really needed.
    base::UmaHistogramBoolean(
        "Blink.Canvas.ResourceProviderIsAccelerated",
        resource_provider_for_offscreen_canvas_.get()->IsAccelerated());
    base::UmaHistogramEnumeration(
        "Blink.Canvas.ResourceProviderType",
        resource_provider_for_offscreen_canvas_.get()->GetType());
    Host()->DidDraw();
  }
  return resource_provider_for_offscreen_canvas_.get();
}

bool ImageBitmapRenderingContext::PushFrame() {
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
  resource_provider_for_offscreen_canvas_->Canvas().drawImage(
      image->PaintImageForCurrentFrame(), 0, 0, SkSamplingOptions(),
      &paint_flags);
  scoped_refptr<CanvasResource> resource =
      resource_provider_for_offscreen_canvas_->ProduceCanvasResource(
          FlushReason::kOther);
  Host()->PushFrame(
      std::move(resource),
      SkIRect::MakeWH(image_layer_bridge_->GetImage()->Size().width(),
                      image_layer_bridge_->GetImage()->Size().height()));
  return true;
}

V8RenderingContext* ImageBitmapRenderingContext::AsV8RenderingContext() {
  return MakeGarbageCollected<V8RenderingContext>(this);
}

V8OffscreenRenderingContext*
ImageBitmapRenderingContext::AsV8OffscreenRenderingContext() {
  return MakeGarbageCollected<V8OffscreenRenderingContext>(this);
}

void ImageBitmapRenderingContext::transferFromImageBitmap(
    ImageBitmap* image_bitmap,
    ExceptionState& exception_state) {
  if (image_bitmap && image_bitmap->IsNeutered()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The input ImageBitmap has been detached");
    return;
  }

  if (image_bitmap && image_bitmap->WouldTaintOrigin()) {
    Host()->SetOriginTainted();
  }

  SetImage(image_bitmap);
}

ImageBitmap* ImageBitmapRenderingContext::TransferToImageBitmap(
    ScriptState*,
    ExceptionState&) {
  scoped_refptr<StaticBitmapImage> image = GetImageAndResetInternal();
  if (!image)
    return nullptr;

  image->Transfer();
  return MakeGarbageCollected<ImageBitmap>(std::move(image));
}

CanvasRenderingContext* ImageBitmapRenderingContext::Factory::Create(
    ExecutionContext*,
    CanvasRenderingContextHost* host,
    const CanvasContextCreationAttributesCore& attrs) {
  CanvasRenderingContext* rendering_context =
      MakeGarbageCollected<ImageBitmapRenderingContext>(host, attrs);
  DCHECK(rendering_context);
  return rendering_context;
}

}  // namespace blink
