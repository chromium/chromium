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

// Converts [|start|, |end|) range with |encrypted_ranges| into a vector of
// SubsampleEntry. |encrypted_ranges| must be with in the range defined by
// |start| and |end|.
// It is OK to pass in empty |encrypted_ranges|; this will return a vector
// with single SubsampleEntry with clear_bytes set to the size of the buffer.
std::vector<SubsampleEntry> EncryptedRangesToSubsampleEntry(
    const uint8_t* start,
    const uint8_t* end,
    const Ranges<const uint8_t*>& encrypted_ranges) {
  std::vector<SubsampleEntry> subsamples(encrypted_ranges.size());
  const uint8_t* cur = start;
  for (size_t i = 0; i < encrypted_ranges.size(); ++i) {
    const uint8_t* encrypted_start = encrypted_ranges.start(i);
    DCHECK_GE(encrypted_start, cur)
        << "Encrypted range started before the current buffer pointer.";
    subsamples[i].clear_bytes = encrypted_start - cur;

    const uint8_t* encrypted_end = encrypted_ranges.end(i);
    subsamples[i].cypher_bytes = encrypted_end - encrypted_start;

    cur = encrypted_end;
    DCHECK_LE(cur, end) << "Encrypted range is outside the buffer range.";
  }

  // If there is more data in the buffer but not covered by encrypted_ranges,
  // then it must be in the clear.
  if (cur < end)
    subsamples.emplace_back(end - cur, 0);

  return subsamples;
}

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

H265PPS::H265PPS() {
  memset(reinterpret_cast<void*>(this), 0, sizeof(*this));
}

H265RefPicListsModifications::H265RefPicListsModifications() {
  memset(reinterpret_cast<void*>(this), 0, sizeof(*this));
}

H265PredWeightTable::H265PredWeightTable() {
  memset(reinterpret_cast<void*>(this), 0, sizeof(*this));
}

H265SliceHeader::H265SliceHeader() {
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

bool H265SliceHeader::IsISlice() const {
  return slice_type == kSliceTypeI;
}

bool H265SliceHeader::IsPSlice() const {
  return slice_type == kSliceTypeP;
}

bool H265SliceHeader::IsBSlice() const {
  return slice_type == kSliceTypeB;
}

void H265Parser::Reset() {
  stream_ = NULL;
  bytes_left_ = 0;
  encrypted_ranges_.clear();
  previous_nalu_range_.clear();
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
  previous_nalu_range_.clear();

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

  DCHECK(nalu);
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

  previous_nalu_range_.clear();
  previous_nalu_range_.Add(nalu->data, nalu->data + nalu->size);
  return kOk;
}

H265Parser::Result H265Parser::ParseSPS(int* sps_id) {
  // 7.4.3.2
  DVLOG(4) << "Parsing SPS";
  Result res = kOk;

  DCHECK(sps_id);
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
  base::CheckedNumeric<int> pic_size = sps->pic_height_in_luma_samples;
  pic_size *= sps->pic_width_in_luma_samples;
  if (!pic_size.IsValid())
    return kInvalidStream;
  int pic_size_in_samples_y = pic_size.ValueOrDefault(0);
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
    height_crop += sps->conf_win_bottom_offset;
    height_crop *= sps->sub_height_c;
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
    int sps_max_latency_increase_plus1;
    READ_UE_OR_RETURN(&sps_max_latency_increase_plus1);
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
    }
  }
  READ_UE_OR_RETURN(&sps->log2_min_luma_coding_block_size_minus3);
  // This enforces that min_cb_log2_size_y below will be <= 30 and prevents
  // integer overflow math there.
  TRUE_OR_RETURN(sps->log2_min_luma_coding_block_size_minus3 <= 27);
  READ_UE_OR_RETURN(&sps->log2_diff_max_min_luma_coding_block_size);

  int min_cb_log2_size_y = sps->log2_min_luma_coding_block_size_minus3 + 3;
  base::CheckedNumeric<int> ctb_log2_size_y = min_cb_log2_size_y;
  ctb_log2_size_y += sps->log2_diff_max_min_luma_coding_block_size;
  if (!ctb_log2_size_y.IsValid())
    return kInvalidStream;

  sps->ctb_log2_size_y = ctb_log2_size_y.ValueOrDefault(0);
  TRUE_OR_RETURN(sps->ctb_log2_size_y <= 30);
  int min_cb_size_y = 1 << min_cb_log2_size_y;
  int ctb_size_y = 1 << sps->ctb_log2_size_y;
  sps->pic_width_in_ctbs_y = base::ClampCeil(
      static_cast<float>(sps->pic_width_in_luma_samples) / ctb_size_y);
  sps->pic_height_in_ctbs_y = base::ClampCeil(
      static_cast<float>(sps->pic_height_in_luma_samples) / ctb_size_y);
  base::CheckedNumeric<int> pic_size_in_ctbs_y = sps->pic_width_in_ctbs_y;
  pic_size_in_ctbs_y *= sps->pic_height_in_ctbs_y;
  if (!pic_size_in_ctbs_y.IsValid())
    return kInvalidStream;
  sps->pic_size_in_ctbs_y = pic_size_in_ctbs_y.ValueOrDefault(0);

  TRUE_OR_RETURN(sps->pic_width_in_luma_samples % min_cb_size_y == 0);
  TRUE_OR_RETURN(sps->pic_height_in_luma_samples % min_cb_size_y == 0);
  READ_UE_OR_RETURN(&sps->log2_min_luma_transform_block_size_minus2);
  TRUE_OR_RETURN(sps->log2_min_luma_transform_block_size_minus2 <
                 min_cb_log2_size_y - 2);
  int min_tb_log2_size_y = sps->log2_min_luma_transform_block_size_minus2 + 2;
  READ_UE_OR_RETURN(&sps->log2_diff_max_min_luma_transform_block_size);
  TRUE_OR_RETURN(sps->log2_diff_max_min_luma_transform_block_size <=
                 std::min(sps->ctb_log2_size_y, 5) - min_tb_log2_size_y);
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
    IN_RANGE_OR_RETURN(sps->log2_min_pcm_luma_coding_block_size_minus3, 0, 2);
    int log2_min_ipcm_cb_size_y =
        sps->log2_min_pcm_luma_coding_block_size_minus3 + 3;
    IN_RANGE_OR_RETURN(log2_min_ipcm_cb_size_y, std::min(min_cb_log2_size_y, 5),
                       std::min(sps->ctb_log2_size_y, 5));
    READ_UE_OR_RETURN(&sps->log2_diff_max_min_pcm_luma_coding_block_size);
    TRUE_OR_RETURN(sps->log2_diff_max_min_pcm_luma_coding_block_size <=
                   std::min(sps->ctb_log2_size_y, 5) - log2_min_ipcm_cb_size_y);
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

H265Parser::Result H265Parser::ParsePPS(const H265NALU& nalu, int* pps_id) {
  // 7.4.3.3
  DVLOG(4) << "Parsing PPS";
  Result res = kOk;

  DCHECK(pps_id);
  *pps_id = -1;
  std::unique_ptr<H265PPS> pps = std::make_unique<H265PPS>();

  pps->temporal_id = nalu.nuh_temporal_id_plus1 - 1;

  // Set these defaults if they are not present here.
  pps->loop_filter_across_tiles_enabled_flag = 1;

  // 7.4.3.3.1
  READ_UE_OR_RETURN(&pps->pps_pic_parameter_set_id);
  IN_RANGE_OR_RETURN(pps->pps_pic_parameter_set_id, 0, 63);
  READ_UE_OR_RETURN(&pps->pps_seq_parameter_set_id);
  IN_RANGE_OR_RETURN(pps->pps_seq_parameter_set_id, 0, 15);
  const H265SPS* sps = GetSPS(pps->pps_seq_parameter_set_id);
  if (!sps) {
    return kMissingParameterSet;
  }
  READ_BOOL_OR_RETURN(&pps->dependent_slice_segments_enabled_flag);
  READ_BOOL_OR_RETURN(&pps->output_flag_present_flag);
  READ_BITS_OR_RETURN(3, &pps->num_extra_slice_header_bits);
  READ_BOOL_OR_RETURN(&pps->sign_data_hiding_enabled_flag);
  READ_BOOL_OR_RETURN(&pps->cabac_init_present_flag);
  READ_UE_OR_RETURN(&pps->num_ref_idx_l0_default_active_minus1);
  IN_RANGE_OR_RETURN(pps->num_ref_idx_l0_default_active_minus1, 0,
                     kMaxRefIdxActive - 1);
  READ_UE_OR_RETURN(&pps->num_ref_idx_l1_default_active_minus1);
  IN_RANGE_OR_RETURN(pps->num_ref_idx_l1_default_active_minus1, 0,
                     kMaxRefIdxActive - 1);
  READ_SE_OR_RETURN(&pps->init_qp_minus26);
  pps->qp_bd_offset_y = 6 * sps->bit_depth_luma_minus8;
  IN_RANGE_OR_RETURN(pps->init_qp_minus26, -(26 + pps->qp_bd_offset_y), 25);
  READ_BOOL_OR_RETURN(&pps->constrained_intra_pred_flag);
  READ_BOOL_OR_RETURN(&pps->transform_skip_enabled_flag);
  READ_BOOL_OR_RETURN(&pps->cu_qp_delta_enabled_flag);
  if (pps->cu_qp_delta_enabled_flag) {
    READ_UE_OR_RETURN(&pps->diff_cu_qp_delta_depth);
    IN_RANGE_OR_RETURN(pps->diff_cu_qp_delta_depth, 0,
                       sps->log2_diff_max_min_luma_coding_block_size);
  }
  READ_SE_OR_RETURN(&pps->pps_cb_qp_offset);
  IN_RANGE_OR_RETURN(pps->pps_cb_qp_offset, -12, 12);
  READ_SE_OR_RETURN(&pps->pps_cr_qp_offset);
  IN_RANGE_OR_RETURN(pps->pps_cr_qp_offset, -12, 12);
  READ_BOOL_OR_RETURN(&pps->pps_slice_chroma_qp_offsets_present_flag);
  READ_BOOL_OR_RETURN(&pps->weighted_pred_flag);
  READ_BOOL_OR_RETURN(&pps->weighted_bipred_flag);
  READ_BOOL_OR_RETURN(&pps->transquant_bypass_enabled_flag);
  READ_BOOL_OR_RETURN(&pps->tiles_enabled_flag);
  READ_BOOL_OR_RETURN(&pps->entropy_coding_sync_enabled_flag);
  if (pps->tiles_enabled_flag) {
    READ_UE_OR_RETURN(&pps->num_tile_columns_minus1);
    IN_RANGE_OR_RETURN(pps->num_tile_columns_minus1, 0,
                       sps->pic_width_in_ctbs_y - 1);
    TRUE_OR_RETURN(pps->num_tile_columns_minus1 <
                   H265PPS::kMaxNumTileColumnWidth);
    READ_UE_OR_RETURN(&pps->num_tile_rows_minus1);
    IN_RANGE_OR_RETURN(pps->num_tile_rows_minus1, 0,
                       sps->pic_height_in_ctbs_y - 1);
    TRUE_OR_RETURN((pps->num_tile_columns_minus1 != 0) ||
                   (pps->num_tile_rows_minus1 != 0));
    TRUE_OR_RETURN(pps->num_tile_rows_minus1 < H265PPS::kMaxNumTileRowHeight);
    READ_BOOL_OR_RETURN(&pps->uniform_spacing_flag);
    if (!pps->uniform_spacing_flag) {
      pps->column_width_minus1[pps->num_tile_columns_minus1] =
          sps->pic_width_in_ctbs_y - 1;
      for (int i = 0; i < pps->num_tile_columns_minus1; ++i) {
        READ_UE_OR_RETURN(&pps->column_width_minus1[i]);
        IN_RANGE_OR_RETURN(
            pps->column_width_minus1[i], 0,
            pps->column_width_minus1[pps->num_tile_columns_minus1] - 1);
        pps->column_width_minus1[pps->num_tile_columns_minus1] -=
            pps->column_width_minus1[i] + 1;
      }
      pps->row_height_minus1[pps->num_tile_rows_minus1] =
          sps->pic_height_in_ctbs_y - 1;
      for (int i = 0; i < pps->num_tile_rows_minus1; ++i) {
        READ_UE_OR_RETURN(&pps->row_height_minus1[i]);
        IN_RANGE_OR_RETURN(
            pps->row_height_minus1[i], 0,
            pps->row_height_minus1[pps->num_tile_rows_minus1] - 1);
        pps->row_height_minus1[pps->num_tile_rows_minus1] -=
            pps->row_height_minus1[i] + 1;
      }
    }
    READ_BOOL_OR_RETURN(&pps->loop_filter_across_tiles_enabled_flag);
  }
  READ_BOOL_OR_RETURN(&pps->pps_loop_filter_across_slices_enabled_flag);
  bool deblocking_filter_control_present_flag;
  READ_BOOL_OR_RETURN(&deblocking_filter_control_present_flag);
  if (deblocking_filter_control_present_flag) {
    READ_BOOL_OR_RETURN(&pps->deblocking_filter_override_enabled_flag);
    READ_BOOL_OR_RETURN(&pps->pps_deblocking_filter_disabled_flag);
    if (!pps->pps_deblocking_filter_disabled_flag) {
      READ_SE_OR_RETURN(&pps->pps_beta_offset_div2);
      IN_RANGE_OR_RETURN(pps->pps_beta_offset_div2, -6, 6);
      READ_SE_OR_RETURN(&pps->pps_tc_offset_div2);
      IN_RANGE_OR_RETURN(pps->pps_tc_offset_div2, -6, 6);
    }
  }
  READ_BOOL_OR_RETURN(&pps->pps_scaling_list_data_present_flag);
  if (pps->pps_scaling_list_data_present_flag) {
    res = ParseScalingListData(&pps->scaling_list_data);
    if (res != kOk)
      return res;
  }
  READ_BOOL_OR_RETURN(&pps->lists_modification_present_flag);
  READ_UE_OR_RETURN(&pps->log2_parallel_merge_level_minus2);
  IN_RANGE_OR_RETURN(pps->log2_parallel_merge_level_minus2, 0,
                     sps->ctb_log2_size_y - 2);
  READ_BOOL_OR_RETURN(&pps->slice_segment_header_extension_present_flag);
  bool pps_extension_present_flag;
  READ_BOOL_OR_RETURN(&pps_extension_present_flag);
  bool pps_range_extension_flag = false;
  bool pps_multilayer_extension_flag = false;
  bool pps_3d_extension_flag = false;
  bool pps_scc_extension_flag = false;
  if (pps_extension_present_flag) {
    READ_BOOL_OR_RETURN(&pps_range_extension_flag);
    READ_BOOL_OR_RETURN(&pps_multilayer_extension_flag);
    READ_BOOL_OR_RETURN(&pps_3d_extension_flag);
    READ_BOOL_OR_RETURN(&pps_scc_extension_flag);
    SKIP_BITS_OR_RETURN(4);  // pps_extension_4bits
  }

  if (pps_range_extension_flag) {
    DVLOG(1) << "HEVC range extension not supported";
    return kInvalidStream;
  }
  if (pps_multilayer_extension_flag) {
    DVLOG(1) << "HEVC multilayer extension not supported";
    return kInvalidStream;
  }
  if (pps_3d_extension_flag) {
    DVLOG(1) << "HEVC 3D extension not supported";
    return kInvalidStream;
  }
  if (pps_scc_extension_flag) {
    DVLOG(1) << "HEVC SCC extension not supported";
    return kInvalidStream;
  }

  // If a PPS with the same id already exists, replace it.
  *pps_id = pps->pps_pic_parameter_set_id;
  active_pps_[*pps_id] = std::move(pps);

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

const H265PPS* H265Parser::GetPPS(int pps_id) const {
  auto it = active_pps_.find(pps_id);
  if (it == active_pps_.end()) {
    DVLOG(1) << "Requested a nonexistent PPS id " << pps_id;
    return nullptr;
  }

  return it->second.get();
}

H265Parser::Result H265Parser::ParseSliceHeader(const H265NALU& nalu,
                                                H265SliceHeader* shdr,
                                                H265SliceHeader* prior_shdr) {
  // 7.4.7 Slice segment header
  DVLOG(4) << "Parsing slice header";
  Result res = kOk;
  const H265SPS* sps;
  const H265PPS* pps;

  DCHECK(shdr);
  shdr->nal_unit_type = nalu.nal_unit_type;
  shdr->nalu_data = nalu.data;
  shdr->nalu_size = nalu.size;

  READ_BOOL_OR_RETURN(&shdr->first_slice_segment_in_pic_flag);
  shdr->irap_pic = (shdr->nal_unit_type >= H265NALU::BLA_W_LP &&
                    shdr->nal_unit_type <= H265NALU::RSV_IRAP_VCL23);
  if (shdr->irap_pic) {
    READ_BOOL_OR_RETURN(&shdr->no_output_of_prior_pics_flag);
  }
  READ_UE_OR_RETURN(&shdr->slice_pic_parameter_set_id);
  IN_RANGE_OR_RETURN(shdr->slice_pic_parameter_set_id, 0, 63);
  pps = GetPPS(shdr->slice_pic_parameter_set_id);
  if (!pps) {
    return kMissingParameterSet;
  }
  sps = GetSPS(pps->pps_seq_parameter_set_id);
  DCHECK(sps);  // We already validated this when we parsed the PPS.

  if (!shdr->first_slice_segment_in_pic_flag) {
    if (pps->dependent_slice_segments_enabled_flag)
      READ_BOOL_OR_RETURN(&shdr->dependent_slice_segment_flag);
    READ_BITS_OR_RETURN(base::bits::Log2Ceiling(sps->pic_size_in_ctbs_y),
                        &shdr->slice_segment_address);
    IN_RANGE_OR_RETURN(shdr->slice_segment_address, 0,
                       sps->pic_size_in_ctbs_y - 1);
  }
  if (shdr->dependent_slice_segment_flag) {
    if (!prior_shdr) {
      DVLOG(1) << "Cannot parse dependent slice w/out prior slice data";
      return kInvalidStream;
    }
    // Copy everything in the structure starting at |slice_type| going forward.
    // This is copying the dependent slice data that we do not parse below.
    size_t skip_amount = offsetof(H265SliceHeader, slice_type);
    memcpy(reinterpret_cast<uint8_t*>(shdr) + skip_amount,
           reinterpret_cast<uint8_t*>(prior_shdr) + skip_amount,
           sizeof(H265SliceHeader) - skip_amount);
  } else {
    // Set these defaults if they are not present here.
    shdr->pic_output_flag = 1;
    shdr->num_ref_idx_l0_active_minus1 =
        pps->num_ref_idx_l0_default_active_minus1;
    shdr->num_ref_idx_l1_active_minus1 =
        pps->num_ref_idx_l1_default_active_minus1;
    shdr->collocated_from_l0_flag = 1;
    shdr->slice_deblocking_filter_disabled_flag =
        pps->pps_deblocking_filter_disabled_flag;
    shdr->slice_beta_offset_div2 = pps->pps_beta_offset_div2;
    shdr->slice_tc_offset_div2 = pps->pps_tc_offset_div2;
    shdr->slice_loop_filter_across_slices_enabled_flag =
        pps->pps_loop_filter_across_slices_enabled_flag;
    shdr->curr_rps_idx = sps->num_short_term_ref_pic_sets;

    // slice_reserved_flag
    SKIP_BITS_OR_RETURN(pps->num_extra_slice_header_bits);
    READ_UE_OR_RETURN(&shdr->slice_type);
    if ((shdr->irap_pic ||
         sps->sps_max_dec_pic_buffering_minus1[pps->temporal_id] == 0) &&
        nalu.nuh_layer_id == 0) {
      TRUE_OR_RETURN(shdr->slice_type == 2);
    }
    if (pps->output_flag_present_flag)
      READ_BOOL_OR_RETURN(&shdr->pic_output_flag);
    if (sps->separate_colour_plane_flag) {
      READ_BITS_OR_RETURN(2, &shdr->colour_plane_id);
      IN_RANGE_OR_RETURN(shdr->colour_plane_id, 0, 2);
    }
    if (shdr->nal_unit_type != H265NALU::IDR_W_RADL &&
        shdr->nal_unit_type != H265NALU::IDR_N_LP) {
      READ_BITS_OR_RETURN(sps->log2_max_pic_order_cnt_lsb_minus4 + 4,
                          &shdr->slice_pic_order_cnt_lsb);
      IN_RANGE_OR_RETURN(shdr->slice_pic_order_cnt_lsb, 0,
                         sps->max_pic_order_cnt_lsb - 1);
      READ_BOOL_OR_RETURN(&shdr->short_term_ref_pic_set_sps_flag);
      if (!shdr->short_term_ref_pic_set_sps_flag) {
        off_t bits_left_prior = br_.NumBitsLeft();
        size_t num_epb_prior = br_.NumEmulationPreventionBytesRead();
        res = ParseStRefPicSet(sps->num_short_term_ref_pic_sets, *sps,
                               &shdr->st_ref_pic_set);
        if (res != kOk)
          return res;
        shdr->st_rps_bits =
            (bits_left_prior - br_.NumBitsLeft()) -
            8 * (br_.NumEmulationPreventionBytesRead() - num_epb_prior);
      } else if (sps->num_short_term_ref_pic_sets > 1) {
        READ_BITS_OR_RETURN(
            base::bits::Log2Ceiling(sps->num_short_term_ref_pic_sets),
            &shdr->short_term_ref_pic_set_idx);
        IN_RANGE_OR_RETURN(shdr->short_term_ref_pic_set_idx, 0,
                           sps->num_short_term_ref_pic_sets - 1);
      }

      if (shdr->short_term_ref_pic_set_sps_flag)
        shdr->curr_rps_idx = shdr->short_term_ref_pic_set_idx;

      if (sps->long_term_ref_pics_present_flag) {
        if (sps->num_long_term_ref_pics_sps > 0) {
          READ_UE_OR_RETURN(&shdr->num_long_term_sps);
          IN_RANGE_OR_RETURN(shdr->num_long_term_sps, 0,
                             sps->num_long_term_ref_pics_sps);
        }
        READ_UE_OR_RETURN(&shdr->num_long_term_pics);
        if (nalu.nuh_layer_id == 0) {
          TRUE_OR_RETURN(
              shdr->num_long_term_pics <=
              (sps->sps_max_dec_pic_buffering_minus1[pps->temporal_id] -
               shdr->GetStRefPicSet(sps).num_negative_pics -
               shdr->GetStRefPicSet(sps).num_positive_pics -
               shdr->num_long_term_sps));
        }
        IN_RANGE_OR_RETURN(shdr->num_long_term_pics, 0,
                           kMaxLongTermRefPicSets - shdr->num_long_term_sps);
        for (int i = 0; i < shdr->num_long_term_sps + shdr->num_long_term_pics;
             ++i) {
          if (i < shdr->num_long_term_sps) {
            int lt_idx_sps = 0;
            if (sps->num_long_term_ref_pics_sps > 1) {
              READ_BITS_OR_RETURN(
                  base::bits::Log2Ceiling(sps->num_long_term_ref_pics_sps),
                  &lt_idx_sps);
              IN_RANGE_OR_RETURN(lt_idx_sps, 0,
                                 sps->num_long_term_ref_pics_sps - 1);
            }
            shdr->poc_lsb_lt[i] = sps->lt_ref_pic_poc_lsb_sps[lt_idx_sps];
            shdr->used_by_curr_pic_lt[i] =
                sps->used_by_curr_pic_lt_sps_flag[lt_idx_sps];
          } else {
            READ_BITS_OR_RETURN(sps->log2_max_pic_order_cnt_lsb_minus4 + 4,
                                &shdr->poc_lsb_lt[i]);
            READ_BOOL_OR_RETURN(&shdr->used_by_curr_pic_lt[i]);
          }
          READ_BOOL_OR_RETURN(&shdr->delta_poc_msb_present_flag[i]);
          if (shdr->delta_poc_msb_present_flag[i]) {
            READ_UE_OR_RETURN(&shdr->delta_poc_msb_cycle_lt[i]);
            IN_RANGE_OR_RETURN(
                shdr->delta_poc_msb_cycle_lt[i], 0,
                std::pow(2, 32 - sps->log2_max_pic_order_cnt_lsb_minus4 - 4));
            // Equation 7-52.
            if (i != 0 && i != shdr->num_long_term_sps) {
              shdr->delta_poc_msb_cycle_lt[i] =
                  shdr->delta_poc_msb_cycle_lt[i] +
                  shdr->delta_poc_msb_cycle_lt[i - 1];
            }
          }
        }
      }
      if (sps->sps_temporal_mvp_enabled_flag)
        READ_BOOL_OR_RETURN(&shdr->slice_temporal_mvp_enabled_flag);
    }
    if (sps->sample_adaptive_offset_enabled_flag) {
      READ_BOOL_OR_RETURN(&shdr->slice_sao_luma_flag);
      if (sps->chroma_array_type != 0)
        READ_BOOL_OR_RETURN(&shdr->slice_sao_chroma_flag);
    }
    if (shdr->IsPSlice() || shdr->IsBSlice()) {
      READ_BOOL_OR_RETURN(&shdr->num_ref_idx_active_override_flag);
      if (shdr->num_ref_idx_active_override_flag) {
        READ_UE_OR_RETURN(&shdr->num_ref_idx_l0_active_minus1);
        IN_RANGE_OR_RETURN(shdr->num_ref_idx_l0_active_minus1, 0,
                           kMaxRefIdxActive - 1);
        if (shdr->IsBSlice()) {
          READ_UE_OR_RETURN(&shdr->num_ref_idx_l1_active_minus1);
          IN_RANGE_OR_RETURN(shdr->num_ref_idx_l1_active_minus1, 0,
                             kMaxRefIdxActive - 1);
        }
      }

      shdr->num_pic_total_curr = 0;
      const H265StRefPicSet& st_ref_pic = shdr->GetStRefPicSet(sps);
      for (int i = 0; i < st_ref_pic.num_negative_pics; ++i) {
        if (st_ref_pic.used_by_curr_pic_s0[i])
          shdr->num_pic_total_curr++;
      }
      for (int i = 0; i < st_ref_pic.num_positive_pics; ++i) {
        if (st_ref_pic.used_by_curr_pic_s1[i])
          shdr->num_pic_total_curr++;
      }
      for (int i = 0; i < shdr->num_long_term_sps + shdr->num_long_term_pics;
           ++i) {
        if (shdr->used_by_curr_pic_lt[i])
          shdr->num_pic_total_curr++;
      }

      TRUE_OR_RETURN(shdr->num_pic_total_curr);
      if (pps->lists_modification_present_flag &&
          shdr->num_pic_total_curr > 1) {
        res = ParseRefPicListsModifications(*shdr,
                                            &shdr->ref_pic_lists_modification);
        if (res != kOk)
          return res;
      }
      if (shdr->IsBSlice())
        READ_BOOL_OR_RETURN(&shdr->mvd_l1_zero_flag);
      if (pps->cabac_init_present_flag)
        READ_BOOL_OR_RETURN(&shdr->cabac_init_flag);
      if (shdr->slice_temporal_mvp_enabled_flag) {
        if (shdr->IsBSlice())
          READ_BOOL_OR_RETURN(&shdr->collocated_from_l0_flag);
        if ((shdr->collocated_from_l0_flag &&
             shdr->num_ref_idx_l0_active_minus1 > 0) ||
            (!shdr->collocated_from_l0_flag &&
             shdr->num_ref_idx_l1_active_minus1 > 0)) {
          READ_UE_OR_RETURN(&shdr->collocated_ref_idx);
          if ((shdr->IsPSlice() || shdr->IsBSlice()) &&
              shdr->collocated_from_l0_flag) {
            IN_RANGE_OR_RETURN(shdr->collocated_ref_idx, 0,
                               shdr->num_ref_idx_l0_active_minus1);
          }
          if (shdr->IsBSlice() && !shdr->collocated_from_l0_flag) {
            IN_RANGE_OR_RETURN(shdr->collocated_ref_idx, 0,
                               shdr->num_ref_idx_l1_active_minus1);
          }
        }
      }

      if ((pps->weighted_pred_flag && shdr->IsPSlice()) ||
          (pps->weighted_bipred_flag && shdr->IsBSlice())) {
        res = ParsePredWeightTable(*sps, *shdr, &shdr->pred_weight_table);
        if (res != kOk)
          return res;
      }
      READ_UE_OR_RETURN(&shdr->five_minus_max_num_merge_cand);
      IN_RANGE_OR_RETURN(5 - shdr->five_minus_max_num_merge_cand, 1, 5);
    }
    READ_SE_OR_RETURN(&shdr->slice_qp_delta);
    IN_RANGE_OR_RETURN(26 + pps->init_qp_minus26 + shdr->slice_qp_delta,
                       -pps->qp_bd_offset_y, 51);

    if (pps->pps_slice_chroma_qp_offsets_present_flag) {
      READ_SE_OR_RETURN(&shdr->slice_cb_qp_offset);
      IN_RANGE_OR_RETURN(shdr->slice_cb_qp_offset, -12, 12);
      IN_RANGE_OR_RETURN(pps->pps_cb_qp_offset + shdr->slice_cb_qp_offset, -12,
                         12);
      READ_SE_OR_RETURN(&shdr->slice_cr_qp_offset);
      IN_RANGE_OR_RETURN(shdr->slice_cr_qp_offset, -12, 12);
      IN_RANGE_OR_RETURN(pps->pps_cr_qp_offset + shdr->slice_cr_qp_offset, -12,
                         12);
    }

    // pps_slice_act_qp_offsets_present_flag is zero, we don't support SCC ext.

    // chroma_qp_offset_list_enabled_flag is zero, we don't support range ext.

    bool deblocking_filter_override_flag = false;
    if (pps->deblocking_filter_override_enabled_flag)
      READ_BOOL_OR_RETURN(&deblocking_filter_override_flag);
    if (deblocking_filter_override_flag) {
      READ_BOOL_OR_RETURN(&shdr->slice_deblocking_filter_disabled_flag);
      if (!shdr->slice_deblocking_filter_disabled_flag) {
        READ_SE_OR_RETURN(&shdr->slice_beta_offset_div2);
        IN_RANGE_OR_RETURN(shdr->slice_beta_offset_div2, -6, 6);
        READ_SE_OR_RETURN(&shdr->slice_tc_offset_div2);
        IN_RANGE_OR_RETURN(shdr->slice_tc_offset_div2, -6, 6);
      }
    }
    if (pps->pps_loop_filter_across_slices_enabled_flag &&
        (shdr->slice_sao_luma_flag || shdr->slice_sao_chroma_flag ||
         !shdr->slice_deblocking_filter_disabled_flag)) {
      READ_BOOL_OR_RETURN(&shdr->slice_loop_filter_across_slices_enabled_flag);
    }
  }

  if (pps->tiles_enabled_flag || pps->entropy_coding_sync_enabled_flag) {
    int num_entry_point_offsets;
    READ_UE_OR_RETURN(&num_entry_point_offsets);
    if (!pps->tiles_enabled_flag) {
      IN_RANGE_OR_RETURN(num_entry_point_offsets, 0,
                         sps->pic_height_in_ctbs_y - 1);
    } else if (!pps->entropy_coding_sync_enabled_flag) {
      IN_RANGE_OR_RETURN(
          num_entry_point_offsets, 0,
          (pps->num_tile_columns_minus1 + 1) * (pps->num_tile_rows_minus1 + 1) -
              1);
    } else {  // both are true
      IN_RANGE_OR_RETURN(
          num_entry_point_offsets, 0,
          (pps->num_tile_columns_minus1 + 1) * sps->pic_height_in_ctbs_y - 1);
    }
    if (num_entry_point_offsets > 0) {
      int offset_len_minus1;
      READ_UE_OR_RETURN(&offset_len_minus1);
      IN_RANGE_OR_RETURN(offset_len_minus1, 0, 31);
      SKIP_BITS_OR_RETURN(num_entry_point_offsets * (offset_len_minus1 + 1));
    }
  }

  if (pps->slice_segment_header_extension_present_flag) {
    int slice_segment_header_extension_length;
    READ_UE_OR_RETURN(&slice_segment_header_extension_length);
    IN_RANGE_OR_RETURN(slice_segment_header_extension_length, 0, 256);
    SKIP_BITS_OR_RETURN(slice_segment_header_extension_length * 8);
  }

  // byte_alignment()
  SKIP_BITS_OR_RETURN(1);  // alignment bit
  int bits_left_to_align = br_.NumBitsLeft() % 8;
  if (bits_left_to_align)
    SKIP_BITS_OR_RETURN(bits_left_to_align);

  shdr->header_emulation_prevention_bytes =
      br_.NumEmulationPreventionBytesRead();
  shdr->header_size = shdr->nalu_size -
                      shdr->header_emulation_prevention_bytes -
                      br_.NumBitsLeft() / 8;
  return res;
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

std::vector<SubsampleEntry> H265Parser::GetCurrentSubsamples() {
  DCHECK_EQ(previous_nalu_range_.size(), 1u)
      << "This should only be called after a "
         "successful call to AdvanceToNextNalu()";

  auto intersection = encrypted_ranges_.IntersectionWith(previous_nalu_range_);
  return EncryptedRangesToSubsampleEntry(
      previous_nalu_range_.start(0), previous_nalu_range_.end(0), intersection);
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
    IN_RANGE_OR_RETURN(abs_delta_rps_minus1, 0, 0x7FFF);
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
    IN_RANGE_OR_RETURN(
        st_ref_pic_set->num_negative_pics, 0,
        sps.sps_max_dec_pic_buffering_minus1[sps.sps_max_sub_layers_minus1]);
    IN_RANGE_OR_RETURN(
        st_ref_pic_set->num_positive_pics, 0,
        sps.sps_max_dec_pic_buffering_minus1[sps.sps_max_sub_layers_minus1] -
            st_ref_pic_set->num_negative_pics);
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
      IN_RANGE_OR_RETURN(delta_poc_s0_minus1, 0, 0x7FFF);
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
      IN_RANGE_OR_RETURN(delta_poc_s1_minus1, 0, 0x7FFF);
      if (i == 0) {
        st_ref_pic_set->delta_poc_s1[i] = delta_poc_s1_minus1 + 1;
      } else {
        st_ref_pic_set->delta_poc_s1[i] =
            st_ref_pic_set->delta_poc_s1[i - 1] + delta_poc_s1_minus1 + 1;
      }
      READ_BOOL_OR_RETURN(&st_ref_pic_set->used_by_curr_pic_s1[i]);
    }
  }
  // Calculate num_delta_pocs.
  st_ref_pic_set->num_delta_pocs =
      st_ref_pic_set->num_negative_pics + st_ref_pic_set->num_positive_pics;
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
      IN_RANGE_OR_RETURN(cpb_cnt, 0, 31);
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

H265Parser::Result H265Parser::ParseRefPicListsModifications(
    const H265SliceHeader& shdr,
    H265RefPicListsModifications* rpl_mod) {
  READ_BOOL_OR_RETURN(&rpl_mod->ref_pic_list_modification_flag_l0);
  if (rpl_mod->ref_pic_list_modification_flag_l0) {
    for (int i = 0; i <= shdr.num_ref_idx_l0_active_minus1; ++i) {
      READ_BITS_OR_RETURN(base::bits::Log2Ceiling(shdr.num_pic_total_curr),
                          &rpl_mod->list_entry_l0[i]);
      IN_RANGE_OR_RETURN(rpl_mod->list_entry_l0[i], 0,
                         shdr.num_pic_total_curr - 1);
    }
  }
  if (shdr.IsBSlice()) {
    READ_BOOL_OR_RETURN(&rpl_mod->ref_pic_list_modification_flag_l1);
    if (rpl_mod->ref_pic_list_modification_flag_l1) {
      for (int i = 0; i <= shdr.num_ref_idx_l1_active_minus1; ++i) {
        READ_BITS_OR_RETURN(base::bits::Log2Ceiling(shdr.num_pic_total_curr),
                            &rpl_mod->list_entry_l1[i]);
        IN_RANGE_OR_RETURN(rpl_mod->list_entry_l1[i], 0,
                           shdr.num_pic_total_curr - 1);
      }
    }
  }
  return kOk;
}

H265Parser::Result H265Parser::ParsePredWeightTable(
    const H265SPS& sps,
    const H265SliceHeader& shdr,
    H265PredWeightTable* pred_weight_table) {
  // 7.4.6.3 Weighted prediction parameters semantics
  READ_UE_OR_RETURN(&pred_weight_table->luma_log2_weight_denom);
  IN_RANGE_OR_RETURN(pred_weight_table->luma_log2_weight_denom, 0, 7);
  if (sps.chroma_array_type) {
    READ_SE_OR_RETURN(&pred_weight_table->delta_chroma_log2_weight_denom);
    pred_weight_table->chroma_log2_weight_denom =
        pred_weight_table->delta_chroma_log2_weight_denom +
        pred_weight_table->luma_log2_weight_denom;
    IN_RANGE_OR_RETURN(pred_weight_table->chroma_log2_weight_denom, 0, 7);
  }
  bool luma_weight_flag[kMaxRefIdxActive];
  bool chroma_weight_flag[kMaxRefIdxActive];
  memset(chroma_weight_flag, 0, sizeof(chroma_weight_flag));
  for (int i = 0; i <= shdr.num_ref_idx_l0_active_minus1; ++i) {
    READ_BOOL_OR_RETURN(&luma_weight_flag[i]);
  }
  if (sps.chroma_array_type) {
    for (int i = 0; i <= shdr.num_ref_idx_l0_active_minus1; ++i) {
      READ_BOOL_OR_RETURN(&chroma_weight_flag[i]);
    }
  }
  int sum_weight_l0_flags = 0;
  for (int i = 0; i <= shdr.num_ref_idx_l0_active_minus1; ++i) {
    if (luma_weight_flag[i]) {
      sum_weight_l0_flags++;
      READ_SE_OR_RETURN(&pred_weight_table->delta_luma_weight_l0[i]);
      IN_RANGE_OR_RETURN(pred_weight_table->delta_luma_weight_l0[i], -128, 127);
      READ_SE_OR_RETURN(&pred_weight_table->luma_offset_l0[i]);
      IN_RANGE_OR_RETURN(pred_weight_table->luma_offset_l0[i],
                         -sps.wp_offset_half_range_y,
                         sps.wp_offset_half_range_y - 1);
    }
    if (chroma_weight_flag[i]) {
      sum_weight_l0_flags += 2;
      for (int j = 0; j < 2; ++j) {
        READ_SE_OR_RETURN(&pred_weight_table->delta_chroma_weight_l0[i][j]);
        IN_RANGE_OR_RETURN(pred_weight_table->delta_chroma_weight_l0[i][j],
                           -128, 127);
        READ_SE_OR_RETURN(&pred_weight_table->delta_chroma_offset_l0[i][j]);
        IN_RANGE_OR_RETURN(pred_weight_table->delta_chroma_offset_l0[i][j],
                           -4 * sps.wp_offset_half_range_c,
                           4 * sps.wp_offset_half_range_c - 1);
      }
    }
  }
  if (shdr.IsPSlice())
    TRUE_OR_RETURN(sum_weight_l0_flags <= 24);
  if (shdr.IsBSlice()) {
    memset(chroma_weight_flag, 0, sizeof(chroma_weight_flag));
    int sum_weight_l1_flags = 0;
    for (int i = 0; i <= shdr.num_ref_idx_l1_active_minus1; ++i) {
      READ_BOOL_OR_RETURN(&luma_weight_flag[i]);
    }
    if (sps.chroma_array_type) {
      for (int i = 0; i <= shdr.num_ref_idx_l1_active_minus1; ++i) {
        READ_BOOL_OR_RETURN(&chroma_weight_flag[i]);
      }
    }
    for (int i = 0; i <= shdr.num_ref_idx_l1_active_minus1; ++i) {
      if (luma_weight_flag[i]) {
        sum_weight_l1_flags++;
        READ_SE_OR_RETURN(&pred_weight_table->delta_luma_weight_l1[i]);
        IN_RANGE_OR_RETURN(pred_weight_table->delta_luma_weight_l1[i], -128,
                           127);
        READ_SE_OR_RETURN(&pred_weight_table->luma_offset_l1[i]);
        IN_RANGE_OR_RETURN(pred_weight_table->luma_offset_l1[i],
                           -sps.wp_offset_half_range_y,
                           sps.wp_offset_half_range_y - 1);
      }
      if (chroma_weight_flag[i]) {
        sum_weight_l1_flags += 2;
        for (int j = 0; j < 2; ++j) {
          READ_SE_OR_RETURN(&pred_weight_table->delta_chroma_weight_l1[i][j]);
          IN_RANGE_OR_RETURN(pred_weight_table->delta_chroma_weight_l1[i][j],
                             -128, 127);
          READ_SE_OR_RETURN(&pred_weight_table->delta_chroma_offset_l1[i][j]);
          IN_RANGE_OR_RETURN(pred_weight_table->delta_chroma_offset_l1[i][j],
                             -4 * sps.wp_offset_half_range_c,
                             4 * sps.wp_offset_half_range_c - 1);
        }
      }
    }
    TRUE_OR_RETURN(sum_weight_l0_flags + sum_weight_l1_flags <= 24);
  }

  return kOk;
}

}  // namespace media
