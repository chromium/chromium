// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuzzer/FuzzedDataProvider.h>
#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <vector>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/numerics/safe_conversions.h"
#include "media/base/decoder_buffer.h"
#include "media/filters/h26x_annex_b_bitstream_builder.h"
#include "media/gpu/h264_builder.h"
#include "media/gpu/h264_decoder.h"
#include "media/gpu/h264_dpb.h"
#include "media/parsers/h264_parser.h"

namespace media {

namespace {

// Mock implementation of H264Picture to satisfy the decoder's requirement
// for picture objects during the decoding process.
class FakeH264Picture : public H264Picture {
 public:
  FakeH264Picture() = default;

 protected:
  ~FakeH264Picture() override = default;
};

// Mock implementation of H264Accelerator that does nothing but return success
// for all decoding operations. This allows the fuzzer to test the decoder's
// state machine and parser without needing real hardware acceleration.
class FakeH264Accelerator : public H264Decoder::H264Accelerator {
 public:
  FakeH264Accelerator() = default;
  ~FakeH264Accelerator() override = default;

  scoped_refptr<H264Picture> CreateH264Picture() override {
    return base::MakeRefCounted<FakeH264Picture>();
  }

  Status SubmitFrameMetadata(const H264SPS* sps,
                             const H264PPS* pps,
                             const H264DPB& dpb,
                             const H264Picture::Vector& ref_pic_listp0,
                             const H264Picture::Vector& ref_pic_listb0,
                             const H264Picture::Vector& ref_pic_listb1,
                             scoped_refptr<H264Picture> pic) override {
    return Status::kOk;
  }

  Status SubmitSlice(const H264PPS* pps,
                     const H264SliceHeader* slice_hdr,
                     const H264Picture::Vector& ref_pic_list0,
                     const H264Picture::Vector& ref_pic_list1,
                     scoped_refptr<H264Picture> pic,
                     const uint8_t* data,
                     size_t size,
                     const std::vector<SubsampleEntry>& subsamples) override {
    return Status::kOk;
  }

  Status SubmitDecode(scoped_refptr<H264Picture> pic) override {
    return Status::kOk;
  }

  bool OutputPicture(scoped_refptr<H264Picture> pic) override { return true; }

  void Reset() override {}
};

// Occasionally returns a value outside of the expected spec range for a given
// parameter. This is used to test the parser's error handling for out-of-range
// values.
template <typename T>
T ConsumeInRangeOrFull(FuzzedDataProvider& fdp, T min, T max) {
  if (fdp.ConsumeProbability<float>() < 0.1f) {
    // Occasionally return a value that might be out of spec but still fits in
    // the variable type. We cap it at 0x0FFFFFFF to prevent Exp-Golomb
    // encodes from exceeding the 64-bit limits of the bitstream builder
    // and to avoid 32-bit overflow in the builder's AppendUE implementation.
    if constexpr (sizeof(T) >= 4) {
      return fdp.ConsumeIntegralInRange<T>(std::numeric_limits<T>::min(),
                                           static_cast<T>(0x0FFFFFFF));
    }
    return fdp.ConsumeIntegral<T>();
  }
  return fdp.ConsumeIntegralInRange<T>(min, max);
}

// Fills an H264SPS structure with values from the fuzzer. This structure is
// then used to build a valid SPS NALU.
void FillSPS(FuzzedDataProvider& fdp, H264SPS& sps) {
  sps.profile_idc = fdp.ConsumeIntegral<int>();
  sps.constraint_set0_flag = fdp.ConsumeBool();
  sps.constraint_set1_flag = fdp.ConsumeBool();
  sps.constraint_set2_flag = fdp.ConsumeBool();
  sps.constraint_set3_flag = fdp.ConsumeBool();
  sps.constraint_set4_flag = fdp.ConsumeBool();
  sps.constraint_set5_flag = fdp.ConsumeBool();
  sps.level_idc = fdp.ConsumeIntegral<int>();
  sps.seq_parameter_set_id = fdp.ConsumeIntegralInRange<int>(0, 31);

  if (sps.profile_idc == H264SPS::kProfileIDCHigh) {
    // Decoder only supports 4:2:0.
    sps.chroma_format_idc = 1;
    sps.bit_depth_luma_minus8 = ConsumeInRangeOrFull<int>(fdp, 0, 6);
    sps.bit_depth_chroma_minus8 = ConsumeInRangeOrFull<int>(fdp, 0, 6);
    sps.qpprime_y_zero_transform_bypass_flag = fdp.ConsumeBool();
    // Builder doesn't support scaling matrix yet.
    sps.seq_scaling_matrix_present_flag = false;
  }

  sps.log2_max_frame_num_minus4 = fdp.ConsumeIntegralInRange<int>(0, 31);
  sps.pic_order_cnt_type = ConsumeInRangeOrFull<int>(fdp, 0, 2);
  if (sps.pic_order_cnt_type == 0) {
    sps.log2_max_pic_order_cnt_lsb_minus4 =
        fdp.ConsumeIntegralInRange<int>(0, 31);
  } else if (sps.pic_order_cnt_type == 1) {
    sps.delta_pic_order_always_zero_flag = fdp.ConsumeBool();
    sps.offset_for_non_ref_pic = fdp.ConsumeIntegral<int>();
    sps.offset_for_top_to_bottom_field = fdp.ConsumeIntegral<int>();
    sps.num_ref_frames_in_pic_order_cnt_cycle =
        fdp.ConsumeIntegralInRange<int>(0, 255);
    for (int i = 0; i < sps.num_ref_frames_in_pic_order_cnt_cycle; ++i) {
      sps.offset_for_ref_frame[i] = fdp.ConsumeIntegral<int>();
    }
  }

  sps.max_num_ref_frames = ConsumeInRangeOrFull<int>(fdp, 0, 16);
  sps.gaps_in_frame_num_value_allowed_flag = fdp.ConsumeBool();
  sps.pic_width_in_mbs_minus1 = ConsumeInRangeOrFull<int>(fdp, 0, 127);
  sps.pic_height_in_map_units_minus1 = ConsumeInRangeOrFull<int>(fdp, 0, 127);

  // Decoder doesn't support interlacing.
  sps.frame_mbs_only_flag = true;

  sps.direct_8x8_inference_flag = fdp.ConsumeBool();

  sps.frame_cropping_flag = fdp.ConsumeBool();
  if (sps.frame_cropping_flag) {
    sps.frame_crop_left_offset = fdp.ConsumeIntegral<int>();
    sps.frame_crop_right_offset = fdp.ConsumeIntegral<int>();
    sps.frame_crop_top_offset = fdp.ConsumeIntegral<int>();
    sps.frame_crop_bottom_offset = fdp.ConsumeIntegral<int>();
  }

  sps.vui_parameters_present_flag = fdp.ConsumeBool();
  if (sps.vui_parameters_present_flag) {
    sps.timing_info_present_flag = fdp.ConsumeBool();
    if (sps.timing_info_present_flag) {
      sps.num_units_in_tick = fdp.ConsumeIntegral<int>();
      sps.time_scale = fdp.ConsumeIntegral<int>();
      sps.fixed_frame_rate_flag = fdp.ConsumeBool();
    }

    sps.nal_hrd_parameters_present_flag = fdp.ConsumeBool();
    if (sps.nal_hrd_parameters_present_flag) {
      sps.cpb_cnt_minus1 = fdp.ConsumeIntegralInRange<int>(0, 31);

      sps.bit_rate_scale = ConsumeInRangeOrFull<int>(fdp, 0, 15);
      sps.cpb_size_scale = ConsumeInRangeOrFull<int>(fdp, 0, 15);
      for (int i = 0; i <= sps.cpb_cnt_minus1; ++i) {
        sps.bit_rate_value_minus1[i] =
            fdp.ConsumeIntegralInRange<uint32_t>(0, 0x0FFFFFFF);
        sps.cpb_size_value_minus1[i] =
            fdp.ConsumeIntegralInRange<uint32_t>(0, 0x0FFFFFFF);
        sps.cbr_flag[i] = fdp.ConsumeBool();
      }
      sps.initial_cpb_removal_delay_length_minus_1 =
          ConsumeInRangeOrFull<int>(fdp, 0, 31);
      sps.cpb_removal_delay_length_minus1 =
          ConsumeInRangeOrFull<int>(fdp, 0, 31);
      sps.dpb_output_delay_length_minus1 =
          ConsumeInRangeOrFull<int>(fdp, 0, 31);
      sps.time_offset_length = ConsumeInRangeOrFull<int>(fdp, 0, 31);
    }
    sps.low_delay_hrd_flag = fdp.ConsumeBool();
  }
}

// Fills an H264PPS structure with fuzzed values. A previously generated SPS
// must be available to correctly link the PPS to it.
void FillPPS(FuzzedDataProvider& fdp, H264PPS& pps) {
  pps.pic_parameter_set_id = fdp.ConsumeIntegralInRange<int>(0, 255);
  pps.seq_parameter_set_id = fdp.ConsumeIntegralInRange<int>(0, 31);
  pps.entropy_coding_mode_flag = fdp.ConsumeBool();
  pps.bottom_field_pic_order_in_frame_present_flag = fdp.ConsumeBool();
  // Builder only supports 0.
  pps.num_slice_groups_minus1 = 0;

  pps.num_ref_idx_l0_default_active_minus1 =
      fdp.ConsumeIntegralInRange<int>(0, 31);
  pps.num_ref_idx_l1_default_active_minus1 =
      fdp.ConsumeIntegralInRange<int>(0, 31);

  pps.weighted_pred_flag = fdp.ConsumeBool();
  pps.weighted_bipred_idc = ConsumeInRangeOrFull<int>(fdp, 0, 2);

  pps.pic_init_qp_minus26 = ConsumeInRangeOrFull<int>(fdp, -26, 25);
  pps.pic_init_qs_minus26 = ConsumeInRangeOrFull<int>(fdp, -26, 25);
  pps.chroma_qp_index_offset = ConsumeInRangeOrFull<int>(fdp, -12, 12);

  pps.deblocking_filter_control_present_flag = fdp.ConsumeBool();
  pps.constrained_intra_pred_flag = fdp.ConsumeBool();
  pps.redundant_pic_cnt_present_flag = fdp.ConsumeBool();

  pps.transform_8x8_mode_flag = fdp.ConsumeBool();
  // Builder doesn't support scaling matrix yet.
  pps.pic_scaling_matrix_present_flag = false;
  pps.second_chroma_qp_index_offset = ConsumeInRangeOrFull<int>(fdp, -12, 12);
}

void BuildRefPicListModifications(FuzzedDataProvider& fdp,
                                  H26xAnnexBBitstreamBuilder& builder,
                                  uint8_t slice_type) {
  // 7.3.3.1 Reference picture list modification syntax
  bool is_p = (slice_type % 5 == 0 || slice_type % 5 == 3);
  bool is_b = (slice_type % 5 == 1);

  if (is_p || is_b) {
    bool ref_pic_list_modification_flag_l0 = fdp.ConsumeBool();
    builder.AppendBool(ref_pic_list_modification_flag_l0);
    if (ref_pic_list_modification_flag_l0) {
      while (true) {
        uint8_t modification_of_pic_nums_idc =
            ConsumeInRangeOrFull<uint8_t>(fdp, 0, 3);
        builder.AppendUE(modification_of_pic_nums_idc);
        if (modification_of_pic_nums_idc == 3) {
          break;
        }
        builder.AppendUE(ConsumeInRangeOrFull<unsigned int>(fdp, 0, 100));
        if (fdp.remaining_bytes() == 0) {
          break;
        }
      }
    }
  }
  if (is_b) {
    bool ref_pic_list_modification_flag_l1 = fdp.ConsumeBool();
    builder.AppendBool(ref_pic_list_modification_flag_l1);
    if (ref_pic_list_modification_flag_l1) {
      while (true) {
        uint8_t modification_of_pic_nums_idc =
            ConsumeInRangeOrFull<uint8_t>(fdp, 0, 3);
        builder.AppendUE(modification_of_pic_nums_idc);
        if (modification_of_pic_nums_idc == 3) {
          break;
        }
        builder.AppendUE(ConsumeInRangeOrFull<unsigned int>(fdp, 0, 100));
        if (fdp.remaining_bytes() == 0) {
          break;
        }
      }
    }
  }
}

void BuildWeightingFactors(FuzzedDataProvider& fdp,
                           H26xAnnexBBitstreamBuilder& builder,
                           int num_ref_idx_active_minus1,
                           int chroma_array_type) {
  for (int i = 0; i <= num_ref_idx_active_minus1; ++i) {
    bool luma_weight_flag = fdp.ConsumeBool();
    builder.AppendBool(luma_weight_flag);
    if (luma_weight_flag) {
      builder.AppendSE(ConsumeInRangeOrFull<int>(fdp, -128, 127));
      builder.AppendSE(ConsumeInRangeOrFull<int>(fdp, -128, 127));
    }
    if (chroma_array_type != 0) {
      bool chroma_weight_flag = fdp.ConsumeBool();
      builder.AppendBool(chroma_weight_flag);
      if (chroma_weight_flag) {
        for (int j = 0; j < 2; ++j) {
          builder.AppendSE(ConsumeInRangeOrFull<int>(fdp, -128, 127));
          builder.AppendSE(ConsumeInRangeOrFull<int>(fdp, -128, 127));
        }
      }
    }
  }
}

void BuildPredWeightTable(FuzzedDataProvider& fdp,
                          H26xAnnexBBitstreamBuilder& builder,
                          const H264SPS& sps,
                          uint8_t slice_type,
                          int num_ref_idx_l0_active_minus1,
                          int num_ref_idx_l1_active_minus1) {
  // 7.3.3.2 Prediction weight table syntax
  unsigned int luma_log2_weight_denom =
      ConsumeInRangeOrFull<unsigned int>(fdp, 0, 7);
  builder.AppendUE(luma_log2_weight_denom);
  int chroma_array_type =
      sps.separate_colour_plane_flag ? 0 : sps.chroma_format_idc;
  if (chroma_array_type != 0) {
    builder.AppendUE(ConsumeInRangeOrFull<unsigned int>(
        fdp, 0, 7));  // chroma_log2_weight_denom
  }

  BuildWeightingFactors(fdp, builder, num_ref_idx_l0_active_minus1,
                        chroma_array_type);

  bool is_b = (slice_type % 5 == 1);
  if (is_b) {
    BuildWeightingFactors(fdp, builder, num_ref_idx_l1_active_minus1,
                          chroma_array_type);
  }
}

void BuildDecRefPicMarking(FuzzedDataProvider& fdp,
                           H26xAnnexBBitstreamBuilder& builder,
                           bool is_idr) {
  // 7.3.3.3 Decoded reference picture marking syntax
  if (is_idr) {
    builder.AppendBool(fdp.ConsumeBool());  // no_output_of_prior_pics_flag
    builder.AppendBool(fdp.ConsumeBool());  // long_term_reference_flag
  } else {
    bool adaptive_ref_pic_marking_mode_flag = fdp.ConsumeBool();
    builder.AppendBool(adaptive_ref_pic_marking_mode_flag);
    if (adaptive_ref_pic_marking_mode_flag) {
      while (true) {
        unsigned int memory_management_control_operation =
            ConsumeInRangeOrFull<unsigned int>(fdp, 0, 6);
        builder.AppendUE(memory_management_control_operation);
        if (memory_management_control_operation == 0) {
          break;
        }
        if (memory_management_control_operation == 1 ||
            memory_management_control_operation == 3) {
          builder.AppendUE(ConsumeInRangeOrFull<unsigned int>(fdp, 0, 100));
        }
        if (memory_management_control_operation == 2) {
          builder.AppendUE(ConsumeInRangeOrFull<unsigned int>(fdp, 0, 100));
        }
        if (memory_management_control_operation == 3 ||
            memory_management_control_operation == 6) {
          builder.AppendUE(ConsumeInRangeOrFull<unsigned int>(fdp, 0, 100));
        }
        if (memory_management_control_operation == 4) {
          builder.AppendUE(ConsumeInRangeOrFull<unsigned int>(fdp, 0, 100));
        }
        if (fdp.remaining_bytes() == 0) {
          break;
        }
      }
    }
  }
}

// Generates a fuzzed H264 slice header NALU by following the bitstream syntax
// using an AnnexB builder. This ensures that the generated slice header
// is structurally valid and can be processed by the parser.
void BuildSliceHeader(FuzzedDataProvider& fdp,
                      H26xAnnexBBitstreamBuilder& builder,
                      const H264SPS& sps,
                      const H264PPS& pps,
                      bool is_idr) {
  builder.BeginNALU(is_idr ? H264NALU::kIDRSlice : H264NALU::kNonIDRSlice,
                    fdp.ConsumeIntegralInRange<int>(1, 3));

  builder.AppendUE(
      ConsumeInRangeOrFull<unsigned int>(fdp, 0, 100));  // first_mb_in_slice
  uint8_t slice_type = ConsumeInRangeOrFull<uint8_t>(fdp, 0, 9);
  builder.AppendUE(slice_type);
  builder.AppendUE(pps.pic_parameter_set_id);

  // frame_num
  // sps.log2_max_frame_num_minus4 + 4 can be out of range if
  // log2_max_frame_num_minus4 was fuzzed out of range.
  // We must ensure num_bits <= 64 for AppendBits.
  size_t frame_num_bits = sps.log2_max_frame_num_minus4 + 4;
  builder.AppendBits(frame_num_bits,
                     fdp.ConsumeIntegralInRange<uint32_t>(0, 32));

  bool field_pic_flag = false;
  if (!sps.frame_mbs_only_flag) {
    field_pic_flag = fdp.ConsumeBool();
    builder.AppendBool(field_pic_flag);
    if (field_pic_flag) {
      builder.AppendBool(fdp.ConsumeBool());  // bottom_field_flag
    }
  }

  if (is_idr) {
    builder.AppendUE(
        ConsumeInRangeOrFull<unsigned int>(fdp, 0, 65535));  // idr_pic_id
  }

  if (sps.pic_order_cnt_type == 0) {
    size_t poc_lsb_bits = sps.log2_max_pic_order_cnt_lsb_minus4 + 4;
    builder.AppendBits(poc_lsb_bits, fdp.ConsumeIntegral<uint32_t>());
    if (pps.bottom_field_pic_order_in_frame_present_flag && !field_pic_flag) {
      builder.AppendSE(
          fdp.ConsumeIntegral<int>());  // delta_pic_order_cnt_bottom
    }
  }

  if (sps.pic_order_cnt_type == 1 && !sps.delta_pic_order_always_zero_flag) {
    builder.AppendSE(fdp.ConsumeIntegral<int>());  // delta_pic_order_cnt0
    if (pps.bottom_field_pic_order_in_frame_present_flag && !field_pic_flag) {
      builder.AppendSE(fdp.ConsumeIntegral<int>());  // delta_pic_order_cnt1
    }
  }

  if (pps.redundant_pic_cnt_present_flag) {
    builder.AppendUE(ConsumeInRangeOrFull<unsigned int>(fdp, 0, 127));
  }

  bool is_b = (slice_type % 5 == 1);
  bool is_p = (slice_type % 5 == 0 || slice_type % 5 == 3);
  bool is_s = (slice_type % 5 == 3 || slice_type % 5 == 4);

  if (is_b) {
    builder.AppendBool(fdp.ConsumeBool());  // direct_spatial_mv_pred_flag
  }

  int num_ref_idx_l0_active_minus1 = pps.num_ref_idx_l0_default_active_minus1;
  int num_ref_idx_l1_active_minus1 = pps.num_ref_idx_l1_default_active_minus1;

  if (is_p || is_s || is_b) {
    bool num_ref_idx_active_override_flag = fdp.ConsumeBool();
    builder.AppendBool(num_ref_idx_active_override_flag);
    if (num_ref_idx_active_override_flag) {
      num_ref_idx_l0_active_minus1 = fdp.ConsumeIntegralInRange(0, 15);
      builder.AppendUE(num_ref_idx_l0_active_minus1);
      if (is_b) {
        num_ref_idx_l1_active_minus1 = fdp.ConsumeIntegralInRange(0, 15);
        builder.AppendUE(num_ref_idx_l1_active_minus1);
      }
    }
  }

  BuildRefPicListModifications(fdp, builder, slice_type);

  if ((pps.weighted_pred_flag && (is_p || is_s)) ||
      (pps.weighted_bipred_idc == 1 && is_b)) {
    BuildPredWeightTable(fdp, builder, sps, slice_type,
                         num_ref_idx_l0_active_minus1,
                         num_ref_idx_l1_active_minus1);
  }

  // nal_ref_idc is used in BuildDecRefPicMarking but we don't have it here
  // directly, we used fdp.ConsumeIntegralInRange<int>(1, 3) in BeginNALU above.
  BuildDecRefPicMarking(fdp, builder, is_idr);

  if (pps.entropy_coding_mode_flag && (slice_type % 5 != 2) &&
      (slice_type % 5 != 4)) {
    builder.AppendUE(
        ConsumeInRangeOrFull<unsigned int>(fdp, 0, 2));  // cabac_init_idc
  }

  // Simplified range for SE fields to avoid triggering parser errors too easily
  builder.AppendSE(ConsumeInRangeOrFull<int>(fdp, -26, 25));  // slice_qp_delta

  if (is_s) {
    if (slice_type % 5 == 3) {
      builder.AppendBool(fdp.ConsumeBool());  // sp_for_switch_flag
    }
    builder.AppendSE(
        ConsumeInRangeOrFull<int>(fdp, -26, 25));  // slice_qs_delta
  }

  if (pps.deblocking_filter_control_present_flag) {
    unsigned int disable_deblocking_filter_idc =
        ConsumeInRangeOrFull<unsigned int>(fdp, 0, 2);
    builder.AppendUE(disable_deblocking_filter_idc);
    if (disable_deblocking_filter_idc != 1) {
      builder.AppendSE(
          ConsumeInRangeOrFull<int>(fdp, -6, 6));  // slice_alpha_c0_offset_div2
      builder.AppendSE(
          ConsumeInRangeOrFull<int>(fdp, -6, 6));  // slice_beta_offset_div2
    }
  }

  builder.FinishNALU();
}

void ParseBitstream(base::span<const uint8_t> data) {
  H264Parser parser;
  parser.SetStream(data);

  while (true) {
    H264NALU nalu;
    H264Parser::Result res = parser.AdvanceToNextNALU(&nalu);
    if (res != H264Parser::kOk) {
      break;
    }

    switch (nalu.nal_unit_type) {
      case H264NALU::kSPS: {
        int id;
        parser.ParseSPS(&id);
        break;
      }
      case H264NALU::kPPS: {
        int id;
        parser.ParsePPS(&id);
        break;
      }
      case H264NALU::kIDRSlice:
      case H264NALU::kNonIDRSlice: {
        H264SliceHeader shdr;
        parser.ParseSliceHeader(nalu, &shdr);
        break;
      }
      case H264NALU::kSEIMessage: {
        H264SEI sei;
        parser.ParseSEI(&sei);
        break;
      }
      default:
        break;
    }
  }
}

}  // namespace

// Main fuzzer entry point. It iteratively generates a sequence of H264 NALUs
// (SPS, PPS, Slice Headers, etc.) to form a plausible Annex B bitstream.
// This bitstream is then passed to the H264 parser and decoder to explore
// deeper into the parser code.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < 1) {
    return 0;
  }

  FuzzedDataProvider fdp(data, size);
  std::vector<uint8_t> final_bitstream;

  // Keep track of generated SPS/PPS to provide to builders.
  std::map<int, H264SPS> generated_sps;
  std::map<int, H264PPS> generated_pps;

  while (fdp.remaining_bytes() > 0) {
    uint8_t action = fdp.ConsumeIntegralInRange<uint8_t>(0, 4);
    switch (action) {
      case 0: {  // SPS
        H264SPS sps;
        FillSPS(fdp, sps);
        H26xAnnexBBitstreamBuilder builder(true);
        BuildPackedH264SPS(builder, sps);
        builder.Flush();
        auto nalu_data = builder.data();
        final_bitstream.insert(final_bitstream.end(), nalu_data.begin(),
                               nalu_data.end());
        generated_sps[sps.seq_parameter_set_id] = sps;
        break;
      }
      case 1: {  // PPS
        if (generated_sps.empty()) {
          break;
        }
        H264PPS pps;
        FillPPS(fdp, pps);
        // Find a matching SPS or use the first one.
        auto it = generated_sps.find(pps.seq_parameter_set_id);
        const H264SPS& sps = (it != generated_sps.end())
                                 ? it->second
                                 : generated_sps.begin()->second;
        pps.seq_parameter_set_id = sps.seq_parameter_set_id;

        H26xAnnexBBitstreamBuilder builder(true);
        BuildPackedH264PPS(builder, sps, pps);
        builder.Flush();
        auto nalu_data = builder.data();
        final_bitstream.insert(final_bitstream.end(), nalu_data.begin(),
                               nalu_data.end());
        generated_pps[pps.pic_parameter_set_id] = pps;
        break;
      }
      case 2: {  // Slice Header
        if (generated_pps.empty()) {
          break;
        }
        // Pick a random PPS.
        auto it_pps = generated_pps.begin();
        std::advance(it_pps, fdp.ConsumeIntegralInRange<size_t>(
                                 0, generated_pps.size() - 1));
        const H264PPS& pps = it_pps->second;

        auto it_sps = generated_sps.find(pps.seq_parameter_set_id);
        if (it_sps == generated_sps.end()) {
          break;
        }
        const H264SPS& sps = it_sps->second;

        H26xAnnexBBitstreamBuilder builder(true);
        BuildSliceHeader(fdp, builder, sps, pps, fdp.ConsumeBool());
        builder.Flush();
        auto nalu_data = builder.data();
        final_bitstream.insert(final_bitstream.end(), nalu_data.begin(),
                               nalu_data.end());
        break;
      }
      case 3: {  // Prefix NALU
        int nal_ref_idc = fdp.ConsumeIntegralInRange<int>(0, 3);
        H264NALU::Type type =
            fdp.ConsumeBool() ? H264NALU::kIDRSlice : H264NALU::kNonIDRSlice;
        uint8_t temporal_id = fdp.ConsumeIntegralInRange<uint8_t>(0, 7);
        if (type == H264NALU::kIDRSlice) {
          temporal_id = 0;
        }
        std::vector<uint8_t> prefix =
            BuildPrefixNALU(nal_ref_idc, type, temporal_id);
        final_bitstream.insert(final_bitstream.end(), prefix.begin(),
                               prefix.end());
        break;
      }
      case 4: {  // Raw data (simulating random junk or hand-crafted NALU)
        size_t raw_size = fdp.ConsumeIntegralInRange<size_t>(1, 100);
        std::vector<uint8_t> raw_bytes = fdp.ConsumeBytes<uint8_t>(raw_size);
        final_bitstream.insert(final_bitstream.end(), raw_bytes.begin(),
                               raw_bytes.end());
        break;
      }
    }

    if (final_bitstream.size() > 20000) {
      break;
    }
  }

  if (!final_bitstream.empty()) {
    ParseBitstream(final_bitstream);

    H264Decoder decoder(std::make_unique<FakeH264Accelerator>(),
                        H264PROFILE_MAIN);
    decoder.SetStream(0, DecoderBuffer::CopyFrom(final_bitstream));

    // Decode until we run out of stream, hit an error, or reach a reasonable
    // limit of config changes to avoid wasting time on SPS-only streams.
    for (int i = 0; i < 100; ++i) {
      if (decoder.Decode() != AcceleratedVideoDecoder::kConfigChange) {
        break;
      }
    }
  }

  return 0;
}

}  // namespace media
