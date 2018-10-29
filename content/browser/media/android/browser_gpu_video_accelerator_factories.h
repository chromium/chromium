// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_ANDROID_BROWSER_GPU_VIDEO_ACCELERATOR_FACTORIES_H_
#define CONTENT_BROWSER_MEDIA_ANDROID_BROWSER_GPU_VIDEO_ACCELERATOR_FACTORIES_H_

#include "base/macros.h"
#include "media/video/gpu_video_accelerator_factories.h"

namespace content {

// Provides hardware video decoding contexts in the browser process. Used to
// generate video thumbnail.
class BrowserGpuVideoAcceleratorFactories
    : public media::GpuVideoAcceleratorFactories {
 public:
  explicit BrowserGpuVideoAcceleratorFactories(
      scoped_refptr<ws::ContextProviderCommandBuffer>);
  ~BrowserGpuVideoAcceleratorFactories() override;

 private:
  // media::GpuVideoAcceleratorFactories implementation.
  bool IsGpuVideoAcceleratorEnabled() override;
  base::UnguessableToken GetChannelToken() override;
  int32_t GetCommandBufferRouteId() override;
  bool IsDecoderConfigSupported(
      const media::VideoDecoderConfig& config) override;
  std::unique_ptr<media::VideoDecoder> CreateVideoDecoder(
      media::MediaLog* media_log,
      const media::RequestOverlayInfoCB& request_overlay_info_cb,
      const gfx::ColorSpace& target_color_space) override;
  std::unique_ptr<media::VideoDecodeAccelerator> CreateVideoDecodeAccelerator()
      override;
  std::unique_ptr<media::VideoEncodeAccelerator> CreateVideoEncodeAccelerator()
      override;
  bool CreateTextures(int32_t count,
                      const gfx::Size& size,
                      std::vector<uint32_t>* texture_ids,
                      std::vector<gpu::Mailbox>* texture_mailboxes,
                      uint32_t texture_target) override;
  void DeleteTexture(uint32_t texture_id) override;
  gpu::SyncToken CreateSyncToken() override;
  void ShallowFlushCHROMIUM() override;
  void WaitSyncToken(const gpu::SyncToken& sync_token) override;
  void SignalSyncToken(const gpu::SyncToken& sync_token,
                       base::OnceClosure callback) override;
  std::unique_ptr<gfx::GpuMemoryBuffer> CreateGpuMemoryBuffer(
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage) override;
  bool ShouldUseGpuMemoryBuffersForVideoFrames(
      bool for_media_stream) const override;
  unsigned ImageTextureTarget(gfx::BufferFormat format) override;
  media::GpuVideoAcceleratorFactories::OutputFormat VideoFrameOutputFormat(
      media::VideoPixelFormat pixel_format) override;
  gpu::gles2::GLES2Interface* ContextGL() override;
  std::unique_ptr<base::SharedMemory> CreateSharedMemory(size_t size) override;
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() override;
  media::VideoDecodeAccelerator::Capabilities
  GetVideoDecodeAcceleratorCapabilities() override;
  media::VideoEncodeAccelerator::SupportedProfiles
  GetVideoEncodeAcceleratorSupportedProfiles() override;
  scoped_refptr<ws::ContextProviderCommandBuffer> GetMediaContextProvider()
      override;
  void SetRenderingColorSpace(const gfx::ColorSpace& color_space) override;

  scoped_refptr<ws::ContextProviderCommandBuffer> context_provider_;
  base::UnguessableToken channel_token_;

  DISALLOW_COPY_AND_ASSIGN(BrowserGpuVideoAcceleratorFactories);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_ANDROID_BROWSER_GPU_VIDEO_ACCELERATOR_FACTORIES_H_
