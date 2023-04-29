// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains an implementation of an H266 Annex-B video stream parser
// conforming to 04/2022 version of VVC spec at
// https://www.itu.int/rec/recommendation.asp?lang=en&parent=T-REC-H.266-202204-I

#ifndef MEDIA_VIDEO_H266_PARSER_H_
#define MEDIA_VIDEO_H266_PARSER_H_

#include <stdint.h>
#include <sys/types.h>

#include <vector>

#include "base/containers/flat_map.h"
#include "media/base/media_export.h"
#include "media/base/ranges.h"
#include "media/base/video_color_space.h"
#include "media/base/video_types.h"
#include "media/video/h264_bit_reader.h"
#include "media/video/h264_parser.h"
#include "media/video/h266_nalu_parser.h"

namespace media {

// For explanations of each struct and its members, see H.266 specification
// at http://www.itu.int/rec/T-REC-H.266.
enum {
  kMaxLayers = 128,        // 7.4.3.3 vps_max_layers_minus1: u(6).
  kMaxSubLayers = 7,       // 7.4.3.3 vps_max_sublayers_minus1: [0, 6]
  kMaxPtls = 256,          // 7.4.3.3 vps_num_ptls_minus1: u(8)
  kMaxTotalNumOLSs = 257,  // 7.4.3.3 vps_num_output_layer_sets_minus2: u(8)
  kMaxDpbPicBuffer = 8,    // A.4.2 profile-specific-level limits
  kMaxSubProfiles = 256,   // 7.4.4.1 sub_profiles: u(8)
  kMaxSlices = 600,        // A.4.1 maximum 600 slices for any level.
  kMaxCpbCount = 32,       // 7.4.6.1: hrd_cpb_cnt_minus1: [0, 31]
};

// Section 7.4.4.2
struct MEDIA_EXPORT H266GeneralConstraintsInfo {
  // syntax elements
  bool gci_present_flag;
  bool gci_intra_only_constraint_flag;
  bool gci_all_layers_independent_constraint_flag;
  bool gci_one_au_only_constraint_flag;
  int gci_sixteen_minus_max_bitdepth_constraint_idc;
  int gci_three_minus_max_chroma_format_constraint_idc;
  bool gci_no_mixed_nalu_types_in_pic_constraint_flag;
  bool gci_no_trail_constraint_flag;
  bool gci_no_stsa_constraint_flag;
  bool gci_no_rasl_constraint_flag;
  bool gci_no_radl_constraint_flag;
  bool gci_no_idr_constraint_flag;
  bool gci_no_cra_constraint_flag;
  bool gci_no_gdr_constraint_flag;
  bool gci_no_aps_constraint_flag;
  bool gci_no_idr_rpl_constraint_flag;
  bool gci_one_tile_per_pic_constraint_flag;
  bool gci_pic_header_in_slice_header_constraint_flag;
  bool gci_one_slice_per_pic_constraint_flag;
  bool gci_no_rectangular_slice_constraint_flag;
  bool gci_one_slice_per_subpic_constraint_flag;
  bool gci_no_subpic_info_constraint_flag;
  bool gci_three_minus_max_log2_ctu_size_constraint_idc;
  bool gci_no_partition_constraints_override_constraint_flag;
  bool gci_no_mtt_constraint_flag;
  bool gci_no_qtbtt_dual_tree_intra_constraint_flag;
  bool gci_no_palette_constraint_flag;
  bool gci_no_ibc_constraint_flag;
  bool gci_no_isp_constraint_flag;
  bool gci_no_mrl_constraint_flag;
  bool gci_no_mip_constraint_flag;
  bool gci_no_cclm_constraint_flag;
  bool gci_no_ref_pic_resampling_constraint_flag;
  bool gci_no_res_change_in_clvs_constraint_flag;
  bool gci_no_weighted_prediction_constraint_flag;
  bool gci_no_ref_wraparound_constraint_flag;
  bool gci_no_temporal_mvp_constraint_flag;
  bool gci_no_sbtmvp_constraint_flag;
  bool gci_no_amvr_constraint_flag;
  bool gci_no_bdof_constraint_flag;
  bool gci_no_smvd_constraint_flag;
  bool gci_no_dmvr_constraint_flag;
  bool gci_no_mmvd_constraint_flag;
  bool gci_no_affine_motion_constraint_flag;
  bool gci_no_prof_constraint_flag;
  bool gci_no_bcw_constraint_flag;
  bool gci_no_ciip_constraint_flag;
  bool gci_no_gpm_constraint_flag;
  bool gci_no_luma_transform_size_64_constraint_flag;
  bool gci_no_transform_skip_constraint_flag;
  bool gci_no_bdpcm_constraint_flag;
  bool gci_no_mts_constraint_flag;
  bool gci_no_lfnst_constraint_flag;
  bool gci_no_joint_cbcr_constraint_flag;
  bool gci_no_sbt_constraint_flag;
  bool gci_no_act_constraint_flag;
  bool gci_no_explicit_scaling_list_constraint_flag;
  bool gci_no_dep_quant_constraint_flag;
  bool gci_no_sign_data_hiding_constraint_flag;
  bool gci_no_cu_qp_delta_constraint_flag;
  bool gci_no_chroma_qp_offset_constraint_flag;
  bool gci_no_sao_constraint_flag;
  bool gci_no_alf_constraint_flag;
  bool gci_no_ccalf_constraint_flag;
  bool gci_no_lmcs_constraint_flag;
  bool gci_no_ladf_constraint_flag;
  bool gci_no_virtual_boundaries_constraint_flag;
  int gci_num_additional_bits;
  bool gci_all_rap_pictures_constraint_flag;
  bool gci_no_extended_precision_processing_constraint_flag;
  bool gci_no_ts_residual_coding_rice_constraint_flag;
  bool gci_no_rrc_rice_extension_constraint_flag;
  bool gci_no_persistent_rice_adaptation_constraint_flag;
  bool gci_no_reverse_last_sig_coeff_constraint_flag;
};

// Section 7.4.4.1
struct MEDIA_EXPORT H266ProfileTierLevel {
  H266ProfileTierLevel();
  enum H266ProfileIdc {
    kProfileIdcMain10 = 1,
    kProfileIdcMain12 = 2,
    kProfileIdcMain12Intra = 10,
    kProfileIdcMultilayerMain10 = 17,
    kProfileIdcMain10444 = 33,
    kProfileIdcMain12444 = 34,
    kProfileIdcMain16444 = 35,
    kProfileIdcMain12444Intra = 42,
    kProfileIdcMain16444Intra = 43,
    kProfileIdcMultilayerMain10444 = 49,
    kProfileIdcMain10Still = 65,
    kProfileIdcMain12Still = 66,
    kProfileIdcMain10444Still = 97,
    kProfileIdcMain12444Still = 98,
    kPrifileIdcMain16444Still = 99,
  };

  // Syntax elements.
  int general_profile_idc;
  int general_tier_flag;
  int general_level_idc;
  bool ptl_frame_only_constraint_flag;
  bool ptl_multilayer_enabled_flag;
  H266GeneralConstraintsInfo general_constraints_info;
  bool ptl_sublayer_level_present_flag[kMaxSubLayers - 1];
  int sub_layer_level_idc[kMaxSubLayers - 1];
  int ptl_num_sub_profiles;
  uint32_t general_sub_profiles_idc[kMaxSubProfiles];

  // From Table A.8 - General tier and level limits.
  int MaxLumaPs() const;

  int MaxSlicesPerAu() const;

  int MaxTilesPerAu() const;
};

struct MEDIA_EXPORT H266DPBParameters {
  // Syntax elements.
  int dpb_max_dec_pic_buffering_minus1[kMaxSubLayers];
  int dpb_max_num_reorder_pics[kMaxSubLayers];
  int dpb_max_latency_increase_plus1[kMaxSubLayers];
};

struct MEDIA_EXPORT H266VPS {
  H266VPS();
  ~H266VPS();

  // Syntax elements
  int vps_video_parameter_set_id;
  int vps_max_layers_minus1;
  int vps_max_sublayers_minus1;
  bool vps_default_ptl_dpb_hrd_max_tid_flag;
  bool vps_all_independent_layers_flags;
  int vps_layer_id[kMaxLayers];
  bool vps_independent_layer_flag[kMaxLayers];
  bool vps_max_tid_ref_present_flag[kMaxLayers];
  bool vps_direct_ref_layer_flag[kMaxLayers][kMaxLayers];
  int vps_max_tid_il_ref_pics_plus1[kMaxLayers][kMaxLayers];
  bool vps_each_layer_is_an_ols_flag;
  int vps_ols_mode_idc;
  int vps_num_output_layer_sets_minus2;
  bool vps_ols_output_layer_flag[kMaxLayers][kMaxLayers - 1];
  int vps_num_ptls_minus1;
  bool vps_pt_present_flag[kMaxPtls];
  int vps_ptl_max_tid[kMaxPtls];
  H266ProfileTierLevel profile_tier_level[kMaxPtls];
  int vps_ols_ptl_idx[kMaxTotalNumOLSs];
  int vps_num_dpb_params_minus1;
  bool vps_sublayer_dpb_params_present_flag;
  int vps_dpb_max_tid[kMaxTotalNumOLSs];
  H266DPBParameters dpb_parameters[kMaxTotalNumOLSs];
  int vps_ols_dpb_pic_width[kMaxTotalNumOLSs];
  int vps_ols_dpb_pic_height[kMaxTotalNumOLSs];
  int vps_ols_dpb_chroma_format[kMaxTotalNumOLSs];
  int vps_ols_dpb_bitdepth_minus8[kMaxTotalNumOLSs];
  int vps_ols_dpb_params_idx[kMaxTotalNumOLSs];
  bool vps_timing_hrd_params_present_flag;
  // Skip the VPS HRD timing info and possible extension.

  // Calculated variables
  int num_direct_ref_layers[kMaxLayers];
  int direct_ref_layer_idx[kMaxLayers][kMaxLayers];
  int num_ref_layers[kMaxLayers];
  int reference_layer_idx[kMaxLayers][kMaxLayers];
  bool layer_used_as_ref_layer_flag[kMaxLayers];
  bool dependency_flag[kMaxLayers][kMaxLayers];
  base::flat_map<int, int> general_layer_idx;
  int GetGeneralLayerIdx(int nuh_layer_id) const;
};

// Class to parse an Annex-B H.266 stream.
class MEDIA_EXPORT H266Parser : public H266NaluParser {
 public:
  H266Parser();
  H266Parser(const H266Parser&) = delete;
  H266Parser& operator=(const H266Parser&) = delete;
  ~H266Parser() override;

  // NALU-specific parsing functions.
  // These should be called after AdvanceToNextNALU().
  // VPSes are owned by the parser class and the memory for
  // their structures is managed here, not by the caller, as they are
  // reused across NALUs.
  //
  // Parse a VPS NALU and save its data in the parser, returning id
  // of the parsed structure in |*vps_id|. To get a pointer
  // to a given VPS structure, use GetVPS(), passing
  // the returned |*vps_id| as parameter.
  Result ParseVPS(int* vps_id);

  // Return a pointer to VPS with given |*vps_id| or
  // null if not present.
  const H266VPS* GetVPS(int vps_id) const;

  static VideoCodecProfile ProfileIDCToVideoCodecProfile(int profile_idc);

 private:
  Result ParseProfileTierLevel(bool profile_tier_present,
                               int max_num_sub_layers_minus1,
                               H266ProfileTierLevel* profile_tier_level);
  Result ParseDpbParameters(int max_sublayers_minus1,
                            bool sublayer_info_flag,
                            H266DPBParameters* dpb_parameters);

  // VPSes stored for future reference.
  base::flat_map<int, std::unique_ptr<H266VPS>> active_vps_;
};

}  // namespace media
#endif  // MEDIA_VIDEO_H266_PARSER_H_
