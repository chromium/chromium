// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/parsers/h266_parser.h"

#include <stddef.h>

#include <algorithm>
#include <cmath>
#include <cstring>

#include "base/bits.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "media/base/decrypt_config.h"
#include "media/base/video_codecs.h"
#include "media/parsers/bit_reader_macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace media {

namespace {
// Equation 24. kDiagScanOrderMxM[i][0] is the horizontal component, and
// kDiagScanOrderMxM[i][1] is the vertical component.
constexpr uint8_t kDiagScanOrder8x8[64][2] = {
    {0, 0}, {0, 1}, {1, 0}, {0, 2}, {1, 1}, {2, 0}, {0, 3}, {1, 2},
    {2, 1}, {3, 0}, {0, 4}, {1, 3}, {2, 2}, {3, 1}, {4, 0}, {0, 5},
    {1, 4}, {2, 3}, {3, 2}, {4, 1}, {5, 0}, {0, 6}, {1, 5}, {2, 4},
    {3, 3}, {4, 2}, {5, 1}, {6, 0}, {0, 7}, {1, 6}, {2, 5}, {3, 4},
    {4, 3}, {5, 2}, {6, 1}, {7, 0}, {1, 7}, {2, 6}, {3, 5}, {4, 4},
    {5, 3}, {6, 2}, {7, 1}, {2, 7}, {3, 6}, {4, 5}, {5, 4}, {6, 3},
    {7, 2}, {3, 7}, {4, 6}, {5, 5}, {6, 4}, {7, 3}, {4, 7}, {5, 6},
    {6, 5}, {7, 4}, {5, 7}, {6, 6}, {7, 5}, {6, 7}, {7, 6}, {7, 7}};

constexpr uint8_t kDiagScanOrder4x4[16][2] = {
    {0, 0}, {0, 1}, {1, 0}, {0, 2}, {1, 1}, {2, 0}, {0, 3}, {1, 2},
    {2, 1}, {3, 0}, {1, 3}, {2, 2}, {3, 1}, {2, 3}, {3, 2}, {3, 3}};

constexpr uint8_t kDiagScanOrder2x2[4][2] = {{0, 0}, {0, 1}, {1, 0}, {1, 1}};

}  // namespace

H266ProfileTierLevel::H266ProfileTierLevel() {
  memset(reinterpret_cast<void*>(this), 0, sizeof(*this));
}

H266VPS::H266VPS() {
  memset(reinterpret_cast<void*>(this), 0, sizeof(*this));
}

H266VPS::~H266VPS() = default;

H266SPS::H266SPS() {
  memset(reinterpret_cast<void*>(this), 0, sizeof(*this));
}

H266SPS::~H266SPS() = default;

H266RefPicListStruct::H266RefPicListStruct() {
  memset(reinterpret_cast<void*>(this), 0, sizeof(*this));
}

H266RefPicLists::H266RefPicLists() {
  memset(reinterpret_cast<void*>(this), 0, sizeof(*this));
}

H266SublayerHrdParameters::H266SublayerHrdParameters() {
  memset(reinterpret_cast<void*>(this), 0, sizeof(*this));
}

H266OlsTimingHrdParameters::H266OlsTimingHrdParameters() {
  memset(reinterpret_cast<void*>(this), 0, sizeof(*this));
}

H266PPS::H266PPS() {
  memset(reinterpret_cast<void*>(this), 0, sizeof(*this));
}

H266APS::H266APS(int aps_type) : aps_params_type(aps_type) {
  memset(reinterpret_cast<void*>(this), 0, sizeof(*this));
  switch (aps_type) {
    case 0:
      data.emplace<H266AlfData>();
      break;
    case 1:
      data.emplace<H266LmcsData>();
      break;
    case 2:
      data.emplace<H266ScalingListData>();
      break;
  }
}

H266APS::~H266APS() = default;

H266ScalingListData::H266ScalingListData() {
  memset(reinterpret_cast<void*>(this), 0, sizeof(*this));
}

H266AlfData::H266AlfData() {
  memset(reinterpret_cast<void*>(this), 0, sizeof(*this));
}

H266LmcsData::H266LmcsData() {
  memset(reinterpret_cast<void*>(this), 0, sizeof(*this));
}

H266PredWeightTable::H266PredWeightTable() {
  memset(reinterpret_cast<void*>(this), 0, sizeof(*this));
}

H266PictureHeader::H266PictureHeader() {
  memset(reinterpret_cast<void*>(this), 0, sizeof(*this));
}

H266SliceHeader::H266SliceHeader() {
  memset(reinterpret_cast<void*>(this), 0, sizeof(*this));
}

H266SliceHeader::~H266SliceHeader() = default;

H266Parser::H266Parser() = default;

H266Parser::~H266Parser() = default;

int H266ProfileTierLevel::MaxLumaPs() const {
  // From Table A.8 - General tier and level limits.
  // |general_level_idc| is major_level * 16 + minor_level * 3
  if (general_level_idc <= 16) {  // level 1
    return 36864;
  }
  if (general_level_idc <= 32) {  // level 2
    return 122880;
  }
  if (general_level_idc <= 35) {  // level 2.1
    return 245760;
  }
  if (general_level_idc <= 48) {  // level 3
    return 552960;
  }
  if (general_level_idc <= 51) {  // level 3.1
    return 983040;
  }
  if (general_level_idc <= 67) {  // level 4, 4.1
    return 2228224;
  }
  if (general_level_idc <= 86) {  // level 5, 5.1, 5.2
    return 8912896;
  }
  if (general_level_idc <= 102) {  // level 6, 6.1, 6.2
    return 35651584;
  }
  // level 6.3 - beyond that there's no actual limit.
  return 80216064;
}

int H266ProfileTierLevel::MaxSlicesPerAu() const {
  // Table A.2
  if (general_level_idc <= 32) {  // level 1, 2
    return 16;
  }
  if (general_level_idc <= 35) {  // level 2.1
    return 20;
  }
  if (general_level_idc <= 48) {  // level 3
    return 30;
  }
  if (general_level_idc <= 51) {  // level 3.1
    return 40;
  }
  if (general_level_idc <= 67) {  // level 4, 4.1
    return 75;
  }
  if (general_level_idc <= 86) {  // level 5, 5.1, 5.2
    return 200;
  }
  if (general_level_idc <= 102) {  // level 6, 6.1, 6.2
    return 600;
  }
  // level 6.3 - beyond that there's no actual limit.
  return 1000;
}

int H266ProfileTierLevel::MaxTilesPerAu() const {
  // Table A.2
  if (general_level_idc <= 35) {  // level 1, 2, 2.1
    return 1;
  }
  if (general_level_idc <= 48) {  // level 3
    return 4;
  }
  if (general_level_idc <= 51) {  // level 3.1
    return 9;
  }
  if (general_level_idc <= 67) {  // level 4, 4.1
    return 25;
  }
  if (general_level_idc <= 86) {  // level 5, 5.1, 5.2
    return 110;
  }
  if (general_level_idc <= 102) {  // level 6, 6.1, 6.2
    return 440;
  }
  // level 6.3 - beyond that there's no actual limit.
  return 990;
}

// Coded size and visible rect here only reflects the largest layer
// picture size in the stream.
gfx::Size H266SPS::GetCodedSize() const {
  return gfx::Size(sps_pic_width_max_in_luma_samples,
                   sps_pic_height_max_in_luma_samples);
}

// This is the stream level visible rect.
gfx::Rect H266SPS::GetVisibleRect() const {
  // 7.4.3.4
  // These are verified in the parser that they won't overflow.
  int left = sps_conf_win_left_offset * sub_width_c;
  int top = sps_conf_win_top_offset * sub_height_c;
  int right = sps_conf_win_right_offset * sub_width_c;
  int bottom = sps_conf_win_bottom_offset * sub_height_c;
  return gfx::Rect(left, top, sps_pic_width_max_in_luma_samples - left - right,
                   sps_pic_height_max_in_luma_samples - top - bottom);
}

// Refer to vui_parameters syntax for more details.
VideoColorSpace H266SPS::GetColorSpace() const {
  if (!vui_parameters.vui_colour_description_present_flag) {
    return VideoColorSpace();
  }

  return VideoColorSpace(vui_parameters.vui_colour_primaries,
                         vui_parameters.vui_transfer_characteristics,
                         vui_parameters.vui_matrix_coeffs,
                         vui_parameters.vui_full_range_flag
                             ? gfx::ColorSpace::RangeID::FULL
                             : gfx::ColorSpace::RangeID::LIMITED);
}

VideoChromaSampling H266SPS::GetChromaSampling() const {
  switch (sps_chroma_format_idc) {
    case 0:
      return VideoChromaSampling::k400;
    case 1:
      return VideoChromaSampling::k420;
    case 2:
      return VideoChromaSampling::k422;
    case 3:
      return VideoChromaSampling::k444;
    default:
      NOTREACHED_IN_MIGRATION();
      return VideoChromaSampling::kUnknown;
  }
}

int H266VPS::GetGeneralLayerIdx(int nuh_layer_id) const {
  auto match = general_layer_idx.find(nuh_layer_id);
  if (match == general_layer_idx.end()) {
    return -1;
  } else {
    return match->second;
  }
}

bool H266SliceHeader::IsISlice() const {
  return sh_slice_type == H266SliceHeader::kSliceTypeI;
}

bool H266SliceHeader::IsPSlice() const {
  return sh_slice_type == H266SliceHeader::kSliceTypeP;
}

bool H266SliceHeader::IsBSlice() const {
  return sh_slice_type == H266SliceHeader::kSliceTypeB;
}

#define READ_BOOL_GENERAL_CONSTRAINT_INFO(a) \
  READ_BOOL_OR_RETURN(&(profile_tier_level->general_constraints_info.a))
#define READ_BITS_GENERAL_CONSTRAINT_INFO(bits, a) \
  READ_BITS_OR_RETURN((bits), &(profile_tier_level->general_constraints_info.a))
H266Parser::Result H266Parser::ParseProfileTierLevel(
    bool profile_tier_present,
    int max_num_sub_layers_minus1,
    H266ProfileTierLevel* profile_tier_level) {
  // 7.3.3.1: General profile, tier, and level syntax
  DVLOG(4) << "Parsing profile_tier_level.";

  if (profile_tier_present) {
    READ_BITS_OR_RETURN(7, &profile_tier_level->general_profile_idc);
    READ_BOOL_OR_RETURN(&profile_tier_level->general_tier_flag);
  }

  READ_BITS_OR_RETURN(8, &profile_tier_level->general_level_idc);
  READ_BOOL_OR_RETURN(&profile_tier_level->ptl_frame_only_constraint_flag);
  READ_BOOL_OR_RETURN(&profile_tier_level->ptl_multilayer_enabled_flag);
  if (profile_tier_present) {
    // 7.3.3.2: General constraints information syntax.
    READ_BOOL_OR_RETURN(
        &profile_tier_level->general_constraints_info.gci_present_flag);

    if (profile_tier_level->general_constraints_info.gci_present_flag) {
      READ_BOOL_GENERAL_CONSTRAINT_INFO(gci_intra_only_constraint_flag);
      READ_BOOL_GENERAL_CONSTRAINT_INFO(
          gci_all_layers_independent_constraint_flag);
      READ_BOOL_GENERAL_CONSTRAINT_INFO(gci_one_au_only_constraint_flag);

      READ_BITS_GENERAL_CONSTRAINT_INFO(
          4, gci_sixteen_minus_max_bitdepth_constraint_idc);
      IN_RANGE_OR_RETURN(profile_tier_level->general_constraints_info
                             .gci_sixteen_minus_max_bitdepth_constraint_idc,
                         0, 8);
      READ_BITS_GENERAL_CONSTRAINT_INFO(
          2, gci_three_minus_max_chroma_format_constraint_idc);

      READ_BOOL_GENERAL_CONSTRAINT_INFO(
          gci_no_mixed_nalu_types_in_pic_constraint_flag);
      READ_BOOL_GENERAL_CONSTRAINT_INFO(gci_no_trail_constraint_flag);
      READ_BOOL_GENERAL_CONSTRAINT_INFO(gci_no_stsa_constraint_flag);
      READ_BOOL_GENERAL_CONSTRAINT_INFO(gci_no_rasl_constraint_flag);
      READ_BOOL_GENERAL_CONSTRAINT_INFO(gci_no_radl_constraint_flag);
      READ_BOOL_GENERAL_CONSTRAINT_INFO(gci_no_idr_constraint_flag);
      READ_BOOL_GENERAL_CONSTRAINT_INFO(gci_no_cra_constraint_flag);
      READ_BOOL_GENERAL_CONSTRAINT_INFO(gci_no_gdr_constraint_flag);
      READ_BOOL_GENERAL_CONSTRAINT_INFO(gci_no_aps_constraint_flag);
      READ_BOOL_GENERAL_CONSTRAINT_INFO(gci_no_idr_rpl_constraint_flag);

      READ_BOOL_GENERAL_CONSTRAINT_INFO(gci_one_tile_per_pic_constraint_flag);
      READ_BOOL_GENERAL_CONSTRAINT_INFO(
          gci_pic_header_in_slice_header_constraint_flag);
      READ_BOOL_GENERAL_CONSTRAINT_INFO(gci_one_slice_per_pic_constraint_flag);
      READ_BOOL_GENERAL_CONSTRAINT_INFO(
          gci_no_rectangular_slice_constraint_flag);
      READ_BOOL_GENERAL_CONSTRAINT_INFO(
          gci_one_slice_per_subpic_constraint_flag);
      READ_BOOL_GENERAL_CONSTRAINT_INFO(gci_no_subpic_info_constraint_flag);

      READ_BITS_GENERAL_CONSTRAINT_INFO(
          2, gci_three_minus_max_log2_ctu_size_constraint_idc);
      READ_BOOL_GENERAL_CONSTRAINT_INFO(
          gci_no_partition_constraints_override_constraint_flag);
      READ_BOOL_GENERAL_CONSTRAINT_INFO(gci_no_mtt_constraint_flag);
      READ_BOOL_GENERAL_CONSTRAINT_INFO(
          gci_no_qtbtt_dual_tree_intra_constraint_flag);

      READ_BOOL_GENERAL_CONSTRAINT_INFO(gci_no_palette_constraint_flag);
      READ_BOOL_GENERAL_CONSTRAINT_INFO(gci_no_ibc_constraint_flag);
      READ_BOOL_GENERAL_CONSTRAINT_INFO(gci_no_isp_constraint_flag);
      READ_BOOL_GENERAL_CONSTRAINT_INFO(gci_no_mrl_constraint_flag);
      READ_BOOL_GENERAL_CONSTRAINT_INFO(gci_no_mip_constraint_flag);
      READ_BOOL_GENERAL_CONSTRAINT_INFO(gci_no_cclm_constraint_flag);

      READ_BOOL_GENERAL_CONSTRAINT_INFO(
          gci_no_ref_pic_resampling_constraint_flag);
      READ_BOOL_GENERAL_CONSTRAINT_INFO(
          gci_no_res_change_in_clvs_constraint_flag);
      READ_BOOL_GENERAL_CONSTRAINT_INFO(
          gci_no_weighted_prediction_constraint_flag);
      READ_BOOL_GENERAL_CONSTRAINT_INFO(gci_no_ref_wraparound_constraint_flag);
      READ_BOOL_GENERAL_CONSTRAINT_INFO(gci_no_temporal_mvp_constraint_flag);
      READ_BOOL_GENERAL_CONSTRAINT_INFO(gci_no_sbtmvp_constraint_flag);
      READ_BOOL_GENERAL_CONSTRAINT_INFO(gci_no_amvr_constraint_flag);
      READ_BOOL_GENERAL_CONSTRAINT_INFO(gci_no_bdof_constraint_flag);
      READ_BOOL_GENERAL_CONSTRAINT_INFO(gci_no_smvd_constraint_flag);
      READ_BOOL_GENERAL_CONSTRAINT_INFO(gci_no_dmvr_constraint_flag);
      READ_BOOL_GENERAL_CONSTRAINT_INFO(gci_no_mmvd_constraint_flag);
      READ_BOOL_GENERAL_CONSTRAINT_INFO(gci_no_affine_motion_constraint_flag);
      READ_BOOL_GENERAL_CONSTRAINT_INFO(gci_no_prof_constraint_flag);
      READ_BOOL_GENERAL_CONSTRAINT_INFO(gci_no_bcw_constraint_flag);
      READ_BOOL_GENERAL_CONSTRAINT_INFO(gci_no_ciip_constraint_flag);
      READ_BOOL_GENERAL_CONSTRAINT_INFO(gci_no_gpm_constraint_flag);

      READ_BOOL_GENERAL_CONSTRAINT_INFO(
          gci_no_luma_transform_size_64_constraint_flag);
      READ_BOOL_GENERAL_CONSTRAINT_INFO(gci_no_transform_skip_constraint_flag);
      READ_BOOL_GENERAL_CONSTRAINT_INFO(gci_no_bdpcm_constraint_flag);
      READ_BOOL_GENERAL_CONSTRAINT_INFO(gci_no_mts_constraint_flag);
      READ_BOOL_GENERAL_CONSTRAINT_INFO(gci_no_lfnst_constraint_flag);
      READ_BOOL_GENERAL_CONSTRAINT_INFO(gci_no_joint_cbcr_constraint_flag);
      READ_BOOL_GENERAL_CONSTRAINT_INFO(gci_no_sbt_constraint_flag);
      READ_BOOL_GENERAL_CONSTRAINT_INFO(gci_no_act_constraint_flag);
      READ_BOOL_GENERAL_CONSTRAINT_INFO(
          gci_no_explicit_scaling_list_constraint_flag);
      READ_BOOL_GENERAL_CONSTRAINT_INFO(gci_no_dep_quant_constraint_flag);
      READ_BOOL_GENERAL_CONSTRAINT_INFO(
          gci_no_sign_data_hiding_constraint_flag);
      READ_BOOL_GENERAL_CONSTRAINT_INFO(gci_no_cu_qp_delta_constraint_flag);
      READ_BOOL_GENERAL_CONSTRAINT_INFO(
          gci_no_chroma_qp_offset_constraint_flag);

      READ_BOOL_GENERAL_CONSTRAINT_INFO(gci_no_sao_constraint_flag);
      READ_BOOL_GENERAL_CONSTRAINT_INFO(gci_no_alf_constraint_flag);
      READ_BOOL_GENERAL_CONSTRAINT_INFO(gci_no_ccalf_constraint_flag);
      READ_BOOL_GENERAL_CONSTRAINT_INFO(gci_no_lmcs_constraint_flag);
      READ_BOOL_GENERAL_CONSTRAINT_INFO(gci_no_ladf_constraint_flag);
      READ_BOOL_GENERAL_CONSTRAINT_INFO(
          gci_no_virtual_boundaries_constraint_flag);
      READ_BOOL_GENERAL_CONSTRAINT_INFO(gci_num_additional_bits);
      int num_additional_bits_used = 0;
      int num_additional_bits =
          profile_tier_level->general_constraints_info.gci_num_additional_bits;
      if (num_additional_bits > 5) {
        READ_BOOL_GENERAL_CONSTRAINT_INFO(gci_all_rap_pictures_constraint_flag);
        READ_BOOL_GENERAL_CONSTRAINT_INFO(
            gci_no_extended_precision_processing_constraint_flag);
        READ_BOOL_GENERAL_CONSTRAINT_INFO(
            gci_no_ts_residual_coding_rice_constraint_flag);
        READ_BOOL_GENERAL_CONSTRAINT_INFO(
            gci_no_rrc_rice_extension_constraint_flag);
        READ_BOOL_GENERAL_CONSTRAINT_INFO(
            gci_no_persistent_rice_adaptation_constraint_flag);
        READ_BOOL_GENERAL_CONSTRAINT_INFO(
            gci_no_reverse_last_sig_coeff_constraint_flag);
        num_additional_bits_used = 6;
      }

      for (int i = 0; i < num_additional_bits - num_additional_bits_used; i++) {
        SKIP_BITS_OR_RETURN(1);
      }
    }
    BYTE_ALIGNMENT();
  }

  for (int i = max_num_sub_layers_minus1 - 1; i >= 0; i--) {
    READ_BOOL_OR_RETURN(
        &profile_tier_level->ptl_sublayer_level_present_flag[i]);
  }
  BYTE_ALIGNMENT();

  // sublayer_level_idc[MaxNumSubLayersMinus1] is inferred to be equal to
  // general_level_idc of the same profile_tier_level() structure.
  profile_tier_level->sub_layer_level_idc[max_num_sub_layers_minus1] =
      profile_tier_level->general_level_idc;
  for (int i = max_num_sub_layers_minus1 - 1; i >= 0; i--) {
    if (profile_tier_level->ptl_sublayer_level_present_flag[i]) {
      READ_BITS_OR_RETURN(8, &profile_tier_level->sub_layer_level_idc[i]);
    } else {
      profile_tier_level->sub_layer_level_idc[i] =
          profile_tier_level->sub_layer_level_idc[i + 1];
    }
  }

  if (profile_tier_present) {
    READ_BITS_OR_RETURN(8, &profile_tier_level->ptl_num_sub_profiles);
    IN_RANGE_OR_RETURN(profile_tier_level->ptl_num_sub_profiles, 0,
                       kMaxSubProfiles);
    for (int i = 0; i < profile_tier_level->ptl_num_sub_profiles; i++) {
      // Reader does not support more than 31-bits, so we have to read twice.
      int general_sub_profile_idc_msb = 0, general_sub_profile_idc_lsb;
      READ_BITS_OR_RETURN(16, &general_sub_profile_idc_msb);
      READ_BITS_OR_RETURN(16, &general_sub_profile_idc_lsb);
      profile_tier_level->general_sub_profiles_idc[i] =
          (general_sub_profile_idc_msb << 16) + general_sub_profile_idc_lsb;
    }
  }

  return kOk;
}
#undef READ_BITS_GENERAL_CONSTRAINT_INFO
#undef READ_BOOL_GENERAL_CONSTRAINT_INFO

H266Parser::Result H266Parser::ParseDpbParameters(
    int max_sublayers_minus1,
    bool sublayer_info_flag,
    H266DPBParameters* dpb_parameters) {
  DCHECK(dpb_parameters);

  for (int i = (sublayer_info_flag ? 0 : max_sublayers_minus1);
       i <= max_sublayers_minus1; i++) {
    READ_UE_OR_RETURN(&dpb_parameters->dpb_max_dec_pic_buffering_minus1[i]);
    // When parsing VPS, there is no information about maximum dpb size, so
    // using the upper bound 16 for range check.
    IN_RANGE_OR_RETURN(dpb_parameters->dpb_max_dec_pic_buffering_minus1[i], 0,
                       kMaxDpbPicBuffer * 2);
    READ_UE_OR_RETURN(&dpb_parameters->dpb_max_num_reorder_pics[i]);
    IN_RANGE_OR_RETURN(dpb_parameters->dpb_max_num_reorder_pics[i], 0,
                       dpb_parameters->dpb_max_dec_pic_buffering_minus1[i]);
    if (i > 0) {
      GT_OR_RETURN(dpb_parameters->dpb_max_num_reorder_pics[i],
                   dpb_parameters->dpb_max_num_reorder_pics[i - 1]);
    }
    READ_UE_OR_RETURN(&dpb_parameters->dpb_max_latency_increase_plus1[i]);
  }
  return kOk;
}

H266Parser::Result H266Parser::ParseVuiPayload(int payload_size,
                                               const H266SPS& sps,
                                               H266VUIParameters* vui) {
  DCHECK(vui);

  int start_remain_size = br_.NumBitsLeft();
  H266Parser::Result res = ParseVuiParameters(sps, vui);
  if (res != kOk) {
    DVLOG(1) << "Failed to parse VUI param.";
    return res;
  }
  int bits_unread = payload_size * 8 - (start_remain_size - br_.NumBitsLeft());
  // Skip the VUI extension data till sei_payload_bit_equal_to_one
  while (bits_unread > 0) {
    SKIP_BITS_OR_RETURN(1);
    bits_unread--;
  }

  BYTE_ALIGNMENT();
  return kOk;
}

// ITU-T H.274|ISO/IEC 23002-7 VUI parameters
H266Parser::Result H266Parser::ParseVuiParameters(const H266SPS& sps,
                                                  H266VUIParameters* vui) {
  DCHECK(vui);

  READ_BOOL_OR_RETURN(&vui->vui_progressive_source_flag);
  READ_BOOL_OR_RETURN(&vui->vui_interlaced_source_flag);
  READ_BOOL_OR_RETURN(&vui->vui_non_packed_constraint_flag);
  READ_BOOL_OR_RETURN(&vui->vui_non_projected_constraint_flag);
  READ_BOOL_OR_RETURN(&vui->vui_aspect_ratio_info_present_flag);
  if (vui->vui_aspect_ratio_info_present_flag) {
    READ_BOOL_OR_RETURN(&vui->vui_aspect_ratio_constant_flag);
    READ_BITS_OR_RETURN(8, &vui->vui_aspect_ratio_idc);
    if (vui->vui_aspect_ratio_idc == 255) {
      READ_BITS_OR_RETURN(16, &vui->vui_sar_width);
      READ_BITS_OR_RETURN(16, &vui->vui_sar_height);
    }
  }

  READ_BOOL_OR_RETURN(&vui->vui_overscan_info_present_flag);
  if (vui->vui_overscan_info_present_flag) {
    READ_BOOL_OR_RETURN(&vui->vui_overscan_appropriate_flag);
  }
  READ_BOOL_OR_RETURN(&vui->vui_colour_description_present_flag);
  if (vui->vui_colour_description_present_flag) {
    READ_BITS_OR_RETURN(8, &vui->vui_colour_primaries);
    READ_BITS_OR_RETURN(8, &vui->vui_transfer_characteristics);
    READ_BITS_OR_RETURN(8, &vui->vui_matrix_coeffs);
    READ_BOOL_OR_RETURN(&vui->vui_full_range_flag);
  } else {
    vui->vui_colour_primaries = 2;
    vui->vui_transfer_characteristics = 2;
    vui->vui_matrix_coeffs = 2;
    vui->vui_full_range_flag = 0;
  }
  READ_BOOL_OR_RETURN(&vui->vui_chroma_loc_info_present_flag);
  if (sps.sps_chroma_format_idc != 1 && vui->vui_chroma_loc_info_present_flag) {
    return kInvalidStream;
  }
  if (vui->vui_chroma_loc_info_present_flag) {
    if (vui->vui_progressive_source_flag && !vui->vui_interlaced_source_flag) {
      READ_UE_OR_RETURN(&vui->vui_chroma_sample_loc_type_frame);
      IN_RANGE_OR_RETURN(vui->vui_chroma_sample_loc_type_frame, 0, 6);
    } else {
      READ_UE_OR_RETURN(&vui->vui_chroma_sample_loc_type_top_field);
      IN_RANGE_OR_RETURN(vui->vui_chroma_sample_loc_type_top_field, 0, 6);
      READ_UE_OR_RETURN(&vui->vui_chroma_sample_loc_type_bottom_field);
      IN_RANGE_OR_RETURN(vui->vui_chroma_sample_loc_type_bottom_field, 0, 6);
    }
  } else {
    if (sps.sps_chroma_format_idc == 1) {
      vui->vui_chroma_sample_loc_type_frame = 6;
      vui->vui_chroma_sample_loc_type_top_field = 6;
      vui->vui_chroma_sample_loc_type_bottom_field = 6;
    }
  }

  return kOk;
}

// 7.3.2.3 Video parameter set.
// Provides overall information of a bitstream, mainly about
// number of layers/sublayers, dependency among layers, number of
// OLSs(output layer sets), PTL of OPs(temporal subset of an OLS),
// DBP and HRD parameters.
// VPS is only used for multi-layer bistreams and decoders can ignore
// them for single-layer stream. Unlike HEVC, for single-layer bitsream,
// it is allowed no VPS is present in the bitstream(when SPS is referring
// to VPS id 0).
H266Parser::Result H266Parser::ParseVPS(int* vps_id) {
  DVLOG(4) << "Parsing VPS";
  Result res = kOk;
  DCHECK(vps_id);

  *vps_id = -1;
  std::unique_ptr<H266VPS> vps = std::make_unique<H266VPS>();
  READ_BITS_OR_RETURN(4, &vps->vps_video_parameter_set_id);
  GT_OR_RETURN(vps->vps_video_parameter_set_id, 0);
  READ_BITS_OR_RETURN(6, &vps->vps_max_layers_minus1);
  IN_RANGE_OR_RETURN(vps->vps_max_layers_minus1, 0, 64);
  READ_BITS_OR_RETURN(3, &vps->vps_max_sublayers_minus1);
  IN_RANGE_OR_RETURN(vps->vps_max_sublayers_minus1, 0, 6);

  // Inferred value of vps_default_ptl_dpb_hrd_max_tid_flag and
  // vps_all_independent_layers_flags are both 1 if not present.
  vps->vps_default_ptl_dpb_hrd_max_tid_flag = 1;
  vps->vps_all_independent_layers_flags = 1;
  if (vps->vps_max_layers_minus1 > 0) {
    if (vps->vps_max_sublayers_minus1 > 0) {
      READ_BOOL_OR_RETURN(&vps->vps_default_ptl_dpb_hrd_max_tid_flag);
    }
    READ_BOOL_OR_RETURN(&vps->vps_all_independent_layers_flags);
  }

  for (int i = 0; i <= vps->vps_max_layers_minus1; i++) {
    READ_BITS_OR_RETURN(6, &vps->vps_layer_id[i]);
    if (i > 0) {
      GT_OR_RETURN(vps->vps_layer_id[i], vps->vps_layer_id[i - 1]);
    }
    if (i > 0 && !vps->vps_all_independent_layers_flags) {
      READ_BOOL_OR_RETURN(&vps->vps_independent_layer_flag[i]);
      if (!vps->vps_independent_layer_flag[i]) {
        READ_BOOL_OR_RETURN(&vps->vps_max_tid_ref_present_flag[i]);
        for (int j = 0; j < i; j++) {
          READ_BOOL_OR_RETURN(&vps->vps_direct_ref_layer_flag[i][j]);
          if (vps->vps_max_tid_ref_present_flag[i] &&
              vps->vps_direct_ref_layer_flag[i][j]) {
            READ_BITS_OR_RETURN(3, &vps->vps_max_tid_il_ref_pics_plus1[i][j]);
          } else {
            // 7.4.3.3: inferred value when not present.
            vps->vps_max_tid_il_ref_pics_plus1[i][j] =
                vps->vps_max_sublayers_minus1 + 1;
          }
        }
      }
    }
  }

  // Equation 29. This maps layer id to nuh_layer_id.
  for (int i = 0; i <= vps->vps_max_layers_minus1; i++) {
    vps->general_layer_idx[vps->vps_layer_id[i]] = i;
  }
  int ols_mode_idc = 0;
  int total_num_olss = 0;
  if (vps->vps_max_layers_minus1 > 0) {
    if (vps->vps_all_independent_layers_flags) {
      READ_BOOL_OR_RETURN(&vps->vps_each_layer_is_an_ols_flag);
    }
    if (!vps->vps_each_layer_is_an_ols_flag) {
      if (!vps->vps_all_independent_layers_flags) {
        READ_BITS_OR_RETURN(2, &vps->vps_ols_mode_idc);
      } else {
        vps->vps_ols_mode_idc = 2;
      }
      // Caution!!! vps_ols_mode_idc might be inferred to 2 instead of read
      // from bitstream.
      if (vps->vps_ols_mode_idc == 2) {
        READ_BITS_OR_RETURN(8, &vps->vps_num_output_layer_sets_minus2);
        for (int i = 1; i <= vps->vps_num_output_layer_sets_minus2 + 1; i++) {
          for (int j = 0; j <= vps->vps_max_layers_minus1; j++) {
            READ_BOOL_OR_RETURN(&vps->vps_ols_output_layer_flag[i][j]);
          }
        }
      }
      // Equation 30: ols_mode_idc derivation.
      ols_mode_idc = vps->vps_ols_mode_idc;
    } else {
      ols_mode_idc = 4;
    }

    // Equation 31: total_num_olss derivation.
    if (ols_mode_idc == 0 || ols_mode_idc == 1 || ols_mode_idc == 4) {
      total_num_olss = vps->vps_max_layers_minus1 + 1;
    } else {
      total_num_olss = vps->vps_num_output_layer_sets_minus2 + 2;
    }
    READ_BITS_OR_RETURN(8, &vps->vps_num_ptls_minus1);
    IN_RANGE_OR_RETURN(vps->vps_num_ptls_minus1, 0, total_num_olss - 1);
  } else {
    vps->vps_each_layer_is_an_ols_flag = 1;
    vps->vps_num_ptls_minus1 = 0;
  }

  for (int i = 0; i <= vps->vps_num_ptls_minus1; i++) {
    if (i > 0) {
      READ_BOOL_OR_RETURN(&vps->vps_pt_present_flag[i]);
    } else {
      vps->vps_pt_present_flag[i] = 1;
    }
    if (!vps->vps_default_ptl_dpb_hrd_max_tid_flag) {
      READ_BITS_OR_RETURN(3, &vps->vps_ptl_max_tid[i]);
      IN_RANGE_OR_RETURN(vps->vps_ptl_max_tid[i], 0,
                         vps->vps_max_sublayers_minus1);
    } else {
      vps->vps_ptl_max_tid[i] = vps->vps_max_sublayers_minus1;
    }
  }

  BYTE_ALIGNMENT();

  // Read profile-tier-level info in VPS.
  for (int i = 0; i <= vps->vps_num_ptls_minus1; i++) {
    ParseProfileTierLevel(vps->vps_pt_present_flag[i], vps->vps_ptl_max_tid[i],
                          &vps->profile_tier_level[i]);
    if (i > 0 && !vps->vps_pt_present_flag[i]) {
      // Section 7.4.3.3, vps_pt_present_flag[i] equal to 0, the profile/tier
      // and general constraints information are copied from the i-1 the
      // profile_tier_level() syntax structure in the VPS. (This should include
      // the sub_profile_idc info.)
      vps->profile_tier_level[i].general_profile_idc =
          vps->profile_tier_level[i - 1].general_profile_idc;
      vps->profile_tier_level[i].general_tier_flag =
          vps->profile_tier_level[i - 1].general_tier_flag;
      memcpy(&vps->profile_tier_level[i].general_constraints_info,
             &vps->profile_tier_level[i - 1].general_constraints_info,
             sizeof(vps->profile_tier_level[i - 1].general_constraints_info));
      LE_OR_RETURN(vps->profile_tier_level[i - 1].ptl_num_sub_profiles,
                   kMaxSubProfiles);
      vps->profile_tier_level[i].ptl_num_sub_profiles =
          vps->profile_tier_level[i - 1].ptl_num_sub_profiles;
      memcpy(
          &vps->profile_tier_level[i].general_sub_profiles_idc[0],
          &vps->profile_tier_level[i - 1].general_sub_profiles_idc[0],
          vps->profile_tier_level[i - 1].ptl_num_sub_profiles *
              sizeof(
                  vps->profile_tier_level[i - 1].general_sub_profiles_idc[0]));
    }
  }

  for (int i = 0; i < total_num_olss; i++) {
    if (vps->vps_num_ptls_minus1 > 0 &&
        vps->vps_num_ptls_minus1 + 1 != total_num_olss) {
      READ_BITS_OR_RETURN(8, &vps->vps_ols_ptl_idx[i]);
      IN_RANGE_OR_RETURN(vps->vps_ols_ptl_idx[i], 0, vps->vps_num_ptls_minus1);
    } else if (vps->vps_num_ptls_minus1 == 0) {
      vps->vps_ols_ptl_idx[i] = 0;
    } else {
      // vps->vps_num_ptls_minus1 > 0 && vps->vps_num_ptls_minus1 + 1 ==
      // total_num_olss
      vps->vps_ols_ptl_idx[i] = i;
    }
  }

  // Equation 28. Ideally we should define vps_direct_ref_layer_flag to be of
  // size vps_max_layers_minus1 * (vps_max_layers_minus1 - 1). However the
  // equation defined in spec requires it to be of size vps_max_layers_minus1 *
  // vps_max_layers_minus1.
  for (int i = 0; i <= vps->vps_max_layers_minus1; i++) {
    for (int j = 0; j <= vps->vps_max_layers_minus1; j++) {
      vps->dependency_flag[i][j] = vps->vps_direct_ref_layer_flag[i][j];
      for (int k = 0; k < i; k++) {
        if (vps->vps_direct_ref_layer_flag[i][k] &&
            vps->dependency_flag[k][j]) {
          vps->dependency_flag[i][j] = 1;
        }
      }
    }
    vps->layer_used_as_ref_layer_flag[i] = 0;
  }
  for (int i = 0; i <= vps->vps_max_layers_minus1; i++) {
    int j, d, r;
    for (j = 0, d = 0, r = 0; j <= vps->vps_max_layers_minus1; j++) {
      if (vps->vps_direct_ref_layer_flag[i][j]) {
        vps->direct_ref_layer_idx[i][d++] = j;
        vps->layer_used_as_ref_layer_flag[j] = 1;
      }
      if (vps->dependency_flag[i][j]) {
        vps->reference_layer_idx[i][r++] = j;
      }
    }
    vps->num_direct_ref_layers[i] = d;
    vps->num_ref_layers[i] = r;
  }

  // Equation 32: calculation of
  // num_output_layers_in_ols/num_sublayers_in_layer_in_ols, etc.
  int num_output_layers_in_ols[kMaxTotalNumOLSs];
  int num_sublayers_in_layer_in_ols[kMaxTotalNumOLSs][kMaxTotalNumOLSs];
  int output_layer_id_in_ols[kMaxTotalNumOLSs][kMaxTotalNumOLSs];
  bool layer_used_as_output_flag[kMaxSubLayers];
  bool layer_included_in_ols_flag[kMaxTotalNumOLSs][kMaxLayers];
  int output_layer_idx[kMaxTotalNumOLSs][kMaxLayers];
  num_output_layers_in_ols[0] = 1;
  output_layer_id_in_ols[0][0] = vps->vps_layer_id[0];
  num_sublayers_in_layer_in_ols[0][0] =
      vps->vps_ptl_max_tid[vps->vps_ols_ptl_idx[0]] + 1;
  for (int i = 1; i <= vps->vps_max_layers_minus1; i++) {
    if (ols_mode_idc == 4 || ols_mode_idc < 2) {
      layer_used_as_output_flag[i] = 1;
    } else if (vps->vps_ols_mode_idc == 2) {
      layer_used_as_output_flag[i] = 0;
    }
  }
  for (int i = 1; i < total_num_olss; i++) {
    if (ols_mode_idc == 4 || ols_mode_idc == 0) {
      num_output_layers_in_ols[i] = 1;
      output_layer_id_in_ols[i][0] = vps->vps_layer_id[i];
      if (vps->vps_each_layer_is_an_ols_flag) {
        num_sublayers_in_layer_in_ols[i][0] =
            vps->vps_ptl_max_tid[vps->vps_ols_ptl_idx[i]] + 1;
      } else {
        num_sublayers_in_layer_in_ols[i][i] =
            vps->vps_ptl_max_tid[vps->vps_ols_ptl_idx[i]] + 1;
        for (int k = i - 1; k >= 0; k--) {
          num_sublayers_in_layer_in_ols[i][i] = 0;
          for (int m = k + 1; m <= i; m++) {
            int max_sublayer_needed =
                std::min(num_sublayers_in_layer_in_ols[i][m],
                         vps->vps_max_tid_il_ref_pics_plus1[m][k]);
            if (vps->vps_direct_ref_layer_flag[m][k] &&
                num_sublayers_in_layer_in_ols[i][k] < max_sublayer_needed) {
              num_sublayers_in_layer_in_ols[i][k] = max_sublayer_needed;
            }
          }
        }
      }
    } else if (vps->vps_ols_mode_idc == 1) {
      num_output_layers_in_ols[i] = i + 1;
      for (int j = 0; j < num_output_layers_in_ols[i]; j++) {
        output_layer_id_in_ols[i][j] = vps->vps_layer_id[j];
        num_sublayers_in_layer_in_ols[i][j] =
            vps->vps_ptl_max_tid[vps->vps_ols_ptl_idx[i]] + 1;
      }
    } else if (vps->vps_ols_mode_idc == 2) {
      int j, k;
      for (j = 0; j <= vps->vps_max_layers_minus1; j++) {
        layer_included_in_ols_flag[i][j] = 0;
        num_sublayers_in_layer_in_ols[i][j] = 0;
      }
      int highest_included_layer = 0;
      for (k = 0, j = 0; k <= vps->vps_max_layers_minus1; k++) {
        if (vps->vps_ols_output_layer_flag[i][k]) {
          layer_included_in_ols_flag[i][k] = 1;
          highest_included_layer = k;
          layer_used_as_output_flag[k] = 1;
          output_layer_idx[i][j] = k;
          output_layer_id_in_ols[i][j++] = vps->vps_layer_id[k];
          num_sublayers_in_layer_in_ols[i][k] =
              vps->vps_ptl_max_tid[vps->vps_ols_ptl_idx[i]] + 1;
        }
      }
      num_output_layers_in_ols[i] = j;
      for (j = 0; j < num_output_layers_in_ols[i]; j++) {
        int idx = output_layer_idx[i][j];
        for (k = 0; k < vps->num_ref_layers[idx]; k++) {
          if (!layer_included_in_ols_flag[i]
                                         [vps->reference_layer_idx[idx][k]]) {
            layer_included_in_ols_flag[i][vps->reference_layer_idx[idx][k]] = 1;
          }
        }
        for (k = highest_included_layer - 1; k >= 0; k--) {
          if (layer_included_in_ols_flag[i][k] &&
              !vps->vps_ols_output_layer_flag[i][k]) {
            for (int m = k + 1; m <= highest_included_layer; m++) {
              int max_sublayer_needed =
                  std::min(num_sublayers_in_layer_in_ols[i][m],
                           vps->vps_max_tid_il_ref_pics_plus1[m][k]);
              if (vps->vps_direct_ref_layer_flag[m][k] &&
                  layer_included_in_ols_flag[i][m] &&
                  num_sublayers_in_layer_in_ols[i][k] < max_sublayer_needed) {
                num_sublayers_in_layer_in_ols[i][k] = max_sublayer_needed;
              }
            }
          }
        }
      }
    }
  }

  // Equation 33: num_layers_in_ols/layer_id_in_ols, etc.
  int num_layers_in_ols[kMaxTotalNumOLSs];
  int layer_id_in_ols[kMaxTotalNumOLSs][kMaxLayers];
  int num_muliti_layer_olss = 0;
  int multi_layer_ols_idx[kMaxTotalNumOLSs];
  num_layers_in_ols[0] = 1;
  layer_id_in_ols[0][0] = vps->vps_layer_id[0];
  for (int i = 1; i < total_num_olss; i++) {
    if (vps->vps_each_layer_is_an_ols_flag) {
      num_layers_in_ols[i] = 1;
      layer_id_in_ols[i][0] = vps->vps_layer_id[i];
    } else if (vps->vps_ols_mode_idc == 0 || vps->vps_ols_mode_idc == 1) {
      num_layers_in_ols[i] = i + 1;
      for (int j = 0; j < num_layers_in_ols[i]; j++) {
        layer_id_in_ols[i][j] = vps->vps_layer_id[j];
      }
    } else if (vps->vps_ols_mode_idc == 2) {
      for (int k = 0, j = 0; k <= vps->vps_max_layers_minus1; k++) {
        if (layer_included_in_ols_flag[i][k]) {
          layer_id_in_ols[i][j++] = vps->vps_layer_id[k];
        }
        num_layers_in_ols[i] = j;
      }
    }
    if (num_layers_in_ols[i] > 1) {
      multi_layer_ols_idx[i] = num_muliti_layer_olss;
      num_muliti_layer_olss++;
    }
  }

  int vps_num_dbp_params = 0;
  if (!vps->vps_each_layer_is_an_ols_flag) {
    READ_UE_OR_RETURN(&vps->vps_num_dpb_params_minus1);
    IN_RANGE_OR_RETURN(vps->vps_num_dpb_params_minus1, 0,
                       num_muliti_layer_olss - 1);

    // Equation 34: Variable VpsNumDpbParams that specifies number
    // of dpb_parameters() syntax structures in the VPS.
    vps_num_dbp_params = vps->vps_num_dpb_params_minus1 + 1;
    if (vps->vps_max_sublayers_minus1 > 0) {
      READ_BOOL_OR_RETURN(&vps->vps_sublayer_dpb_params_present_flag);
    }
    for (int i = 0; i < vps_num_dbp_params; i++) {
      if (!vps->vps_default_ptl_dpb_hrd_max_tid_flag) {
        READ_BITS_OR_RETURN(3, &vps->vps_dpb_max_tid[i]);
      } else {
        vps->vps_dpb_max_tid[i] = vps->vps_max_sublayers_minus1;
      }
      ParseDpbParameters(vps->vps_dpb_max_tid[i],
                         vps->vps_sublayer_dpb_params_present_flag,
                         &vps->dpb_parameters[i]);
    }
    for (int i = 0; i < num_muliti_layer_olss; i++) {
      READ_UE_OR_RETURN(&vps->vps_ols_dpb_pic_width[i]);
      READ_UE_OR_RETURN(&vps->vps_ols_dpb_pic_height[i]);
      READ_BITS_OR_RETURN(2, &vps->vps_ols_dpb_chroma_format[i]);
      READ_UE_OR_RETURN(&vps->vps_ols_dpb_bitdepth_minus8[i]);
      IN_RANGE_OR_RETURN(vps->vps_ols_dpb_bitdepth_minus8[i], 0, 8);
      if (vps_num_dbp_params > 1 &&
          vps_num_dbp_params != num_muliti_layer_olss) {
        READ_UE_OR_RETURN(&vps->vps_ols_dpb_params_idx[i]);
        IN_RANGE_OR_RETURN(vps->vps_ols_dpb_params_idx[i], 0,
                           vps_num_dbp_params - 1);
      } else if (vps_num_dbp_params == 1) {
        vps->vps_ols_dpb_params_idx[i] = 0;
      } else {
        vps->vps_ols_dpb_params_idx[i] = i;
      }
    }
    READ_BOOL_OR_RETURN(&vps->vps_timing_hrd_params_present_flag);
    // We stop here without parsing remaining syntax elements.
  }

  // If a VPS with the same id already exists, replace it.
  *vps_id = vps->vps_video_parameter_set_id;
  active_vps_[*vps_id] = std::move(vps);
  return res;
}

// 7.3.10
H266Parser::Result H266Parser::ParseRefPicListStruct(
    int list_idx,
    int rpl_idx,
    const H266SPS& sps,
    H266RefPicListStruct* ref_pic_list_struct) {
  DCHECK(ref_pic_list_struct);

  const H266VPS* vps = GetVPS(sps.sps_video_parameter_set_id);
  if (!vps) {
    DVLOG(1) << "Invalid VPS.";
    return kMissingParameterSet;
  }

  ref_pic_list_struct->num_ltrp_entries = 0;
  int abs_delta_poc_st = 0;

  READ_UE_OR_RETURN(&ref_pic_list_struct->num_ref_entries);
  IN_RANGE_OR_RETURN(ref_pic_list_struct->num_ref_entries, 0,
                     sps.max_dpb_size + 13);
  // When ltrp_in_header_flag is 1, it means we do not signal
  // the LTR here, but instead it exists in the ref_pic_lists() syntax
  // in picture header or slice header.
  if (sps.sps_long_term_ref_pics_flag &&
      rpl_idx < sps.sps_num_ref_pic_lists[list_idx] &&
      ref_pic_list_struct->num_ref_entries > 0) {
    READ_BOOL_OR_RETURN(&ref_pic_list_struct->ltrp_in_header_flag);
  } else if (sps.sps_long_term_ref_pics_flag &&
             rpl_idx == sps.sps_num_ref_pic_lists[list_idx]) {
    ref_pic_list_struct->ltrp_in_header_flag = 1;
  }
  // If the inter_layer_ref_pic_flag[list_idx][rpl_idx] is 0,
  // corresponding entry into ref_pic_list_struct[0/1] is a combination of
  // {inter_layer_ref_pic_flag, st_ref_pic_flag, abs_delta_poc_st,
  // strp_entry_sign_flag, rpls_poc_lsb_lt}; Otherwise, each entry into the
  // ref_pic_list_struct will contain the index of entry of the direct reference
  // layers.
  for (int i = 0, j = 0; i < ref_pic_list_struct->num_ref_entries; i++) {
    if (sps.sps_inter_layer_prediction_enabled_flag) {
      READ_BOOL_OR_RETURN(&ref_pic_list_struct->inter_layer_ref_pic_flag[i]);
    } else {
      ref_pic_list_struct->inter_layer_ref_pic_flag[i] = false;
    }
    if (!ref_pic_list_struct->inter_layer_ref_pic_flag[i]) {
      if (sps.sps_long_term_ref_pics_flag) {
        READ_BOOL_OR_RETURN(&ref_pic_list_struct->st_ref_pic_flag[i]);
      } else {
        // If LTR is disabled, VVC does not explicitly signal STR flag.
        ref_pic_list_struct->st_ref_pic_flag[i] = 1;
      }
      if (ref_pic_list_struct->st_ref_pic_flag[i]) {
        READ_UE_OR_RETURN(&ref_pic_list_struct->abs_delta_poc_st[i]);
        IN_RANGE_OR_RETURN(ref_pic_list_struct->abs_delta_poc_st[i], 0,
                           std::pow(2, 15) - 1);
        // Equation 150. For the first abs_delta_poc_st in the
        // ref_pic_list_struct, or when weighted prediction is used, the short
        // term POC delta is the absolute delta; Otherwise this needs to be
        // added by 1 to reflect the real delta.
        if ((sps.sps_weighted_pred_flag || sps.sps_weighted_bipred_flag) &&
            i != 0) {
          abs_delta_poc_st = ref_pic_list_struct->abs_delta_poc_st[i];
        } else {
          abs_delta_poc_st = ref_pic_list_struct->abs_delta_poc_st[i] + 1;
        }

        if (abs_delta_poc_st > 0) {
          READ_BOOL_OR_RETURN(&ref_pic_list_struct->strp_entry_sign_flag[i]);
        }
      } else if (!ref_pic_list_struct->ltrp_in_header_flag) {
        READ_BITS_OR_RETURN(sps.sps_log2_max_pic_order_cnt_lsb_minus4 + 4,
                            &ref_pic_list_struct->rpls_poc_lsb_lt[j++]);
      }
    } else {
      READ_UE_OR_RETURN(&ref_pic_list_struct->ilrp_idx[i]);
      int general_layer_id = vps->GetGeneralLayerIdx(sps.nuh_layer_id);
      if (general_layer_id != -1) {
        IN_RANGE_OR_RETURN(ref_pic_list_struct->ilrp_idx[i], 0,
                           vps->num_direct_ref_layers[general_layer_id] - 1);
      }
    }

    // Equation 149 & Equation 151
    if (!ref_pic_list_struct->inter_layer_ref_pic_flag[i]) {
      if (!ref_pic_list_struct->st_ref_pic_flag[i]) {
        ref_pic_list_struct->num_ltrp_entries++;
      } else {
        // Combine short term ref delta POC values with their signs.
        ref_pic_list_struct->delta_poc_val_st[i] =
            (1 - 2 * ref_pic_list_struct->strp_entry_sign_flag[i]) *
            abs_delta_poc_st;
      }
    }
  }

  return kOk;
}

H266Parser::Result H266Parser::ParseGeneralTimingHrdParameters(
    H266GeneralTimingHrdParameters* general_timing_hrd_parameters) {
  DCHECK(general_timing_hrd_parameters);

  int num_units_in_tick_high16 = 0, num_units_in_tick_low16 = 0;
  READ_BITS_OR_RETURN(16, &num_units_in_tick_high16);
  READ_BITS_OR_RETURN(16, &num_units_in_tick_low16);
  general_timing_hrd_parameters->num_units_in_tick =
      (num_units_in_tick_high16 << 16) + num_units_in_tick_low16;

  int time_scale_high16 = 0, time_scale_low16 = 0;
  READ_BITS_OR_RETURN(16, &time_scale_high16);
  READ_BITS_OR_RETURN(16, &time_scale_low16);
  general_timing_hrd_parameters->time_scale =
      (time_scale_high16 << 16) + time_scale_low16;

  READ_BOOL_OR_RETURN(
      &general_timing_hrd_parameters->general_nal_hrd_params_present_flag);
  READ_BOOL_OR_RETURN(
      &general_timing_hrd_parameters->general_vcl_hrd_params_present_flag);
  if (general_timing_hrd_parameters->general_nal_hrd_params_present_flag ||
      general_timing_hrd_parameters->general_vcl_hrd_params_present_flag) {
    READ_BOOL_OR_RETURN(&general_timing_hrd_parameters
                             ->general_same_pic_timing_in_all_ols_flag);
    READ_BOOL_OR_RETURN(
        &general_timing_hrd_parameters->general_du_hrd_params_present_flag);
    if (general_timing_hrd_parameters->general_du_hrd_params_present_flag) {
      READ_BITS_OR_RETURN(8,
                          &general_timing_hrd_parameters->tick_divisor_minus2);
    }
    READ_BITS_OR_RETURN(4, &general_timing_hrd_parameters->bit_rate_scale);
    READ_BITS_OR_RETURN(4, &general_timing_hrd_parameters->cpb_size_scale);
    if (general_timing_hrd_parameters->general_du_hrd_params_present_flag) {
      READ_BITS_OR_RETURN(4, &general_timing_hrd_parameters->cpb_size_du_scale);
    }
    READ_UE_OR_RETURN(&general_timing_hrd_parameters->hrd_cpb_cnt_minus1);
    IN_RANGE_OR_RETURN(general_timing_hrd_parameters->hrd_cpb_cnt_minus1, 0,
                       31);
  } else {
    general_timing_hrd_parameters->general_du_hrd_params_present_flag = 0;
  }
  return kOk;
}

H266Parser::Result H266Parser::ParseOlsTimingHrdParameters(
    int first_sublayer,
    int max_sublayer_val,
    const H266GeneralTimingHrdParameters& general_timing_hrd_parameters,
    H266OlsTimingHrdParameters* ols_timing_hrd_parameters) {
  DCHECK(ols_timing_hrd_parameters);
  DCHECK(first_sublayer >= 0);
  DCHECK(max_sublayer_val >= 0);

  for (int i = first_sublayer; i <= max_sublayer_val; i++) {
    READ_BOOL_OR_RETURN(
        &ols_timing_hrd_parameters->fixed_pic_rate_general_flag[i]);
    if (!ols_timing_hrd_parameters->fixed_pic_rate_general_flag[i]) {
      READ_BOOL_OR_RETURN(
          &ols_timing_hrd_parameters->fixed_pic_rate_within_cvs_flag[i]);
    } else {
      ols_timing_hrd_parameters->fixed_pic_rate_within_cvs_flag[i] = 1;
    }
    if (ols_timing_hrd_parameters->fixed_pic_rate_within_cvs_flag[i]) {
      READ_UE_OR_RETURN(
          &ols_timing_hrd_parameters->element_duration_in_tc_minus1[i]);
      IN_RANGE_OR_RETURN(
          ols_timing_hrd_parameters->element_duration_in_tc_minus1[i], 0, 2047);
    } else if ((general_timing_hrd_parameters
                    .general_nal_hrd_params_present_flag ||
                general_timing_hrd_parameters
                    .general_vcl_hrd_params_present_flag) &&
               general_timing_hrd_parameters.hrd_cpb_cnt_minus1 == 0) {
      READ_BOOL_OR_RETURN(&ols_timing_hrd_parameters->low_delay_hrd_flag[i]);
    }

    if (general_timing_hrd_parameters.general_nal_hrd_params_present_flag) {
      ParseSublayerHrdParameters(
          i, general_timing_hrd_parameters,
          &ols_timing_hrd_parameters->nal_sublayer_hrd_parameters[i]);
    }
    if (general_timing_hrd_parameters.general_vcl_hrd_params_present_flag) {
      ParseSublayerHrdParameters(
          i, general_timing_hrd_parameters,
          &ols_timing_hrd_parameters->vcl_sublayer_hrd_parameters[i]);
    }
  }
  return kOk;
}

H266Parser::Result H266Parser::ParseSublayerHrdParameters(
    int sublayer_id,
    const H266GeneralTimingHrdParameters& general_timing_hrd_parameters,
    H266SublayerHrdParameters* sublayer_hrd_parameters) {
  DCHECK(sublayer_id >= 0 && sublayer_id < kMaxSubLayers);
  DCHECK(sublayer_hrd_parameters);

  for (int j = 0; j <= general_timing_hrd_parameters.hrd_cpb_cnt_minus1; j++) {
    READ_UE_OR_RETURN(&sublayer_hrd_parameters->bit_rate_value_minus1[j]);
    IN_RANGE_OR_RETURN(sublayer_hrd_parameters->bit_rate_du_value_minus1[j], 0,
                       std::pow(2, 32) - 2);
    if (j > 0) {
      GT_OR_RETURN(sublayer_hrd_parameters->bit_rate_du_value_minus1[j],
                   sublayer_hrd_parameters->bit_rate_du_value_minus1[j - 1]);
    }
    READ_UE_OR_RETURN(&sublayer_hrd_parameters->cpb_size_value_minus1[j]);
    IN_RANGE_OR_RETURN(sublayer_hrd_parameters->cpb_size_value_minus1[j], 0,
                       std::pow(2, 32) - 2);
    if (j > 0) {
      LE_OR_RETURN(sublayer_hrd_parameters->cpb_size_value_minus1[j],
                   sublayer_hrd_parameters->cpb_size_value_minus1[j - 1]);
    }
    READ_BOOL_OR_RETURN(&sublayer_hrd_parameters->cbr_flag[j]);
  }
  return kOk;
}

// The SPS might be indirectly referenced by PH->PPS->SPS, where PH is either
// a PH_NUT or picture header structure in slice header.
H266Parser::Result H266Parser::ParseSPS(const H266NALU& nalu, int* sps_id) {
  // 7.4.3.4
  DVLOG(4) << "Parsing SPS";
  Result res = kOk;

  DCHECK(sps_id);
  *sps_id = -1;

  std::unique_ptr<H266SPS> sps = std::make_unique<H266SPS>();

  sps->nuh_layer_id = nalu.nuh_layer_id;
  READ_BITS_OR_RETURN(4, &sps->sps_seq_parameter_set_id);
  IN_RANGE_OR_RETURN(sps->sps_seq_parameter_set_id, 0, 15);
  READ_BITS_OR_RETURN(4, &sps->sps_video_parameter_set_id);
  GE_OR_RETURN(sps->sps_video_parameter_set_id, 0);
  READ_BITS_OR_RETURN(3, &sps->sps_max_sublayers_minus1);
  if (sps->sps_video_parameter_set_id > 0) {
    const H266VPS* vps = GetVPS(sps->sps_video_parameter_set_id);
    if (!vps) {
      return kMissingParameterSet;
    }
    IN_RANGE_OR_RETURN(sps->sps_max_sublayers_minus1, 0,
                       vps->vps_max_sublayers_minus1);
  } else {
    IN_RANGE_OR_RETURN(sps->sps_max_sublayers_minus1, 0, 6);
    if (!GetVPS(0)) {
      // Create a fake VPS for the inferred VPS members.
      std::unique_ptr<H266VPS> vps = std::make_unique<H266VPS>();
      vps->vps_video_parameter_set_id = 0;
      vps->vps_max_layers_minus1 = 0;
      vps->vps_max_sublayers_minus1 = sps->sps_max_sublayers_minus1;
      vps->vps_independent_layer_flag[0] = 1;
      vps->vps_layer_id[0] = nalu.nuh_layer_id;
      vps->general_layer_idx[nalu.nuh_layer_id] = 0;
      vps->vps_ols_ptl_idx[0] = 0;
      vps->vps_ptl_max_tid[0] = sps->sps_max_sublayers_minus1;
      active_vps_[0] = std::move(vps);
    }
  }
  READ_BITS_OR_RETURN(2, &sps->sps_chroma_format_idc);
  IN_RANGE_OR_RETURN(sps->sps_chroma_format_idc, 0, 3);
  switch (sps->sps_chroma_format_idc) {
    case 0:  // monochrome
    case 3:  // 4:4:4
      sps->sub_width_c = 1;
      sps->sub_height_c = 1;
      break;
    case 1:  // 4:2:0
      sps->sub_width_c = 2;
      sps->sub_height_c = 2;
      break;
    case 2:  // 4:2:2
      sps->sub_width_c = 2;
      sps->sub_height_c = 1;
      break;
  }

  READ_BITS_OR_RETURN(2, &sps->sps_log2_ctu_size_minus5);
  IN_RANGE_OR_RETURN(sps->sps_log2_ctu_size_minus5, 0, 2);

  // Equation 35 & 36
  sps->ctb_log2_size_y = sps->sps_log2_ctu_size_minus5 + 5;
  sps->ctb_size_y = 1 << sps->ctb_log2_size_y;
  READ_BOOL_OR_RETURN(&sps->sps_ptl_dpb_hrd_params_present_flag);
  // When present, profile_tier_level/dpb/hrd syntax structures
  // are in SPS. If not present, we should use corresponding info
  // in VPS.
  if (sps->sps_ptl_dpb_hrd_params_present_flag) {
    ParseProfileTierLevel(true, sps->sps_max_sublayers_minus1,
                          &sps->profile_tier_level);
  } else {
    // profile-tier-level info must be in SPS when VPS id is 0.
    TRUE_OR_RETURN(sps->sps_video_parameter_set_id != 0);
  }
  READ_BOOL_OR_RETURN(&sps->sps_gdr_enabled_flag);
  // Below two indicates dynamic frame scaling on/off.
  READ_BOOL_OR_RETURN(&sps->sps_ref_pic_resampling_enabled_flag);
  if (sps->sps_ref_pic_resampling_enabled_flag) {
    READ_BOOL_OR_RETURN(&sps->sps_res_change_in_clvs_allowed_flag);
  }
  READ_UE_OR_RETURN(&sps->sps_pic_width_max_in_luma_samples);
  READ_UE_OR_RETURN(&sps->sps_pic_height_max_in_luma_samples);
  TRUE_OR_RETURN(sps->sps_pic_width_max_in_luma_samples != 0 &&
                 sps->sps_pic_height_max_in_luma_samples != 0);

  // A.4.2. Equation 1587, calculate the max_dpb_size.
  int max_luma_ps = sps->profile_tier_level.MaxLumaPs();
  base::CheckedNumeric<int> pic_size = sps->sps_pic_height_max_in_luma_samples;
  pic_size *= sps->sps_pic_width_max_in_luma_samples;
  if (!pic_size.IsValid()) {
    return kInvalidStream;
  }
  int pic_size_in_samples_y = pic_size.ValueOrDefault(0);
  if (2 * pic_size_in_samples_y <= max_luma_ps) {
    sps->max_dpb_size = 2 * kMaxDpbPicBuffer;
  } else if (3 * pic_size_in_samples_y <= (2 * max_luma_ps)) {
    sps->max_dpb_size = 3 * kMaxDpbPicBuffer / 2;
  } else {
    sps->max_dpb_size = kMaxDpbPicBuffer;
  }

  READ_BOOL_OR_RETURN(&sps->sps_conformance_window_flag);
  if (sps->sps_conformance_window_flag) {
    READ_UE_OR_RETURN(&sps->sps_conf_win_left_offset);
    READ_UE_OR_RETURN(&sps->sps_conf_win_right_offset);
    READ_UE_OR_RETURN(&sps->sps_conf_win_top_offset);
    READ_UE_OR_RETURN(&sps->sps_conf_win_bottom_offset);
    base::CheckedNumeric<int> width_crop = sps->sps_conf_win_left_offset;
    width_crop += sps->sps_conf_win_right_offset;
    width_crop *= sps->sub_width_c;
    if (!width_crop.IsValid()) {
      return kInvalidStream;
    }
    TRUE_OR_RETURN(width_crop.ValueOrDefault(0) <
                   sps->sps_pic_width_max_in_luma_samples);
    base::CheckedNumeric<int> height_crop = sps->sps_conf_win_top_offset;
    height_crop += sps->sps_conf_win_bottom_offset;
    height_crop *= sps->sub_height_c;
    if (!height_crop.IsValid()) {
      return kInvalidStream;
    }
    TRUE_OR_RETURN(height_crop.ValueOrDefault(0) <
                   sps->sps_pic_height_max_in_luma_samples);
  }

  // Number of luma CTBs in width and height.
  int tmp_width_val =
      (sps->sps_pic_width_max_in_luma_samples + sps->ctb_size_y - 1) /
      sps->ctb_size_y;
  int tmp_height_val =
      (sps->sps_pic_height_max_in_luma_samples + sps->ctb_size_y - 1) /
      sps->ctb_size_y;

  // Subpictures in VVC are functionally MCTSs(motion-constrained tile sets) in
  // HEVC. The layout of subpictures is signalled in SPS, while subpicture ID
  // mapping is either statically signalled in SPS, or dynamic across pictures
  // when signalled in PPS. Subpictures are rectangular regions covering
  // multiple slices, and it is required following conditions to be fulfilled:
  // - All CTUs in a subpicture belong to the same tile, and,
  // - All CTUs in a tile belong to the same subpicture.
  READ_BOOL_OR_RETURN(&sps->sps_subpic_info_present_flag);
  if (sps->sps_subpic_info_present_flag) {
    READ_UE_OR_RETURN(&sps->sps_num_subpics_minus1);
    IN_RANGE_OR_RETURN(sps->sps_num_subpics_minus1, 0,
                       sps->profile_tier_level.MaxSlicesPerAu() - 1);
    if (sps->sps_num_subpics_minus1 > 0) {
      READ_BOOL_OR_RETURN(&sps->sps_independent_subpics_flag);
      READ_BOOL_OR_RETURN(&sps->sps_subpic_same_size_flag);
    } else {
      sps->sps_independent_subpics_flag = 1;
    }

    int width_bits = base::bits::Log2Ceiling(tmp_width_val);
    int height_bits = base::bits::Log2Ceiling(tmp_height_val);
    IN_RANGE_OR_RETURN(width_bits, 0, 31);
    IN_RANGE_OR_RETURN(height_bits, 0, 31);

    if (sps->sps_num_subpics_minus1 > 0) {
      // For the 0-th subpicture, do not overlook sps_subpic_same_size_flag
      // when parsing, and left_x/left_y is implied to be 0.
      sps->sps_subpic_ctu_top_left_x[0] = 0;
      sps->sps_subpic_ctu_top_left_y[0] = 0;
      if (sps->sps_pic_width_max_in_luma_samples > sps->ctb_size_y) {
        READ_BITS_OR_RETURN(width_bits, &sps->sps_subpic_width_minus1[0]);
      } else {
        // For 0-th subpicture in raster scan order this would be 0.
        sps->sps_subpic_width_minus1[0] =
            tmp_width_val - sps->sps_subpic_ctu_top_left_x[0] - 1;
      }
      if (sps->sps_pic_height_max_in_luma_samples > sps->ctb_size_y) {
        READ_BITS_OR_RETURN(height_bits, &sps->sps_subpic_height_minus1[0]);
      } else {
        sps->sps_subpic_height_minus1[0] =
            tmp_height_val - sps->sps_subpic_ctu_top_left_y[0] - 1;
      }
      if (!sps->sps_independent_subpics_flag) {
        READ_BOOL_OR_RETURN(&sps->sps_subpic_treated_as_pic_flag[0]);
        READ_BOOL_OR_RETURN(
            &sps->sps_loop_filter_across_subpic_enabled_flag[0]);
      }

      if (!sps->sps_subpic_same_size_flag) {
        for (int i = 1; i <= sps->sps_num_subpics_minus1; i++) {
          if (sps->sps_pic_width_max_in_luma_samples > sps->ctb_size_y) {
            READ_BITS_OR_RETURN(width_bits, &sps->sps_subpic_ctu_top_left_x[i]);
          } else {
            sps->sps_subpic_ctu_top_left_x[i] = 0;
          }
          if (sps->sps_pic_height_max_in_luma_samples > sps->ctb_size_y) {
            READ_BITS_OR_RETURN(height_bits,
                                &sps->sps_subpic_ctu_top_left_y[i]);
          } else {
            sps->sps_subpic_ctu_top_left_y[i] = 0;
          }
          if (i < sps->sps_num_subpics_minus1 &&
              sps->sps_pic_width_max_in_luma_samples > sps->ctb_size_y) {
            READ_BITS_OR_RETURN(width_bits, &sps->sps_subpic_width_minus1[i]);
          } else {
            sps->sps_subpic_width_minus1[i] =
                tmp_width_val - sps->sps_subpic_ctu_top_left_x[i] - 1;
          }
          if (i < sps->sps_num_subpics_minus1 &&
              sps->sps_pic_height_max_in_luma_samples > sps->ctb_size_y) {
            READ_BITS_OR_RETURN(height_bits, &sps->sps_subpic_height_minus1[i]);
          } else {
            sps->sps_subpic_height_minus1[i] =
                tmp_height_val - sps->sps_subpic_ctu_top_left_y[i] - 1;
          }
          if (!sps->sps_independent_subpics_flag) {
            READ_BOOL_OR_RETURN(&sps->sps_subpic_treated_as_pic_flag[i]);
            READ_BOOL_OR_RETURN(
                &sps->sps_loop_filter_across_subpic_enabled_flag[i]);
          }
        }
      } else {  // sps_subpic_same_size_flag = 1
        int num_subpic_cols = tmp_width_val / (sps->sps_subpic_width_minus1[0] +
                                               1);  // Equation 37
        GT_OR_RETURN(num_subpic_cols, 0);
        for (int i = 1; i <= sps->sps_num_subpics_minus1; i++) {
          sps->sps_subpic_ctu_top_left_x[i] =
              (i % num_subpic_cols) * (sps->sps_subpic_width_minus1[0] + 1);
          sps->sps_subpic_ctu_top_left_y[i] =
              (i / num_subpic_cols) * (sps->sps_subpic_height_minus1[0] + 1);
          sps->sps_subpic_width_minus1[i] = sps->sps_subpic_width_minus1[0];
          sps->sps_subpic_height_minus1[i] = sps->sps_subpic_height_minus1[0];
          if (!sps->sps_independent_subpics_flag) {
            READ_BOOL_OR_RETURN(&sps->sps_subpic_treated_as_pic_flag[i]);
            READ_BOOL_OR_RETURN(
                &sps->sps_loop_filter_across_subpic_enabled_flag[i]);
          }
        }
      }
    }

    READ_UE_OR_RETURN(&sps->sps_subpic_id_len_minus1);
    IN_RANGE_OR_RETURN(sps->sps_subpic_id_len_minus1, 0, 15);
    TRUE_OR_RETURN((1 << (sps->sps_subpic_id_len_minus1 + 1)) >=
                   sps->sps_num_subpics_minus1 + 1);
    READ_BOOL_OR_RETURN(&sps->sps_subpic_id_mapping_explicitly_signaled_flag);
    if (sps->sps_subpic_id_mapping_explicitly_signaled_flag) {
      READ_BOOL_OR_RETURN(&sps->sps_subpic_id_mapping_present_flag);
      if (sps->sps_subpic_id_mapping_present_flag) {
        for (int i = 0; i <= sps->sps_num_subpics_minus1; i++) {
          READ_BITS_OR_RETURN(sps->sps_subpic_id_len_minus1 + 1,
                              &sps->sps_subpic_id[i]);
        }
      }
    }
  } else {
    sps->sps_subpic_width_minus1[0] = tmp_width_val - 1;
    sps->sps_subpic_height_minus1[0] = tmp_height_val - 1;
  }

  READ_UE_OR_RETURN(&sps->sps_bitdepth_minus8);
  IN_RANGE_OR_RETURN(sps->sps_bitdepth_minus8, 0, 8);
  // Equation 39
  sps->qp_bd_offset = 6 * sps->sps_bitdepth_minus8;

  // This controls the wavefront parallel processing(WPP) tool on/off.
  READ_BOOL_OR_RETURN(&sps->sps_entropy_coding_sync_enabled_flag);
  READ_BOOL_OR_RETURN(&sps->sps_entry_point_offsets_present_flag);
  READ_BITS_OR_RETURN(4, &sps->sps_log2_max_pic_order_cnt_lsb_minus4);
  IN_RANGE_OR_RETURN(sps->sps_log2_max_pic_order_cnt_lsb_minus4, 0, 12);
  sps->max_pic_order_cnt_lsb =
      std::pow(2, sps->sps_log2_max_pic_order_cnt_lsb_minus4 + 4);
  READ_BOOL_OR_RETURN(&sps->sps_poc_msb_cycle_flag);
  if (sps->sps_poc_msb_cycle_flag) {
    READ_UE_OR_RETURN(&sps->sps_poc_msb_cycle_len_minus1);
    IN_RANGE_OR_RETURN(sps->sps_poc_msb_cycle_len_minus1, 0,
                       32 - sps->sps_log2_max_pic_order_cnt_lsb_minus4 - 5);
  }

  // Be noted spec requires sps_num_extra_ph_bytes/sps_num_extra_sh_bytes to
  // be 0, but allows 1 or 2 to appear in the syntax.
  READ_BITS_OR_RETURN(2, &sps->sps_num_extra_ph_bytes);
  IN_RANGE_OR_RETURN(sps->sps_num_extra_ph_bytes, 0, 2);

  // Equation 41.
  for (int i = 0; i < (sps->sps_num_extra_ph_bytes * 8); i++) {
    READ_BOOL_OR_RETURN(&sps->sps_extra_ph_bit_present_flag[i]);
    if (sps->sps_extra_ph_bit_present_flag[i]) {
      sps->num_extra_ph_bits++;
    }
  }
  READ_BITS_OR_RETURN(2, &sps->sps_num_extra_sh_bytes);
  IN_RANGE_OR_RETURN(sps->sps_num_extra_sh_bytes, 0, 2);

  // Equation 42.
  for (int i = 0; i < (sps->sps_num_extra_sh_bytes * 8); i++) {
    READ_BOOL_OR_RETURN(&sps->sps_extra_sh_bit_present_flag[i]);
    if (sps->sps_extra_sh_bit_present_flag[i]) {
      sps->num_extra_sh_bits++;
    }
  }
  if (sps->sps_ptl_dpb_hrd_params_present_flag) {
    if (sps->sps_max_sublayers_minus1 > 0) {
      READ_BOOL_OR_RETURN(&sps->sps_sublayer_dpb_params_flag);
    }
    ParseDpbParameters(sps->sps_max_sublayers_minus1,
                       sps->sps_sublayer_dpb_params_flag, &sps->dpb_params);
  }

  READ_UE_OR_RETURN(&sps->sps_log2_min_luma_coding_block_size_minus2);
  // Allowed minimum CB size would be 4x4 to 64x64 and it must not
  // be larger than 1/4 of CTU width.
  IN_RANGE_OR_RETURN(sps->sps_log2_min_luma_coding_block_size_minus2, 0,
                     std::min(4, sps->sps_log2_ctu_size_minus5 + 3));

  // TODO(crbugs.com/1417910): Equation 43 - 49. Calculation of IbcBufWidthY,
  // IbcBufWidthC and VSize, CtbWidthC, CTbHeightC if needed for decoding
  // process.
  sps->min_cb_log2_size_y = sps->sps_log2_min_luma_coding_block_size_minus2 + 2;
  sps->min_cb_size_y = 1 << sps->min_cb_log2_size_y;

  // Recheck pic width/height alignment as min_cb_size_y is unknown when parsing
  // them. They must be at least 8-aligned, but if a larger min CB size is
  // selected during encoding, will need to align to min CB size.
  int pic_size_alignment = std::max(8, sps->min_cb_size_y);
  TRUE_OR_RETURN(
      sps->sps_pic_width_max_in_luma_samples % pic_size_alignment == 0 &&
      sps->sps_pic_height_max_in_luma_samples % pic_size_alignment == 0);

  READ_BOOL_OR_RETURN(&sps->sps_partition_constraints_override_enabled_flag);
  READ_UE_OR_RETURN(&sps->sps_log2_diff_min_qt_min_cb_intra_slice_luma);
  IN_RANGE_OR_RETURN(
      sps->sps_log2_diff_min_qt_min_cb_intra_slice_luma, 0,
      std::min(6, sps->ctb_log2_size_y) - sps->min_cb_log2_size_y);

  // Equation 50. This calculates the min log2 luma leaf CB size after a
  // quadtree splitting of CTU.
  int min_qt_log2_size_intra_y =
      sps->sps_log2_diff_min_qt_min_cb_intra_slice_luma +
      sps->min_cb_log2_size_y;
  int min_qt_log2_size_intra_c = 0;

  // Below syntax elements specify constraints for quadtree/ternary/binary split
  // of CTUs.
  READ_UE_OR_RETURN(&sps->sps_max_mtt_hierarchy_depth_intra_slice_luma);
  IN_RANGE_OR_RETURN(sps->sps_max_mtt_hierarchy_depth_intra_slice_luma, 0,
                     2 * (sps->ctb_log2_size_y - sps->min_cb_log2_size_y));
  if (sps->sps_max_mtt_hierarchy_depth_intra_slice_luma) {
    READ_UE_OR_RETURN(&sps->sps_log2_diff_max_bt_min_qt_intra_slice_luma);
    IN_RANGE_OR_RETURN(sps->sps_log2_diff_max_bt_min_qt_intra_slice_luma, 0,
                       sps->ctb_log2_size_y - min_qt_log2_size_intra_y);
    READ_UE_OR_RETURN(&sps->sps_log2_diff_max_tt_min_qt_intra_slice_luma);
    IN_RANGE_OR_RETURN(
        sps->sps_log2_diff_max_tt_min_qt_intra_slice_luma, 0,
        std::min(6, sps->ctb_log2_size_y) - min_qt_log2_size_intra_y);
  }
  if (sps->sps_chroma_format_idc != 0) {
    READ_BOOL_OR_RETURN(&sps->sps_qtbtt_dual_tree_intra_flag);
  }
  if (sps->sps_qtbtt_dual_tree_intra_flag) {
    READ_UE_OR_RETURN(&sps->sps_log2_diff_min_qt_min_cb_intra_slice_chroma);
    IN_RANGE_OR_RETURN(
        sps->sps_log2_diff_min_qt_min_cb_intra_slice_chroma, 0,
        std::min(6, sps->ctb_log2_size_y) - sps->min_cb_log2_size_y);
    min_qt_log2_size_intra_c =
        sps->sps_log2_diff_min_qt_min_cb_intra_slice_chroma +
        sps->min_cb_log2_size_y;
    READ_UE_OR_RETURN(&sps->sps_max_mtt_hierarchy_depth_intra_slice_chroma);
    IN_RANGE_OR_RETURN(sps->sps_max_mtt_hierarchy_depth_intra_slice_chroma, 0,
                       2 * (sps->ctb_log2_size_y - sps->min_cb_log2_size_y));
    if (sps->sps_max_mtt_hierarchy_depth_intra_slice_chroma != 0) {
      READ_UE_OR_RETURN(&sps->sps_log2_diff_max_bt_min_qt_intra_slice_chroma);
      IN_RANGE_OR_RETURN(
          sps->sps_log2_diff_max_bt_min_qt_intra_slice_chroma, 0,
          std::min(6, sps->ctb_log2_size_y) - min_qt_log2_size_intra_c);
      READ_UE_OR_RETURN(&sps->sps_log2_diff_max_tt_min_qt_intra_slice_chroma);
      IN_RANGE_OR_RETURN(
          sps->sps_log2_diff_max_tt_min_qt_intra_slice_chroma, 0,
          std::min(6, sps->ctb_log2_size_y) - min_qt_log2_size_intra_c);
    }
  }

  READ_UE_OR_RETURN(&sps->sps_log2_diff_min_qt_min_cb_inter_slice);
  IN_RANGE_OR_RETURN(
      sps->sps_log2_diff_min_qt_min_cb_inter_slice, 0,
      std::min(6, sps->ctb_log2_size_y) - sps->min_cb_log2_size_y);
  // Equation 52
  int min_qt_log2_size_inter_y =
      sps->sps_log2_diff_min_qt_min_cb_inter_slice + sps->min_cb_log2_size_y;
  READ_UE_OR_RETURN(&sps->sps_max_mtt_hierarchy_depth_inter_slice);
  IN_RANGE_OR_RETURN(sps->sps_max_mtt_hierarchy_depth_inter_slice, 0,
                     2 * (sps->ctb_log2_size_y - sps->min_cb_log2_size_y));
  if (sps->sps_max_mtt_hierarchy_depth_inter_slice != 0) {
    READ_UE_OR_RETURN(&sps->sps_log2_diff_max_bt_min_qt_inter_slice);
    IN_RANGE_OR_RETURN(sps->sps_log2_diff_max_bt_min_qt_inter_slice, 0,
                       sps->ctb_log2_size_y - min_qt_log2_size_inter_y);
    READ_UE_OR_RETURN(&sps->sps_log2_diff_max_tt_min_qt_inter_slice);
    IN_RANGE_OR_RETURN(
        sps->sps_log2_diff_max_tt_min_qt_inter_slice, 0,
        std::min(6, sps->ctb_log2_size_y) - min_qt_log2_size_inter_y);
  }

  // Transform block size cannot be larger than CTB size.
  if (sps->ctb_size_y > 32) {
    READ_BOOL_OR_RETURN(&sps->sps_max_luma_transform_size_64_flag);
  } else {
    sps->sps_max_luma_transform_size_64_flag = 0;
  }
  // Equation 53 - 56. Transform unit/block related information.
  int min_tb_log2_size_y = 2;
  int max_tb_log2_size_y = sps->sps_max_luma_transform_size_64_flag ? 6 : 5;
  sps->min_tb_size_y = (1 << min_tb_log2_size_y);
  sps->max_tb_size_y = (1 << max_tb_log2_size_y);

  READ_BOOL_OR_RETURN(&sps->sps_transform_skip_enabled_flag);
  if (sps->sps_transform_skip_enabled_flag) {
    READ_UE_OR_RETURN(&sps->sps_log2_transform_skip_max_size_minus2);
    IN_RANGE_OR_RETURN(sps->sps_log2_transform_skip_max_size_minus2, 0, 3);
    READ_BOOL_OR_RETURN(&sps->sps_bdpcm_enabled_flag);
  }
  sps->max_ts_size_y =
      (1 << (sps->sps_log2_transform_skip_max_size_minus2 + 2));

  // Multiple transform selection on/off.
  READ_BOOL_OR_RETURN(&sps->sps_mts_enabled_flag);
  if (sps->sps_mts_enabled_flag) {
    READ_BOOL_OR_RETURN(&sps->sps_explicit_mts_intra_enabled_flag);
    READ_BOOL_OR_RETURN(&sps->sps_explicit_mts_inter_enabled_flag);
  }

  // Low frequency non-separable transform on/off. It is only applied for
  // intra blocks for both luma/chroma component.
  READ_BOOL_OR_RETURN(&sps->sps_lfnst_enabled_flag);

  int num_qp_tables = 0;
  if (sps->sps_chroma_format_idc != 0) {
    READ_BOOL_OR_RETURN(&sps->sps_joint_cbcr_enabled_flag);
    READ_BOOL_OR_RETURN(&sps->sps_same_qp_table_for_chroma_flag);
    num_qp_tables = sps->sps_same_qp_table_for_chroma_flag
                        ? 1
                        : (sps->sps_joint_cbcr_enabled_flag ? 3 : 2);
    for (int i = 0; i < num_qp_tables; i++) {
      READ_SE_OR_RETURN(&sps->sps_qp_table_start_minus26[i]);
      IN_RANGE_OR_RETURN(sps->sps_qp_table_start_minus26[i],
                         -26 - sps->qp_bd_offset, 36);
      READ_UE_OR_RETURN(&sps->sps_num_points_in_qp_table_minus1[i]);
      IN_RANGE_OR_RETURN(sps->sps_num_points_in_qp_table_minus1[i], 0,
                         36 - sps->sps_qp_table_start_minus26[i]);
      LE_OR_RETURN(sps->sps_num_points_in_qp_table_minus1[i],
                   kMaxPointsInQpTable);
      for (int j = 0; j <= sps->sps_num_points_in_qp_table_minus1[i]; j++) {
        READ_UE_OR_RETURN(&sps->sps_delta_qp_in_val_minus1[i][j]);
        READ_UE_OR_RETURN(&sps->sps_delta_qp_diff_val[i][j]);
      }
    }
  } else {
    sps->sps_same_qp_table_for_chroma_flag = 1;
  }
  // Equation 57. Set up the ChromaQpTables.
  // In the spec, keys into ChromaQpTable[i] might be negative values in
  // [-QpBdoffset, 63], so sps->chroma_qp_table[i][m] provided by the parser
  // corresponds to ChromaQpTable[i][m - QpBdOffset] in the spec.
  int qp_in_val[3][kMaxPointsInQpTable + 1];
  int qp_out_val[3][kMaxPointsInQpTable + 1];

  for (int i = 0; i < num_qp_tables; i++) {
    qp_in_val[i][0] = sps->sps_qp_table_start_minus26[i] + 26;
    qp_out_val[i][0] = qp_in_val[i][0];
    int j, k, m;
    for (j = 0; j <= sps->sps_num_points_in_qp_table_minus1[i]; j++) {
      qp_in_val[i][j + 1] =
          qp_in_val[i][j] + sps->sps_delta_qp_in_val_minus1[i][j] + 1;
      qp_out_val[i][j + 1] =
          qp_out_val[i][j] + (sps->sps_delta_qp_in_val_minus1[i][j] ^
                              sps->sps_delta_qp_diff_val[i][j]);
    }
    sps->chroma_qp_table[i][qp_in_val[i][0] + sps->qp_bd_offset] =
        qp_out_val[i][0];
    for (k = qp_in_val[i][0] - 1 + sps->qp_bd_offset; k >= 0; k--) {
      sps->chroma_qp_table[i][k] = std::clamp(
          sps->chroma_qp_table[i][k + 1] - 1, -sps->qp_bd_offset, 63);
    }
    for (j = 0; j <= sps->sps_num_points_in_qp_table_minus1[i]; j++) {
      int sh = (sps->sps_delta_qp_in_val_minus1[i][j] + 1) >> 1;
      for (k = qp_in_val[i][j] + 1, m = 1; k <= qp_in_val[i][j + 1]; k++, m++) {
        LE_OR_RETURN(j, kMaxPointsInQpTable - 1);
        IN_RANGE_OR_RETURN(k + sps->qp_bd_offset, 0, kMaxPointsInQpTable - 1);
        IN_RANGE_OR_RETURN(qp_in_val[i][j] + sps->qp_bd_offset, 0,
                           kMaxPointsInQpTable - 1);
        sps->chroma_qp_table[i][k + sps->qp_bd_offset] =
            sps->chroma_qp_table[i][qp_in_val[i][j] + sps->qp_bd_offset] +
            ((qp_out_val[i][j + 1] - qp_out_val[i][j]) * m + sh) /
                (sps->sps_delta_qp_in_val_minus1[i][j] + 1);
      }
    }
    for (k = qp_in_val[i][sps->sps_num_points_in_qp_table_minus1[i] + 1] + 1;
         k <= 63; k++) {
      IN_RANGE_OR_RETURN(k + sps->qp_bd_offset, 0, kMaxPointsInQpTable - 1);
      sps->chroma_qp_table[i][k + sps->qp_bd_offset] =
          std::clamp(sps->chroma_qp_table[i][k + sps->qp_bd_offset - 1] + 1,
                     -sps->qp_bd_offset, 63);
    }
  }
  // If same qp table is used, replicate chroma_qp_table[0][k] to
  // chroma_qp_table[1][k] and chroma_qp_table[2][k].
  if (sps->sps_same_qp_table_for_chroma_flag) {
    memcpy(&sps->chroma_qp_table[1][0], &sps->chroma_qp_table[0][0],
           sizeof(sps->chroma_qp_table) / 3);
    memcpy(&sps->chroma_qp_table[2][0], &sps->chroma_qp_table[0][0],
           sizeof(sps->chroma_qp_table) / 3);
  }

  // Sample adaptive offset filter and adaptive loop filter  on/off.
  READ_BOOL_OR_RETURN(&sps->sps_sao_enabled_flag);
  READ_BOOL_OR_RETURN(&sps->sps_alf_enabled_flag);
  if (sps->sps_alf_enabled_flag && sps->sps_chroma_format_idc != 0) {
    READ_BOOL_OR_RETURN(&sps->sps_ccalf_enabled_flag);
  }
  READ_BOOL_OR_RETURN(&sps->sps_lmcs_enabled_flag);
  READ_BOOL_OR_RETURN(&sps->sps_weighted_pred_flag);
  READ_BOOL_OR_RETURN(&sps->sps_weighted_bipred_flag);
  READ_BOOL_OR_RETURN(&sps->sps_long_term_ref_pics_flag);
  if (sps->sps_video_parameter_set_id > 0) {
    READ_BOOL_OR_RETURN(&sps->sps_inter_layer_prediction_enabled_flag);
  }

  // Reference picture list structure handling. When ref_pic_list_struct
  // syntax elements are in SPS, they're merely listed as candidates for
  // RPL 0 and RPL 1.
  // When sps_idr_rpl_present_flag is 1, the RPL syntax elements could
  // be present in slice headers of IDR_N_LP/IDR_W_RADL slices.
  READ_BOOL_OR_RETURN(&sps->sps_idr_rpl_present_flag);
  READ_BOOL_OR_RETURN(&sps->sps_rpl1_same_as_rpl0_flag);
  for (int i = 0; i < (sps->sps_rpl1_same_as_rpl0_flag ? 1 : 2); i++) {
    READ_UE_OR_RETURN(&sps->sps_num_ref_pic_lists[i]);
    // Be noted decoder could allocate sps_num_ref_pic_list[i] + 1
    // ref_pic_list_struct(list_idx, rpls_idx) syntax structures, because
    // there could be one ref_pic_list_struct that is directly signalled in
    // picture header structure.
    IN_RANGE_OR_RETURN(sps->sps_num_ref_pic_lists[i], 0, 64);
    for (int j = 0; j < sps->sps_num_ref_pic_lists[i]; j++) {
      ParseRefPicListStruct(i, j, *sps, &sps->ref_pic_list_struct[i][j]);
    }
  }
  // sps_num_ref_pic_list[1] and ref_pic_list_struct(1, rplsIdx) not present.
  if (sps->sps_rpl1_same_as_rpl0_flag) {
    sps->sps_num_ref_pic_lists[1] = sps->sps_num_ref_pic_lists[0];
    // 7.4.3.4: Infer ref_pic_list_struct(1, rplsIdx) from
    // ref_pic_list_struct(0, rplsIdx) for rplsIdx ranging from 0 to
    // sps_num_ref_pic_lists[0] - 1;
    memcpy(&sps->ref_pic_list_struct[1][0], &sps->ref_pic_list_struct[0][0],
           sizeof(sps->ref_pic_list_struct[0]));
  }

  READ_BOOL_OR_RETURN(&sps->sps_ref_wraparound_enabled_flag);
  READ_BOOL_OR_RETURN(&sps->sps_temporal_mvp_enabled_flag);
  if (sps->sps_temporal_mvp_enabled_flag) {
    READ_BOOL_OR_RETURN(&sps->sps_sbtmvp_enabled_flag);
  }
  READ_BOOL_OR_RETURN(&sps->sps_amvr_enabled_flag);
  READ_BOOL_OR_RETURN(&sps->sps_bdof_enabled_flag);
  if (sps->sps_bdof_enabled_flag) {
    READ_BOOL_OR_RETURN(&sps->sps_bdof_control_present_in_ph_flag);
  }
  READ_BOOL_OR_RETURN(&sps->sps_smvd_enabled_flag);
  READ_BOOL_OR_RETURN(&sps->sps_dmvr_enabled_flag);
  if (sps->sps_dmvr_enabled_flag) {
    READ_BOOL_OR_RETURN(&sps->sps_dmvr_control_present_in_ph_flag);
  }
  READ_BOOL_OR_RETURN(&sps->sps_mmvd_enabled_flag);
  if (sps->sps_mmvd_enabled_flag) {
    READ_BOOL_OR_RETURN(&sps->sps_mmvd_fullpel_only_enabled_flag);
  }
  READ_UE_OR_RETURN(&sps->sps_six_minus_max_num_merge_cand);
  IN_RANGE_OR_RETURN(sps->sps_six_minus_max_num_merge_cand, 0, 5);
  READ_BOOL_OR_RETURN(&sps->sps_sbt_enabled_flag);
  READ_BOOL_OR_RETURN(&sps->sps_affine_enabled_flag);
  if (sps->sps_affine_enabled_flag) {
    READ_UE_OR_RETURN(&sps->sps_five_minus_max_num_subblock_merge_cand);
    IN_RANGE_OR_RETURN(sps->sps_five_minus_max_num_subblock_merge_cand, 0,
                       5 - sps->sps_sbtmvp_enabled_flag);
    READ_BOOL_OR_RETURN(&sps->sps_6param_affine_enabled_flag);
    if (sps->sps_amvr_enabled_flag) {
      READ_BOOL_OR_RETURN(&sps->sps_affine_amvr_enabled_flag);
    }
    READ_BOOL_OR_RETURN(&sps->sps_affine_prof_enabled_flag);
    if (sps->sps_affine_prof_enabled_flag) {
      READ_BOOL_OR_RETURN(&sps->sps_prof_control_present_in_ph_flag);
    }
  }

  READ_BOOL_OR_RETURN(&sps->sps_bcw_enabled_flag);
  READ_BOOL_OR_RETURN(&sps->sps_ciip_enabled_flag);

  // Equation 58.
  int max_num_merge_cand = 6 - sps->sps_six_minus_max_num_merge_cand;
  if (max_num_merge_cand >= 2) {
    READ_BOOL_OR_RETURN(&sps->sps_gpm_enabled_flag);
  }
  if (sps->sps_gpm_enabled_flag && max_num_merge_cand >= 3) {
    READ_UE_OR_RETURN(&sps->sps_max_num_merge_cand_minus_max_num_gpm_cand);
    IN_RANGE_OR_RETURN(sps->sps_max_num_merge_cand_minus_max_num_gpm_cand, 0,
                       max_num_merge_cand - 2);
  }
  READ_UE_OR_RETURN(&sps->sps_log2_parallel_merge_level_minus2);
  IN_RANGE_OR_RETURN(sps->sps_log2_parallel_merge_level_minus2, 0,
                     sps->ctb_log2_size_y - 2);

  READ_BOOL_OR_RETURN(&sps->sps_isp_enabled_flag);
  READ_BOOL_OR_RETURN(&sps->sps_mrl_enabled_flag);
  READ_BOOL_OR_RETURN(&sps->sps_mip_enabled_flag);
  if (sps->sps_chroma_format_idc != 0) {
    READ_BOOL_OR_RETURN(&sps->sps_cclm_enabled_flag);
  }
  if (sps->sps_chroma_format_idc == 1) {
    READ_BOOL_OR_RETURN(&sps->sps_chroma_horizontal_collocated_flag);
    READ_BOOL_OR_RETURN(&sps->sps_chroma_vertical_collocated_flag);
  } else {
    sps->sps_chroma_horizontal_collocated_flag = 1;
    sps->sps_chroma_vertical_collocated_flag = 1;
  }
  READ_BOOL_OR_RETURN(&sps->sps_palette_enabled_flag);
  if (sps->sps_chroma_format_idc == 3 &&
      !sps->sps_max_luma_transform_size_64_flag) {
    READ_BOOL_OR_RETURN(&sps->sps_act_enabled_flag);
  }

  if (sps->sps_transform_skip_enabled_flag || sps->sps_palette_enabled_flag) {
    READ_UE_OR_RETURN(&sps->sps_min_qp_prime_ts);
    IN_RANGE_OR_RETURN(sps->sps_min_qp_prime_ts, 0, 8);
  }
  READ_BOOL_OR_RETURN(&sps->sps_ibc_enabled_flag);
  if (sps->sps_ibc_enabled_flag) {
    READ_UE_OR_RETURN(&sps->sps_six_minus_max_num_ibc_merge_cand);
    IN_RANGE_OR_RETURN(sps->sps_six_minus_max_num_ibc_merge_cand, 0, 5);
  }

  READ_BOOL_OR_RETURN(&sps->sps_ladf_enabled_flag);
  if (sps->sps_ladf_enabled_flag) {
    READ_BITS_OR_RETURN(2, &sps->sps_num_ladf_intervals_minus2);
    IN_RANGE_OR_RETURN(sps->sps_num_ladf_intervals_minus2, 0, 3);
    READ_SE_OR_RETURN(&sps->sps_ladf_lowest_interval_qp_offset);
    IN_RANGE_OR_RETURN(sps->sps_ladf_lowest_interval_qp_offset, -63, 63);
    for (int i = 0; i < sps->sps_num_ladf_intervals_minus2 + 1; i++) {
      READ_SE_OR_RETURN(&sps->sps_ladf_qp_offset[i]);
      IN_RANGE_OR_RETURN(sps->sps_ladf_qp_offset[i], -63, 63);
      READ_UE_OR_RETURN(&sps->sps_ladf_delta_threshold_minus1[i]);
      IN_RANGE_OR_RETURN(sps->sps_ladf_delta_threshold_minus1[i], 0,
                         std::pow(2, sps->sps_bitdepth_minus8 + 8) - 3);
    }
  }

  READ_BOOL_OR_RETURN(&sps->sps_explicit_scaling_list_enabled_flag);
  if (sps->sps_lfnst_enabled_flag &&
      sps->sps_explicit_scaling_list_enabled_flag) {
    READ_BOOL_OR_RETURN(&sps->sps_scaling_matrix_for_lfnst_disabled_flag);
  }
  if (sps->sps_act_enabled_flag &&
      sps->sps_explicit_scaling_list_enabled_flag) {
    READ_BOOL_OR_RETURN(
        &sps->sps_scaling_matrix_for_alternative_colour_space_disabled_flag);
  }
  if (sps->sps_scaling_matrix_for_alternative_colour_space_disabled_flag) {
    READ_BOOL_OR_RETURN(&sps->sps_scaling_matrix_designated_colour_space_flag);
  }
  READ_BOOL_OR_RETURN(&sps->sps_dep_quant_enabled_flag);
  READ_BOOL_OR_RETURN(&sps->sps_sign_data_hiding_enabled_flag);
  READ_BOOL_OR_RETURN(&sps->sps_virtual_boundaries_enabled_flag);
  if (sps->sps_virtual_boundaries_enabled_flag) {
    READ_BOOL_OR_RETURN(&sps->sps_virtual_boundaries_present_flag);
    if (sps->sps_virtual_boundaries_present_flag) {
      READ_UE_OR_RETURN(&sps->sps_num_ver_virtual_boundaries);
      IN_RANGE_OR_RETURN(sps->sps_num_ver_virtual_boundaries, 0,
                         (sps->sps_pic_width_max_in_luma_samples <= 8) ? 0 : 3);
      for (int i = 0; i < sps->sps_num_ver_virtual_boundaries; i++) {
        READ_UE_OR_RETURN(&sps->sps_virtual_boundary_pos_x_minus1[i]);
        IN_RANGE_OR_RETURN(
            sps->sps_virtual_boundary_pos_x_minus1[i], 0,
            (sps->sps_pic_width_max_in_luma_samples + 7) / 8 - 2);
      }
      READ_UE_OR_RETURN(&sps->sps_num_hor_virtual_boundaries);
      IN_RANGE_OR_RETURN(
          sps->sps_num_hor_virtual_boundaries, 0,
          (sps->sps_pic_height_max_in_luma_samples <= 8) ? 0 : 3);
      for (int i = 0; i < sps->sps_num_hor_virtual_boundaries; i++) {
        READ_UE_OR_RETURN(&sps->sps_virtual_boundary_pos_y_minus1[i]);
        IN_RANGE_OR_RETURN(
            sps->sps_virtual_boundary_pos_y_minus1[i], 0,
            (sps->sps_pic_height_max_in_luma_samples + 7) / 8 - 2);
      }
    }
  }

  if (sps->sps_ptl_dpb_hrd_params_present_flag) {
    READ_BOOL_OR_RETURN(&sps->sps_timing_hrd_params_present_flag);
    if (sps->sps_timing_hrd_params_present_flag) {
      ParseGeneralTimingHrdParameters(&sps->general_timing_hrd_parameters);
      if (sps->sps_max_sublayers_minus1 > 0) {
        READ_BOOL_OR_RETURN(&sps->sps_sublayer_cpb_params_present_flag);
      }
      int first_sublayer = sps->sps_sublayer_cpb_params_present_flag
                               ? 0
                               : sps->sps_max_sublayers_minus1;
      ParseOlsTimingHrdParameters(first_sublayer, sps->sps_max_sublayers_minus1,
                                  sps->general_timing_hrd_parameters,
                                  &sps->ols_timing_hrd_parameters);
      if (!sps->sps_sublayer_cpb_params_present_flag) {
        for (int i = 0; i < sps->sps_max_sublayers_minus1; i++) {
          sps->ols_timing_hrd_parameters.element_duration_in_tc_minus1[i] =
              sps->ols_timing_hrd_parameters
                  .element_duration_in_tc_minus1[first_sublayer];
          sps->ols_timing_hrd_parameters.fixed_pic_rate_general_flag[i] =
              sps->ols_timing_hrd_parameters
                  .fixed_pic_rate_general_flag[first_sublayer];
          sps->ols_timing_hrd_parameters.fixed_pic_rate_within_cvs_flag[i] =
              sps->ols_timing_hrd_parameters
                  .fixed_pic_rate_within_cvs_flag[first_sublayer];
          sps->ols_timing_hrd_parameters.low_delay_hrd_flag[i] =
              sps->ols_timing_hrd_parameters.low_delay_hrd_flag[first_sublayer];
          memcpy(
              &(sps->ols_timing_hrd_parameters.nal_sublayer_hrd_parameters[i]),
              &(sps->ols_timing_hrd_parameters
                    .nal_sublayer_hrd_parameters[first_sublayer]),
              sizeof(sps->ols_timing_hrd_parameters
                         .nal_sublayer_hrd_parameters[first_sublayer]));
          memcpy(
              &(sps->ols_timing_hrd_parameters.vcl_sublayer_hrd_parameters[i]),
              &(sps->ols_timing_hrd_parameters
                    .vcl_sublayer_hrd_parameters[first_sublayer]),
              sizeof(sps->ols_timing_hrd_parameters
                         .vcl_sublayer_hrd_parameters[first_sublayer]));
        }
      }
    }
  }

  READ_BOOL_OR_RETURN(&sps->sps_field_seq_flag);
  READ_BOOL_OR_RETURN(&sps->sps_vui_parameters_present_flag);
  if (sps->sps_vui_parameters_present_flag) {
    READ_UE_OR_RETURN(&sps->sps_vui_payload_size_minus1);
    IN_RANGE_OR_RETURN(sps->sps_vui_payload_size_minus1, 0, 1023);

    BYTE_ALIGNMENT();
    ParseVuiPayload(sps->sps_vui_payload_size_minus1 + 1, *sps,
                    &sps->vui_parameters);
  }

  READ_BOOL_OR_RETURN(&sps->sps_extension_flag);
  if (sps->sps_extension_flag) {
    READ_BOOL_OR_RETURN(&sps->sps_range_extension_flag);
    SKIP_BITS_OR_RETURN(7);
    if (sps->sps_range_extension_flag) {
      READ_BOOL_OR_RETURN(
          &sps->sps_range_extension.sps_extended_precision_flag);
      if (sps->sps_range_extension.sps_extended_precision_flag) {
        READ_BOOL_OR_RETURN(
            &sps->sps_range_extension
                 .sps_ts_residual_coding_rice_present_in_sh_flag);
      }
      READ_BOOL_OR_RETURN(
          &sps->sps_range_extension.sps_rrc_rice_extension_flag);
      READ_BOOL_OR_RETURN(&sps->sps_range_extension
                               .sps_persistent_rice_adaptation_enabled_flag);
      READ_BOOL_OR_RETURN(
          &sps->sps_range_extension.sps_reverse_last_sig_coeff_enabled_flag);
    }
  }
  // Stop here. Skip trailing bits at the end of SPS when sps_extension_7bits
  // is true.

  // If an SPS with the same id already exists, replace it.
  *sps_id = sps->sps_seq_parameter_set_id;
  active_sps_[*sps_id] = std::move(sps);

  return res;
}

// Picture parameter set contains information that's not changed frequently
// across pictures, thus typically shared by many pictures. Since VVC
// supports adaptive resolution change, the width and height information may be
// in PPS instead of SPS. Also PPS includes information of reference picture
// resamping scaling window, layout of tiles and rectangular slices, default
// numbers of current active RPL entries, deblocking params/QP initials at
// picture level, as well as many other flags.
H266Parser::Result H266Parser::ParsePPS(const H266NALU& nalu, int* pps_id) {
  // 7.3.2.5
  DVLOG(4) << "Parsing PPS";
  DCHECK(pps_id);
  *pps_id = -1;

  std::unique_ptr<H266PPS> pps = std::make_unique<H266PPS>();

  READ_BITS_OR_RETURN(6, &pps->pps_pic_parameter_set_id);
  READ_BITS_OR_RETURN(4, &pps->pps_seq_parameter_set_id);
  IN_RANGE_OR_RETURN(pps->pps_seq_parameter_set_id, 0, 15);
  const H266SPS* sps = GetSPS(pps->pps_seq_parameter_set_id);
  if (!sps) {
    return kMissingParameterSet;
  }

  READ_BOOL_OR_RETURN(&pps->pps_mixed_nalu_types_in_pic_flag);
  READ_UE_OR_RETURN(&pps->pps_pic_width_in_luma_samples);
  IN_RANGE_OR_RETURN(pps->pps_pic_width_in_luma_samples, 1,
                     sps->sps_pic_width_max_in_luma_samples);

  READ_UE_OR_RETURN(&pps->pps_pic_height_in_luma_samples);
  IN_RANGE_OR_RETURN(pps->pps_pic_height_in_luma_samples, 1,
                     sps->sps_pic_height_max_in_luma_samples);

  int multiplier = std::max(8, sps->min_cb_size_y);
  if ((pps->pps_pic_width_in_luma_samples % multiplier != 0) ||
      (pps->pps_pic_height_in_luma_samples % multiplier != 0)) {
    DVLOG(1) << "Invalid pps pic width/height";
    return kInvalidStream;
  }

  if (!sps->sps_res_change_in_clvs_allowed_flag &&
      (pps->pps_pic_width_in_luma_samples !=
           sps->sps_pic_width_max_in_luma_samples ||
       (pps->pps_pic_height_in_luma_samples !=
        sps->sps_pic_height_max_in_luma_samples))) {
    DVLOG(1) << "pps pic width/height is different from sps pic width/height "
                "when sps_res_change_in_clvs_allowed_flag is false.";
    return kInvalidStream;
  }
  if (sps->sps_ref_wraparound_enabled_flag) {
    LE_OR_RETURN(sps->ctb_size_y / sps->min_cb_size_y + 1,
                 pps->pps_pic_width_in_luma_samples / sps->min_cb_size_y - 1);
  }

  pps->pic_width_in_ctbs_y =
      (pps->pps_pic_width_in_luma_samples + sps->ctb_size_y - 1) /
      sps->ctb_size_y;
  pps->pic_height_in_ctbs_y =
      (pps->pps_pic_height_in_luma_samples + sps->ctb_size_y - 1) /
      sps->ctb_size_y;
  pps->pic_size_in_ctbs_y =
      pps->pic_width_in_ctbs_y * pps->pic_height_in_ctbs_y;
  pps->pic_width_in_min_cbs_y =
      pps->pps_pic_width_in_luma_samples / sps->min_cb_size_y;
  pps->pic_height_in_min_cbs_y =
      pps->pps_pic_height_in_luma_samples / sps->min_cb_size_y;
  pps->pic_size_in_min_cbs_y =
      pps->pic_width_in_min_cbs_y * pps->pic_height_in_min_cbs_y;
  pps->pic_size_in_samples_y =
      pps->pps_pic_width_in_luma_samples * pps->pps_pic_height_in_luma_samples;
  pps->pic_width_in_samples_c =
      pps->pps_pic_width_in_luma_samples / sps->sub_width_c;
  pps->pic_height_in_samples_c =
      pps->pps_pic_height_in_luma_samples / sps->sub_width_c;

  READ_BOOL_OR_RETURN(&pps->pps_conformance_window_flag);
  if (pps->pps_pic_width_in_luma_samples ==
          sps->sps_pic_width_max_in_luma_samples &&
      pps->pps_pic_height_in_luma_samples ==
          sps->sps_pic_height_max_in_luma_samples) {
    if (pps->pps_conformance_window_flag) {
      DVLOG(1) << "Invalid pps_conformance_window_flag.";
      return kInvalidStream;
    }
  }
  if (pps->pps_conformance_window_flag) {
    READ_UE_OR_RETURN(&pps->pps_conf_win_left_offset);
    READ_UE_OR_RETURN(&pps->pps_conf_win_right_offset);
    READ_UE_OR_RETURN(&pps->pps_conf_win_top_offset);
    READ_UE_OR_RETURN(&pps->pps_conf_win_bottom_offset);
    // Verify cropping window.
    if ((sps->sub_width_c *
             (pps->pps_conf_win_left_offset + pps->pps_conf_win_right_offset) >=
         pps->pps_pic_width_in_luma_samples) ||
        (sps->sub_height_c *
             (pps->pps_conf_win_top_offset + pps->pps_conf_win_bottom_offset) >=
         pps->pps_pic_height_in_luma_samples)) {
      DVLOG(1) << "Invalid cropping window in PPS.";
      return kInvalidStream;
    }
  } else {
    if (pps->pps_pic_width_in_luma_samples ==
            sps->sps_pic_width_max_in_luma_samples &&
        pps->pps_pic_height_in_luma_samples ==
            sps->sps_pic_height_max_in_luma_samples) {
      pps->pps_conf_win_left_offset = sps->sps_conf_win_left_offset;
      pps->pps_conf_win_right_offset = sps->sps_conf_win_right_offset;
      pps->pps_conf_win_top_offset = sps->sps_conf_win_top_offset;
      pps->pps_conf_win_bottom_offset = sps->sps_conf_win_bottom_offset;
    }
  }

  READ_BOOL_OR_RETURN(&pps->pps_scaling_window_explicit_signaling_flag);
  if (!sps->sps_ref_pic_resampling_enabled_flag &&
      pps->pps_scaling_window_explicit_signaling_flag) {
    DVLOG(1) << "Scaling window cannot be explicitly signaled in PPS if ref "
                "picture resampling is disabled.";
    return kInvalidStream;
  }
  if (pps->pps_scaling_window_explicit_signaling_flag) {
    READ_SE_OR_RETURN(&pps->pps_scaling_win_left_offset);
    READ_SE_OR_RETURN(&pps->pps_scaling_win_right_offset);
    READ_SE_OR_RETURN(&pps->pps_scaling_win_top_offset);
    READ_SE_OR_RETURN(&pps->pps_scaling_win_bottom_offset);
    // Verify scaling window.
    IN_RANGE_OR_RETURN(sps->sub_width_c * pps->pps_scaling_win_left_offset,
                       -15 * pps->pps_pic_width_in_luma_samples,
                       pps->pps_pic_width_in_luma_samples - 1);
    IN_RANGE_OR_RETURN(sps->sub_width_c * pps->pps_scaling_win_right_offset,
                       -15 * pps->pps_pic_width_in_luma_samples,
                       pps->pps_pic_width_in_luma_samples - 1);
    IN_RANGE_OR_RETURN(sps->sub_height_c * pps->pps_scaling_win_top_offset,
                       -15 * pps->pps_pic_height_in_luma_samples,
                       pps->pps_pic_height_in_luma_samples - 1);
    IN_RANGE_OR_RETURN(sps->sub_height_c * pps->pps_scaling_win_bottom_offset,
                       -15 * pps->pps_pic_height_in_luma_samples,
                       pps->pps_pic_height_in_luma_samples - 1);
    IN_RANGE_OR_RETURN(sps->sub_width_c * (pps->pps_scaling_win_left_offset +
                                           pps->pps_scaling_win_right_offset),
                       -15 * pps->pps_pic_width_in_luma_samples,
                       pps->pps_pic_width_in_luma_samples - 1);
    IN_RANGE_OR_RETURN(sps->sub_height_c * (pps->pps_scaling_win_top_offset +
                                            pps->pps_scaling_win_bottom_offset),
                       -15 * pps->pps_pic_height_in_luma_samples,
                       pps->pps_pic_height_in_luma_samples - 1);
  } else {
    pps->pps_scaling_win_left_offset = pps->pps_conf_win_left_offset;
    pps->pps_scaling_win_right_offset = pps->pps_conf_win_right_offset;
    pps->pps_scaling_win_top_offset = pps->pps_conf_win_top_offset;
    pps->pps_scaling_win_bottom_offset = pps->pps_conf_win_bottom_offset;
  }

  READ_BOOL_OR_RETURN(&pps->pps_output_flag_present_flag);
  READ_BOOL_OR_RETURN(&pps->pps_no_pic_partition_flag);
  if (sps->sps_num_subpics_minus1 > 0 ||
      pps->pps_mixed_nalu_types_in_pic_flag == 1) {
    TRUE_OR_RETURN(!pps->pps_no_pic_partition_flag);
  }

  READ_BOOL_OR_RETURN(&pps->pps_subpic_id_mapping_present_flag);
  if (!sps->sps_subpic_id_mapping_explicitly_signaled_flag ||
      sps->sps_subpic_id_mapping_present_flag) {
    TRUE_OR_RETURN(!pps->pps_subpic_id_mapping_present_flag);
  } else {
    TRUE_OR_RETURN(pps->pps_subpic_id_mapping_present_flag);
  }

  if (pps->pps_subpic_id_mapping_present_flag) {
    if (!pps->pps_no_pic_partition_flag) {
      READ_UE_OR_RETURN(&pps->pps_num_subpics_minus1);
      TRUE_OR_RETURN(pps->pps_num_subpics_minus1 ==
                     sps->sps_num_subpics_minus1);
    }

    READ_UE_OR_RETURN(&pps->pps_subpic_id_len_minus1);
    TRUE_OR_RETURN(pps->pps_subpic_id_len_minus1 ==
                   sps->sps_subpic_id_len_minus1);
    for (int i = 0; i <= pps->pps_num_subpics_minus1; i++) {
      READ_BITS_OR_RETURN(pps->pps_subpic_id_len_minus1 + 1,
                          &pps->pps_subpic_id[i]);
    }
  }

  // Handle tile/slice layout information.
  if (!pps->pps_no_pic_partition_flag) {
    READ_BITS_OR_RETURN(2, &pps->pps_log2_ctu_size_minus5);
    // CTU size info in PPS must be exactly the same as SPS.
    TRUE_OR_RETURN(pps->pps_log2_ctu_size_minus5 ==
                   sps->sps_log2_ctu_size_minus5);
    READ_UE_OR_RETURN(&pps->pps_num_exp_tile_columns_minus1);
    IN_RANGE_OR_RETURN(pps->pps_num_exp_tile_columns_minus1, 0,
                       pps->pic_width_in_ctbs_y - 1);
    READ_UE_OR_RETURN(&pps->pps_num_exp_tile_rows_minus1);
    IN_RANGE_OR_RETURN(pps->pps_num_exp_tile_rows_minus1, 0,
                       pps->pic_height_in_ctbs_y - 1);

    // Clause 6.5.1, equation 14 & equation 15: calculate number
    // of tile columns and rows.
    // For those tile column/row sizes not explicitly signalled,
    // use the last explicitly signalled size as the implicit
    // tile size, unless it is the last tile column/row.
    int remaining_width_in_ctbs_y = pps->pic_width_in_ctbs_y;
    int explicit_tile_width = 0;
    for (int i = 0; i <= pps->pps_num_exp_tile_columns_minus1; i++) {
      READ_UE_OR_RETURN(&pps->pps_tile_column_width_minus1[i]);
      IN_RANGE_OR_RETURN(pps->pps_tile_column_width_minus1[i], 0,
                         pps->pic_width_in_ctbs_y - 1);
      explicit_tile_width += pps->pps_tile_column_width_minus1[i] + 1;
    }
    remaining_width_in_ctbs_y -= explicit_tile_width;
    int uniform_tile_col_width = pps->pps_tile_column_width_minus1
                                     [pps->pps_num_exp_tile_columns_minus1] +
                                 1;
    int next_tile_column_idx = pps->pps_num_exp_tile_columns_minus1 + 1;
    while (remaining_width_in_ctbs_y >= uniform_tile_col_width) {
      pps->pps_tile_column_width_minus1[next_tile_column_idx++] =
          uniform_tile_col_width - 1;
      remaining_width_in_ctbs_y -= uniform_tile_col_width;
    }
    if (remaining_width_in_ctbs_y > 0) {
      pps->pps_tile_column_width_minus1[next_tile_column_idx++] =
          remaining_width_in_ctbs_y;
    }
    pps->num_tile_columns = next_tile_column_idx;

    int remaining_height_in_ctbs_y = pps->pic_height_in_ctbs_y;
    int explicit_tile_height = 0;
    for (int i = 0; i <= pps->pps_num_exp_tile_rows_minus1; i++) {
      READ_UE_OR_RETURN(&pps->pps_tile_row_height_minus1[i]);
      IN_RANGE_OR_RETURN(pps->pps_tile_row_height_minus1[i], 0,
                         pps->pic_height_in_ctbs_y - 1);
      explicit_tile_height += pps->pps_tile_row_height_minus1[i] + 1;
    }
    remaining_height_in_ctbs_y -= explicit_tile_height;
    int uniform_tile_row_height =
        pps->pps_tile_row_height_minus1[pps->pps_num_exp_tile_rows_minus1] + 1;
    int next_tile_row_idx = pps->pps_num_exp_tile_rows_minus1 + 1;
    while (remaining_height_in_ctbs_y >= uniform_tile_row_height) {
      pps->pps_tile_row_height_minus1[next_tile_row_idx++] =
          uniform_tile_row_height - 1;
      remaining_height_in_ctbs_y -= uniform_tile_row_height;
    }
    if (remaining_height_in_ctbs_y > 0) {
      pps->pps_tile_row_height_minus1[next_tile_row_idx++] =
          remaining_height_in_ctbs_y;
    }
    pps->num_tile_rows = next_tile_row_idx;
    pps->num_tiles_in_pic = pps->num_tile_columns * pps->num_tile_rows;

    if (pps->num_tiles_in_pic > 1) {
      READ_BOOL_OR_RETURN(&pps->pps_loop_filter_across_tiles_enabled_flag);
      READ_BOOL_OR_RETURN(&pps->pps_rect_slice_flag);
    } else {
      pps->pps_rect_slice_flag = 1;
    }
    if (pps->pps_rect_slice_flag) {
      READ_BOOL_OR_RETURN(&pps->pps_single_slice_per_subpic_flag);
    }

    // When pps_rect_slice_flag is 0, PPS will not contain the slice
    // width/height measured in units of tiles. In that case, VVC depends on
    // sh_slice_address and sh_num_tiles_in_slice_minus1 syntax for the slice
    // layout in the picture.
    if (pps->pps_rect_slice_flag && !pps->pps_single_slice_per_subpic_flag) {
      READ_UE_OR_RETURN(&pps->pps_num_slices_in_pic_minus1);
      IN_RANGE_OR_RETURN(pps->pps_num_slices_in_pic_minus1, 0,
                         sps->profile_tier_level.MaxSlicesPerAu() - 1);

      if (pps->pps_num_slices_in_pic_minus1 > 1) {
        // When this flag is 0, all pictures referring to current PPS are
        // partitioned into rectangular slice columns and rows in slice raster
        // order. Otherwise all rectangular slices in picture are specified in
        // the order by the values of pps_tile_idx_delta_val[i] in increasing
        // values of i.
        READ_BOOL_OR_RETURN(&pps->pps_tile_idx_delta_present_flag);
      }

      int tile_idx = 0, tile_x = 0, tile_y = 0, ctu_x = 0, ctu_y = 0;
      int slice_top_left_ctu_x[kMaxSlices];
      int slice_top_left_ctu_y[kMaxSlices];
      int i;

      for (i = 0; i < pps->pps_num_slices_in_pic_minus1; i++) {
        // Equation 21. tile_x is the 0-based index horizontally; tile_y is the
        // 0-based tile row.
        tile_x = tile_idx % pps->num_tile_columns;
        tile_y = tile_idx / pps->num_tile_columns;
        ctu_x = ctu_y = 0;
        if (tile_x != pps->num_tile_columns - 1) {
          READ_UE_OR_RETURN(&pps->pps_slice_width_in_tiles_minus1[i]);
          IN_RANGE_OR_RETURN(pps->pps_slice_width_in_tiles_minus1[i], 0,
                             pps->num_tile_columns - 1);
        }

        if ((tile_y != pps->num_tile_rows - 1) &&
            (pps->pps_tile_idx_delta_present_flag || tile_x == 0)) {
          READ_UE_OR_RETURN(&pps->pps_slice_height_in_tiles_minus1[i]);
          IN_RANGE_OR_RETURN(pps->pps_slice_height_in_tiles_minus1[i], 0,
                             pps->num_tile_rows - 1);
        } else {
          if (tile_y == pps->num_tile_rows - 1) {
            pps->pps_slice_height_in_tiles_minus1[i] = 0;
          } else {
            if (i > 0) {
              pps->pps_slice_height_in_tiles_minus1[i] =
                  pps->pps_slice_height_in_tiles_minus1[i - 1];
            }
          }
        }

        for (int j = 0; j < tile_x; j++) {
          ctu_x += pps->pps_tile_column_width_minus1[j] + 1;
        }
        for (int j = 0; j < tile_y; j++) {
          ctu_y += pps->pps_tile_row_height_minus1[j] + 1;
        }

        int num_slices_in_tile = 0, uniform_slice_height = 0;
        remaining_height_in_ctbs_y = 0;
        if (pps->pps_slice_width_in_tiles_minus1[i] == 0 &&
            pps->pps_slice_height_in_tiles_minus1[i] == 0 &&
            pps->pps_tile_row_height_minus1[tile_y] > 0) {
          remaining_height_in_ctbs_y =
              pps->pps_tile_row_height_minus1[tile_y] + 1;
          READ_UE_OR_RETURN(&pps->pps_num_exp_slices_in_tile[i]);
          IN_RANGE_OR_RETURN(pps->pps_num_exp_slices_in_tile[i], 0,
                             pps->pps_tile_row_height_minus1[tile_y]);

          if (!pps->pps_num_exp_slices_in_tile[i]) {
            slice_top_left_ctu_x[i] = ctu_x;
            slice_top_left_ctu_y[i] = ctu_y;
            pps->slice_height_in_ctus[i] =
                pps->pps_tile_row_height_minus1[tile_y] + 1;
            num_slices_in_tile = 1;
          } else {
            int slice_height_in_ctus = 0, j;
            for (j = 0; j < pps->pps_num_exp_slices_in_tile[i]; j++) {
              READ_UE_OR_RETURN(
                  &pps->pps_exp_slice_height_in_ctus_minus1[i][j]);
              IN_RANGE_OR_RETURN(pps->pps_exp_slice_height_in_ctus_minus1[i][j],
                                 0, pps->pps_tile_row_height_minus1[tile_y]);
              slice_height_in_ctus =
                  pps->pps_exp_slice_height_in_ctus_minus1[i][j] + 1;
              LE_OR_RETURN(i + j, kMaxSlices - 1);
              pps->slice_height_in_ctus[i + j] = slice_height_in_ctus;
              slice_top_left_ctu_x[i + j] = ctu_x;
              slice_top_left_ctu_y[i + j] = ctu_y;
              ctu_y += slice_height_in_ctus;
              remaining_height_in_ctbs_y -= slice_height_in_ctus;
            }
            uniform_slice_height =
                1 + pps->pps_exp_slice_height_in_ctus_minus1[i][j - 1];
            while (remaining_height_in_ctbs_y > uniform_slice_height) {
              LE_OR_RETURN(i + j, kMaxSlices - 1);
              pps->slice_height_in_ctus[i + j] = uniform_slice_height;
              slice_top_left_ctu_x[i + j] = ctu_x;
              slice_top_left_ctu_y[i + j] = ctu_y;
              ctu_y += uniform_slice_height;
              j++;
            }
            if (remaining_height_in_ctbs_y > 0) {
              LE_OR_RETURN(i + j, kMaxSlices - 1);
              pps->slice_height_in_ctus[i + j] = remaining_height_in_ctbs_y;
              slice_top_left_ctu_x[i + j] = ctu_x;
              slice_top_left_ctu_y[i + j] = ctu_y;
              j++;
            }
            num_slices_in_tile = j;
          }
          i += num_slices_in_tile - 1;
        } else {
          int height = 0;
          pps->pps_num_exp_slices_in_tile[i] = 0;
          for (int j = 0; j <= pps->pps_slice_height_in_tiles_minus1[i]; j++) {
            height += pps->pps_tile_row_height_minus1[tile_y + j] + 1;
          }
          pps->slice_height_in_ctus[i] = height;
          slice_top_left_ctu_x[i] = ctu_x;
          slice_top_left_ctu_y[i] = ctu_y;
        }

        // Fetch next slice's tile idx, which is used for calculating next
        // tile_x & tile_y.
        if (i < pps->pps_num_slices_in_pic_minus1) {
          if (pps->pps_tile_idx_delta_present_flag) {
            READ_SE_OR_RETURN(&pps->pps_tile_idx_delta_val[i]);
            IN_RANGE_OR_RETURN(pps->pps_tile_idx_delta_val[i], 1 - tile_idx,
                               pps->num_tiles_in_pic - 1 - tile_idx);
            TRUE_OR_RETURN(pps->pps_tile_idx_delta_val[i] != 0);
            tile_idx += pps->pps_tile_idx_delta_val[i];
          } else {
            pps->pps_tile_idx_delta_val[i] = 0;
            tile_idx += pps->pps_slice_width_in_tiles_minus1[i] + 1;
            if (tile_idx % pps->num_tile_columns == 0) {
              tile_idx += pps->pps_slice_height_in_tiles_minus1[i] *
                          pps->num_tile_columns;
            }
          }
        }
      }

      // Handle the last slice.
      if (i == pps->pps_num_slices_in_pic_minus1) {
        int height = 0;
        tile_x = tile_idx % pps->num_tile_columns;
        tile_y = tile_idx / pps->num_tile_columns;

        ctu_x = ctu_y = 0;
        for (int j = 0; j < tile_x; j++) {
          ctu_x += pps->pps_tile_column_width_minus1[j] + 1;
        }
        for (int j = 0; j < tile_y; j++) {
          ctu_y += pps->pps_tile_row_height_minus1[j] + 1;
        }
        slice_top_left_ctu_x[i] = ctu_x;
        slice_top_left_ctu_y[i] = ctu_y;
        pps->pps_slice_width_in_tiles_minus1[i] =
            pps->num_tile_columns - tile_x - 1;
        pps->pps_slice_height_in_tiles_minus1[i] =
            pps->num_tile_rows - tile_y - 1;

        for (int j = 0; j <= pps->pps_slice_height_in_tiles_minus1[i]; j++) {
          height += pps->pps_tile_row_height_minus1[tile_y + j] + 1;
        }
        pps->slice_height_in_ctus[i] = height;
        pps->pps_num_exp_slices_in_tile[i] = 0;
      }

      for (int p = 0; p <= sps->sps_num_subpics_minus1; p++) {
        pps->num_slices_in_subpic[p] = 0;
        for (int k = 0; k <= pps->pps_num_slices_in_pic_minus1; k++) {
          int pos_x = 0, pos_y = 0;
          pos_x = slice_top_left_ctu_x[k];
          pos_y = slice_top_left_ctu_y[k];
          if ((pos_x >= sps->sps_subpic_ctu_top_left_x[p]) &&
              (pos_x < sps->sps_subpic_ctu_top_left_x[p] +
                           sps->sps_subpic_width_minus1[p] + 1) &&
              (pos_y >= sps->sps_subpic_ctu_top_left_y[p]) &&
              (pos_y < sps->sps_subpic_ctu_top_left_y[p] +
                           sps->sps_subpic_height_minus1[p] + 1)) {
            pps->num_slices_in_subpic[p]++;
          }
        }
      }
      // pps_rect_slice_flag && !pps_single_slice_per_subpic_flag
    }

    if (!pps->pps_rect_slice_flag || pps->pps_single_slice_per_subpic_flag ||
        pps->pps_num_slices_in_pic_minus1 > 0) {
      READ_BOOL_OR_RETURN(&pps->pps_loop_filter_across_slices_enabled_flag);
    } else {
      pps->pps_loop_filter_across_slices_enabled_flag = 0;
    }

  } else {
    // pps_no_pic_partition_flag = 1, so that tiling and slicing layout
    // need to be inferred.
    pps->pps_num_exp_tile_columns_minus1 = 0;
    pps->pps_num_exp_tile_rows_minus1 = 0;
    pps->pps_tile_column_width_minus1[0] = pps->pic_width_in_ctbs_y - 1;
    pps->pps_tile_row_height_minus1[0] = pps->pic_height_in_ctbs_y - 1;
    pps->pps_loop_filter_across_tiles_enabled_flag = 0;
    pps->pps_rect_slice_flag = 1;
    pps->pps_single_slice_per_subpic_flag = 1;
    // Spec requires when pps_no_pic_partition_flag is 1,
    // pps_num_slices_in_pic_minus1 should be 0; But at the same time, if
    // pps_single_slcie_per_subpic_flag is 1, it should be inferred to
    // sps_num_subpics_minus1.
    pps->pps_num_slices_in_pic_minus1 = 0;
    pps->pps_tile_idx_delta_present_flag = 0;
  }

  READ_BOOL_OR_RETURN(&pps->pps_cabac_init_present_flag);
  for (int i = 0; i < 2; i++) {
    READ_UE_OR_RETURN(&pps->pps_num_ref_idx_default_active_minus1[i]);
    IN_RANGE_OR_RETURN(pps->pps_num_ref_idx_default_active_minus1[i], 0, 14);
  }
  READ_BOOL_OR_RETURN(&pps->pps_rpl1_idx_present_flag);
  READ_BOOL_OR_RETURN(&pps->pps_weighted_pred_flag);
  if (!sps->sps_weighted_pred_flag) {
    TRUE_OR_RETURN(!pps->pps_weighted_pred_flag);
  }
  READ_BOOL_OR_RETURN(&pps->pps_weighted_bipred_flag);
  if (!sps->sps_weighted_bipred_flag) {
    TRUE_OR_RETURN(!pps->pps_weighted_bipred_flag);
  }
  READ_BOOL_OR_RETURN(&pps->pps_ref_wraparound_enabled_flag);
  if (sps->sps_ref_pic_resampling_enabled_flag == 0 ||
      sps->ctb_size_y / sps->min_cb_size_y + 1 >
          pps->pps_pic_width_in_luma_samples / sps->min_cb_size_y - 1) {
    TRUE_OR_RETURN(pps->pps_ref_wraparound_enabled_flag == 0);
  }

  if (pps->pps_ref_wraparound_enabled_flag) {
    READ_UE_OR_RETURN(&pps->pps_pic_width_minus_wraparound_offset);
    IN_RANGE_OR_RETURN(
        pps->pps_pic_width_minus_wraparound_offset, 0,
        (pps->pps_pic_width_in_luma_samples / sps->min_cb_size_y) -
            (sps->ctb_size_y / sps->min_cb_size_y) - 2);
  }

  // QP
  READ_SE_OR_RETURN(&pps->pps_init_qp_minus26);
  IN_RANGE_OR_RETURN(pps->pps_init_qp_minus26, -(26 + sps->qp_bd_offset), 37);
  READ_BOOL_OR_RETURN(&pps->pps_cu_qp_delta_enabled_flag);
  READ_BOOL_OR_RETURN(&pps->pps_chroma_tool_offsets_present_flag);
  if (sps->sps_chroma_format_idc == 0) {
    TRUE_OR_RETURN(pps->pps_chroma_tool_offsets_present_flag == 0);
  }

  if (pps->pps_chroma_tool_offsets_present_flag) {
    READ_SE_OR_RETURN(&pps->pps_cb_qp_offset);
    READ_SE_OR_RETURN(&pps->pps_cr_qp_offset);
    IN_RANGE_OR_RETURN(pps->pps_cb_qp_offset, -12, 12);
    IN_RANGE_OR_RETURN(pps->pps_cr_qp_offset, -12, 12);
    READ_BOOL_OR_RETURN(&pps->pps_joint_cbcr_qp_offset_present_flag);
    if (sps->sps_chroma_format_idc == 0 ||
        sps->sps_joint_cbcr_enabled_flag == 0) {
      TRUE_OR_RETURN(pps->pps_joint_cbcr_qp_offset_present_flag == 0);
    }

    if (pps->pps_joint_cbcr_qp_offset_present_flag) {
      READ_SE_OR_RETURN(&pps->pps_joint_cbcr_qp_offset_value);
      IN_RANGE_OR_RETURN(pps->pps_joint_cbcr_qp_offset_value, -12, 12);
    }

    READ_BOOL_OR_RETURN(&pps->pps_slice_chroma_qp_offsets_present_flag);
    READ_BOOL_OR_RETURN(&pps->pps_cu_chroma_qp_offset_list_enabled_flag);
    if (pps->pps_cu_chroma_qp_offset_list_enabled_flag) {
      READ_UE_OR_RETURN(&pps->pps_chroma_qp_offset_list_len_minus1);
      IN_RANGE_OR_RETURN(pps->pps_chroma_qp_offset_list_len_minus1, 0, 5);

      for (int i = 0; i <= pps->pps_chroma_qp_offset_list_len_minus1; i++) {
        READ_SE_OR_RETURN(&pps->pps_cb_qp_offset_list[i]);
        READ_SE_OR_RETURN(&pps->pps_cr_qp_offset_list[i]);
        IN_RANGE_OR_RETURN(pps->pps_cb_qp_offset_list[i], -12, 12);
        IN_RANGE_OR_RETURN(pps->pps_cr_qp_offset_list[i], -12, 12);

        if (pps->pps_joint_cbcr_qp_offset_present_flag) {
          READ_SE_OR_RETURN(&pps->pps_joint_cbcr_qp_offset_list[i]);
          IN_RANGE_OR_RETURN(pps->pps_joint_cbcr_qp_offset_list[i], -12, 12);
        } else {
          pps->pps_joint_cbcr_qp_offset_list[i] = 0;
        }
      }
    }
  }

  // Deblocking filter
  READ_BOOL_OR_RETURN(&pps->pps_deblocking_filter_control_present_flag);
  if (pps->pps_deblocking_filter_control_present_flag) {
    READ_BOOL_OR_RETURN(&pps->pps_deblocking_filter_override_enabled_flag);
    READ_BOOL_OR_RETURN(&pps->pps_deblocking_filter_disabled_flag);
    if (!pps->pps_no_pic_partition_flag &&
        pps->pps_deblocking_filter_override_enabled_flag) {
      READ_BOOL_OR_RETURN(&pps->pps_dbf_info_in_ph_flag);
    } else {
      pps->pps_dbf_info_in_ph_flag = 0;
    }
    if (!pps->pps_deblocking_filter_disabled_flag) {
      READ_SE_OR_RETURN(&pps->pps_luma_beta_offset_div2);
      READ_SE_OR_RETURN(&pps->pps_luma_tc_offset_div2);
      IN_RANGE_OR_RETURN(pps->pps_luma_beta_offset_div2, -12, 12);
      IN_RANGE_OR_RETURN(pps->pps_luma_tc_offset_div2, -12, 12);
      if (pps->pps_chroma_tool_offsets_present_flag) {
        READ_SE_OR_RETURN(&pps->pps_cb_beta_offset_div2);
        READ_SE_OR_RETURN(&pps->pps_cb_tc_offset_div2);
        READ_SE_OR_RETURN(&pps->pps_cr_beta_offset_div2);
        READ_SE_OR_RETURN(&pps->pps_cr_tc_offset_div2);
        IN_RANGE_OR_RETURN(pps->pps_cb_beta_offset_div2, -12, 12);
        IN_RANGE_OR_RETURN(pps->pps_cb_tc_offset_div2, -12, 12);
        IN_RANGE_OR_RETURN(pps->pps_cr_beta_offset_div2, -12, 12);
        IN_RANGE_OR_RETURN(pps->pps_cr_tc_offset_div2, -12, 12);
      } else {
        pps->pps_cb_beta_offset_div2 = pps->pps_luma_beta_offset_div2;
        pps->pps_cb_tc_offset_div2 = pps->pps_luma_tc_offset_div2;
        pps->pps_cr_beta_offset_div2 = pps->pps_luma_beta_offset_div2;
        pps->pps_cr_tc_offset_div2 = pps->pps_luma_tc_offset_div2;
      }
    } else {
      pps->pps_luma_beta_offset_div2 = pps->pps_luma_tc_offset_div2 = 0;
    }
  } else {
    pps->pps_deblocking_filter_override_enabled_flag = 0;
    pps->pps_deblocking_filter_disabled_flag = 0;
    pps->pps_dbf_info_in_ph_flag = 0;
    pps->pps_luma_beta_offset_div2 = pps->pps_luma_tc_offset_div2 = 0;
    pps->pps_cb_beta_offset_div2 = pps->pps_luma_beta_offset_div2;
    pps->pps_cb_tc_offset_div2 = pps->pps_luma_tc_offset_div2;
    pps->pps_cr_beta_offset_div2 = pps->pps_luma_beta_offset_div2;
    pps->pps_cr_tc_offset_div2 = pps->pps_luma_tc_offset_div2;
  }

  if (!pps->pps_no_pic_partition_flag) {
    READ_BOOL_OR_RETURN(&pps->pps_rpl_info_in_ph_flag);
    READ_BOOL_OR_RETURN(&pps->pps_sao_info_in_ph_flag);
    READ_BOOL_OR_RETURN(&pps->pps_alf_info_in_ph_flag);

    if ((pps->pps_weighted_pred_flag || pps->pps_weighted_bipred_flag) &&
        pps->pps_rpl_info_in_ph_flag) {
      READ_BOOL_OR_RETURN(&pps->pps_wp_info_in_ph_flag);
    } else {
      pps->pps_wp_info_in_ph_flag = 0;
    }
    READ_BOOL_OR_RETURN(&pps->pps_qp_delta_info_in_ph_flag);
  } else {
    pps->pps_rpl_info_in_ph_flag = 0;
    pps->pps_sao_info_in_ph_flag = 0;
    pps->pps_alf_info_in_ph_flag = 0;
    pps->pps_wp_info_in_ph_flag = 0;
    pps->pps_qp_delta_info_in_ph_flag = 0;
  }

  READ_BOOL_OR_RETURN(&pps->pps_picture_header_extension_present_flag);
  READ_BOOL_OR_RETURN(&pps->pps_slice_header_extension_present_flag);
  READ_BOOL_OR_RETURN(&pps->pps_extension_flag);
  // We stop here.

  // If a PPS with the same id already exists, replace it.
  *pps_id = pps->pps_pic_parameter_set_id;
  active_pps_[*pps_id] = std::move(pps);

  return kOk;
}

// 7.3.2.6 Adaptation parameter set
// APS conveys slice level information which may be shared by
// multiple slices of a picture or slices of different pictures.
// There might be many APSs for a bitstream, and would be typically
// updated very frequently.
H266Parser::Result H266Parser::ParseAPS(const H266NALU& nalu,
                                        int* aps_id,
                                        H266APS::ParamType* type) {
  DCHECK(aps_id);

  int aps_type;
  READ_BITS_OR_RETURN(3, &aps_type);
  IN_RANGE_OR_RETURN(aps_type, 0, 2);
  std::unique_ptr<H266APS> aps = std::make_unique<H266APS>(aps_type);

  aps->nal_unit_type = nalu.nal_unit_type;
  aps->nuh_layer_id = nalu.nuh_layer_id;
  aps->aps_params_type = aps_type;

  READ_BITS_OR_RETURN(5, &aps->aps_adaptation_parameter_set_id);
  if (aps->aps_params_type == H266APS::kAlf ||
      aps->aps_params_type == H266APS::kScalingList) {
    IN_RANGE_OR_RETURN(aps->aps_adaptation_parameter_set_id, 0, 7);
  } else if (aps->aps_params_type == H266APS::kLmcs) {
    IN_RANGE_OR_RETURN(aps->aps_adaptation_parameter_set_id, 0, 3);
  }
  READ_BOOL_OR_RETURN(&aps->aps_chroma_present_flag);

  if (aps->aps_params_type == H266APS::kAlf) {
    // 7.3.2.18: Adaptive loop filter
    H266AlfData* alf_data = &std::get<H266AlfData>(aps->data);
    READ_BOOL_OR_RETURN(&alf_data->alf_luma_filter_signal_flag);
    if (aps->aps_chroma_present_flag) {
      READ_BOOL_OR_RETURN(&alf_data->alf_chroma_filter_signal_flag);
      READ_BOOL_OR_RETURN(&alf_data->alf_cc_cb_filter_signal_flag);
      READ_BOOL_OR_RETURN(&alf_data->alf_cc_cr_filter_signal_flag);
    } else {
      alf_data->alf_chroma_filter_signal_flag = 0;
      alf_data->alf_cc_cb_filter_signal_flag = 0;
      alf_data->alf_cc_cr_filter_signal_flag = 0;
    }
    // 7.4.3.18: at least one of above signal flag should be 1.
    TRUE_OR_RETURN(alf_data->alf_luma_filter_signal_flag ||
                   alf_data->alf_chroma_filter_signal_flag ||
                   alf_data->alf_cc_cb_filter_signal_flag ||
                   alf_data->alf_cc_cr_filter_signal_flag);

    if (alf_data->alf_luma_filter_signal_flag) {
      READ_BOOL_OR_RETURN(&alf_data->alf_luma_clip_flag);
      READ_UE_OR_RETURN(&alf_data->alf_luma_num_filters_signalled_minus1);
      IN_RANGE_OR_RETURN(alf_data->alf_luma_num_filters_signalled_minus1, 0,
                         kNumAlfFilters - 1);

      if (alf_data->alf_luma_num_filters_signalled_minus1 > 0) {
        int signaled_filters_len = base::bits::Log2Ceiling(
            alf_data->alf_luma_num_filters_signalled_minus1 + 1);
        for (int filter_idx = 0; filter_idx < kNumAlfFilters; filter_idx++) {
          READ_BITS_OR_RETURN(signaled_filters_len,
                              &std::get<H266AlfData>(aps->data)
                                   .alf_luma_coeff_delta_idx[filter_idx]);
          IN_RANGE_OR_RETURN(alf_data->alf_luma_coeff_delta_idx[filter_idx], 0,
                             alf_data->alf_luma_num_filters_signalled_minus1);
        }
      }

      for (int sf_idx = 0;
           sf_idx <= alf_data->alf_luma_num_filters_signalled_minus1;
           sf_idx++) {
        for (int j = 0; j < 12; j++) {
          READ_UE_OR_RETURN(
              &std::get<H266AlfData>(aps->data).alf_luma_coeff_abs[sf_idx][j]);
          IN_RANGE_OR_RETURN(alf_data->alf_luma_coeff_abs[sf_idx][j], 0, 128);
          if (alf_data->alf_luma_coeff_abs[sf_idx][j]) {
            // alf_luma_coeff_sign[sf_idx][j] equals to 0 indicates a positive
            // value and otherwise a negative value.
            READ_BOOL_OR_RETURN(&std::get<H266AlfData>(aps->data)
                                     .alf_luma_coeff_sign[sf_idx][j]);
          } else {
            alf_data->alf_luma_coeff_sign[sf_idx][j] = 0;
          }
        }
      }

      if (alf_data->alf_luma_clip_flag) {
        for (int sf_idx = 0;
             sf_idx <= alf_data->alf_luma_num_filters_signalled_minus1;
             sf_idx++) {
          for (int j = 0; j < 12; j++) {
            READ_BITS_OR_RETURN(
                2,
                &std::get<H266AlfData>(aps->data).alf_luma_clip_idx[sf_idx][j]);
          }
        }
      }
    }

    if (alf_data->alf_chroma_filter_signal_flag) {
      READ_BOOL_OR_RETURN(
          &std::get<H266AlfData>(aps->data).alf_chroma_clip_flag);
      READ_UE_OR_RETURN(
          &std::get<H266AlfData>(aps->data).alf_chroma_num_alt_filters_minus1);
      IN_RANGE_OR_RETURN(alf_data->alf_chroma_num_alt_filters_minus1, 0, 7);

      for (int alt_idx = 0;
           alt_idx <= alf_data->alf_chroma_num_alt_filters_minus1; alt_idx++) {
        for (int j = 0; j < 6; j++) {
          READ_UE_OR_RETURN(&std::get<H266AlfData>(aps->data)
                                 .alf_chroma_coeff_abs[alt_idx][j]);
          IN_RANGE_OR_RETURN(alf_data->alf_chroma_coeff_abs[alt_idx][j], 0,
                             128);
          if (alf_data->alf_chroma_coeff_abs[alt_idx][j] > 0) {
            READ_BOOL_OR_RETURN(&std::get<H266AlfData>(aps->data)
                                     .alf_chroma_coeff_sign[alt_idx][j]);
          } else {
            alf_data->alf_chroma_coeff_sign[alt_idx][j] = 0;
          }
        }

        if (alf_data->alf_chroma_clip_flag) {
          for (int j = 0; j < 6; j++) {
            READ_BITS_OR_RETURN(2, &alf_data->alf_chroma_clip_idx[alt_idx][j]);
          }
        }
      }
    } else {
      alf_data->alf_chroma_clip_flag = 0;
    }

    if (alf_data->alf_cc_cb_filter_signal_flag) {
      READ_UE_OR_RETURN(&alf_data->alf_cc_cb_filters_signalled_minus1);
      IN_RANGE_OR_RETURN(alf_data->alf_cc_cb_filters_signalled_minus1, 0, 3);

      for (int k = 0; k < alf_data->alf_cc_cb_filters_signalled_minus1 + 1;
           k++) {
        for (int j = 0; j < 7; j++) {
          READ_BITS_OR_RETURN(3, &alf_data->alf_cc_cb_mapped_coeff_abs[k][j]);
          if (alf_data->alf_cc_cb_mapped_coeff_abs[k][j]) {
            READ_BOOL_OR_RETURN(&alf_data->alf_cc_cb_coeff_sign[k][j]);
          } else {
            alf_data->alf_cc_cb_coeff_sign[k][j] = 0;
          }
        }
      }
    }

    if (alf_data->alf_cc_cr_filter_signal_flag) {
      READ_UE_OR_RETURN(&alf_data->alf_cc_cr_filters_signalled_minus1);
      IN_RANGE_OR_RETURN(alf_data->alf_cc_cr_filters_signalled_minus1, 0, 3);

      for (int k = 0; k < alf_data->alf_cc_cr_filters_signalled_minus1 + 1;
           k++) {
        for (int j = 0; j < 7; j++) {
          READ_BITS_OR_RETURN(3, &alf_data->alf_cc_cr_mapped_coeff_abs[k][j]);
          if (alf_data->alf_cc_cr_mapped_coeff_abs[k][j]) {
            READ_BOOL_OR_RETURN(&alf_data->alf_cc_cr_coeff_sign[k][j]);
          } else {
            alf_data->alf_cc_cr_coeff_sign[k][j] = 0;
          }
        }
      }
    }
  } else if (aps->aps_params_type == H266APS::kLmcs) {
    H266LmcsData* lmcs_data = &std::get<H266LmcsData>(aps->data);
    READ_UE_OR_RETURN(&lmcs_data->lmcs_min_bin_idx);
    IN_RANGE_OR_RETURN(lmcs_data->lmcs_min_bin_idx, 0, 15);
    READ_UE_OR_RETURN(&lmcs_data->lmcs_delta_max_bin_idx);
    IN_RANGE_OR_RETURN(lmcs_data->lmcs_delta_max_bin_idx, 0, 15);
    READ_UE_OR_RETURN(&lmcs_data->lmcs_delta_cw_prec_minus1);
    IN_RANGE_OR_RETURN(lmcs_data->lmcs_delta_cw_prec_minus1, 0, 14);

    int lmcs_max_bin_idx = 15 - lmcs_data->lmcs_delta_max_bin_idx;
    TRUE_OR_RETURN(lmcs_max_bin_idx >= lmcs_data->lmcs_min_bin_idx);

    for (int i = lmcs_data->lmcs_min_bin_idx; i <= lmcs_max_bin_idx; i++) {
      READ_BITS_OR_RETURN(lmcs_data->lmcs_delta_cw_prec_minus1 + 1,
                          &lmcs_data->lmcs_delta_abs_cw[i]);
      if (lmcs_data->lmcs_delta_abs_cw[i] > 0) {
        READ_BOOL_OR_RETURN(&lmcs_data->lmcs_delta_sign_cw_flag[i]);
      } else {
        lmcs_data->lmcs_delta_sign_cw_flag[i] = 0;
      }
    }
    if (aps->aps_chroma_present_flag) {
      READ_BITS_OR_RETURN(3, &lmcs_data->lmcs_delta_abs_crs);
      if (lmcs_data->lmcs_delta_abs_crs > 0) {
        READ_BOOL_OR_RETURN(&lmcs_data->lmcs_delta_sign_crs_flag);
      } else {
        lmcs_data->lmcs_delta_sign_crs_flag = 0;
      }
    } else {
      lmcs_data->lmcs_delta_abs_crs = 0;
      lmcs_data->lmcs_delta_sign_crs_flag = 0;
    }
  } else if (aps->aps_params_type == H266APS::kScalingList) {
    // VVC defines default quantization matrices(QM) for INTER_2x2,
    // INTER_4x4, INTRA_8x8 & INTER_8x8 with flat value of 16 in them.
    // Other sizes, including 16x16, 32x32, 64x64 are upsampled from the 8x8
    // quantization matrix.
    // If explicit scaling list is signaled in SPS/PH/SH/APS, VVC allows encoder
    // customize up to 28 quantization matrices. For QM of 16x16, 32x32 and
    // 64x64, the DC values are coded explicitly, while only 64(8x8) AC values
    // for every such matrix may be coded explicitly, with the entire matrix
    // upsampled to desired size.

    H266ScalingListData* scaling_list_data =
        &(std::get<H266ScalingListData>(aps->data));

    int max_id_delta = 0, matrix_size = 0, ref_id = 0;
    int scaling_matrix_pred2x2[2][2][2], scaling_matrix_pred4x4[6][4][4],
        scaling_matrix_pred8x8[20][8][8];
    int scaling_matrix_dc_pred[28];

    // id: [0, 1]:   2x2,   INTRA2x2 & INTER2x2
    //     [2, 7]:   4x4,   INTRA4x4_Y|U|V & INTER4x4_Y|U|V
    //     [8, 13]:  8x8,   INTRA8x8_Y|U|V & INTER8x8_Y|U|V
    //     [14, 19]: 16x16, INTRA16x16_Y|U|V & INTER16x16_Y|U|V
    //     [20, 25]: 32x32, INTRA32x32_Y|U|V & INTER32x32_Y|U|V
    //     [26, 27]: 64x64, INTRA64x64_Y & INTER64x64_Y

    for (int id = 0; id < 28; id++) {
      // Equation 101
      max_id_delta = (id < 2) ? id : ((id < 8) ? (id - 2) : (id - 8));
      // Equation 103
      matrix_size = (id < 2) ? 2 : ((id < 8) ? 4 : 8);

      scaling_list_data->scaling_list_copy_mode_flag[id] = 1;
      if (aps->aps_chroma_present_flag || id % 3 == 2 || id == 27) {
        READ_BOOL_OR_RETURN(
            &scaling_list_data->scaling_list_copy_mode_flag[id]);
        if (!scaling_list_data->scaling_list_copy_mode_flag[id]) {
          READ_BOOL_OR_RETURN(
              &scaling_list_data->scaling_list_pred_mode_flag[id]);
        }

        // id 0/2/8 are for 2x2/4x4/8x8 initial lists so they don't have
        // the scaling_list_pred_id_delta syntax signaled.
        if ((scaling_list_data->scaling_list_copy_mode_flag[id] ||
             scaling_list_data->scaling_list_pred_mode_flag[id]) &&
            id != 0 && id != 2 && id != 8) {
          READ_UE_OR_RETURN(&scaling_list_data->scaling_list_pred_id_delta[id]);
          IN_RANGE_OR_RETURN(scaling_list_data->scaling_list_pred_id_delta[id],
                             0, max_id_delta);
        }

        if (!scaling_list_data->scaling_list_copy_mode_flag[id]) {
          int next_coef = 0;
          if (id > 13) {
            READ_SE_OR_RETURN(
                &scaling_list_data->scaling_list_dc_coef[id - 14]);
            IN_RANGE_OR_RETURN(scaling_list_data->scaling_list_dc_coef[id - 14],
                               -128, 127);

            next_coef += scaling_list_data->scaling_matrix_dc_rec[id - 14];
          }

          for (int i = 0; i < matrix_size * matrix_size; i++) {
            int x = kDiagScanOrder8x8[i][0], y = kDiagScanOrder8x8[i][1];
            if (!(id > 25 && x >= 4 && y >= 4)) {
              READ_SE_OR_RETURN(
                  &scaling_list_data->scaling_list_delta_coef[id][i]);
              IN_RANGE_OR_RETURN(
                  scaling_list_data->scaling_list_delta_coef[id][i], -128, 127);

              next_coef += scaling_list_data->scaling_list_delta_coef[id][i];
            }
            if (id < 2) {
              scaling_list_data->scaling_list_2x2[id][i] = next_coef;
            } else if (id < 8) {
              scaling_list_data->scaling_list_4x4[id - 2][i] = next_coef;
            } else {
              scaling_list_data->scaling_list_8x8[id - 8][i] = next_coef;
            }
          }
        }

        // Equation 102
        ref_id = id - scaling_list_data->scaling_list_pred_id_delta[id];

        if (!scaling_list_data->scaling_list_copy_mode_flag[id] &&
            !scaling_list_data->scaling_list_pred_mode_flag[id]) {
          scaling_matrix_dc_pred[id] = 8;
          if (id < 2) {
            std::fill_n(scaling_matrix_pred2x2[id][0],
                        matrix_size * matrix_size, 8);
          } else if (id < 8) {
            std::fill_n(scaling_matrix_pred4x4[id - 2][0],
                        matrix_size * matrix_size, 8);
          } else {
            std::fill_n(scaling_matrix_pred8x8[id - 8][0],
                        matrix_size * matrix_size, 8);
          }
        } else if (scaling_list_data->scaling_list_pred_id_delta[id] == 0) {
          scaling_matrix_dc_pred[id] = 16;
          if (id < 2) {
            std::fill_n(scaling_matrix_pred2x2[id][0],
                        matrix_size * matrix_size, 16);
          } else if (id < 8) {
            std::fill_n(scaling_matrix_pred4x4[id - 2][0],
                        matrix_size * matrix_size, 16);
          } else {
            std::fill_n(scaling_matrix_pred8x8[id - 8][0],
                        matrix_size * matrix_size, 16);
          }
        } else {
          if (id < 2 && id > 0 & ref_id >= 0) {
            memcpy(&scaling_matrix_pred2x2[id][0],
                   &scaling_list_data->scaling_matrix_rec_2x2[ref_id][0][0],
                   4 * sizeof(int));
          } else if (id < 8 && id > 2 && ref_id >= 2) {
            memcpy(&scaling_matrix_pred4x4[id - 2][0][0],
                   &scaling_list_data->scaling_matrix_rec_4x4[ref_id - 2][0][0],
                   16 * sizeof(int));
          } else if (ref_id >= 8) {
            memcpy(&scaling_matrix_pred8x8[id - 8][0][0],
                   &scaling_list_data->scaling_matrix_rec_8x8[ref_id - 8][0][0],
                   64 * sizeof(int));
          }
          if (ref_id > 13) {
            scaling_matrix_dc_pred[id] =
                scaling_list_data->scaling_matrix_dc_rec[ref_id - 14];
          } else {
            if (id < 2) {
              scaling_matrix_dc_pred[id] = scaling_matrix_pred2x2[id][0][0];
            } else if (id < 8) {
              scaling_matrix_dc_pred[id] = scaling_matrix_pred4x4[id - 2][0][0];
            } else {
              scaling_matrix_dc_pred[id] = scaling_matrix_pred8x8[id - 8][0][0];
            }
          }
        }

        // Equation 104
        if (id > 13) {
          scaling_list_data->scaling_matrix_dc_rec[id - 14] =
              (scaling_matrix_dc_pred[id] +
               scaling_list_data->scaling_list_dc_coef[id - 14]) &
              255;
        }
      }

      // Equation 105
      int rec_x = 0, rec_y = 0, k = 0;
      if (id < 2) {
        for (k = 0; k <= 3; k++) {
          rec_x = kDiagScanOrder2x2[k][0];
          rec_y = kDiagScanOrder2x2[k][1];
          scaling_list_data->scaling_matrix_rec_2x2[id][rec_x][rec_y] =
              (scaling_matrix_pred2x2[id][rec_x][rec_y] +
               scaling_list_data->scaling_list_2x2[id][k]) &
              255;
        }
      } else if (id < 8) {
        for (k = 0; k <= 15; k++) {
          rec_x = kDiagScanOrder4x4[k][0];
          rec_y = kDiagScanOrder4x4[k][1];
          scaling_list_data->scaling_matrix_rec_4x4[id - 2][rec_x][rec_y] =
              (scaling_matrix_pred4x4[id - 2][rec_x][rec_y] +
               scaling_list_data->scaling_list_4x4[id - 2][k]) &
              255;
        }
      } else {
        for (k = 0; k <= 63; k++) {
          rec_x = kDiagScanOrder8x8[k][0];
          rec_y = kDiagScanOrder8x8[k][1];
          scaling_list_data->scaling_matrix_rec_8x8[id - 8][rec_x][rec_y] =
              (scaling_matrix_pred8x8[id - 8][rec_x][rec_y] +
               scaling_list_data->scaling_list_8x8[id - 8][k]) &
              255;
        }
      }
    }
  }

  // If an APS with the same id already exists, replace it.
  *aps_id = aps->aps_adaptation_parameter_set_id;
  switch (aps->aps_params_type) {
    case 0:
      *type = H266APS::ParamType::kAlf;
      active_alf_aps_[*aps_id] = std::move(aps);
      break;
    case 1:
      *type = H266APS::ParamType::kLmcs;
      active_lmcs_aps_[*aps_id] = std::move(aps);
      break;
    case 2:
      *type = H266APS::ParamType::kScalingList;
      active_scaling_list_aps_[*aps_id] = std::move(aps);
      break;
  }

  return kOk;
}

// 7.3.9 & 7.4.10
H266Parser::Result H266Parser::ParseRefPicLists(
    const H266SPS& sps,
    const H266PPS& pps,
    H266RefPicLists* ref_pic_lists) {
  DCHECK(ref_pic_lists);

  for (int i = 0; i < 2; i++) {
    if (sps.sps_num_ref_pic_lists[i] > 0 &&
        (i == 0 || (i == 1 && pps.pps_rpl1_idx_present_flag))) {
      READ_BOOL_OR_RETURN(&ref_pic_lists->rpl_sps_flag[i]);
    } else {
      if (sps.sps_num_ref_pic_lists[i] == 0) {
        ref_pic_lists->rpl_sps_flag[i] = 0;
      } else if (sps.sps_num_ref_pic_lists[i] > 0) {
        if (pps.pps_rpl1_idx_present_flag == 0 && i == 1) {
          ref_pic_lists->rpl_sps_flag[i] = ref_pic_lists->rpl_sps_flag[0];
        }
      }
    }

    if (ref_pic_lists->rpl_sps_flag[i]) {
      if (sps.sps_num_ref_pic_lists[i] > 1 &&
          (i == 0 || (i == 1 && pps.pps_rpl1_idx_present_flag))) {
        READ_BITS_OR_RETURN(base::bits::Log2Ceiling(static_cast<uint32_t>(
                                sps.sps_num_ref_pic_lists[i])),
                            &ref_pic_lists->rpl_idx[i]);
        IN_RANGE_OR_RETURN(ref_pic_lists->rpl_idx[i], 0,
                           sps.sps_num_ref_pic_lists[i] - 1);
      } else {
        if (sps.sps_num_ref_pic_lists[i] == 1) {
          ref_pic_lists->rpl_idx[i] = 0;
        }
        if (i == 1 && pps.pps_rpl1_idx_present_flag == 0 &&
            sps.sps_num_ref_pic_lists[1] > 1) {
          ref_pic_lists->rpl_idx[i] = ref_pic_lists->rpl_idx[0];
        }
      }
    } else {
      ParseRefPicListStruct(i, sps.sps_num_ref_pic_lists[i], sps,
                            &ref_pic_lists->rpl_ref_lists[i]);
    }

    int num_ltrp_entries =
        !ref_pic_lists->rpl_sps_flag[i]
            ? ref_pic_lists->rpl_ref_lists[i].num_ltrp_entries
            : sps.ref_pic_list_struct[i][ref_pic_lists->rpl_idx[i]]
                  .num_ltrp_entries;
    bool ltrp_in_header =
        !ref_pic_lists->rpl_sps_flag[i]
            ? ref_pic_lists->rpl_ref_lists[i].ltrp_in_header_flag
            : sps.ref_pic_list_struct[i][ref_pic_lists->rpl_idx[i]]
                  .ltrp_in_header_flag;
    for (int j = 0; j < num_ltrp_entries; j++) {
      if (ltrp_in_header) {
        READ_BITS_OR_RETURN(sps.sps_log2_max_pic_order_cnt_lsb_minus4 + 4,
                            &ref_pic_lists->poc_lsb_lt[i][j]);
      }
      READ_BOOL_OR_RETURN(
          &ref_pic_lists->delta_poc_msb_cycle_present_flag[i][j]);
      if (ref_pic_lists->delta_poc_msb_cycle_present_flag[i][j]) {
        READ_UE_OR_RETURN(&ref_pic_lists->delta_poc_msb_cycle_lt[i][j]);
        IN_RANGE_OR_RETURN(
            ref_pic_lists->delta_poc_msb_cycle_lt[i][j], 0,
            std::pow(2, 32 - sps.sps_log2_max_pic_order_cnt_lsb_minus4 - 4));
      } else {
        ref_pic_lists->delta_poc_msb_cycle_lt[i][j] = 0;
      }

      // Equation 148
      if (j == 0) {
        ref_pic_lists->unpacked_delta_poc_msb_cycle_lt[i][j] =
            ref_pic_lists->delta_poc_msb_cycle_lt[i][j];
      } else {
        ref_pic_lists->unpacked_delta_poc_msb_cycle_lt[i][j] =
            ref_pic_lists->delta_poc_msb_cycle_lt[i][j] +
            ref_pic_lists->unpacked_delta_poc_msb_cycle_lt[i][j - 1];
      }
    }

    // Equation 146
    ref_pic_lists->rpls_idx[i] = ref_pic_lists->rpl_sps_flag[i]
                                     ? ref_pic_lists->rpl_idx[i]
                                     : sps.sps_num_ref_pic_lists[i];
  }

  return kOk;
}

H266Parser::Result H266Parser::ParsePredWeightTable(
    const H266SPS& sps,
    const H266PPS& pps,
    const H266RefPicLists& ref_pic_lists,
    int num_ref_idx_active[2],
    H266PredWeightTable* pred_weight_table) {
  DCHECK(pred_weight_table);

  // 7.3.8
  READ_UE_OR_RETURN(&pred_weight_table->luma_log2_weight_denom);
  IN_RANGE_OR_RETURN(pred_weight_table->luma_log2_weight_denom, 0, 7);
  if (sps.sps_chroma_format_idc != 0) {
    READ_SE_OR_RETURN(&pred_weight_table->delta_chroma_log2_weight_denom);
  } else {
    pred_weight_table->delta_chroma_log2_weight_denom = 0;
  }
  pred_weight_table->chroma_log2_weight_denom =
      pred_weight_table->luma_log2_weight_denom +
      pred_weight_table->delta_chroma_log2_weight_denom;
  IN_RANGE_OR_RETURN(pred_weight_table->chroma_log2_weight_denom, 0, 7);

  if (pps.pps_wp_info_in_ph_flag) {
    READ_UE_OR_RETURN(&pred_weight_table->num_l0_weights);
    IN_RANGE_OR_RETURN(
        pred_weight_table->num_l0_weights, 0,
        std::min(15, ref_pic_lists.rpl_ref_lists[0].num_ref_entries));
    pred_weight_table->num_weights_l0 = pred_weight_table->num_l0_weights;
  } else {
    pred_weight_table->num_weights_l0 = num_ref_idx_active[0];
  }
  for (int i = 0; i < pred_weight_table->num_weights_l0; i++) {
    READ_BOOL_OR_RETURN(&pred_weight_table->luma_weight_l0_flag[i]);
  }
  if (sps.sps_chroma_format_idc != 0) {
    for (int i = 0; i < pred_weight_table->num_weights_l0; i++) {
      READ_BOOL_OR_RETURN(&pred_weight_table->chroma_weight_l0_flag[i]);
    }
  }
  for (int i = 0; i < pred_weight_table->num_weights_l0; i++) {
    if (pred_weight_table->luma_weight_l0_flag[i]) {
      READ_SE_OR_RETURN(&pred_weight_table->delta_luma_weight_l0[i]);
      IN_RANGE_OR_RETURN(pred_weight_table->delta_luma_weight_l0[i], -128, 127);
      READ_SE_OR_RETURN(&pred_weight_table->luma_offset_l0[i]);
      IN_RANGE_OR_RETURN(pred_weight_table->luma_offset_l0[i], -128, 127);
    } else {
      pred_weight_table->delta_luma_weight_l0[i] = 0;
      pred_weight_table->luma_offset_l0[i] = 0;
    }
    if (pred_weight_table->chroma_weight_l0_flag[i]) {
      for (int j = 0; j < 2; j++) {
        READ_SE_OR_RETURN(&pred_weight_table->delta_chroma_weight_l0[i][j]);
        IN_RANGE_OR_RETURN(pred_weight_table->delta_chroma_weight_l0[i][j],
                           -128, 127);
        READ_SE_OR_RETURN(&pred_weight_table->delta_chroma_offset_l0[i][j]);
        IN_RANGE_OR_RETURN(pred_weight_table->delta_chroma_offset_l0[i][j],
                           -4 * 128, 4 * 127);
      }
    }
  }

  if (pps.pps_weighted_bipred_flag && pps.pps_wp_info_in_ph_flag &&
      ref_pic_lists.rpl_ref_lists[1].num_ref_entries > 0) {
    READ_UE_OR_RETURN(&pred_weight_table->num_l1_weights);
    IN_RANGE_OR_RETURN(
        pred_weight_table->num_l1_weights, 0,
        std::min(15, ref_pic_lists.rpl_ref_lists[1].num_ref_entries));
  }
  // Equation 145
  if (!pps.pps_weighted_bipred_flag ||
      (pps.pps_wp_info_in_ph_flag &&
       ref_pic_lists.rpl_ref_lists[1].num_ref_entries == 0)) {
    pred_weight_table->num_weights_l1 = 0;
  } else if (pps.pps_wp_info_in_ph_flag) {
    pred_weight_table->num_weights_l1 = pred_weight_table->num_l1_weights;
  } else {
    pred_weight_table->num_weights_l1 = num_ref_idx_active[1];
  }

  for (int i = 0; i < pred_weight_table->num_weights_l1; i++) {
    READ_BOOL_OR_RETURN(&pred_weight_table->luma_weight_l1_flag[i]);
  }
  if (sps.sps_chroma_format_idc != 0) {
    for (int i = 0; i < pred_weight_table->num_weights_l1; i++) {
      READ_BOOL_OR_RETURN(&pred_weight_table->chroma_weight_l1_flag[i]);
    }
  }
  for (int i = 0; i < pred_weight_table->num_weights_l1; i++) {
    if (pred_weight_table->luma_weight_l1_flag[i]) {
      READ_SE_OR_RETURN(&pred_weight_table->delta_luma_weight_l1[i]);
      IN_RANGE_OR_RETURN(pred_weight_table->delta_luma_weight_l1[i], -128, 127);
      READ_SE_OR_RETURN(&pred_weight_table->luma_offset_l1[i]);
      IN_RANGE_OR_RETURN(pred_weight_table->luma_offset_l1[i], -128, 127);
    } else {
      pred_weight_table->delta_luma_weight_l1[i] = 0;
      pred_weight_table->luma_offset_l1[i] = 0;
    }
    if (pred_weight_table->chroma_weight_l1_flag[i]) {
      for (int j = 0; j < 2; j++) {
        READ_SE_OR_RETURN(&pred_weight_table->delta_chroma_weight_l1[i][j]);
        IN_RANGE_OR_RETURN(pred_weight_table->delta_chroma_weight_l1[i][j],
                           -128, 127);
        READ_SE_OR_RETURN(&pred_weight_table->delta_chroma_offset_l1[i][j]);
        IN_RANGE_OR_RETURN(pred_weight_table->delta_chroma_offset_l1[i][j],
                           -4 * 128, 4 * 127);
      }
    }
  }

  return kOk;
}

H266Parser::Result H266Parser::ParsePHNut(const H266NALU& nalu,
                                          H266PictureHeader* ph) {
  DCHECK(ph);
  memset(reinterpret_cast<void*>(ph), 0, sizeof(H266PictureHeader));

  if (nalu.nal_unit_type != H266NALU::kPH) {
    DVLOG(1) << "Not a picture header NALU.";
    return kIgnored;
  }

  ph->nal_unit_type = nalu.nal_unit_type;
  return ParsePictureHeaderStructure(nalu, ph);
}

H266Parser::Result H266Parser::ParsePHInSlice(const H266NALU& nalu,
                                              H266PictureHeader* ph) {
  DCHECK(ph);
  memset(reinterpret_cast<void*>(ph), 0, sizeof(H266PictureHeader));

  if (!(nalu.nal_unit_type >= H266NALU::kTrail &&
        nalu.nal_unit_type <= H266NALU::kReservedIRAP11)) {
    DVLOG(1) << "Embedded picture header structure must be in slice.";
    return kInvalidStream;
  }

  // The nalu type of slice that current picture header is embedded into.
  ph->nal_unit_type = nalu.nal_unit_type;
  return ParsePictureHeaderStructure(nalu, ph);
}

// 7.3.2.8 Picture header structure
// May be in separate PH_NUT or included in slice header. They convey
// information for a particular picture including but not limited to:
// 1. Indication of IRAP/GDR, and if inter-intra slices are allowed.
// 2. LSB and MSB of POC, coding partitioning.
// 3. Picture level collocated info and tool switches.
// 4. RPLs, deblocking/QP info, etc.
// Be noted for each picture there needs to be exactly one PH associated
// with it.
H266Parser::Result H266Parser::ParsePictureHeaderStructure(
    const H266NALU& nalu,
    H266PictureHeader* ph) {
  READ_BOOL_OR_RETURN(&ph->ph_gdr_or_irap_pic_flag);
  READ_BOOL_OR_RETURN(&ph->ph_non_ref_pic_flag);
  if (ph->ph_gdr_or_irap_pic_flag) {
    READ_BOOL_OR_RETURN(&ph->ph_gdr_pic_flag);
  } else {
    ph->ph_gdr_pic_flag = 0;
  }
  READ_BOOL_OR_RETURN(&ph->ph_inter_slice_allowed_flag);

  if (ph->ph_inter_slice_allowed_flag) {
    READ_BOOL_OR_RETURN(&ph->ph_intra_slice_allowed_flag);
  } else {
    ph->ph_intra_slice_allowed_flag = 1;
  }

  READ_UE_OR_RETURN(&ph->ph_pic_parameter_set_id);
  IN_RANGE_OR_RETURN(ph->ph_pic_parameter_set_id, 0, 63);

  const H266PPS* pps = GetPPS(ph->ph_pic_parameter_set_id);
  if (!pps) {
    DVLOG(1) << "Invalid PPS in picture header.";
    return kInvalidStream;
  }

  const H266SPS* sps = GetSPS(pps->pps_seq_parameter_set_id);
  if (!sps) {
    DVLOG(1) << "Failed to find SPS for current PPS.";
    return kInvalidStream;
  }
  const H266VPS* vps = GetVPS(sps->sps_video_parameter_set_id);
  if (!vps) {
    DVLOG(1) << "VPS for current SPS is not found.";
    return kInvalidStream;
  }

  if (ph->ph_gdr_or_irap_pic_flag && !ph->ph_gdr_pic_flag) {
    int general_layer_idx = vps->GetGeneralLayerIdx(nalu.nuh_layer_id);
    if (general_layer_idx > 0 & general_layer_idx < kMaxLayers &&
        vps->vps_independent_layer_flag[general_layer_idx]) {
      TRUE_OR_RETURN(!ph->ph_inter_slice_allowed_flag);
    }
  }

  // Late validation of ph_gdr_pic_flag as pps id is fetched after
  // ph_gdr_pic_flag.
  if (sps->sps_gdr_enabled_flag == 0) {
    TRUE_OR_RETURN(ph->ph_gdr_pic_flag == 0);
  }

  READ_BITS_OR_RETURN(sps->sps_log2_max_pic_order_cnt_lsb_minus4 + 4,
                      &ph->ph_pic_order_cnt_lsb);
  IN_RANGE_OR_RETURN(ph->ph_pic_order_cnt_lsb, 0,
                     sps->max_pic_order_cnt_lsb - 1);
  if (ph->ph_gdr_pic_flag) {
    READ_UE_OR_RETURN(&ph->ph_recovery_poc_cnt);
    IN_RANGE_OR_RETURN(ph->ph_recovery_poc_cnt, 0,
                       sps->max_pic_order_cnt_lsb - 1);
  }

  if (sps->num_extra_ph_bits > 0) {
    SKIP_BITS_OR_RETURN(sps->num_extra_ph_bits);
  }

  if (sps->sps_poc_msb_cycle_flag) {
    READ_BOOL_OR_RETURN(&ph->ph_poc_msb_cycle_present_flag);
    if (ph->ph_poc_msb_cycle_present_flag) {
      READ_BITS_OR_RETURN(sps->sps_poc_msb_cycle_len_minus1 + 1,
                          &ph->ph_poc_msb_cycle_val);
    }
  }

  // PH alf info.
  if (sps->sps_alf_enabled_flag && pps->pps_alf_info_in_ph_flag) {
    READ_BOOL_OR_RETURN(&ph->ph_alf_enabled_flag);
    if (ph->ph_alf_enabled_flag) {
      READ_BITS_OR_RETURN(3, &ph->ph_num_alf_aps_ids_luma);
      for (int i = 0; i < ph->ph_num_alf_aps_ids_luma; i++) {
        READ_BITS_OR_RETURN(3, &ph->ph_alf_aps_id_luma[i]);
      }
      if (sps->sps_chroma_format_idc != 0) {
        READ_BOOL_OR_RETURN(&ph->ph_alf_cb_enabled_flag);
        READ_BOOL_OR_RETURN(&ph->ph_alf_cr_enabled_flag);
      } else {
        ph->ph_alf_cb_enabled_flag = ph->ph_alf_cr_enabled_flag = 0;
      }
      if (ph->ph_alf_cb_enabled_flag || ph->ph_alf_cr_enabled_flag) {
        READ_BITS_OR_RETURN(3, &ph->ph_alf_aps_id_chroma);
      }
      if (sps->sps_ccalf_enabled_flag) {
        READ_BOOL_OR_RETURN(&ph->ph_alf_cc_cb_enabled_flag);
        if (ph->ph_alf_cc_cb_enabled_flag) {
          READ_BITS_OR_RETURN(3, &ph->ph_alf_cc_cb_aps_id);
        }
        READ_BOOL_OR_RETURN(&ph->ph_alf_cc_cr_enabled_flag);
        if (ph->ph_alf_cc_cr_enabled_flag) {
          READ_BITS_OR_RETURN(3, &ph->ph_alf_cc_cr_aps_id);
        }
      } else {
        ph->ph_alf_cc_cb_enabled_flag = 0;
        ph->ph_alf_cc_cr_enabled_flag = 0;
      }
    }
  } else {
    ph->ph_alf_enabled_flag = 0;
    ph->ph_alf_cb_enabled_flag = ph->ph_alf_cr_enabled_flag = 0;
    ph->ph_alf_cc_cb_enabled_flag = 0;
    ph->ph_alf_cc_cr_enabled_flag = 0;
  }

  if (sps->sps_lmcs_enabled_flag) {
    READ_BOOL_OR_RETURN(&ph->ph_lmcs_enabled_flag);
    if (ph->ph_lmcs_enabled_flag) {
      READ_BITS_OR_RETURN(2, &ph->ph_lmcs_aps_id);
      if (sps->sps_chroma_format_idc != 0) {
        READ_BOOL_OR_RETURN(&ph->ph_chroma_residual_scale_flag);
      } else {
        ph->ph_chroma_residual_scale_flag = 0;
      }
    }
  } else {
    ph->ph_lmcs_enabled_flag = 0;
    ph->ph_chroma_residual_scale_flag = 0;
  }

  if (sps->sps_explicit_scaling_list_enabled_flag) {
    READ_BOOL_OR_RETURN(&ph->ph_explicit_scaling_list_enabled_flag);
    if (ph->ph_explicit_scaling_list_enabled_flag) {
      READ_BITS_OR_RETURN(3, &ph->ph_scaling_list_aps_id);
    }
  } else {
    ph->ph_explicit_scaling_list_enabled_flag = 0;
  }

  if (sps->sps_virtual_boundaries_enabled_flag &&
      !sps->sps_virtual_boundaries_present_flag) {
    READ_BOOL_OR_RETURN(&ph->ph_virtual_boundaries_present_flag);
    // Equation 77.
    ph->virtual_boundaries_present_flag = 0;
    if (sps->sps_virtual_boundaries_enabled_flag) {
      ph->virtual_boundaries_present_flag =
          sps->sps_virtual_boundaries_present_flag ||
          ph->ph_virtual_boundaries_present_flag;
    }
    if (ph->ph_virtual_boundaries_present_flag) {
      READ_UE_OR_RETURN(&ph->ph_num_ver_virtual_boundaries);
      IN_RANGE_OR_RETURN(ph->ph_num_ver_virtual_boundaries, 0,
                         (pps->pps_pic_width_in_luma_samples <= 8) ? 0 : 3);
      for (int i = 0; i < ph->ph_num_ver_virtual_boundaries; i++) {
        READ_UE_OR_RETURN(&ph->ph_virtual_boundary_pos_x_minus1[i]);
        IN_RANGE_OR_RETURN(ph->ph_virtual_boundary_pos_x_minus1[i], 0,
                           (pps->pps_pic_width_in_luma_samples + 7 / 8) - 2);
      }

      READ_UE_OR_RETURN(&ph->ph_num_hor_virtual_boundaries);
      IN_RANGE_OR_RETURN(ph->ph_num_hor_virtual_boundaries, 0,
                         (pps->pps_pic_height_in_luma_samples <= 8) ? 0 : 3);
      for (int i = 0; i < ph->ph_num_hor_virtual_boundaries; i++) {
        READ_UE_OR_RETURN(&ph->ph_virtual_boundary_pos_y_minus1[i]);
        IN_RANGE_OR_RETURN(ph->ph_virtual_boundary_pos_y_minus1[i], 0,
                           (pps->pps_pic_height_in_luma_samples + 7 / 8) - 2);
      }
    } else {
      ph->ph_num_ver_virtual_boundaries = 0;
    }
  } else {
    ph->ph_virtual_boundaries_present_flag = 0;
    ph->ph_num_ver_virtual_boundaries = 0;
  }

  if (pps->pps_output_flag_present_flag && !ph->ph_non_ref_pic_flag) {
    READ_BOOL_OR_RETURN(&ph->ph_pic_output_flag);
  } else {
    ph->ph_pic_output_flag = 1;
  }

  if (pps->pps_rpl_info_in_ph_flag) {
    ParseRefPicLists(*sps, *pps, &ph->ref_pic_lists);
  }

  if (sps->sps_partition_constraints_override_enabled_flag) {
    READ_BOOL_OR_RETURN(&ph->ph_partition_constraints_override_flag);
  } else {
    ph->ph_partition_constraints_override_flag = 0;
  }

  if (ph->ph_intra_slice_allowed_flag) {
    if (ph->ph_partition_constraints_override_flag) {
      READ_UE_OR_RETURN(&ph->ph_log2_diff_min_qt_min_cb_intra_slice_luma);
      IN_RANGE_OR_RETURN(
          ph->ph_log2_diff_min_qt_min_cb_intra_slice_luma, 0,
          std::min(6, sps->ctb_log2_size_y) - sps->min_cb_log2_size_y);
      // Equation 82
      ph->min_qt_log2_size_intra_y =
          ph->ph_log2_diff_min_qt_min_cb_intra_slice_luma +
          sps->min_cb_log2_size_y;

      READ_UE_OR_RETURN(&ph->ph_max_mtt_hierarchy_depth_intra_slice_luma);
      IN_RANGE_OR_RETURN(ph->ph_max_mtt_hierarchy_depth_intra_slice_luma, 0,
                         2 * (sps->ctb_log2_size_y - sps->min_cb_log2_size_y));

      if (ph->ph_max_mtt_hierarchy_depth_intra_slice_luma != 0) {
        READ_UE_OR_RETURN(&ph->ph_log2_diff_max_bt_min_qt_intra_slice_luma);
        IN_RANGE_OR_RETURN(ph->ph_log2_diff_max_bt_min_qt_intra_slice_luma, 0,
                           (sps->sps_qtbtt_dual_tree_intra_flag
                                ? std::min(6, sps->ctb_log2_size_y)
                                : sps->ctb_log2_size_y) -
                               ph->min_qt_log2_size_intra_y);
        READ_UE_OR_RETURN(&ph->ph_log2_diff_max_tt_min_qt_intra_slice_luma);
        IN_RANGE_OR_RETURN(
            ph->ph_log2_diff_max_tt_min_qt_intra_slice_luma, 0,
            std::min(6, sps->ctb_log2_size_y) - ph->min_qt_log2_size_intra_y);
      } else {
        ph->ph_log2_diff_max_bt_min_qt_intra_slice_luma =
            sps->sps_log2_diff_max_bt_min_qt_intra_slice_luma;
        ph->ph_log2_diff_max_tt_min_qt_intra_slice_luma =
            sps->sps_log2_diff_max_tt_min_qt_intra_slice_luma;
      }

      if (sps->sps_qtbtt_dual_tree_intra_flag) {
        READ_UE_OR_RETURN(&ph->ph_log2_diff_min_qt_min_cb_intra_slice_chroma);
        IN_RANGE_OR_RETURN(
            ph->ph_log2_diff_min_qt_min_cb_intra_slice_chroma, 0,
            std::min(6, sps->ctb_log2_size_y) - sps->min_cb_log2_size_y);
        READ_UE_OR_RETURN(&ph->ph_max_mtt_hierarchy_depth_intra_slice_chroma);
        IN_RANGE_OR_RETURN(
            ph->ph_max_mtt_hierarchy_depth_intra_slice_chroma, 0,
            2 * (sps->ctb_log2_size_y - sps->min_cb_log2_size_y));

        // Equation 83
        ph->min_qt_log2_size_intra_c =
            ph->ph_log2_diff_min_qt_min_cb_intra_slice_chroma +
            sps->min_cb_log2_size_y;

        if (ph->ph_max_mtt_hierarchy_depth_intra_slice_chroma) {
          READ_UE_OR_RETURN(&ph->ph_log2_diff_max_bt_min_qt_intra_slice_chroma);
          IN_RANGE_OR_RETURN(
              ph->ph_log2_diff_max_bt_min_qt_intra_slice_chroma, 0,
              std::min(6, sps->ctb_log2_size_y) - ph->min_qt_log2_size_intra_c);
          READ_UE_OR_RETURN(&ph->ph_log2_diff_max_tt_min_qt_intra_slice_chroma);
          IN_RANGE_OR_RETURN(
              ph->ph_log2_diff_max_tt_min_qt_intra_slice_chroma, 0,
              std::min(6, sps->ctb_log2_size_y) - ph->min_qt_log2_size_intra_c);
        } else {
          ph->ph_log2_diff_max_bt_min_qt_intra_slice_chroma =
              sps->sps_log2_diff_max_bt_min_qt_intra_slice_chroma;
          ph->ph_log2_diff_max_tt_min_qt_intra_slice_chroma =
              sps->sps_log2_diff_max_tt_min_qt_intra_slice_chroma;
        }
      } else {
        ph->ph_log2_diff_max_bt_min_qt_intra_slice_chroma =
            sps->sps_log2_diff_max_bt_min_qt_intra_slice_chroma;
        ph->ph_log2_diff_max_tt_min_qt_intra_slice_chroma =
            sps->sps_log2_diff_max_tt_min_qt_intra_slice_chroma;
        ph->ph_log2_diff_min_qt_min_cb_intra_slice_chroma =
            sps->sps_log2_diff_min_qt_min_cb_intra_slice_chroma;
        ph->ph_max_mtt_hierarchy_depth_intra_slice_chroma =
            sps->sps_max_mtt_hierarchy_depth_intra_slice_chroma;
      }
    } else {
      ph->ph_log2_diff_min_qt_min_cb_intra_slice_luma =
          sps->sps_log2_diff_min_qt_min_cb_intra_slice_luma;
      ph->ph_max_mtt_hierarchy_depth_intra_slice_luma =
          sps->sps_max_mtt_hierarchy_depth_intra_slice_luma;
      ph->ph_log2_diff_max_bt_min_qt_intra_slice_luma =
          sps->sps_log2_diff_max_bt_min_qt_intra_slice_luma;
      ph->ph_log2_diff_max_tt_min_qt_intra_slice_luma =
          sps->sps_log2_diff_max_tt_min_qt_intra_slice_luma;
      ph->ph_log2_diff_max_bt_min_qt_intra_slice_chroma =
          sps->sps_log2_diff_max_bt_min_qt_intra_slice_chroma;
      ph->ph_log2_diff_max_tt_min_qt_intra_slice_chroma =
          sps->sps_log2_diff_max_tt_min_qt_intra_slice_chroma;
      ph->ph_log2_diff_min_qt_min_cb_intra_slice_chroma =
          sps->sps_log2_diff_min_qt_min_cb_intra_slice_chroma;
      ph->ph_max_mtt_hierarchy_depth_intra_slice_chroma =
          sps->sps_max_mtt_hierarchy_depth_intra_slice_chroma;
    }
    if (pps->pps_cu_qp_delta_enabled_flag) {
      READ_UE_OR_RETURN(&ph->ph_cu_qp_delta_subdiv_intra_slice);
      IN_RANGE_OR_RETURN(
          ph->ph_cu_qp_delta_subdiv_intra_slice, 0,
          2 * (sps->ctb_log2_size_y - ph->min_qt_log2_size_intra_y +
               ph->ph_max_mtt_hierarchy_depth_intra_slice_luma));
    } else {
      ph->ph_cu_qp_delta_subdiv_intra_slice = 0;
    }
    if (pps->pps_cu_chroma_qp_offset_list_enabled_flag) {
      READ_UE_OR_RETURN(&ph->ph_cu_chroma_qp_offset_subdiv_intra_slice);
      IN_RANGE_OR_RETURN(
          ph->ph_cu_chroma_qp_offset_subdiv_intra_slice, 0,
          2 * (sps->ctb_log2_size_y - ph->min_qt_log2_size_intra_y +
               ph->ph_max_mtt_hierarchy_depth_intra_slice_luma));
    } else {
      ph->ph_cu_chroma_qp_offset_subdiv_intra_slice = 0;
    }
  }  // ph_intra_slice_allowed_flag

  if (ph->ph_inter_slice_allowed_flag) {  // Syntax parsing till before
                                          // ph_qp_delta
    if (ph->ph_partition_constraints_override_flag) {
      READ_UE_OR_RETURN(&ph->ph_log2_diff_min_qt_min_cb_inter_slice);
      IN_RANGE_OR_RETURN(
          ph->ph_log2_diff_min_qt_min_cb_inter_slice, 0,
          std::min(6, sps->ctb_log2_size_y) - sps->min_cb_log2_size_y);
      // Equation 84
      ph->min_qt_log2_size_inter_y =
          ph->ph_log2_diff_min_qt_min_cb_inter_slice + sps->min_cb_log2_size_y;

      READ_UE_OR_RETURN(&ph->ph_max_mtt_hierarchy_depth_inter_slice);
      IN_RANGE_OR_RETURN(ph->ph_max_mtt_hierarchy_depth_inter_slice, 0,
                         2 * (sps->ctb_log2_size_y - sps->min_cb_log2_size_y));

      if (ph->ph_max_mtt_hierarchy_depth_inter_slice != 0) {
        READ_UE_OR_RETURN(&ph->ph_log2_diff_max_bt_min_qt_inter_slice);
        IN_RANGE_OR_RETURN(ph->ph_log2_diff_max_bt_min_qt_inter_slice, 0,
                           sps->ctb_log2_size_y - ph->min_qt_log2_size_inter_y);
        READ_UE_OR_RETURN(&ph->ph_log2_diff_max_tt_min_qt_inter_slice);
        IN_RANGE_OR_RETURN(
            ph->ph_log2_diff_max_tt_min_qt_inter_slice, 0,
            std::min(6, sps->ctb_log2_size_y) - ph->min_qt_log2_size_inter_y);
      } else {
        ph->ph_log2_diff_max_bt_min_qt_inter_slice =
            sps->sps_log2_diff_max_bt_min_qt_inter_slice;
        ph->ph_log2_diff_max_tt_min_qt_inter_slice =
            sps->sps_log2_diff_max_tt_min_qt_inter_slice;
      }
    } else {
      ph->ph_log2_diff_min_qt_min_cb_inter_slice =
          sps->sps_log2_diff_max_bt_min_qt_inter_slice;
      ph->ph_max_mtt_hierarchy_depth_inter_slice =
          sps->sps_max_mtt_hierarchy_depth_inter_slice;
      ph->ph_log2_diff_max_bt_min_qt_inter_slice =
          sps->sps_log2_diff_max_bt_min_qt_inter_slice;
      ph->ph_log2_diff_max_tt_min_qt_inter_slice =
          sps->sps_log2_diff_max_tt_min_qt_inter_slice;
    }

    if (pps->pps_cu_qp_delta_enabled_flag) {
      READ_UE_OR_RETURN(&ph->ph_cu_qp_delta_subdiv_inter_slice);
      IN_RANGE_OR_RETURN(
          ph->ph_cu_qp_delta_subdiv_inter_slice, 0,
          2 * (sps->ctb_log2_size_y - ph->min_qt_log2_size_inter_y +
               ph->ph_max_mtt_hierarchy_depth_inter_slice));
    } else {
      ph->ph_cu_qp_delta_subdiv_inter_slice = 0;
    }

    if (pps->pps_cu_chroma_qp_offset_list_enabled_flag) {
      READ_UE_OR_RETURN(&ph->ph_cu_chroma_qp_offset_subdiv_inter_slice);
      IN_RANGE_OR_RETURN(
          ph->ph_cu_chroma_qp_offset_subdiv_inter_slice, 0,
          2 * (sps->ctb_log2_size_y - ph->min_qt_log2_size_inter_y +
               ph->ph_max_mtt_hierarchy_depth_inter_slice));
    } else {
      ph->ph_cu_chroma_qp_offset_subdiv_inter_slice = 0;
    }

    if (sps->sps_temporal_mvp_enabled_flag) {
      READ_BOOL_OR_RETURN(&ph->ph_temporal_mvp_enabled_flag);

      if (ph->ph_temporal_mvp_enabled_flag && pps->pps_rpl_info_in_ph_flag) {
        if (ph->ref_pic_lists.rpl_ref_lists[1].num_ref_entries > 0) {
          READ_BOOL_OR_RETURN(&ph->ph_collocated_from_l0_flag);
        } else {
          ph->ph_collocated_from_l0_flag = 1;
        }

        if ((ph->ph_collocated_from_l0_flag &&
             ph->ref_pic_lists.rpl_ref_lists[0].num_ref_entries > 1) ||
            (!ph->ph_collocated_from_l0_flag &&
             ph->ref_pic_lists.rpl_ref_lists[1].num_ref_entries > 1)) {
          READ_UE_OR_RETURN(&ph->ph_collocated_ref_idx);
          if (ph->ph_collocated_from_l0_flag) {
            IN_RANGE_OR_RETURN(
                ph->ph_collocated_ref_idx, 0,
                ph->ref_pic_lists.rpl_ref_lists[0].num_ref_entries - 1);
          } else {
            IN_RANGE_OR_RETURN(
                ph->ph_collocated_ref_idx, 0,
                ph->ref_pic_lists.rpl_ref_lists[1].num_ref_entries - 1);
          }
        } else {
          ph->ph_collocated_ref_idx = 0;
        }
      }
    }

    if (sps->sps_mmvd_fullpel_only_enabled_flag) {
      READ_BOOL_OR_RETURN(&ph->ph_mmvd_fullpel_only_flag);
    } else {
      ph->ph_mmvd_fullpel_only_flag = 0;
    }

    bool presence_flag = 0;
    if (!pps->pps_rpl_info_in_ph_flag) {
      presence_flag = 1;
    } else if (ph->ref_pic_lists.rpl_ref_lists[1].num_ref_entries > 0) {
      presence_flag = 1;
    }
    if (presence_flag) {
      READ_BOOL_OR_RETURN(&ph->ph_mvd_l1_zero_flag);
      if (sps->sps_bdof_control_present_in_ph_flag) {
        READ_BOOL_OR_RETURN(&ph->ph_bdof_disabled_flag);
      } else {
        ph->ph_bdof_disabled_flag = 1 - sps->sps_bdof_enabled_flag;
      }
      if (sps->sps_dmvr_control_present_in_ph_flag) {
        READ_BOOL_OR_RETURN(&ph->ph_dmvr_disabled_flag);
      } else {
        ph->ph_dmvr_disabled_flag = 1 - sps->sps_dmvr_enabled_flag;
      }
    } else {
      ph->ph_mvd_l1_zero_flag = 1;
      ph->ph_bdof_disabled_flag = 1;
    }

    if (sps->sps_prof_control_present_in_ph_flag) {
      READ_BOOL_OR_RETURN(&ph->ph_prof_disabled_flag);
    } else {
      if (sps->sps_affine_prof_enabled_flag) {
        ph->ph_prof_disabled_flag = 0;
      } else {
        ph->ph_prof_disabled_flag = 1;
      }
    }

    if ((pps->pps_weighted_pred_flag || pps->pps_weighted_bipred_flag) &&
        pps->pps_wp_info_in_ph_flag) {
      int num_active_ref_idx[2] = {0, 0};
      ParsePredWeightTable(*sps, *pps, ph->ref_pic_lists, num_active_ref_idx,
                           &ph->pred_weight_table);
    }
  }  // ph_inter_slice_allowed_flag

  if (pps->pps_qp_delta_info_in_ph_flag) {
    READ_SE_OR_RETURN(&ph->ph_qp_delta);
    // Equation 86
    ph->slice_qp_y = 26 + pps->pps_init_qp_minus26 + ph->ph_qp_delta;
    IN_RANGE_OR_RETURN(ph->slice_qp_y, -sps->qp_bd_offset, 63);
  }

  if (sps->sps_joint_cbcr_enabled_flag) {
    READ_BOOL_OR_RETURN(&ph->ph_joint_cbcr_sign_flag);
  }

  if (sps->sps_sao_enabled_flag && pps->pps_sao_info_in_ph_flag) {
    READ_BOOL_OR_RETURN(&ph->ph_sao_luma_enabled_flag);
    if (sps->sps_chroma_format_idc != 0) {
      READ_BOOL_OR_RETURN(&ph->ph_sao_chroma_enabled_flag);
    } else {
      ph->ph_sao_chroma_enabled_flag = 0;
    }
  } else {
    ph->ph_sao_luma_enabled_flag = 0;
    ph->ph_sao_chroma_enabled_flag = 0;
  }

  if (pps->pps_dbf_info_in_ph_flag) {
    READ_BOOL_OR_RETURN(&ph->ph_deblocking_params_present_flag);
    if (ph->ph_deblocking_params_present_flag) {
      if (!pps->pps_deblocking_filter_disabled_flag) {
        READ_BOOL_OR_RETURN(&ph->ph_deblocking_filter_disabled_flag);
      } else {
        if (pps->pps_deblocking_filter_disabled_flag &&
            ph->ph_deblocking_params_present_flag) {
          ph->ph_deblocking_filter_disabled_flag = 0;
        } else {
          ph->ph_deblocking_filter_disabled_flag =
              pps->pps_deblocking_filter_disabled_flag;
        }
      }
    }
    if (!ph->ph_deblocking_filter_disabled_flag) {
      READ_SE_OR_RETURN(&ph->ph_luma_beta_offset_div2);
      READ_SE_OR_RETURN(&ph->ph_luma_tc_offset_div2);
      IN_RANGE_OR_RETURN(ph->ph_luma_beta_offset_div2, -12, 12);
      IN_RANGE_OR_RETURN(ph->ph_luma_tc_offset_div2, -12, 12);

      if (pps->pps_chroma_tool_offsets_present_flag) {
        READ_SE_OR_RETURN(&ph->ph_cb_beta_offset_div2);
        READ_SE_OR_RETURN(&ph->ph_cb_tc_offset_div2);
        READ_SE_OR_RETURN(&ph->ph_cr_beta_offset_div2);
        READ_SE_OR_RETURN(&ph->ph_cr_tc_offset_div2);
        IN_RANGE_OR_RETURN(ph->ph_cb_beta_offset_div2, -12, 12);
        IN_RANGE_OR_RETURN(ph->ph_cb_tc_offset_div2, -12, 12);
        IN_RANGE_OR_RETURN(ph->ph_cr_beta_offset_div2, -12, 12);
        IN_RANGE_OR_RETURN(ph->ph_cr_tc_offset_div2, -12, 12);
      } else {
        if (pps->pps_chroma_tool_offsets_present_flag) {
          ph->ph_cb_beta_offset_div2 = pps->pps_cb_beta_offset_div2;
          ph->ph_cb_tc_offset_div2 = pps->pps_cb_tc_offset_div2;
          ph->ph_cr_beta_offset_div2 = pps->pps_cr_beta_offset_div2;
          ph->ph_cr_tc_offset_div2 = pps->pps_cr_tc_offset_div2;
        } else {
          ph->ph_cb_beta_offset_div2 = ph->ph_luma_beta_offset_div2;
          ph->ph_cb_tc_offset_div2 = ph->ph_luma_tc_offset_div2;
          ph->ph_cr_beta_offset_div2 = ph->ph_luma_beta_offset_div2;
          ph->ph_cr_tc_offset_div2 = ph->ph_luma_tc_offset_div2;
        }
      }
    } else {
      ph->ph_luma_beta_offset_div2 = pps->pps_luma_beta_offset_div2;
      ph->ph_luma_tc_offset_div2 = pps->pps_luma_tc_offset_div2;
      if (pps->pps_chroma_tool_offsets_present_flag) {
        ph->ph_cb_beta_offset_div2 = pps->pps_cb_beta_offset_div2;
        ph->ph_cb_tc_offset_div2 = pps->pps_cb_tc_offset_div2;
        ph->ph_cr_beta_offset_div2 = pps->pps_cr_beta_offset_div2;
        ph->ph_cr_tc_offset_div2 = pps->pps_cr_tc_offset_div2;
      } else {
        ph->ph_cb_beta_offset_div2 = ph->ph_luma_beta_offset_div2;
        ph->ph_cb_tc_offset_div2 = ph->ph_luma_tc_offset_div2;
        ph->ph_cr_beta_offset_div2 = ph->ph_luma_beta_offset_div2;
        ph->ph_cr_tc_offset_div2 = ph->ph_luma_tc_offset_div2;
      }
    }
  } else {
    ph->ph_deblocking_params_present_flag = 0;
  }

  // We stop here and do not parse ph extension when
  // pps_picture_header_extension_present_flag is 1.

  return kOk;
}

// Decoder may pass |picture_header| which is already retrieved
// from PH_NUT, so for current slice we're not expecting any picture header
// structure parsed from current slice; Or alternatively, decoder may pass
// nullptr for |picture_header|, which decoder expects the parser to avoid any
// picture header parsing, and return error in case it does exist in current
// slice.
H266Parser::Result H266Parser::ParseSliceHeader(
    const H266NALU& nalu,
    bool first_picture,
    const H266PictureHeader* picture_header,
    H266SliceHeader* shdr) {
  // 7.3.7 Slice header
  DVLOG(4) << "Parsing slice header";

  Result res = kOk;
  const H266SPS* sps;
  const H266PPS* pps;
  H266PictureHeader* ph = nullptr;

  // Decoder may pass |picture_header| as nullptr here when
  // picture header structure is in slice header.
  DCHECK(shdr);
  // Clear the slice header.
  memset(reinterpret_cast<void*>(shdr), 0, sizeof(H266SliceHeader));

  shdr->nal_unit_type = nalu.nal_unit_type;
  shdr->nuh_layer_id = nalu.nuh_layer_id;
  shdr->temporal_id = nalu.nuh_temporal_id_plus1 - 1;
  shdr->nalu_data = nalu.data;
  shdr->nalu_size = nalu.size;

  bool is_irap = shdr->nal_unit_type <= H266NALU::kCRA &&
                 shdr->nal_unit_type >= H266NALU::kIDRWithRADL;
  bool is_gdr = shdr->nal_unit_type == H266NALU::kGDR;

  // 8.1.1
  if (is_irap) {
    if (first_picture ||
        (shdr->nal_unit_type == H266NALU::kIDRWithRADL ||
         shdr->nal_unit_type == H266NALU::kIDRNoLeadingPicture)) {
      shdr->no_output_before_recovery_flag = 1;
    } else if (shdr->handle_cra_as_clvs_start_flag.has_value()) {
      shdr->cra_as_clvs_start_flag =
          shdr->handle_cra_as_clvs_start_flag.value();
      shdr->no_output_before_recovery_flag = shdr->cra_as_clvs_start_flag;
    } else {
      shdr->cra_as_clvs_start_flag = 0;
      shdr->no_output_before_recovery_flag = 0;
    }
  }

  if (is_gdr) {
    if (first_picture) {
      shdr->no_output_before_recovery_flag = 1;
    } else if (shdr->handle_gdr_as_clvs_start_flag.has_value()) {
      shdr->gdr_as_clvs_start_flag =
          shdr->handle_gdr_as_clvs_start_flag.value();
      shdr->no_output_before_recovery_flag = shdr->gdr_as_clvs_start_flag;
    } else {
      shdr->gdr_as_clvs_start_flag = 0;
      shdr->no_output_before_recovery_flag = 0;
    }
  }

  READ_BOOL_OR_RETURN(&shdr->sh_picture_header_in_slice_header_flag);
  if (shdr->sh_picture_header_in_slice_header_flag) {
    res = ParsePHInSlice(nalu, &shdr->picture_header);
    if (res != kOk) {
      DVLOG(1) << "Failed to parse picture header structure in slice header.";
      return kMissingPictureHeader;
    }
  }

  if (!shdr->sh_picture_header_in_slice_header_flag) {
    if (!picture_header) {
      DVLOG(1) << "No picture header available for current slice.";
      return kMissingPictureHeader;
    } else {
      memcpy(&shdr->picture_header, picture_header, sizeof(H266PictureHeader));
    }
  }
  ph = &shdr->picture_header;

  pps = GetPPS(ph->ph_pic_parameter_set_id);
  if (!pps) {
    DVLOG(1) << "Cannot find the PPS associated with current slice.";
    return kMissingParameterSet;
  }

  sps = GetSPS(pps->pps_seq_parameter_set_id);
  if (!sps) {
    DVLOG(1) << "Cannot find the SPS associated with current slice.";
    return kMissingParameterSet;
  }

  if (shdr->sh_picture_header_in_slice_header_flag) {
    if (sps->sps_subpic_info_present_flag || !pps->pps_rect_slice_flag ||
        pps->pps_rpl_info_in_ph_flag || pps->pps_dbf_info_in_ph_flag ||
        pps->pps_sao_info_in_ph_flag || pps->pps_alf_info_in_ph_flag ||
        pps->pps_wp_info_in_ph_flag || pps->pps_qp_delta_info_in_ph_flag) {
      // TODO(crbugs.com/1417910): Return instead of just raise a warning here.
      // By now VTM does not follow spec for sh_picture_header_in_slice_flag.
      DVLOG(1) << "PH is required to be in PH_NUT for current stream, while it "
                  "is in slice header instead.";
    }
  }
  int curr_subpic_idx = -1;
  if (sps->sps_subpic_info_present_flag) {
    READ_BITS_OR_RETURN(sps->sps_subpic_id_len_minus1 + 1, &shdr->sh_subpic_id);
    if (sps->sps_subpic_id_mapping_explicitly_signaled_flag) {
      for (int i = 0; i <= sps->sps_num_subpics_minus1; i++) {
        int subpic_id_val = pps->pps_subpic_id_mapping_present_flag
                                ? pps->pps_subpic_id[i]
                                : sps->sps_subpic_id[i];
        if (subpic_id_val == shdr->sh_subpic_id) {
          curr_subpic_idx = i;
        }
      }
    } else {
      curr_subpic_idx = shdr->sh_subpic_id;
      if (curr_subpic_idx > sps->sps_num_subpics_minus1) {
        DVLOG(1) << "Invalid subpicture ID in slice header.";
        return kInvalidStream;
      }
    }
  } else {
    curr_subpic_idx = 0;
  }

  if ((pps->pps_rect_slice_flag &&
       pps->num_slices_in_subpic[curr_subpic_idx] > 1) ||
      (!pps->pps_rect_slice_flag && pps->num_tiles_in_pic > 1)) {
    if (!pps->pps_rect_slice_flag) {
      READ_BITS_OR_RETURN(base::bits::Log2Ceiling(pps->num_tiles_in_pic),
                          &shdr->sh_slice_address);
      IN_RANGE_OR_RETURN(shdr->sh_slice_address, 0, pps->num_tiles_in_pic - 1);
    } else {
      READ_BITS_OR_RETURN(
          base::bits::Log2Ceiling(pps->num_slices_in_subpic[curr_subpic_idx]),
          &shdr->sh_slice_address);
      IN_RANGE_OR_RETURN(shdr->sh_slice_address, 0,
                         pps->num_slices_in_subpic[curr_subpic_idx] - 1);
    }
  }

  if (sps->num_extra_sh_bits > 0) {
    SKIP_BITS_OR_RETURN(sps->num_extra_sh_bits);
  }

  if (!pps->pps_rect_slice_flag &&
      (pps->num_tiles_in_pic - shdr->sh_slice_address) > 1) {
    READ_UE_OR_RETURN(&shdr->sh_num_tiles_in_slice_minus1);
    IN_RANGE_OR_RETURN(shdr->sh_num_tiles_in_slice_minus1, 0,
                       pps->num_tiles_in_pic - 1);
  }

  if (ph->ph_inter_slice_allowed_flag) {
    READ_UE_OR_RETURN(&shdr->sh_slice_type);
  } else {
    shdr->sh_slice_type = 2;
  }

  if (!ph->ph_intra_slice_allowed_flag && shdr->sh_slice_type == 2) {
    DVLOG(1) << "I-slice disallowed when intra-slice is not allowed in picture "
                "header.";
    return kInvalidStream;
  }

  if (shdr->nal_unit_type >= H266NALU::kIDRWithRADL &&
      shdr->nal_unit_type <= H266NALU::kGDR) {
    READ_BOOL_OR_RETURN(&shdr->sh_no_output_of_prior_pics_flag);
  }

  // Alf info is not in picture header, but instead in slice header directly.
  if (sps->sps_alf_enabled_flag && !pps->pps_alf_info_in_ph_flag) {
    READ_BOOL_OR_RETURN(&shdr->sh_alf_enabled_flag);
    // When sh_alf_enabled_flag is not inferred but read from SH, current slice
    // is not using alf, so we don't need to infer alf params from PH as they
    // default to 0.
    if (shdr->sh_alf_enabled_flag) {
      READ_BITS_OR_RETURN(3, &shdr->sh_num_alf_aps_ids_luma);
      for (int i = 0; i < shdr->sh_num_alf_aps_ids_luma; i++) {
        READ_BITS_OR_RETURN(3, &shdr->sh_alf_aps_id_luma[i]);
      }
      if (sps->sps_chroma_format_idc != 0) {
        READ_BOOL_OR_RETURN(&shdr->sh_alf_cb_enabled_flag);
        READ_BOOL_OR_RETURN(&shdr->sh_alf_cr_enabled_flag);
      } else {
        // When pps->alf_info_in_ph_flag is 0, expecting sh_alf_cb_enabled_flag
        // and sh_alf_cr_enabled_flag to be 0 as well. So they're not impacting
        // the parsing logic.
        shdr->sh_alf_cb_enabled_flag = ph->ph_alf_cb_enabled_flag;
        shdr->sh_alf_cr_enabled_flag = ph->ph_alf_cr_enabled_flag;
      }
      if (shdr->sh_alf_cb_enabled_flag || shdr->sh_alf_cr_enabled_flag) {
        READ_BITS_OR_RETURN(3, &shdr->sh_alf_aps_id_chroma);
      } else {
        shdr->sh_alf_aps_id_chroma = ph->ph_alf_aps_id_chroma;
      }
      if (sps->sps_ccalf_enabled_flag) {
        READ_BOOL_OR_RETURN(&shdr->sh_alf_cc_cb_enabled_flag);
        if (shdr->sh_alf_cc_cb_enabled_flag) {
          READ_BITS_OR_RETURN(3, &shdr->sh_alf_cc_cb_aps_id);
        } else {
          shdr->sh_alf_cc_cr_enabled_flag = ph->ph_alf_cc_cb_enabled_flag;
        }
        READ_BOOL_OR_RETURN(&shdr->sh_alf_cc_cr_enabled_flag);
        if (shdr->sh_alf_cc_cr_enabled_flag) {
          READ_BITS_OR_RETURN(3, &shdr->sh_alf_cc_cr_aps_id);
        } else {
          shdr->sh_alf_cc_cr_aps_id = ph->ph_alf_cc_cr_aps_id;
        }
      } else {
        shdr->sh_alf_cc_cb_enabled_flag = ph->ph_alf_cc_cb_enabled_flag;
        shdr->sh_alf_cc_cb_aps_id = ph->ph_alf_cc_cb_aps_id;
        shdr->sh_alf_cc_cr_aps_id = ph->ph_alf_cc_cr_aps_id;
      }
    }
  } else {
    shdr->sh_alf_enabled_flag = ph->ph_alf_enabled_flag;
    if (shdr->sh_alf_enabled_flag) {
      shdr->sh_num_alf_aps_ids_luma = ph->ph_num_alf_aps_ids_luma;
      for (int i = 0; i < shdr->sh_num_alf_aps_ids_luma; i++) {
        shdr->sh_alf_aps_id_luma[i] = ph->ph_alf_aps_id_luma[i];
      }
      shdr->sh_alf_aps_id_chroma = ph->ph_alf_aps_id_chroma;
      shdr->sh_alf_cb_enabled_flag = ph->ph_alf_cb_enabled_flag;
      shdr->sh_alf_cr_enabled_flag = ph->ph_alf_cr_enabled_flag;
      shdr->sh_alf_cc_cb_enabled_flag = ph->ph_alf_cc_cb_enabled_flag;
      shdr->sh_alf_cc_cr_enabled_flag = ph->ph_alf_cc_cr_enabled_flag;
      shdr->sh_alf_cc_cb_aps_id = ph->ph_alf_cc_cb_aps_id;
      shdr->sh_alf_cc_cr_aps_id = ph->ph_alf_cc_cr_aps_id;
    }
  }

  if (ph->ph_lmcs_enabled_flag &&
      !shdr->sh_picture_header_in_slice_header_flag) {
    READ_BOOL_OR_RETURN(&shdr->sh_lmcs_used_flag);
  } else {
    shdr->sh_lmcs_used_flag = shdr->sh_picture_header_in_slice_header_flag
                                  ? ph->ph_lmcs_enabled_flag
                                  : 0;
  }

  if (ph->ph_explicit_scaling_list_enabled_flag &&
      !shdr->sh_picture_header_in_slice_header_flag) {
    READ_BOOL_OR_RETURN(&shdr->sh_explicit_scaling_list_used_flag);
  } else {
    shdr->sh_explicit_scaling_list_used_flag =
        shdr->sh_picture_header_in_slice_header_flag
            ? ph->ph_explicit_scaling_list_enabled_flag
            : 0;
  }

  const H266RefPicLists* ref_pic_lists = nullptr;
  if (!pps->pps_rpl_info_in_ph_flag &&
      ((shdr->nal_unit_type != H266NALU::kIDRWithRADL &&
        shdr->nal_unit_type != H266NALU::kIDRNoLeadingPicture) ||
       sps->sps_idr_rpl_present_flag)) {
    ParseRefPicLists(*sps, *pps, &shdr->ref_pic_lists);
    ref_pic_lists = &shdr->ref_pic_lists;
  } else {
    ref_pic_lists = &ph->ref_pic_lists;
  }

  const H266RefPicListStruct* ref_pic_list_structs[2] = {nullptr, nullptr};
  for (int i = 0; i < 2; i++) {
    if (ref_pic_lists->rpl_sps_flag[i]) {
      ref_pic_list_structs[i] =
          &sps->ref_pic_list_struct[0][ref_pic_lists->rpl_idx[i]];
    } else {
      ref_pic_list_structs[i] = &ref_pic_lists->rpl_ref_lists[i];
    }
  }

  // Caution!!! |sh_num_ref_idx_active_override| might be inferred
  // instead of read from bitstream, and used for subsequent syntax
  // parsing after it.
  shdr->sh_num_ref_idx_active_override_flag = 1;
  if ((shdr->sh_slice_type != H266SliceHeader::kSliceTypeI &&
       ref_pic_list_structs[0]->num_ref_entries > 1) ||
      (shdr->sh_slice_type == H266SliceHeader::kSliceTypeB &&
       ref_pic_list_structs[1]->num_ref_entries > 1)) {
    READ_BOOL_OR_RETURN(&shdr->sh_num_ref_idx_active_override_flag);
    if (shdr->sh_num_ref_idx_active_override_flag) {
      for (int i = 0;
           i < (shdr->sh_slice_type == H266SliceHeader::kSliceTypeB ? 2 : 1);
           i++) {
        if (ref_pic_list_structs[i]->num_ref_entries > 1) {
          READ_UE_OR_RETURN(&shdr->sh_num_ref_idx_active_minus1[i]);
          IN_RANGE_OR_RETURN(shdr->sh_num_ref_idx_active_minus1[i], 0, 14);
        } else {
          shdr->sh_num_ref_idx_active_minus1[i] = 0;
        }
      }
    }
  }

  // Equation 139
  for (int i = 0; i < 2; i++) {
    if (shdr->sh_slice_type == H266SliceHeader::kSliceTypeB ||
        (shdr->sh_slice_type == H266SliceHeader::kSliceTypeP && i == 0)) {
      if (shdr->sh_num_ref_idx_active_override_flag) {
        shdr->num_ref_idx_active[i] = shdr->sh_num_ref_idx_active_minus1[i] + 1;
      } else {
        if (ref_pic_list_structs[i]->num_ref_entries >=
            pps->pps_num_ref_idx_default_active_minus1[i] + 1) {
          shdr->num_ref_idx_active[i] =
              pps->pps_num_ref_idx_default_active_minus1[i] + 1;
        } else {
          shdr->num_ref_idx_active[i] =
              ref_pic_list_structs[i]->num_ref_entries;
        }
      }
    } else {
      shdr->num_ref_idx_active[i] = 0;
    }
  }

  if (shdr->sh_slice_type != H266SliceHeader::kSliceTypeI) {
    if (pps->pps_cabac_init_present_flag) {
      READ_BOOL_OR_RETURN(&shdr->sh_cabac_init_flag);
    } else {
      shdr->sh_cabac_init_flag = 0;
    }

    if (ph->ph_temporal_mvp_enabled_flag && !pps->pps_rpl_info_in_ph_flag) {
      if (shdr->sh_slice_type == H266SliceHeader::kSliceTypeB) {
        READ_BOOL_OR_RETURN(&shdr->sh_collocated_from_l0_flag);
      } else {
        shdr->sh_collocated_from_l0_flag = 1;
      }
      if ((shdr->sh_collocated_from_l0_flag &&
           shdr->num_ref_idx_active[0] > 1) ||
          (!shdr->sh_collocated_from_l0_flag &&
           shdr->num_ref_idx_active[1] > 1)) {
        READ_UE_OR_RETURN(&shdr->sh_collocated_ref_idx);
        if (shdr->sh_slice_type == H266SliceHeader::kSliceTypeP ||
            (shdr->sh_slice_type == H266SliceHeader::kSliceTypeB &&
             shdr->sh_collocated_from_l0_flag)) {
          IN_RANGE_OR_RETURN(shdr->sh_collocated_ref_idx, 0,
                             shdr->num_ref_idx_active[0] - 1);
        } else if (shdr->sh_slice_type == H266SliceHeader::kSliceTypeB &&
                   !shdr->sh_collocated_from_l0_flag) {
          IN_RANGE_OR_RETURN(shdr->sh_collocated_ref_idx, 0,
                             shdr->num_ref_idx_active[1] - 1);
        }
      }
    } else {
      if (ph->ph_temporal_mvp_enabled_flag) {
        if (shdr->sh_slice_type == H266SliceHeader::kSliceTypeB) {
          shdr->sh_collocated_from_l0_flag = ph->ph_collocated_from_l0_flag;
        } else {
          shdr->sh_collocated_from_l0_flag = 1;
        }
      }
    }

    if (!pps->pps_wp_info_in_ph_flag &&
        ((pps->pps_weighted_pred_flag &&
          shdr->sh_slice_type == H266SliceHeader::kSliceTypeP) ||
         (pps->pps_weighted_bipred_flag &&
          shdr->sh_slice_type == H266SliceHeader::kSliceTypeB))) {
      ParsePredWeightTable(*sps, *pps, *ref_pic_lists,
                           &shdr->num_ref_idx_active[0],
                           &shdr->sh_pred_weight_table);
    }
  }

  if (!pps->pps_qp_delta_info_in_ph_flag) {
    READ_SE_OR_RETURN(&shdr->sh_qp_delta);
    // Equation 140
    shdr->slice_qp_y = 26 + pps->pps_init_qp_minus26 + shdr->sh_qp_delta;
    IN_RANGE_OR_RETURN(shdr->slice_qp_y, -sps->qp_bd_offset, 63);
  }
  if (pps->pps_slice_chroma_qp_offsets_present_flag) {
    READ_SE_OR_RETURN(&shdr->sh_cb_qp_offset);
    READ_SE_OR_RETURN(&shdr->sh_cr_qp_offset);
    if (sps->sps_joint_cbcr_enabled_flag) {
      READ_SE_OR_RETURN(&shdr->sh_joint_cbcr_qp_offset);
    }
    IN_RANGE_OR_RETURN(shdr->sh_cb_qp_offset, -12, 12);
    IN_RANGE_OR_RETURN(shdr->sh_cr_qp_offset, -12, 12);
    IN_RANGE_OR_RETURN(shdr->sh_joint_cbcr_qp_offset, -12, 12);
  }
  IN_RANGE_OR_RETURN(pps->pps_cb_qp_offset + shdr->sh_cb_qp_offset, -12, 12);
  IN_RANGE_OR_RETURN(pps->pps_cr_qp_offset + shdr->sh_cr_qp_offset, -12, 12);
  IN_RANGE_OR_RETURN(
      pps->pps_joint_cbcr_qp_offset_value + shdr->sh_joint_cbcr_qp_offset, -12,
      12);
  if (pps->pps_cu_chroma_qp_offset_list_enabled_flag) {
    READ_BOOL_OR_RETURN(&shdr->sh_cu_chroma_qp_offset_enabled_flag);
  }

  if (sps->sps_sao_enabled_flag && !pps->pps_sao_info_in_ph_flag) {
    READ_BOOL_OR_RETURN(&shdr->sh_sao_luma_used_flag);
    if (sps->sps_chroma_format_idc != 0) {
      READ_BOOL_OR_RETURN(&shdr->sh_sao_chroma_used_flag);
    } else {
      shdr->sh_sao_chroma_used_flag = ph->ph_sao_chroma_enabled_flag;
    }
  } else {
    shdr->sh_sao_luma_used_flag = ph->ph_sao_luma_enabled_flag;
    shdr->sh_sao_chroma_used_flag = ph->ph_sao_chroma_enabled_flag;
  }

  if (pps->pps_deblocking_filter_override_enabled_flag &&
      !pps->pps_dbf_info_in_ph_flag) {
    READ_BOOL_OR_RETURN(&shdr->sh_deblocking_params_present_flag);
  }

  // Load default chroma/luma beta/tc offsets from PH.
  if (pps->pps_chroma_tool_offsets_present_flag) {
    shdr->sh_luma_beta_offset_div2 = ph->ph_luma_beta_offset_div2;
    shdr->sh_luma_tc_offset_div2 = ph->ph_luma_tc_offset_div2;
    shdr->sh_cb_beta_offset_div2 = ph->ph_cb_beta_offset_div2;
    shdr->sh_cb_tc_offset_div2 = ph->ph_cb_tc_offset_div2;
    shdr->sh_cr_beta_offset_div2 = ph->ph_cr_beta_offset_div2;
    shdr->sh_cr_tc_offset_div2 = ph->ph_cr_tc_offset_div2;
  }
  if (shdr->sh_deblocking_params_present_flag) {
    if (!pps->pps_deblocking_filter_disabled_flag) {
      READ_BOOL_OR_RETURN(&shdr->sh_deblocking_filter_disabled_flag);
    }
    if (!shdr->sh_deblocking_filter_disabled_flag) {
      READ_SE_OR_RETURN(&shdr->sh_luma_beta_offset_div2);
      READ_SE_OR_RETURN(&shdr->sh_luma_tc_offset_div2);
      IN_RANGE_OR_RETURN(shdr->sh_luma_beta_offset_div2, -12, 12);
      IN_RANGE_OR_RETURN(shdr->sh_luma_tc_offset_div2, -12, 12);
      if (pps->pps_chroma_tool_offsets_present_flag) {
        READ_SE_OR_RETURN(&shdr->sh_cb_beta_offset_div2);
        READ_SE_OR_RETURN(&shdr->sh_cb_tc_offset_div2);
        READ_SE_OR_RETURN(&shdr->sh_cr_beta_offset_div2);
        READ_SE_OR_RETURN(&shdr->sh_cr_tc_offset_div2);
        IN_RANGE_OR_RETURN(shdr->sh_cb_beta_offset_div2, -12, 12);
        IN_RANGE_OR_RETURN(shdr->sh_cb_tc_offset_div2, -12, 12);
        IN_RANGE_OR_RETURN(shdr->sh_cr_beta_offset_div2, -12, 12);
        IN_RANGE_OR_RETURN(shdr->sh_cr_tc_offset_div2, -12, 12);
      } else {
        shdr->sh_cb_beta_offset_div2 = shdr->sh_luma_beta_offset_div2;
        shdr->sh_cb_tc_offset_div2 = shdr->sh_luma_tc_offset_div2;
        shdr->sh_cr_beta_offset_div2 = shdr->sh_luma_beta_offset_div2;
        shdr->sh_cr_tc_offset_div2 = shdr->sh_luma_tc_offset_div2;
      }
    } else {
      if (!pps->pps_chroma_tool_offsets_present_flag) {
        shdr->sh_cb_beta_offset_div2 = shdr->sh_luma_beta_offset_div2;
        shdr->sh_cb_tc_offset_div2 = shdr->sh_luma_tc_offset_div2;
        shdr->sh_cr_beta_offset_div2 = shdr->sh_luma_beta_offset_div2;
        shdr->sh_cr_tc_offset_div2 = shdr->sh_luma_tc_offset_div2;
      }
    }
  } else {
    if (!pps->pps_deblocking_filter_disabled_flag ||
        !shdr->sh_deblocking_params_present_flag) {
      shdr->sh_deblocking_filter_disabled_flag =
          ph->ph_deblocking_filter_disabled_flag;
    }
    if (!pps->pps_chroma_tool_offsets_present_flag) {
      shdr->sh_cb_beta_offset_div2 = shdr->sh_luma_beta_offset_div2;
      shdr->sh_cb_tc_offset_div2 = shdr->sh_luma_tc_offset_div2;
      shdr->sh_cr_beta_offset_div2 = shdr->sh_luma_beta_offset_div2;
      shdr->sh_cr_tc_offset_div2 = shdr->sh_luma_tc_offset_div2;
    }
  }

  if (sps->sps_dep_quant_enabled_flag) {
    READ_BOOL_OR_RETURN(&shdr->sh_dep_quant_used_flag);
  }
  if (sps->sps_sign_data_hiding_enabled_flag && !shdr->sh_dep_quant_used_flag) {
    READ_BOOL_OR_RETURN(&shdr->sh_sign_data_hiding_used_flag);
  }
  if (sps->sps_transform_skip_enabled_flag && !shdr->sh_dep_quant_used_flag &&
      !shdr->sh_sign_data_hiding_used_flag) {
    READ_BOOL_OR_RETURN(&shdr->sh_ts_residual_coding_disabled_flag);
  }
  if (!shdr->sh_ts_residual_coding_disabled_flag &&
      sps->sps_range_extension.sps_ts_residual_coding_rice_present_in_sh_flag) {
    READ_BITS_OR_RETURN(3, &shdr->sh_ts_residual_coding_rice_idx_minus1);
  }
  if (sps->sps_range_extension.sps_reverse_last_sig_coeff_enabled_flag) {
    READ_BOOL_OR_RETURN(&shdr->sh_reverse_last_sig_coeff_flag);
  }
  if (pps->pps_slice_header_extension_present_flag) {
    READ_UE_OR_RETURN(&shdr->sh_slice_header_extension_length);
    IN_RANGE_OR_RETURN(shdr->sh_slice_header_extension_length, 0, 256);
    for (int i = 0; i < shdr->sh_slice_header_extension_length; i++) {
      SKIP_BITS_OR_RETURN(8);
    }
  }

  // Equation 113 & 141
  int num_entry_points = 0;
  if (sps->sps_entry_point_offsets_present_flag) {
    if (pps->pps_rect_slice_flag) {
      int width_in_tiles = 0, slice_idx = shdr->sh_slice_address;
      for (int i = 0; i < curr_subpic_idx; i++) {
        slice_idx += pps->num_slices_in_subpic[i];
      }
      width_in_tiles = pps->pps_slice_width_in_tiles_minus1[slice_idx] + 1;
      int height = sps->sps_entropy_coding_sync_enabled_flag
                       ? pps->slice_height_in_ctus[slice_idx]
                       : pps->pps_slice_height_in_tiles_minus1[slice_idx] + 1;
      num_entry_points = width_in_tiles * height;
    } else {
      int tile_idx = 0, tile_y = 0, height = 0;
      for (tile_idx = shdr->sh_slice_address;
           tile_idx <=
           shdr->sh_slice_address + shdr->sh_num_tiles_in_slice_minus1;
           tile_idx++) {
        tile_y = tile_idx / pps->num_tile_rows;
        height = pps->pps_tile_row_height_minus1[tile_y] + 1;
        num_entry_points +=
            sps->sps_entropy_coding_sync_enabled_flag ? height : 1;
      }
    }

    num_entry_points--;
    if (num_entry_points > 0) {
      shdr->sh_entry_point_offset_minus1.resize(num_entry_points);
      READ_UE_OR_RETURN(&shdr->sh_entry_offset_len_minus1);
      IN_RANGE_OR_RETURN(shdr->sh_entry_offset_len_minus1, 0, 31);
      for (int i = 0; i < num_entry_points; i++) {
        if (shdr->sh_entry_offset_len_minus1 == 31) {
          int entry_offset_high16 = 0, entry_offset_low16 = 0;
          READ_BITS_OR_RETURN(16, &entry_offset_high16);
          READ_BITS_OR_RETURN(16, &entry_offset_low16);
          shdr->sh_entry_point_offset_minus1[i] =
              (entry_offset_high16 << 16) + entry_offset_low16;
        } else {
          READ_BITS_OR_RETURN(shdr->sh_entry_offset_len_minus1 + 1,
                              &shdr->sh_entry_point_offset_minus1[i]);
        }
      }
    }
  }

  BYTE_ALIGNMENT();
  shdr->header_emulation_prevention_bytes =
      br_.NumEmulationPreventionBytesRead();
  shdr->header_size = shdr->nalu_size -
                      shdr->header_emulation_prevention_bytes -
                      br_.NumBitsLeft() / 8;
  return kOk;
}

const H266VPS* H266Parser::GetVPS(int vps_id) const {
  auto it = active_vps_.find(vps_id);
  if (it == active_vps_.end()) {
    DVLOG(1) << "Requested a nonexistent VPS id " << vps_id;
    return nullptr;
  }
  return it->second.get();
}

const H266SPS* H266Parser::GetSPS(int sps_id) const {
  auto it = active_sps_.find(sps_id);
  if (it == active_sps_.end()) {
    DVLOG(1) << "Requested a nonexistent SPS id " << sps_id;
    return nullptr;
  }

  return it->second.get();
}

const H266PPS* H266Parser::GetPPS(int pps_id) const {
  auto it = active_pps_.find(pps_id);
  if (it == active_pps_.end()) {
    DVLOG(1) << "Requested a nonexistent PPS id " << pps_id;
    return nullptr;
  }

  return it->second.get();
}

const H266APS* H266Parser::GetAPS(const H266APS::ParamType& type,
                                  int aps_id) const {
  switch (type) {
    case H266APS::ParamType::kAlf: {
      auto it = active_alf_aps_.find(aps_id);
      if (it == active_alf_aps_.end()) {
        DVLOG(1) << "Requested a nonexistent ALF APS id " << aps_id;
        return nullptr;
      }
      return it->second.get();
    }
    case H266APS::ParamType::kLmcs: {
      auto it = active_lmcs_aps_.find(aps_id);
      if (it == active_lmcs_aps_.end()) {
        DVLOG(1) << "Requested a nonexistent LMCS APS id " << aps_id;
        return nullptr;
      }
      return it->second.get();
    }
    case H266APS::ParamType::kScalingList: {
      auto it = active_scaling_list_aps_.find(aps_id);
      if (it == active_scaling_list_aps_.end()) {
        DVLOG(1) << "Requested a nonexistent ScalingData APS id " << aps_id;
        return nullptr;
      }
      return it->second.get();
    }
  }
}

}  // namespace media
