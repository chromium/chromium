// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_WEBGPU_SWAP_BUFFER_PROVIDER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_WEBGPU_SWAP_BUFFER_PROVIDER_H_

#include "cc/layers/texture_layer.h"
#include "cc/layers/texture_layer_client.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "third_party/blink/renderer/platform/geometry/int_size.h"
#include "third_party/blink/renderer/platform/graphics/gpu/dawn_control_client_holder.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"

namespace blink {

// WebGPUSwapBufferProvider contains the cc::Layer used by the swapchain as well
// as the resources used for the layer. It is separate from GPUSwapChain so that
// it can be kept alive via refcounting instead of garbage collection and so it
// can live in blink_platform and use gpu:: or viz:: types.
class PLATFORM_EXPORT WebGPUSwapBufferProvider
    : public cc::TextureLayerClient,
      public RefCounted<WebGPUSwapBufferProvider> {
 public:
  class Client {
   public:
    // Called to make the WebGPU/Dawn stop accessing the texture prior to its
    // transfer to the compositor.
    virtual void OnTextureTransferred() = 0;
  };

  WebGPUSwapBufferProvider(
      Client* client,
      scoped_refptr<DawnControlClientHolder> dawn_control_client,
      WGPUDevice device,
      WGPUTextureUsage usage,
      WGPUTextureFormat format);
  ~WebGPUSwapBufferProvider() override;

  viz::ResourceFormat Format() const { return format_; }
  const gfx::Size& Size() const;
  cc::Layer* CcLayer();
  void SetFilterQuality(cc::PaintFlags::FilterQuality);
  void Neuter();
  WGPUTexture GetNewTexture(const IntSize& size);

  base::WeakPtr<WebGraphicsContext3DProviderWrapper> GetContextProviderWeakPtr()
      const;

  // cc::TextureLayerClient implementation.
  bool PrepareTransferableResource(
      cc::SharedBitmapIdRegistrar* bitmap_registrar,
      viz::TransferableResource* out_resource,
      viz::ReleaseCallback* out_release_callback) override;

  gpu::Mailbox GetCurrentMailboxForTesting() const;

 private:
  // Holds resources and synchronization for one of the swapchain images.
  struct SwapBuffer {
    SwapBuffer(
        base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider,
        gpu::Mailbox mailbox,
        gpu::SyncToken creation_token,
        gfx::Size size);
    SwapBuffer(const SwapBuffer&) = delete;
    SwapBuffer& operator=(const SwapBuffer&) = delete;
    ~SwapBuffer();

    gfx::Size size;
    gpu::Mailbox mailbox;

    // A weak ptr to the context provider so that the destructor can
    // destroy shared images.
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider;

    // A token signaled when the previous user of the image is finished using
    // it. It could be WebGPU, the compositor or the shared image creation.
    gpu::SyncToken access_finished_token;
  };

  std::unique_ptr<WebGPUSwapBufferProvider::SwapBuffer> NewOrRecycledSwapBuffer(
      gpu::SharedImageInterface* sii,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider,
      const gfx::Size& size);

  void RecycleSwapBuffer(std::unique_ptr<SwapBuffer> swap_buffer);

  void MailboxReleased(std::unique_ptr<SwapBuffer> swap_buffer,
                       const gpu::SyncToken& sync_token,
                       bool lost_resource);

  scoped_refptr<DawnControlClientHolder> dawn_control_client_;
  Client* client_;
  WGPUDevice device_;
  scoped_refptr<cc::TextureLayer> layer_;
  bool neutered_ = false;

  WGPUTextureUsage usage_;

  // The maximum number of in-flight swap-buffers waiting to be used for
  // recycling.
  static constexpr int kMaxRecycledSwapBuffers = 3;

  WTF::Vector<std::unique_ptr<SwapBuffer>> unused_swap_buffers_;

  uint32_t wire_texture_id_ = 0;
  uint32_t wire_texture_generation_ = 0;
  std::unique_ptr<SwapBuffer> current_swap_buffer_;
  viz::ResourceFormat format_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_WEBGPU_SWAP_BUFFER_PROVIDER_H_
