// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/test/h264_decoder.h"

#include <linux/v4l2-controls.h>
#include <linux/videodev2.h>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "media/gpu/macros.h"
#include "ui/gfx/geometry/rect.h"

namespace media {

namespace v4l2_test {

namespace {
constexpr uint32_t kDriverCodecFourcc = V4L2_PIX_FMT_H264_SLICE;

constexpr uint8_t zigzag_4x4[] = {0, 1,  4,  8,  5, 2,  3,  6,
                                  9, 12, 13, 10, 7, 11, 14, 15};

constexpr uint8_t zigzag_8x8[] = {
    0,  1,  8,  16, 9,  2,  3,  10, 17, 24, 32, 25, 18, 11, 4,  5,
    12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6,  7,  14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63};

// TODO(b/234752983): Set number of buffers in CAPTURE queue dynamically.
// |18| is the minimum number of buffers in the CAPTURE queue required to
// successfully decode all ITUT baseline and main bitstreams.
constexpr uint32_t kNumberOfBuffersInCaptureQueue = 18;

// Comparator struct used for H.264 picture reordering
struct H264PicOrderCompare {
  bool operator()(const H264SliceMetadata* a,
                  const H264SliceMetadata* b) const {
    return a->pic_order_cnt < b->pic_order_cnt;
  }
};

// Extracts bit depth to |bit_depth| from the SPS. Returns true if is able
// to successfully extract bit depth. Otherwise returns false.
bool ParseBitDepth(const H264SPS& sps, uint8_t& bit_depth) {
  // Spec 7.4.2.1.1
  if (sps.bit_depth_luma_minus8 != sps.bit_depth_chroma_minus8) {
    VLOGF(4) << "H264Decoder doesn't support different bit depths between luma"
             << "and chroma, bit_depth_luma_minus8="
             << sps.bit_depth_luma_minus8
             << ", bit_depth_chroma_minus8=" << sps.bit_depth_chroma_minus8;
    return false;
  }
  DCHECK_GE(sps.bit_depth_luma_minus8, 0);
  DCHECK_LE(sps.bit_depth_luma_minus8, 6);
  switch (sps.bit_depth_luma_minus8) {
    case 0:
      bit_depth = 8u;
      break;
    case 2:
      bit_depth = 10u;
      break;
    case 4:
      bit_depth = 12u;
      break;
    case 6:
      bit_depth = 14u;
      break;
    default:
      VLOGF(4) << "Invalid bit depth: "
               << base::checked_cast<int>(sps.bit_depth_luma_minus8 + 8);
      return false;
  }
  return true;
}

// Translates SPS into h264 sps ctrl structure.
v4l2_ctrl_h264_sps SetupSPSCtrl(const H264SPS* sps) {
  struct v4l2_ctrl_h264_sps v4l2_sps = {};

  v4l2_sps.profile_idc = sps->profile_idc;
  v4l2_sps.constraint_set_flags =
      (sps->constraint_set0_flag ? V4L2_H264_SPS_CONSTRAINT_SET0_FLAG : 0) |
      (sps->constraint_set1_flag ? V4L2_H264_SPS_CONSTRAINT_SET1_FLAG : 0) |
      (sps->constraint_set2_flag ? V4L2_H264_SPS_CONSTRAINT_SET2_FLAG : 0) |
      (sps->constraint_set3_flag ? V4L2_H264_SPS_CONSTRAINT_SET3_FLAG : 0) |
      (sps->constraint_set4_flag ? V4L2_H264_SPS_CONSTRAINT_SET4_FLAG : 0) |
      (sps->constraint_set5_flag ? V4L2_H264_SPS_CONSTRAINT_SET5_FLAG : 0);

  v4l2_sps.level_idc = sps->level_idc;
  v4l2_sps.seq_parameter_set_id = sps->seq_parameter_set_id;
  v4l2_sps.chroma_format_idc = sps->chroma_format_idc;
  v4l2_sps.bit_depth_luma_minus8 = sps->bit_depth_luma_minus8;
  v4l2_sps.bit_depth_chroma_minus8 = sps->bit_depth_chroma_minus8;
  v4l2_sps.log2_max_frame_num_minus4 = sps->log2_max_frame_num_minus4;
  v4l2_sps.pic_order_cnt_type = sps->pic_order_cnt_type;
  v4l2_sps.log2_max_pic_order_cnt_lsb_minus4 =
      sps->log2_max_pic_order_cnt_lsb_minus4;
  v4l2_sps.max_num_ref_frames = sps->max_num_ref_frames;
  v4l2_sps.num_ref_frames_in_pic_order_cnt_cycle =
      sps->num_ref_frames_in_pic_order_cnt_cycle;

  // Check that SPS offsets for ref frames size matches v4l2 sps.
  static_assert(std::extent<decltype(v4l2_sps.offset_for_ref_frame)>() ==
                    std::extent<decltype(sps->offset_for_ref_frame)>(),
                "SPS Offsets for ref frames size must match");
  for (size_t i = 0; i < std::size(v4l2_sps.offset_for_ref_frame); i++)
    v4l2_sps.offset_for_ref_frame[i] = sps->offset_for_ref_frame[i];

  v4l2_sps.offset_for_non_ref_pic = sps->offset_for_non_ref_pic;
  v4l2_sps.offset_for_top_to_bottom_field = sps->offset_for_top_to_bottom_field;
  v4l2_sps.pic_width_in_mbs_minus1 = sps->pic_width_in_mbs_minus1;
  v4l2_sps.pic_height_in_map_units_minus1 = sps->pic_height_in_map_units_minus1;

  v4l2_sps.flags = 0;
  if (sps->separate_colour_plane_flag)
    v4l2_sps.flags |= V4L2_H264_SPS_FLAG_SEPARATE_COLOUR_PLANE;
  if (sps->qpprime_y_zero_transform_bypass_flag)
    v4l2_sps.flags |= V4L2_H264_SPS_FLAG_QPPRIME_Y_ZERO_TRANSFORM_BYPASS;
  if (sps->delta_pic_order_always_zero_flag)
    v4l2_sps.flags |= V4L2_H264_SPS_FLAG_DELTA_PIC_ORDER_ALWAYS_ZERO;
  if (sps->gaps_in_frame_num_value_allowed_flag)
    v4l2_sps.flags |= V4L2_H264_SPS_FLAG_GAPS_IN_FRAME_NUM_VALUE_ALLOWED;
  if (sps->frame_mbs_only_flag)
    v4l2_sps.flags |= V4L2_H264_SPS_FLAG_FRAME_MBS_ONLY;
  if (sps->mb_adaptive_frame_field_flag)
    v4l2_sps.flags |= V4L2_H264_SPS_FLAG_MB_ADAPTIVE_FRAME_FIELD;
  if (sps->direct_8x8_inference_flag)
    v4l2_sps.flags |= V4L2_H264_SPS_FLAG_DIRECT_8X8_INFERENCE;

  return v4l2_sps;
}

// Translates PPS into h264 pps ctrl structure.
v4l2_ctrl_h264_pps SetupPPSCtrl(const H264PPS* pps) {
  struct v4l2_ctrl_h264_pps v4l2_pps = {};
  v4l2_pps.pic_parameter_set_id = pps->pic_parameter_set_id;
  v4l2_pps.seq_parameter_set_id = pps->seq_parameter_set_id;
  v4l2_pps.num_slice_groups_minus1 = pps->num_slice_groups_minus1;
  v4l2_pps.num_ref_idx_l0_default_active_minus1 =
      pps->num_ref_idx_l0_default_active_minus1;
  v4l2_pps.num_ref_idx_l1_default_active_minus1 =
      pps->num_ref_idx_l1_default_active_minus1;
  v4l2_pps.weighted_bipred_idc = pps->weighted_bipred_idc;
  v4l2_pps.pic_init_qp_minus26 = pps->pic_init_qp_minus26;
  v4l2_pps.pic_init_qs_minus26 = pps->pic_init_qs_minus26;
  v4l2_pps.chroma_qp_index_offset = pps->chroma_qp_index_offset;
  v4l2_pps.second_chroma_qp_index_offset = pps->second_chroma_qp_index_offset;

  v4l2_pps.flags = 0;
  if (pps->entropy_coding_mode_flag)
    v4l2_pps.flags |= V4L2_H264_PPS_FLAG_ENTROPY_CODING_MODE;
  if (pps->bottom_field_pic_order_in_frame_present_flag)
    v4l2_pps.flags |=
        V4L2_H264_PPS_FLAG_BOTTOM_FIELD_PIC_ORDER_IN_FRAME_PRESENT;
  if (pps->weighted_pred_flag)
    v4l2_pps.flags |= V4L2_H264_PPS_FLAG_WEIGHTED_PRED;
  if (pps->deblocking_filter_control_present_flag)
    v4l2_pps.flags |= V4L2_H264_PPS_FLAG_DEBLOCKING_FILTER_CONTROL_PRESENT;
  if (pps->constrained_intra_pred_flag)
    v4l2_pps.flags |= V4L2_H264_PPS_FLAG_CONSTRAINED_INTRA_PRED;
  if (pps->redundant_pic_cnt_present_flag)
    v4l2_pps.flags |= V4L2_H264_PPS_FLAG_REDUNDANT_PIC_CNT_PRESENT;
  if (pps->transform_8x8_mode_flag)
    v4l2_pps.flags |= V4L2_H264_PPS_FLAG_TRANSFORM_8X8_MODE;
  if (pps->pic_scaling_matrix_present_flag)
    v4l2_pps.flags |= V4L2_H264_PPS_FLAG_SCALING_MATRIX_PRESENT;

  return v4l2_pps;
}

// Sets up the h264 scaling matrix ctrl and checks against sps
// and pps scaling matrix sizes.
v4l2_ctrl_h264_scaling_matrix SetupScalingMatrix(const H264SPS* sps,
                                                 const H264PPS* pps) {
  struct v4l2_ctrl_h264_scaling_matrix matrix = {};

  // Makes sure that the size of the matrix scaling lists correspond
  // to the PPS scaling matrix sizes.
  static_assert(std::extent<decltype(matrix.scaling_list_4x4)>() <=
                        std::extent<decltype(pps->scaling_list4x4)>() &&
                    std::extent<decltype(matrix.scaling_list_4x4[0])>() <=
                        std::extent<decltype(pps->scaling_list4x4[0])>() &&
                    std::extent<decltype(matrix.scaling_list_8x8)>() <=
                        std::extent<decltype(pps->scaling_list8x8)>() &&
                    std::extent<decltype(matrix.scaling_list_8x8[0])>() <=
                        std::extent<decltype(pps->scaling_list8x8[0])>(),
                "PPS scaling_lists must be of correct size");

  // Makes sure that the size of the matrix scaling lists correspond
  // to the SPS scaling matrix sizes.
  static_assert(std::extent<decltype(matrix.scaling_list_4x4)>() <=
                        std::extent<decltype(sps->scaling_list4x4)>() &&
                    std::extent<decltype(matrix.scaling_list_4x4[0])>() <=
                        std::extent<decltype(sps->scaling_list4x4[0])>() &&
                    std::extent<decltype(matrix.scaling_list_8x8)>() <=
                        std::extent<decltype(sps->scaling_list8x8)>() &&
                    std::extent<decltype(matrix.scaling_list_8x8[0])>() <=
                        std::extent<decltype(sps->scaling_list8x8[0])>(),
                "SPS scaling_lists must be of correct size");

  const auto* scaling_list4x4 = &sps->scaling_list4x4[0];
  const auto* scaling_list8x8 = &sps->scaling_list8x8[0];
  if (pps->pic_scaling_matrix_present_flag) {
    scaling_list4x4 = &pps->scaling_list4x4[0];
    scaling_list8x8 = &pps->scaling_list8x8[0];
  }

  static_assert(std::extent<decltype(matrix.scaling_list_4x4), 1>() ==
                std::extent<decltype(zigzag_4x4)>());
  for (size_t i = 0; i < std::size(matrix.scaling_list_4x4); ++i) {
    for (size_t j = 0; j < std::size(matrix.scaling_list_4x4[i]); ++j) {
      matrix.scaling_list_4x4[i][zigzag_4x4[j]] = scaling_list4x4[i][j];
    }
  }

  static_assert(std::extent<decltype(matrix.scaling_list_8x8), 1>() ==
                std::extent<decltype(zigzag_8x8)>());
  for (size_t i = 0; i < std::size(matrix.scaling_list_8x8); ++i) {
    for (size_t j = 0; j < std::size(matrix.scaling_list_8x8[i]); ++j) {
      matrix.scaling_list_8x8[i][zigzag_8x8[j]] = scaling_list8x8[i][j];
    }
  }

  return matrix;
}

// Sets up v4l2_ctrl_h264_decode_params from data in the H264SliceHeader and
// the current H264SliceMetadata.
v4l2_ctrl_h264_decode_params SetupDecodeParams(
    const H264SliceHeader& slice,
    const H264SliceMetadata& slice_metadata,
    const H264DPB& dpb) {
  v4l2_ctrl_h264_decode_params v4l2_decode_params = {};

  v4l2_decode_params.nal_ref_idc = slice.nal_ref_idc;
  v4l2_decode_params.frame_num = slice.frame_num;
  v4l2_decode_params.idr_pic_id = slice.idr_pic_id;
  v4l2_decode_params.pic_order_cnt_lsb = slice.pic_order_cnt_lsb;
  v4l2_decode_params.delta_pic_order_cnt_bottom =
      slice.delta_pic_order_cnt_bottom;
  v4l2_decode_params.delta_pic_order_cnt0 = slice.delta_pic_order_cnt0;
  v4l2_decode_params.delta_pic_order_cnt1 = slice.delta_pic_order_cnt1;
  v4l2_decode_params.dec_ref_pic_marking_bit_size =
      slice.dec_ref_pic_marking_bit_size;
  v4l2_decode_params.pic_order_cnt_bit_size = slice.pic_order_cnt_bit_size;

  v4l2_decode_params.flags = 0;
  if (slice.idr_pic_flag)
    v4l2_decode_params.flags |= V4L2_H264_DECODE_PARAM_FLAG_IDR_PIC;

  v4l2_decode_params.top_field_order_cnt = slice_metadata.top_field_order_cnt;
  v4l2_decode_params.bottom_field_order_cnt =
      slice_metadata.bottom_field_order_cnt;

  size_t i = 0;
  constexpr size_t kTimestampToNanoSecs = 1000;
  for (const auto& element : dpb) {
    struct v4l2_h264_dpb_entry& entry = v4l2_decode_params.dpb[i++];
    entry = {.reference_ts = element.second.ref_ts_nsec * kTimestampToNanoSecs,
             .pic_num = static_cast<unsigned short>(element.second.pic_num),
             .frame_num = static_cast<unsigned short>(element.second.frame_num),
             .fields = V4L2_H264_FRAME_REF,
             .top_field_order_cnt = element.second.top_field_order_cnt,
             .bottom_field_order_cnt = element.second.bottom_field_order_cnt,
             .flags = static_cast<uint32_t>(
                 V4L2_H264_DPB_ENTRY_FLAG_VALID |
                 (element.second.ref ? V4L2_H264_DPB_ENTRY_FLAG_ACTIVE : 0) |
                 (element.second.long_term_reference_flag
                      ? V4L2_H264_DPB_ENTRY_FLAG_LONG_TERM
                      : 0))};
  }

  return v4l2_decode_params;
}

// Determines whether the current slice is part of the same
// frame as the previous slice.
// From h264 specification 7.4.1.2.4
bool IsNewFrame(const H264SliceMetadata& curr_picture,
                const H264SliceHeader& curr_slice_hdr,
                const H264SPS& sps) {
  bool nalu_size_error = curr_picture.slice_header.nalu_size < 1;

  bool slice_changed =
      curr_slice_hdr.frame_num != curr_picture.slice_header.frame_num ||
      curr_slice_hdr.pic_parameter_set_id !=
          curr_picture.slice_header.pic_parameter_set_id ||
      curr_slice_hdr.nal_ref_idc != curr_picture.slice_header.nal_ref_idc ||
      curr_slice_hdr.idr_pic_flag != curr_picture.slice_header.idr_pic_flag ||
      curr_slice_hdr.idr_pic_id != curr_picture.slice_header.idr_pic_id;

  bool slice_pic_order_changed = false;

  if (sps.pic_order_cnt_type == 0) {
    slice_pic_order_changed =
        curr_slice_hdr.pic_order_cnt_lsb !=
            curr_picture.slice_header.pic_order_cnt_lsb ||
        curr_slice_hdr.delta_pic_order_cnt_bottom !=
            curr_picture.slice_header.delta_pic_order_cnt_bottom;

  } else if (sps.pic_order_cnt_type == 1) {
    slice_pic_order_changed =
        curr_slice_hdr.delta_pic_order_cnt0 !=
            curr_picture.slice_header.delta_pic_order_cnt0 ||
        curr_slice_hdr.delta_pic_order_cnt1 !=
            curr_picture.slice_header.delta_pic_order_cnt1;
  }

  return (nalu_size_error || slice_changed || slice_pic_order_changed);
}

// Returns the maximum DPB Macro Block Size (MBS) per level specified.
// Based on spec table A-2.
uint32_t GetMaxDPBMBS(uint8_t level) {
  switch (level) {
    case H264SPS::kLevelIDC1p0:
      return 396;  // Level 1.0
    case H264SPS::kLevelIDC1B:
      return 396;  // Level 1b
    case H264SPS::kLevelIDC1p1:
      return 900;  // Level 1.1
    case H264SPS::kLevelIDC1p2:
      return 2376;  // Level 1.2
    case H264SPS::kLevelIDC1p3:
      return 2376;  // Level 1.3
    case H264SPS::kLevelIDC2p0:
      return 2376;  // Level 2.0
    case H264SPS::kLevelIDC2p1:
      return 4752;  // Level 2.1
    case H264SPS::kLevelIDC2p2:
      return 8100;  // Level 2.2
    case H264SPS::kLevelIDC3p0:
      return 8100;  // Level 3.0
    case H264SPS::kLevelIDC3p1:
      return 18000;  // Level 3.1
    case H264SPS::kLevelIDC3p2:
      return 20480;  // Level 3.2
    case H264SPS::kLevelIDC4p0:
      return 32768;  // Level 4.0
    case H264SPS::kLevelIDC4p1:
      return 32768;  // Level 4.1
    case H264SPS::kLevelIDC4p2:
      return 34816;  // Level 4.2
    case H264SPS::kLevelIDC5p0:
      return 110400;  // Level 5.0
    case H264SPS::kLevelIDC5p1:
      return 184320;  // Level 5.1
    case H264SPS::kLevelIDC5p2:
    default:
      return 0;
  }
}

}  // namespace

void H264Decoder::ProcessSPS(const int sps_id) {
  const H264SPS* sps = parser_->GetSPS(sps_id);
  gfx::Size new_pic_size = sps->GetCodedSize().value_or(gfx::Size());

  int width_mb = new_pic_size.width() / 16;
  int height_mb = new_pic_size.height() / 16;

  // Spec A.3.1 and A.3.2
  // For Baseline, Constrained Baseline and Main profile, the indicated level is
  // Level 1b if level_idc is equal to 11 and constraint_set3_flag is equal to 1
  uint8_t level = base::checked_cast<uint8_t>(sps->level_idc);
  if ((sps->profile_idc == H264SPS::kProfileIDCBaseline ||
       sps->profile_idc == H264SPS::kProfileIDCConstrainedBaseline ||
       sps->profile_idc == H264SPS::kProfileIDCMain) &&
      level == 11 && sps->constraint_set3_flag) {
    level = 9;  // Level 1b
  }
  int max_dpb_mbs = base::checked_cast<int>(GetMaxDPBMBS(level));

  // MaxDpbFrames from level limits per spec.
  size_t max_dpb_frames = std::min(max_dpb_mbs / (width_mb * height_mb), 16);

  size_t max_dpb_size =
      std::max(static_cast<int>(max_dpb_frames),
               std::max(sps->max_num_ref_frames, sps->max_dec_frame_buffering));

  VideoCodecProfile new_profile =
      H264Parser::ProfileIDCToVideoCodecProfile(sps->profile_idc);
  uint8_t new_bit_depth = 0;
  ParseBitDepth(*sps, new_bit_depth);

  if (sps->vui_parameters_present_flag && sps->bitstream_restriction_flag) {
    max_num_reorder_frames_ =
        base::checked_cast<size_t>(sps->max_num_reorder_frames);
  } else if (sps->constraint_set3_flag) {
    // max_num_reorder_frames not present, infer from profile/constraints
    // (see VUI semantics in spec).
    switch (sps->profile_idc) {
      case 44:
      case 86:
      case 100:
      case 110:
      case 122:
      case 244:
        max_num_reorder_frames_ = 0;
        break;
      default:
        max_num_reorder_frames_ = max_dpb_size;
        break;
    }
  } else {
    max_num_reorder_frames_ = max_dpb_size;
  }

  if (pic_size_ != new_pic_size || dpb_.max_dpb_size_ != max_dpb_size ||
      profile_ != new_profile || bit_depth_ != new_bit_depth) {
    FlushDPB();
    profile_ = new_profile;
    bit_depth_ = new_bit_depth;
    pic_size_ = new_pic_size;
    dpb_.max_dpb_size_ = max_dpb_size;
  }
}

void H264Decoder::FlushDPB() {
  std::vector<H264SliceMetadata*> transmittable_slices =
      dpb_.GetNotOutputtedPicsAppending();
  std::sort(transmittable_slices.begin(), transmittable_slices.end(),
            H264PicOrderCompare());

  for (auto* i : transmittable_slices) {
    i->outputted = true;
    slice_ready_queue_.push(*i);
  }

  dpb_.clear();
}

void H264Decoder::InitializeDecoderLogic() {
  parser_ = std::make_unique<H264Parser>();
  parser_->SetStream(data_stream_->data(), data_stream_->length());

  // Advance through NALUs until the first SPS.  The start of the decodable
  // data in an h.264 bistreams starts with an SPS.
  while (true) {
    H264NALU nalu;
    H264Parser::Result res = parser_->AdvanceToNextNALU(&nalu);
    CHECK(res == H264Parser::kOk);

    if (nalu.nal_unit_type == H264NALU::kSPS) {
      break;
    }
  }

  int sps_id;
  H264Parser::Result res = parser_->ParseSPS(&sps_id);
  CHECK(res == H264Parser::kOk);

  // Process initial SPS in bitstream and navigate to first slice in bitstream
  // to setup ProcessNextFrame for decoding.
  ProcessSPS(sps_id);
  std::unique_ptr<H264NALU> curr_nalu;
  while (true) {
    curr_nalu = std::make_unique<H264NALU>();
    if (parser_->AdvanceToNextNALU(curr_nalu.get()) == H264Parser::kEOStream) {
      break;
    }

    if (curr_nalu->nal_unit_type == H264NALU::kIDRSlice ||
        curr_nalu->nal_unit_type == H264NALU::kNonIDRSlice) {
      break;
    } else if (curr_nalu->nal_unit_type == H264NALU::kPPS) {
      int pps_id;
      CHECK(parser_->ParsePPS(&pps_id) == H264Parser::kOk);
    }
  }

  curr_slice_hdr_ = std::make_unique<H264SliceHeader>();
  CHECK(parser_->ParseSliceHeader(*curr_nalu, curr_slice_hdr_.get()) ==
        H264Parser::kOk);
}

VideoDecoder::Result H264Decoder::SubmitSlice() {
  std::vector<uint8_t> slice_data(
      sizeof(V4L2_STATELESS_H264_START_CODE_ANNEX_B) - 1);
  slice_data[2] = V4L2_STATELESS_H264_START_CODE_ANNEX_B;
  slice_data.insert(slice_data.end(), (curr_slice_hdr_->nalu_data).get(),
                    (curr_slice_hdr_->nalu_data +
                     base::checked_cast<size_t>(curr_slice_hdr_->nalu_size))
                        .get());

  scoped_refptr<MmappedBuffer> OUTPUT_buffer = OUTPUT_queue_->GetBuffer(0);
  OUTPUT_buffer->mmapped_planes()[0].CopyIn(&slice_data[0], slice_data.size());
  OUTPUT_buffer->set_frame_number(global_pic_count_);

  if (!v4l2_ioctl_->QBuf(OUTPUT_queue_, 0)) {
    VLOG(4) << "VIDIOC_QBUF failed for OUTPUT queue.";
    return VideoDecoder::kError;
  }

  global_pic_count_++;
  return VideoDecoder::kOk;
}

VideoDecoder::Result H264Decoder::InitializeSliceMetadata(
    const H264SliceHeader& slice_hdr,
    const H264SPS* sps,
    H264SliceMetadata* slice_metadata) const {
  if (!sps) {
    return VideoDecoder::kError;
  }

  slice_metadata->slice_header = slice_hdr;
  slice_metadata->ref_ts_nsec = global_pic_count_;
  slice_metadata->ref = slice_hdr.nal_ref_idc != 0;
  slice_metadata->frame_num = slice_hdr.frame_num;
  slice_metadata->pic_num = slice_hdr.frame_num;
  slice_metadata->pic_order_cnt_lsb = slice_hdr.pic_order_cnt_lsb;

  const auto visible_rect = sps->GetVisibleRect();
  // If there is no value, then the bitstream is invalid
  CHECK(visible_rect.has_value());
  slice_metadata->visible_rect_ = *visible_rect;

  slice_metadata->long_term_reference_flag = slice_hdr.long_term_reference_flag;

  if (slice_hdr.adaptive_ref_pic_marking_mode_flag) {
    static_assert(sizeof(slice_metadata->ref_pic_marking) ==
                      sizeof(slice_hdr.ref_pic_marking),
                  "Array sizes of ref pic marking do not match.");
    memcpy(slice_metadata->ref_pic_marking, slice_hdr.ref_pic_marking,
           sizeof(slice_metadata->ref_pic_marking));
  }

  // Calculate H264 slice order counts.
  switch (sps->pic_order_cnt_type) {
    // See specification 8.2.1.1.
    case 0: {
      int prev_pic_order_cnt_msb, prev_pic_order_cnt_lsb;
      if (slice_hdr.idr_pic_flag) {
        prev_pic_order_cnt_msb = prev_pic_order_cnt_lsb = 0;
      } else {
        prev_pic_order_cnt_msb = prev_pic_order_.prev_ref_pic_order_cnt_msb;
        prev_pic_order_cnt_lsb = prev_pic_order_.prev_ref_pic_order_cnt_lsb;
      }

      const int max_pic_order_cnt_lsb =
          1 << (sps->log2_max_pic_order_cnt_lsb_minus4 + 4);
      if ((slice_metadata->pic_order_cnt_lsb < prev_pic_order_cnt_lsb) &&
          (prev_pic_order_cnt_lsb - slice_metadata->pic_order_cnt_lsb >=
           max_pic_order_cnt_lsb / 2)) {
        slice_metadata->pic_order_cnt_msb =
            prev_pic_order_cnt_msb + max_pic_order_cnt_lsb;
      } else if ((slice_metadata->pic_order_cnt_lsb > prev_pic_order_cnt_lsb) &&
                 (slice_metadata->pic_order_cnt_lsb - prev_pic_order_cnt_lsb >
                  max_pic_order_cnt_lsb / 2)) {
        slice_metadata->pic_order_cnt_msb =
            prev_pic_order_cnt_msb - max_pic_order_cnt_lsb;
      } else {
        slice_metadata->pic_order_cnt_msb = prev_pic_order_cnt_msb;
      }

      slice_metadata->top_field_order_cnt =
          slice_metadata->pic_order_cnt_msb + slice_metadata->pic_order_cnt_lsb;
      slice_metadata->bottom_field_order_cnt =
          slice_metadata->top_field_order_cnt +
          slice_hdr.delta_pic_order_cnt_bottom;
      break;
    }
    case 1: {
      // TODO(b/234752983): Implement pic ordering for pic order count type 1
      // as defined in H.264 section 8.2.1.2.
      break;
    }
    case 2: {
      // Implements pic ordering for pic order count type 2 as defined
      // in H.264 section 8.2.1.3.
      if (slice_metadata->slice_header.idr_pic_flag) {
        slice_metadata->frame_num_offset = 0;
      } else if (prev_frame_num_ > slice_metadata->pic_num) {
        slice_metadata->frame_num_offset =
            prev_frame_num_offset_ +
            (1 << (sps->log2_max_frame_num_minus4 + 4));
      } else {
        slice_metadata->frame_num_offset = prev_frame_num_offset_;
      }

      int temp_pic_order_cnt;
      if (slice_metadata->slice_header.idr_pic_flag) {
        temp_pic_order_cnt = 0;
      } else if (!slice_metadata->slice_header.nal_ref_idc) {
        temp_pic_order_cnt =
            2 * (slice_metadata->frame_num_offset + slice_metadata->frame_num) -
            1;
      } else {
        temp_pic_order_cnt =
            2 * (slice_metadata->frame_num_offset + slice_metadata->frame_num);
      }

      slice_metadata->top_field_order_cnt = temp_pic_order_cnt;
      slice_metadata->bottom_field_order_cnt = temp_pic_order_cnt;
      break;
    }
    default: {
      VLOGF(4) << "Invalid pic_order_cnt_type: " << sps->pic_order_cnt_type;
      return VideoDecoder::kError;
    }
  }

  slice_metadata->pic_order_cnt =
      std::min(slice_metadata->top_field_order_cnt,
               slice_metadata->bottom_field_order_cnt);

  return VideoDecoder::kOk;
}

VideoDecoder::Result H264Decoder::StartNewFrame(
    bool is_OUTPUT_queue_new,
    H264SliceMetadata* slice_metadata) {
  const H264PPS* pps = parser_->GetPPS(curr_slice_hdr_->pic_parameter_set_id);
  const H264SPS* sps = parser_->GetSPS(pps->seq_parameter_set_id);

  if (InitializeSliceMetadata(*(curr_slice_hdr_.get()), sps, slice_metadata) ==
      VideoDecoder::kError) {
    return VideoDecoder::kError;
  }

  if (curr_slice_hdr_->idr_pic_flag) {
    if (!curr_slice_hdr_->no_output_of_prior_pics_flag) {
      FlushDPB();
    }
    dpb_.clear();
  }

  int max_frame_num = 1 << (sps->log2_max_frame_num_minus4 + 4);
  dpb_.UpdatePicNums(curr_slice_hdr_->frame_num, max_frame_num);

  struct v4l2_ctrl_h264_sps v4l2_sps = SetupSPSCtrl(sps);
  struct v4l2_ctrl_h264_pps v4l2_pps = SetupPPSCtrl(pps);
  struct v4l2_ctrl_h264_scaling_matrix v4l2_matrix =
      SetupScalingMatrix(sps, pps);

  struct v4l2_ext_control ctrls[] = {
      {.id = V4L2_CID_STATELESS_H264_SPS,
       .size = sizeof(v4l2_sps),
       .ptr = &v4l2_sps},
      {.id = V4L2_CID_STATELESS_H264_PPS,
       .size = sizeof(v4l2_pps),
       .ptr = &v4l2_pps},
      {.id = V4L2_CID_STATELESS_H264_SCALING_MATRIX,
       .size = sizeof(v4l2_matrix),
       .ptr = &v4l2_matrix}};
  struct v4l2_ext_controls ext_ctrls = {
      .count = (sizeof(ctrls) / sizeof(ctrls[0])), .controls = ctrls};

  v4l2_ioctl_->SetExtCtrls(OUTPUT_queue_, &ext_ctrls, is_OUTPUT_queue_new);

  return VideoDecoder::kOk;
}

void H264Decoder::ProcessNextFrame() {
  H264SliceMetadata slice_metadata = {};

  const bool is_OUTPUT_queue_new = !OUTPUT_queue_;
  if (!OUTPUT_queue_) {
    CreateOUTPUTQueue(kDriverCodecFourcc);
  }

  StartNewFrame(is_OUTPUT_queue_new, &slice_metadata);
  v4l2_ctrl_h264_decode_params v4l2_decode_params =
      SetupDecodeParams(*curr_slice_hdr_, slice_metadata, dpb_);

  const int pps_id = curr_slice_hdr_->pic_parameter_set_id;
  const int sps_id = parser_->GetPPS(pps_id)->seq_parameter_set_id;

  struct v4l2_ext_control ctrls[] = {
      {.id = V4L2_CID_STATELESS_H264_DECODE_PARAMS,
       .size = sizeof(v4l2_decode_params),
       .ptr = &v4l2_decode_params},
      {.id = V4L2_CID_STATELESS_H264_DECODE_MODE,
       .value = V4L2_STATELESS_H264_DECODE_MODE_FRAME_BASED}};
  struct v4l2_ext_controls ext_ctrls = {
      .count = (sizeof(ctrls) / sizeof(ctrls[0])), .controls = ctrls};

  v4l2_ioctl_->SetExtCtrls(OUTPUT_queue_, &ext_ctrls);

  SubmitSlice();

  while (true) {
    std::unique_ptr<H264NALU> curr_nalu = std::make_unique<H264NALU>();
    if (parser_->AdvanceToNextNALU(curr_nalu.get()) == H264Parser::kEOStream) {
      stream_finished_ = true;
      break;
    }

    if (curr_nalu->nal_unit_type == H264NALU::kNonIDRSlice ||
        curr_nalu->nal_unit_type == H264NALU::kIDRSlice) {
      curr_slice_hdr_ = std::make_unique<H264SliceHeader>();
      CHECK(parser_->ParseSliceHeader(*curr_nalu, curr_slice_hdr_.get()) ==
            H264Parser::kOk);

      const H264SPS* sps = parser_->GetSPS(sps_id);
      if (IsNewFrame(slice_metadata, *curr_slice_hdr_.get(), *sps)) {
        break;
      }

    } else if (curr_nalu->nal_unit_type == H264NALU::kSPS) {
      int sps_info;
      H264Parser::Result res = parser_->ParseSPS(&sps_info);
      CHECK(res == H264Parser::kOk);
      ProcessSPS(sps_id);

    } else if (curr_nalu->nal_unit_type == H264NALU::kPPS) {
      int pps_info;
      H264Parser::Result res = parser_->ParsePPS(&pps_info);
      CHECK(res == H264Parser::kOk);
    }
    // All other NALU's can be safely dropped/ignored.
  }

  FinishPicture(slice_metadata, sps_id);

  if (stream_finished_) {
    FlushDPB();
  }
}

void H264Decoder::FinishPicture(H264SliceMetadata picture, const int sps_id) {
  v4l2_ioctl_->MediaRequestIocQueue(OUTPUT_queue_);

  if (!CAPTURE_queue_) {
    CreateCAPTUREQueue(kNumberOfBuffersInCaptureQueue);
  }

  v4l2_ioctl_->WaitForRequestCompletion(OUTPUT_queue_);

  uint32_t CAPTURE_id;
  v4l2_ioctl_->DQBuf(CAPTURE_queue_, &CAPTURE_id);

  CAPTURE_queue_->DequeueBufferId(CAPTURE_id);
  picture.capture_queue_buffer_id = CAPTURE_id;

  const std::set<uint32_t> reusable_buffer_slots =
      GetReusableReferenceSlots(*CAPTURE_queue_->GetBuffer(CAPTURE_id).get(),
                                CAPTURE_queue_->queued_buffer_ids());

  for (const auto reusable_buffer_slot : reusable_buffer_slots) {
    if (!v4l2_ioctl_->QBuf(CAPTURE_queue_, reusable_buffer_slot)) {
      VLOGF(4) << "VIDIOC_QBUF failed for CAPTURE queue.";
    }
    // Keeps track of which indices are currently queued in the
    // CAPTURE queue. This will be used to determine which indices
    // can/cannot be refreshed.
    CAPTURE_queue_->QueueBufferId(reusable_buffer_slot);
  }

  if (picture.ref) {
    // If picture is an IDR, need to unmark all unused reference pics.
    // H.264 section 8.2.4.1.2.
    if (picture.slice_header.idr_pic_flag) {
      dpb_.MarkAllUnusedRef();
      if (picture.long_term_reference_flag) {
        picture.long_term_frame_idx = 0;
      }

    } else if (picture.slice_header.adaptive_ref_pic_marking_mode_flag) {
      for (size_t i = 0; i < std::size(picture.ref_pic_marking); ++i) {
        H264DecRefPicMarking* ref_pic_marking = &picture.ref_pic_marking[i];

        // Handle Memory Mgmt operations as specified in specification 8.2.5.4.
        switch (ref_pic_marking->memory_mgmnt_control_operation) {
          case 0:
            break;

          case 1: {
            const int pic_num_x =
                picture.pic_num -
                (ref_pic_marking->difference_of_pic_nums_minus1 + 1);
            dpb_.UnmarkPicByPicNum(pic_num_x);
            break;
          }

          case 2: {
            dpb_.UnmarkLongTerm(ref_pic_marking->long_term_pic_num);
            break;
          }

          case 3: {
            // H.264 section 8.2.5.4.3
            const int pic_num_x =
                picture.pic_num -
                (ref_pic_marking->difference_of_pic_nums_minus1 + 1);
            H264SliceMetadata* short_pic =
                dpb_.GetShortRefPicByPicNum(pic_num_x);
            if (short_pic) {
              H264SliceMetadata* long_term_mark = dpb_.GetLongRefPicByFrameIdx(
                  ref_pic_marking->long_term_frame_idx);

              if (long_term_mark) {
                long_term_mark->ref = false;
              }

              short_pic->long_term_reference_flag = true;
              short_pic->long_term_frame_idx =
                  ref_pic_marking->long_term_frame_idx;
            }
            break;
          }

          case 4: {
            const int max_long_term_frame_idx =
                ref_pic_marking->max_long_term_frame_idx_plus1 - 1;
            dpb_.UnmarkLongTermPicsGreaterThanFrameIndex(
                max_long_term_frame_idx);
            break;
          }

          default:
            break;
        }
      }
    } else {
      // Use a sliding window method decoded reference picture marking process
      // H.264 section 8.2.4.3.
      const H264SPS* sps = parser_->GetSPS(sps_id);
      int num_ref_pics = dpb_.CountRefPics();
      if (num_ref_pics == std::max<int>(sps->max_num_ref_frames, 1)) {
        dpb_.UnmarkLowestFrameNumWrapShortRefPic();
      }
    }

    prev_pic_order_.prev_ref_pic_order_cnt_msb = picture.pic_order_cnt_msb;
    prev_pic_order_.prev_ref_pic_order_cnt_lsb = picture.pic_order_cnt_lsb;
  }

  prev_frame_num_ = picture.frame_num;
  prev_frame_num_offset_ = picture.frame_num_offset;

  dpb_.DeleteUnused();

  std::vector<H264SliceMetadata*> transmittable_slices =
      dpb_.GetNotOutputtedPicsAppending();
  // Include the current slice metadata to the list of transmittable slices.
  transmittable_slices.push_back(&picture);

  std::sort(transmittable_slices.begin(), transmittable_slices.end(),
            H264PicOrderCompare());

  auto output_candidate = transmittable_slices.begin();
  size_t slices_remaining = transmittable_slices.size();

  // Tries to output as many pictures as we can. A picture can be output,
  // if the number of decoded and not yet outputted pictures that would remain
  // in DPB afterwards would at least be equal to |max_num_reorder_frames|.
  while (output_candidate != transmittable_slices.end() &&
         (slices_remaining > max_num_reorder_frames_ ||
          // If the DPB is full and the output candidate has not been
          // outputted or is a reference picture then output this picture.
          (dpb_.size() == dpb_.max_dpb_size_ &&
           ((!(*output_candidate)->outputted || (*output_candidate)->ref)) &&
           slices_remaining))) {
    DVLOG_IF(1, slices_remaining <= max_num_reorder_frames_)
        << "Invalid stream: max_num_reorder_frames not preserved";

    (*output_candidate)->outputted = true;
    slice_ready_queue_.push(**output_candidate);

    // If the outputted picture is not a reference picture, it doesn't have
    // to remain in the DPB and can be removed.
    if (!(*output_candidate)->ref) {
      // Current picture hasn't been inserted into DPB yet, so don't remove it
      // if we managed to output it immediately.
      if ((*output_candidate)->ref_ts_nsec != picture.ref_ts_nsec) {
        dpb_.Delete(**output_candidate);
      }
    }

    ++output_candidate;
    --slices_remaining;
  }

  if (!picture.outputted || picture.ref) {
    dpb_.insert({picture.ref_ts_nsec, picture});
  }

  uint32_t OUTPUT_queue_buffer_id;
  v4l2_ioctl_->DQBuf(OUTPUT_queue_, &OUTPUT_queue_buffer_id);
  v4l2_ioctl_->MediaRequestIocReinit(OUTPUT_queue_);
}

// static
std::unique_ptr<H264Decoder> H264Decoder::Create(
    const base::MemoryMappedFile& stream) {
  auto parser = std::make_unique<H264Parser>();
  parser->SetStream(stream.data(), stream.length());

  // Advance through NALUs until the first SPS.  The start of the decodable
  // data in an h.264 bistreams starts with an SPS.
  while (true) {
    H264NALU nalu;
    H264Parser::Result res = parser->AdvanceToNextNALU(&nalu);
    if (res != H264Parser::kOk) {
      LOG(ERROR) << "Unable to find SPS in stream";
      return nullptr;
    }

    if (nalu.nal_unit_type == H264NALU::kSPS)
      break;
  }

  int sps_id;
  H264Parser::Result res = parser->ParseSPS(&sps_id);
  CHECK(res == H264Parser::kOk);

  const H264SPS* sps = parser->GetSPS(sps_id);
  CHECK(sps);

  std::optional<gfx::Size> coded_size = sps->GetCodedSize();
  CHECK(coded_size);

  auto v4l2_ioctl = std::make_unique<V4L2IoctlShim>(kDriverCodecFourcc);

  return base::WrapUnique(
      new H264Decoder(std::move(v4l2_ioctl), coded_size.value(), stream));
}

H264Decoder::H264Decoder(std::unique_ptr<V4L2IoctlShim> v4l2_ioctl,
                         gfx::Size display_resolution,
                         const base::MemoryMappedFile& data_stream)
    : VideoDecoder::VideoDecoder(std::move(v4l2_ioctl), display_resolution),
      curr_slice_hdr_(nullptr),
      stream_finished_(false),
      data_stream_(data_stream) {}

H264Decoder::~H264Decoder() = default;

std::set<uint32_t> H264Decoder::GetReusableReferenceSlots(
    const MmappedBuffer& buffer,
    std::set<uint32_t> queued_buffer_ids) {
  std::set<uint32_t> reusable_buffer_slots = {};
  const std::set<int> dpb_ids = dpb_.GetHeldCaptureIds();
  for (size_t i = 0; i < CAPTURE_queue_->num_buffers(); i++) {
    // Check that index is not currently queued in the CAPTURE queue and
    // that it is not the same buffer index previously written to.
    if (!queued_buffer_ids.count(i) && i != buffer.buffer_id()) {
      if (dpb_ids.find(i) == dpb_ids.end()) {
        reusable_buffer_slots.insert(i);
      }
    }
  }
  return reusable_buffer_slots;
}

VideoDecoder::Result H264Decoder::DecodeNextFrame(const int frame_number,
                                                  std::vector<uint8_t>& y_plane,
                                                  std::vector<uint8_t>& u_plane,
                                                  std::vector<uint8_t>& v_plane,
                                                  gfx::Size& size,
                                                  BitDepth& bit_depth) {
  // If this is the start of the Decoder, initialize Decoder state.
  if (!parser_) {
    InitializeDecoderLogic();
  }

  // Keep decoding until either decoder has parsed entire bitstream or there is
  // a decoded frame ready.
  while (!stream_finished_ && slice_ready_queue_.empty()) {
    ProcessNextFrame();
  }

  if (stream_finished_ && slice_ready_queue_.empty()) {
    return VideoDecoder::kEOStream;
  }

  if (slice_ready_queue_.empty()) {
    NOTREACHED_IN_MIGRATION() << "Stream ended with |slice_ready_queue_| empty";
  }

  H264SliceMetadata picture = slice_ready_queue_.front();
  last_decoded_frame_visible_ = picture.outputted;
  scoped_refptr<MmappedBuffer> buffer =
      CAPTURE_queue_->GetBuffer(picture.capture_queue_buffer_id);
  size = picture.visible_rect_.size();

  if (!picture.visible_rect_.origin().IsOrigin()) {
    // TODO(b/315491484): Handle cropping with non-zero origin
    LOG(INFO) << "Non-zero visible rect origin.";
  }

  bit_depth =
      ConvertToYUV(y_plane, u_plane, v_plane, size, buffer->mmapped_planes(),
                   CAPTURE_queue_->resolution(), CAPTURE_queue_->fourcc());

  slice_ready_queue_.pop();
  return VideoDecoder::kOk;
}

}  // namespace v4l2_test
}  // namespace media
