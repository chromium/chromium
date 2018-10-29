// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_RENDERERS_GPU_VIDEO_ACCELERATOR_FACTORIES_H_
#define MEDIA_RENDERERS_GPU_VIDEO_ACCELERATOR_FACTORIES_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/unguessable_token.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "media/base/media_export.h"
#include "media/base/overlay_info.h"
#include "media/base/video_decoder.h"
#include "media/base/video_types.h"
#include "media/video/video_decode_accelerator.h"
#include "media/video/video_encode_accelerator.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace base {
class SingleThreadTaskRunner;
class SharedMemory;
}  // namespace base

namespace gfx {
class ColorSpace;
class Size;
}

namespace gpu {
struct SyncToken;
}

namespace ws {
class ContextProviderCommandBuffer;
}  // namespace ws

namespace media {

class MediaLog;
class VideoDecodeAccelerator;

// Helper interface for specifying factories needed to instantiate a hardware
// video accelerator.
// Threading model:
// * The GpuVideoAcceleratorFactories may be constructed on any thread.
// * The GpuVideoAcceleratorFactories has an associated message loop, which may
//   be retrieved as |GetMessageLoop()|.
// * All calls to the Factories after construction must be made on its message
//   loop.
class MEDIA_EXPORT GpuVideoAcceleratorFactories {
 public:
  enum class OutputFormat {
    UNDEFINED = 0,    // Unset state
    I420,             // 3 x R8 GMBs
    UYVY,             // One 422 GMB
    NV12_SINGLE_GMB,  // One NV12 GMB
    NV12_DUAL_GMB,    // One R8, one RG88 GMB
    XR30,             // 10:10:10:2 BGRX in one GMB (Usually Mac)
    XB30,             // 10:10:10:2 RGBX in one GMB
    RGBA,             // One 8:8:8:8 RGBA
    BGRA,             // One 8:8:8:8 BGRA (Usually Mac)
  };

  // Return whether GPU encoding/decoding is enabled.
  virtual bool IsGpuVideoAcceleratorEnabled() = 0;

  // Return the channel token, or an empty token if the channel is unusable.
  virtual base::UnguessableToken GetChannelToken() = 0;

  // Returns the |route_id| of the command buffer, or 0 if there is none.
  virtual int32_t GetCommandBufferRouteId() = 0;

  // Return true if |config| is potentially supported by a decoder created with
  // CreateVideoDecoder().
  virtual bool IsDecoderConfigSupported(const VideoDecoderConfig& config) = 0;

  virtual std::unique_ptr<media::VideoDecoder> CreateVideoDecoder(
      MediaLog* media_log,
      const RequestOverlayInfoCB& request_overlay_info_cb,
      const gfx::ColorSpace& target_color_space) = 0;

  // Caller owns returned pointer, but should call Destroy() on it (instead of
  // directly deleting) for proper destruction, as per the
  // VideoDecodeAccelerator interface.
  virtual std::unique_ptr<VideoDecodeAccelerator>
  CreateVideoDecodeAccelerator() = 0;

  // Caller owns returned pointer, but should call Destroy() on it (instead of
  // directly deleting) for proper destruction, as per the
  // VideoEncodeAccelerator interface.
  virtual std::unique_ptr<VideoEncodeAccelerator>
  CreateVideoEncodeAccelerator() = 0;

  // Allocate & delete native textures.
  virtual bool CreateTextures(int32_t count,
                              const gfx::Size& size,
                              std::vector<uint32_t>* texture_ids,
                              std::vector<gpu::Mailbox>* texture_mailboxes,
                              uint32_t texture_target) = 0;
  virtual void DeleteTexture(uint32_t texture_id) = 0;
  virtual gpu::SyncToken CreateSyncToken() = 0;
  virtual void ShallowFlushCHROMIUM() = 0;

  virtual void WaitSyncToken(const gpu::SyncToken& sync_token) = 0;
  virtual void SignalSyncToken(const gpu::SyncToken& sync_token,
                               base::OnceClosure callback) = 0;

  virtual std::unique_ptr<gfx::GpuMemoryBuffer> CreateGpuMemoryBuffer(
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage) = 0;

  // |for_media_stream| specifies webrtc use case of media streams.
  virtual bool ShouldUseGpuMemoryBuffersForVideoFrames(
      bool for_media_stream) const = 0;

  // The GLContextLock must be taken when calling this.
  virtual unsigned ImageTextureTarget(gfx::BufferFormat format) = 0;

  // Pixel format of the hardware video frames created when GpuMemoryBuffers
  // video frames are enabled.
  virtual OutputFormat VideoFrameOutputFormat(
      VideoPixelFormat pixel_format) = 0;

  // Returns a GL Context that can be used on the task runner associated with
  // the same instance of GpuVideoAcceleratorFactories.
  // nullptr will be returned in cases where a context couldn't be created or
  // the context was lost.
  virtual gpu::gles2::GLES2Interface* ContextGL() = 0;

  // Allocate & return a shared memory segment.
  virtual std::unique_ptr<base::SharedMemory> CreateSharedMemory(
      size_t size) = 0;

  // Returns the task runner the video accelerator runs on.
  virtual scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() = 0;

  // Return the capabilities of video decode accelerator, which includes the
  // supported codec profiles.
  virtual VideoDecodeAccelerator::Capabilities
  GetVideoDecodeAcceleratorCapabilities() = 0;

  // Returns the supported codec profiles of video encode accelerator.
  virtual VideoEncodeAccelerator::SupportedProfiles
  GetVideoEncodeAcceleratorSupportedProfiles() = 0;

  virtual scoped_refptr<ws::ContextProviderCommandBuffer>
  GetMediaContextProvider() = 0;

  // Sets the current pipeline rendering color space.
  virtual void SetRenderingColorSpace(const gfx::ColorSpace& color_space) = 0;

  virtual ~GpuVideoAcceleratorFactories() = default;
};

}  // namespace media

#endif  // MEDIA_RENDERERS_GPU_VIDEO_ACCELERATOR_FACTORIES_H_
