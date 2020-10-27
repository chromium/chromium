// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/h265_parser.h"

#include <stddef.h>

#include <algorithm>
#include <cmath>

#include "base/bits.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/stl_util.h"
#include "media/base/decrypt_config.h"
#include "media/base/video_codecs.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace media {

namespace {

// From Table 7-6.
constexpr int kDefaultScalingListSize1To3Matrix0To2[] = {
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 17, 16, 17, 16, 17, 18,
    17, 18, 18, 17, 18, 21, 19, 20, 21, 20, 19, 21, 24, 22, 22, 24,
    24, 22, 22, 24, 25, 25, 27, 30, 27, 25, 25, 29, 31, 35, 35, 31,
    29, 36, 41, 44, 41, 36, 47, 54, 54, 47, 65, 70, 65, 88, 88, 115,
};
constexpr int kDefaultScalingListSize1To3Matrix3To5[] = {
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 17, 17, 17, 17, 17, 18,
    18, 18, 18, 18, 18, 20, 20, 20, 20, 20, 20, 20, 24, 24, 24, 24,
    24, 24, 24, 24, 25, 25, 25, 25, 25, 25, 25, 28, 28, 28, 28, 28,
    28, 33, 33, 33, 33, 33, 41, 41, 41, 41, 54, 54, 54, 71, 71, 91,
};

// VUI parameters: Table E-1 "Interpretation of sample aspect ratio indicator"
constexpr int kTableSarWidth[] = {0,  1,  12, 10, 16,  40, 24, 20, 32,
                                  80, 18, 15, 64, 160, 4,  3,  2};
constexpr int kTableSarHeight[] = {0,  1,  11, 11, 11, 33, 11, 11, 11,
                                   33, 11, 11, 33, 99, 3,  2,  1};
static_assert(base::size(kTableSarWidth) == base::size(kTableSarHeight),
              "sar tables must have the same size");

void FillInDefaultScalingListData(H265ScalingListData* scaling_list_data,
                                  int size_id,
                                  int matrix_id) {
  if (size_id == 0) {
    std::fill_n(scaling_list_data->scaling_list_4x4[matrix_id],
                H265ScalingListData::kScalingListSizeId0Count,
                H265ScalingListData::kDefaultScalingListSize0Values);
    return;
  }

  int* dst;
  switch (size_id) {
    case 1:
      dst = scaling_list_data->scaling_list_8x8[matrix_id];
      break;
    case 2:
      dst = scaling_list_data->scaling_list_16x16[matrix_id];
      break;
    case 3:
      dst = scaling_list_data->scaling_list_32x32[matrix_id];
      break;
  }
  const int* src;
  if (matrix_id < 3)
    src = kDefaultScalingListSize1To3Matrix0To2;
  else
    src = kDefaultScalingListSize1To3Matrix3To5;
  memcpy(dst, src,
         H265ScalingListData::kScalingListSizeId1To3Count * sizeof(*src));

  // These are sixteen because the default for the minus8 values is 8.
  if (size_id == 2)
    scaling_list_data->scaling_list_dc_coef_16x16[matrix_id] = 16;
  else if (size_id == 3)
    scaling_list_data->scaling_list_dc_coef_32x32[matrix_id] = 16;
}

}  // namespace

#define READ_BITS_OR_RETURN(num_bits, out)                                 \
  do {                                                                     \
    int _out;                                                              \
    if (!br_.ReadBits(num_bits, &_out)) {                                  \
      DVLOG(1)                                                             \
          << "Error in stream: unexpected EOS while trying to read " #out; \
      return kInvalidStream;                                               \
    }                                                                      \
    *out = _out;                                                           \
  } while (0)

#define SKIP_BITS_OR_RETURN(num_bits)                                       \
  do {                                                                      \
    int bits_left = num_bits;                                               \
    int data;                                                               \
    while (bits_left > 0) {                                                 \
      if (!br_.ReadBits(bits_left > 16 ? 16 : bits_left, &data)) {          \
        DVLOG(1) << "Error in stream: unexpected EOS while trying to skip"; \
        return kInvalidStream;                                              \
      }                                                                     \
      bits_left -= 16;                                                      \
    }                                                                       \
  } while (0)

#define READ_BOOL_OR_RETURN(out)                                           \
  do {                                                                     \
    int _out;                                                              \
    if (!br_.ReadBits(1, &_out)) {                                         \
      DVLOG(1)                                                             \
          << "Error in stream: unexpected EOS while trying to read " #out; \
      return kInvalidStream;                                               \
    }                                                                      \
    *out = _out != 0;                                                      \
  } while (0)

#define READ_UE_OR_RETURN(out)                                                 \
  do {                                                                         \
    if (ReadUE(out) != kOk) {                                                  \
      DVLOG(1) << "Error in stream: invalid value while trying to read " #out; \
      return kInvalidStream;                                                   \
    }                                                                          \
  } while (0)

#define READ_SE_OR_RETURN(out)                                                 \
  do {                                                                         \
    if (ReadSE(out) != kOk) {                                                  \
      DVLOG(1) << "Error in stream: invalid value while trying to read " #out; \
      return kInvalidStream;                                                   \
    }                                                                          \
  } while (0)

#define IN_RANGE_OR_RETURN(val, min, max)                                   \
  do {                                                                      \
    if ((val) < (min) || (val) > (max)) {                                   \
      DVLOG(1) << "Error in stream: invalid value, expected " #val " to be" \
               << " in range [" << (min) << ":" << (max) << "]"             \
               << " found " << (val) << " instead";                         \
      return kInvalidStream;                                                \
    }                                                                       \
  } while (0)

#define TRUE_OR_RETURN(a)                                            \
  do {                                                               \
    if (!(a)) {                                                      \
      DVLOG(1) << "Error in stream: invalid value, expected " << #a; \
      return kInvalidStream;                                         \
    }                                                                \
  } while (0)

H265NALU::H265NALU() {
  memset(reinterpret_cast<void*>(this), 0, sizeof(*this));
}

H265ScalingListData::H265ScalingListData() {
  memset(reinterpret_cast<void*>(this), 0, sizeof(*this));
}

H265StRefPicSet::H265StRefPicSet() {
  memset(reinterpret_cast<void*>(this), 0, sizeof(*this));
}

H265SPS::H265SPS() {
  memset(reinterpret_cast<void*>(this), 0, sizeof(*this));
}

H265ProfileTierLevel::H265ProfileTierLevel() {
  memset(reinterpret_cast<void*>(this), 0, sizeof(*this));
}

H265VUIParameters::H265VUIParameters() {
  memset(reinterpret_cast<void*>(this), 0, sizeof(*this));
}

H265Parser::H265Parser() {
  Reset();
}

H265Parser::~H265Parser() {}

int H265ProfileTierLevel::GetMaxLumaPs() const {
  // From Table A.8 - General tier and level limits.
  // |general_level_idc| is 30x the actual level.
  if (general_level_idc <= 30)  // level 1
    return 36864;
  if (general_level_idc <= 60)  // level 2
    return 122880;
  if (general_level_idc <= 63)  // level 2.1
    return 245760;
  if (general_level_idc <= 90)  // level 3
    return 552960;
  if (general_level_idc <= 93)  // level 3.1
    return 983040;
  if (general_level_idc <= 123)  // level 4, 4.1
    return 2228224;
  if (general_level_idc <= 156)  // level 5, 5.1, 5.2
    return 8912896;
  // level 6, 6.1, 6.2 - beyond that there's no actual limit.
  return 35651584;
}

size_t H265ProfileTierLevel::GetDpbMaxPicBuf() const {
  // From A.4.2 - Profile-specific level limits for the video profiles.
  // If sps_curr_pic_ref_enabled_flag is required to be zero, than this is 6
  // otherwise it is 7.
  return (general_profile_idc >= kProfileIdcMain &&
          general_profile_idc <= kProfileIdcHighThroughput)
             ? 6
             : 7;
}

gfx::Size H265SPS::GetCodedSize() const {
  return gfx::Size(pic_width_in_luma_samples, pic_height_in_luma_samples);
}

gfx::Rect H265SPS::GetVisibleRect() const {
  // 7.4.3.2.1
  // These are verified in the parser that they won't overflow.
  int left = (conf_win_left_offset + vui_parameters.def_disp_win_left_offset) *
             sub_width_c;
  int top = (conf_win_top_offset + vui_parameters.def_disp_win_top_offset) *
            sub_height_c;
  int right =
      (conf_win_right_offset + vui_parameters.def_disp_win_right_offset) *
      sub_width_c;
  int bottom =
      (conf_win_bottom_offset + vui_parameters.def_disp_win_bottom_offset) *
      sub_height_c;
  return gfx::Rect(left, top, pic_width_in_luma_samples - left - right,
                   pic_height_in_luma_samples - top - bottom);
}

// From E.3.1 VUI parameters semantics
VideoColorSpace H265SPS::GetColorSpace() const {
  if (!vui_parameters.colour_description_present_flag)
    return VideoColorSpace();

  return VideoColorSpace(
      vui_parameters.colour_primaries, vui_parameters.transfer_characteristics,
      vui_parameters.matrix_coeffs,
      vui_parameters.video_full_range_flag ? gfx::ColorSpace::RangeID::FULL
                                           : gfx::ColorSpace::RangeID::LIMITED);
}

void H265Parser::Reset() {
  stream_ = NULL;
  bytes_left_ = 0;
  encrypted_ranges_.clear();
}

void H265Parser::SetStream(const uint8_t* stream, off_t stream_size) {
  std::vector<SubsampleEntry> subsamples;
  SetEncryptedStream(stream, stream_size, subsamples);
}

void H265Parser::SetEncryptedStream(
    const uint8_t* stream,
    off_t stream_size,
    const std::vector<SubsampleEntry>& subsamples) {
  DCHECK(stream);
  DCHECK_GT(stream_size, 0);

  stream_ = stream;
  bytes_left_ = stream_size;

  encrypted_ranges_.clear();
  const uint8_t* start = stream;
  const uint8_t* stream_end = stream_ + bytes_left_;
  for (size_t i = 0; i < subsamples.size() && start < stream_end; ++i) {
    start += subsamples[i].clear_bytes;

    const uint8_t* end =
        std::min(start + subsamples[i].cypher_bytes, stream_end);
    encrypted_ranges_.Add(start, end);
    start = end;
  }
}

bool H265Parser::LocateNALU(off_t* nalu_size, off_t* start_code_size) {
  // Find the start code of next NALU.
  off_t nalu_start_off = 0;
  off_t annexb_start_code_size = 0;

  if (!H264Parser::FindStartCodeInClearRanges(
          stream_, bytes_left_, encrypted_ranges_, &nalu_start_off,
          &annexb_start_code_size)) {
    DVLOG(4) << "Could not find start code, end of stream?";
    return false;
  }

  // Move the stream to the beginning of the NALU (pointing at the start code).
  stream_ += nalu_start_off;
  bytes_left_ -= nalu_start_off;

  const uint8_t* nalu_data = stream_ + annexb_start_code_size;
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
  if (!H264Parser::FindStartCodeInClearRanges(
          nalu_data, max_nalu_data_size, encrypted_ranges_,
          &nalu_size_without_start_code, &next_start_code_size)) {
    nalu_size_without_start_code = max_nalu_data_size;
  }
  *nalu_size = nalu_size_without_start_code + annexb_start_code_size;
  *start_code_size = annexb_start_code_size;
  return true;
}

H265Parser::Result H265Parser::ReadUE(int* val) {
  // Count the number of contiguous zero bits.
  int bit;
  int num_bits = -1;
  do {
    READ_BITS_OR_RETURN(1, &bit);
    num_bits++;
  } while (bit == 0);

  if (num_bits > 31)
    return kInvalidStream;

  // Calculate exp-Golomb code value of size num_bits.
  // Special case for |num_bits| == 31 to avoid integer overflow. The only
  // valid representation as an int is 2^31 - 1, so the remaining bits must
  // be 0 or else the number is too large.
  *val = (1u << num_bits) - 1u;

  int rest;
  if (num_bits == 31) {
    READ_BITS_OR_RETURN(num_bits, &rest);
    return (rest == 0) ? kOk : kInvalidStream;
  }

  if (num_bits > 0) {
    READ_BITS_OR_RETURN(num_bits, &rest);
    *val += rest;
  }

  return kOk;
}

H265Parser::Result H265Parser::ReadSE(int* val) {
  // See Chapter 9 in the spec.
  int ue;
  Result res;
  res = ReadUE(&ue);
  if (res != kOk)
    return res;

  if (ue % 2 == 0)
    *val = -(ue / 2);
  else
    *val = ue / 2 + 1;

  return kOk;
}

H265Parser::Result H265Parser::AdvanceToNextNALU(H265NALU* nalu) {
  off_t start_code_size;
  off_t nalu_size_with_start_code;
  if (!LocateNALU(&nalu_size_with_start_code, &start_code_size)) {
    DVLOG(4) << "Could not find next NALU, bytes left in stream: "
             << bytes_left_;
    return kEOStream;
  }

  nalu->data = stream_ + start_code_size;
  nalu->size = nalu_size_with_start_code - start_code_size;
  DVLOG(4) << "NALU found: size=" << nalu_size_with_start_code;

  // Initialize bit reader at the start of found NALU.
  if (!br_.Initialize(nalu->data, nalu->size))
    return kEOStream;

  // Move parser state to after this NALU, so next time AdvanceToNextNALU
  // is called, we will effectively be skipping it;
  // other parsing functions will use the position saved
  // in bit reader for parsing, so we don't have to remember it here.
  stream_ += nalu_size_with_start_code;
  bytes_left_ -= nalu_size_with_start_code;

  // Read NALU header, skip the forbidden_zero_bit, but check for it.
  int data;
  READ_BITS_OR_RETURN(1, &data);
  TRUE_OR_RETURN(data == 0);

  READ_BITS_OR_RETURN(6, &nalu->nal_unit_type);
  READ_BITS_OR_RETURN(6, &nalu->nuh_layer_id);
  READ_BITS_OR_RETURN(3, &nalu->nuh_temporal_id_plus1);

  DVLOG(4) << "NALU type: " << static_cast<int>(nalu->nal_unit_type)
           << " at: " << reinterpret_cast<const void*>(nalu->data)
           << " size: " << nalu->size;

  return kOk;
}

H265Parser::Result H265Parser::ParseSPS(int* sps_id) {
  // 7.4.3.2
  DVLOG(4) << "Parsing SPS";
  Result res = kOk;

  *sps_id = -1;

  std::unique_ptr<H265SPS> sps = std::make_unique<H265SPS>();
  SKIP_BITS_OR_RETURN(4);  // sps_video_parameter_set_id
  READ_BITS_OR_RETURN(3, &sps->sps_max_sub_layers_minus1);
  IN_RANGE_OR_RETURN(sps->sps_max_sub_layers_minus1, 0, 6);
  SKIP_BITS_OR_RETURN(1);  // sps_temporal_id_nesting_flag

  res = ParseProfileTierLevel(true, sps->sps_max_sub_layers_minus1,
                              &sps->profile_tier_level);
  if (res != kOk)
    return res;

  READ_UE_OR_RETURN(&sps->sps_seq_parameter_set_id);
  IN_RANGE_OR_RETURN(sps->sps_seq_parameter_set_id, 0, 15);
  READ_UE_OR_RETURN(&sps->chroma_format_idc);
  IN_RANGE_OR_RETURN(sps->chroma_format_idc, 0, 3);
  if (sps->chroma_format_idc == 3) {
    READ_BOOL_OR_RETURN(&sps->separate_colour_plane_flag);
  }
  sps->chroma_array_type =
      sps->separate_colour_plane_flag ? 0 : sps->chroma_format_idc;
  // Table 6-1.
  if (sps->chroma_format_idc == 1) {
    sps->sub_width_c = sps->sub_height_c = 2;
  } else if (sps->chroma_format_idc == 2) {
    sps->sub_width_c = 2;
    sps->sub_height_c = 1;
  } else {
    sps->sub_width_c = sps->sub_height_c = 1;
  }
  READ_UE_OR_RETURN(&sps->pic_width_in_luma_samples);
  READ_UE_OR_RETURN(&sps->pic_height_in_luma_samples);
  TRUE_OR_RETURN(sps->pic_width_in_luma_samples != 0);
  TRUE_OR_RETURN(sps->pic_height_in_luma_samples != 0);

  // Equation A-2: Calculate max_dpb_size.
  int max_luma_ps = sps->profile_tier_level.GetMaxLumaPs();
  int pic_size_in_samples_y = sps->pic_height_in_luma_samples;
  size_t max_dpb_pic_buf = sps->profile_tier_level.GetDpbMaxPicBuf();
  if (pic_size_in_samples_y <= (max_luma_ps >> 2))
    sps->max_dpb_size = std::min(4 * max_dpb_pic_buf, size_t{16});
  else if (pic_size_in_samples_y <= (max_luma_ps >> 1))
    sps->max_dpb_size = std::min(2 * max_dpb_pic_buf, size_t{16});
  else if (pic_size_in_samples_y <= ((3 * max_luma_ps) >> 2))
    sps->max_dpb_size = std::min((4 * max_dpb_pic_buf) / 3, size_t{16});
  else
    sps->max_dpb_size = max_dpb_pic_buf;

  bool conformance_window_flag;
  READ_BOOL_OR_RETURN(&conformance_window_flag);
  if (conformance_window_flag) {
    READ_UE_OR_RETURN(&sps->conf_win_left_offset);
    READ_UE_OR_RETURN(&sps->conf_win_right_offset);
    READ_UE_OR_RETURN(&sps->conf_win_top_offset);
    READ_UE_OR_RETURN(&sps->conf_win_bottom_offset);
    base::CheckedNumeric<int> width_crop = sps->conf_win_left_offset;
    width_crop += sps->conf_win_right_offset;
    width_crop *= sps->sub_width_c;
    if (!width_crop.IsValid())
      return kInvalidStream;
    TRUE_OR_RETURN(width_crop.ValueOrDefault(0) <
                   sps->pic_width_in_luma_samples);
    base::CheckedNumeric<int> height_crop = sps->conf_win_top_offset;
    width_crop += sps->conf_win_bottom_offset;
    width_crop *= sps->sub_height_c;
    if (!height_crop.IsValid())
      return kInvalidStream;
    TRUE_OR_RETURN(height_crop.ValueOrDefault(0) <
                   sps->pic_height_in_luma_samples);
  }
  READ_UE_OR_RETURN(&sps->bit_depth_luma_minus8);
  IN_RANGE_OR_RETURN(sps->bit_depth_luma_minus8, 0, 8);
  sps->bit_depth_y = sps->bit_depth_luma_minus8 + 8;
  READ_UE_OR_RETURN(&sps->bit_depth_chroma_minus8);
  IN_RANGE_OR_RETURN(sps->bit_depth_chroma_minus8, 0, 8);
  sps->bit_depth_c = sps->bit_depth_chroma_minus8 + 8;
  READ_UE_OR_RETURN(&sps->log2_max_pic_order_cnt_lsb_minus4);
  IN_RANGE_OR_RETURN(sps->log2_max_pic_order_cnt_lsb_minus4, 0, 12);
  sps->max_pic_order_cnt_lsb =
      std::pow(2, sps->log2_max_pic_order_cnt_lsb_minus4 + 4);
  bool sps_sub_layer_ordering_info_present_flag;
  READ_BOOL_OR_RETURN(&sps_sub_layer_ordering_info_present_flag);
  for (int i = sps_sub_layer_ordering_info_present_flag
                   ? 0
                   : sps->sps_max_sub_layers_minus1;
       i <= sps->sps_max_sub_layers_minus1; ++i) {
    READ_UE_OR_RETURN(&sps->sps_max_dec_pic_buffering_minus1[i]);
    IN_RANGE_OR_RETURN(sps->sps_max_dec_pic_buffering_minus1[i], 0,
                       static_cast<int>(sps->max_dpb_size) - 1);
    READ_UE_OR_RETURN(&sps->sps_max_num_reorder_pics[i]);
    IN_RANGE_OR_RETURN(sps->sps_max_num_reorder_pics[i], 0,
                       sps->sps_max_dec_pic_buffering_minus1[i]);
    if (i > 0) {
      TRUE_OR_RETURN(sps->sps_max_dec_pic_buffering_minus1[i] >=
                     sps->sps_max_dec_pic_buffering_minus1[i - 1]);
      TRUE_OR_RETURN(sps->sps_max_num_reorder_pics[i] >=
                     sps->sps_max_num_reorder_pics[i - 1]);
    }
    READ_UE_OR_RETURN(&sps->sps_max_latency_increase_plus1[i]);
    sps->sps_max_latency_pictures[i] = sps->sps_max_num_reorder_pics[i] +
                                       sps->sps_max_latency_increase_plus1[i] -
                                       1;
  }
  if (!sps_sub_layer_ordering_info_present_flag) {
    // Fill in the default values for the other sublayers.
    for (int i = 0; i < sps->sps_max_sub_layers_minus1; ++i) {
      sps->sps_max_dec_pic_buffering_minus1[i] =
          sps->sps_max_dec_pic_buffering_minus1[sps->sps_max_sub_layers_minus1];
      sps->sps_max_num_reorder_pics[i] =
          sps->sps_max_num_reorder_pics[sps->sps_max_sub_layers_minus1];
      sps->sps_max_latency_increase_plus1[i] =
          sps->sps_max_latency_increase_plus1[sps->sps_max_sub_layers_minus1];
      sps->sps_max_latency_pictures[i] =
          sps->sps_max_num_reorder_pics[i] +
          sps->sps_max_latency_increase_plus1[i] - 1;
    }
  }
  READ_UE_OR_RETURN(&sps->log2_min_luma_coding_block_size_minus3);
  READ_UE_OR_RETURN(&sps->log2_diff_max_min_luma_coding_block_size);

  int min_cb_log2_size_y = sps->log2_min_luma_coding_block_size_minus3 + 3;
  sps->ctb_log2_size_y =
      min_cb_log2_size_y + sps->log2_diff_max_min_luma_coding_block_size;
  int min_cb_size_y = 1 << min_cb_log2_size_y;
  int ctb_size_y = 1 << sps->ctb_log2_size_y;
  sps->pic_width_in_ctbs_y = base::ClampCeil(
      static_cast<float>(sps->pic_width_in_luma_samples) / ctb_size_y);
  sps->pic_height_in_ctbs_y = base::ClampCeil(
      static_cast<float>(sps->pic_height_in_luma_samples) / ctb_size_y);
  sps->pic_size_in_ctbs_y =
      sps->pic_width_in_ctbs_y * sps->pic_height_in_ctbs_y;

  TRUE_OR_RETURN(sps->pic_width_in_luma_samples % min_cb_size_y == 0);
  TRUE_OR_RETURN(sps->pic_height_in_luma_samples % min_cb_size_y == 0);
  READ_UE_OR_RETURN(&sps->log2_min_luma_transform_block_size_minus2);
  int min_tb_log2_size_y = sps->log2_min_luma_transform_block_size_minus2 + 2;
  TRUE_OR_RETURN(min_tb_log2_size_y < min_cb_log2_size_y);
  READ_UE_OR_RETURN(&sps->log2_diff_max_min_luma_transform_block_size);
  sps->max_tb_log2_size_y =
      min_tb_log2_size_y + sps->log2_diff_max_min_luma_transform_block_size;
  TRUE_OR_RETURN(sps->max_tb_log2_size_y <= std::min(sps->ctb_log2_size_y, 5));
  READ_UE_OR_RETURN(&sps->max_transform_hierarchy_depth_inter);
  IN_RANGE_OR_RETURN(sps->max_transform_hierarchy_depth_inter, 0,
                     sps->ctb_log2_size_y - min_tb_log2_size_y);
  READ_UE_OR_RETURN(&sps->max_transform_hierarchy_depth_intra);
  IN_RANGE_OR_RETURN(sps->max_transform_hierarchy_depth_intra, 0,
                     sps->ctb_log2_size_y - min_tb_log2_size_y);
  READ_BOOL_OR_RETURN(&sps->scaling_list_enabled_flag);
  if (sps->scaling_list_enabled_flag) {
    READ_BOOL_OR_RETURN(&sps->sps_scaling_list_data_present_flag);
    res = ParseScalingListData(&sps->scaling_list_data);
    if (res != kOk)
      return res;
  } else {
    // Fill it in with the default values.
    for (int size_id = 0; size_id < 4; ++size_id) {
      for (int matrix_id = 0; matrix_id < 6;
           matrix_id += (size_id == 3) ? 3 : 1) {
        FillInDefaultScalingListData(&sps->scaling_list_data, size_id,
                                     matrix_id);
      }
    }
  }
  READ_BOOL_OR_RETURN(&sps->amp_enabled_flag);
  READ_BOOL_OR_RETURN(&sps->sample_adaptive_offset_enabled_flag);
  READ_BOOL_OR_RETURN(&sps->pcm_enabled_flag);
  if (sps->pcm_enabled_flag) {
    READ_BITS_OR_RETURN(4, &sps->pcm_sample_bit_depth_luma_minus1);
    TRUE_OR_RETURN(sps->pcm_sample_bit_depth_luma_minus1 + 1 <=
                   sps->bit_depth_y);
    READ_BITS_OR_RETURN(4, &sps->pcm_sample_bit_depth_chroma_minus1);
    TRUE_OR_RETURN(sps->pcm_sample_bit_depth_chroma_minus1 + 1 <=
                   sps->bit_depth_c);
    READ_UE_OR_RETURN(&sps->log2_min_pcm_luma_coding_block_size_minus3);
    int log2_min_ipcm_cb_size_y =
        sps->log2_min_pcm_luma_coding_block_size_minus3 + 3;
    IN_RANGE_OR_RETURN(log2_min_ipcm_cb_size_y, std::min(min_cb_log2_size_y, 5),
                       std::min(sps->ctb_log2_size_y, 5));
    READ_UE_OR_RETURN(&sps->log2_diff_max_min_pcm_luma_coding_block_size);
    int log2_max_ipcm_cb_size_y =
        log2_min_ipcm_cb_size_y +
        sps->log2_diff_max_min_pcm_luma_coding_block_size;
    TRUE_OR_RETURN(log2_max_ipcm_cb_size_y <=
                   std::min(sps->ctb_log2_size_y, 5));
    READ_BOOL_OR_RETURN(&sps->pcm_loop_filter_disabled_flag);
  }
  READ_UE_OR_RETURN(&sps->num_short_term_ref_pic_sets);
  IN_RANGE_OR_RETURN(sps->num_short_term_ref_pic_sets, 0,
                     kMaxShortTermRefPicSets);
  for (int i = 0; i < sps->num_short_term_ref_pic_sets; ++i) {
    res = ParseStRefPicSet(i, *sps, &sps->st_ref_pic_set[i]);
    if (res != kOk)
      return res;
  }
  READ_BOOL_OR_RETURN(&sps->long_term_ref_pics_present_flag);
  if (sps->long_term_ref_pics_present_flag) {
    READ_UE_OR_RETURN(&sps->num_long_term_ref_pics_sps);
    IN_RANGE_OR_RETURN(sps->num_long_term_ref_pics_sps, 0,
                       kMaxLongTermRefPicSets);
    for (int i = 0; i < sps->num_long_term_ref_pics_sps; ++i) {
      READ_BITS_OR_RETURN(sps->log2_max_pic_order_cnt_lsb_minus4 + 4,
                          &sps->lt_ref_pic_poc_lsb_sps[i]);
      READ_BOOL_OR_RETURN(&sps->used_by_curr_pic_lt_sps_flag[i]);
    }
  }
  READ_BOOL_OR_RETURN(&sps->sps_temporal_mvp_enabled_flag);
  READ_BOOL_OR_RETURN(&sps->strong_intra_smoothing_enabled_flag);
  bool vui_parameters_present_flag;
  READ_BOOL_OR_RETURN(&vui_parameters_present_flag);
  if (vui_parameters_present_flag) {
    res = ParseVuiParameters(*sps, &sps->vui_parameters);
    if (res != kOk)
      return res;
    // Verify cropping parameters. We already verified the conformance window
    // ranges previously.
    base::CheckedNumeric<int> width_crop =
        sps->conf_win_left_offset + sps->conf_win_right_offset;
    width_crop += sps->vui_parameters.def_disp_win_left_offset;
    width_crop += sps->vui_parameters.def_disp_win_right_offset;
    width_crop *= sps->sub_width_c;
    if (!width_crop.IsValid())
      return kInvalidStream;
    TRUE_OR_RETURN(width_crop.ValueOrDefault(0) <
                   sps->pic_width_in_luma_samples);
    base::CheckedNumeric<int> height_crop =
        sps->conf_win_top_offset + sps->conf_win_bottom_offset;
    height_crop += sps->vui_parameters.def_disp_win_top_offset;
    height_crop += sps->vui_parameters.def_disp_win_bottom_offset;
    height_crop *= sps->sub_height_c;
    if (!height_crop.IsValid())
      return kInvalidStream;
    TRUE_OR_RETURN(height_crop.ValueOrDefault(0) <
                   sps->pic_height_in_luma_samples);
  }

  bool sps_extension_present_flag;
  bool sps_range_extension_flag = false;
  bool sps_multilayer_extension_flag = false;
  bool sps_3d_extension_flag = false;
  bool sps_scc_extension_flag = false;
  READ_BOOL_OR_RETURN(&sps_extension_present_flag);
  if (sps_extension_present_flag) {
    READ_BOOL_OR_RETURN(&sps_range_extension_flag);
    READ_BOOL_OR_RETURN(&sps_multilayer_extension_flag);
    READ_BOOL_OR_RETURN(&sps_3d_extension_flag);
    READ_BOOL_OR_RETURN(&sps_scc_extension_flag);
    SKIP_BITS_OR_RETURN(4);  // sps_extension_4bits
  }
  if (sps_range_extension_flag) {
    DVLOG(1) << "HEVC range extension not supported";
    return kInvalidStream;
  }
  if (sps_multilayer_extension_flag) {
    DVLOG(1) << "HEVC multilayer extension not supported";
    return kInvalidStream;
  }
  if (sps_3d_extension_flag) {
    DVLOG(1) << "HEVC 3D extension not supported";
    return kInvalidStream;
  }
  if (sps_scc_extension_flag) {
    DVLOG(1) << "HEVC SCC extension not supported";
    return kInvalidStream;
  }

  // NOTE: The below 2 values are dependent upon the range extension if that is
  // ever implemented.
  sps->wp_offset_half_range_y = 1 << 7;
  sps->wp_offset_half_range_c = 1 << 7;

  // If an SPS with the same id already exists, replace it.
  *sps_id = sps->sps_seq_parameter_set_id;
  active_sps_[*sps_id] = std::move(sps);

  return res;
}

const H265SPS* H265Parser::GetSPS(int sps_id) const {
  auto it = active_sps_.find(sps_id);
  if (it == active_sps_.end()) {
    DVLOG(1) << "Requested a nonexistent SPS id " << sps_id;
    return nullptr;
  }

  return it->second.get();
}

// static
VideoCodecProfile H265Parser::ProfileIDCToVideoCodecProfile(int profile_idc) {
  switch (profile_idc) {
    case H265ProfileTierLevel::kProfileIdcMain:
      return HEVCPROFILE_MAIN;
    case H265ProfileTierLevel::kProfileIdcMain10:
      return HEVCPROFILE_MAIN10;
    case H265ProfileTierLevel::kProfileIdcMainStill:
      return HEVCPROFILE_MAIN_STILL_PICTURE;
    default:
      DVLOG(1) << "unknown video profile: " << profile_idc;
      return VIDEO_CODEC_PROFILE_UNKNOWN;
  }
}

H265Parser::Result H265Parser::ParseProfileTierLevel(
    bool profile_present,
    int max_num_sub_layers_minus1,
    H265ProfileTierLevel* profile_tier_level) {
  // 7.4.4
  DVLOG(4) << "Parsing profile_tier_level";
  if (profile_present) {
    int general_profile_space;
    READ_BITS_OR_RETURN(2, &general_profile_space);
    TRUE_OR_RETURN(general_profile_space == 0);
    SKIP_BITS_OR_RETURN(1);  // general_tier_flag
    READ_BITS_OR_RETURN(5, &profile_tier_level->general_profile_idc);
    IN_RANGE_OR_RETURN(profile_tier_level->general_profile_idc, 0, 11);
    bool general_profile_compatibility_flag[32];
    for (int j = 0; j < 32; ++j) {
      READ_BOOL_OR_RETURN(&general_profile_compatibility_flag[j]);
    }
    bool general_progressive_source_flag;
    bool general_interlaced_source_flag;
    READ_BOOL_OR_RETURN(&general_progressive_source_flag);
    READ_BOOL_OR_RETURN(&general_interlaced_source_flag);
    if (!general_progressive_source_flag && general_interlaced_source_flag) {
      DVLOG(1) << "Interlaced streams not supported";
      return kUnsupportedStream;
    }
    SKIP_BITS_OR_RETURN(2);  // general_{non_packed,frame_only}_constraint_flag
    // Skip the compatibility flags, they are always 43 bits.
    SKIP_BITS_OR_RETURN(43);
    SKIP_BITS_OR_RETURN(1);  // general_inbld_flag
  }
  READ_BITS_OR_RETURN(8, &profile_tier_level->general_level_idc);
  bool sub_layer_profile_present_flag[8];
  bool sub_layer_level_present_flag[8];
  for (int i = 0; i < max_num_sub_layers_minus1; ++i) {
    READ_BOOL_OR_RETURN(&sub_layer_profile_present_flag[i]);
    READ_BOOL_OR_RETURN(&sub_layer_level_present_flag[i]);
  }
  if (max_num_sub_layers_minus1 > 0) {
    for (int i = max_num_sub_layers_minus1; i < 8; i++) {
      SKIP_BITS_OR_RETURN(2);
    }
  }
  for (int i = 0; i < max_num_sub_layers_minus1; i++) {
    if (sub_layer_profile_present_flag[i]) {
      SKIP_BITS_OR_RETURN(2);   // sub_layer_profile_space
      SKIP_BITS_OR_RETURN(1);   // sub_layer_tier_flag
      SKIP_BITS_OR_RETURN(5);   // sub_layer_profile_idc
      SKIP_BITS_OR_RETURN(32);  // sub_layer_profile_compatibility_flag
      SKIP_BITS_OR_RETURN(2);  // sub_layer_{progressive,interlaced}_source_flag
      // Ignore sub_layer_non_packed_constraint_flag and
      // sub_layer_frame_only_constraint_flag.
      SKIP_BITS_OR_RETURN(2);
      // Skip the compatibility flags, they are always 43 bits.
      SKIP_BITS_OR_RETURN(43);
      SKIP_BITS_OR_RETURN(1);  // sub_layer_inbld_flag
    }
    if (sub_layer_level_present_flag[i]) {
      SKIP_BITS_OR_RETURN(8);  // sub_layer_level_idc
    }
  }

  return kOk;
}

H265Parser::Result H265Parser::ParseScalingListData(
    H265ScalingListData* scaling_list_data) {
  for (int size_id = 0; size_id < 4; ++size_id) {
    for (int matrix_id = 0; matrix_id < 6;
         matrix_id += (size_id == 3) ? 3 : 1) {
      bool scaling_list_pred_mode_flag;
      READ_BOOL_OR_RETURN(&scaling_list_pred_mode_flag);
      if (!scaling_list_pred_mode_flag) {
        int scaling_list_pred_matrix_id_delta;
        READ_UE_OR_RETURN(&scaling_list_pred_matrix_id_delta);
        if (size_id <= 2) {
          IN_RANGE_OR_RETURN(scaling_list_pred_matrix_id_delta, 0, matrix_id);
        } else {  // size_id == 3
          IN_RANGE_OR_RETURN(scaling_list_pred_matrix_id_delta, 0,
                             matrix_id / 3);
        }
        if (scaling_list_pred_matrix_id_delta == 0) {
          FillInDefaultScalingListData(scaling_list_data, size_id, matrix_id);
        } else {
          int ref_matrix_id = matrix_id - scaling_list_pred_matrix_id_delta *
                                              (size_id == 3 ? 3 : 1);
          int* dst;
          int* src;
          int count = H265ScalingListData::kScalingListSizeId1To3Count;
          switch (size_id) {
            case 0:
              src = scaling_list_data->scaling_list_4x4[ref_matrix_id];
              dst = scaling_list_data->scaling_list_4x4[matrix_id];
              count = H265ScalingListData::kScalingListSizeId0Count;
              break;
            case 1:
              src = scaling_list_data->scaling_list_8x8[ref_matrix_id];
              dst = scaling_list_data->scaling_list_8x8[matrix_id];
              break;
            case 2:
              src = scaling_list_data->scaling_list_16x16[ref_matrix_id];
              dst = scaling_list_data->scaling_list_16x16[matrix_id];
              break;
            case 3:
              src = scaling_list_data->scaling_list_32x32[ref_matrix_id];
              dst = scaling_list_data->scaling_list_32x32[matrix_id];
              break;
          }
          memcpy(dst, src, count * sizeof(*src));

          if (size_id == 2) {
            scaling_list_data->scaling_list_dc_coef_16x16[matrix_id] =
                scaling_list_data->scaling_list_dc_coef_16x16[ref_matrix_id];
          } else if (size_id == 3) {
            scaling_list_data->scaling_list_dc_coef_32x32[matrix_id] =
                scaling_list_data->scaling_list_dc_coef_32x32[ref_matrix_id];
          }
        }
      } else {
        int next_coef = 8;
        int coef_num = std::min(64, (1 << (4 + (size_id << 1))));
        if (size_id > 1) {
          if (size_id == 2) {
            READ_SE_OR_RETURN(
                &scaling_list_data->scaling_list_dc_coef_16x16[matrix_id]);
            IN_RANGE_OR_RETURN(
                scaling_list_data->scaling_list_dc_coef_16x16[matrix_id], -7,
                247);
            // This is parsed as minus8;
            scaling_list_data->scaling_list_dc_coef_16x16[matrix_id] += 8;
            next_coef =
                scaling_list_data->scaling_list_dc_coef_16x16[matrix_id];
          } else {  // size_id == 3
            READ_SE_OR_RETURN(
                &scaling_list_data->scaling_list_dc_coef_32x32[matrix_id]);
            IN_RANGE_OR_RETURN(
                scaling_list_data->scaling_list_dc_coef_32x32[matrix_id], -7,
                247);
            // This is parsed as minus8;
            scaling_list_data->scaling_list_dc_coef_32x32[matrix_id] += 8;
            next_coef =
                scaling_list_data->scaling_list_dc_coef_32x32[matrix_id];
          }
        }
        for (int i = 0; i < coef_num; ++i) {
          int scaling_list_delta_coef;
          READ_SE_OR_RETURN(&scaling_list_delta_coef);
          IN_RANGE_OR_RETURN(scaling_list_delta_coef, -128, 127);
          next_coef = (next_coef + scaling_list_delta_coef + 256) % 256;
          switch (size_id) {
            case 0:
              scaling_list_data->scaling_list_4x4[matrix_id][i] = next_coef;
              break;
            case 1:
              scaling_list_data->scaling_list_8x8[matrix_id][i] = next_coef;
              break;
            case 2:
              scaling_list_data->scaling_list_16x16[matrix_id][i] = next_coef;
              break;
            case 3:
              scaling_list_data->scaling_list_32x32[matrix_id][i] = next_coef;
              break;
          }
        }
      }
    }
  }
  return kOk;
}

H265Parser::Result H265Parser::ParseStRefPicSet(
    int st_rps_idx,
    const H265SPS& sps,
    H265StRefPicSet* st_ref_pic_set) {
  // 7.4.8
  bool inter_ref_pic_set_prediction_flag = false;
  if (st_rps_idx != 0) {
    READ_BOOL_OR_RETURN(&inter_ref_pic_set_prediction_flag);
  }
  if (inter_ref_pic_set_prediction_flag) {
    int delta_idx_minus1 = 0;
    if (st_rps_idx == sps.num_short_term_ref_pic_sets) {
      READ_UE_OR_RETURN(&delta_idx_minus1);
      IN_RANGE_OR_RETURN(delta_idx_minus1, 0, st_rps_idx - 1);
    }
    int ref_rps_idx = st_rps_idx - (delta_idx_minus1 + 1);
    int delta_rps_sign;
    int abs_delta_rps_minus1;
    READ_BOOL_OR_RETURN(&delta_rps_sign);
    READ_UE_OR_RETURN(&abs_delta_rps_minus1);
    int delta_rps = (1 - 2 * delta_rps_sign) * (abs_delta_rps_minus1 + 1);
    const H265StRefPicSet& ref_set = sps.st_ref_pic_set[ref_rps_idx];
    bool used_by_curr_pic_flag[kMaxShortTermRefPicSets];
    bool use_delta_flag[kMaxShortTermRefPicSets];
    // 7.4.8 - use_delta_flag defaults to 1 if not present.
    std::fill_n(use_delta_flag, kMaxShortTermRefPicSets, true);

    for (int j = 0; j <= ref_set.num_delta_pocs; j++) {
      READ_BOOL_OR_RETURN(&used_by_curr_pic_flag[j]);
      if (!used_by_curr_pic_flag[j]) {
        READ_BOOL_OR_RETURN(&use_delta_flag[j]);
      }
      // The spec does not define how to calculate NumDeltaPocs when
      // inter_ref_pic_set_prediction_flag is set. FFMPEG does it by counting
      // the number of entries with flags set.
      if (used_by_curr_pic_flag[j] || use_delta_flag[j]) {
        st_ref_pic_set->num_delta_pocs++;
      }
    }
    // Calculate delta_poc_s{0,1}, used_by_curr_pic_s{0,1}, num_negative_pics
    // and num_positive_pics.
    // Equation 7-61
    int i = 0;
    for (int j = ref_set.num_positive_pics - 1; j >= 0; --j) {
      int d_poc = ref_set.delta_poc_s1[j] + delta_rps;
      if (d_poc < 0 && use_delta_flag[ref_set.num_negative_pics + j]) {
        st_ref_pic_set->delta_poc_s0[i] = d_poc;
        st_ref_pic_set->used_by_curr_pic_s0[i++] =
            used_by_curr_pic_flag[ref_set.num_negative_pics + j];
      }
    }
    if (delta_rps < 0 && use_delta_flag[ref_set.num_delta_pocs]) {
      st_ref_pic_set->delta_poc_s0[i] = delta_rps;
      st_ref_pic_set->used_by_curr_pic_s0[i++] =
          used_by_curr_pic_flag[ref_set.num_delta_pocs];
    }
    for (int j = 0; j < ref_set.num_negative_pics; ++j) {
      int d_poc = ref_set.delta_poc_s0[j] + delta_rps;
      if (d_poc < 0 && use_delta_flag[j]) {
        st_ref_pic_set->delta_poc_s0[i] = d_poc;
        st_ref_pic_set->used_by_curr_pic_s0[i++] = used_by_curr_pic_flag[j];
      }
    }
    st_ref_pic_set->num_negative_pics = i;
    // Equation 7-62
    i = 0;
    for (int j = ref_set.num_negative_pics - 1; j >= 0; --j) {
      int d_poc = ref_set.delta_poc_s0[j] + delta_rps;
      if (d_poc > 0 && use_delta_flag[j]) {
        st_ref_pic_set->delta_poc_s1[i] = d_poc;
        st_ref_pic_set->used_by_curr_pic_s1[i++] = used_by_curr_pic_flag[j];
      }
    }
    if (delta_rps > 0 && use_delta_flag[ref_set.num_delta_pocs]) {
      st_ref_pic_set->delta_poc_s1[i] = delta_rps;
      st_ref_pic_set->used_by_curr_pic_s1[i++] =
          used_by_curr_pic_flag[ref_set.num_delta_pocs];
    }
    for (int j = 0; j < ref_set.num_positive_pics; ++j) {
      int d_poc = ref_set.delta_poc_s1[j] + delta_rps;
      if (d_poc > 0 && use_delta_flag[ref_set.num_negative_pics + j]) {
        st_ref_pic_set->delta_poc_s1[i] = d_poc;
        st_ref_pic_set->used_by_curr_pic_s1[i++] =
            used_by_curr_pic_flag[ref_set.num_negative_pics + j];
      }
    }
    st_ref_pic_set->num_positive_pics = i;
  } else {
    READ_UE_OR_RETURN(&st_ref_pic_set->num_negative_pics);
    READ_UE_OR_RETURN(&st_ref_pic_set->num_positive_pics);
    IN_RANGE_OR_RETURN(
        st_ref_pic_set->num_negative_pics, 0,
        sps.sps_max_dec_pic_buffering_minus1[sps.sps_max_sub_layers_minus1]);
    IN_RANGE_OR_RETURN(
        st_ref_pic_set->num_positive_pics, 0,
        sps.sps_max_dec_pic_buffering_minus1[sps.sps_max_sub_layers_minus1] -
            st_ref_pic_set->num_negative_pics);
    for (int i = 0; i < st_ref_pic_set->num_negative_pics; ++i) {
      int delta_poc_s0_minus1;
      READ_UE_OR_RETURN(&delta_poc_s0_minus1);
      if (i == 0) {
        st_ref_pic_set->delta_poc_s0[i] = -(delta_poc_s0_minus1 + 1);
      } else {
        st_ref_pic_set->delta_poc_s0[i] =
            st_ref_pic_set->delta_poc_s0[i - 1] - (delta_poc_s0_minus1 + 1);
      }
      READ_BOOL_OR_RETURN(&st_ref_pic_set->used_by_curr_pic_s0[i]);
    }
    for (int i = 0; i < st_ref_pic_set->num_positive_pics; ++i) {
      int delta_poc_s1_minus1;
      READ_UE_OR_RETURN(&delta_poc_s1_minus1);
      if (i == 0) {
        st_ref_pic_set->delta_poc_s1[i] = delta_poc_s1_minus1 + 1;
      } else {
        st_ref_pic_set->delta_poc_s1[i] =
            st_ref_pic_set->delta_poc_s1[i - 1] + delta_poc_s1_minus1 + 1;
      }
      READ_BOOL_OR_RETURN(&st_ref_pic_set->used_by_curr_pic_s1[i]);
    }
    // Calculate num_delta_pocs.
    st_ref_pic_set->num_delta_pocs =
        st_ref_pic_set->num_negative_pics + st_ref_pic_set->num_positive_pics;
  }
  return kOk;
}

H265Parser::Result H265Parser::ParseVuiParameters(const H265SPS& sps,
                                                  H265VUIParameters* vui) {
  Result res = kOk;
  bool aspect_ratio_info_present_flag;
  READ_BOOL_OR_RETURN(&aspect_ratio_info_present_flag);
  if (aspect_ratio_info_present_flag) {
    int aspect_ratio_idc;
    READ_BITS_OR_RETURN(8, &aspect_ratio_idc);
    constexpr int kExtendedSar = 255;
    if (aspect_ratio_idc == kExtendedSar) {
      READ_BITS_OR_RETURN(16, &vui->sar_width);
      READ_BITS_OR_RETURN(16, &vui->sar_height);
    } else {
      const int max_aspect_ratio_idc = base::size(kTableSarWidth) - 1;
      IN_RANGE_OR_RETURN(aspect_ratio_idc, 0, max_aspect_ratio_idc);
      vui->sar_width = kTableSarWidth[aspect_ratio_idc];
      vui->sar_height = kTableSarHeight[aspect_ratio_idc];
    }
  }

  int data;
  // Read and ignore overscan info.
  READ_BOOL_OR_RETURN(&data);  // overscan_info_present_flag
  if (data)
    SKIP_BITS_OR_RETURN(1);  // overscan_appropriate_flag

  bool video_signal_type_present_flag;
  READ_BOOL_OR_RETURN(&video_signal_type_present_flag);
  if (video_signal_type_present_flag) {
    SKIP_BITS_OR_RETURN(3);  // video_format
    READ_BOOL_OR_RETURN(&vui->video_full_range_flag);
    READ_BOOL_OR_RETURN(&vui->colour_description_present_flag);
    if (vui->colour_description_present_flag) {
      // color description syntax elements
      READ_BITS_OR_RETURN(8, &vui->colour_primaries);
      READ_BITS_OR_RETURN(8, &vui->transfer_characteristics);
      READ_BITS_OR_RETURN(8, &vui->matrix_coeffs);
    }
  }

  READ_BOOL_OR_RETURN(&data);  // chroma_loc_info_present_flag
  if (data) {
    READ_UE_OR_RETURN(&data);  // chroma_sample_loc_type_top_field
    READ_UE_OR_RETURN(&data);  // chroma_sample_loc_type_bottom_field
  }

  // Ignore neutral_chroma_indication_flag, field_seq_flag and
  // frame_field_info_present_flag.
  SKIP_BITS_OR_RETURN(3);

  bool default_display_window_flag;
  READ_BOOL_OR_RETURN(&default_display_window_flag);
  if (default_display_window_flag) {
    READ_UE_OR_RETURN(&vui->def_disp_win_left_offset);
    READ_UE_OR_RETURN(&vui->def_disp_win_right_offset);
    READ_UE_OR_RETURN(&vui->def_disp_win_top_offset);
    READ_UE_OR_RETURN(&vui->def_disp_win_bottom_offset);
  }

  // Read and ignore timing info.
  READ_BOOL_OR_RETURN(&data);  // timing_info_present_flag
  if (data) {
    SKIP_BITS_OR_RETURN(32);     // vui_num_units_in_tick
    SKIP_BITS_OR_RETURN(32);     // vui_time_scale
    READ_BOOL_OR_RETURN(&data);  // vui_poc_proportional_to_timing_flag
    if (data)
      READ_UE_OR_RETURN(&data);  // vui_num_ticks_poc_diff_one_minus1
    res = ParseAndIgnoreHrdParameters(true, sps.sps_max_sub_layers_minus1);
    if (res != kOk)
      return res;
  }

  bool bitstream_restriction_flag;
  READ_BOOL_OR_RETURN(&bitstream_restriction_flag);
  if (bitstream_restriction_flag) {
    // Skip tiles_fixed_structure_flag, motion_vectors_over_pic_boundaries_flag
    // and restricted_ref_pic_lists_flag.
    SKIP_BITS_OR_RETURN(3);
    READ_UE_OR_RETURN(&data);  // min_spatial_segmentation_idc
    READ_UE_OR_RETURN(&data);  // max_bytes_per_pic_denom
    READ_UE_OR_RETURN(&data);  // max_bits_per_min_cu_denom
    READ_UE_OR_RETURN(&data);  // log2_max_mv_length_horizontal
    READ_UE_OR_RETURN(&data);  // log2_max_mv_length_vertical
  }

  return kOk;
}

H265Parser::Result H265Parser::ParseAndIgnoreHrdParameters(
    bool common_inf_present_flag,
    int max_num_sub_layers_minus1) {
  Result res = kOk;
  int data;
  READ_BOOL_OR_RETURN(&data);  // present_flag
  if (!data)
    return res;

  bool nal_hrd_parameters_present_flag = false;
  bool vcl_hrd_parameters_present_flag = false;
  bool sub_pic_hrd_params_present_flag = false;
  if (common_inf_present_flag) {
    READ_BOOL_OR_RETURN(&nal_hrd_parameters_present_flag);
    READ_BOOL_OR_RETURN(&vcl_hrd_parameters_present_flag);
    if (nal_hrd_parameters_present_flag || vcl_hrd_parameters_present_flag) {
      READ_BOOL_OR_RETURN(&sub_pic_hrd_params_present_flag);
      if (sub_pic_hrd_params_present_flag) {
        SKIP_BITS_OR_RETURN(8);  // tick_divisor_minus2
        SKIP_BITS_OR_RETURN(5);  // du_cpb_removal_delay_increment_length_minus1
        SKIP_BITS_OR_RETURN(1);  // sub_pic_cpb_params_in_pic_timing_sei_flag
        SKIP_BITS_OR_RETURN(5);  // dpb_output_delay_du_length_minus1
      }
      SKIP_BITS_OR_RETURN(4);  // bit_rate_scale;
      SKIP_BITS_OR_RETURN(4);  // cpb_size_scale;
      if (sub_pic_hrd_params_present_flag)
        SKIP_BITS_OR_RETURN(4);  // cpb_size_du_scale
      SKIP_BITS_OR_RETURN(5);    // initial_cpb_removal_delay_length_minus1
      SKIP_BITS_OR_RETURN(5);    // au_cpb_removal_delay_length_minus1
      SKIP_BITS_OR_RETURN(5);    // dpb_output_delay_length_minus1
    }
  }
  for (int i = 0; i <= max_num_sub_layers_minus1; ++i) {
    bool fixed_pic_rate_flag;
    READ_BOOL_OR_RETURN(&fixed_pic_rate_flag);  // general
    if (!fixed_pic_rate_flag)
      READ_BOOL_OR_RETURN(&fixed_pic_rate_flag);  // within_cvs
    bool low_delay_hrd_flag = false;
    if (fixed_pic_rate_flag)
      READ_UE_OR_RETURN(&data);  // elemental_duration_in_tc_minus1
    else
      READ_BOOL_OR_RETURN(&low_delay_hrd_flag);
    int cpb_cnt = 1;
    if (!low_delay_hrd_flag) {
      READ_UE_OR_RETURN(&cpb_cnt);
      cpb_cnt += 1;  // parsed as minus1
    }
    if (nal_hrd_parameters_present_flag) {
      res = ParseAndIgnoreSubLayerHrdParameters(
          cpb_cnt, sub_pic_hrd_params_present_flag);
      if (res != kOk)
        return res;
    }
    if (vcl_hrd_parameters_present_flag) {
      res = ParseAndIgnoreSubLayerHrdParameters(
          cpb_cnt, sub_pic_hrd_params_present_flag);
      if (res != kOk)
        return res;
    }
  }
  return res;
}

H265Parser::Result H265Parser::ParseAndIgnoreSubLayerHrdParameters(
    int cpb_cnt,
    bool sub_pic_hrd_params_present_flag) {
  int data;
  for (int i = 0; i < cpb_cnt; ++i) {
    READ_UE_OR_RETURN(&data);  // bit_rate_value_minus1[i]
    READ_UE_OR_RETURN(&data);  // cpb_size_value_minus1[i]
    if (sub_pic_hrd_params_present_flag) {
      READ_UE_OR_RETURN(&data);  // cpb_size_du_value_minus1[i]
      READ_UE_OR_RETURN(&data);  // bit_rate_du_value_minus1[i]
    }
    SKIP_BITS_OR_RETURN(1);  // cbr_flag[i]
  }
  return kOk;
}

}  // namespace media
