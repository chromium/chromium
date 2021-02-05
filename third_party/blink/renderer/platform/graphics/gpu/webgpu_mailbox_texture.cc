// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_mailbox_texture.h"

#include "gpu/command_buffer/client/webgpu_interface.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"

namespace blink {

WebGPUMailboxTexture::WebGPUMailboxTexture(
    scoped_refptr<DawnControlClientHolder> dawn_control_client,
    WGPUDevice device,
    StaticBitmapImage* image,
    WGPUTextureUsage usage)
    : dawn_control_client_(std::move(dawn_control_client)), device_(device) {
  dawn_control_client_->GetProcs().deviceReference(device_);

  mailbox_ = image->GetMailboxHolder().mailbox;

  // Produce and inject image to WebGPU texture
  gpu::webgpu::WebGPUInterface* webgpu = dawn_control_client_->GetInterface();
  gpu::webgpu::ReservedTexture reservation = webgpu->ReserveTexture(device_);
  DCHECK(reservation.texture);

  wire_texture_id_ = reservation.id;
  wire_texture_generation_ = reservation.generation;
  texture_ = reservation.texture;

  // This may fail because gl_backing resource cannot produce dawn
  // representation.
  webgpu->AssociateMailbox(reservation.deviceId, reservation.deviceGeneration,
                           wire_texture_id_, wire_texture_generation_, usage,
                           reinterpret_cast<GLbyte*>(&mailbox_));
}

WebGPUMailboxTexture::~WebGPUMailboxTexture() {
  DCHECK_NE(wire_texture_id_, 0u);

  gpu::webgpu::WebGPUInterface* webgpu = dawn_control_client_->GetInterface();
  webgpu->DissociateMailbox(wire_texture_id_, wire_texture_generation_);
  mailbox_.SetZero();

  dawn_control_client_->GetProcs().deviceRelease(device_);
}

}  // namespace blink
