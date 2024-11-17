// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_MF_VIDEO_PROCESSOR_ACCELERATOR_H_
#define MEDIA_GPU_WINDOWS_MF_VIDEO_PROCESSOR_ACCELERATOR_H_

#include <mfapi.h>
#include <mfidl.h>

#include "base/memory/scoped_refptr.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_preferences.h"
#include "media/base/media_log.h"
#include "media/base/video_color_space.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/base/win/dxgi_device_manager.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/windows/d3d_com_defs.h"

namespace media {

// Wrapper for Media Foundation's video processor.  Supports color space
// conversion and resize on the GPU.
// Internally, the video processor will use D3D video processing when
// available, and shaders when not.  Currently this implementation is
// designed to work directly with MediaFoundationVideoEncodeAccelerator,
// so it outputs Media Foundation IMFSample and not media::VideoFrame.
class MEDIA_GPU_EXPORT MediaFoundationVideoProcessorAccelerator {
 public:
  struct MEDIA_GPU_EXPORT Config {
    VideoPixelFormat input_format;
    gfx::Size input_visible_size;
    gfx::ColorSpace input_color_space;

    VideoPixelFormat output_format;
    gfx::Size output_visible_size;
    gfx::ColorSpace output_color_space;
  };

  explicit MediaFoundationVideoProcessorAccelerator(
      const gpu::GpuPreferences& gpu_preferences,
      const gpu::GpuDriverBugWorkarounds& gpu_workarounds);
  ~MediaFoundationVideoProcessorAccelerator();

  MediaFoundationVideoProcessorAccelerator(
      const MediaFoundationVideoProcessorAccelerator&) = delete;
  MediaFoundationVideoProcessorAccelerator& operator=(
      const MediaFoundationVideoProcessorAccelerator&) = delete;

  bool Initialize(const Config& config,
                  scoped_refptr<DXGIDeviceManager> dxgi_device_manager,
                  std::unique_ptr<MediaLog> media_log);

  HRESULT Convert(scoped_refptr<VideoFrame> frame, IMFSample** sample_out);
  HRESULT Convert(IMFSample* sample, IMFSample** sample_out);

 private:
  bool InitializeVideoProcessor(const Config& config);
  HRESULT AdjustInputSizeIfNeeded(IMFSample* sample);

  std::unique_ptr<MediaLog> media_log_;

  ComMFTransform video_processor_;
  ComMFMediaType input_media_type_;

  scoped_refptr<DXGIDeviceManager> dxgi_device_manager_;

  gpu::GpuDriverBugWorkarounds workarounds_;
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_MF_VIDEO_PROCESSOR_ACCELERATOR_H_
