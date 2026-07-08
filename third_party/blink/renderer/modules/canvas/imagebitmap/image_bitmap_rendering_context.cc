// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/imagebitmap/image_bitmap_rendering_context.h"

#include <utility>

#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "cc/layers/texture_layer.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_flags.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_graphics_context_3d_provider.h"
#include "third_party/blink/public/platform/web_graphics_shared_image_interface_provider.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_htmlcanvaselement_offscreencanvas.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_offscreen_rendering_context.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_rendering_context.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/graphics/canvas_2d_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/canvas_non_2d_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {

// static
scoped_refptr<StaticBitmapImage> ImageBitmapRenderingContext::MakeAccelerated(
    const scoped_refptr<StaticBitmapImage>& source,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper>
        context_provider_wrapper) {
  bool can_use_source_directly = source->IsTextureBacked();
#if BUILDFLAG(IS_MAC)
  //  On MacOS, if |source| doesn't have SCANOUT usage, it is worth copying it
  //  to a new buffer with the SCANOUT even when |source| is
  //  already on the GPU, to keep using delegated compositing.
  can_use_source_directly =
      can_use_source_directly &&
      source->GetSharedImage()->usage().Has(gpu::SHARED_IMAGE_USAGE_SCANOUT);
#endif

  if (can_use_source_directly) {
    return source;
  }

#if BUILDFLAG(IS_LINUX)
  // TODO(b/330865436): On Linux, CanvasResourceProvider doesn't always check
  // for SCANOUT support correctly on X11 and it's never supported in
  // practice. Therefore, don't include it until this flow is reworked.
  constexpr gpu::SharedImageUsageSet kSharedImageUsageFlags =
      gpu::SHARED_IMAGE_USAGE_DISPLAY_READ;
#else
  // Always request gpu::SHARED_IMAGE_USAGE_SCANOUT when using gpu compositing,
  // if possible. This is safe because the prerequisite capabilities are checked
  // downstream in CanvasResourceProvider::CreateSharedImageProvider.
  constexpr gpu::SharedImageUsageSet kSharedImageUsageFlags =
      gpu::SHARED_IMAGE_USAGE_DISPLAY_READ | gpu::SHARED_IMAGE_USAGE_SCANOUT;
#endif  // BUILDFLAG(IS_LINUX)
  auto provider = CanvasNon2DResourceProvider::Create(
      source->Size(), source->GetSharedImageFormat(), source->GetAlphaType(),
      source->GetColorSpace(), source->GetHdrMetadata(),
      context_provider_wrapper, kSharedImageUsageFlags);
  if (!provider) {
    return nullptr;
  }

  const auto paint_image = source->PaintImageForCurrentFrame();
  return provider->DoExternalOverdrawAndSnapshot(
      [paint_image](cc::PaintCanvas& canvas) {
        cc::PaintFlags paint;
        paint.setBlendMode(SkBlendMode::kSrc);
        canvas.drawImage(paint_image, 0, 0, SkSamplingOptions(), &paint);
      },
      ImageOrientationEnum::kDefault);
}

ImageBitmapRenderingContext::ImageBitmapRenderingContext(
    CanvasRenderingContextHost* host,
    const CanvasContextCreationAttributesCore& attrs)
    : CanvasRenderingContext(host, attrs, CanvasRenderingAPI::kBitmaprenderer),
      is_opaque_(!attrs.alpha) {
  layer_ = cc::TextureLayer::Create(this);
  layer_->SetIsDrawable(true);
  layer_->SetHitTestable(true);
  if (is_opaque_) {
    layer_->SetContentsOpaque(true);
    layer_->SetBlendBackgroundColor(false);
  }
  host->InitializeLayerWithCSSProperties(layer_.get());
}

ImageBitmapRenderingContext::~ImageBitmapRenderingContext() = default;

V8UnionHTMLCanvasElementOrOffscreenCanvas::Ret
ImageBitmapRenderingContext::getHTMLOrOffscreenCanvas(
    ScriptState* script_state) const {
  if (Host()->IsOffscreenCanvas()) {
    return V8UnionHTMLCanvasElementOrOffscreenCanvas::Ret(
        script_state, static_cast<OffscreenCanvas*>(Host()));
  }
  return V8UnionHTMLCanvasElementOrOffscreenCanvas::Ret(
      script_state, static_cast<HTMLCanvasElement*>(Host()));
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
  base::ByteSize result = image_->EstimatedSizeInBytes();
  if (resource_provider_for_offscreen_canvas_) {
    result += resource_provider_for_offscreen_canvas_->EstimatedSizeInBytes();
  }
  return result;
}

void ImageBitmapRenderingContext::Stop() {
  image_ = nullptr;
  disposed_ = true;
  if (layer_) {
    layer_->ClearClient();
    layer_ = nullptr;
  }
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

void ImageBitmapRenderingContext::SetImageInternal(
    scoped_refptr<StaticBitmapImage> image) {
  if (disposed_) {
    return;
  }
  // There could be the case that the current PaintImage is null, meaning
  // that something went wrong during the creation of the image and we should
  // not try and setImage with it
  if (image && !image->PaintImageForCurrentFrame()) {
    return;
  }

  image_ = std::move(image);
  if (image_) {
    const bool image_is_opaque = image_->IsOpaque();
    if (is_opaque_) {
      // If we in opaque mode but image might have transparency we need to
      // ensure its opacity is not used.
      layer_->SetForceTextureToOpaque(!image_is_opaque);
    } else {
      layer_->SetContentsOpaque(image_is_opaque);
      layer_->SetBlendBackgroundColor(!image_is_opaque);
    }
  }
  has_presented_since_last_set_image_ = false;
}

void ImageBitmapRenderingContext::ResetInternalBitmapToBlackTransparent(
    int width,
    int height) {
  SkBitmap black_bitmap;
  if (black_bitmap.tryAllocN32Pixels(width, height)) {
    black_bitmap.eraseARGB(0, 0, 0, 0);
    auto image = SkImages::RasterFromBitmap(black_bitmap);
    if (image) {
      SetImageInternal(UnacceleratedStaticBitmapImage::Create(image));
    }
  }
}

void ImageBitmapRenderingContext::SetImage(ImageBitmap* image_bitmap) {
  DCHECK(!image_bitmap || !image_bitmap->IsNeutered());

  // According to the standard TransferFromImageBitmap(null) has to reset the
  // internal bitmap and create a black transparent one.
  if (image_bitmap) {
    SetImageInternal(image_bitmap->BitmapImage());
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
  return image_;
}

scoped_refptr<StaticBitmapImage>
ImageBitmapRenderingContext::GetImageAndResetInternal() {
  if (!image_) {
    return nullptr;
  }
  scoped_refptr<StaticBitmapImage> copy_image = image_;

  ResetInternalBitmapToBlackTransparent(copy_image->width(),
                                        copy_image->height());

  return copy_image;
}

void ImageBitmapRenderingContext::SetUV(const gfx::PointF& left_top,
                                        const gfx::PointF& right_bottom) {
  if (disposed_) {
    return;
  }
  layer_->SetUV(left_top, right_bottom);
}

cc::Layer* ImageBitmapRenderingContext::CcLayer() const {
  return layer_ ? layer_.get() : nullptr;
}

bool ImageBitmapRenderingContext::PrepareTransferableResource(
    viz::TransferableResource* out_resource,
    viz::ReleaseCallback* out_release_callback) {
  if (disposed_) {
    return false;
  }

  if (!image_) {
    return false;
  }

  if (has_presented_since_last_set_image_) {
    return false;
  }

  has_presented_since_last_set_image_ = true;

  const bool gpu_compositing = SharedGpuContext::IsGpuCompositingEnabled();

  if (gpu_compositing) {
    scoped_refptr<StaticBitmapImage> image_for_compositor =
        ImageBitmapRenderingContext::MakeAccelerated(
            image_, SharedGpuContext::ContextProviderWrapper());
    if (!image_for_compositor || !image_for_compositor->ContextProvider()) {
      return false;
    }

    auto shared_image = image_for_compositor->GetSharedImage();

    if (!shared_image) {
      return false;
    }

    viz::TransferableResource::MetadataOverride overrides = {
        .color_space = gfx::ColorSpace(),
        .alpha_type = kPremul_SkAlphaType,
    };

    *out_resource = viz::TransferableResource::Make(
        shared_image,
        viz::TransferableResource::ResourceSource::kImageLayerBridge,
        image_for_compositor->GetSyncToken(), overrides);

    auto func = blink::BindOnce(
        &ImageBitmapRenderingContext::ResourceReleasedGpu,
        WrapWeakPersistent(this), std::move(image_for_compositor));
    *out_release_callback = std::move(func);
  } else {
    image_ = image_->MakeUnaccelerated();
    if (!image_) {
      return false;
    }

    sk_sp<SkImage> sk_image =
        image_->PaintImageForCurrentFrame().GetSwSkImage();
    if (!sk_image) {
      return false;
    }

    const gfx::Size size(image_->width(), image_->height());

    SoftwareResource resource =
        CreateOrRecycleSoftwareResource(size, image_->GetColorSpace());
    if (!resource.shared_image) {
      return false;
    }

    SkImageInfo dst_info =
        SkImageInfo::Make(size.width(), size.height(),
                          ToClosestSkColorType(resource.shared_image->format()),
                          kPremul_SkAlphaType, sk_image->refColorSpace());

    // Copy from SkImage into SharedMemory owned by |resource|.
    auto dst_mapping = resource.shared_image->Map();
    if (!sk_image->readPixels(/*context=*/nullptr,
                              dst_mapping->GetSkPixmapForPlane(0, dst_info), 0,
                              0)) {
      return false;
    }

    *out_resource = viz::TransferableResource::Make(
        resource.shared_image,
        viz::TransferableResource::ResourceSource::kImageLayerBridge,
        resource.sync_token);
    auto func =
        blink::BindOnce(&ImageBitmapRenderingContext::ResourceReleasedSoftware,
                        WrapWeakPersistent(this), std::move(resource));
    *out_release_callback = std::move(func);
  }

  return true;
}

void ImageBitmapRenderingContext::ResourceReleasedGpu(
    scoped_refptr<StaticBitmapImage> image,
    const gpu::SyncToken& token,
    bool lost_resource) {
  if (image && image->IsValid()) {
    DCHECK(image->IsTextureBacked());
    if (token.HasData()) {
      image->UpdateSyncToken(token);
    }
  }
}

ImageBitmapRenderingContext::SoftwareResource::SoftwareResource() = default;
ImageBitmapRenderingContext::SoftwareResource::SoftwareResource(
    SoftwareResource&& other) = default;
ImageBitmapRenderingContext::SoftwareResource&
ImageBitmapRenderingContext::SoftwareResource::operator=(
    SoftwareResource&& other) = default;

ImageBitmapRenderingContext::SoftwareResource
ImageBitmapRenderingContext::CreateOrRecycleSoftwareResource(
    const gfx::Size& size,
    const gfx::ColorSpace& color_space) {
  // Must call SharedImageInterfaceProvider() first so all base::WeakPtr
  // restored in |resource.sii_provider| is updated.
  auto* sii_provider = SharedGpuContext::SharedImageInterfaceProvider();
  DCHECK(sii_provider);
  auto it = std::remove_if(
      recycled_software_resources_.begin(), recycled_software_resources_.end(),
      [&size, &color_space](const SoftwareResource& resource) {
        return resource.shared_image->size() != size ||
               resource.shared_image->color_space() != color_space ||
               !resource.sii_provider;
      });

  recycled_software_resources_.Shrink(
      static_cast<wtf_size_t>(it - recycled_software_resources_.begin()));

  if (!recycled_software_resources_.empty()) {
    SoftwareResource resource = std::move(recycled_software_resources_.back());
    recycled_software_resources_.pop_back();
    return resource;
  }

  // There are no resources to recycle so allocate a new one.
  SoftwareResource resource;
  auto* shared_image_interface = sii_provider->SharedImageInterface();
  if (!shared_image_interface) {
    return resource;
  }
  resource.shared_image =
      shared_image_interface->CreateSharedImageForSoftwareCompositor(
          {viz::SinglePlaneFormat::kBGRA_8888, size, color_space,
           gpu::SHARED_IMAGE_USAGE_CPU_WRITE_ONLY, "ImageLayerBridgeBitmap"});

  resource.sii_provider = sii_provider->GetWeakPtr();
  resource.sync_token = resource.shared_image->creation_sync_token();
  shared_image_interface->VerifySyncToken(resource.sync_token);

  return resource;
}

void ImageBitmapRenderingContext::ResourceReleasedSoftware(
    SoftwareResource resource,
    const gpu::SyncToken& sync_token,
    bool lost_resource) {
  if (!disposed_ && !lost_resource) {
    recycled_software_resources_.push_back(std::move(resource));
  }
}

bool ImageBitmapRenderingContext::IsPaintable() const {
  return !!image_;
}

void ImageBitmapRenderingContext::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  CanvasRenderingContext::Trace(visitor);
}

scoped_refptr<CanvasResource>
ImageBitmapRenderingContext::GetResourceForPushFrame(
    bool& should_call_push_frame) {
  should_call_push_frame = false;
  CHECK(Host()->IsOffscreenCanvas());
  if (isContextLost() && !IsContextBeingRestored()) {
    return nullptr;
  }

  scoped_refptr<StaticBitmapImage> image = image_;
  if (!image) {
    return nullptr;
  }

  // If the size of the cached provider doesn't match that of the current image
  // (e.g. because it was created for a previous image of a different size),
  // drop it to ensure that it is recreated with the correct size below.
  if (resource_provider_for_offscreen_canvas_ &&
      resource_provider_for_offscreen_canvas_->Size() != image->Size()) {
    resource_provider_for_offscreen_canvas_.reset();
  }

  if (resource_provider_for_offscreen_canvas_) {
    if (!resource_provider_for_offscreen_canvas_->IsValid()) {
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
  } else {
    if (!Host()->IsValidImageSize() && !Host()->Size().IsEmpty()) {
      LoseContext(CanvasRenderingContext::kInvalidCanvasSize);
      return nullptr;
    }

    const bool is_gpu_compositing_enabled =
        SharedGpuContext::IsGpuCompositingEnabled();

    // TODO(https://crbug.com/40206688): These values should reflect the
    // ImageBitmap.
    const SkAlphaType alpha_type = kPremul_SkAlphaType;
    const viz::SharedImageFormat format = GetN32FormatForCanvas();
    const gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB();
    const gfx::HDRMetadata hdr_metadata;
    if (is_gpu_compositing_enabled) {
      resource_provider_for_offscreen_canvas_ =
          CanvasNon2DResourceProvider::Create(
              image->Size(), format, alpha_type, color_space, hdr_metadata,
              SharedGpuContext::ContextProviderWrapper(),
              gpu::SHARED_IMAGE_USAGE_DISPLAY_READ, Host());
    } else if (static_cast<OffscreenCanvas*>(Host())->HasPlaceholderCanvas()) {
      resource_provider_for_offscreen_canvas_ =
          CanvasNon2DResourceProvider::CreateForSoftwareCompositor(
              image->Size(), format, alpha_type, color_space, hdr_metadata,
              SharedGpuContext::SharedImageInterfaceProvider(), Host());
    }

    Host()->UpdateMemoryUsage();

    if (resource_provider_for_offscreen_canvas_.get()) {
      base::UmaHistogramBoolean("Blink.Canvas.ResourceProviderIsAccelerated",
                                is_gpu_compositing_enabled);
      base::UmaHistogramEnumeration("Blink.Canvas.ResourceProviderType",
                                    CanvasResourceProviderType::kSharedImage);
      Host()->DidDraw();
    }
  }

  if (!resource_provider_for_offscreen_canvas_) {
    return nullptr;
  }

  cc::PaintFlags paint_flags;
  paint_flags.setBlendMode(SkBlendMode::kSrc);
  scoped_refptr<CanvasResource> resource =
      resource_provider_for_offscreen_canvas_
          ->DoExternalOverdrawAndProduceResource([&](cc::PaintCanvas& canvas) {
            canvas.drawImage(image->PaintImageForCurrentFrame(), 0, 0,
                             SkSamplingOptions(), &paint_flags);
          });
  should_call_push_frame = true;
  return resource;
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
