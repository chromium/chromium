// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/image_layer_bridge.h"

#include "base/memory/read_only_shared_memory_region.h"
#include "cc/layers/texture_layer.h"
#include "cc/resources/cross_thread_shared_bitmap.h"
#include "components/viz/common/resources/bitmap_allocation.h"
#include "components/viz/common/resources/shared_bitmap.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/canvas_color_params.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/color_behavior.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "ui/gfx/geometry/size.h"

namespace blink {
namespace {

scoped_refptr<StaticBitmapImage> MakeAccelerated(
    const scoped_refptr<StaticBitmapImage>& source,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper>
        context_provider_wrapper) {
  if (source->IsTextureBacked())
    return source;

  auto paint_image = source->PaintImageForCurrentFrame();
  auto provider = CanvasResourceProvider::CreateSharedImageProvider(
      source->Size(), kLow_SkFilterQuality,
      CanvasColorParams(paint_image.GetSkImageInfo()),
      CanvasResourceProvider::ShouldInitialize::kNo, context_provider_wrapper,
      RasterMode::kGPU, source->IsOriginTopLeft(),
      gpu::SHARED_IMAGE_USAGE_DISPLAY);
  if (!provider || !provider->IsAccelerated())
    return nullptr;

  provider->Canvas()->drawImage(paint_image, 0, 0, nullptr);
  return provider->Snapshot();
}

}  // namespace

ImageLayerBridge::ImageLayerBridge(OpacityMode opacity_mode)
    : opacity_mode_(opacity_mode) {
  layer_ = cc::TextureLayer::CreateForMailbox(this);
  layer_->SetIsDrawable(true);
  layer_->SetHitTestable(true);
  layer_->SetNearestNeighbor(filter_quality_ == kNone_SkFilterQuality);
  if (opacity_mode_ == kOpaque) {
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
    if (opacity_mode_ == kNonOpaque) {
      layer_->SetContentsOpaque(image_->CurrentFrameKnownToBeOpaque());
      layer_->SetBlendBackgroundColor(!image_->CurrentFrameKnownToBeOpaque());
    }
    if (opacity_mode_ == kOpaque) {
      // If we in opaque mode but image might have transparency we need to
      // ensure its opacity is not used.
      layer_->SetForceTextureToOpaque(!image_->CurrentFrameKnownToBeOpaque());
    }
    if (!has_presented_since_last_set_image_ && image_->IsTextureBacked()) {
      // If the layer bridge is not presenting, the GrContext may not be getting
      // flushed regularly.  The flush is normally triggered inside the
      // m_image->EnsureMailbox() call of
      // ImageLayerBridge::PrepareTransferableResource. To prevent a potential
      // memory leak we must flush the GrContext here.
      image_->PaintImageForCurrentFrame().GetSkImage()->getBackendTexture(
          true);  // GrContext flush.
    }
  }
  has_presented_since_last_set_image_ = false;
}

void ImageLayerBridge::SetUV(const FloatPoint& left_top,
                             const FloatPoint& right_bottom) {
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
    cc::SharedBitmapIdRegistrar* bitmap_registrar,
    viz::TransferableResource* out_resource,
    std::unique_ptr<viz::SingleReleaseCallback>* out_release_callback) {
  if (disposed_)
    return false;

  if (!image_)
    return false;

  if (has_presented_since_last_set_image_)
    return false;

  has_presented_since_last_set_image_ = true;

  bool gpu_compositing = SharedGpuContext::IsGpuCompositingEnabled();
  bool gpu_image = image_->IsTextureBacked();

  // Expect software images for software compositing.
  if (!gpu_compositing && gpu_image)
    return false;

  // If the texture comes from a software image then it does not need to be
  // flipped.
  layer_->SetFlipped(gpu_image);

  if (gpu_compositing) {
    scoped_refptr<StaticBitmapImage> image_for_compositor =
        MakeAccelerated(image_, SharedGpuContext::ContextProviderWrapper());
    if (!image_for_compositor || !image_for_compositor->ContextProvider())
      return false;

    const gfx::Size size(image_for_compositor->width(),
                         image_for_compositor->height());
    uint32_t filter =
        filter_quality_ == kNone_SkFilterQuality ? GL_NEAREST : GL_LINEAR;
    auto mailbox_holder = image_for_compositor->GetMailboxHolder();
    auto* sii = image_for_compositor->ContextProvider()->SharedImageInterface();
    bool is_overlay_candidate = sii->UsageForMailbox(mailbox_holder.mailbox) &
                                gpu::SHARED_IMAGE_USAGE_SCANOUT;

    *out_resource = viz::TransferableResource::MakeGL(
        mailbox_holder.mailbox, filter, mailbox_holder.texture_target,
        mailbox_holder.sync_token, size, is_overlay_candidate);
    auto func =
        WTF::Bind(&ImageLayerBridge::ResourceReleasedGpu,
                  WrapWeakPersistent(this), std::move(image_for_compositor));
    *out_release_callback = viz::SingleReleaseCallback::Create(std::move(func));
  } else {
    // Readback if needed and retain the readback in image_ to prevent future
    // readbacks
    image_ = image_->MakeUnaccelerated();
    if (!image_)
      return false;

    sk_sp<SkImage> sk_image =
        image_->PaintImageForCurrentFrame().GetSwSkImage();
    if (!sk_image)
      return false;

    const gfx::Size size(image_->width(), image_->height());
    viz::ResourceFormat resource_format = viz::RGBA_8888;
    if (sk_image->colorType() == SkColorType::kRGBA_F16_SkColorType)
      resource_format = viz::RGBA_F16;
    RegisteredBitmap registered =
        CreateOrRecycleBitmap(size, resource_format, bitmap_registrar);

    SkImageInfo dst_info =
        SkImageInfo::Make(size.width(), size.height(), sk_image->colorType(),
                          kPremul_SkAlphaType, sk_image->refColorSpace());
    void* pixels = registered.bitmap->memory();

    // Copy from SkImage into SharedMemory owned by |registered|.
    if (!sk_image->readPixels(dst_info, pixels, dst_info.minRowBytes(), 0, 0))
      return false;

    *out_resource = viz::TransferableResource::MakeSoftware(
        registered.bitmap->id(), size, resource_format);
    if (RuntimeEnabledFeatures::CanvasColorManagementEnabled()) {
      out_resource->color_space = sk_image->colorSpace()
                                      ? gfx::ColorSpace(*sk_image->colorSpace())
                                      : gfx::ColorSpace::CreateSRGB();
    }
    auto func = WTF::Bind(&ImageLayerBridge::ResourceReleasedSoftware,
                          WrapWeakPersistent(this), std::move(registered));
    *out_release_callback = viz::SingleReleaseCallback::Create(std::move(func));
  }

  return true;
}

ImageLayerBridge::RegisteredBitmap ImageLayerBridge::CreateOrRecycleBitmap(
    const gfx::Size& size,
    viz::ResourceFormat format,
    cc::SharedBitmapIdRegistrar* bitmap_registrar) {
  auto* it = std::remove_if(
      recycled_bitmaps_.begin(), recycled_bitmaps_.end(),
      [&size, &format](const RegisteredBitmap& registered) {
        unsigned src_bytes_per_pixel =
            (registered.bitmap->format() == viz::RGBA_8888) ? 4 : 8;
        unsigned target_bytes_per_pixel = (format == viz::RGBA_8888) ? 4 : 8;
        return (registered.bitmap->size().GetArea() * src_bytes_per_pixel !=
                size.GetArea() * target_bytes_per_pixel);
      });
  recycled_bitmaps_.Shrink(it - recycled_bitmaps_.begin());

  if (!recycled_bitmaps_.IsEmpty()) {
    RegisteredBitmap registered = std::move(recycled_bitmaps_.back());
    recycled_bitmaps_.pop_back();
    DCHECK(registered.bitmap->size() == size);
    return registered;
  }

  // There are no bitmaps to recycle so allocate a new one.
  viz::SharedBitmapId id = viz::SharedBitmap::GenerateId();
  base::MappedReadOnlyRegion shm =
      viz::bitmap_allocation::AllocateSharedBitmap(size, format);

  RegisteredBitmap registered;
  registered.bitmap = base::MakeRefCounted<cc::CrossThreadSharedBitmap>(
      id, std::move(shm), size, format);
  registered.registration =
      bitmap_registrar->RegisterSharedBitmapId(id, registered.bitmap);

  return registered;
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
    RegisteredBitmap registered,
    const gpu::SyncToken& sync_token,
    bool lost_resource) {
  DCHECK(!sync_token.HasData());  // No sync tokens for software resources.
  if (!disposed_ && !lost_resource)
    recycled_bitmaps_.push_back(std::move(registered));
}

cc::Layer* ImageLayerBridge::CcLayer() const {
  return layer_.get();
}

ImageLayerBridge::RegisteredBitmap::RegisteredBitmap() = default;
ImageLayerBridge::RegisteredBitmap::RegisteredBitmap(RegisteredBitmap&& other) =
    default;
ImageLayerBridge::RegisteredBitmap& ImageLayerBridge::RegisteredBitmap::
operator=(RegisteredBitmap&& other) = default;

}  // namespace blink
