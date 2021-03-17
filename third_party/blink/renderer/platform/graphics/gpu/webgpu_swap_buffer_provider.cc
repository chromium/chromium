// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_swap_buffer_provider.h"

#include "build/build_config.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/client/webgpu_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"

namespace blink {

namespace {
viz::ResourceFormat WGPUFormatToViz(WGPUTextureFormat format) {
  if (format == WGPUTextureFormat_BGRA8Unorm) {
    return viz::BGRA_8888;
  }
  if (format == WGPUTextureFormat_RGBA8Unorm) {
    return viz::RGBA_8888;
  }
  NOTREACHED();
  return viz::RGBA_8888;
}
}  // namespace

WebGPUSwapBufferProvider::WebGPUSwapBufferProvider(
    Client* client,
    scoped_refptr<DawnControlClientHolder> dawn_control_client,
    WGPUDevice device,
    WGPUTextureUsage usage,
    WGPUTextureFormat format)
    : dawn_control_client_(dawn_control_client),
      client_(client),
      device_(device),
      usage_(usage),
      format_(WGPUFormatToViz(format)) {
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

cc::Layer* WebGPUSwapBufferProvider::CcLayer() {
  DCHECK(!neutered_);
  return layer_.get();
}

void WebGPUSwapBufferProvider::SetFilterQuality(
    SkFilterQuality filter_quality) {
  if (layer_) {
    layer_->SetNearestNeighbor(filter_quality == kNone_SkFilterQuality);
  }
}

void WebGPUSwapBufferProvider::Neuter() {
  if (neutered_) {
    return;
  }

  if (layer_) {
    layer_->ClearClient();
    layer_ = nullptr;
  }

  if (current_swap_buffer_) {
    // Ensure we wait for previous WebGPU commands before destroying the shared
    // image.
    gpu::webgpu::WebGPUInterface* webgpu = dawn_control_client_->GetInterface();
    webgpu->GenUnverifiedSyncTokenCHROMIUM(
        current_swap_buffer_->access_finished_token.GetData());
    current_swap_buffer_ = nullptr;
  }

  client_ = nullptr;
  neutered_ = true;
}

WGPUTexture WebGPUSwapBufferProvider::GetNewTexture(const IntSize& size) {
  DCHECK(!current_swap_buffer_);

  gpu::webgpu::WebGPUInterface* webgpu = dawn_control_client_->GetInterface();
  gpu::SharedImageInterface* sii =
      dawn_control_client_->GetContextProvider()->SharedImageInterface();

  // Create a new swap buffer.
  // TODO(cwallez@chromium.org): have some recycling mechanism.
  gpu::Mailbox mailbox = sii->CreateSharedImage(
      format_, static_cast<gfx::Size>(size), gfx::ColorSpace::CreateSRGB(),
      kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType,
      gpu::SHARED_IMAGE_USAGE_WEBGPU |
          gpu::SHARED_IMAGE_USAGE_WEBGPU_SWAP_CHAIN_TEXTURE |
          gpu::SHARED_IMAGE_USAGE_DISPLAY,
      gpu::kNullSurfaceHandle);
  gpu::SyncToken creation_token = sii->GenUnverifiedSyncToken();

  current_swap_buffer_ = base::AdoptRef(new SwapBuffer(
      this, mailbox, creation_token, static_cast<gfx::Size>(size)));

  // Ensure the shared image is allocated service-side before working with it
  webgpu->WaitSyncTokenCHROMIUM(
      current_swap_buffer_->access_finished_token.GetConstData());

  // Associate the mailbox to a dawn_wire client DawnTexture object
  gpu::webgpu::ReservedTexture reservation = webgpu->ReserveTexture(device_);
  DCHECK(reservation.texture);
  wire_texture_id_ = reservation.id;
  wire_texture_generation_ = reservation.generation;

  webgpu->AssociateMailbox(
      reservation.deviceId, reservation.deviceGeneration, reservation.id,
      reservation.generation, usage_,
      reinterpret_cast<GLbyte*>(&current_swap_buffer_->mailbox));

  // When the page request a texture it means we'll need to present it on the
  // next animation frame.
  layer_->SetNeedsDisplay();

  return reservation.texture;
}

base::WeakPtr<WebGraphicsContext3DProviderWrapper>
WebGPUSwapBufferProvider::GetContextProviderWeakPtr() const {
  return dawn_control_client_->GetContextProviderWeakPtr();
}

bool WebGPUSwapBufferProvider::PrepareTransferableResource(
    cc::SharedBitmapIdRegistrar* bitmap_registrar,
    viz::TransferableResource* out_resource,
    std::unique_ptr<viz::SingleReleaseCallback>* out_release_callback) {
  DCHECK(!neutered_);
  if (!current_swap_buffer_ || neutered_) {
    return false;
  }

  DCHECK(client_);
  client_->OnTextureTransferred();

  // Make Dawn relinquish access to the texture so it can be used by the
  // compositor. This will call wgpu::Texture::Destroy so that further accesses
  // to the texture are errors.
  gpu::webgpu::WebGPUInterface* webgpu = dawn_control_client_->GetInterface();
  DCHECK_NE(wire_texture_id_, 0u);
  webgpu->DissociateMailbox(wire_texture_id_, wire_texture_generation_);

  // Make the compositor wait on previous Dawn commands.
  webgpu->GenUnverifiedSyncTokenCHROMIUM(
      current_swap_buffer_->access_finished_token.GetData());

  // On macOS, shared images are backed by IOSurfaces that can only be used
  // with OpenGL via the rectangle texture target. Every other shared image
  // implementation is implemented on OpenGL via some form of eglSurface and
  // eglBindTexImage (on ANGLE or system drivers) so they use the 2D texture
  // target.
  const uint32_t texture_target =
#if defined(OS_MAC)
      GL_TEXTURE_RECTANGLE_ARB
#else
      GL_TEXTURE_2D
#endif
      ;
  // Populate the output resource
  *out_resource = viz::TransferableResource::MakeGL(
      current_swap_buffer_->mailbox, GL_LINEAR, texture_target,
      current_swap_buffer_->access_finished_token, current_swap_buffer_->size,
      false);
  out_resource->color_space = gfx::ColorSpace::CreateSRGB();
  out_resource->format = format_;

  // This holds a ref on the SwapBuffers that will keep it alive until the
  // mailbox is released (and while the release callback is running).
  auto func = WTF::Bind(&WebGPUSwapBufferProvider::MailboxReleased,
                        scoped_refptr<WebGPUSwapBufferProvider>(this),
                        current_swap_buffer_);
  *out_release_callback = viz::SingleReleaseCallback::Create(std::move(func));

  current_swap_buffer_ = nullptr;
  wire_texture_id_ = 0;
  wire_texture_generation_ = 0;

  return true;
}

void WebGPUSwapBufferProvider::MailboxReleased(
    scoped_refptr<SwapBuffer> swap_buffer,
    const gpu::SyncToken& sync_token,
    bool lost_resource) {
  // Update the SyncToken to ensure that we will wait for it even if we
  // immediately destroy this buffer.
  swap_buffer->access_finished_token = sync_token;
}

WebGPUSwapBufferProvider::SwapBuffer::SwapBuffer(
    WebGPUSwapBufferProvider* swap_buffers,
    gpu::Mailbox mailbox,
    gpu::SyncToken creation_token,
    gfx::Size size)
    : size(size),
      mailbox(mailbox),
      swap_buffers(swap_buffers),
      access_finished_token(creation_token) {}

WebGPUSwapBufferProvider::SwapBuffer::~SwapBuffer() {
  gpu::SharedImageInterface* sii =
      swap_buffers->dawn_control_client_->GetContextProvider()
          ->SharedImageInterface();
  sii->DestroySharedImage(access_finished_token, mailbox);
}

}  // namespace blink
