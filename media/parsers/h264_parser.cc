// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/parsers/h264_parser.h"

#include <cstring>
#include <limits>
#include <memory>

#include "base/logging.h"
#include "base/notreached.h"
#include "base/numerics/safe_math.h"
#include "media/base/subsample_entry.h"
#include "media/parsers/bit_reader_macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/hdr_metadata.h"

namespace media {

bool H264SliceHeader::IsPSlice() const {
  return (slice_type % 5 == kPSlice);
}

bool H264SliceHeader::IsBSlice() const {
  return (slice_type % 5 == kBSlice);
}

bool H264SliceHeader::IsISlice() const {
  return (slice_type % 5 == kISlice);
}

bool H264SliceHeader::IsSPSlice() const {
  return (slice_type % 5 == kSPSlice);
}

bool H264SliceHeader::IsSISlice() const {
  return (slice_type % 5 == kSISlice);
}

H264NALU::H264NALU() = default;

// static
void H264SPS::GetLevelConfigFromProfileLevel(VideoCodecProfile profile,
                                             uint8_t level,
                                             int* level_idc,
                                             bool* constraint_set3_flag) {
  // Spec A.3.1.
  // Note: we always use h264_output_level = 9 to indicate Level 1b in
  //       VideoEncodeAccelerator::Config, in order to tell apart from Level 1.1
  //       which level IDC is also 11.
  // For Baseline and Main profile, if requested level is Level 1b, set
  // level_idc to 11 and constraint_set3_flag to true. Otherwise, set level_idc
  // to 9 for Level 1b, and ten times level number for others.
  if ((profile == H264PROFILE_BASELINE || profile == H264PROFILE_MAIN) &&
      level == kLevelIDC1B) {
    *level_idc = 11;
    *constraint_set3_flag = true;
  } else {
    *level_idc = level;
  }
}

H264SPS::H264SPS() {
  memset(this, 0, sizeof(*this));
}

H264SPS& H264SPS::operator=(const H264SPS&) = default;

H264SPS::H264SPS(H264SPS&&) noexcept = default;

// Based on T-REC-H.264 7.4.2.1.1, "Sequence parameter set data semantics",
// available from http://www.itu.int/rec/T-REC-H.264.
std::optional<gfx::Size> H264SPS::GetCodedSize() const {
  // Interlaced frames are twice the height of each field.
  const int mb_unit = 16;
  int map_unit = frame_mbs_only_flag ? 16 : 32;

  // Verify that the values are not too large before multiplying them.
  // TODO(sandersd): These limits could be much smaller. The currently-largest
  // specified limit (excluding SVC, multiview, etc., which I didn't bother to
  // read) is 543 macroblocks (section A.3.1).
  int max_mb_minus1 = std::numeric_limits<int>::max() / mb_unit - 1;
  int max_map_units_minus1 = std::numeric_limits<int>::max() / map_unit - 1;
  if (pic_width_in_mbs_minus1 > max_mb_minus1 ||
      pic_height_in_map_units_minus1 > max_map_units_minus1) {
    DVLOG(1) << "Coded size is too large.";
    return std::nullopt;
  }

  return gfx::Size(mb_unit * (pic_width_in_mbs_minus1 + 1),
                   map_unit * (pic_height_in_map_units_minus1 + 1));
}

// Also based on section 7.4.2.1.1.
std::optional<gfx::Rect> H264SPS::GetVisibleRect() const {
  std::optional<gfx::Size> coded_size = GetCodedSize();
  if (!coded_size)
    return std::nullopt;

  if (!frame_cropping_flag)
    return gfx::Rect(coded_size.value());

  int crop_unit_x;
  int crop_unit_y;
  if (chroma_array_type == 0) {
    crop_unit_x = 1;
    crop_unit_y = frame_mbs_only_flag ? 1 : 2;
  } else {
    // Section 6.2.
    // |chroma_format_idc| may be:
    //   1 => 4:2:0
    //   2 => 4:2:2
    //   3 => 4:4:4
    // Everything else has |chroma_array_type| == 0.
    int sub_width_c = chroma_format_idc > 2 ? 1 : 2;
    int sub_height_c = chroma_format_idc > 1 ? 1 : 2;
    crop_unit_x = sub_width_c;
    crop_unit_y = sub_height_c * (frame_mbs_only_flag ? 1 : 2);
  }

  // Verify that the values are not too large before multiplying.
  if (coded_size->width() / crop_unit_x < frame_crop_left_offset ||
      coded_size->width() / crop_unit_x < frame_crop_right_offset ||
      coded_size->height() / crop_unit_y < frame_crop_top_offset ||
      coded_size->height() / crop_unit_y < frame_crop_bottom_offset) {
    DVLOG(1) << "Frame cropping exceeds coded size.";
    return std::nullopt;
  }
  int crop_left = crop_unit_x * frame_crop_left_offset;
  int crop_right = crop_unit_x * frame_crop_right_offset;
  int crop_top = crop_unit_y * frame_crop_top_offset;
  int crop_bottom = crop_unit_y * frame_crop_bottom_offset;

  // Verify that the values are sane. Note that some decoders also require that
  // crops are smaller than a macroblock and/or that crops must be adjacent to
  // at least one corner of the coded frame.
  if (coded_size->width() - crop_left <= crop_right ||
      coded_size->height() - crop_top <= crop_bottom) {
    DVLOG(1) << "Frame cropping excludes entire frame.";
    return std::nullopt;
  }

  return gfx::Rect(crop_left, crop_top,
                   coded_size->width() - crop_left - crop_right,
                   coded_size->height() - crop_top - crop_bottom);
}

// Based on T-REC-H.264 E.2.1, "VUI parameters semantics",
// available from http://www.itu.int/rec/T-REC-H.264.
VideoColorSpace H264SPS::GetColorSpace() const {
  if (colour_description_present_flag) {
    return VideoColorSpace(
        colour_primaries, transfer_characteristics, matrix_coefficients,
        video_full_range_flag ? gfx::ColorSpace::RangeID::FULL
                              : gfx::ColorSpace::RangeID::LIMITED);
  } else {
    return VideoColorSpace();
  }
}

VideoChromaSampling H264SPS::GetChromaSampling() const {
  // Spec section 6.2
  switch (chroma_format_idc) {
    case 0:
      return VideoChromaSampling::k400;
    case 1:
      return VideoChromaSampling::k420;
    case 2:
      return VideoChromaSampling::k422;
    case 3:
      return VideoChromaSampling::k444;
    default:
      DVLOG(1) << "Unknown chroma subsampling format.";
      return VideoChromaSampling::kUnknown;
  }
}

uint8_t H264SPS::GetIndicatedLevel() const {
  // Spec A.3.1 and A.3.2
  // For Baseline, Constrained Baseline and Main profile, the indicated level is
  // Level 1b if level_idc is equal to 11 and constraint_set3_flag is true.
  if ((profile_idc == H264SPS::kProfileIDCBaseline ||
       profile_idc == H264SPS::kProfileIDCConstrainedBaseline ||
       profile_idc == H264SPS::kProfileIDCMain) &&
      level_idc == 11 && constraint_set3_flag) {
    return kLevelIDC1B;  // Level 1b
  }

  // Otherwise, the level_idc is equal to 9 for Level 1b, and others are equal
  // to values of ten times the level numbers.
  return base::checked_cast<uint8_t>(level_idc);
}

bool H264SPS::CheckIndicatedLevelWithinTarget(uint8_t target_level) const {
  // See table A-1 in spec.
  // Level 1.0 < 1b < 1.1 < 1.2 .... (in numeric order).
  uint8_t level = GetIndicatedLevel();
  if (target_level == kLevelIDC1p0)
    return level == kLevelIDC1p0;
  if (target_level == kLevelIDC1B)
    return level == kLevelIDC1p0 || level == kLevelIDC1B;
  return level <= target_level;
}

H264PPS::H264PPS() {
  memset(this, 0, sizeof(*this));
}

H264PPS::H264PPS(H264PPS&&) noexcept = default;

H264SliceHeader::H264SliceHeader() {
  memset(this, 0, sizeof(*this));
}

H264SliceHeader::H264SliceHeader(const H264SliceHeader& t) = default;
H264SliceHeader& H264SliceHeader::operator=(const H264SliceHeader& t) = default;

H264SEIMessage::H264SEIMessage() {
  memset(this, 0, sizeof(*this));
}

gfx::HdrMetadataCta861_3 H264SEIContentLightLevelInfo::ToGfx() const {
  return gfx::HdrMetadataCta861_3(max_content_light_level,
                                  max_picture_average_light_level);
}

gfx::HdrMetadataSmpteSt2086 H264SEIMasteringDisplayInfo::ToGfx() const {
  constexpr auto kChromaDenominator = 50000.0f;
  constexpr auto kLumaDenoninator = 10000.0f;
  // display primaries are in G/B/R order in MDCV SEI.
  return gfx::HdrMetadataSmpteSt2086(
      {display_primaries[2][0] / kChromaDenominator,
       display_primaries[2][1] / kChromaDenominator,
       display_primaries[0][0] / kChromaDenominator,
       display_primaries[0][1] / kChromaDenominator,
       display_primaries[1][0] / kChromaDenominator,
       display_primaries[1][1] / kChromaDenominator,
       white_points[0] / kChromaDenominator,
       white_points[1] / kChromaDenominator},
      /*luminance_max=*/max_luminance / kLumaDenoninator,
      /*luminance_min=*/min_luminance / kLumaDenoninator);
}

H264SEI::H264SEI() = default;

H264SEI::~H264SEI() = default;

// ISO 14496 part 10
// VUI parameters: Table E-1 "Meaning of sample aspect ratio indicator"
static const int kTableSarWidth[] = {0,  1,  12, 10, 16,  40, 24, 20, 32,
                                     80, 18, 15, 64, 160, 4,  3,  2};
static const int kTableSarHeight[] = {0,  1,  11, 11, 11, 33, 11, 11, 11,
                                      33, 11, 11, 33, 99, 3,  2,  1};
static_assert(std::size(kTableSarWidth) == std::size(kTableSarHeight),
              "sar tables must have the same size");

H264Parser::H264Parser() {
  Reset();
}

H264Parser::~H264Parser() = default;

void H264Parser::Reset() {
  stream_ = nullptr;
  bytes_left_ = 0;
  encrypted_ranges_.clear();
  previous_nalu_range_.clear();
}

void H264Parser::SetStream(const uint8_t* stream, off_t stream_size) {
  std::vector<SubsampleEntry> subsamples;
  SetEncryptedStream(stream, stream_size, subsamples);
}

void H264Parser::SetEncryptedStream(
    const uint8_t* stream,
    off_t stream_size,
    const std::vector<SubsampleEntry>& subsamples) {
  DCHECK(stream);
  DCHECK_GT(stream_size, 0);

  stream_ = stream;
  bytes_left_ = stream_size;
  previous_nalu_range_.clear();

  encrypted_ranges_.clear();
  const uint8_t* start = stream;
  const uint8_t* stream_end = stream_ + base::checked_cast<size_t>(bytes_left_);
  for (size_t i = 0; i < subsamples.size() && start < stream_end; ++i) {
    start += subsamples[i].clear_bytes;

    const uint8_t* end =
        std::min(start + subsamples[i].cypher_bytes, stream_end);
    encrypted_ranges_.Add(start, end);
    start = end;
  }
}

const H264PPS* H264Parser::GetPPS(int pps_id) const {
  auto it = active_PPSes_.find(pps_id);
  if (it == active_PPSes_.end()) {
    DVLOG(1) << "Requested a nonexistent PPS id " << pps_id;
    return nullptr;
  }

  return it->second.get();
}

const H264SPS* H264Parser::GetSPS(int sps_id) const {
  auto it = active_SPSes_.find(sps_id);
  if (it == active_SPSes_.end()) {
    DVLOG(1) << "Requested a nonexistent SPS id " << sps_id;
    return nullptr;
  }

  return it->second.get();
}

static inline bool IsStartCode(const uint8_t* data) {
  return data[0] == 0x00 && data[1] == 0x00 && data[2] == 0x01;
}

// static
bool H264Parser::FindStartCode(const uint8_t* data,
                               off_t data_size,
                               off_t* offset,
                               off_t* start_code_size) {
  DCHECK_GE(data_size, 0);
  off_t bytes_left = data_size;

  while (bytes_left >= 3) {
    // The start code is "\0\0\1", ones are more unusual than zeroes, so let's
    // search for it first.
    const uint8_t* tmp =
        reinterpret_cast<const uint8_t*>(memchr(data + 2, 1, bytes_left - 2));
    if (!tmp) {
      data += bytes_left - 2;
      bytes_left = 2;
      break;
    }
    tmp -= 2;
    bytes_left -= tmp - data;
    data = tmp;

    if (IsStartCode(data)) {
      // Found three-byte start code, set pointer at its beginning.
      *offset = data_size - bytes_left;
      *start_code_size = 3;

      // If there is a zero byte before this start code,
      // then it's actually a four-byte start code, so backtrack one byte.
      if (*offset > 0 && *(data - 1) == 0x00) {
        --(*offset);
        ++(*start_code_size);
      }

      return true;
    }

    ++data;
    --bytes_left;
  }

  // End of data: offset is pointing to the first byte that was not considered
  // as a possible start of a start code.
  // Note: there is no security issue when receiving a negative |data_size|
  // since in this case, |bytes_left| is equal to |data_size| and thus
  // |*offset| is equal to 0 (valid offset).
  *offset = data_size - bytes_left;
  *start_code_size = 0;
  return false;
}

bool H264Parser::LocateNALU(off_t* nalu_size, off_t* start_code_size) {
  // Find the start code of next NALU.
  off_t nalu_start_off = 0;
  off_t annexb_start_code_size = 0;

  if (!FindStartCodeInClearRanges(stream_, bytes_left_, encrypted_ranges_,
                                  &nalu_start_off, &annexb_start_code_size)) {
    DVLOG(4) << "Could not find start code, end of stream?";
    return false;
  }

  // Move the stream to the beginning of the NALU (pointing at the start code).
  stream_ += base::checked_cast<size_t>(nalu_start_off);
  bytes_left_ -= nalu_start_off;

  const uint8_t* nalu_data =
      stream_ + base::checked_cast<size_t>(annexb_start_code_size);
  off_t max_nalu_data_size = bytes_left_ - annexb_start_code_size;
  if (max_nalu_data_size <= 0) {
    DVLOG(3) << "End of stream";
    return false;
  }

  // Find the start code of next NALU;
  // if successful, |nalu_size_without_start_code| is the number of bytes from
  // after previous start code to before this one;
  // if next start code is not found, it is still a valid NALU since there
  // are some bytes left after the first start code: all the remaining bytes
  // belong to the current NALU.
  off_t next_start_code_size = 0;
  off_t nalu_size_without_start_code = 0;
  if (!FindStartCodeInClearRanges(
          nalu_data, max_nalu_data_size, encrypted_ranges_,
          &nalu_size_without_start_code, &next_start_code_size)) {
    nalu_size_without_start_code = max_nalu_data_size;
  }
  *nalu_size = nalu_size_without_start_code + annexb_start_code_size;
  *start_code_size = annexb_start_code_size;
  return true;
}

// static
bool H264Parser::FindStartCodeInClearRanges(
    const uint8_t* data,
    off_t data_size,
    const Ranges<const uint8_t*>& encrypted_ranges,
    off_t* offset,
    off_t* start_code_size) {
  if (encrypted_ranges.size() == 0)
    return FindStartCode(data, data_size, offset, start_code_size);

  DCHECK_GE(data_size, 0);
  const uint8_t* start = data;
  do {
    off_t bytes_left = data_size - (start - data);

    if (!FindStartCode(start, bytes_left, offset, start_code_size))
      return false;

    // Construct a Ranges object that represents the region occupied
    // by the start code and the 1 byte needed to read the NAL unit type.
    const uint8_t* start_code = start + *offset;
    const uint8_t* start_code_end = start_code + *start_code_size;
    Ranges<const uint8_t*> start_code_range;
    start_code_range.Add(start_code, start_code_end + 1);

    if (encrypted_ranges.IntersectionWith(start_code_range).size() > 0) {
      // The start code is inside an encrypted section so we need to scan
      // for another start code.
      *start_code_size = 0;
      start += std::min(*offset + 1, bytes_left);
    }
  } while (*start_code_size == 0);

  // Update |*offset| to include the data we skipped over.
  *offset += start - data;
  return true;
}

// static
VideoCodecProfile H264Parser::ProfileIDCToVideoCodecProfile(int profile_idc) {
  switch (profile_idc) {
    case H264SPS::kProfileIDCBaseline:
      return H264PROFILE_BASELINE;
    case H264SPS::kProfileIDCMain:
      return H264PROFILE_MAIN;
    case H264SPS::kProfileIDCHigh:
      return H264PROFILE_HIGH;
    case H264SPS::kProfileIDHigh10:
      return H264PROFILE_HIGH10PROFILE;
    case H264SPS::kProfileIDHigh422:
      return H264PROFILE_HIGH422PROFILE;
    case H264SPS::kProfileIDHigh444Predictive:
      return H264PROFILE_HIGH444PREDICTIVEPROFILE;
    case H264SPS::kProfileIDScalableBaseline:
      return H264PROFILE_SCALABLEBASELINE;
    case H264SPS::kProfileIDScalableHigh:
      return H264PROFILE_SCALABLEHIGH;
    case H264SPS::kProfileIDStereoHigh:
      return H264PROFILE_STEREOHIGH;
    case H264SPS::kProfileIDSMultiviewHigh:
      return H264PROFILE_MULTIVIEWHIGH;
  }
  DVLOG(1) << "unknown video profile: " << profile_idc;
  return VIDEO_CODEC_PROFILE_UNKNOWN;
}

// static
bool H264Parser::ParseNALUs(const uint8_t* stream,
                            size_t stream_size,
                            std::vector<H264NALU>* nalus) {
  DCHECK(nalus);
  H264Parser parser;
  parser.SetStream(stream, stream_size);

  while (true) {
    H264NALU nalu;
    const H264Parser::Result result = parser.AdvanceToNextNALU(&nalu);
    if (result == H264Parser::kOk) {
      nalus->push_back(nalu);
    } else if (result == media::H264Parser::kEOStream) {
      return true;
    } else {
      DLOG(ERROR) << "Unexpected H264 parser result";
      return false;
    }
  }
}

H264Parser::Result H264Parser::AdvanceToNextNALU(H264NALU* nalu) {
  off_t start_code_size;
  off_t nalu_size_with_start_code;
  if (!LocateNALU(&nalu_size_with_start_code, &start_code_size)) {
    DVLOG(4) << "Could not find next NALU, bytes left in stream: "
             << bytes_left_;
    stream_ = nullptr;
    bytes_left_ = 0;
    return kEOStream;
  }

  nalu->data = stream_ + base::checked_cast<size_t>(start_code_size);
  nalu->size = nalu_size_with_start_code - start_code_size;
  DVLOG(4) << "NALU found: size=" << nalu_size_with_start_code;

  // Initialize bit reader at the start of found NALU.
  if (!br_.Initialize(nalu->data, nalu->size)) {
    stream_ = nullptr;
    bytes_left_ = 0;
    return kEOStream;
  }

  // Move parser state to after this NALU, so next time AdvanceToNextNALU
  // is called, we will effectively be skipping it;
  // other parsing functions will use the position saved
  // in bit reader for parsing, so we don't have to remember it here.
  stream_ += base::checked_cast<size_t>(nalu_size_with_start_code);
  bytes_left_ -= nalu_size_with_start_code;

  // Read NALU header, skip the forbidden_zero_bit, but check for it.
  int data;
  READ_BITS_OR_RETURN(1, &data);
  TRUE_OR_RETURN(data == 0);

  READ_BITS_OR_RETURN(2, &nalu->nal_ref_idc);
  READ_BITS_OR_RETURN(5, &nalu->nal_unit_type);

  DVLOG(4) << "NALU type: " << static_cast<int>(nalu->nal_unit_type)
           << " at: " << reinterpret_cast<const void*>(nalu->data.get())
           << " size: " << nalu->size
           << " ref: " << static_cast<int>(nalu->nal_ref_idc);

  previous_nalu_range_.clear();
  previous_nalu_range_.Add(nalu->data.get(),
                           nalu->data + base::checked_cast<size_t>(nalu->size));
  return kOk;
}

// Default scaling lists (per spec).
static const uint8_t kDefault4x4Intra[kH264ScalingList4x4Length] = {
    6, 13, 13, 20, 20, 20, 28, 28, 28, 28, 32, 32, 32, 37, 37, 42,
};

static const uint8_t kDefault4x4Inter[kH264ScalingList4x4Length] = {
    10, 14, 14, 20, 20, 20, 24, 24, 24, 24, 27, 27, 27, 30, 30, 34,
};

static const uint8_t kDefault8x8Intra[kH264ScalingList8x8Length] = {
    6,  10, 10, 13, 11, 13, 16, 16, 16, 16, 18, 18, 18, 18, 18, 23,
    23, 23, 23, 23, 23, 25, 25, 25, 25, 25, 25, 25, 27, 27, 27, 27,
    27, 27, 27, 27, 29, 29, 29, 29, 29, 29, 29, 31, 31, 31, 31, 31,
    31, 33, 33, 33, 33, 33, 36, 36, 36, 36, 38, 38, 38, 40, 40, 42,
};

static const uint8_t kDefault8x8Inter[kH264ScalingList8x8Length] = {
    9,  13, 13, 15, 13, 15, 17, 17, 17, 17, 19, 19, 19, 19, 19, 21,
    21, 21, 21, 21, 21, 22, 22, 22, 22, 22, 22, 22, 24, 24, 24, 24,
    24, 24, 24, 24, 25, 25, 25, 25, 25, 25, 25, 27, 27, 27, 27, 27,
    27, 28, 28, 28, 28, 28, 30, 30, 30, 30, 32, 32, 32, 33, 33, 35,
};

static inline void DefaultScalingList4x4(
    int i,
    uint8_t scaling_list4x4[][kH264ScalingList4x4Length]) {
  DCHECK_LT(i, 6);

  if (i < 3)
    memcpy(scaling_list4x4[i], kDefault4x4Intra, sizeof(kDefault4x4Intra));
  else if (i < 6)
    memcpy(scaling_list4x4[i], kDefault4x4Inter, sizeof(kDefault4x4Inter));
}

static inline void DefaultScalingList8x8(
    int i,
    uint8_t scaling_list8x8[][kH264ScalingList8x8Length]) {
  DCHECK_LT(i, 6);

  if (i % 2 == 0)
    memcpy(scaling_list8x8[i], kDefault8x8Intra, sizeof(kDefault8x8Intra));
  else
    memcpy(scaling_list8x8[i], kDefault8x8Inter, sizeof(kDefault8x8Inter));
}

static void FallbackScalingList4x4(
    int i,
    const uint8_t default_scaling_list_intra[],
    const uint8_t default_scaling_list_inter[],
    uint8_t scaling_list4x4[][kH264ScalingList4x4Length]) {
  static const int kScalingList4x4ByteSize =
      sizeof(scaling_list4x4[0][0]) * kH264ScalingList4x4Length;

  switch (i) {
    case 0:
      memcpy(scaling_list4x4[i], default_scaling_list_intra,
             kScalingList4x4ByteSize);
      break;

    case 1:
      memcpy(scaling_list4x4[i], scaling_list4x4[0], kScalingList4x4ByteSize);
      break;

    case 2:
      memcpy(scaling_list4x4[i], scaling_list4x4[1], kScalingList4x4ByteSize);
      break;

    case 3:
      memcpy(scaling_list4x4[i], default_scaling_list_inter,
             kScalingList4x4ByteSize);
      break;

    case 4:
      memcpy(scaling_list4x4[i], scaling_list4x4[3], kScalingList4x4ByteSize);
      break;

    case 5:
      memcpy(scaling_list4x4[i], scaling_list4x4[4], kScalingList4x4ByteSize);
      break;

    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

static void FallbackScalingList8x8(
    int i,
    const uint8_t default_scaling_list_intra[],
    const uint8_t default_scaling_list_inter[],
    uint8_t scaling_list8x8[][kH264ScalingList8x8Length]) {
  static const int kScalingList8x8ByteSize =
      sizeof(scaling_list8x8[0][0]) * kH264ScalingList8x8Length;

  switch (i) {
    case 0:
      memcpy(scaling_list8x8[i], default_scaling_list_intra,
             kScalingList8x8ByteSize);
      break;

    case 1:
      memcpy(scaling_list8x8[i], default_scaling_list_inter,
             kScalingList8x8ByteSize);
      break;

    case 2:
      memcpy(scaling_list8x8[i], scaling_list8x8[0], kScalingList8x8ByteSize);
      break;

    case 3:
      memcpy(scaling_list8x8[i], scaling_list8x8[1], kScalingList8x8ByteSize);
      break;

    case 4:
      memcpy(scaling_list8x8[i], scaling_list8x8[2], kScalingList8x8ByteSize);
      break;

    case 5:
      memcpy(scaling_list8x8[i], scaling_list8x8[3], kScalingList8x8ByteSize);
      break;

    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

H264Parser::Result H264Parser::ParseScalingList(int size,
                                                uint8_t* scaling_list,
                                                bool* use_default) {
  // See chapter 7.3.2.1.1.1.
  int last_scale = 8;
  int next_scale = 8;
  int delta_scale;

  *use_default = false;

  for (int j = 0; j < size; ++j) {
    if (next_scale != 0) {
      READ_SE_OR_RETURN(&delta_scale);
      IN_RANGE_OR_RETURN(delta_scale, -128, 127);
      next_scale = (last_scale + delta_scale + 256) & 0xff;

      if (j == 0 && next_scale == 0) {
        *use_default = true;
        return kOk;
      }
    }

    scaling_list[j] = (next_scale == 0) ? last_scale : next_scale;
    last_scale = scaling_list[j];
  }

  return kOk;
}

H264Parser::Result H264Parser::ParseSPSScalingLists(H264SPS* sps) {
  // See 7.4.2.1.1.
  bool seq_scaling_list_present_flag;
  bool use_default;
  Result res;

  // Parse scaling_list4x4.
  for (int i = 0; i < 6; ++i) {
    READ_BOOL_OR_RETURN(&seq_scaling_list_present_flag);

    if (seq_scaling_list_present_flag) {
      res = ParseScalingList(std::size(sps->scaling_list4x4[i]),
                             sps->scaling_list4x4[i], &use_default);
      if (res != kOk)
        return res;

      if (use_default)
        DefaultScalingList4x4(i, sps->scaling_list4x4);

    } else {
      FallbackScalingList4x4(i, kDefault4x4Intra, kDefault4x4Inter,
                             sps->scaling_list4x4);
    }
  }

  // Parse scaling_list8x8.
  for (int i = 0; i < ((sps->chroma_format_idc != 3) ? 2 : 6); ++i) {
    READ_BOOL_OR_RETURN(&seq_scaling_list_present_flag);

    if (seq_scaling_list_present_flag) {
      res = ParseScalingList(std::size(sps->scaling_list8x8[i]),
                             sps->scaling_list8x8[i], &use_default);
      if (res != kOk)
        return res;

      if (use_default)
        DefaultScalingList8x8(i, sps->scaling_list8x8);

    } else {
      FallbackScalingList8x8(i, kDefault8x8Intra, kDefault8x8Inter,
                             sps->scaling_list8x8);
    }
  }

  return kOk;
}

H264Parser::Result H264Parser::ParsePPSScalingLists(const H264SPS& sps,
                                                    H264PPS* pps) {
  // See 7.4.2.2.
  bool pic_scaling_list_present_flag;
  bool use_default;
  Result res;

  for (int i = 0; i < 6; ++i) {
    READ_BOOL_OR_RETURN(&pic_scaling_list_present_flag);

    if (pic_scaling_list_present_flag) {
      res = ParseScalingList(std::size(pps->scaling_list4x4[i]),
                             pps->scaling_list4x4[i], &use_default);
      if (res != kOk)
        return res;

      if (use_default)
        DefaultScalingList4x4(i, pps->scaling_list4x4);

    } else {
      if (!sps.seq_scaling_matrix_present_flag) {
        // Table 7-2 fallback rule A in spec.
        FallbackScalingList4x4(i, kDefault4x4Intra, kDefault4x4Inter,
                               pps->scaling_list4x4);
      } else {
        // Table 7-2 fallback rule B in spec.
        FallbackScalingList4x4(i, sps.scaling_list4x4[0],
                               sps.scaling_list4x4[3], pps->scaling_list4x4);
      }
    }
  }

  if (pps->transform_8x8_mode_flag) {
    for (int i = 0; i < ((sps.chroma_format_idc != 3) ? 2 : 6); ++i) {
      READ_BOOL_OR_RETURN(&pic_scaling_list_present_flag);

      if (pic_scaling_list_present_flag) {
        res = ParseScalingList(std::size(pps->scaling_list8x8[i]),
                               pps->scaling_list8x8[i], &use_default);
        if (res != kOk)
          return res;

        if (use_default)
          DefaultScalingList8x8(i, pps->scaling_list8x8);

      } else {
        if (!sps.seq_scaling_matrix_present_flag) {
          // Table 7-2 fallback rule A in spec.
          FallbackScalingList8x8(i, kDefault8x8Intra, kDefault8x8Inter,
                                 pps->scaling_list8x8);
        } else {
          // Table 7-2 fallback rule B in spec.
          FallbackScalingList8x8(i, sps.scaling_list8x8[0],
                                 sps.scaling_list8x8[1], pps->scaling_list8x8);
        }
      }
    }
  }
  return kOk;
}

H264Parser::Result H264Parser::ParseAndIgnoreHRDParameters(
    bool* hrd_parameters_present) {
  int data;
  READ_BOOL_OR_RETURN(&data);  // {nal,vcl}_hrd_parameters_present_flag
  if (!data)
    return kOk;

  *hrd_parameters_present = true;

  int cpb_cnt_minus1;
  READ_UE_OR_RETURN(&cpb_cnt_minus1);
  IN_RANGE_OR_RETURN(cpb_cnt_minus1, 0, 31);
  READ_BITS_OR_RETURN(8, &data);  // bit_rate_scale, cpb_size_scale
  for (int i = 0; i <= cpb_cnt_minus1; ++i) {
    READ_UE_OR_RETURN(&data);    // bit_rate_value_minus1[i]
    READ_UE_OR_RETURN(&data);    // cpb_size_value_minus1[i]
    READ_BOOL_OR_RETURN(&data);  // cbr_flag
  }
  READ_BITS_OR_RETURN(20, &data);  // cpb/dpb delays, etc.

  return kOk;
}

H264Parser::Result H264Parser::ParseVUIParameters(H264SPS* sps) {
  bool aspect_ratio_info_present_flag;
  READ_BOOL_OR_RETURN(&aspect_ratio_info_present_flag);
  if (aspect_ratio_info_present_flag) {
    int aspect_ratio_idc;
    READ_BITS_OR_RETURN(8, &aspect_ratio_idc);
    if (aspect_ratio_idc == H264SPS::kExtendedSar) {
      READ_BITS_OR_RETURN(16, &sps->sar_width);
      READ_BITS_OR_RETURN(16, &sps->sar_height);
    } else {
      const int max_aspect_ratio_idc = std::size(kTableSarWidth) - 1;
      IN_RANGE_OR_RETURN(aspect_ratio_idc, 0, max_aspect_ratio_idc);
      sps->sar_width = kTableSarWidth[aspect_ratio_idc];
      sps->sar_height = kTableSarHeight[aspect_ratio_idc];
    }
  }

  int data;
  // Read and ignore overscan and video signal type info.
  READ_BOOL_OR_RETURN(&data);  // overscan_info_present_flag
  if (data)
    READ_BOOL_OR_RETURN(&data);  // overscan_appropriate_flag

  READ_BOOL_OR_RETURN(&sps->video_signal_type_present_flag);
  if (sps->video_signal_type_present_flag) {
    READ_BITS_OR_RETURN(3, &sps->video_format);
    READ_BOOL_OR_RETURN(&sps->video_full_range_flag);
    READ_BOOL_OR_RETURN(&sps->colour_description_present_flag);
    if (sps->colour_description_present_flag) {
      // color description syntax elements
      READ_BITS_OR_RETURN(8, &sps->colour_primaries);
      READ_BITS_OR_RETURN(8, &sps->transfer_characteristics);
      READ_BITS_OR_RETURN(8, &sps->matrix_coefficients);
    }
  }

  READ_BOOL_OR_RETURN(&data);  // chroma_loc_info_present_flag
  if (data) {
    READ_UE_OR_RETURN(&data);  // chroma_sample_loc_type_top_field
    READ_UE_OR_RETURN(&data);  // chroma_sample_loc_type_bottom_field
  }

  // Read and ignore timing info.
  READ_BOOL_OR_RETURN(&data);  // timing_info_present_flag
  if (data) {
    READ_BITS_OR_RETURN(16, &data);  // num_units_in_tick
    READ_BITS_OR_RETURN(16, &data);  // num_units_in_tick
    READ_BITS_OR_RETURN(16, &data);  // time_scale
    READ_BITS_OR_RETURN(16, &data);  // time_scale
    READ_BOOL_OR_RETURN(&data);      // fixed_frame_rate_flag
  }

  // Read and ignore NAL HRD parameters, if present.
  bool hrd_parameters_present = false;
  Result res = ParseAndIgnoreHRDParameters(&hrd_parameters_present);
  if (res != kOk)
    return res;

  // Read and ignore VCL HRD parameters, if present.
  res = ParseAndIgnoreHRDParameters(&hrd_parameters_present);
  if (res != kOk)
    return res;

  if (hrd_parameters_present)    // One of NAL or VCL params present is enough.
    READ_BOOL_OR_RETURN(&data);  // low_delay_hrd_flag

  READ_BOOL_OR_RETURN(&data);  // pic_struct_present_flag
  READ_BOOL_OR_RETURN(&sps->bitstream_restriction_flag);
  if (sps->bitstream_restriction_flag) {
    READ_BOOL_OR_RETURN(&data);  // motion_vectors_over_pic_boundaries_flag
    READ_UE_OR_RETURN(&data);    // max_bytes_per_pic_denom
    READ_UE_OR_RETURN(&data);    // max_bits_per_mb_denom
    READ_UE_OR_RETURN(&data);    // log2_max_mv_length_horizontal
    READ_UE_OR_RETURN(&data);    // log2_max_mv_length_vertical
    READ_UE_OR_RETURN(&sps->max_num_reorder_frames);
    READ_UE_OR_RETURN(&sps->max_dec_frame_buffering);
    TRUE_OR_RETURN(sps->max_dec_frame_buffering >= sps->max_num_ref_frames);
    IN_RANGE_OR_RETURN(sps->max_num_reorder_frames, 0,
                       sps->max_dec_frame_buffering);
  }

  return kOk;
}

static void FillDefaultSeqScalingLists(H264SPS* sps) {
  static_assert(sizeof(sps->scaling_list4x4[0][0]) == sizeof(uint8_t));
  memset(sps->scaling_list4x4, 16, sizeof(sps->scaling_list4x4));
  static_assert(sizeof(sps->scaling_list8x8[0][0]) == sizeof(uint8_t));
  memset(sps->scaling_list8x8, 16, sizeof(sps->scaling_list8x8));
}

H264Parser::Result H264Parser::ParseSPS(int* sps_id) {
  // See 7.4.2.1.
  int data;
  Result res;

  *sps_id = -1;

  std::unique_ptr<H264SPS> sps(new H264SPS());

  READ_BITS_OR_RETURN(8, &sps->profile_idc);
  READ_BOOL_OR_RETURN(&sps->constraint_set0_flag);
  READ_BOOL_OR_RETURN(&sps->constraint_set1_flag);
  READ_BOOL_OR_RETURN(&sps->constraint_set2_flag);
  READ_BOOL_OR_RETURN(&sps->constraint_set3_flag);
  READ_BOOL_OR_RETURN(&sps->constraint_set4_flag);
  READ_BOOL_OR_RETURN(&sps->constraint_set5_flag);
  READ_BITS_OR_RETURN(2, &data);  // reserved_zero_2bits
  READ_BITS_OR_RETURN(8, &sps->level_idc);
  READ_UE_OR_RETURN(&sps->seq_parameter_set_id);
  TRUE_OR_RETURN(sps->seq_parameter_set_id < 32);

  if (sps->profile_idc == 100 || sps->profile_idc == 110 ||
      sps->profile_idc == 122 || sps->profile_idc == 244 ||
      sps->profile_idc == 44 || sps->profile_idc == 83 ||
      sps->profile_idc == 86 || sps->profile_idc == 118 ||
      sps->profile_idc == 128) {
    READ_UE_OR_RETURN(&sps->chroma_format_idc);
    TRUE_OR_RETURN(sps->chroma_format_idc < 4);

    if (sps->chroma_format_idc == 3)
      READ_BOOL_OR_RETURN(&sps->separate_colour_plane_flag);

    READ_UE_OR_RETURN(&sps->bit_depth_luma_minus8);
    TRUE_OR_RETURN(sps->bit_depth_luma_minus8 < 7);

    READ_UE_OR_RETURN(&sps->bit_depth_chroma_minus8);
    TRUE_OR_RETURN(sps->bit_depth_chroma_minus8 < 7);

    READ_BOOL_OR_RETURN(&sps->qpprime_y_zero_transform_bypass_flag);
    READ_BOOL_OR_RETURN(&sps->seq_scaling_matrix_present_flag);

    if (sps->seq_scaling_matrix_present_flag) {
      DVLOG(4) << "Scaling matrix present";
      res = ParseSPSScalingLists(sps.get());
      if (res != kOk)
        return res;
    } else {
      FillDefaultSeqScalingLists(sps.get());
    }
  } else {
    sps->chroma_format_idc = 1;
    FillDefaultSeqScalingLists(sps.get());
  }

  if (sps->separate_colour_plane_flag)
    sps->chroma_array_type = 0;
  else
    sps->chroma_array_type = sps->chroma_format_idc;

  READ_UE_OR_RETURN(&sps->log2_max_frame_num_minus4);
  TRUE_OR_RETURN(sps->log2_max_frame_num_minus4 < 13);

  READ_UE_OR_RETURN(&sps->pic_order_cnt_type);
  TRUE_OR_RETURN(sps->pic_order_cnt_type < 3);

  if (sps->pic_order_cnt_type == 0) {
    READ_UE_OR_RETURN(&sps->log2_max_pic_order_cnt_lsb_minus4);
    TRUE_OR_RETURN(sps->log2_max_pic_order_cnt_lsb_minus4 < 13);
    sps->expected_delta_per_pic_order_cnt_cycle = 0;
  } else if (sps->pic_order_cnt_type == 1) {
    READ_BOOL_OR_RETURN(&sps->delta_pic_order_always_zero_flag);
    READ_SE_OR_RETURN(&sps->offset_for_non_ref_pic);
    READ_SE_OR_RETURN(&sps->offset_for_top_to_bottom_field);
    READ_UE_OR_RETURN(&sps->num_ref_frames_in_pic_order_cnt_cycle);
    TRUE_OR_RETURN(sps->num_ref_frames_in_pic_order_cnt_cycle < 255);

    base::CheckedNumeric<int> offset_acc = 0;
    for (int i = 0; i < sps->num_ref_frames_in_pic_order_cnt_cycle; ++i) {
      READ_SE_OR_RETURN(&sps->offset_for_ref_frame[i]);
      offset_acc += sps->offset_for_ref_frame[i];
    }
    if (!offset_acc.IsValid())
      return kInvalidStream;
    sps->expected_delta_per_pic_order_cnt_cycle = offset_acc.ValueOrDefault(0);
  }

  READ_UE_OR_RETURN(&sps->max_num_ref_frames);
  READ_BOOL_OR_RETURN(&sps->gaps_in_frame_num_value_allowed_flag);

  READ_UE_OR_RETURN(&sps->pic_width_in_mbs_minus1);
  READ_UE_OR_RETURN(&sps->pic_height_in_map_units_minus1);

  READ_BOOL_OR_RETURN(&sps->frame_mbs_only_flag);
  if (!sps->frame_mbs_only_flag)
    READ_BOOL_OR_RETURN(&sps->mb_adaptive_frame_field_flag);

  READ_BOOL_OR_RETURN(&sps->direct_8x8_inference_flag);

  READ_BOOL_OR_RETURN(&sps->frame_cropping_flag);
  if (sps->frame_cropping_flag) {
    READ_UE_OR_RETURN(&sps->frame_crop_left_offset);
    READ_UE_OR_RETURN(&sps->frame_crop_right_offset);
    READ_UE_OR_RETURN(&sps->frame_crop_top_offset);
    READ_UE_OR_RETURN(&sps->frame_crop_bottom_offset);
  }

  READ_BOOL_OR_RETURN(&sps->vui_parameters_present_flag);
  if (sps->vui_parameters_present_flag) {
    DVLOG(4) << "VUI parameters present";
    res = ParseVUIParameters(sps.get());
    if (res != kOk)
      return res;
  }

  // If an SPS with the same id already exists, replace it.
  *sps_id = sps->seq_parameter_set_id;
  active_SPSes_[*sps_id] = std::move(sps);

  return kOk;
}

H264Parser::Result H264Parser::ParsePPS(int* pps_id) {
  // See 7.4.2.2.
  const H264SPS* sps;
  Result res;

  *pps_id = -1;

  std::unique_ptr<H264PPS> pps(new H264PPS());

  READ_UE_OR_RETURN(&pps->pic_parameter_set_id);
  READ_UE_OR_RETURN(&pps->seq_parameter_set_id);
  TRUE_OR_RETURN(pps->seq_parameter_set_id < 32);

  if (active_SPSes_.find(pps->seq_parameter_set_id) == active_SPSes_.end()) {
    DVLOG(1) << "Invalid stream, no SPS id: " << pps->seq_parameter_set_id;
    return kInvalidStream;
  }

  sps = GetSPS(pps->seq_parameter_set_id);
  TRUE_OR_RETURN(sps);

  READ_BOOL_OR_RETURN(&pps->entropy_coding_mode_flag);
  READ_BOOL_OR_RETURN(&pps->bottom_field_pic_order_in_frame_present_flag);

  READ_UE_OR_RETURN(&pps->num_slice_groups_minus1);
  if (pps->num_slice_groups_minus1 > 1) {
    DVLOG(1) << "Slice groups not supported";
    return kUnsupportedStream;
  }

  READ_UE_OR_RETURN(&pps->num_ref_idx_l0_default_active_minus1);
  TRUE_OR_RETURN(pps->num_ref_idx_l0_default_active_minus1 < 32);

  READ_UE_OR_RETURN(&pps->num_ref_idx_l1_default_active_minus1);
  TRUE_OR_RETURN(pps->num_ref_idx_l1_default_active_minus1 < 32);

  READ_BOOL_OR_RETURN(&pps->weighted_pred_flag);
  READ_BITS_OR_RETURN(2, &pps->weighted_bipred_idc);
  TRUE_OR_RETURN(pps->weighted_bipred_idc < 3);

  READ_SE_OR_RETURN(&pps->pic_init_qp_minus26);
  IN_RANGE_OR_RETURN(pps->pic_init_qp_minus26, -26, 25);

  READ_SE_OR_RETURN(&pps->pic_init_qs_minus26);
  IN_RANGE_OR_RETURN(pps->pic_init_qs_minus26, -26, 25);

  READ_SE_OR_RETURN(&pps->chroma_qp_index_offset);
  IN_RANGE_OR_RETURN(pps->chroma_qp_index_offset, -12, 12);
  pps->second_chroma_qp_index_offset = pps->chroma_qp_index_offset;

  READ_BOOL_OR_RETURN(&pps->deblocking_filter_control_present_flag);
  READ_BOOL_OR_RETURN(&pps->constrained_intra_pred_flag);
  READ_BOOL_OR_RETURN(&pps->redundant_pic_cnt_present_flag);

  bool pps_remainder_unencrypted = true;
  if (encrypted_ranges_.size()) {
    Ranges<const uint8_t*> pps_range;
    // Only check that the next byte is unencrypted, not the rest of the NALU.
    const uint8_t* next_byte =
        previous_nalu_range_.end(0) - br_.NumBitsLeft() / 8;
    pps_range.Add(next_byte, next_byte + 1);
    pps_remainder_unencrypted =
        (encrypted_ranges_.IntersectionWith(pps_range).size() == 0);
  }
  if (pps_remainder_unencrypted && br_.HasMoreRBSPData()) {
    if (sps->profile_idc == H264SPS::kProfileIDCBaseline ||
        sps->profile_idc == H264SPS::kProfileIDCConstrainedBaseline ||
        sps->profile_idc == H264SPS::kProfileIDCMain) {
      DVLOG(1) << "Invalid stream, ignored unexpected RBSB data in PPS frame";
    } else {
      READ_BOOL_OR_RETURN(&pps->transform_8x8_mode_flag);
      READ_BOOL_OR_RETURN(&pps->pic_scaling_matrix_present_flag);

      if (pps->pic_scaling_matrix_present_flag) {
        DVLOG(4) << "Picture scaling matrix present";
        res = ParsePPSScalingLists(*sps, pps.get());
        if (res != kOk)
          return res;
      }

      READ_SE_OR_RETURN(&pps->second_chroma_qp_index_offset);
    }
  }

  // If a PPS with the same id already exists, replace it.
  *pps_id = pps->pic_parameter_set_id;
  active_PPSes_[*pps_id] = std::move(pps);

  return kOk;
}

H264Parser::Result H264Parser::ParseSPSExt(int* sps_id) {
  // See 7.4.2.1.
  int local_sps_id = -1;

  *sps_id = -1;

  READ_UE_OR_RETURN(&local_sps_id);
  TRUE_OR_RETURN(local_sps_id < 32);

  *sps_id = local_sps_id;
  return kOk;
}

H264Parser::Result H264Parser::ParseRefPicListModification(
    int num_ref_idx_active_minus1,
    H264ModificationOfPicNum* ref_list_mods) {
  H264ModificationOfPicNum* pic_num_mod;

  if (num_ref_idx_active_minus1 >= 32)
    return kInvalidStream;

  for (int i = 0; i < 32; ++i) {
    pic_num_mod = &ref_list_mods[i];
    READ_UE_OR_RETURN(&pic_num_mod->modification_of_pic_nums_idc);
    TRUE_OR_RETURN(pic_num_mod->modification_of_pic_nums_idc < 4);

    switch (pic_num_mod->modification_of_pic_nums_idc) {
      case 0:
      case 1:
        READ_UE_OR_RETURN(&pic_num_mod->abs_diff_pic_num_minus1);
        break;

      case 2:
        READ_UE_OR_RETURN(&pic_num_mod->long_term_pic_num);
        break;

      case 3:
        // Per spec, list cannot be empty.
        if (i == 0)
          return kInvalidStream;
        return kOk;

      default:
        return kInvalidStream;
    }
  }

  // If we got here, we didn't get loop end marker prematurely,
  // so make sure it is there for our client.
  int modification_of_pic_nums_idc;
  READ_UE_OR_RETURN(&modification_of_pic_nums_idc);
  TRUE_OR_RETURN(modification_of_pic_nums_idc == 3);

  return kOk;
}

H264Parser::Result H264Parser::ParseRefPicListModifications(
    H264SliceHeader* shdr) {
  Result res;

  if (!shdr->IsISlice() && !shdr->IsSISlice()) {
    READ_BOOL_OR_RETURN(&shdr->ref_pic_list_modification_flag_l0);
    if (shdr->ref_pic_list_modification_flag_l0) {
      res = ParseRefPicListModification(shdr->num_ref_idx_l0_active_minus1,
                                        shdr->ref_list_l0_modifications);
      if (res != kOk)
        return res;
    }
  }

  if (shdr->IsBSlice()) {
    READ_BOOL_OR_RETURN(&shdr->ref_pic_list_modification_flag_l1);
    if (shdr->ref_pic_list_modification_flag_l1) {
      res = ParseRefPicListModification(shdr->num_ref_idx_l1_active_minus1,
                                        shdr->ref_list_l1_modifications);
      if (res != kOk)
        return res;
    }
  }

  return kOk;
}

H264Parser::Result H264Parser::ParseWeightingFactors(
    int num_ref_idx_active_minus1,
    int chroma_array_type,
    int luma_log2_weight_denom,
    int chroma_log2_weight_denom,
    H264WeightingFactors* w_facts) {
  int def_luma_weight = 1 << luma_log2_weight_denom;
  int def_chroma_weight = 1 << chroma_log2_weight_denom;

  for (int i = 0; i < num_ref_idx_active_minus1 + 1; ++i) {
    READ_BOOL_OR_RETURN(&w_facts->luma_weight_flag);
    if (w_facts->luma_weight_flag) {
      READ_SE_OR_RETURN(&w_facts->luma_weight[i]);
      IN_RANGE_OR_RETURN(w_facts->luma_weight[i], -128, 127);

      READ_SE_OR_RETURN(&w_facts->luma_offset[i]);
      IN_RANGE_OR_RETURN(w_facts->luma_offset[i], -128, 127);
    } else {
      w_facts->luma_weight[i] = def_luma_weight;
      w_facts->luma_offset[i] = 0;
    }

    if (chroma_array_type != 0) {
      READ_BOOL_OR_RETURN(&w_facts->chroma_weight_flag);
      if (w_facts->chroma_weight_flag) {
        for (int j = 0; j < 2; ++j) {
          READ_SE_OR_RETURN(&w_facts->chroma_weight[i][j]);
          IN_RANGE_OR_RETURN(w_facts->chroma_weight[i][j], -128, 127);

          READ_SE_OR_RETURN(&w_facts->chroma_offset[i][j]);
          IN_RANGE_OR_RETURN(w_facts->chroma_offset[i][j], -128, 127);
        }
      } else {
        for (int j = 0; j < 2; ++j) {
          w_facts->chroma_weight[i][j] = def_chroma_weight;
          w_facts->chroma_offset[i][j] = 0;
        }
      }
    }
  }

  return kOk;
}

H264Parser::Result H264Parser::ParsePredWeightTable(const H264SPS& sps,
                                                    H264SliceHeader* shdr) {
  READ_UE_OR_RETURN(&shdr->luma_log2_weight_denom);
  TRUE_OR_RETURN(shdr->luma_log2_weight_denom < 8);

  if (sps.chroma_array_type != 0)
    READ_UE_OR_RETURN(&shdr->chroma_log2_weight_denom);
  TRUE_OR_RETURN(shdr->chroma_log2_weight_denom < 8);

  Result res = ParseWeightingFactors(
      shdr->num_ref_idx_l0_active_minus1, sps.chroma_array_type,
      shdr->luma_log2_weight_denom, shdr->chroma_log2_weight_denom,
      &shdr->pred_weight_table_l0);
  if (res != kOk)
    return res;

  if (shdr->IsBSlice()) {
    res = ParseWeightingFactors(
        shdr->num_ref_idx_l1_active_minus1, sps.chroma_array_type,
        shdr->luma_log2_weight_denom, shdr->chroma_log2_weight_denom,
        &shdr->pred_weight_table_l1);
    if (res != kOk)
      return res;
  }

  return kOk;
}

H264Parser::Result H264Parser::ParseDecRefPicMarking(H264SliceHeader* shdr) {
  size_t bits_left_at_start = br_.NumBitsLeft();

  if (shdr->idr_pic_flag) {
    READ_BOOL_OR_RETURN(&shdr->no_output_of_prior_pics_flag);
    READ_BOOL_OR_RETURN(&shdr->long_term_reference_flag);
  } else {
    READ_BOOL_OR_RETURN(&shdr->adaptive_ref_pic_marking_mode_flag);

    H264DecRefPicMarking* marking;
    if (shdr->adaptive_ref_pic_marking_mode_flag) {
      size_t i;
      for (i = 0; i < std::size(shdr->ref_pic_marking); ++i) {
        marking = &shdr->ref_pic_marking[i];

        READ_UE_OR_RETURN(&marking->memory_mgmnt_control_operation);
        if (marking->memory_mgmnt_control_operation == 0)
          break;

        if (marking->memory_mgmnt_control_operation == 1 ||
            marking->memory_mgmnt_control_operation == 3)
          READ_UE_OR_RETURN(&marking->difference_of_pic_nums_minus1);

        if (marking->memory_mgmnt_control_operation == 2)
          READ_UE_OR_RETURN(&marking->long_term_pic_num);

        if (marking->memory_mgmnt_control_operation == 3 ||
            marking->memory_mgmnt_control_operation == 6)
          READ_UE_OR_RETURN(&marking->long_term_frame_idx);

        if (marking->memory_mgmnt_control_operation == 4)
          READ_UE_OR_RETURN(&marking->max_long_term_frame_idx_plus1);

        if (marking->memory_mgmnt_control_operation > 6)
          return kInvalidStream;
      }

      if (i == std::size(shdr->ref_pic_marking)) {
        DVLOG(1) << "Ran out of dec ref pic marking fields";
        return kUnsupportedStream;
      }
    }
  }

  shdr->dec_ref_pic_marking_bit_size = bits_left_at_start - br_.NumBitsLeft();
  return kOk;
}

H264Parser::Result H264Parser::ParseSliceHeader(const H264NALU& nalu,
                                                H264SliceHeader* shdr) {
  // See 7.4.3.
  const H264SPS* sps;
  const H264PPS* pps;
  Result res;

  *shdr = H264SliceHeader();

  shdr->idr_pic_flag = (nalu.nal_unit_type == 5);
  shdr->nal_ref_idc = nalu.nal_ref_idc;
  shdr->nalu_data = nalu.data.get();
  shdr->nalu_size = nalu.size;

  READ_UE_OR_RETURN(&shdr->first_mb_in_slice);
  READ_UE_OR_RETURN(&shdr->slice_type);
  TRUE_OR_RETURN(shdr->slice_type < 10);

  READ_UE_OR_RETURN(&shdr->pic_parameter_set_id);

  pps = GetPPS(shdr->pic_parameter_set_id);
  TRUE_OR_RETURN(pps);

  sps = GetSPS(pps->seq_parameter_set_id);
  TRUE_OR_RETURN(sps);

  if (sps->separate_colour_plane_flag) {
    DVLOG(1) << "Interlaced streams not supported";
    return kUnsupportedStream;
  }

  READ_BITS_OR_RETURN(sps->log2_max_frame_num_minus4 + 4, &shdr->frame_num);
  if (!sps->frame_mbs_only_flag) {
    READ_BOOL_OR_RETURN(&shdr->field_pic_flag);
    if (shdr->field_pic_flag) {
      DVLOG(1) << "Interlaced streams not supported";
      return kUnsupportedStream;
    }
  }

  if (shdr->idr_pic_flag)
    READ_UE_OR_RETURN(&shdr->idr_pic_id);

  size_t bits_left_at_pic_order_cnt_start = br_.NumBitsLeft();
  if (sps->pic_order_cnt_type == 0) {
    READ_BITS_OR_RETURN(sps->log2_max_pic_order_cnt_lsb_minus4 + 4,
                        &shdr->pic_order_cnt_lsb);
    if (pps->bottom_field_pic_order_in_frame_present_flag &&
        !shdr->field_pic_flag)
      READ_SE_OR_RETURN(&shdr->delta_pic_order_cnt_bottom);
  }

  if (sps->pic_order_cnt_type == 1 && !sps->delta_pic_order_always_zero_flag) {
    READ_SE_OR_RETURN(&shdr->delta_pic_order_cnt0);
    if (pps->bottom_field_pic_order_in_frame_present_flag &&
        !shdr->field_pic_flag)
      READ_SE_OR_RETURN(&shdr->delta_pic_order_cnt1);
  }

  shdr->pic_order_cnt_bit_size =
      bits_left_at_pic_order_cnt_start - br_.NumBitsLeft();

  if (pps->redundant_pic_cnt_present_flag) {
    READ_UE_OR_RETURN(&shdr->redundant_pic_cnt);
    TRUE_OR_RETURN(shdr->redundant_pic_cnt < 128);
  }

  if (shdr->IsBSlice())
    READ_BOOL_OR_RETURN(&shdr->direct_spatial_mv_pred_flag);

  if (shdr->IsPSlice() || shdr->IsSPSlice() || shdr->IsBSlice()) {
    READ_BOOL_OR_RETURN(&shdr->num_ref_idx_active_override_flag);
    if (shdr->num_ref_idx_active_override_flag) {
      READ_UE_OR_RETURN(&shdr->num_ref_idx_l0_active_minus1);
      if (shdr->IsBSlice())
        READ_UE_OR_RETURN(&shdr->num_ref_idx_l1_active_minus1);
    } else {
      shdr->num_ref_idx_l0_active_minus1 =
          pps->num_ref_idx_l0_default_active_minus1;
      if (shdr->IsBSlice()) {
        shdr->num_ref_idx_l1_active_minus1 =
            pps->num_ref_idx_l1_default_active_minus1;
      }
    }
  }
  if (shdr->field_pic_flag) {
    TRUE_OR_RETURN(shdr->num_ref_idx_l0_active_minus1 < 32);
    TRUE_OR_RETURN(shdr->num_ref_idx_l1_active_minus1 < 32);
  } else {
    TRUE_OR_RETURN(shdr->num_ref_idx_l0_active_minus1 < 16);
    TRUE_OR_RETURN(shdr->num_ref_idx_l1_active_minus1 < 16);
  }

  if (nalu.nal_unit_type == H264NALU::kCodedSliceExtension) {
    return kUnsupportedStream;
  } else {
    res = ParseRefPicListModifications(shdr);
    if (res != kOk)
      return res;
  }

  if ((pps->weighted_pred_flag && (shdr->IsPSlice() || shdr->IsSPSlice())) ||
      (pps->weighted_bipred_idc == 1 && shdr->IsBSlice())) {
    res = ParsePredWeightTable(*sps, shdr);
    if (res != kOk)
      return res;
  }

  if (nalu.nal_ref_idc != 0) {
    res = ParseDecRefPicMarking(shdr);
    if (res != kOk)
      return res;
  }

  if (pps->entropy_coding_mode_flag && !shdr->IsISlice() &&
      !shdr->IsSISlice()) {
    READ_UE_OR_RETURN(&shdr->cabac_init_idc);
    TRUE_OR_RETURN(shdr->cabac_init_idc < 3);
  }

  READ_SE_OR_RETURN(&shdr->slice_qp_delta);

  if (shdr->IsSPSlice() || shdr->IsSISlice()) {
    if (shdr->IsSPSlice())
      READ_BOOL_OR_RETURN(&shdr->sp_for_switch_flag);
    READ_SE_OR_RETURN(&shdr->slice_qs_delta);
  }

  if (pps->deblocking_filter_control_present_flag) {
    READ_UE_OR_RETURN(&shdr->disable_deblocking_filter_idc);
    TRUE_OR_RETURN(shdr->disable_deblocking_filter_idc < 3);

    if (shdr->disable_deblocking_filter_idc != 1) {
      READ_SE_OR_RETURN(&shdr->slice_alpha_c0_offset_div2);
      IN_RANGE_OR_RETURN(shdr->slice_alpha_c0_offset_div2, -6, 6);

      READ_SE_OR_RETURN(&shdr->slice_beta_offset_div2);
      IN_RANGE_OR_RETURN(shdr->slice_beta_offset_div2, -6, 6);
    }
  }

  if (pps->num_slice_groups_minus1 > 0) {
    DVLOG(1) << "Slice groups not supported";
    return kUnsupportedStream;
  }

  size_t epb = br_.NumEmulationPreventionBytesRead();
  shdr->header_bit_size = (shdr->nalu_size - epb) * 8 - br_.NumBitsLeft();

  return kOk;
}

H264Parser::Result H264Parser::ParseSEI(H264SEI* sei) {
  int byte;
  int num_parsed_sei_msg = 0;
  // According to spec 7.3.2.3, we should loop parsing SEI NALU
  // as long as `more_rbsp_data` condition is true, which means
  // if the NALU's RBSP data is large enough and `more_rbsp_data`
  // condition keeps true all the time, we are very likely have to
  // loop the parsing millions of times.
  //
  // The spec doesn't provide any pattern to let us validate the
  // the parsed SEI messages, so we have to set a limit here.
  constexpr int kMaxParsedSEIMessages = 64;
  do {
    H264SEIMessage sei_msg;
    sei_msg.type = 0;
    READ_BITS_OR_RETURN(8, &byte);
    while (byte == 0xff) {
      sei_msg.type += 255;
      READ_BITS_OR_RETURN(8, &byte);
    }
    sei_msg.type += byte;

    sei_msg.payload_size = 0;
    READ_BITS_OR_RETURN(8, &byte);
    while (byte == 0xff) {
      sei_msg.payload_size += 255;
      READ_BITS_OR_RETURN(8, &byte);
    }
    sei_msg.payload_size += byte;
    int num_bits_remain = sei_msg.payload_size * 8;

    DVLOG(4) << "Found SEI message type: " << sei_msg.type
             << " payload size: " << sei_msg.payload_size;

    switch (sei_msg.type) {
      case H264SEIMessage::kSEIRecoveryPoint:
        READ_UE_AND_MINUS_BITS_READ_OR_RETURN(
            &sei_msg.recovery_point.recovery_frame_cnt, &num_bits_remain);
        READ_BOOL_AND_MINUS_BITS_READ_OR_RETURN(
            &sei_msg.recovery_point.exact_match_flag, &num_bits_remain);
        READ_BOOL_AND_MINUS_BITS_READ_OR_RETURN(
            &sei_msg.recovery_point.broken_link_flag, &num_bits_remain);
        READ_BITS_AND_MINUS_BITS_READ_OR_RETURN(
            2, &sei_msg.recovery_point.changing_slice_group_idc,
            &num_bits_remain);
        break;
      case H264SEIMessage::kSEIContentLightLevelInfo:
        READ_BITS_AND_MINUS_BITS_READ_OR_RETURN(
            16, &sei_msg.content_light_level_info.max_content_light_level,
            &num_bits_remain);
        READ_BITS_AND_MINUS_BITS_READ_OR_RETURN(
            16,
            &sei_msg.content_light_level_info.max_picture_average_light_level,
            &num_bits_remain);
        break;
      case H264SEIMessage::kSEIMasteringDisplayInfo:
        for (auto& primary : sei_msg.mastering_display_info.display_primaries) {
          for (auto& component : primary) {
            READ_BITS_AND_MINUS_BITS_READ_OR_RETURN(16, &component,
                                                    &num_bits_remain);
          }
        }
        READ_BITS_AND_MINUS_BITS_READ_OR_RETURN(
            16, &sei_msg.mastering_display_info.white_points[0],
            &num_bits_remain);
        READ_BITS_AND_MINUS_BITS_READ_OR_RETURN(
            16, &sei_msg.mastering_display_info.white_points[1],
            &num_bits_remain);
        uint32_t luminace_high_31bits, luminance_low_1bit;
        READ_BITS_AND_MINUS_BITS_READ_OR_RETURN(31, &luminace_high_31bits,
                                                &num_bits_remain);
        READ_BITS_AND_MINUS_BITS_READ_OR_RETURN(1, &luminance_low_1bit,
                                                &num_bits_remain);
        sei_msg.mastering_display_info.max_luminance =
            (luminace_high_31bits << 1) + (luminance_low_1bit & 0x1);
        READ_BITS_AND_MINUS_BITS_READ_OR_RETURN(31, &luminace_high_31bits,
                                                &num_bits_remain);
        READ_BITS_AND_MINUS_BITS_READ_OR_RETURN(1, &luminance_low_1bit,
                                                &num_bits_remain);
        sei_msg.mastering_display_info.min_luminance =
            (luminace_high_31bits << 1) + (luminance_low_1bit & 0x1);
        break;
      default:
        DVLOG(4) << "Unsupported SEI message";
        break;
    }
    TRUE_OR_RETURN(num_bits_remain >= 0);
    // D.1.1 Not byted aligned in payload or unsupported SEI, skip bits.
    if (num_bits_remain > 0)
      SKIP_BITS_OR_RETURN(num_bits_remain);
    // Only add parsed SEI messages.
    if (num_bits_remain < sei_msg.payload_size * 8)
      sei->msgs.push_back(sei_msg);
    // In case the loop endless.
    if (++num_parsed_sei_msg > kMaxParsedSEIMessages)
      return kInvalidStream;
  } while (br_.HasMoreRBSPData());

  return kOk;
}

std::vector<SubsampleEntry> H264Parser::GetCurrentSubsamples() {
  DCHECK_EQ(previous_nalu_range_.size(), 1u)
      << "This should only be called after a "
         "successful call to AdvanceToNextNalu()";

  auto intersection = encrypted_ranges_.IntersectionWith(previous_nalu_range_);
  return EncryptedRangesToSubsampleEntry(
      previous_nalu_range_.start(0), previous_nalu_range_.end(0), intersection);
}

}  // namespace media
