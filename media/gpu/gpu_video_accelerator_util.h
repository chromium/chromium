// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_GPU_VIDEO_ACCELERATOR_UTIL_H_
#define MEDIA_GPU_GPU_VIDEO_ACCELERATOR_UTIL_H_

#include "gpu/config/gpu_info.h"
#include "media/base/supported_video_decoder_config.h"
#include "media/gpu/media_gpu_export.h"
#include "media/video/video_decode_accelerator.h"
#include "media/video/video_encode_accelerator.h"

namespace media {

class MEDIA_GPU_EXPORT GpuVideoAcceleratorUtil {
 public:
  // Convert decoder gpu capabilities to media capabilities.
  static VideoDecodeAccelerator::Capabilities
  ConvertGpuToMediaDecodeCapabilities(
      const gpu::VideoDecodeAcceleratorCapabilities& gpu_capabilities);

  // Convert decoder gpu profiles to media profiles.
  static VideoDecodeAccelerator::SupportedProfiles
  ConvertGpuToMediaDecodeProfiles(
      const gpu::VideoDecodeAcceleratorSupportedProfiles& gpu_profiles);

  // Convert decoder media capabilities to gpu capabilities.
  static gpu::VideoDecodeAcceleratorCapabilities
  ConvertMediaToGpuDecodeCapabilities(
      const VideoDecodeAccelerator::Capabilities& media_capabilities);

  // Convert decoder media profiles to gpu profiles.
  static gpu::VideoDecodeAcceleratorSupportedProfiles
  ConvertMediaToGpuDecodeProfiles(
      const VideoDecodeAccelerator::SupportedProfiles& media_profiles);

  static gpu::VideoDecodeAcceleratorSupportedProfiles
  ConvertMediaConfigsToGpuDecodeProfiles(
      const SupportedVideoDecoderConfigs& configs);

  // Convert encoder gpu profiles to media profiles.
  static VideoEncodeAccelerator::SupportedProfiles
  ConvertGpuToMediaEncodeProfiles(
      const gpu::VideoEncodeAcceleratorSupportedProfiles& gpu_profiles);

  // Convert encoder media profiles to gpu profiles.
  static gpu::VideoEncodeAcceleratorSupportedProfiles
  ConvertMediaToGpuEncodeProfiles(
      const VideoEncodeAccelerator::SupportedProfiles& media_profiles);

  // Insert |new_profiles| into |media_profiles|, ensuring no duplicates are
  // inserted.
  static void InsertUniqueDecodeProfiles(
      const VideoDecodeAccelerator::SupportedProfiles& new_profiles,
      VideoDecodeAccelerator::SupportedProfiles* media_profiles);

  // Insert |new_profiles| into |media_profiles|, ensuring no duplicates are
  // inserted.
  static void InsertUniqueEncodeProfiles(
      const VideoEncodeAccelerator::SupportedProfiles& new_profiles,
      VideoEncodeAccelerator::SupportedProfiles* media_profiles);
};

}  // namespace media

#endif  // MEDIA_GPU_GPU_VIDEO_ACCELERATOR_UTIL_H_
