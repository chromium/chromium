// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/video_capture_buffer_pool_util.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "media/base/media_switches.h"
#include "media/capture/capture_switches.h"
#include "services/video_effects/public/cpp/buildflags.h"

namespace media {

int DeviceVideoCaptureMaxBufferPoolSize() {
  // The maximum number of video frame buffers in-flight at any one time.
  // If all buffers are still in use by consumers when new frames are produced
  // those frames get dropped.
  static int max_buffer_count = kVideoCaptureDefaultMaxBufferPoolSize;

#if BUILDFLAG(IS_APPLE)
  // On macOS, we allow a few more buffers as it's routinely observed that it
  // runs out of three when just displaying 60 FPS media in a video element.
  // Also, this must be greater than the frame delay of VideoToolbox encoder
  // for AVC High Profile.
  max_buffer_count = 15;
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  // On Chrome OS with MIPI cameras running on HAL v3, there can be four
  // concurrent streams of camera pipeline depth ~6. We allow at most 36 buffers
  // here to take into account the delay caused by the consumer (e.g. display or
  // video encoder).
  if (switches::IsVideoCaptureUseGpuMemoryBufferEnabled()) {
    max_buffer_count = 36;
  }
#elif BUILDFLAG(IS_WIN)
  // On Windows, for GMB backed zero-copy more buffers are needed because it's
  // routinely observed that it runs out of default buffer count when just
  // displaying 60 FPS media in a video element

  // It's confirmed that MFCaptureEngine may terminate the capture with
  // MF_E_SAMPLEALLOCATOR_EMPTY error if more than 10 buffers are held by the
  // client. Usually there are 3-5 frames in flight, unless there's some
  // micro-freeze in the renderer or the gpu service.
  if (switches::IsVideoCaptureUseGpuMemoryBufferEnabled()) {
    max_buffer_count = 10;
  }
#endif

#if BUILDFLAG(ENABLE_VIDEO_EFFECTS)
  if (base::FeatureList::IsEnabled(media::kCameraMicEffects)) {
    // We may need 2x as many buffers if video effects are going to be applied
    // to account for the fact that each captured video frame will have
    // additional buffer allocated for post-processing result.
    max_buffer_count = max_buffer_count * 2;
  }
#endif

  return max_buffer_count;
}

}  // namespace media
