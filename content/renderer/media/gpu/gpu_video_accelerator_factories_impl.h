// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_GPU_GPU_VIDEO_ACCELERATOR_FACTORIES_IMPL_H_
#define CONTENT_RENDERER_MEDIA_GPU_GPU_VIDEO_ACCELERATOR_FACTORIES_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "components/viz/common/gpu/context_lost_observer.h"
#include "content/common/content_export.h"
#include "content/renderer/media/codec_factory.h"
#include "media/mojo/buildflags.h"
#include "media/video/gpu_video_accelerator_factories.h"

#if BUILDFLAG(IS_FUCHSIA)
#include <fuchsia/mediacodec/cpp/fidl.h>
#endif

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace gpu {
class GpuChannelHost;
class GpuMemoryBufferManager;
}  // namespace gpu

namespace viz {
class ContextProviderCommandBuffer;
}  // namespace viz

namespace content {

// Glue code to expose functionality needed by media::GpuVideoAccelerator to
// RenderViewImpl.  This class is entirely an implementation detail of
// RenderViewImpl and only has its own header to allow extraction of its
// implementation from render_view_impl.cc which is already far too large.
//
// The GpuVideoAcceleratorFactoriesImpl can be constructed on any thread,
// but subsequent calls to all public methods of the class must be called from
// the |task_runner_|, as provided during construction.
// |context_provider| should not support locking and will be bound to
// |task_runner_| where all the operations on the context should also happen.
class CONTENT_EXPORT GpuVideoAcceleratorFactoriesImpl
    : public media::GpuVideoAcceleratorFactories,
      public viz::ContextLostObserver {
 public:
  // Takes a ref on |gpu_channel_host| and tests |context| for loss before each
  // use.  Safe to call from any thread.
  static std::unique_ptr<GpuVideoAcceleratorFactoriesImpl> Create(
      scoped_refptr<gpu::GpuChannelHost> gpu_channel_host,
      const scoped_refptr<base::SequencedTaskRunner>& main_thread_task_runner,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      const scoped_refptr<viz::ContextProviderCommandBuffer>& context_provider,
      std::unique_ptr<CodecFactory> codec_factory,
      bool enable_video_gpu_memory_buffers,
      bool enable_media_stream_gpu_memory_buffers,
      bool enable_video_decode_accelerator,
      bool enable_video_encode_accelerator);
  static std::unique_ptr<GpuVideoAcceleratorFactoriesImpl> CreateForTesting(
      scoped_refptr<gpu::GpuChannelHost> gpu_channel_host,
      const scoped_refptr<base::SequencedTaskRunner>& main_thread_task_runner,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      const scoped_refptr<viz::ContextProviderCommandBuffer>& context_provider,
      std::unique_ptr<CodecFactory> codec_factory,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      bool enable_video_gpu_memory_buffers,
      bool enable_media_stream_gpu_memory_buffers,
      bool enable_video_decode_accelerator,
      bool enable_video_encode_accelerator);

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
  void NotifyDecoderSupportKnown(base::OnceClosure callback) override;
  std::unique_ptr<media::VideoDecoder> CreateVideoDecoder(
      media::MediaLog* media_log,
      media::RequestOverlayInfoCB request_overlay_info_cb) override;
  std::optional<media::VideoEncodeAccelerator::SupportedProfiles>
  GetVideoEncodeAcceleratorSupportedProfiles() override;
  bool IsEncoderSupportKnown() override;
  void NotifyEncoderSupportKnown(base::OnceClosure callback) override;
  std::unique_ptr<media::VideoEncodeAccelerator> CreateVideoEncodeAccelerator()
      override;

  std::unique_ptr<gfx::GpuMemoryBuffer> CreateGpuMemoryBuffer(
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage) override;

  bool ShouldUseGpuMemoryBuffersForVideoFrames(
      bool for_media_stream) const override;
  OutputFormat VideoFrameOutputFormat(
      media::VideoPixelFormat pixel_format) override;

  // Called on the media thread. Returns the SharedImageInterface unless the
  // ContextProvider has been lost, in which case it returns null.
  gpu::SharedImageInterface* SharedImageInterface() override;
  gpu::GpuMemoryBufferManager* GpuMemoryBufferManager() override;
  // Called on the media thread. Verifies if the ContextProvider is lost and
  // notifies the main thread of loss if it has occured, which can be seen later
  // from CheckContextProviderLost().
  bool CheckContextLost();
  // Called on the media thread. Destroys the ContextProvider held in this
  // class. Should only be called if the ContextProvider was previously lost,
  // and this class will no longer be used, as it assumes a ContextProvider is
  // present otherwise.
  void DestroyContext();
  base::UnsafeSharedMemoryRegion CreateSharedMemoryRegion(size_t size) override;
  scoped_refptr<base::SequencedTaskRunner> GetTaskRunner() override;

  viz::RasterContextProvider* GetMediaContextProvider() override;

  const gpu::Capabilities* ContextCapabilities() override;

  void SetRenderingColorSpace(const gfx::ColorSpace& color_space) override;
  const gfx::ColorSpace& GetRenderingColorSpace() const override;

  // Called on the main thread. Returns whether the media thread has seen the
  // ContextProvider become lost, in which case this class should be replaced
  // with a new ContextProvider.
  bool CheckContextProviderLostOnMainThread();

  GpuVideoAcceleratorFactoriesImpl(const GpuVideoAcceleratorFactoriesImpl&) =
      delete;
  GpuVideoAcceleratorFactoriesImpl& operator=(
      const GpuVideoAcceleratorFactoriesImpl&) = delete;

  ~GpuVideoAcceleratorFactoriesImpl() override;

 private:
  GpuVideoAcceleratorFactoriesImpl(
      scoped_refptr<gpu::GpuChannelHost> gpu_channel_host,
      const scoped_refptr<base::SequencedTaskRunner>& main_thread_task_runner,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      const scoped_refptr<viz::ContextProviderCommandBuffer>& context_provider,
      std::unique_ptr<CodecFactory> codec_factory,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      bool enable_gpu_memory_buffer_video_frames_for_video,
      bool enable_gpu_memory_buffer_video_frames_for_media_stream,
      bool enable_video_decode_accelerator,
      bool enable_video_encode_accelerator);

  void BindOnTaskRunner();

  // viz::ContextLostObserver implementation.
  void OnContextLost() override;
  void SetContextProviderLostOnMainThread();

  void OnChannelTokenReady(const base::UnguessableToken& token);

  // Implementation of VideoFrameOutputFormat method.
  OutputFormat VideoFrameOutputFormatImpl(media::VideoPixelFormat pixel_format);

  const scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner_;
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  const scoped_refptr<gpu::GpuChannelHost> gpu_channel_host_;

  const std::unique_ptr<CodecFactory> codec_factory_;

  // Shared pointer to a shared context provider. It is initially set on main
  // thread, but all subsequent access and destruction should happen only on the
  // media thread.
  scoped_refptr<viz::ContextProviderCommandBuffer> context_provider_;
  // Signals if |context_provider_| is alive on the media thread. For use on the
  // main thread.
  bool context_provider_lost_ = false;
  // A shadow of |context_provider_lost_| for the media thread.
  bool context_provider_lost_on_media_thread_ = false;

  base::UnguessableToken channel_token_;
  base::OnceCallbackList<void(const base::UnguessableToken&)>
      channel_token_callbacks_;

  // Whether gpu memory buffers should be used to hold video frames data.
  const bool enable_video_gpu_memory_buffers_;
  const bool enable_media_stream_gpu_memory_buffers_;
  // Whether video acceleration encoding/decoding should be enabled.
  const bool video_decode_accelerator_enabled_;
  const bool video_encode_accelerator_enabled_;

  gfx::ColorSpace rendering_color_space_;

  const raw_ptr<gpu::GpuMemoryBufferManager> gpu_memory_buffer_manager_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_GPU_GPU_VIDEO_ACCELERATOR_FACTORIES_IMPL_H_
