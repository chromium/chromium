// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_ANDROID_BROWSER_GPU_VIDEO_ACCELERATOR_FACTORIES_H_
#define CONTENT_BROWSER_MEDIA_ANDROID_BROWSER_GPU_VIDEO_ACCELERATOR_FACTORIES_H_

#include "base/callback_list.h"
#include "base/task/sequenced_task_runner.h"
#include "media/video/gpu_video_accelerator_factories.h"

namespace viz {
class ContextProviderCommandBuffer;
}  // namespace viz

namespace content {

// Provides hardware video decoding contexts in the browser process. Used to
// generate video thumbnail.
class BrowserGpuVideoAcceleratorFactories
    : public media::GpuVideoAcceleratorFactories {
 public:
  explicit BrowserGpuVideoAcceleratorFactories(
      scoped_refptr<viz::ContextProviderCommandBuffer>);

  BrowserGpuVideoAcceleratorFactories(
      const BrowserGpuVideoAcceleratorFactories&) = delete;
  BrowserGpuVideoAcceleratorFactories& operator=(
      const BrowserGpuVideoAcceleratorFactories&) = delete;

  ~BrowserGpuVideoAcceleratorFactories() override;

 private:
  // media::GpuVideoAcceleratorFactories implementation.
  bool IsGpuVideoDecodeAcceleratorEnabled() override;
  bool IsGpuVideoEncodeAcceleratorEnabled() override;
  void GetChannelToken(
      gpu::mojom::GpuChannel::GetChannelTokenCallback cb) override;
  int32_t GetCommandBufferRouteId() override;
  Supported IsDecoderConfigSupported(
      const media::VideoDecoderConfig& config) override;
  media::VideoDecoderType GetDecoderType() override;
  bool IsDecoderSupportKnown() override;
  void NotifyDecoderSupportKnown(base::OnceClosure) override;
  std::unique_ptr<media::VideoDecoder> CreateVideoDecoder(
      media::MediaLog* media_log,
      media::RequestOverlayInfoCB request_overlay_info_cb) override;
  std::optional<media::VideoEncodeAccelerator::SupportedProfiles>
  GetVideoEncodeAcceleratorSupportedProfiles() override;
  bool IsEncoderSupportKnown() override;
  void NotifyEncoderSupportKnown(base::OnceClosure) override;
  std::unique_ptr<media::VideoEncodeAccelerator> CreateVideoEncodeAccelerator()
      override;
  std::unique_ptr<gfx::GpuMemoryBuffer> CreateGpuMemoryBuffer(
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage) override;
  bool ShouldUseGpuMemoryBuffersForVideoFrames(
      bool for_media_stream) const override;
  media::GpuVideoAcceleratorFactories::OutputFormat VideoFrameOutputFormat(
      media::VideoPixelFormat pixel_format) override;
  gpu::SharedImageInterface* SharedImageInterface() override;
  gpu::GpuMemoryBufferManager* GpuMemoryBufferManager() override;
  base::UnsafeSharedMemoryRegion CreateSharedMemoryRegion(size_t size) override;
  scoped_refptr<base::SequencedTaskRunner> GetTaskRunner() override;
  viz::RasterContextProvider* GetMediaContextProvider() override;
  const gpu::Capabilities* ContextCapabilities() override;
  void SetRenderingColorSpace(const gfx::ColorSpace& color_space) override;
  const gfx::ColorSpace& GetRenderingColorSpace() const override;

  // Called when gpu channel token is retrieved asynchronously.
  void OnChannelTokenReady(const base::UnguessableToken& token);

  scoped_refptr<viz::ContextProviderCommandBuffer> context_provider_;
  base::UnguessableToken channel_token_;
  base::OnceCallbackList<void(const base::UnguessableToken&)>
      channel_token_callbacks_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_ANDROID_BROWSER_GPU_VIDEO_ACCELERATOR_FACTORIES_H_
