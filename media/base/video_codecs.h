// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VIDEO_CODECS_H_
#define MEDIA_BASE_VIDEO_CODECS_H_

#include <stdint.h>
#include <string>

#include "base/strings/string_piece_forward.h"
#include "media/base/media_export.h"
#include "media/media_buildflags.h"

namespace media {

class VideoColorSpace;

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
  DOLBYVISION_PROFILE4 = 20,
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
std::string MEDIA_EXPORT GetCodecName(VideoCodec codec);

// Returns a name for `codec` to be used for UMA reporting.
std::string MEDIA_EXPORT GetCodecNameForUMA(VideoCodec codec);

std::string MEDIA_EXPORT GetProfileName(VideoCodecProfile profile);
std::string MEDIA_EXPORT BuildH264MimeSuffix(VideoCodecProfile profile,
                                             uint8_t level);

// ParseNewStyleVp9CodecID handles parsing of new style vp9 codec IDs per
// proposed VP Codec ISO Media File Format Binding specification:
// https://storage.googleapis.com/downloads.webmproject.org/docs/vp9/vp-codec-iso-media-file-format-binding-20160516-draft.pdf
// ParseLegacyVp9CodecID handles parsing of legacy VP9 codec strings defined
// for WebM.
// TODO(kqyang): Consolidate the two functions once we address crbug.com/667834
MEDIA_EXPORT bool ParseNewStyleVp9CodecID(base::StringPiece codec_id,
                                          VideoCodecProfile* profile,
                                          uint8_t* level_idc,
                                          VideoColorSpace* color_space);

MEDIA_EXPORT bool ParseLegacyVp9CodecID(base::StringPiece codec_id,
                                        VideoCodecProfile* profile,
                                        uint8_t* level_idc);

#if BUILDFLAG(ENABLE_AV1_DECODER)
MEDIA_EXPORT bool ParseAv1CodecId(base::StringPiece codec_id,
                                  VideoCodecProfile* profile,
                                  uint8_t* level_idc,
                                  VideoColorSpace* color_space);
#endif

// Handle parsing AVC/H.264 codec ids as outlined in RFC 6381 and ISO-14496-10.
MEDIA_EXPORT bool ParseAVCCodecId(base::StringPiece codec_id,
                                  VideoCodecProfile* profile,
                                  uint8_t* level_idc);

#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
MEDIA_EXPORT bool ParseHEVCCodecId(base::StringPiece codec_id,
                                   VideoCodecProfile* profile,
                                   uint8_t* level_idc);
#endif

#if BUILDFLAG(ENABLE_PLATFORM_VVC)
MEDIA_EXPORT bool ParseVVCCodecId(base::StringPiece codec_id,
                                  VideoCodecProfile* profile,
                                  uint8_t* level_idc);
#endif

#if BUILDFLAG(ENABLE_PLATFORM_DOLBY_VISION)
MEDIA_EXPORT bool ParseDolbyVisionCodecId(base::StringPiece codec_id,
                                          VideoCodecProfile* profile,
                                          uint8_t* level_id);
#endif

MEDIA_EXPORT void ParseCodec(base::StringPiece codec_id,
                             VideoCodec& codec,
                             VideoCodecProfile& profile,
                             uint8_t& level,
                             VideoColorSpace& color_space);
MEDIA_EXPORT VideoCodec StringToVideoCodec(base::StringPiece codec_id);

MEDIA_EXPORT VideoCodec
VideoCodecProfileToVideoCodec(VideoCodecProfile profile);

#if BUILDFLAG(ENABLE_MSE_MPEG2TS_STREAM_PARSER)
// Translate legacy avc1 codec ids (like avc1.66.30 or avc1.77.31) into a new
// style standard avc1 codec ids like avc1.4D002F. If the input codec is not
// recognized as a legacy codec id, then returns the input string unchanged.
std::string TranslateLegacyAvc1CodecIds(base::StringPiece codec_id);
#endif

MEDIA_EXPORT std::ostream& operator<<(std::ostream& os,
                                      const VideoCodec& codec);

}  // namespace media

#endif  // MEDIA_BASE_VIDEO_CODECS_H_
