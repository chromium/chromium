// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/base/video_codec_string_parsers.h"

#include <string_view>

#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "media/base/video_color_space.h"
#include "ui/gfx/color_space.h"

namespace {

bool IsDolbyVisionAVCCodecId(std::string_view codec_id) {
  return base::StartsWith(codec_id, "dva1.", base::CompareCase::SENSITIVE) ||
         base::StartsWith(codec_id, "dvav.", base::CompareCase::SENSITIVE);
}

bool IsDolbyVisionHEVCCodecId(std::string_view codec_id) {
  return base::StartsWith(codec_id, "dvh1.", base::CompareCase::SENSITIVE) ||
         base::StartsWith(codec_id, "dvhe.", base::CompareCase::SENSITIVE);
}

}  // namespace

namespace media {

std::optional<VideoType> ParseNewStyleVp9CodecID(std::string_view codec_id) {
  // Initialize optional fields to their defaults.
  VideoType result = {
      .codec = VideoCodec::kVP9,
      .color_space = VideoColorSpace::REC709(),
      .subsampling = VideoChromaSampling::k420,
  };

  std::vector<std::string> fields = base::SplitString(
      codec_id, ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);

  // First four fields are mandatory. No more than 9 fields are expected.
  if (fields.size() < 4 || fields.size() > 9) {
    DVLOG(3) << __func__ << " Invalid number of fields (" << fields.size()
             << ")";
    return std::nullopt;
  }

  if (fields[0] != "vp09") {
    DVLOG(3) << __func__ << " Invalid 4CC (" << fields[0] << ")";
    return std::nullopt;
  }

  std::vector<int> values;
  for (size_t i = 1; i < fields.size(); ++i) {
    // Missing value is not allowed.
    if (fields[i] == "") {
      DVLOG(3) << __func__ << " Invalid missing field (position:" << i << ")";
      return std::nullopt;
    }
    int value;
    if (!base::StringToInt(fields[i], &value) || value < 0) {
      DVLOG(3) << __func__ << " Invalid field value (" << value << ")";
      return std::nullopt;
    }
    values.push_back(value);
  }

  const int profile_idc = values[0];
  switch (profile_idc) {
    case 0:
      result.profile = VP9PROFILE_PROFILE0;
      break;
    case 1:
      result.profile = VP9PROFILE_PROFILE1;
      break;
    case 2:
      result.profile = VP9PROFILE_PROFILE2;
      break;
    case 3:
      result.profile = VP9PROFILE_PROFILE3;
      break;
    default:
      DVLOG(3) << __func__ << " Invalid profile (" << profile_idc << ")";
      return std::nullopt;
  }

  result.level = values[1];
  switch (result.level) {
    case 10:
    case 11:
    case 20:
    case 21:
    case 30:
    case 31:
    case 40:
    case 41:
    case 50:
    case 51:
    case 52:
    case 60:
    case 61:
    case 62:
      break;
    default:
      DVLOG(3) << __func__ << " Invalid level (" << result.level << ")";
      return std::nullopt;
  }

  result.bit_depth = values[2];
  if (result.bit_depth != 8 && result.bit_depth != 10 &&
      result.bit_depth != 12) {
    DVLOG(3) << __func__ << " Invalid bit-depth (" << *result.bit_depth << ")";
    return std::nullopt;
  }

  // 4:2:0 isn't supported in profiles 1, 3.
  if (profile_idc == 1 || profile_idc == 3) {
    result.subsampling = VideoChromaSampling::k422;
  }

  if (values.size() < 4) {
    return result;
  }
  const int chroma_subsampling = values[3];
  switch (chroma_subsampling) {
    case 0:
    case 1:
      result.subsampling = VideoChromaSampling::k420;
      break;
    case 2:
      result.subsampling = VideoChromaSampling::k422;
      break;
    case 3:
      result.subsampling = VideoChromaSampling::k444;
      break;
    default:
      DVLOG(3) << __func__ << " Invalid chroma subsampling ("
               << chroma_subsampling << ")";
      return std::nullopt;
  }

  if (result.subsampling != VideoChromaSampling::k420 && profile_idc != 1 &&
      profile_idc != 3) {
    DVLOG(3) << __func__
             << " 4:2:2 and 4:4:4 are only supported in profile 1, 3";

    // Ideally this would be an error, but even Netflix broke when we tried...
    result.subsampling = VideoChromaSampling::k420;
  }

  if (values.size() < 5) {
    return result;
  }
  result.color_space.primaries = VideoColorSpace::GetPrimaryID(values[4]);
  if (result.color_space.primaries == VideoColorSpace::PrimaryID::INVALID) {
    DVLOG(3) << __func__ << " Invalid color primaries (" << values[4] << ")";
    return std::nullopt;
  }

  if (values.size() < 6) {
    return result;
  }
  result.color_space.transfer = VideoColorSpace::GetTransferID(values[5]);
  if (result.color_space.transfer == VideoColorSpace::TransferID::INVALID) {
    DVLOG(3) << __func__ << " Invalid transfer function (" << values[5] << ")";
    return std::nullopt;
  }

  if (values.size() < 7) {
    return result;
  }
  result.color_space.matrix = VideoColorSpace::GetMatrixID(values[6]);
  if (result.color_space.matrix == VideoColorSpace::MatrixID::INVALID) {
    DVLOG(3) << __func__ << " Invalid matrix coefficients (" << values[6]
             << ")";
    return std::nullopt;
  }
  if (result.color_space.matrix == VideoColorSpace::MatrixID::RGB &&
      chroma_subsampling != 3) {
    DVLOG(3) << __func__ << " Invalid combination of chroma_subsampling ("
             << ") and matrix coefficients (" << values[6] << ")";
  }

  if (values.size() < 8) {
    return result;
  }
  const int video_full_range_flag = values[7];
  if (video_full_range_flag > 1) {
    DVLOG(3) << __func__ << " Invalid full range flag ("
             << video_full_range_flag << ")";
    return std::nullopt;
  }
  result.color_space.range = video_full_range_flag == 1
                                 ? gfx::ColorSpace::RangeID::FULL
                                 : gfx::ColorSpace::RangeID::LIMITED;

  return result;
}

std::optional<VideoType> ParseLegacyVp9CodecID(std::string_view codec_id) {
  if (codec_id == "vp9" || codec_id == "vp9.0") {
    // Profile is not included in the codec string. Consumers of parsed codec
    // should handle by rejecting ambiguous string or resolving to a default
    // profile.
    VideoType result{.codec = VideoCodec::kVP9,
                     .profile = VIDEO_CODEC_PROFILE_UNKNOWN,
                     .level = kNoVideoCodecLevel};
    return result;
  }

  return std::nullopt;
}

std::optional<VideoType> ParseAv1CodecId(std::string_view codec_id) {
  // The codecs parameter string for the AOM AV1 codec is as follows:
  // See https://aomediacodec.github.io/av1-isobmff/#codecsparam.
  //
  // <sample entry4CC>.<profile>.<level><tier>.<bitDepth>.<monochrome>.
  // <chromaSubsampling>.<colorPrimaries>.<transferCharacteristics>.
  // <matrixCoefficients>.<videoFullRangeFlag>

  std::vector<std::string> fields = base::SplitString(
      codec_id, ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);

  // The parameters sample entry 4CC, profile, level, tier, and bitDepth are all
  // mandatory fields. If any of these fields are empty, or not within their
  // allowed range, the processing device SHOULD treat it as an error.
  if (fields.size() < 4 || fields.size() > 10) {
    DVLOG(3) << __func__ << " Invalid number of fields (" << fields.size()
             << ")";
    return std::nullopt;
  }

  // All the other fields (including their leading '.') are optional, mutually
  // inclusive (all or none) fields. If not specified then the values listed in
  // the table below are assumed.
  //
  // mono_chrome              0
  // chromaSubsampling        112 (4:2:0 colocated with luma (0,0))
  // colorPrimaries           1 (ITU-R BT.709)
  // transferCharacteristics  1 (ITU-R BT.709)
  // matrixCoefficients       1 (ITU-R BT.709)
  // videoFullRangeFlag       0 (studio swing representation)
  // Initialize optional fields to their defaults.
  VideoType result = {
      .codec = VideoCodec::kAV1,
      .color_space = VideoColorSpace::REC709(),
      .subsampling = VideoChromaSampling::k420,
  };

  if (fields[0] != "av01") {
    DVLOG(3) << __func__ << " Invalid AV1 4CC (" << fields[0] << ")";
    return std::nullopt;
  }

  // The level parameter value SHALL equal the first level value indicated by
  // seq_level_idx in the Sequence Header. The tier parameter value SHALL be
  // equal to M when the first seq_tier value in the Sequence Header is equal to
  // 0, and H when it is equal to 1.
  if (fields[2].size() != 3 || (fields[2][2] != 'M' && fields[2][2] != 'H')) {
    DVLOG(3) << __func__ << " Invalid level+tier (" << fields[2] << ")";
    return std::nullopt;
  }

  // Since tier has been validated, strip the trailing tier indicator to allow
  // int conversion below.
  fields[2].resize(2);

  // Fill with dummy values to ensure parallel indices with fields.
  std::vector<int> values(fields.size(), 0);
  for (size_t i = 1; i < fields.size(); ++i) {
    if (fields[i].empty()) {
      DVLOG(3) << __func__ << " Invalid empty field (position:" << i << ")";
      return std::nullopt;
    }

    if (!base::StringToInt(fields[i], &values[i]) || values[i] < 0) {
      DVLOG(3) << __func__ << " Invalid field value (" << values[i] << ")";
      return std::nullopt;
    }
  }

  // The profile parameter value, represented by a single digit decimal, SHALL
  // equal the value of seq_profile in the Sequence Header.
  const int profile_idc = fields[1].size() == 1 ? values[1] : -1;
  switch (profile_idc) {
    case 0:
      result.profile = AV1PROFILE_PROFILE_MAIN;
      break;
    case 1:
      result.subsampling = VideoChromaSampling::k444;
      result.profile = AV1PROFILE_PROFILE_HIGH;
      break;
    case 2:
      result.profile = AV1PROFILE_PROFILE_PRO;
      break;
    default:
      DVLOG(3) << __func__ << " Invalid profile (" << fields[1] << ")";
      return std::nullopt;
  }

  // The level parameter value SHALL equal the first level value indicated by
  // seq_level_idx in the Sequence Header. Note: We validate that this field has
  // the required leading zeros above.
  result.level = values[2];
  if (result.level > 31) {
    DVLOG(3) << __func__ << " Invalid level (" << result.level << ")";
    return std::nullopt;
  }

  // The bitDepth parameter value SHALL equal the value of BitDepth variable as
  // defined in [AV1] derived from the Sequence Header. Leading zeros required.
  result.bit_depth = values[3];
  if (fields[3].size() != 2 ||
      (result.bit_depth != 8 && result.bit_depth != 10 &&
       result.bit_depth != 12)) {
    DVLOG(3) << __func__ << " Invalid bit-depth (" << fields[3] << ")";
    return std::nullopt;
  }

  if (values.size() <= 4) {
    return result;
  }

  // The monochrome parameter value, represented by a single digit decimal,
  // SHALL equal the value of mono_chrome in the Sequence Header.
  const int monochrome = values[4];
  if (fields[4].size() != 1 || monochrome > 1) {
    DVLOG(3) << __func__ << " Invalid monochrome (" << fields[4] << ")";
    return std::nullopt;
  }
  if (monochrome == 1 && result.profile == AV1PROFILE_PROFILE_HIGH) {
    DVLOG(3) << "Monochrome isn't supported in high profile.";
    return std::nullopt;
  }

  if (values.size() <= 5) {
    if (monochrome == 1) {
      result.subsampling = VideoChromaSampling::k400;
    }
    return result;
  }

  // The chromaSubsampling parameter value, represented by a three-digit
  // decimal, SHALL have its first digit equal to subsampling_x and its second
  // digit equal to subsampling_y. If both subsampling_x and subsampling_y are
  // set to 1, then the third digit SHALL be equal to chroma_sample_position,
  // otherwise it SHALL be set to 0.
  if (fields[5].size() != 3) {
    DVLOG(3) << __func__ << " Invalid chroma subsampling (" << fields[5] << ")";
    return std::nullopt;
  }

  const char subsampling_x = fields[5][0];
  const char subsampling_y = fields[5][1];
  const char chroma_sample_position = fields[5][2];
  if ((subsampling_x < '0' || subsampling_x > '1') ||
      (subsampling_y < '0' || subsampling_y > '1') ||
      (chroma_sample_position < '0' || chroma_sample_position > '3')) {
    DVLOG(3) << __func__ << " Invalid chroma subsampling (" << fields[5] << ")";
    return std::nullopt;
  }

  if (((subsampling_x == '0' || subsampling_y == '0') &&
       chroma_sample_position != '0')) {
    DVLOG(3) << __func__ << " Invalid chroma subsampling (" << fields[5] << ")";
    return std::nullopt;
  }

  if (subsampling_x == '0' && subsampling_y == '0' && monochrome == 0) {
    if (result.profile == AV1PROFILE_PROFILE_MAIN) {
      DVLOG(3) << __func__ << "4:4:4 isn't supported in main profile.";
    } else {
      result.subsampling = VideoChromaSampling::k444;
    }
  } else if (subsampling_x == '1' && subsampling_y == '0' && monochrome == 0) {
    if (result.profile != AV1PROFILE_PROFILE_PRO) {
      DVLOG(3) << __func__ << "4:2:2 is only supported in pro profile.";
    } else {
      result.subsampling = VideoChromaSampling::k422;
    }
  } else if (subsampling_x == '1' && subsampling_y == '1' && monochrome == 0) {
    result.subsampling = VideoChromaSampling::k420;
  } else if (subsampling_x == '1' && subsampling_y == '1' && monochrome == 1) {
    result.subsampling = VideoChromaSampling::k400;
  }

  if (values.size() <= 6) {
    return result;
  }

  // The colorPrimaries, transferCharacteristics, matrixCoefficients and
  // videoFullRangeFlag parameter values SHALL equal the value of matching
  // fields in the Sequence Header, if color_description_present_flag is set to
  // 1, otherwise they SHOULD not be set, defaulting to the values below. The
  // videoFullRangeFlag is represented by a single digit.
  result.color_space.primaries = VideoColorSpace::GetPrimaryID(values[6]);
  if (fields[6].size() != 2 ||
      result.color_space.primaries == VideoColorSpace::PrimaryID::INVALID) {
    DVLOG(3) << __func__ << " Invalid color primaries (" << fields[6] << ")";
    return std::nullopt;
  }

  if (values.size() <= 7) {
    return result;
  }

  result.color_space.transfer = VideoColorSpace::GetTransferID(values[7]);
  if (fields[7].size() != 2 ||
      result.color_space.transfer == VideoColorSpace::TransferID::INVALID) {
    DVLOG(3) << __func__ << " Invalid transfer function (" << fields[7] << ")";
    return std::nullopt;
  }

  if (values.size() <= 8) {
    return result;
  }

  result.color_space.matrix = VideoColorSpace::GetMatrixID(values[8]);
  if (fields[8].size() != 2 ||
      result.color_space.matrix == VideoColorSpace::MatrixID::INVALID) {
    // TODO(dalecurtis): AV1 allows a few matrices we don't support yet.
    // https://crbug.com/854290
    if (values[8] == 12 || values[8] == 13 || values[8] == 14) {
      DVLOG(3) << __func__ << " Unsupported matrix coefficients (" << fields[8]
               << ")";
    } else {
      DVLOG(3) << __func__ << " Invalid matrix coefficients (" << fields[8]
               << ")";
    }
    return std::nullopt;
  }

  if (values.size() <= 9) {
    return result;
  }

  const int video_full_range_flag = values[9];
  if (fields[9].size() != 1 || video_full_range_flag > 1) {
    DVLOG(3) << __func__ << " Invalid full range flag (" << fields[9] << ")";
    return std::nullopt;
  }
  result.color_space.range = video_full_range_flag == 1
                                 ? gfx::ColorSpace::RangeID::FULL
                                 : gfx::ColorSpace::RangeID::LIMITED;

  return result;
}

std::optional<VideoType> ParseAVCCodecId(std::string_view codec_id) {
  // Make sure we have avc1.xxxxxx or avc3.xxxxxx , where xxxxxx are hex digits
  if (!base::StartsWith(codec_id, "avc1.", base::CompareCase::SENSITIVE) &&
      !base::StartsWith(codec_id, "avc3.", base::CompareCase::SENSITIVE)) {
    return std::nullopt;
  }
  uint32_t elem = 0;
  if (codec_id.size() != 11 ||
      !base::HexStringToUInt(codec_id.substr(5), &elem)) {
    DVLOG(4) << __func__ << ": invalid avc codec id (" << codec_id << ")";
    return std::nullopt;
  }

  uint8_t level_byte = elem & 0xFF;
  uint8_t constraints_byte = (elem >> 8) & 0xFF;
  uint8_t profile_idc = (elem >> 16) & 0xFF;

  // Check that the lower two bits of |constraints_byte| are zero (those are
  // reserved and must be zero according to ISO IEC 14496-10).
  if (constraints_byte & 3) {
    DVLOG(4) << __func__ << ": non-zero reserved bits in codec id " << codec_id;
    return std::nullopt;
  }

  VideoCodecProfile out_profile = VIDEO_CODEC_PROFILE_UNKNOWN;
  // profile_idc values for each profile are taken from ISO IEC 14496-10 and
  // https://en.wikipedia.org/wiki/H.264/MPEG-4_AVC#Profiles
  switch (profile_idc) {
    case 66:
      out_profile = H264PROFILE_BASELINE;
      break;
    case 77:
      out_profile = H264PROFILE_MAIN;
      break;
    case 83:
      out_profile = H264PROFILE_SCALABLEBASELINE;
      break;
    case 86:
      out_profile = H264PROFILE_SCALABLEHIGH;
      break;
    case 88:
      out_profile = H264PROFILE_EXTENDED;
      break;
    case 100:
      out_profile = H264PROFILE_HIGH;
      break;
    case 110:
      out_profile = H264PROFILE_HIGH10PROFILE;
      break;
    case 118:
      out_profile = H264PROFILE_MULTIVIEWHIGH;
      break;
    case 122:
      out_profile = H264PROFILE_HIGH422PROFILE;
      break;
    case 128:
      out_profile = H264PROFILE_STEREOHIGH;
      break;
    case 244:
      out_profile = H264PROFILE_HIGH444PREDICTIVEPROFILE;
      break;
    default:
      DVLOG(1) << "Warning: unrecognized AVC/H.264 profile " << profile_idc;
      return std::nullopt;
  }

  // TODO(servolk): Take into account also constraint set flags 3 through 5.
  uint8_t constraint_set0_flag = (constraints_byte >> 7) & 1;
  uint8_t constraint_set1_flag = (constraints_byte >> 6) & 1;
  uint8_t constraint_set2_flag = (constraints_byte >> 5) & 1;
  if (constraint_set2_flag && out_profile > H264PROFILE_EXTENDED) {
    out_profile = H264PROFILE_EXTENDED;
  }
  if (constraint_set1_flag && out_profile > H264PROFILE_MAIN) {
    out_profile = H264PROFILE_MAIN;
  }
  if (constraint_set0_flag && out_profile > H264PROFILE_BASELINE) {
    out_profile = H264PROFILE_BASELINE;
  }

  VideoType result = {
      .codec = VideoCodec::kH264,
      .profile = out_profile,
      .level = level_byte,
  };
  return result;
}

// The specification for HEVC codec id strings can be found in ISO IEC 14496-15
// dated 2012 or newer in the Annex E.3
std::optional<VideoType> ParseHEVCCodecId(std::string_view codec_id) {
  if (!base::StartsWith(codec_id, "hev1.", base::CompareCase::SENSITIVE) &&
      !base::StartsWith(codec_id, "hvc1.", base::CompareCase::SENSITIVE)) {
    return std::nullopt;
  }

  // HEVC codec id consists of:
  const int kMaxHevcCodecIdLength =
      5 +  // 'hev1.' or 'hvc1.' prefix (5 chars)
      4 +  // profile, e.g. '.A12' (max 4 chars)
      9 +  // profile_compatibility, dot + 32-bit hex number (max 9 chars)
      5 +  // tier and level, e.g. '.H120' (max 5 chars)
      18;  // up to 6 constraint bytes, bytes are dot-separated and hex-encoded.

  if (codec_id.size() > kMaxHevcCodecIdLength) {
    DVLOG(4) << __func__ << ": Codec id is too long (" << codec_id << ")";
    return std::nullopt;
  }

  std::vector<std::string> elem = base::SplitString(
      codec_id, ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  DCHECK(elem[0] == "hev1" || elem[0] == "hvc1");

  if (elem.size() < 4) {
    DVLOG(4) << __func__ << ": invalid HEVC codec id " << codec_id;
    return std::nullopt;
  }

  uint8_t general_profile_space = 0;
  if (elem[1].size() > 0 &&
      (elem[1][0] == 'A' || elem[1][0] == 'B' || elem[1][0] == 'C')) {
    general_profile_space = 1 + (elem[1][0] - 'A');
    elem[1].erase(0, 1);
  }
  DCHECK(general_profile_space >= 0 && general_profile_space <= 3);

  unsigned general_profile_idc = 0;
  if (!base::StringToUint(elem[1], &general_profile_idc) ||
      general_profile_idc > 0x1f) {
    DVLOG(4) << __func__ << ": invalid general_profile_idc=" << elem[1];
    return std::nullopt;
  }

  uint32_t general_profile_compatibility_flags = 0;
  if (!base::HexStringToUInt(elem[2], &general_profile_compatibility_flags)) {
    DVLOG(4) << __func__
             << ": invalid general_profile_compatibility_flags=" << elem[2];
    return std::nullopt;
  }

  VideoCodecProfile out_profile = VIDEO_CODEC_PROFILE_UNKNOWN;
  // Spec A.3.8
  if (general_profile_idc == 11 ||
      (general_profile_compatibility_flags & 2048)) {
    out_profile = HEVCPROFILE_HIGH_THROUGHPUT_SCREEN_EXTENDED;
  }
  // Spec H.11.1.2
  if (general_profile_idc == 10 ||
      (general_profile_compatibility_flags & 1024)) {
    out_profile = HEVCPROFILE_SCALABLE_REXT;
  }
  // Spec A.3.7
  if (general_profile_idc == 9 || (general_profile_compatibility_flags & 512)) {
    out_profile = HEVCPROFILE_SCREEN_EXTENDED;
  }
  // Spec I.11.1.1
  if (general_profile_idc == 8 || (general_profile_compatibility_flags & 256)) {
    out_profile = HEVCPROFILE_3D_MAIN;
  }
  // Spec H.11.1.1
  if (general_profile_idc == 7 || (general_profile_compatibility_flags & 128)) {
    out_profile = HEVCPROFILE_SCALABLE_MAIN;
  }
  // Spec G.11.1.1
  if (general_profile_idc == 6 || (general_profile_compatibility_flags & 64)) {
    out_profile = HEVCPROFILE_MULTIVIEW_MAIN;
  }
  // Spec A.3.6
  if (general_profile_idc == 5 || (general_profile_compatibility_flags & 32)) {
    out_profile = HEVCPROFILE_HIGH_THROUGHPUT;
  }
  // Spec A.3.5
  if (general_profile_idc == 4 || (general_profile_compatibility_flags & 16)) {
    out_profile = HEVCPROFILE_REXT;
  }
  // Spec A.3.3
  // NOTICE: Do not change the order of below sections
  if (general_profile_idc == 2 || (general_profile_compatibility_flags & 4)) {
    out_profile = HEVCPROFILE_MAIN10;
  }
  // Spec A.3.2
  // When general_profile_compatibility_flag[1] is equal to 1,
  // general_profile_compatibility_flag[2] should be equal to 1 as well.
  if (general_profile_idc == 1 || (general_profile_compatibility_flags & 2)) {
    out_profile = HEVCPROFILE_MAIN;
  }
  // Spec A.3.4
  // When general_profile_compatibility_flag[3] is equal to 1,
  // general_profile_compatibility_flag[1] and
  // general_profile_compatibility_flag[2] should be equal to 1 as well.
  if (general_profile_idc == 3 || (general_profile_compatibility_flags & 8)) {
    out_profile = HEVCPROFILE_MAIN_STILL_PICTURE;
  }

  if (out_profile == VIDEO_CODEC_PROFILE_UNKNOWN) {
    DVLOG(1) << "Warning: unrecognized HEVC/H.265 general_profile_idc: "
             << general_profile_idc << ", general_profile_compatibility_flags: "
             << general_profile_compatibility_flags;
    return std::nullopt;
  }

  uint8_t general_tier_flag;
  if (elem[3].size() > 0 && (elem[3][0] == 'L' || elem[3][0] == 'H')) {
    general_tier_flag = (elem[3][0] == 'L') ? 0 : 1;
    elem[3].erase(0, 1);
  } else {
    DVLOG(4) << __func__ << ": invalid general_tier_flag=" << elem[3];
    return std::nullopt;
  }
  DCHECK(general_tier_flag == 0 || general_tier_flag == 1);

  unsigned general_level_idc = 0;
  if (!base::StringToUint(elem[3], &general_level_idc) ||
      general_level_idc > 0xff) {
    DVLOG(4) << __func__ << ": invalid general_level_idc=" << elem[3];
    return std::nullopt;
  }

  uint8_t constraint_flags[6];
  memset(constraint_flags, 0, sizeof(constraint_flags));

  if (elem.size() > 10) {
    DVLOG(4) << __func__ << ": unexpected number of trailing bytes in HEVC "
             << "codec id " << codec_id;
    return std::nullopt;
  }
  for (size_t i = 4; i < elem.size(); ++i) {
    unsigned constr_byte = 0;
    if (!base::HexStringToUInt(elem[i], &constr_byte) || constr_byte > 0xFF) {
      DVLOG(4) << __func__ << ": invalid constraint byte=" << elem[i];
      return std::nullopt;
    }
    constraint_flags[i - 4] = constr_byte;
  }

  VideoType result = {
      .codec = VideoCodec::kHEVC,
      .profile = out_profile,
      .level = general_level_idc,
  };
  return result;
}
// The specification for VVC codec id strings can be found in ISO/IEC 14496-15
// 2022, annex E.6.
// In detail it would be:
// <sample entry FourCC>    ("vvi1: if config is inband, or "vvc1" otherwise.)
// .<general_profile_idc>   (base10)
// .<general_tier_flag>     ("L" or "H")
// <op_level_idc>           (base10. <= general_level_idc in SPS)
// .C<ptl_frame_only_constraint_flag><ptl_multi_layer_enabled_flag> (optional)
// <general_constraint_info)  (base32 with "=" might be omitted.)
// .S<general_sub_profile_idc1>  (Optional, base32 with "=" might be omitted.)
// <+general_sub_profile_
// .O<ols_idx>+<max_tid>   (Optional, base10 OlsIdx & MaxTid)
std::optional<VideoType> ParseVVCCodecId(std::string_view codec_id) {
  if (!base::StartsWith(codec_id, "vvc1.", base::CompareCase::SENSITIVE) &&
      !base::StartsWith(codec_id, "vvi1.", base::CompareCase::SENSITIVE)) {
    return std::nullopt;
  }

  std::vector<std::string> elem = base::SplitString(
      codec_id, ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  DCHECK(elem[0] == "vvc1" || elem[0] == "vvi1");

  if (elem.size() < 3 || elem.size() > 6) {
    DVLOG(4) << __func__ << ": invalid VVC codec id " << codec_id;
    return std::nullopt;
  }

  for (auto& item : elem) {
    if (item.size() < 1 ||
        ((item[0] == 'C' || item[0] == 'S' || item[0] == 'O') &&
         item.size() < 2)) {
      DVLOG(4) << __func__ << ": subelement of VVC codec id invalid.";
      return std::nullopt;
    }
    if (item[0] == 'O' && item.back() == '+') {
      DVLOG(4) << __func__ << ": invalid OlxIdx and MaxTid string.";
      return std::nullopt;
    }
  }

  unsigned general_profile_idc = 0;
  if (!base::StringToUint(elem[1], &general_profile_idc) ||
      general_profile_idc > 0x63) {
    DVLOG(4) << __func__ << ": invalid general_profile_idc=" << elem[1];
    return std::nullopt;
  }

  VideoCodecProfile out_profile = VIDEO_CODEC_PROFILE_UNKNOWN;
  switch (general_profile_idc) {
    case 99:  // Spec A.3.5
      out_profile = VVCPROFILE_MAIN16_444_STILL_PICTURE;
      break;
    case 98:  // Spec A.3.5
      out_profile = VVCPROFILE_MAIN12_444_STILL_PICTURE;
      break;
    case 97:  // Spec A.3.2
      out_profile = VVCPROFILE_MAIN10_444_STILL_PICTURE;
      break;
    case 66:  // Spec A.3.5
      out_profile = VVCPROFILE_MAIN12_STILL_PICTURE;
      break;
    case 65:  // Spec A.3.1
      out_profile = VVCPROFILE_MAIN10_STILL_PICTURE;
      break;
    case 49:  // Spec A.3.4
      out_profile = VVCPROFILE_MULTILAYER_MAIN10_444;
      break;
    case 43:  // Spec A.3.5
      out_profile = VVCPROFILE_MAIN16_444_INTRA;
      break;
    case 42:  // Spec A.3.5
      out_profile = VVCPROFILE_MAIN12_444_INTRA;
      break;
    case 35:  // Spec A.3.5
      out_profile = VVCPROFILE_MAIN16_444;
      break;
    case 34:  // Spec A.3.5
      out_profile = VVCPROFILE_MAIN12_444;
      break;
    case 33:  // Spec A.3.2
      out_profile = VVCPROFILE_MAIN10_444;
      break;
    case 17:  // Spec A.3.3
      out_profile = VVCPROIFLE_MULTILAYER_MAIN10;
      break;
    case 10:  // Spec A.3.5
      out_profile = VVCPROFILE_MAIN12_INTRA;
      break;
    case 2:  // Spec A.3.5
      out_profile = VVCPROFILE_MAIN12;
      break;
    case 1:  // Spec A.3.1
      out_profile = VVCPROFILE_MAIN10;
      break;
    default:
      break;
  }

  if (out_profile == VIDEO_CODEC_PROFILE_UNKNOWN) {
    DVLOG(1) << "Warning: unrecognized VVC/H.266 general_profile_idc: "
             << general_profile_idc;
    return std::nullopt;
  }

  uint8_t general_tier_flag;
  if (elem[2][0] == 'L' || elem[2][0] == 'H') {
    general_tier_flag = (elem[2][0] == 'L') ? 0 : 1;
    elem[2].erase(0, 1);
  } else {
    DVLOG(4) << __func__ << ": invalid general_tier_flag=" << elem[2];
    return std::nullopt;
  }
  DCHECK(general_tier_flag == 0 || general_tier_flag == 1);

  unsigned general_level_idc = 0;
  if (!base::StringToUint(elem[2], &general_level_idc) ||
      general_level_idc > 0xff) {
    DVLOG(4) << __func__ << ": invalid general_level_idc=" << elem[2];
    return std::nullopt;
  }

  // C-string, if existing, should proceed S-string and O-string.
  // Similarly, S-string should proceed O-string.
  bool trailing_valid = true;
  if (elem.size() == 4) {
    if (elem[3][0] != 'C' && elem[3][0] != 'S' && elem[3][0] != 'O') {
      trailing_valid = false;
    }
  } else if (elem.size() == 5) {
    if (!((elem[3][0] == 'C' && elem[4][0] == 'S') ||
          (elem[3][0] == 'C' && elem[4][0] == 'O') ||
          (elem[3][0] == 'S' && elem[4][0] == 'O'))) {
      trailing_valid = false;
    }
  } else if (elem.size() == 6) {
    if (elem[3][0] != 'C' || elem[4][0] != 'S' || elem[5][0] != 'O') {
      trailing_valid = false;
    }
  }

  if (!trailing_valid) {
    DVLOG(4) << __func__ << ": invalid traing codec string.";
    return std::nullopt;
  }

  // TODO(crbug.com/40257449): Add VideoCodec::kVVC here when its ready.
  VideoType result = {
      .codec = VideoCodec::kUnknown,
      .profile = out_profile,
      .level = general_level_idc,
  };
  return result;
}

// The specification for Dolby Vision codec id strings can be found in Dolby
// Vision streams within the MPEG-DASH format:
// https://professional.dolby.com/siteassets/content-creation/dolby-vision-for-content-creators/dolbyvisioninmpegdashspecification_v2_0_public_20190107.pdf
std::optional<VideoType> ParseDolbyVisionCodecId(std::string_view codec_id) {
  if (!IsDolbyVisionAVCCodecId(codec_id) &&
      !IsDolbyVisionHEVCCodecId(codec_id)) {
    return std::nullopt;
  }

  const int kMaxDvCodecIdLength = 5     // FOURCC string
                                  + 1   // delimiting period
                                  + 2   // profile id as 2 digit string
                                  + 1   // delimiting period
                                  + 2;  // level id as 2 digit string.

  if (codec_id.size() > kMaxDvCodecIdLength) {
    DVLOG(4) << __func__ << ": Codec id is too long (" << codec_id << ")";
    return std::nullopt;
  }

  std::vector<std::string> elem = base::SplitString(
      codec_id, ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  DCHECK(elem[0] == "dvh1" || elem[0] == "dvhe" || elem[0] == "dva1" ||
         elem[0] == "dvav");

  if (elem.size() != 3) {
    DVLOG(4) << __func__ << ": invalid dolby vision codec id " << codec_id;
    return std::nullopt;
  }

  // Profile string should be two digits.
  unsigned profile_id = 0;
  if (elem[1].size() != 2 || !base::StringToUint(elem[1], &profile_id) ||
      profile_id > 9) {
    DVLOG(4) << __func__ << ": invalid format or profile_id=" << elem[1];
    return std::nullopt;
  }

  VideoType result = {
      .codec = VideoCodec::kDolbyVision,
  };

  // Only profiles 0, 4, 5, 7, 8 and 9 are valid. Profile 0 and 9 are encoded
  // based on AVC while profile 4, 5, 7 and 8 are based on HEVC.
  switch (profile_id) {
    case 0:
    case 9:
      if (!IsDolbyVisionAVCCodecId(codec_id)) {
        DVLOG(4) << __func__
                 << ": codec id is mismatched with profile_id=" << profile_id;
        return std::nullopt;
      }
      if (profile_id == 0) {
        result.profile = DOLBYVISION_PROFILE0;
      } else if (profile_id == 9) {
        result.profile = DOLBYVISION_PROFILE9;
      }
      break;
    case 5:
    case 7:
    case 8:
      if (!IsDolbyVisionHEVCCodecId(codec_id)) {
        DVLOG(4) << __func__
                 << ": codec id is mismatched with profile_id=" << profile_id;
        return std::nullopt;
      }
      if (profile_id == 5) {
        result.profile = DOLBYVISION_PROFILE5;
      } else if (profile_id == 7) {
        result.profile = DOLBYVISION_PROFILE7;
      } else if (profile_id == 8) {
        result.profile = DOLBYVISION_PROFILE8;
      }
      break;
    default:
      DVLOG(4) << __func__
               << ": depecrated and not supported profile_id=" << profile_id;
      return std::nullopt;
  }

  // Level string should be two digits.
  unsigned level_id = 0;
  if (elem[2].size() != 2 || !base::StringToUint(elem[2], &level_id) ||
      level_id > 13 || level_id < 1) {
    DVLOG(4) << __func__ << ": invalid format level_id=" << elem[2];
    return std::nullopt;
  }

  result.level = level_id;
  return result;
}

std::optional<VideoType> ParseCodec(std::string_view codec_id) {
  std::vector<std::string> elem = base::SplitString(
      codec_id, ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (elem.empty()) {
    return std::nullopt;
  }

  if (codec_id == "vp8" || codec_id == "vp8.0") {
    VideoType result = {
        .codec = VideoCodec::kVP8,
        .profile = VP8PROFILE_ANY,
        .level = kNoVideoCodecLevel,
    };
    return result;
  }

  if (codec_id == "theora") {
    VideoType result = {
        .codec = VideoCodec::kTheora,
        .profile = THEORAPROFILE_ANY,
        .level = kNoVideoCodecLevel,
    };
    return result;
  }

  if (auto result = ParseNewStyleVp9CodecID(codec_id)) {
    return result;
  }

  if (auto result = ParseLegacyVp9CodecID(codec_id)) {
    return result;
  }

  if (auto result = ParseAv1CodecId(codec_id)) {
    return result;
  }

  if (auto result = ParseAVCCodecId(TranslateLegacyAvc1CodecIds(codec_id))) {
    return result;
  }

  if (auto result = ParseHEVCCodecId(codec_id)) {
    return result;
  }

  if (auto result = ParseDolbyVisionCodecId(codec_id)) {
    return result;
  }

  return std::nullopt;
}

VideoCodec StringToVideoCodec(std::string_view codec_id) {
  auto result = ParseCodec(codec_id);
  return result ? result->codec : VideoCodec::kUnknown;
}

std::string TranslateLegacyAvc1CodecIds(std::string_view codec_id) {
  // Special handling for old, pre-RFC 6381 format avc1 strings, which are still
  // being used by some HLS apps to preserve backward compatibility with older
  // iOS devices. The old format was avc1.<profile>.<level>
  // Where <profile> is H.264 profile_idc encoded as a decimal number, i.e.
  // 66 is baseline profile (0x42)
  // 77 is main profile (0x4d)
  // 100 is high profile (0x64)
  // And <level> is H.264 level multiplied by 10, also encoded as decimal number
  // E.g. <level> 31 corresponds to H.264 level 3.1
  // See, for example, http://qtdevseed.apple.com/qadrift/testcases/tc-0133.php
  uint32_t level_start = 0;
  std::string result;
  if (base::StartsWith(codec_id, "avc1.66.", base::CompareCase::SENSITIVE)) {
    level_start = 8;
    result = "avc1.4200";
  } else if (base::StartsWith(codec_id, "avc1.77.",
                              base::CompareCase::SENSITIVE)) {
    level_start = 8;
    result = "avc1.4D00";
  } else if (base::StartsWith(codec_id, "avc1.100.",
                              base::CompareCase::SENSITIVE)) {
    level_start = 9;
    result = "avc1.6400";
  }

  uint32_t level = 0;
  if (level_start > 0 &&
      base::StringToUint(codec_id.substr(level_start), &level) && level < 256) {
    // This is a valid legacy avc1 codec id - return the codec id translated
    // into RFC 6381 format.
    base::AppendHexEncodedByte(static_cast<uint8_t>(level), result);
    return result;
  }

  // This is not a valid legacy avc1 codec id - return the original codec id.
  return std::string(codec_id);
}

}  // namespace media
