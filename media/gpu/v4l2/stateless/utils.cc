// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/stateless/utils.h"

#include <linux/videodev2.h>

#include "media/gpu/macros.h"

namespace media {

VideoDecodeAccelerator::SupportedProfiles GetSupportedDecodeProfiles(
    Device* device) {
  VideoDecodeAccelerator::SupportedProfiles supported_profiles;

  auto input_codecs = device->EnumerateInputFormats();

  for (VideoCodec codec : input_codecs) {
    VideoDecodeAccelerator::SupportedProfile profile;

    const auto resolution_range = device->GetFrameResolutionRange(codec);
    profile.min_resolution = resolution_range.first;
    profile.max_resolution = resolution_range.second;

    auto video_codec_profiles = device->ProfilesForVideoCodec(codec);
    for (const auto& video_codec_profile : video_codec_profiles) {
      profile.profile = video_codec_profile;
      supported_profiles.push_back(profile);

      DVLOGF(3) << "Found decoder profile " << GetProfileName(profile.profile)
                << ", resolutions: " << profile.min_resolution.ToString() << " "
                << profile.max_resolution.ToString();
    }
  }

  return supported_profiles;
}

std::string IoctlToString(uint64_t request) {
#define IOCTL_TO_STR(i) \
  case i:               \
    return #i;

  switch (request) {
    IOCTL_TO_STR(VIDIOC_DECODER_CMD)
    IOCTL_TO_STR(VIDIOC_DQBUF)
    IOCTL_TO_STR(VIDIOC_DQEVENT)
    IOCTL_TO_STR(VIDIOC_ENCODER_CMD)
    IOCTL_TO_STR(VIDIOC_ENUM_FMT)
    IOCTL_TO_STR(VIDIOC_ENUM_FRAMESIZES)
    IOCTL_TO_STR(VIDIOC_EXPBUF)
    IOCTL_TO_STR(VIDIOC_G_CROP)
    IOCTL_TO_STR(VIDIOC_G_EXT_CTRLS)
    IOCTL_TO_STR(VIDIOC_G_FMT)
    IOCTL_TO_STR(VIDIOC_G_PARM)
    IOCTL_TO_STR(VIDIOC_G_SELECTION)
    IOCTL_TO_STR(VIDIOC_QBUF)
    IOCTL_TO_STR(VIDIOC_QUERYBUF)
    IOCTL_TO_STR(VIDIOC_QUERYCAP)
    IOCTL_TO_STR(VIDIOC_QUERYCTRL)
    IOCTL_TO_STR(VIDIOC_QUERYMENU)
    IOCTL_TO_STR(VIDIOC_QUERY_EXT_CTRL)
    IOCTL_TO_STR(VIDIOC_REQBUFS)
    IOCTL_TO_STR(VIDIOC_STREAMOFF)
    IOCTL_TO_STR(VIDIOC_STREAMON)
    IOCTL_TO_STR(VIDIOC_SUBSCRIBE_EVENT)
    IOCTL_TO_STR(VIDIOC_S_CROP)
    IOCTL_TO_STR(VIDIOC_S_CTRL)
    IOCTL_TO_STR(VIDIOC_S_EXT_CTRLS)
    IOCTL_TO_STR(VIDIOC_S_FMT)
    IOCTL_TO_STR(VIDIOC_S_PARM)
    IOCTL_TO_STR(VIDIOC_S_SELECTION)
    IOCTL_TO_STR(VIDIOC_TRY_DECODER_CMD)
    IOCTL_TO_STR(VIDIOC_TRY_ENCODER_CMD)
    IOCTL_TO_STR(VIDIOC_TRY_FMT)
    IOCTL_TO_STR(VIDIOC_UNSUBSCRIBE_EVENT)
  }

  return "unknown";

#undef IOCTL_TO_STR
}
}  // namespace media
