// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_swap_buffer_provider.h"

#include "build/build_config.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/client/webgpu_interface.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/common/shared_image_usage.h"

namespace blink {

namespace {
viz::SharedImageFormat WGPUFormatToViz(WGPUTextureFormat format) {
  switch (format) {
    case WGPUTextureFormat_BGRA8Unorm:
      return viz::SinglePlaneFormat::kBGRA_8888;
    case WGPUTextureFormat_RGBA8Unorm:
      return viz::SinglePlaneFormat::kRGBA_8888;
    case WGPUTextureFormat_RGBA16Float:
      return viz::SinglePlaneFormat::kRGBA_F16;
    default:
      NOTREACHED();
      return viz::SinglePlaneFormat::kRGBA_8888;
  }
}

}  // namespace

WebGPUSwapBufferProvider::WebGPUSwapBufferProvider(
    Client* client,
    scoped_refptr<DawnControlClientHolder> dawn_control_client,
    WGPUDevice device,
    WGPUTextureUsage usage,
    WGPUTextureFormat format,
    PredefinedColorSpace color_space)
    : dawn_control_client_(dawn_control_client),
      client_(client),
      device_(device),
      format_(WGPUFormatToViz(format)),
      usage_(usage),
      color_space_(color_space) {
  // Create a layer that will be used by the canvas and will ask for a
  // SharedImage each frame.
  layer_ = cc::TextureLayer::CreateForMailbox(this);

  layer_->SetIsDrawable(true);
  layer_->SetBlendBackgroundColor(false);
  layer_->SetNearestNeighbor(false);
  layer_->SetFlipped(false);
  // TODO(cwallez@chromium.org): These flags aren't taken into account when the
  // layer is promoted to an overlay. Make sure we have fallback / emulation
  // paths to keep the rendering correct in that cases.
  layer_->SetContentsOpaque(true);
  layer_->SetPremultipliedAlpha(true);

  dawn_control_client_->GetProcs().deviceReference(device_);
}

WebGPUSwapBufferProvider::~WebGPUSwapBufferProvider() {
  Neuter();
  dawn_control_client_->GetProcs().deviceRelease(device_);
  device_ = nullptr;
}

viz::SharedImageFormat WebGPUSwapBufferProvider::Format() const {
  return format_;
}

const gfx::Size& WebGPUSwapBufferProvider::Size() const {
  if (current_swap_buffer_)
    return current_swap_buffer_->size;

  static constexpr gfx::Size kEmpty;
  return kEmpty;
}

cc::Layer* WebGPUSwapBufferProvider::CcLayer() {
  DCHECK(!neutered_);
  return layer_.get();
}

void WebGPUSwapBufferProvider::SetFilterQuality(
    cc::PaintFlags::FilterQuality filter_quality) {
  if (layer_) {
    layer_->SetNearestNeighbor(filter_quality ==
                               cc::PaintFlags::FilterQuality::kNone);
  }
}

std::tuple<uint32_t, bool>
WebGPUSwapBufferProvider::GetTextureTargetAndOverlayCandidacy() const {
// On macOS, shared images are backed by IOSurfaces that can only be used with
// OpenGL via the rectangle texture target and are overlay candidates. Every
// other shared image implementation is implemented on OpenGL via some form of
// eglSurface and eglBindTexImage (on ANGLE or system drivers) so they use the
// 2D texture target and cannot always be overlay candidates.
#if BUILDFLAG(IS_MAC)
  const uint32_t texture_target = gpu::GetPlatformSpecificTextureTarget();
  const bool is_overlay_candidate = true;
#else
  const uint32_t texture_target = GL_TEXTURE_2D;
  const bool is_overlay_candidate = false;
#endif

  return std::make_tuple(texture_target, is_overlay_candidate);
}

uint32_t WebGPUSwapBufferProvider::GetTextureTarget() const {
  return std::get<0>(GetTextureTargetAndOverlayCandidacy());
}
bool WebGPUSwapBufferProvider::IsOverlayCandidate() const {
  return std::get<1>(GetTextureTargetAndOverlayCandidacy());
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

scoped_refptr<WebGPUSwapBufferProvider::SwapBuffer>
WebGPUSwapBufferProvider::NewOrRecycledSwapBuffer(
    gpu::SharedImageInterface* sii,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider,
    const gfx::Size& size,
    SkAlphaType alpha_mode) {
  // Recycled SwapBuffers must be the same size.
  if (!unused_swap_buffers_.empty() &&
      unused_swap_buffers_.back()->size != size) {
    unused_swap_buffers_.clear();
  }

  if (unused_swap_buffers_.empty()) {
    gpu::Mailbox mailbox = sii->CreateSharedImage(
        Format(), size, PredefinedColorSpaceToGfxColorSpace(color_space_),
        kTopLeft_GrSurfaceOrigin, alpha_mode,
        gpu::SHARED_IMAGE_USAGE_WEBGPU |
            gpu::SHARED_IMAGE_USAGE_WEBGPU_SWAP_CHAIN_TEXTURE |
            gpu::SHARED_IMAGE_USAGE_DISPLAY_READ,
        gpu::kNullSurfaceHandle);
    gpu::SyncToken creation_token = sii->GenUnverifiedSyncToken();

    unused_swap_buffers_.push_back(base::MakeRefCounted<SwapBuffer>(
        std::move(context_provider), mailbox, creation_token, size));
    DCHECK_EQ(unused_swap_buffers_.back()->size, size);
  }

  scoped_refptr<SwapBuffer> swap_buffer =
      std::move(unused_swap_buffers_.back());
  unused_swap_buffers_.pop_back();

  DCHECK_EQ(swap_buffer->size, size);
  return swap_buffer;
}

void WebGPUSwapBufferProvider::RecycleSwapBuffer(
    scoped_refptr<SwapBuffer> swap_buffer) {
  // We don't want to keep an arbitrary large number of swap buffers.
  if (unused_swap_buffers_.size() >
      static_cast<unsigned int>(kMaxRecycledSwapBuffers))
    return;

  unused_swap_buffers_.push_back(std::move(swap_buffer));
}

scoped_refptr<WebGPUMailboxTexture> WebGPUSwapBufferProvider::GetNewTexture(
    const WGPUTextureDescriptor& desc,
    SkAlphaType alpha_mode) {
  DCHECK_EQ(desc.nextInChain, nullptr);
  DCHECK_EQ(desc.usage, usage_);
  DCHECK_EQ(WGPUFormatToViz(desc.format), format_);
  DCHECK_EQ(desc.dimension, WGPUTextureDimension_2D);
  DCHECK_EQ(desc.size.depthOrArrayLayers, 1u);
  DCHECK_EQ(desc.mipLevelCount, 1u);
  DCHECK_EQ(desc.sampleCount, 1u);

  auto context_provider = GetContextProviderWeakPtr();
  if (!context_provider) {
    return nullptr;
  }

  gfx::Size size(desc.size.width, desc.size.height);
  if (size.IsEmpty()) {
    return nullptr;
  }

  // Create a new swap buffer.
  current_swap_buffer_ = NewOrRecycledSwapBuffer(
      context_provider->ContextProvider()->SharedImageInterface(),
      context_provider, size, alpha_mode);

  // Make a mailbox texture from the swap buffer.
  current_swap_buffer_->mailbox_texture =
      WebGPUMailboxTexture::FromExistingMailbox(
          dawn_control_client_, device_, desc, current_swap_buffer_->mailbox,
          // Wait on the last usage of this swap buffer.
          current_swap_buffer_->access_finished_token,
          gpu::webgpu::WEBGPU_MAILBOX_DISCARD,
          // When the mailbox texture is dissociated, set the access finished
          // token back on the swap buffer for the next time it is used.
          base::BindOnce(
              [](scoped_refptr<SwapBuffer> swap_buffer,
                 const gpu::SyncToken& access_finished_token) {
                swap_buffer->access_finished_token = access_finished_token;
              },
              current_swap_buffer_));

  // When the page request a texture it means we'll need to present it on the
  // next animation frame.
  layer_->SetNeedsDisplay();

  return current_swap_buffer_->mailbox_texture;
}

WebGPUSwapBufferProvider::WebGPUMailboxTextureAndSize
WebGPUSwapBufferProvider::GetLastWebGPUMailboxTextureAndSize() const {
  auto context_provider = GetContextProviderWeakPtr();
  if (!last_swap_buffer_ || !context_provider)
    return WebGPUMailboxTextureAndSize(nullptr, gfx::Size());

  WGPUTextureDescriptor desc = {};
  desc.usage = usage_;

  return WebGPUMailboxTextureAndSize(
      WebGPUMailboxTexture::FromExistingMailbox(
          dawn_control_client_, device_, desc, last_swap_buffer_->mailbox,
          last_swap_buffer_->access_finished_token,
          gpu::webgpu::WEBGPU_MAILBOX_NONE),
      last_swap_buffer_->size);
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

  // Populate the output resource
  *out_resource = viz::TransferableResource::MakeGpu(
      current_swap_buffer_->mailbox, GL_LINEAR, GetTextureTarget(),
      current_swap_buffer_->access_finished_token, current_swap_buffer_->size,
      Format(), IsOverlayCandidate());
  out_resource->color_space = PredefinedColorSpaceToGfxColorSpace(color_space_);

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
  if (neutered_ || !GetContextProviderWeakPtr()) {
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

  gpu::MailboxHolder mailbox_holder(current_swap_buffer_->mailbox,
                                    current_swap_buffer_->access_finished_token,
                                    GetTextureTarget());

  auto success = frame_pool->CopyRGBATextureToVideoFrame(
      Format().resource_format(), current_swap_buffer_->size,
      PredefinedColorSpaceToGfxColorSpace(color_space_),
      kTopLeft_GrSurfaceOrigin, mailbox_holder, dst_color_space,
      std::move(callback));

  // Subsequent access to this swap buffer (either webgpu or compositor) must
  // wait for the copy operation to finish.
  frame_pool_ri->GenUnverifiedSyncTokenCHROMIUM(
      current_swap_buffer_->access_finished_token.GetData());

  return success;
}

void WebGPUSwapBufferProvider::MailboxReleased(
    scoped_refptr<SwapBuffer> swap_buffer,
    const gpu::SyncToken& sync_token,
    bool lost_resource) {
  // Update the SyncToken to ensure that we will wait for it even if we
  // immediately destroy this buffer.
  swap_buffer->access_finished_token = sync_token;

  if (lost_resource)
    return;

  if (last_swap_buffer_) {
    RecycleSwapBuffer(std::move(last_swap_buffer_));
  }

  last_swap_buffer_ = std::move(swap_buffer);
}

WebGPUSwapBufferProvider::SwapBuffer::SwapBuffer(
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider,
    gpu::Mailbox mailbox,
    gpu::SyncToken creation_token,
    gfx::Size size)
    : size(size),
      mailbox(mailbox),
      context_provider(context_provider),
      access_finished_token(creation_token) {}

WebGPUSwapBufferProvider::SwapBuffer::~SwapBuffer() {
  if (context_provider) {
    gpu::SharedImageInterface* sii =
        context_provider->ContextProvider()->SharedImageInterface();
    sii->DestroySharedImage(access_finished_token, mailbox);
  }
}

gpu::Mailbox WebGPUSwapBufferProvider::GetCurrentMailboxForTesting() const {
  DCHECK(current_swap_buffer_);
  return current_swap_buffer_->mailbox;
}
}  // namespace blink
