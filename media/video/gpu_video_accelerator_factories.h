// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_GPU_VIDEO_ACCELERATOR_FACTORIES_H_
#define MEDIA_VIDEO_GPU_VIDEO_ACCELERATOR_FACTORIES_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/optional.h"
#include "base/unguessable_token.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "media/base/media_export.h"
#include "media/base/overlay_info.h"
#include "media/base/supported_video_decoder_config.h"
#include "media/base/video_decoder.h"
#include "media/base/video_types.h"
#include "media/video/video_encode_accelerator.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace gfx {
class ColorSpace;
class Size;
}

namespace gpu {
class GpuMemoryBufferManager;
class SharedImageInterface;
}

namespace viz {
class RasterContextProvider;
}  // namespace viz

namespace media {

class MediaLog;

// Helper interface for specifying factories needed to instantiate a hardware
// video accelerator.
// Threading model:
// * The GpuVideoAcceleratorFactories may be constructed on any thread.
// * The GpuVideoAcceleratorFactories has an associated message loop, which may
//   be retrieved as |GetMessageLoop()|.
// * All calls to the Factories after construction must be made on its message
//   loop, unless otherwise documented below.
class MEDIA_EXPORT GpuVideoAcceleratorFactories {
 public:
  enum class OutputFormat {
    UNDEFINED = 0,    // Unset state
    I420,             // 3 x R8 GMBs
    NV12_SINGLE_GMB,  // One NV12 GMB
    NV12_DUAL_GMB,    // One R8, one RG88 GMB
    XR30,             // 10:10:10:2 BGRX in one GMB (Usually Mac)
    XB30,             // 10:10:10:2 RGBX in one GMB
    RGBA,             // One 8:8:8:8 RGBA
    BGRA,             // One 8:8:8:8 BGRA (Usually Mac)
    P010,             // One P010 GMB.
  };

  enum class Supported {
    kFalse = 0,
    kTrue,
    kUnknown,
  };

  // Return whether GPU encoding/decoding is enabled.
  virtual bool IsGpuVideoAcceleratorEnabled() = 0;

  // Return the channel token, or an empty token if the channel is unusable.
  virtual base::UnguessableToken GetChannelToken() = 0;

  // Returns the |route_id| of the command buffer, or 0 if there is none.
  virtual int32_t GetCommandBufferRouteId() = 0;

  // Returns Supported::kTrue if |config| is supported by a decoder created with
  // CreateVideoDecoder() using |implementation|. Returns Supported::kMaybe if
  // it's not known at this time whether |config| is supported or not. Returns
  // Supported::kFalse if |config| is not supported.
  //
  // May be called on any thread.
  //
  // TODO(sandersd): Switch to bool if/when all clients check
  // IsDecoderSupportKnown().
  virtual Supported IsDecoderConfigSupported(
      VideoDecoderImplementation implementation,
      const VideoDecoderConfig& config) = 0;

  // Helper function that merges IsDecoderConfigSupported() results across all
  // VideoDecoderImplementations. Returns kTrue if any of the implementations
  // support the config.
  //
  // Callers must verify IsDecoderSupportKnown() prior to using this, or they
  // will immediately receive a kUnknown.
  //
  // May be called on any thread.
  Supported IsDecoderConfigSupported(const VideoDecoderConfig& config);

  // Returns true if IsDecoderConfigSupported() is ready to answer queries.
  // Once decoder support is known, it remains known for the lifetime of |this|.
  //
  // May be called on any thread.
  virtual bool IsDecoderSupportKnown() = 0;

  // Registers a callback to be notified when IsDecoderConfigSupported() is
  // ready to answer queries. The callback will be invoked on the caller's
  // sequence.
  //
  // There is no way to unsubscribe a callback, it is recommended to use a
  // WeakPtr if you need this feature.
  //
  // May be called on any thread.
  virtual void NotifyDecoderSupportKnown(base::OnceClosure callback) = 0;

  virtual std::unique_ptr<media::VideoDecoder> CreateVideoDecoder(
      MediaLog* media_log,
      VideoDecoderImplementation implementation,
      RequestOverlayInfoCB request_overlay_info_cb) = 0;

  // Returns the supported codec profiles of video encode accelerator.
  // Returns nullopt if GpuVideoAcceleratorFactories don't know the VEA
  // supported profiles.
  //
  // May be called on any thread.
  //
  // TODO(sandersd): Remove Optional if/when all clients check
  // IsEncoderSupportKnown().
  virtual base::Optional<VideoEncodeAccelerator::SupportedProfiles>
  GetVideoEncodeAcceleratorSupportedProfiles() = 0;

  // Returns true if GetVideoEncodeAcceleratorSupportedProfiles() is populated.
  // Once encoder support is known, it remains known for the lifetime of |this|.
  //
  // May be called on any thread.
  virtual bool IsEncoderSupportKnown() = 0;

  // Registers a callback to be notified when
  // GetVideoEncodeAcceleratorSupportedProfiles() has been populated. The
  // callback will be invoked on the caller's sequence.
  //
  // There is no way to unsubscribe a callback, it is recommended to use a
  // WeakPtr if you need this feature.
  //
  // May be called on any thread.
  virtual void NotifyEncoderSupportKnown(base::OnceClosure callback) = 0;

  // Caller owns returned pointer, but should call Destroy() on it (instead of
  // directly deleting) for proper destruction, as per the
  // VideoEncodeAccelerator interface.
  virtual std::unique_ptr<VideoEncodeAccelerator>
  CreateVideoEncodeAccelerator() = 0;

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

  // Returns a SharedImageInterface that can be used (on any thread) to allocate
  // and update shared images.
  // nullptr will be returned in cases where a context couldn't be created or
  // the context was lost.
  virtual gpu::SharedImageInterface* SharedImageInterface() = 0;

  // Returns the GpuMemoryBufferManager that is used to allocate
  // GpuMemoryBuffers. May return null if
  // ShouldUseGpuMemoryBuffersForVideoFrames return false.
  virtual gpu::GpuMemoryBufferManager* GpuMemoryBufferManager() = 0;

  // Allocate & return an unsafe shared memory region
  virtual base::UnsafeSharedMemoryRegion CreateSharedMemoryRegion(
      size_t size) = 0;

  // Returns the task runner the video accelerator runs on.
  virtual scoped_refptr<base::SequencedTaskRunner> GetTaskRunner() = 0;

  virtual viz::RasterContextProvider* GetMediaContextProvider() = 0;

  // Sets or gets the current pipeline rendering color space.
  virtual void SetRenderingColorSpace(const gfx::ColorSpace& color_space) = 0;
  virtual const gfx::ColorSpace& GetRenderingColorSpace() const = 0;

  virtual ~GpuVideoAcceleratorFactories() = default;
};

}  // namespace media

#endif  // MEDIA_VIDEO_GPU_VIDEO_ACCELERATOR_FACTORIES_H_
