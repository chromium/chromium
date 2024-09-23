// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_MOCK_GPU_VIDEO_ACCELERATOR_FACTORIES_H_
#define MEDIA_VIDEO_MOCK_GPU_VIDEO_ACCELERATOR_FACTORIES_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "gpu/ipc/common/gpu_channel.mojom.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "media/video/video_encode_accelerator.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

class MockGpuVideoAcceleratorFactories : public GpuVideoAcceleratorFactories {
 public:
  explicit MockGpuVideoAcceleratorFactories(gpu::SharedImageInterface* sii);

  MockGpuVideoAcceleratorFactories(const MockGpuVideoAcceleratorFactories&) =
      delete;
  MockGpuVideoAcceleratorFactories& operator=(
      const MockGpuVideoAcceleratorFactories&) = delete;

  ~MockGpuVideoAcceleratorFactories() override;

  bool IsGpuVideoDecodeAcceleratorEnabled() override;
  bool IsGpuVideoEncodeAcceleratorEnabled() override;

  MOCK_METHOD1(GetChannelToken,
               void(gpu::mojom::GpuChannel::GetChannelTokenCallback));
  MOCK_METHOD0(GetCommandBufferRouteId, int32_t());

  MOCK_METHOD1(IsDecoderConfigSupported, Supported(const VideoDecoderConfig&));
  MOCK_METHOD0(GetDecoderType, VideoDecoderType());
  MOCK_METHOD0(IsDecoderSupportKnown, bool());
  MOCK_METHOD1(NotifyDecoderSupportKnown, void(base::OnceClosure));
  MOCK_METHOD2(CreateVideoDecoder,
               std::unique_ptr<media::VideoDecoder>(MediaLog*,
                                                    RequestOverlayInfoCB));

  MOCK_METHOD0(GetVideoEncodeAcceleratorSupportedProfiles,
               std::optional<VideoEncodeAccelerator::SupportedProfiles>());
  MOCK_METHOD0(IsEncoderSupportKnown, bool());
  MOCK_METHOD1(NotifyEncoderSupportKnown, void(base::OnceClosure));
  // CreateVideoEncodeAccelerator returns scoped_ptr, which the mocking
  // framework does not want. Trampoline it.
  MOCK_METHOD0(DoCreateVideoEncodeAccelerator, VideoEncodeAccelerator*());

  MOCK_METHOD0(GetTaskRunner, scoped_refptr<base::SequencedTaskRunner>());
  MOCK_METHOD0(GetMediaContextProvider, viz::RasterContextProvider*());
  MOCK_METHOD0(ContextCapabilities, gpu::Capabilities*());
  MOCK_METHOD1(SetRenderingColorSpace, void(const gfx::ColorSpace&));
  MOCK_CONST_METHOD0(GetRenderingColorSpace, const gfx::ColorSpace&());

  std::unique_ptr<gfx::GpuMemoryBuffer> CreateGpuMemoryBuffer(
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage) override;

  bool ShouldUseGpuMemoryBuffersForVideoFrames(
      bool for_media_stream) const override;
  OutputFormat VideoFrameOutputFormat(VideoPixelFormat pixel_format) override {
    return video_frame_output_format_;
  }

  gpu::SharedImageInterface* SharedImageInterface() override { return sii_; }
  gpu::GpuMemoryBufferManager* GpuMemoryBufferManager() override {
    return nullptr;
  }

  void SetVideoFrameOutputFormat(const OutputFormat video_frame_output_format) {
    video_frame_output_format_ = video_frame_output_format;
  }

  void SetFailToAllocateGpuMemoryBufferForTesting(bool fail) {
    fail_to_allocate_gpu_memory_buffer_ = fail;
  }

  void SetFailToMapGpuMemoryBufferForTesting(bool fail) {
    fail_to_map_gpu_memory_buffer_ = fail;
  }

  void SetGpuMemoryBuffersInUseByMacOSWindowServer(bool in_use);

  // Allocate & return a read-only shared memory region
  base::UnsafeSharedMemoryRegion CreateSharedMemoryRegion(size_t size) override;

  std::unique_ptr<VideoEncodeAccelerator> CreateVideoEncodeAccelerator()
      override;

  const std::vector<raw_ptr<gfx::GpuMemoryBuffer, VectorExperimental>>&
  created_memory_buffers() {
    return created_memory_buffers_;
  }

 private:
  base::Lock lock_;
  OutputFormat video_frame_output_format_ = OutputFormat::YV12;

  bool fail_to_allocate_gpu_memory_buffer_ = false;

  bool fail_to_map_gpu_memory_buffer_ = false;

  raw_ptr<gpu::SharedImageInterface> sii_;

  std::vector<raw_ptr<gfx::GpuMemoryBuffer, VectorExperimental>>
      created_memory_buffers_;
};

}  // namespace media

#endif  // MEDIA_VIDEO_MOCK_GPU_VIDEO_ACCELERATOR_FACTORIES_H_
