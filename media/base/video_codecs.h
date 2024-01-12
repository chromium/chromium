// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VIDEO_CODECS_H_
#define MEDIA_BASE_VIDEO_CODECS_H_

#include <stdint.h>

#include <string>

#include "media/base/media_export.h"

namespace media {

// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.media
enum class VideoCodec {
  // These values are histogrammed over time; do not change their ordinal
  // values.  When deleting a codec replace it with a dummy value; when adding a
  // codec, do so at the bottom (and update kMaxValue).
  kUnknown = 0,
  kH264,
  kVC1,
  kMPEG2,
  kMPEG4,
  kTheora,
  kVP8,
  kVP9,
  kHEVC,
  kDolbyVision,
  kAV1,
  // DO NOT ADD RANDOM VIDEO CODECS!
  //
  // The only acceptable time to add a new codec is if there is production code
  // that uses said codec in the same CL.

  kMaxValue = kAV1,  // Must equal the last "real" codec above.
};

// Video codec profiles. Keep in sync with mojo::VideoCodecProfile (see
// media/mojo/mojom/media_types.mojom), gpu::VideoCodecProfile (see
// gpu/config/gpu_info.h), and PP_VideoDecoder_Profile (translation is performed
// in content/renderer/pepper/ppb_video_decoder_impl.cc).
// NOTE: These values are histogrammed over time in UMA so the values must never
// ever change (add new values to tools/metrics/histograms/histograms.xml)
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.media
enum VideoCodecProfile {
  // Keep the values in this enum unique, as they imply format (h.264 vs. VP8,
  // for example), and keep the values for a particular format grouped
  // together for clarity.
  VIDEO_CODEC_PROFILE_UNKNOWN = -1,
  VIDEO_CODEC_PROFILE_MIN = VIDEO_CODEC_PROFILE_UNKNOWN,
  H264PROFILE_MIN = 0,
  H264PROFILE_BASELINE = H264PROFILE_MIN,
  H264PROFILE_MAIN = 1,
  H264PROFILE_EXTENDED = 2,
  H264PROFILE_HIGH = 3,
  H264PROFILE_HIGH10PROFILE = 4,
  H264PROFILE_HIGH422PROFILE = 5,
  H264PROFILE_HIGH444PREDICTIVEPROFILE = 6,
  H264PROFILE_SCALABLEBASELINE = 7,
  H264PROFILE_SCALABLEHIGH = 8,
  H264PROFILE_STEREOHIGH = 9,
  H264PROFILE_MULTIVIEWHIGH = 10,
  H264PROFILE_MAX = H264PROFILE_MULTIVIEWHIGH,
  VP8PROFILE_MIN = 11,
  VP8PROFILE_ANY = VP8PROFILE_MIN,
  VP8PROFILE_MAX = VP8PROFILE_ANY,
  VP9PROFILE_MIN = 12,
  VP9PROFILE_PROFILE0 = VP9PROFILE_MIN,
  VP9PROFILE_PROFILE1 = 13,
  VP9PROFILE_PROFILE2 = 14,
  VP9PROFILE_PROFILE3 = 15,
  VP9PROFILE_MAX = VP9PROFILE_PROFILE3,
  HEVCPROFILE_MIN = 16,
  HEVCPROFILE_MAIN = HEVCPROFILE_MIN,
  HEVCPROFILE_MAIN10 = 17,
  HEVCPROFILE_MAIN_STILL_PICTURE = 18,
  HEVCPROFILE_MAX = HEVCPROFILE_MAIN_STILL_PICTURE,
  DOLBYVISION_PROFILE0 = 19,
  // Deprecated: DOLBYVISION_PROFILE4 = 20,
  DOLBYVISION_PROFILE5 = 21,
  DOLBYVISION_PROFILE7 = 22,
  THEORAPROFILE_MIN = 23,
  THEORAPROFILE_ANY = THEORAPROFILE_MIN,
  THEORAPROFILE_MAX = THEORAPROFILE_ANY,
  AV1PROFILE_MIN = 24,
  AV1PROFILE_PROFILE_MAIN = AV1PROFILE_MIN,
  AV1PROFILE_PROFILE_HIGH = 25,
  AV1PROFILE_PROFILE_PRO = 26,
  AV1PROFILE_MAX = AV1PROFILE_PROFILE_PRO,
  DOLBYVISION_PROFILE8 = 27,
  DOLBYVISION_PROFILE9 = 28,
  HEVCPROFILE_EXT_MIN = 29,
  HEVCPROFILE_REXT = HEVCPROFILE_EXT_MIN,
  HEVCPROFILE_HIGH_THROUGHPUT = 30,
  HEVCPROFILE_MULTIVIEW_MAIN = 31,
  HEVCPROFILE_SCALABLE_MAIN = 32,
  HEVCPROFILE_3D_MAIN = 33,
  HEVCPROFILE_SCREEN_EXTENDED = 34,
  HEVCPROFILE_SCALABLE_REXT = 35,
  HEVCPROFILE_HIGH_THROUGHPUT_SCREEN_EXTENDED = 36,
  HEVCPROFILE_EXT_MAX = HEVCPROFILE_HIGH_THROUGHPUT_SCREEN_EXTENDED,
  VVCPROFILE_MIN = 37,
  VVCPROFILE_MAIN10 = VVCPROFILE_MIN,
  VVCPROFILE_MAIN12 = 38,
  VVCPROFILE_MAIN12_INTRA = 39,
  VVCPROIFLE_MULTILAYER_MAIN10 = 40,
  VVCPROFILE_MAIN10_444 = 41,
  VVCPROFILE_MAIN12_444 = 42,
  VVCPROFILE_MAIN16_444 = 43,
  VVCPROFILE_MAIN12_444_INTRA = 44,
  VVCPROFILE_MAIN16_444_INTRA = 45,
  VVCPROFILE_MULTILAYER_MAIN10_444 = 46,
  VVCPROFILE_MAIN10_STILL_PICTURE = 47,
  VVCPROFILE_MAIN12_STILL_PICTURE = 48,
  VVCPROFILE_MAIN10_444_STILL_PICTURE = 49,
  VVCPROFILE_MAIN12_444_STILL_PICTURE = 50,
  VVCPROFILE_MAIN16_444_STILL_PICTURE = 51,
  VVCPROFILE_MAX = VVCPROFILE_MAIN16_444_STILL_PICTURE,
  VIDEO_CODEC_PROFILE_MAX = VVCPROFILE_MAIN16_444_STILL_PICTURE,
};

using VideoCodecLevel = uint32_t;
constexpr VideoCodecLevel kNoVideoCodecLevel = 0;

struct CodecProfileLevel {
  VideoCodec codec;
  VideoCodecProfile profile;
  VideoCodecLevel level;
};

// Returns a name for `codec` for logging and display purposes.
MEDIA_EXPORT std::string GetCodecName(VideoCodec codec);

// Returns a name for `codec` to be used for UMA reporting.
MEDIA_EXPORT std::string GetCodecNameForUMA(VideoCodec codec);

MEDIA_EXPORT std::string GetProfileName(VideoCodecProfile profile);

MEDIA_EXPORT std::string BuildH264MimeSuffix(VideoCodecProfile profile,
                                             uint8_t level);

MEDIA_EXPORT VideoCodec
VideoCodecProfileToVideoCodec(VideoCodecProfile profile);

MEDIA_EXPORT std::ostream& operator<<(std::ostream& os,
                                      const VideoCodec& codec);

}  // namespace media

#endif  // MEDIA_BASE_VIDEO_CODECS_H_
