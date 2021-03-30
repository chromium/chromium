// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_swap_chain.h"

#include "third_party/blink/renderer/modules/webgpu/gpu_canvas_context.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_texture.h"
#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_mailbox_texture.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

GPUSwapChain::GPUSwapChain(GPUCanvasContext* context,
                           GPUDevice* device,
                           WGPUTextureUsage usage,
                           WGPUTextureFormat format,
                           SkFilterQuality filter_quality)
    : DawnObjectImpl(device),
      context_(context),
      usage_(usage),
      format_(format) {
  // TODO: Use label from GPUObjectDescriptorBase.
  swap_buffers_ = base::AdoptRef(new WebGPUSwapBufferProvider(
      this, GetDawnControlClient(), device->GetHandle(), usage_, format));
  swap_buffers_->SetFilterQuality(filter_quality);
}

GPUSwapChain::~GPUSwapChain() {
  Neuter();
}

void GPUSwapChain::Trace(Visitor* visitor) const {
  visitor->Trace(context_);
  visitor->Trace(texture_);
  DawnObjectImpl::Trace(visitor);
}

void GPUSwapChain::Neuter() {
  texture_ = nullptr;
  if (swap_buffers_) {
    swap_buffers_->Neuter();
    swap_buffers_ = nullptr;
  }
}

cc::Layer* GPUSwapChain::CcLayer() {
  DCHECK(swap_buffers_);
  return swap_buffers_->CcLayer();
}

void GPUSwapChain::SetFilterQuality(SkFilterQuality filter_quality) {
  DCHECK(swap_buffers_);
  if (swap_buffers_) {
    swap_buffers_->SetFilterQuality(filter_quality);
  }
}

scoped_refptr<StaticBitmapImage> GPUSwapChain::TransferToStaticBitmapImage() {
  viz::TransferableResource transferable_resource;
  std::unique_ptr<viz::SingleReleaseCallback> release_callback;
  if (!swap_buffers_->PrepareTransferableResource(
          nullptr, &transferable_resource, &release_callback)) {
    // If we can't get a mailbox, return an transparent black ImageBitmap.
    // The only situation in which this could happen is when two or more calls
    // to transferToImageBitmap are made back-to-back, or when the context gets
    // lost. We intentionally leave the transparent black image in legacy color
    // space.
    SkBitmap black_bitmap;
    black_bitmap.allocN32Pixels(transferable_resource.size.width(),
                                transferable_resource.size.height());
    black_bitmap.eraseARGB(0, 0, 0, 0);
    return UnacceleratedStaticBitmapImage::Create(
        SkImage::MakeFromBitmap(black_bitmap));
  }
  DCHECK(release_callback);

  // We reuse the same mailbox name from above since our texture id was consumed
  // from it.
  const auto& sk_image_mailbox = transferable_resource.mailbox_holder.mailbox;
  // Use the sync token generated after producing the mailbox. Waiting for this
  // before trying to use the mailbox with some other context will ensure it is
  // valid.
  const auto& sk_image_sync_token =
      transferable_resource.mailbox_holder.sync_token;

  const SkImageInfo sk_image_info = SkImageInfo::MakeN32Premul(
      transferable_resource.size.width(), transferable_resource.size.height());

  return AcceleratedStaticBitmapImage::CreateFromCanvasMailbox(
      sk_image_mailbox, sk_image_sync_token, /* shared_image_texture_id = */ 0,
      sk_image_info, transferable_resource.mailbox_holder.texture_target,
      /* is_origin_top_left = */ kBottomLeft_GrSurfaceOrigin,
      swap_buffers_->GetContextProviderWeakPtr(),
      base::PlatformThread::CurrentRef(), Thread::Current()->GetTaskRunner(),
      std::move(release_callback));
}

// gpu_swap_chain.idl
GPUTexture* GPUSwapChain::getCurrentTexture() {
  if (!swap_buffers_) {
    // TODO(cwallez@chromium.org) return an error texture.
    return nullptr;
  }

  // Calling getCurrentTexture returns a texture that is valid until the
  // animation frame it gets presented. If getCurrenTexture is called multiple
  // time, the same texture should be returned. |texture_| is set to null when
  // presented so that we know we should create a new one.
  if (texture_) {
    return texture_;
  }

  WGPUTexture dawn_client_texture =
      swap_buffers_->GetNewTexture(context_->CanvasSize());
  DCHECK(dawn_client_texture);
  texture_ =
      MakeGarbageCollected<GPUTexture>(device_, dawn_client_texture, format_);
  return texture_;
}

// WebGPUSwapBufferProvider::Client implementation
void GPUSwapChain::OnTextureTransferred() {
  DCHECK(texture_);
  texture_ = nullptr;
}

}  // namespace blink
