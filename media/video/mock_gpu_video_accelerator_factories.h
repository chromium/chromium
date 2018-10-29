// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_MOCK_GPU_VIDEO_ACCELERATOR_FACTORIES_H_
#define MEDIA_VIDEO_MOCK_GPU_VIDEO_ACCELERATOR_FACTORIES_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "media/video/video_decode_accelerator.h"
#include "media/video/video_encode_accelerator.h"
#include "services/ws/public/cpp/gpu/context_provider_command_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace base {
class SharedMemory;
}

namespace media {

class MockGpuVideoAcceleratorFactories : public GpuVideoAcceleratorFactories {
 public:
  explicit MockGpuVideoAcceleratorFactories(gpu::gles2::GLES2Interface* gles2);
  ~MockGpuVideoAcceleratorFactories() override;

  bool IsGpuVideoAcceleratorEnabled() override;

  MOCK_METHOD0(GetChannelToken, base::UnguessableToken());
  MOCK_METHOD0(GetCommandBufferRouteId, int32_t());

  MOCK_METHOD1(IsDecoderConfigSupported, bool(const VideoDecoderConfig&));
  MOCK_METHOD3(CreateVideoDecoder,
               std::unique_ptr<media::VideoDecoder>(MediaLog*,
                                                    const RequestOverlayInfoCB&,
                                                    const gfx::ColorSpace&));

  // CreateVideo{Decode,Encode}Accelerator returns scoped_ptr, which the mocking
  // framework does not want.  Trampoline them.
  MOCK_METHOD0(DoCreateVideoDecodeAccelerator, VideoDecodeAccelerator*());
  MOCK_METHOD0(DoCreateVideoEncodeAccelerator, VideoEncodeAccelerator*());

  MOCK_METHOD5(CreateTextures,
               bool(int32_t count,
                    const gfx::Size& size,
                    std::vector<uint32_t>* texture_ids,
                    std::vector<gpu::Mailbox>* texture_mailboxes,
                    uint32_t texture_target));
  MOCK_METHOD1(DeleteTexture, void(uint32_t texture_id));
  MOCK_METHOD0(CreateSyncToken, gpu::SyncToken());
  MOCK_METHOD1(WaitSyncToken, void(const gpu::SyncToken& sync_token));
  MOCK_METHOD2(SignalSyncToken,
               void(const gpu::SyncToken& sync_token,
                    base::OnceClosure callback));
  MOCK_METHOD0(ShallowFlushCHROMIUM, void());
  MOCK_METHOD0(GetTaskRunner, scoped_refptr<base::SingleThreadTaskRunner>());
  MOCK_METHOD0(GetVideoDecodeAcceleratorCapabilities,
               VideoDecodeAccelerator::Capabilities());
  MOCK_METHOD0(GetVideoEncodeAcceleratorSupportedProfiles,
               VideoEncodeAccelerator::SupportedProfiles());
  MOCK_METHOD0(GetMediaContextProvider,
               scoped_refptr<ws::ContextProviderCommandBuffer>());
  MOCK_METHOD1(SetRenderingColorSpace, void(const gfx::ColorSpace&));

  std::unique_ptr<gfx::GpuMemoryBuffer> CreateGpuMemoryBuffer(
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage) override;

  bool ShouldUseGpuMemoryBuffersForVideoFrames(
      bool for_media_stream) const override;
  unsigned ImageTextureTarget(gfx::BufferFormat format) override;
  OutputFormat VideoFrameOutputFormat(VideoPixelFormat pixel_format) override {
    return video_frame_output_format_;
  };

  gpu::gles2::GLES2Interface* ContextGL() override { return gles2_; }

  void SetVideoFrameOutputFormat(const OutputFormat video_frame_output_format) {
    video_frame_output_format_ = video_frame_output_format;
  };

  void SetFailToAllocateGpuMemoryBufferForTesting(bool fail) {
    fail_to_allocate_gpu_memory_buffer_ = fail;
  }

  void SetGpuMemoryBuffersInUseByMacOSWindowServer(bool in_use);

  std::unique_ptr<base::SharedMemory> CreateSharedMemory(size_t size) override;

  std::unique_ptr<VideoDecodeAccelerator> CreateVideoDecodeAccelerator()
      override;

  std::unique_ptr<VideoEncodeAccelerator> CreateVideoEncodeAccelerator()
      override;

  gpu::gles2::GLES2Interface* GetGLES2Interface() { return gles2_; }

  const std::vector<gfx::GpuMemoryBuffer*>& created_memory_buffers() {
    return created_memory_buffers_;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockGpuVideoAcceleratorFactories);

  base::Lock lock_;
  OutputFormat video_frame_output_format_ = OutputFormat::I420;

  bool fail_to_allocate_gpu_memory_buffer_ = false;

  gpu::gles2::GLES2Interface* gles2_;

  std::vector<gfx::GpuMemoryBuffer*> created_memory_buffers_;
};

}  // namespace media

#endif  // MEDIA_VIDEO_MOCK_GPU_VIDEO_ACCELERATOR_FACTORIES_H_
