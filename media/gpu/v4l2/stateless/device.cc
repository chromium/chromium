// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/stateless/device.h"

#include <map>

// build_config.h must come before BUILDFLAG()
#include "build/build_config.h"
#if BUILDFLAG(IS_CHROMEOS)
#include <linux/media/av1-ctrls.h>
#include <linux/media/vp9-ctrls-upstream.h>
#endif

#include <fcntl.h>
#include <linux/media.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_number_conversions.h"
#include "media/base/video_types.h"
#include "media/gpu/macros.h"
#include "media/gpu/v4l2/v4l2_utils.h"

// This has not been accepted upstream.
#ifndef V4L2_PIX_FMT_AV1
#define V4L2_PIX_FMT_AV1 v4l2_fourcc('A', 'V', '0', '1') /* AV1 */
#endif
// This has been upstreamed and backported for ChromeOS, but has not been
// picked up by the Chromium sysroots.
#ifndef V4L2_PIX_FMT_AV1_FRAME
#define V4L2_PIX_FMT_AV1_FRAME v4l2_fourcc('A', 'V', '1', 'F')
#endif

namespace media {

namespace {

using v4l2_enum_type = decltype(V4L2_PIX_FMT_H264);
// Correspondence from V4L2 codec described as a pixel format to a Control ID.
static const std::map<v4l2_enum_type, v4l2_enum_type>
    kV4L2CodecPixFmtToProfileCID = {
        {V4L2_PIX_FMT_H264_SLICE, V4L2_CID_MPEG_VIDEO_H264_PROFILE},
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
        {V4L2_PIX_FMT_HEVC_SLICE, V4L2_CID_MPEG_VIDEO_HEVC_PROFILE},
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
        {V4L2_PIX_FMT_VP8_FRAME, V4L2_CID_MPEG_VIDEO_VP8_PROFILE},
        {V4L2_PIX_FMT_VP9_FRAME, V4L2_CID_MPEG_VIDEO_VP9_PROFILE},
#if BUILDFLAG(IS_CHROMEOS)
        {V4L2_PIX_FMT_AV1_FRAME, V4L2_CID_MPEG_VIDEO_AV1_PROFILE},
#endif
};

// Default VideoCodecProfiles associated to a V4L2 Codec Control ID.
static const std::map<v4l2_enum_type, std::vector<VideoCodecProfile>>
    kDefaultVideoCodecProfilesForProfileCID = {
        {V4L2_CID_MPEG_VIDEO_H264_PROFILE,
         {
             H264PROFILE_BASELINE,
             H264PROFILE_MAIN,
             H264PROFILE_HIGH,
         }},
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
        {V4L2_CID_MPEG_VIDEO_HEVC_PROFILE, {HEVCPROFILE_MAIN}},
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
        {V4L2_CID_MPEG_VIDEO_VP8_PROFILE, {VP8PROFILE_ANY}},
        {V4L2_CID_MPEG_VIDEO_VP9_PROFILE, {VP9PROFILE_PROFILE0}},
#if BUILDFLAG(IS_CHROMEOS)
        {V4L2_CID_MPEG_VIDEO_AV1_PROFILE, {AV1PROFILE_PROFILE_MAIN}},
#endif
};

VideoCodec V4L2PixFmtToVideoCodec(uint32_t pix_fmt) {
  switch (pix_fmt) {
    case V4L2_PIX_FMT_H264_SLICE:
      return VideoCodec::kH264;
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
    case V4L2_PIX_FMT_HEVC_SLICE:
      return VideoCodec::kHEVC;
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
    case V4L2_PIX_FMT_VP8_FRAME:
      return VideoCodec::kVP8;
    case V4L2_PIX_FMT_VP9_FRAME:
      return VideoCodec::kVP9;
    case V4L2_PIX_FMT_AV1_FRAME:
      return VideoCodec::kAV1;
  }
  return VideoCodec::kUnknown;
}

}  // namespace

Device::Device() {}

// VIDIOC_ENUM_FMT
std::set<VideoCodec> Device::EnumerateInputFormats() {
  std::set<VideoCodec> pix_fmts;
  v4l2_fmtdesc fmtdesc = {.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE};
  for (; IoctlDevice(VIDIOC_ENUM_FMT, &fmtdesc) == kIoctlOk; ++fmtdesc.index) {
    DVLOGF(4) << "Enumerated codec: "
              << media::FourccToString(fmtdesc.pixelformat) << " ("
              << fmtdesc.description << ")";
    pix_fmts.insert(V4L2PixFmtToVideoCodec(fmtdesc.pixelformat));
  }

  return pix_fmts;
}

// VIDIOC_ENUM_FRAMESIZES
std::pair<gfx::Size, gfx::Size> Device::GetFrameResolutionRange(
    VideoCodec codec) {
  constexpr gfx::Size kDefaultMaxCodedSize(1920, 1088);
  constexpr gfx::Size kDefaultMinCodedSize(16, 16);

  v4l2_frmsizeenum frame_size;
  memset(&frame_size, 0, sizeof(frame_size));
  frame_size.pixel_format = VideoCodecToV4L2PixFmt(codec);
  if (IoctlDevice(VIDIOC_ENUM_FRAMESIZES, &frame_size) == kIoctlOk) {
#if BUILDFLAG(IS_CHROMEOS)
    // All of Chrome-supported implementations support STEPWISE only.
    CHECK_EQ(frame_size.type, V4L2_FRMSIZE_TYPE_STEPWISE);
#endif
    if (frame_size.type == V4L2_FRMSIZE_TYPE_STEPWISE) {
      return std::make_pair(gfx::Size(frame_size.stepwise.min_width,
                                      frame_size.stepwise.min_height),
                            gfx::Size(frame_size.stepwise.max_width,
                                      frame_size.stepwise.max_height));
    }
  }

  VPLOGF(1) << "VIDIOC_ENUM_FRAMESIZES failed, using default values";
  return std::make_pair(kDefaultMinCodedSize, kDefaultMaxCodedSize);
}

// VIDIOC_QUERYCTRL, VIDIOC_QUERYMENU
std::vector<VideoCodecProfile> Device::ProfilesForVideoCodec(VideoCodec codec) {
  const uint32_t pix_fmt = VideoCodecToV4L2PixFmt(codec);

  if (!base::Contains(kV4L2CodecPixFmtToProfileCID, pix_fmt)) {
    // This is OK: there are many codecs that are not supported by Chrome.
    DVLOGF(4) << "Unsupported codec: " << FourccToString(pix_fmt);
    return {};
  }

  const auto profile_cid = kV4L2CodecPixFmtToProfileCID.at(pix_fmt);

  v4l2_queryctrl query_ctrl = {.id = base::strict_cast<__u32>(profile_cid)};
  const int ret = IoctlDevice(VIDIOC_QUERYCTRL, &query_ctrl);
  if (ret != kIoctlOk) {
    VPLOGF(1) << "VIDIOC_QUERYCTRL failed.";
    return {};
  }

  std::vector<VideoCodecProfile> profiles;

  v4l2_querymenu query_menu = {
      .id = query_ctrl.id,
      .index = base::checked_cast<__u32>(query_ctrl.minimum)};
  for (; query_menu.index <= base::checked_cast<__u32>(query_ctrl.maximum);
       query_menu.index++) {
    if (IoctlDevice(VIDIOC_QUERYMENU, &query_menu) != kIoctlOk) {
      continue;
    }
    const VideoCodecProfile profile =
        V4L2ProfileToVideoCodecProfile(profile_cid, query_menu.index);
    DVLOGF_IF(4, profile == VIDEO_CODEC_PROFILE_UNKNOWN)
        << "Profile: " << query_menu.name << " for " << FourccToString(pix_fmt)
        << " not supported by Chrome, skipping.";

    if (profile != VIDEO_CODEC_PROFILE_UNKNOWN) {
      profiles.push_back(profile);
      DVLOGF(4) << FourccToString(pix_fmt) << " profile " << query_menu.name
                << " supported.";
    }
  }

  // Erase duplicated profiles. This is needed because H264PROFILE_BASELINE maps
  // to both V4L2_MPEG_VIDEO_H264_PROFILE__BASELINE/CONSTRAINED_BASELINE
  base::ranges::sort(profiles);
  profiles.erase(base::ranges::unique(profiles), profiles.end());
  return profiles;
}

bool Device::OpenDevice() {
  DVLOGF(3);
  static const std::string kDecoderDevicePrefix = "/dev/video-dec";

  // We are sandboxed, so we can't query directory contents to check which
  // devices are actually available. Try to open the first 10; if not present,
  // we will just fail to open immediately.
  for (int i = 0; i < 10; ++i) {
    const auto path = kDecoderDevicePrefix + base::NumberToString(i);
    device_fd_.reset(
        HANDLE_EINTR(open(path.c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC, 0)));
    if (!device_fd_.is_valid()) {
      VPLOGF(2) << "Failed to open media device: " << path;
      continue;
    }

    break;
  }

  if (!device_fd_.is_valid()) {
    DVLOGF(1) << "Failed to open device fd.";
    return false;
  }

  return true;
}

void Device::Close() {
  device_fd_.reset();
}

Device::~Device() {}

int Device::IoctlDevice(int request, void* arg) {
  DCHECK(device_fd_.is_valid());

  return HANDLE_EINTR(ioctl(device_fd_.get(), request, arg));
}

}  //  namespace media
