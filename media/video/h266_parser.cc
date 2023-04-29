// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/h266_parser.h"

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
#include "media/video/bit_reader_macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace media {

H266ProfileTierLevel::H266ProfileTierLevel() {
  memset(reinterpret_cast<void*>(this), 0, sizeof(*this));
}

H266VPS::H266VPS() {
  memset(reinterpret_cast<void*>(this), 0, sizeof(*this));
}

H266VPS::~H266VPS() = default;

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

const H266VPS* H266Parser::GetVPS(int vps_id) const {
  auto it = active_vps_.find(vps_id);
  if (it == active_vps_.end()) {
    DVLOG(1) << "Requested a nonexistent VPS id " << vps_id;
    return nullptr;
  }
  return it->second.get();
}

}  // namespace media
