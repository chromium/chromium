// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_gpu_swap_chain.h"

#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_texture.h"
#include "third_party/blink/renderer/modules/xr/xr_composition_layer.h"
#include "third_party/blink/renderer/modules/xr/xr_layer_mailbox_manager.h"

namespace blink {

void XRGPUSwapChain::OnFrameStart() {}
void XRGPUSwapChain::OnFrameEnd() {}
void XRGPUSwapChain::Trace(Visitor* visitor) const {
  visitor->Trace(layer_);
}

XRGPUMailboxSwapChain::XRGPUMailboxSwapChain(
    GPUDevice* device,
    const wgpu::TextureDescriptor& desc)
    : device_(device) {
  CHECK(device);

  descriptor_ = desc;
}

GPUTexture* XRGPUMailboxSwapChain::GetCurrentTexture() {
  if (texture_) {
    return texture_.Get();
  }

  const XRLayerMailboxes& mailboxes = layer()->GetMailboxes();

  // TODO(crbug.com/359418629): Allow for other mailboxes as well?
  CHECK(mailboxes.color_mailbox_holder);

  scoped_refptr<WebGPUMailboxTexture> mailbox_texture =
      WebGPUMailboxTexture::FromExistingMailbox(
          device_->GetDawnControlClient(), device_->GetHandle(), descriptor_,
          mailboxes.color_mailbox_holder->mailbox,
          mailboxes.color_mailbox_holder->sync_token);

  texture_ = MakeGarbageCollected<GPUTexture>(
      device_, descriptor_.format, descriptor_.usage,
      std::move(mailbox_texture), "WebXR Mailbox Swap Chain");

  return texture_.Get();
}

void XRGPUMailboxSwapChain::OnFrameEnd() {
  if (texture_) {
    texture_->DissociateMailbox();
    texture_ = nullptr;
  }
}

void XRGPUMailboxSwapChain::Trace(Visitor* visitor) const {
  visitor->Trace(device_);
  visitor->Trace(texture_);
  XRGPUSwapChain::Trace(visitor);
}

}  // namespace blink
