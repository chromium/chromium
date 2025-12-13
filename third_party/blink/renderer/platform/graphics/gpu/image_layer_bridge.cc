// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/image_layer_bridge.h"

#include "base/memory/read_only_shared_memory_region.h"
#include "cc/layers/texture_layer.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_graphics_context_3d_provider.h"
#include "third_party/blink/public/platform/web_graphics_shared_image_interface_provider.h"
#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/color_behavior.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/image_orientation.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "ui/gfx/geometry/size.h"

namespace blink {
namespace {

scoped_refptr<StaticBitmapImage> MakeAccelerated(
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

  const auto paint_image = source->PaintImageForCurrentFrame();
  const auto image_info = paint_image.GetSkImageInfo();
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
  auto provider = CanvasResourceProvider::CreateSharedImageProvider(
      source->Size(),
      viz::SkColorTypeToSinglePlaneSharedImageFormat(image_info.colorType()),
      image_info.alphaType(),
      SkColorSpaceToGfxColorSpace(image_info.refColorSpace()),
      CanvasResourceProvider::ShouldInitialize::kNo, context_provider_wrapper,
      RasterMode::kGPU, kSharedImageUsageFlags);
  if (!provider || !provider->IsAccelerated())
    return nullptr;

  cc::PaintFlags paint;
  paint.setBlendMode(SkBlendMode::kSrc);
  provider->Canvas().drawImage(paint_image, 0, 0, SkSamplingOptions(), &paint);
  return provider->Snapshot();
}

}  // namespace

ImageLayerBridge::ImageLayerBridge(OpacityMode opacity_mode)
    : is_opaque_(opacity_mode == kOpaque) {
  layer_ = cc::TextureLayer::Create(this);
  layer_->SetIsDrawable(true);
  layer_->SetHitTestable(true);
  if (is_opaque_) {
    layer_->SetContentsOpaque(true);
    layer_->SetBlendBackgroundColor(false);
  }
}

ImageLayerBridge::~ImageLayerBridge() {
  if (!disposed_)
    Dispose();
}

void ImageLayerBridge::SetImage(scoped_refptr<StaticBitmapImage> image) {
  if (disposed_)
    return;
  // There could be the case that the current PaintImage is null, meaning
  // that something went wrong during the creation of the image and we should
  // not try and setImage with it
  if (image && !image->PaintImageForCurrentFrame())
    return;

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

void ImageLayerBridge::SetUV(const gfx::PointF& left_top,
                             const gfx::PointF& right_bottom) {
  if (disposed_)
    return;

  layer_->SetUV(left_top, right_bottom);
}

void ImageLayerBridge::Dispose() {
  if (layer_) {
    layer_->ClearClient();
    layer_ = nullptr;
  }
  image_ = nullptr;
  disposed_ = true;
}

bool ImageLayerBridge::PrepareTransferableResource(
    viz::TransferableResource* out_resource,
    viz::ReleaseCallback* out_release_callback) {
  if (disposed_)
    return false;

  if (!image_)
    return false;

  if (has_presented_since_last_set_image_)
    return false;

  has_presented_since_last_set_image_ = true;

  const bool gpu_compositing = SharedGpuContext::IsGpuCompositingEnabled();

  if (gpu_compositing) {
    scoped_refptr<StaticBitmapImage> image_for_compositor =
        MakeAccelerated(image_, SharedGpuContext::ContextProviderWrapper());
    if (!image_for_compositor || !image_for_compositor->ContextProvider())
      return false;

    auto shared_image = image_for_compositor->GetSharedImage();

    if (!shared_image) {
      // This can happen, for example, if an ImageBitmap is produced from a
      // WebGL-rendered OffscreenCanvas and then the WebGL context is forcibly
      // lost. This seems to be the only reliable point where this can be
      // detected.
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

    auto func = blink::BindOnce(&ImageLayerBridge::ResourceReleasedGpu,
                                WrapWeakPersistent(this),
                                std::move(image_for_compositor));
    *out_release_callback = std::move(func);
  } else {
    image_ = image_->MakeUnaccelerated();
    if (!image_) {
      return false;
    }

    sk_sp<SkImage> sk_image =
        image_->PaintImageForCurrentFrame().GetSwSkImage();
    if (!sk_image)
      return false;

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
    auto func = BindOnce(&ImageLayerBridge::ResourceReleasedSoftware,
                         WrapWeakPersistent(this), std::move(resource));
    *out_release_callback = std::move(func);
  }

  return true;
}

ImageLayerBridge::SoftwareResource
ImageLayerBridge::CreateOrRecycleSoftwareResource(
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
  resource.sync_token = shared_image_interface->GenVerifiedSyncToken();

  return resource;
}

void ImageLayerBridge::ResourceReleasedGpu(
    scoped_refptr<StaticBitmapImage> image,
    const gpu::SyncToken& token,
    bool lost_resource) {
  if (image && image->IsValid()) {
    DCHECK(image->IsTextureBacked());
    if (token.HasData() && image->ContextProvider() &&
        image->ContextProvider()->InterfaceBase()) {
      image->ContextProvider()->InterfaceBase()->WaitSyncTokenCHROMIUM(
          token.GetConstData());
    }
  }
  // let 'image' go out of scope to release gpu resources.
}

void ImageLayerBridge::ResourceReleasedSoftware(
    SoftwareResource resource,
    const gpu::SyncToken& sync_token,
    bool lost_resource) {
  if (!disposed_ && !lost_resource) {
    recycled_software_resources_.push_back(std::move(resource));
  }
}

cc::Layer* ImageLayerBridge::CcLayer() const {
  return layer_.get();
}

ImageLayerBridge::SoftwareResource::SoftwareResource() = default;
ImageLayerBridge::SoftwareResource::SoftwareResource(SoftwareResource&& other) =
    default;
ImageLayerBridge::SoftwareResource&
ImageLayerBridge::SoftwareResource::operator=(SoftwareResource&& other) =
    default;

}  // namespace blink
