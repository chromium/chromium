// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/imagebitmap/image_bitmap_rendering_context_base.h"

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
          attrs.alpha ? kNonOpaque : kOpaque)) {}

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
  Host()->DiscardResourceProvider();
}

void ImageBitmapRenderingContextBase::Stop() {
  image_layer_bridge_->Dispose();
}

void ImageBitmapRenderingContextBase::Dispose() {
  Stop();
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

void ImageBitmapRenderingContextBase::SetFilterQuality(
    cc::PaintFlags::FilterQuality filter_quality) {
  image_layer_bridge_->SetFilterQuality(filter_quality);
}

cc::Layer* ImageBitmapRenderingContextBase::CcLayer() const {
  return image_layer_bridge_->CcLayer();
}

bool ImageBitmapRenderingContextBase::IsPaintable() const {
  return !!image_layer_bridge_->GetImage();
}

void ImageBitmapRenderingContextBase::Trace(Visitor* visitor) const {
  visitor->Trace(image_layer_bridge_);
  CanvasRenderingContext::Trace(visitor);
}

bool ImageBitmapRenderingContextBase::CanCreateCanvas2dResourceProvider()
    const {
  DCHECK(Host());
  DCHECK(Host()->IsOffscreenCanvas());
  return !!static_cast<OffscreenCanvas*>(Host())->GetOrCreateResourceProvider();
}

bool ImageBitmapRenderingContextBase::PushFrame() {
  DCHECK(Host());
  DCHECK(Host()->IsOffscreenCanvas());
  if (!CanCreateCanvas2dResourceProvider())
    return false;

  scoped_refptr<StaticBitmapImage> image = image_layer_bridge_->GetImage();
  if (!image) {
    return false;
  }
  cc::PaintFlags paint_flags;
  paint_flags.setBlendMode(SkBlendMode::kSrc);
  Host()->ResourceProvider()->Canvas().drawImage(
      image->PaintImageForCurrentFrame(), 0, 0, SkSamplingOptions(),
      &paint_flags);
  scoped_refptr<CanvasResource> resource =
      Host()->ResourceProvider()->ProduceCanvasResource(
          FlushReason::kNon2DCanvas);
  Host()->PushFrame(
      std::move(resource),
      SkIRect::MakeWH(image_layer_bridge_->GetImage()->Size().width(),
                      image_layer_bridge_->GetImage()->Size().height()));
  return true;
}

bool ImageBitmapRenderingContextBase::IsOriginTopLeft() const {
  if (Host()->IsOffscreenCanvas())
    return false;
  return Host()->IsAccelerated();
}

}  // namespace blink
