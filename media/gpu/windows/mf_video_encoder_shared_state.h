// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_MF_VIDEO_ENCODER_SHARED_STATE_H_
#define MEDIA_GPU_WINDOWS_MF_VIDEO_ENCODER_SHARED_STATE_H_

#include <vector>

#include "base/containers/flat_map.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/windows/mf_video_encoder_util.h"
#include "media/video/video_encode_accelerator.h"
#include "ui/gfx/geometry/size.h"

namespace media {

// This class is used to share some common states among multiple instances of
// MediaFoundationVideoEncodeAccelerator, such as supported profiles, max
// supported resolutions and framerates, as well as minimum supported resolution
// of HMFTs enumerated on the platform.
class MEDIA_GPU_EXPORT MediaFoundationVideoEncoderSharedState {
 public:
  static MediaFoundationVideoEncoderSharedState* GetInstance(
      const gpu::GpuDriverBugWorkarounds& workarounds);

  MediaFoundationVideoEncoderSharedState(
      const MediaFoundationVideoEncoderSharedState&) = delete;
  MediaFoundationVideoEncoderSharedState& operator=(
      const MediaFoundationVideoEncoderSharedState&) = delete;

  const VideoEncodeAccelerator::SupportedProfiles& GetSupportedProfiles()
      const {
    return supported_profiles_;
  }

  // Returns the maximum framerate and resolution combinations supported by the
  // activate whose CLSID hash is |activate_hash|.
  const std::vector<FramerateAndResolution> GetMaxFramerateAndResolutions(
      size_t activate_hash) const;

  // Returns the minimum resolution supported by the activate whose CLSID hash
  // is |activate_hash|.
  const gfx::Size GetMinResolution(size_t activate_hash) const;

 private:
  MediaFoundationVideoEncoderSharedState(
      const gpu::GpuDriverBugWorkarounds& workarounds);
  virtual ~MediaFoundationVideoEncoderSharedState();

  void GetSupportedProfilesInternal();

  gpu::GpuDriverBugWorkarounds workarounds_;
  VideoEncodeAccelerator::SupportedProfiles supported_profiles_;
  base::flat_map<size_t, std::vector<FramerateAndResolution>>
      max_framerate_and_resolutions_;
  base::flat_map<size_t, gfx::Size> min_resolutions_;
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_MF_VIDEO_ENCODER_SHARED_STATE_H_
