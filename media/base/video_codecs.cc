// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_codecs.h"

#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"

namespace media {

// The names come from src/third_party/ffmpeg/libavcodec/codec_desc.c
// TODO(crbug.com/40236537): The returned strings are used by ChunkDemuxer in
// the code logic as well in tests. Merge with GetCodecNameForUMA() if possible.
std::string GetCodecName(VideoCodec codec) {
  switch (codec) {
    case VideoCodec::kUnknown:
      return "unknown";
    case VideoCodec::kH264:
      return "h264";
    case VideoCodec::kHEVC:
      return "hevc";
    case VideoCodec::kDolbyVision:
      return "dolbyvision";
    case VideoCodec::kVC1:
      return "vc1";
    case VideoCodec::kMPEG2:
      return "mpeg2video";
    case VideoCodec::kMPEG4:
      return "mpeg4";
    case VideoCodec::kTheora:
      return "theora";
    case VideoCodec::kVP8:
      return "vp8";
    case VideoCodec::kVP9:
      return "vp9";
    case VideoCodec::kAV1:
      return "av1";
  }
  NOTREACHED();
}

// Reported as part of some UMA names. NEVER change existing strings!
std::string GetCodecNameForUMA(VideoCodec codec) {
  switch (codec) {
    case VideoCodec::kUnknown:
      return "Unknown";
    case VideoCodec::kH264:
      return "H264";
    case VideoCodec::kHEVC:
      return "HEVC";
    case VideoCodec::kDolbyVision:
      return "DolbyVision";
    case VideoCodec::kVC1:
      return "VC1";
    case VideoCodec::kMPEG2:
      return "MPEG2Video";
    case VideoCodec::kMPEG4:
      return "MPEG4";
    case VideoCodec::kTheora:
      return "Theora";
    case VideoCodec::kVP8:
      return "VP8";
    case VideoCodec::kVP9:
      return "VP9";
    case VideoCodec::kAV1:
      return "AV1";
  }
  NOTREACHED();
}

std::string GetProfileName(VideoCodecProfile profile) {
  switch (profile) {
    case VIDEO_CODEC_PROFILE_UNKNOWN:
      return "unknown";
    case H264PROFILE_BASELINE:
      return "h264 baseline";
    case H264PROFILE_MAIN:
      return "h264 main";
    case H264PROFILE_EXTENDED:
      return "h264 extended";
    case H264PROFILE_HIGH:
      return "h264 high";
    case H264PROFILE_HIGH10PROFILE:
      return "h264 high 10";
    case H264PROFILE_HIGH422PROFILE:
      return "h264 high 4:2:2";
    case H264PROFILE_HIGH444PREDICTIVEPROFILE:
      return "h264 high 4:4:4 predictive";
    case H264PROFILE_SCALABLEBASELINE:
      return "h264 scalable baseline";
    case H264PROFILE_SCALABLEHIGH:
      return "h264 scalable high";
    case H264PROFILE_STEREOHIGH:
      return "h264 stereo high";
    case H264PROFILE_MULTIVIEWHIGH:
      return "h264 multiview high";
    case HEVCPROFILE_MAIN:
      return "hevc main";
    case HEVCPROFILE_MAIN10:
      return "hevc main 10";
    case HEVCPROFILE_MAIN_STILL_PICTURE:
      return "hevc main still-picture";
    case HEVCPROFILE_REXT:
      return "hevc range extensions";
    case HEVCPROFILE_HIGH_THROUGHPUT:
      return "hevc high throughput";
    case HEVCPROFILE_MULTIVIEW_MAIN:
      return "hevc multiview main";
    case HEVCPROFILE_SCALABLE_MAIN:
      return "hevc scalable main";
    case HEVCPROFILE_3D_MAIN:
      return "hevc 3d main";
    case HEVCPROFILE_SCREEN_EXTENDED:
      return "hevc screen extended";
    case HEVCPROFILE_SCALABLE_REXT:
      return "hevc scalable range extensions";
    case HEVCPROFILE_HIGH_THROUGHPUT_SCREEN_EXTENDED:
      return "hevc high throughput screen extended";
    case VP8PROFILE_ANY:
      return "vp8";
    case VP9PROFILE_PROFILE0:
      return "vp9 profile0";
    case VP9PROFILE_PROFILE1:
      return "vp9 profile1";
    case VP9PROFILE_PROFILE2:
      return "vp9 profile2";
    case VP9PROFILE_PROFILE3:
      return "vp9 profile3";
    case DOLBYVISION_PROFILE0:
      return "dolby vision profile 0";
    case DOLBYVISION_PROFILE5:
      return "dolby vision profile 5";
    case DOLBYVISION_PROFILE7:
      return "dolby vision profile 7";
    case DOLBYVISION_PROFILE8:
      return "dolby vision profile 8";
    case DOLBYVISION_PROFILE9:
      return "dolby vision profile 9";
    case THEORAPROFILE_ANY:
      return "theora";
    case AV1PROFILE_PROFILE_MAIN:
      return "av1 profile main";
    case AV1PROFILE_PROFILE_HIGH:
      return "av1 profile high";
    case AV1PROFILE_PROFILE_PRO:
      return "av1 profile pro";
    case VVCPROFILE_MAIN10:
      return "vvc profile main10";
    case VVCPROFILE_MAIN12:
      return "vvc profile main12";
    case VVCPROFILE_MAIN12_INTRA:
      return "vvc profile main12 intra";
    case VVCPROIFLE_MULTILAYER_MAIN10:
      return "vvc profile multilayer main10";
    case VVCPROFILE_MAIN10_444:
      return "vvc profile main10 444";
    case VVCPROFILE_MAIN12_444:
      return "vvc profile main12 444";
    case VVCPROFILE_MAIN16_444:
      return "vvc profile main16 444";
    case VVCPROFILE_MAIN12_444_INTRA:
      return "vvc profile main12 444 intra";
    case VVCPROFILE_MAIN16_444_INTRA:
      return "vvc profile main16 444 intra";
    case VVCPROFILE_MULTILAYER_MAIN10_444:
      return "vvc profile multilayer main10 444";
    case VVCPROFILE_MAIN10_STILL_PICTURE:
      return "vvc profile main10 still picture";
    case VVCPROFILE_MAIN12_STILL_PICTURE:
      return "vvc profile main12 still picture";
    case VVCPROFILE_MAIN10_444_STILL_PICTURE:
      return "vvc profile main10 444 still picture";
    case VVCPROFILE_MAIN12_444_STILL_PICTURE:
      return "vvc profile main12 444 still picture";
    case VVCPROFILE_MAIN16_444_STILL_PICTURE:
      return "vvc profile main16 444 still picture";
  }
  NOTREACHED();
}

std::string BuildH264MimeSuffix(media::VideoCodecProfile profile,
                                uint8_t level) {
  std::string profile_str;
  switch (profile) {
    case media::VideoCodecProfile::H264PROFILE_BASELINE:
      profile_str = "42";
      break;
    case media::VideoCodecProfile::H264PROFILE_MAIN:
      profile_str = "4d";
      break;
    case media::VideoCodecProfile::H264PROFILE_SCALABLEBASELINE:
      profile_str = "53";
      break;
    case media::VideoCodecProfile::H264PROFILE_SCALABLEHIGH:
      profile_str = "56";
      break;
    case media::VideoCodecProfile::H264PROFILE_EXTENDED:
      profile_str = "58";
      break;
    case media::VideoCodecProfile::H264PROFILE_HIGH:
      profile_str = "64";
      break;
    case media::VideoCodecProfile::H264PROFILE_HIGH10PROFILE:
      profile_str = "6e";
      break;
    case media::VideoCodecProfile::H264PROFILE_MULTIVIEWHIGH:
      profile_str = "76";
      break;
    case media::VideoCodecProfile::H264PROFILE_HIGH422PROFILE:
      profile_str = "7a";
      break;
    case media::VideoCodecProfile::H264PROFILE_STEREOHIGH:
      profile_str = "80";
      break;
    case media::VideoCodecProfile::H264PROFILE_HIGH444PREDICTIVEPROFILE:
      profile_str = "f4";
      break;
    default:
      return "";
  }

  return base::StringPrintf(".%s%04x", profile_str.c_str(), level);
}

VideoCodec VideoCodecProfileToVideoCodec(VideoCodecProfile profile) {
  switch (profile) {
    case VIDEO_CODEC_PROFILE_UNKNOWN:
      return VideoCodec::kUnknown;
    case H264PROFILE_BASELINE:
    case H264PROFILE_MAIN:
    case H264PROFILE_EXTENDED:
    case H264PROFILE_HIGH:
    case H264PROFILE_HIGH10PROFILE:
    case H264PROFILE_HIGH422PROFILE:
    case H264PROFILE_HIGH444PREDICTIVEPROFILE:
    case H264PROFILE_SCALABLEBASELINE:
    case H264PROFILE_SCALABLEHIGH:
    case H264PROFILE_STEREOHIGH:
    case H264PROFILE_MULTIVIEWHIGH:
      return VideoCodec::kH264;
    case HEVCPROFILE_MAIN:
    case HEVCPROFILE_MAIN10:
    case HEVCPROFILE_MAIN_STILL_PICTURE:
    case HEVCPROFILE_REXT:
    case HEVCPROFILE_HIGH_THROUGHPUT:
    case HEVCPROFILE_MULTIVIEW_MAIN:
    case HEVCPROFILE_SCALABLE_MAIN:
    case HEVCPROFILE_3D_MAIN:
    case HEVCPROFILE_SCREEN_EXTENDED:
    case HEVCPROFILE_SCALABLE_REXT:
    case HEVCPROFILE_HIGH_THROUGHPUT_SCREEN_EXTENDED:
      return VideoCodec::kHEVC;
    case VP8PROFILE_ANY:
      return VideoCodec::kVP8;
    case VP9PROFILE_PROFILE0:
    case VP9PROFILE_PROFILE1:
    case VP9PROFILE_PROFILE2:
    case VP9PROFILE_PROFILE3:
      return VideoCodec::kVP9;
    case DOLBYVISION_PROFILE0:
    case DOLBYVISION_PROFILE5:
    case DOLBYVISION_PROFILE7:
    case DOLBYVISION_PROFILE8:
    case DOLBYVISION_PROFILE9:
      return VideoCodec::kDolbyVision;
    case THEORAPROFILE_ANY:
      return VideoCodec::kTheora;
    case AV1PROFILE_PROFILE_MAIN:
    case AV1PROFILE_PROFILE_HIGH:
    case AV1PROFILE_PROFILE_PRO:
      return VideoCodec::kAV1;
    // TODO(crbugs.com/1417910): Update to VideoCodec::kVVC when
    // the production VVC decoder is enabled and corresponding
    // enum is allowed to be added.
    case VVCPROFILE_MAIN10:
    case VVCPROFILE_MAIN12:
    case VVCPROFILE_MAIN12_INTRA:
    case VVCPROIFLE_MULTILAYER_MAIN10:
    case VVCPROFILE_MAIN10_444:
    case VVCPROFILE_MAIN12_444:
    case VVCPROFILE_MAIN16_444:
    case VVCPROFILE_MAIN12_444_INTRA:
    case VVCPROFILE_MAIN16_444_INTRA:
    case VVCPROFILE_MULTILAYER_MAIN10_444:
    case VVCPROFILE_MAIN10_STILL_PICTURE:
    case VVCPROFILE_MAIN12_STILL_PICTURE:
    case VVCPROFILE_MAIN10_444_STILL_PICTURE:
    case VVCPROFILE_MAIN12_444_STILL_PICTURE:
    case VVCPROFILE_MAIN16_444_STILL_PICTURE:
      return VideoCodec::kUnknown;
  }
}

std::ostream& operator<<(std::ostream& os, const VideoCodec& codec) {
  return os << GetCodecName(codec);
}

}  // namespace media
