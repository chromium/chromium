// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_swap_buffer_provider.h"

#include "base/feature_list.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/client/webgpu_interface.h"
#include "gpu/command_buffer/common/shared_image_capabilities.h"
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
      NOTREACHED();
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
    const gfx::HDRMetadata& hdr_metadata,
    GrSurfaceOrigin surface_origin)
    : dawn_control_client_(dawn_control_client),
      client_(client),
      device_(device),
      shared_image_format_(WGPUFormatToViz(format)),
      format_(format),
      usage_(usage),
      internal_usage_(internal_usage),
      color_space_(color_space),
      hdr_metadata_(hdr_metadata),
      surface_origin_(surface_origin) {
  wgpu::Limits limits = {};
  auto get_limits_succeeded = device_.GetLimits(&limits);
  CHECK(get_limits_succeeded);

  max_texture_size_ = limits.maxTextureDimension2D;
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
  // We're discarding the current texture without sending it to the compositor.
  if (current_swap_buffer_ && current_swap_buffer_->mailbox_texture) {
    current_swap_buffer_->mailbox_texture->SetNeedsPresent(false);

    // Release the texture access and put it back in the pool to be recycled.
    // Otherwise, we'll destroy the shared image associated with the texture
    // instead of reusing it like if the texture was composited.
    ReleaseWGPUTextureAccessIfNeeded();

    swap_buffer_pool_->ReleaseImage(std::move(current_swap_buffer_));
  }
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

  // Clear the pool after discarding the current swap buffer since the current
  // swap buffer could be recycled into the pool.
  DiscardCurrentSwapBuffer();

  // Check that the pool is present before clearing it - the pool is created
  // in the first GetNewTexture() call.
  if (swap_buffer_pool_) {
    swap_buffer_pool_->Clear();
  }

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
  // sent off to the display compositor. They can also be read over raster
  // interface as part of video frame.
  gpu::SharedImageUsageSet usage =
      gpu::SHARED_IMAGE_USAGE_WEBGPU_READ |
      gpu::SHARED_IMAGE_USAGE_WEBGPU_WRITE |
      gpu::SHARED_IMAGE_USAGE_WEBGPU_SWAP_CHAIN_TEXTURE |
      gpu::SHARED_IMAGE_USAGE_RASTER_READ | GetSharedImageUsagesForDisplay();
  if (usage_ & wgpu::TextureUsage::StorageBinding) {
    usage |= gpu::SHARED_IMAGE_USAGE_WEBGPU_STORAGE_TEXTURE;
  }

  wgpu::AdapterInfo adapter_info;
  device_.GetAdapter().GetInfo(&adapter_info);
  if (adapter_info.adapterType == wgpu::AdapterType::CPU) {
    // When using the fallback adapter, service-side writes of the
    // SharedImage occur via Skia with copies from/to Dawn textures.
    usage |= gpu::SHARED_IMAGE_USAGE_RASTER_WRITE;
  }

  gpu::ImageInfo info = {size,
                         Format(),
                         usage,
                         PredefinedColorSpaceToGfxColorSpace(color_space_),
                         surface_origin_,
                         alpha_mode};

  // Note that if the pool already exists but have different ImageInfo than what
  // is required, we reconfigure the same pool with new ImageInfo instead of
  // deleting old pool and creating a new one. This is required to take
  // advantage of the temporal information the pool might have from its previous
  // use.
  if (!swap_buffer_pool_) {
    swap_buffer_pool_ = gpu::SharedImagePool<SwapBuffer>::Create(
        info, context_provider->ContextProvider().SharedImageInterface(),
        "WebGPUSwapBufferProvider", /*max_pool_size=*/4);
  } else if (swap_buffer_pool_->GetImageInfo() != info) {
    swap_buffer_pool_->Reconfigure(info);
  }

  // Get a swap buffer from pool.
  CHECK(swap_buffer_pool_);
  current_swap_buffer_ = swap_buffer_pool_->GetImage();

  // Make a mailbox texture from the swap buffer.
  // NOTE: Passing WEBGPU_MAILBOX_DISCARD to request clearing requires passing a
  // usage that supports clearing. Swapbuffer textures will always be
  // renderable, so we can pass RenderAttachment.
  current_swap_buffer_->mailbox_texture =
      WebGPUMailboxTexture::FromExistingSharedImage(
          dawn_control_client_, device_, desc,
          current_swap_buffer_->GetSharedImage(),
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
    layer_ = cc::TextureLayer::Create(this);
    if (client_) {
      client_->InitializeLayer(layer_.get());
    }
    layer_->SetIsDrawable(true);

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
base::WeakPtr<WebGraphicsContext3DProviderWrapper>
WebGPUSwapBufferProvider::GetContextProviderWeakPtr() const {
  return dawn_control_client_->GetContextProviderWeakPtr();
}

scoped_refptr<gpu::ClientSharedImage>
WebGPUSwapBufferProvider::ExportCurrentSharedImage(
    gpu::SyncToken& sync_token,
    viz::ReleaseCallback* out_release_callback) {
  DCHECK(!neutered_);
  if (!current_swap_buffer_ || neutered_ || !GetContextProviderWeakPtr()) {
    return nullptr;
  }

  if (client_ && client_->IsGPUDeviceDestroyed()) {
    return nullptr;
  }

  scoped_refptr<gpu::ClientSharedImage> shared_image = GetCurrentSharedImage();

  ReleaseWGPUTextureAccessIfNeeded();

  // NOTE: This must be populated *after* the above call as that call updates
  // the current swap buffer's sync token.
  sync_token = current_swap_buffer_->GetSyncToken();

  // We are binding current_swap_buffer_ to callback that can be destroyed on a
  // different thread, so make sure we don't have any non thread-safe state.
  CHECK(!current_swap_buffer_->mailbox_texture);
  // This holds a ref on the current_swap_buffer_ that will keep it alive until
  // the mailbox is released (and while the release callback is running). Note,
  // that callback can be invoked only on this thread, but can be destroyed on
  // any thread in case this thread was terminated. Ref to SwapBuffers is enough
  // to keep underlying resources alive, so we don't need to hold ref to
  // WebGPUSwapBufferProvider itself.
  *out_release_callback = blink::BindOnce(
      &WebGPUSwapBufferProvider::MailboxReleased,
      weak_ptr_factory_.GetWeakPtr(), base::PlatformThread::CurrentRef(),
      std::move(current_swap_buffer_));

  return shared_image;
}

bool WebGPUSwapBufferProvider::PrepareTransferableResource(
    viz::TransferableResource* out_resource,
    viz::ReleaseCallback* out_release_callback) {
  front_buffer_shared_image_ = nullptr;
  front_buffer_sync_token_ = gpu::SyncToken();

  gpu::SyncToken sync_token;

  scoped_refptr<gpu::ClientSharedImage> shared_image =
      ExportCurrentSharedImage(sync_token, out_release_callback);
  if (!shared_image) {
    return false;
  }

  front_buffer_shared_image_ = shared_image;
  front_buffer_sync_token_ = sync_token;

  // Populate the output resource.
  *out_resource = viz::TransferableResource::Make(
      shared_image,
      viz::TransferableResource::ResourceSource::kWebGPUSwapBuffer, sync_token);
  out_resource->hdr_metadata = GetHDRMetadata();

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

  if (client_ && client_->IsGPUDeviceDestroyed()) {
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

  std::optional<gpu::SyncToken> optional_sync_token =
      frame_pool->CopyRGBATextureToVideoFrame(
          current_swap_buffer_->GetSharedImage()->size(),
          current_swap_buffer_->GetSharedImage(),
          current_swap_buffer_->GetSyncToken(), dst_color_space,
          std::move(callback));
  if (optional_sync_token.has_value()) {
    // Subsequent access to this swap buffer (either webgpu or compositor) must
    // wait for the copy operation to finish.
    current_swap_buffer_->SetReleaseSyncToken(
        std::move(optional_sync_token.value()));
    return true;
  }
  return false;
}

void WebGPUSwapBufferProvider::MailboxReleased(
    base::WeakPtr<WebGPUSwapBufferProvider> provider,
    base::PlatformThreadRef thread_ref,
    scoped_refptr<SwapBuffer> swap_buffer,
    const gpu::SyncToken& sync_token,
    bool lost_resource) {
  // Update the SyncToken to ensure that we will wait for it even if we
  // immediately destroy this buffer.
  swap_buffer->SetReleaseSyncToken(sync_token);

  if (lost_resource)
    return;

  // This callback should never run on different thread. In case our thread was
  // destroyed, callback should be discarded (it can be discarded on any
  // thread).
  CHECK_EQ(thread_ref, base::PlatformThread::CurrentRef());

  if (provider && !provider->neutered_) {
    provider->swap_buffer_pool_->ReleaseImage(std::move(swap_buffer));
  }
}

WebGPUSwapBufferProvider::SwapBuffer::SwapBuffer(
    scoped_refptr<gpu::ClientSharedImage> shared_image)
    : ClientImage(std::move(shared_image)) {}

WebGPUSwapBufferProvider::SwapBuffer::~SwapBuffer() = default;

#if BUILDFLAG(IS_CHROMEOS)
// This feature is only used as a possible killswitch.
BASE_FEATURE(kWebGPUSwapBufferProviderAllowScanout,
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

gpu::SharedImageUsageSet
WebGPUSwapBufferProvider::GetSharedImageUsagesForDisplay() {
#if BUILDFLAG(IS_MAC)
  // On Mac it is safe to allow SharedImages created with WebGPU usage that will
  // be sent to the display to be used as overlays, as specifying WebGPU usage
  // when creating a SharedImage forces that SharedImage to be backed by an
  // IOSurface.
  return gpu::SHARED_IMAGE_USAGE_DISPLAY_READ | gpu::SHARED_IMAGE_USAGE_SCANOUT;
#elif BUILDFLAG(IS_CHROMEOS)
  gpu::SharedImageUsageSet usage = gpu::SHARED_IMAGE_USAGE_DISPLAY_READ;
  static bool is_scanout_allowed =
      base::FeatureList::IsEnabled(kWebGPUSwapBufferProviderAllowScanout);

  if (is_scanout_allowed && GetContextProviderWeakPtr() &&
      GetContextProviderWeakPtr()
          ->ContextProvider()
          .SharedImageInterface()
          ->GetCapabilities()
          .supports_scanout_shared_images) {
    usage |= gpu::SHARED_IMAGE_USAGE_SCANOUT;
  }
  return usage;
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

scoped_refptr<gpu::ClientSharedImage>
WebGPUSwapBufferProvider::GetFrontBufferSharedImage() {
  return front_buffer_shared_image_;
}

gpu::SyncToken WebGPUSwapBufferProvider::GetFrontBufferSyncToken() {
  return front_buffer_sync_token_;
}

gpu::Mailbox WebGPUSwapBufferProvider::GetCurrentMailboxForTesting() const {
  DCHECK(current_swap_buffer_);
  DCHECK(current_swap_buffer_->GetSharedImage());
  return current_swap_buffer_->GetSharedImage()->mailbox();
}
}  // namespace blink
