// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_mailbox_texture.h"

#include "gpu/command_buffer/client/webgpu_interface.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"

namespace blink {

// static
scoped_refptr<WebGPUMailboxTexture> WebGPUMailboxTexture::FromStaticBitmapImage(
    scoped_refptr<DawnControlClientHolder> dawn_control_client,
    WGPUDevice device,
    WGPUTextureUsage usage,
    scoped_refptr<StaticBitmapImage> image) {
  DCHECK(image->IsTextureBacked());
  auto finished_access_callback =
      WTF::Bind(&StaticBitmapImage::UpdateSyncToken, WTF::RetainedRef(image));

  return base::AdoptRef(new WebGPUMailboxTexture(
      std::move(dawn_control_client), device, usage,
      image->GetMailboxHolder().mailbox, image->GetMailboxHolder().sync_token,
      std::move(finished_access_callback)));
}

// static
scoped_refptr<WebGPUMailboxTexture> WebGPUMailboxTexture::FromCanvasResource(
    scoped_refptr<DawnControlClientHolder> dawn_control_client,
    WGPUDevice device,
    WGPUTextureUsage usage,
    scoped_refptr<CanvasResource> canvas_resource) {
  auto finished_access_callback = WTF::Bind(&CanvasResource::WaitSyncToken,
                                            WTF::RetainedRef(canvas_resource));

  const gpu::Mailbox& mailbox =
      canvas_resource->GetOrCreateGpuMailbox(kUnverifiedSyncToken);
  gpu::SyncToken sync_token = canvas_resource->GetSyncToken();
  return base::AdoptRef(new WebGPUMailboxTexture(
      std::move(dawn_control_client), device, usage, mailbox, sync_token,
      std::move(finished_access_callback)));
}

WebGPUMailboxTexture::WebGPUMailboxTexture(
    scoped_refptr<DawnControlClientHolder> dawn_control_client,
    WGPUDevice device,
    WGPUTextureUsage usage,
    const gpu::Mailbox& mailbox,
    const gpu::SyncToken& sync_token,
    base::OnceCallback<void(const gpu::SyncToken&)> destroy_callback)
    : dawn_control_client_(std::move(dawn_control_client)),
      device_(device),
      destroy_callback_(std::move(destroy_callback)) {
  dawn_control_client_->GetProcs().deviceReference(device_);

  gpu::webgpu::WebGPUInterface* webgpu = dawn_control_client_->GetInterface();

  // Wait on any work using the image.
  webgpu->WaitSyncTokenCHROMIUM(sync_token.GetConstData());

  // Produce and inject image to WebGPU texture
  gpu::webgpu::ReservedTexture reservation = webgpu->ReserveTexture(device_);
  DCHECK(reservation.texture);

  wire_texture_id_ = reservation.id;
  wire_texture_generation_ = reservation.generation;
  texture_ = reservation.texture;

  // This may fail because gl_backing resource cannot produce dawn
  // representation.
  webgpu->AssociateMailbox(reservation.deviceId, reservation.deviceGeneration,
                           wire_texture_id_, wire_texture_generation_, usage,
                           reinterpret_cast<const GLbyte*>(&mailbox));
}

WebGPUMailboxTexture::~WebGPUMailboxTexture() {
  DCHECK_NE(wire_texture_id_, 0u);

  gpu::webgpu::WebGPUInterface* webgpu = dawn_control_client_->GetInterface();
  webgpu->DissociateMailbox(wire_texture_id_, wire_texture_generation_);

  if (destroy_callback_) {
    gpu::SyncToken finished_access_token;
    webgpu->GenUnverifiedSyncTokenCHROMIUM(finished_access_token.GetData());
    std::move(destroy_callback_).Run(finished_access_token);
  }

  dawn_control_client_->GetProcs().deviceRelease(device_);
}

}  // namespace blink
