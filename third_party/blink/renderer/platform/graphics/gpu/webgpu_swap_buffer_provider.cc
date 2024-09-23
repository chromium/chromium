// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_swap_buffer_provider.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/client/webgpu_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"

namespace blink {

namespace {
viz::SharedImageFormat WGPUFormatToViz(wgpu::TextureFormat format) {
  switch (format) {
    case wgpu::TextureFormat::BGRA8Unorm:
      return viz::SinglePlaneFormat::kBGRA_8888;
    case wgpu::TextureFormat::RGBA8Unorm:
      return viz::SinglePlaneFormat::kRGBA_8888;
    case wgpu::TextureFormat::RGBA16Float:
      return viz::SinglePlaneFormat::kRGBA_F16;
    default:
      NOTREACHED_IN_MIGRATION();
      return viz::SinglePlaneFormat::kRGBA_8888;
  }
}

}  // namespace

WebGPUSwapBufferProvider::WebGPUSwapBufferProvider(
    Client* client,
    scoped_refptr<DawnControlClientHolder> dawn_control_client,
    const wgpu::Device& device,
    wgpu::TextureUsage usage,
    wgpu::TextureUsage internal_usage,
    wgpu::TextureFormat format,
    PredefinedColorSpace color_space,
    const gfx::HDRMetadata& hdr_metadata)
    : dawn_control_client_(dawn_control_client),
      client_(client),
      device_(device),
      shared_image_format_(WGPUFormatToViz(format)),
      format_(format),
      usage_(usage),
      internal_usage_(internal_usage),
      color_space_(color_space),
      hdr_metadata_(hdr_metadata) {
  wgpu::SupportedLimits limits = {};
  auto get_limits_succeeded = device_.GetLimits(&limits);
  CHECK(get_limits_succeeded);

  max_texture_size_ = limits.limits.maxTextureDimension2D;
}

WebGPUSwapBufferProvider::~WebGPUSwapBufferProvider() {
  Neuter();
}

viz::SharedImageFormat WebGPUSwapBufferProvider::Format() const {
  return shared_image_format_;
}

gfx::Size WebGPUSwapBufferProvider::Size() const {
  if (current_swap_buffer_)
    return current_swap_buffer_->GetSharedImage()->size();
  return gfx::Size();
}

cc::Layer* WebGPUSwapBufferProvider::CcLayer() {
  DCHECK(!neutered_);
  return layer_.get();
}

void WebGPUSwapBufferProvider::SetFilterQuality(
    cc::PaintFlags::FilterQuality filter_quality) {
  if (filter_quality != filter_quality_) {
    filter_quality_ = filter_quality;
    if (layer_) {
      layer_->SetNearestNeighbor(filter_quality ==
                                 cc::PaintFlags::FilterQuality::kNone);
    }
  }
}

void WebGPUSwapBufferProvider::ReleaseWGPUTextureAccessIfNeeded() {
  if (!current_swap_buffer_ || !current_swap_buffer_->mailbox_texture) {
    return;
  }

  // The client's lifetime is independent of the swap buffers that can be kept
  // alive longer due to pending shared image callbacks.
  if (client_) {
    client_->OnTextureTransferred();
  }

  current_swap_buffer_->mailbox_texture->Dissociate();
  current_swap_buffer_->mailbox_texture = nullptr;
}

void WebGPUSwapBufferProvider::DiscardCurrentSwapBuffer() {
  if (current_swap_buffer_ && current_swap_buffer_->mailbox_texture) {
    current_swap_buffer_->mailbox_texture->SetNeedsPresent(false);
  }
  ReleaseWGPUTextureAccessIfNeeded();
  current_swap_buffer_ = nullptr;
}

void WebGPUSwapBufferProvider::Neuter() {
  if (neutered_) {
    return;
  }

  if (layer_) {
    layer_->ClearClient();
    layer_ = nullptr;
  }

  DiscardCurrentSwapBuffer();
  client_ = nullptr;
  neutered_ = true;
}

scoped_refptr<WebGPUMailboxTexture> WebGPUSwapBufferProvider::GetNewTexture(
    const wgpu::TextureDescriptor& desc,
    SkAlphaType alpha_mode) {
  DCHECK_EQ(desc.usage, usage_);
  DCHECK_EQ(desc.format, format_);
  DCHECK_EQ(desc.dimension, wgpu::TextureDimension::e2D);
  DCHECK_EQ(desc.size.depthOrArrayLayers, 1u);
  DCHECK_EQ(desc.mipLevelCount, 1u);
  DCHECK_EQ(desc.sampleCount, 1u);

  if (desc.nextInChain) {
    // The internal usage descriptor is the only valid struct to chain.
    CHECK_EQ(desc.nextInChain->sType,
             wgpu::SType::DawnTextureInternalUsageDescriptor);
    CHECK_EQ(desc.nextInChain->nextInChain, nullptr);
    const auto* internal_usage_desc =
        static_cast<const wgpu::DawnTextureInternalUsageDescriptor*>(
            desc.nextInChain);
    DCHECK_EQ(internal_usage_desc->internalUsage, internal_usage_);
  } else {
    DCHECK_EQ(internal_usage_, wgpu::TextureUsage::None);
  }

  auto context_provider = GetContextProviderWeakPtr();
  if (!context_provider) {
    return nullptr;
  }

  gfx::Size size(desc.size.width, desc.size.height);
  if (size.IsEmpty()) {
    return nullptr;
  }

  if (size.width() > max_texture_size_ || size.height() > max_texture_size_) {
    LOG(ERROR) << "GetNewTexture(): invalid size " << size.width() << "x"
               << size.height();
    return nullptr;
  }

  // These SharedImages are read and written by WebGPU clients and can then be
  // sent off to the display compositor.
  gpu::SharedImageUsageSet usage =
      gpu::SHARED_IMAGE_USAGE_WEBGPU_READ |
      gpu::SHARED_IMAGE_USAGE_WEBGPU_WRITE |
      gpu::SHARED_IMAGE_USAGE_WEBGPU_SWAP_CHAIN_TEXTURE |
      GetSharedImageUsagesForDisplay();
  if (usage_ & wgpu::TextureUsage::StorageBinding) {
    usage |= gpu::SHARED_IMAGE_USAGE_WEBGPU_STORAGE_TEXTURE;
  }

  wgpu::AdapterInfo adapter_info;
  device_.GetAdapter().GetInfo(&adapter_info);
  if (adapter_info.adapterType == wgpu::AdapterType::CPU) {
    // When using the fallback adapter, service-side reads and writes of the
    // SharedImage occur via Skia with copies from/to Dawn textures.
    usage |= gpu::SHARED_IMAGE_USAGE_RASTER_READ |
             gpu::SHARED_IMAGE_USAGE_RASTER_WRITE;
  }

  gpu::ImageInfo info = {size,
                         Format(),
                         usage,
                         PredefinedColorSpaceToGfxColorSpace(color_space_),
                         kTopLeft_GrSurfaceOrigin,
                         alpha_mode};

  // If there is no pool or if the pool configuration does not match with the
  // currently needed configuration, then create/re-create the pool with current
  // configuration. This will reset/clear the existing pool if any.
  if (!swap_buffer_pool_ || swap_buffer_pool_->GetImageInfo() != info) {
    swap_buffer_pool_ = gpu::SharedImagePool<SwapBuffer>::Create(
        info, context_provider->ContextProvider()->SharedImageInterface(),
        /*max_pool_size=*/4);
  }

  // Get a swap buffer from pool.
  CHECK(swap_buffer_pool_);
  current_swap_buffer_ = swap_buffer_pool_->GetImage();

  // Make a mailbox texture from the swap buffer.
  // NOTE: Passing WEBGPU_MAILBOX_DISCARD to request clearing requires passing a
  // usage that supports clearing. Swapbuffer textures will always be
  // renderable, so we can pass RenderAttachment.
  current_swap_buffer_->mailbox_texture =
      WebGPUMailboxTexture::FromExistingMailbox(
          dawn_control_client_, device_, desc,
          current_swap_buffer_->GetSharedImage()->mailbox(),
          // Wait on the last usage of this swap buffer.
          current_swap_buffer_->GetSyncToken(),
          gpu::webgpu::WEBGPU_MAILBOX_DISCARD,
          wgpu::TextureUsage::RenderAttachment,
          // When the mailbox texture is dissociated, set the access finished
          // token back on the swap buffer for the next time it is used.
          base::BindOnce(
              [](scoped_refptr<SwapBuffer> swap_buffer,
                 const gpu::SyncToken& access_finished_token) {
                swap_buffer->SetReleaseSyncToken(access_finished_token);
              },
              current_swap_buffer_));

  if (!layer_) {
    // Create a layer that will be used by the canvas and will ask for a
    // SharedImage each frame.
    layer_ = cc::TextureLayer::CreateForMailbox(this);
    layer_->SetIsDrawable(true);
    layer_->SetFlipped(false);
    layer_->SetNearestNeighbor(filter_quality_ ==
                               cc::PaintFlags::FilterQuality::kNone);
    // TODO(cwallez@chromium.org): These flags aren't taken into account when
    // the layer is promoted to an overlay. Make sure we have fallback /
    // emulation paths to keep the rendering correct in that cases.
    layer_->SetPremultipliedAlpha(true);

    if (client_) {
      client_->SetNeedsCompositingUpdate();
    }
  }

  // When the page request a texture it means we'll need to present it on the
  // next animation frame.
  layer_->SetNeedsDisplay();
  layer_->SetContentsOpaque(alpha_mode == kOpaque_SkAlphaType);
  layer_->SetBlendBackgroundColor(alpha_mode != kOpaque_SkAlphaType);

  return current_swap_buffer_->mailbox_texture;
}
scoped_refptr<WebGPUMailboxTexture>
WebGPUSwapBufferProvider::GetLastWebGPUMailboxTexture() const {
  // It's possible this is called after the canvas context current texture has
  // been destroyed, but `current_swap_buffer_` is still available e.g. when the
  // context is used offscreen only.
  auto latest_swap_buffer =
      current_swap_buffer_ ? current_swap_buffer_ : last_swap_buffer_;
  auto context_provider = GetContextProviderWeakPtr();
  if (!latest_swap_buffer || !context_provider) {
    return nullptr;
  }

  wgpu::DawnTextureInternalUsageDescriptor internal_usage;
  internal_usage.internalUsage = internal_usage_;
  wgpu::TextureDescriptor desc = {
      .nextInChain = &internal_usage,
      .usage = usage_,
      .size = {static_cast<uint32_t>(
                   latest_swap_buffer->GetSharedImage()->size().width()),
               static_cast<uint32_t>(
                   latest_swap_buffer->GetSharedImage()->size().height())},
      .format = format_,
  };

  return WebGPUMailboxTexture::FromExistingMailbox(
      dawn_control_client_, device_, desc,
      latest_swap_buffer->GetSharedImage()->mailbox(),
      latest_swap_buffer->GetSyncToken(), gpu::webgpu::WEBGPU_MAILBOX_NONE);
}

base::WeakPtr<WebGraphicsContext3DProviderWrapper>
WebGPUSwapBufferProvider::GetContextProviderWeakPtr() const {
  return dawn_control_client_->GetContextProviderWeakPtr();
}

bool WebGPUSwapBufferProvider::PrepareTransferableResource(
    cc::SharedBitmapIdRegistrar* bitmap_registrar,
    viz::TransferableResource* out_resource,
    viz::ReleaseCallback* out_release_callback) {
  DCHECK(!neutered_);
  if (!current_swap_buffer_ || neutered_ || !GetContextProviderWeakPtr()) {
    return false;
  }

  ReleaseWGPUTextureAccessIfNeeded();

  // Populate the output resource.
  uint32_t texture_target =
      current_swap_buffer_->GetSharedImage()->GetTextureTarget();

  *out_resource = viz::TransferableResource::MakeGpu(
      current_swap_buffer_->GetSharedImage(), texture_target,
      current_swap_buffer_->GetSyncToken(),
      current_swap_buffer_->GetSharedImage()->size(), Format(),
      current_swap_buffer_->GetSharedImage()->usage().Has(
          gpu::SHARED_IMAGE_USAGE_SCANOUT),
      viz::TransferableResource::ResourceSource::kWebGPUSwapBuffer);
  out_resource->color_space = PredefinedColorSpaceToGfxColorSpace(color_space_);
  out_resource->hdr_metadata = hdr_metadata_;

  // This holds a ref on the SwapBuffers that will keep it alive until the
  // mailbox is released (and while the release callback is running).
  *out_release_callback =
      WTF::BindOnce(&WebGPUSwapBufferProvider::MailboxReleased,
                    scoped_refptr<WebGPUSwapBufferProvider>(this),
                    std::move(current_swap_buffer_));

  return true;
}

bool WebGPUSwapBufferProvider::CopyToVideoFrame(
    WebGraphicsContext3DVideoFramePool* frame_pool,
    SourceDrawingBuffer src_buffer,
    const gfx::ColorSpace& dst_color_space,
    WebGraphicsContext3DVideoFramePool::FrameReadyCallback callback) {
  DCHECK(!neutered_);
  if (!current_swap_buffer_ || neutered_ || !GetContextProviderWeakPtr()) {
    return false;
  }

  DCHECK(frame_pool);

  auto* frame_pool_ri = frame_pool->GetRasterInterface();
  DCHECK(frame_pool_ri);

  // Copy kFrontBuffer to a video frame is not supported
  DCHECK_EQ(src_buffer, kBackBuffer);

  // For a conversion from swap buffer's texture to video frame, we do it
  // using WebGraphicsContext3DVideoFramePool's graphics context. Thus, we
  // need to release WebGPU/Dawn's context's access to the texture.
  ReleaseWGPUTextureAccessIfNeeded();

  uint32_t texture_target =
      current_swap_buffer_->GetSharedImage()->GetTextureTarget();

  gpu::MailboxHolder mailbox_holder(
      current_swap_buffer_->GetSharedImage()->mailbox(),
      current_swap_buffer_->GetSyncToken(), texture_target);

  if (frame_pool->CopyRGBATextureToVideoFrame(
          Format(), current_swap_buffer_->GetSharedImage()->size(),
          PredefinedColorSpaceToGfxColorSpace(color_space_),
          kTopLeft_GrSurfaceOrigin, mailbox_holder, dst_color_space,
          std::move(callback))) {
    // Subsequent access to this swap buffer (either webgpu or compositor) must
    // wait for the copy operation to finish.
    gpu::SyncToken sync_token;
    frame_pool_ri->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());
    current_swap_buffer_->SetReleaseSyncToken(std::move(sync_token));
    return true;
  }
  return false;
}

void WebGPUSwapBufferProvider::MailboxReleased(
    scoped_refptr<SwapBuffer> swap_buffer,
    const gpu::SyncToken& sync_token,
    bool lost_resource) {
  // Update the SyncToken to ensure that we will wait for it even if we
  // immediately destroy this buffer.
  swap_buffer->SetReleaseSyncToken(sync_token);

  if (lost_resource)
    return;

  if (last_swap_buffer_) {
    swap_buffer_pool_->ReleaseImage(std::move(last_swap_buffer_));
  }

  last_swap_buffer_ = std::move(swap_buffer);
}

WebGPUSwapBufferProvider::SwapBuffer::SwapBuffer(
    scoped_refptr<gpu::ClientSharedImage> shared_image)
    : ClientImage(std::move(shared_image)) {}

WebGPUSwapBufferProvider::SwapBuffer::~SwapBuffer() = default;

gpu::SharedImageUsageSet
WebGPUSwapBufferProvider::GetSharedImageUsagesForDisplay() {
#if BUILDFLAG(IS_MAC)
  // On Mac it is safe to allow SharedImages created with WebGPU usage that will
  // be sent to the display to be used as overlays, as specifying WebGPU usage
  // when creating a SharedImage forces that SharedImage to be backed by an
  // IOSurface.
  return gpu::SHARED_IMAGE_USAGE_DISPLAY_READ | gpu::SHARED_IMAGE_USAGE_SCANOUT;
#else
  // On other platforms we cannot assume and do not require that a SharedImage
  // created with WebGPU usage be backed by a native buffer.
  return gpu::SHARED_IMAGE_USAGE_DISPLAY_READ;
#endif
}

scoped_refptr<gpu::ClientSharedImage>
WebGPUSwapBufferProvider::GetCurrentSharedImage() {
  return current_swap_buffer_ ? current_swap_buffer_->GetSharedImage()
                              : nullptr;
}

gpu::Mailbox WebGPUSwapBufferProvider::GetCurrentMailboxForTesting() const {
  DCHECK(current_swap_buffer_);
  DCHECK(current_swap_buffer_->GetSharedImage());
  return current_swap_buffer_->GetSharedImage()->mailbox();
}
}  // namespace blink
