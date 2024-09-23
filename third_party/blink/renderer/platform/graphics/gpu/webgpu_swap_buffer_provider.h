// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_WEBGPU_SWAP_BUFFER_PROVIDER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_WEBGPU_SWAP_BUFFER_PROVIDER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "cc/layers/texture_layer.h"
#include "cc/layers/texture_layer_client.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/client/shared_image_pool.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "third_party/blink/renderer/platform/graphics/gpu/dawn_control_client_holder.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_mailbox_texture.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types_3d.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_video_frame_pool.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/hdr_metadata.h"

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
    // transfer to the compositor/video frame
    virtual void OnTextureTransferred() = 0;
    virtual void SetNeedsCompositingUpdate() = 0;
  };

  WebGPUSwapBufferProvider(
      Client* client,
      scoped_refptr<DawnControlClientHolder> dawn_control_client,
      const wgpu::Device& device,
      wgpu::TextureUsage usage,
      wgpu::TextureUsage internal_usage,
      wgpu::TextureFormat format,
      PredefinedColorSpace color_space,
      const gfx::HDRMetadata& hdr_metadata);
  ~WebGPUSwapBufferProvider() override;

  viz::SharedImageFormat Format() const;
  gfx::Size Size() const;
  cc::Layer* CcLayer();
  void SetFilterQuality(cc::PaintFlags::FilterQuality);
  void Neuter();
  void DiscardCurrentSwapBuffer();
  scoped_refptr<WebGPUMailboxTexture> GetNewTexture(
      const wgpu::TextureDescriptor& desc,
      SkAlphaType alpha_type);

  // Copy swapchain's texture to a video frame.
  // This happens at the end of an animation frame. Dawn's access to the
  // texture will be released in order for the texture to be processed by
  // WebGraphicsContext3DVideoFramePool context. Attempting to use the texture
  // via WebGPU/Dawn API in JS after this point won't work.
  //
  // These are typical sequential steps of an animation frame:
  // 1. start frame:
  //     - WebGPUSwapBufferProvider::GetNewTexture()
  // 2. client draws to the swap chain's texture using WebGPU APIs.
  // 3. finalize frame:
  //    (if client uses canvas.captureStream())
  //     - WebGPUSwapBufferProvider::CopyToVideoFrame()
  //         - ReleaseWGPUTextureAccessIfNeeded()
  //         - copy swap chain's texture to a video frame using another graphics
  //         context.
  // 4. TextureLayer::Update()
  //     - WebGPUSwapBufferProvider::PrepareTransferableResource()
  //         - ReleaseWGPUTextureAccessIfNeeded() (if not already done)
  //         - current_swap_buffer_'s ownership is transferred to caller.
  bool CopyToVideoFrame(
      WebGraphicsContext3DVideoFramePool* frame_pool,
      SourceDrawingBuffer src_buffer,
      const gfx::ColorSpace& dst_color_space,
      WebGraphicsContext3DVideoFramePool::FrameReadyCallback callback);

  scoped_refptr<WebGPUMailboxTexture> GetLastWebGPUMailboxTexture() const;

  base::WeakPtr<WebGraphicsContext3DProviderWrapper> GetContextProviderWeakPtr()
      const;

  // cc::TextureLayerClient implementation.
  bool PrepareTransferableResource(
      cc::SharedBitmapIdRegistrar* bitmap_registrar,
      viz::TransferableResource* out_resource,
      viz::ReleaseCallback* out_release_callback) override;

  // Gets the appropriate SharedImage usages to add when a SharedImage that will
  // be used with WebGPU will additionally be sent to the display.
  gpu::SharedImageUsageSet GetSharedImageUsagesForDisplay();

  scoped_refptr<gpu::ClientSharedImage> GetCurrentSharedImage();

  gpu::Mailbox GetCurrentMailboxForTesting() const;

 private:
  // Holds resources and synchronization for one of the swapchain images.
  class SwapBuffer : public gpu::ClientImage {
   public:
    explicit SwapBuffer(scoped_refptr<gpu::ClientSharedImage> shared_image);

    scoped_refptr<WebGPUMailboxTexture> mailbox_texture;

   protected:
    friend class RefCounted<SwapBuffer>;
    ~SwapBuffer() override;
  };

  void MailboxReleased(scoped_refptr<SwapBuffer> swap_buffer,
                       const gpu::SyncToken& sync_token,
                       bool lost_resource);

  // This method will dissociate current Dawn Texture (produced by
  // GetNewTexture()) from the mailbox so that the mailbox can be used by other
  // components (Compositor/Skia).
  // After this method returns, Dawn won't be able to access the mailbox
  // anymore.
  void ReleaseWGPUTextureAccessIfNeeded();

  scoped_refptr<DawnControlClientHolder> dawn_control_client_;
  raw_ptr<Client> client_;
  wgpu::Device device_;
  scoped_refptr<cc::TextureLayer> layer_;
  bool neutered_ = false;
  const viz::SharedImageFormat shared_image_format_;
  const wgpu::TextureFormat format_;
  const wgpu::TextureUsage usage_;
  const wgpu::TextureUsage internal_usage_;
  const PredefinedColorSpace color_space_;
  const gfx::HDRMetadata hdr_metadata_;
  cc::PaintFlags::FilterQuality filter_quality_ =
      cc::PaintFlags::FilterQuality::kLow;
  int max_texture_size_;

  // Pool of SwapBuffers which manages creation, release and recycling of
  // SwapBuffer resources.
  std::unique_ptr<gpu::SharedImagePool<SwapBuffer>> swap_buffer_pool_;
  scoped_refptr<SwapBuffer> last_swap_buffer_;
  scoped_refptr<SwapBuffer> current_swap_buffer_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_WEBGPU_SWAP_BUFFER_PROVIDER_H_
