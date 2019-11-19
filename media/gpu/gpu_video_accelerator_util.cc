// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/gpu_video_accelerator_util.h"

namespace media {

// Make sure the enum values of VideoCodecProfile and
// gpu::VideoCodecProfile match.
#define STATIC_ASSERT_ENUM_MATCH(name)                             \
  static_assert(name == static_cast<VideoCodecProfile>(gpu::name), \
                #name " value must match in media and gpu.")

STATIC_ASSERT_ENUM_MATCH(VIDEO_CODEC_PROFILE_UNKNOWN);
STATIC_ASSERT_ENUM_MATCH(VIDEO_CODEC_PROFILE_MIN);
STATIC_ASSERT_ENUM_MATCH(H264PROFILE_BASELINE);
STATIC_ASSERT_ENUM_MATCH(H264PROFILE_MAIN);
STATIC_ASSERT_ENUM_MATCH(H264PROFILE_EXTENDED);
STATIC_ASSERT_ENUM_MATCH(H264PROFILE_HIGH);
STATIC_ASSERT_ENUM_MATCH(H264PROFILE_HIGH10PROFILE);
STATIC_ASSERT_ENUM_MATCH(H264PROFILE_HIGH422PROFILE);
STATIC_ASSERT_ENUM_MATCH(H264PROFILE_HIGH444PREDICTIVEPROFILE);
STATIC_ASSERT_ENUM_MATCH(H264PROFILE_SCALABLEBASELINE);
STATIC_ASSERT_ENUM_MATCH(H264PROFILE_SCALABLEHIGH);
STATIC_ASSERT_ENUM_MATCH(H264PROFILE_STEREOHIGH);
STATIC_ASSERT_ENUM_MATCH(H264PROFILE_MULTIVIEWHIGH);
STATIC_ASSERT_ENUM_MATCH(VP8PROFILE_ANY);
STATIC_ASSERT_ENUM_MATCH(VP9PROFILE_PROFILE0);
STATIC_ASSERT_ENUM_MATCH(VP9PROFILE_PROFILE1);
STATIC_ASSERT_ENUM_MATCH(VP9PROFILE_PROFILE2);
STATIC_ASSERT_ENUM_MATCH(VP9PROFILE_PROFILE3);
STATIC_ASSERT_ENUM_MATCH(HEVCPROFILE_MAIN);
STATIC_ASSERT_ENUM_MATCH(HEVCPROFILE_MAIN10);
STATIC_ASSERT_ENUM_MATCH(HEVCPROFILE_MAIN_STILL_PICTURE);
STATIC_ASSERT_ENUM_MATCH(AV1PROFILE_PROFILE_MAIN);
STATIC_ASSERT_ENUM_MATCH(AV1PROFILE_PROFILE_HIGH);
STATIC_ASSERT_ENUM_MATCH(AV1PROFILE_PROFILE_PRO);
STATIC_ASSERT_ENUM_MATCH(VIDEO_CODEC_PROFILE_MAX);

// static
VideoDecodeAccelerator::Capabilities
GpuVideoAcceleratorUtil::ConvertGpuToMediaDecodeCapabilities(
    const gpu::VideoDecodeAcceleratorCapabilities& gpu_capabilities) {
  VideoDecodeAccelerator::Capabilities capabilities;
  capabilities.supported_profiles =
      ConvertGpuToMediaDecodeProfiles(gpu_capabilities.supported_profiles);
  capabilities.flags = gpu_capabilities.flags;
  return capabilities;
}

// static
VideoDecodeAccelerator::SupportedProfiles
GpuVideoAcceleratorUtil::ConvertGpuToMediaDecodeProfiles(
    const gpu::VideoDecodeAcceleratorSupportedProfiles& gpu_profiles) {
  VideoDecodeAccelerator::SupportedProfiles profiles;
  for (const auto& gpu_profile : gpu_profiles) {
    VideoDecodeAccelerator::SupportedProfile profile;
    profile.profile = static_cast<VideoCodecProfile>(gpu_profile.profile);
    profile.max_resolution = gpu_profile.max_resolution;
    profile.min_resolution = gpu_profile.min_resolution;
    profile.encrypted_only = gpu_profile.encrypted_only;
    profiles.push_back(profile);
  }
  return profiles;
}

// static
gpu::VideoDecodeAcceleratorCapabilities
GpuVideoAcceleratorUtil::ConvertMediaToGpuDecodeCapabilities(
    const VideoDecodeAccelerator::Capabilities& media_capabilities) {
  gpu::VideoDecodeAcceleratorCapabilities capabilities;
  capabilities.supported_profiles =
      ConvertMediaToGpuDecodeProfiles(media_capabilities.supported_profiles);
  capabilities.flags = media_capabilities.flags;
  return capabilities;
}

// static
gpu::VideoDecodeAcceleratorSupportedProfiles
GpuVideoAcceleratorUtil::ConvertMediaToGpuDecodeProfiles(
    const VideoDecodeAccelerator::SupportedProfiles& media_profiles) {
  gpu::VideoDecodeAcceleratorSupportedProfiles profiles;
  for (const auto& media_profile : media_profiles) {
    gpu::VideoDecodeAcceleratorSupportedProfile profile;
    profile.profile =
        static_cast<gpu::VideoCodecProfile>(media_profile.profile);
    profile.max_resolution = media_profile.max_resolution;
    profile.min_resolution = media_profile.min_resolution;
    profile.encrypted_only = media_profile.encrypted_only;
    profiles.push_back(profile);
  }
  return profiles;
}

// static
VideoEncodeAccelerator::SupportedProfiles
GpuVideoAcceleratorUtil::ConvertGpuToMediaEncodeProfiles(
    const gpu::VideoEncodeAcceleratorSupportedProfiles& gpu_profiles) {
  VideoEncodeAccelerator::SupportedProfiles profiles;
  for (const auto& gpu_profile : gpu_profiles) {
    VideoEncodeAccelerator::SupportedProfile profile;
    profile.profile = static_cast<VideoCodecProfile>(gpu_profile.profile);
    profile.min_resolution = gpu_profile.min_resolution;
    profile.max_resolution = gpu_profile.max_resolution;
    profile.max_framerate_numerator = gpu_profile.max_framerate_numerator;
    profile.max_framerate_denominator = gpu_profile.max_framerate_denominator;
    profiles.push_back(profile);
  }
  return profiles;
}

// static
gpu::VideoEncodeAcceleratorSupportedProfiles
GpuVideoAcceleratorUtil::ConvertMediaToGpuEncodeProfiles(
    const VideoEncodeAccelerator::SupportedProfiles& media_profiles) {
  gpu::VideoEncodeAcceleratorSupportedProfiles profiles;
  for (const auto& media_profile : media_profiles) {
    gpu::VideoEncodeAcceleratorSupportedProfile profile;
    profile.profile =
        static_cast<gpu::VideoCodecProfile>(media_profile.profile);
    profile.min_resolution = media_profile.min_resolution;
    profile.max_resolution = media_profile.max_resolution;
    profile.max_framerate_numerator = media_profile.max_framerate_numerator;
    profile.max_framerate_denominator = media_profile.max_framerate_denominator;
    profiles.push_back(profile);
  }
  return profiles;
}

// static
void GpuVideoAcceleratorUtil::InsertUniqueDecodeProfiles(
    const VideoDecodeAccelerator::SupportedProfiles& new_profiles,
    VideoDecodeAccelerator::SupportedProfiles* media_profiles) {
  for (const auto& profile : new_profiles) {
    bool duplicate = false;
    for (const auto& media_profile : *media_profiles) {
      if (media_profile.profile == profile.profile) {
        duplicate = true;
        break;
      }
    }
    if (!duplicate)
      media_profiles->push_back(profile);
  }
}

// static
void GpuVideoAcceleratorUtil::InsertUniqueEncodeProfiles(
    const VideoEncodeAccelerator::SupportedProfiles& new_profiles,
    VideoEncodeAccelerator::SupportedProfiles* media_profiles) {
  for (const auto& profile : new_profiles) {
    bool duplicate = false;
    for (const auto& media_profile : *media_profiles) {
      if (media_profile.profile == profile.profile) {
        duplicate = true;
        break;
      }
    }
    if (!duplicate)
      media_profiles->push_back(profile);
  }
}

}  // namespace media
