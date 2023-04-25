// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/image_layer_bridge.h"

#include "base/memory/read_only_shared_memory_region.h"
#include "cc/layers/texture_layer.h"
#include "cc/resources/cross_thread_shared_bitmap.h"
#include "components/viz/common/resources/bitmap_allocation.h"
#include "components/viz/common/resources/resource_format_utils.h"
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
#include "third_party/blink/renderer/platform/graphics/image_orientation.h"
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
#if BUILDFLAG(IS_MAC)
  // On MacOS, if |source| is not an overlay candidate, it is worth copying it
  // to a new buffer that is an overlay candidate, even when |source| is
  // already on the GPU.
  if (source->IsOverlayCandidate()) {
#else
  if (source->IsTextureBacked()) {
#endif
    return source;
  }

  auto paint_image = source->PaintImageForCurrentFrame();
  auto image_info = paint_image.GetSkImageInfo().makeWH(
      source->Size().width(), source->Size().height());
  // Always request gpu::SHARED_IMAGE_USAGE_SCANOUT when using gpu compositing,
  // if possible. This is safe because the prerequisite capabilities are checked
  // downstream in CanvasResourceProvider::CreateSharedImageProvider.
  auto provider = CanvasResourceProvider::CreateSharedImageProvider(
      image_info, cc::PaintFlags::FilterQuality::kLow,
      CanvasResourceProvider::ShouldInitialize::kNo, context_provider_wrapper,
      RasterMode::kGPU, source->IsOriginTopLeft(),
      gpu::SHARED_IMAGE_USAGE_DISPLAY_READ | gpu::SHARED_IMAGE_USAGE_SCANOUT);
  if (!provider || !provider->IsAccelerated())
    return nullptr;

  cc::PaintFlags paint;
  paint.setBlendMode(SkBlendMode::kSrc);
  provider->Canvas()->drawImage(paint_image, 0, 0, SkSamplingOptions(), &paint);
  return provider->Snapshot(CanvasResourceProvider::FlushReason::kNon2DCanvas);
}

}  // namespace

ImageLayerBridge::ImageLayerBridge(OpacityMode opacity_mode)
    : opacity_mode_(opacity_mode) {
  layer_ = cc::TextureLayer::CreateForMailbox(this);
  layer_->SetIsDrawable(true);
  layer_->SetHitTestable(true);
  layer_->SetNearestNeighbor(filter_quality_ ==
                             cc::PaintFlags::FilterQuality::kNone);
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
      image_->PaintImageForCurrentFrame().FlushPendingSkiaOps();
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
    cc::SharedBitmapIdRegistrar* bitmap_registrar,
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

  if (!gpu_compositing) {
    // Readback if needed and retain the readback in image_ to prevent future
    // readbacks.
    // Note: Switching to unaccelerated may change the value of
    // image_->IsOriginTopLeft(), so it is important to make the switch before
    // calling IsOriginTopLeft().
    image_ = image_->MakeUnaccelerated();
    if (!image_) {
      return false;
    }
  }

  layer_->SetFlipped(!image_->IsOriginTopLeft());

  if (gpu_compositing) {
    scoped_refptr<StaticBitmapImage> image_for_compositor =
        MakeAccelerated(image_, SharedGpuContext::ContextProviderWrapper());
    if (!image_for_compositor || !image_for_compositor->ContextProvider())
      return false;

    const gfx::Size size(image_for_compositor->width(),
                         image_for_compositor->height());
    auto mailbox_holder = image_for_compositor->GetMailboxHolder();

    if (mailbox_holder.mailbox.IsZero()) {
      // This can happen, for example, if an ImageBitmap is produced from a
      // WebGL-rendered OffscreenCanvas and then the WebGL context is forcibly
      // lost. This seems to be the only reliable point where this can be
      // detected.
      return false;
    }

    auto* sii = image_for_compositor->ContextProvider()->SharedImageInterface();
    bool is_overlay_candidate = sii->UsageForMailbox(mailbox_holder.mailbox) &
                                gpu::SHARED_IMAGE_USAGE_SCANOUT;

    SkColorType color_type = image_for_compositor->GetSkColorInfo().colorType();
    *out_resource = viz::TransferableResource::MakeGpu(
        mailbox_holder.mailbox, mailbox_holder.texture_target,
        mailbox_holder.sync_token, size,
        viz::SharedImageFormat::SinglePlane(
            viz::SkColorTypeToResourceFormat(color_type)),
        is_overlay_candidate);

    // If the transferred ImageBitmap contained in this ImageLayerBridge was
    // originated in a WebGPU context, we need to set the layer to be flipped.
    // Canvas2D and WebGL contexts handle this aspect internally, whereas
    // WebGPU does not.
    if (sii->UsageForMailbox(mailbox_holder.mailbox) &
        gpu::SHARED_IMAGE_USAGE_WEBGPU_SWAP_CHAIN_TEXTURE) {
      // Using image_for_compositor->IsOriginTopLeft() and remove
      // implementation of sii->UsageForMailbox()?
      layer_->SetFlipped(false);
    }

    auto func = WTF::BindOnce(&ImageLayerBridge::ResourceReleasedGpu,
                              WrapWeakPersistent(this),
                              std::move(image_for_compositor));
    *out_release_callback = std::move(func);
  } else {
    sk_sp<SkImage> sk_image =
        image_->PaintImageForCurrentFrame().GetSwSkImage();
    if (!sk_image)
      return false;

    const gfx::Size size(image_->width(), image_->height());

    // Always convert to N32 format.  This is a constraint of the software
    // compositor.
    constexpr SkColorType dst_color_type = kN32_SkColorType;
    // TODO(vasilyt): this used to be
    // viz::SkColorTypeToResourceFormat(dst_color_type), but on some platforms
    // (including Mac), kN32_SkColorType is BGRA8888 which is disallowed as a
    // bitmap format. Deeper refactorings are needed to fix this properly; in
    // the meantime, force the use of viz::SinglePlaneFormat::kRGBA_8888 as the
    // resource format. This addresses assertion failures when serializing these
    // bitmaps to the GPU process.
    viz::SharedImageFormat format = viz::SinglePlaneFormat::kRGBA_8888;
    RegisteredBitmap registered =
        CreateOrRecycleBitmap(size, format, bitmap_registrar);

    SkImageInfo dst_info =
        SkImageInfo::Make(size.width(), size.height(), dst_color_type,
                          kPremul_SkAlphaType, sk_image->refColorSpace());
    void* pixels = registered.bitmap->memory();

    // Copy from SkImage into SharedMemory owned by |registered|.
    if (!sk_image->readPixels(dst_info, pixels, dst_info.minRowBytes(), 0, 0))
      return false;

    *out_resource = viz::TransferableResource::MakeSoftware(
        registered.bitmap->id(), size, format);
    out_resource->color_space = sk_image->colorSpace()
                                    ? gfx::ColorSpace(*sk_image->colorSpace())
                                    : gfx::ColorSpace::CreateSRGB();
    auto func = WTF::BindOnce(&ImageLayerBridge::ResourceReleasedSoftware,
                              WrapWeakPersistent(this), std::move(registered));
    *out_release_callback = std::move(func);
  }

  return true;
}

ImageLayerBridge::RegisteredBitmap ImageLayerBridge::CreateOrRecycleBitmap(
    const gfx::Size& size,
    viz::SharedImageFormat format,
    cc::SharedBitmapIdRegistrar* bitmap_registrar) {
  auto* it = std::remove_if(
      recycled_bitmaps_.begin(), recycled_bitmaps_.end(),
      [&size, &format](const RegisteredBitmap& registered) {
        unsigned src_bytes_per_pixel =
            viz::BitsPerPixel(registered.bitmap->format().resource_format()) /
            8;
        unsigned target_bytes_per_pixel =
            viz::BitsPerPixel(format.resource_format()) / 8;
        return (registered.bitmap->size().GetArea() * src_bytes_per_pixel !=
                size.GetArea() * target_bytes_per_pixel);
      });
  recycled_bitmaps_.Shrink(
      static_cast<wtf_size_t>(it - recycled_bitmaps_.begin()));

  if (!recycled_bitmaps_.empty()) {
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
