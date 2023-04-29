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
  kMaxCpbCount = 32,       // 7.4.6.1: hrd_cpb_cnt_minus1: [0, 31].

  // 7.4.3.4.
  // QpBdOffset = 6 * sps_bitdepth_minus8 ->[0, 48]
  // According to equation 57, index into each ChromaQpTable[i] is within
  // [-QpBdOffset, 63], so maximum number of indices is 112.
  kMaxPointsInQpTable = 112,

  kMaxRefPicLists = 64,  // 7.4.3.4 sps_num_ref_pic_lists: [0, 64]

  // 7.4.11 num_ref_entries: [0, kMaxDpbSize + 13]
  kMaxRefEntries = 29,
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

// 7.3.2.22.
struct MEDIA_EXPORT H266SPSRangeExtension {
  // Syntax elements.
  bool sps_extended_precision_flag;
  bool sps_ts_residual_coding_rice_present_in_sh_flag;
  bool sps_rrc_rice_extension_flag;
  bool sps_persistent_rice_adaptation_enabled_flag;
  bool sps_reverse_last_sig_coeff_enabled_flag;
};

// 7.3.5.1
struct MEDIA_EXPORT H266GeneralTimingHrdParameters {
  // Syntax elements.
  uint32_t num_units_in_tick;
  uint32_t time_scale;
  bool general_nal_hrd_params_present_flag;
  bool general_vcl_hrd_params_present_flag;
  bool general_same_pic_timing_in_all_ols_flag;
  bool general_du_hrd_params_present_flag;
  int tick_divisor_minus2;
  int bit_rate_scale;
  int cpb_size_scale;
  int cpb_size_du_scale;
  int hrd_cpb_cnt_minus1;
};

// 7.3.5.3
struct MEDIA_EXPORT H266SublayerHrdParameters {
  H266SublayerHrdParameters();

  // Syntax elements.
  int bit_rate_value_minus1[kMaxCpbCount];
  int cpb_size_value_minus1[kMaxCpbCount];
  int cpb_size_du_value_minus1[kMaxCpbCount];
  int bit_rate_du_value_minus1[kMaxCpbCount];
  bool cbr_flag[kMaxCpbCount];
};

// 7.3.5.2
struct MEDIA_EXPORT H266OlsTimingHrdParameters {
  H266OlsTimingHrdParameters();

  // Syntax elements.
  bool fixed_pic_rate_general_flag[kMaxSubLayers];
  bool fixed_pic_rate_within_cvs_flag[kMaxSubLayers];
  int element_duration_in_tc_minus1[kMaxSubLayers];
  bool low_delay_hrd_flag[kMaxSubLayers];
  H266SublayerHrdParameters nal_sublayer_hrd_parameters[kMaxSubLayers];
  H266SublayerHrdParameters vcl_sublayer_hrd_parameters[kMaxSubLayers];
};

// 7.3.10
struct MEDIA_EXPORT H266RefPicListStruct {
  H266RefPicListStruct();

  // Syntax elements.
  int num_ref_entries;
  bool ltrp_in_header_flag;
  bool inter_layer_ref_pic_flag[kMaxRefEntries];
  bool st_ref_pic_flag[kMaxRefEntries];
  int abs_delta_poc_st[kMaxRefEntries];
  bool strp_entry_sign_flag[kMaxRefEntries];
  int rpls_poc_lsb_lt[kMaxRefEntries];
  int ilrp_idx[kMaxRefEntries];

  // Calculated values.
  int num_ltrp_entries;
  int delta_poc_val_st[kMaxRefEntries];
};

// 7.3.9
struct MEDIA_EXPORT H266RefPicLists {
  H266RefPicLists();

  // Syntax elements
  bool rpl_sps_flag[2];
  int rpl_idx[2];
  H266RefPicListStruct rpl_ref_lists[2];
  int poc_lsb_lt[2][kMaxRefEntries];
  bool delta_poc_msb_cycle_present_flag[2][kMaxRefEntries];
  int delta_poc_msb_cycle_lt[2][kMaxRefEntries];

  // Calculated values.
  int rpls_idx[2];
};

// ITU-T H.274: Video usability information parameters.
// This is defined out of VVC spec.
struct MEDIA_EXPORT H266VUIParameters {
  // Syntax elements.
  bool vui_progressive_source_flag;
  bool vui_interlaced_source_flag;
  bool vui_non_packed_constraint_flag;
  bool vui_non_projected_constraint_flag;
  bool vui_aspect_ratio_info_present_flag;
  bool vui_aspect_ratio_constant_flag;
  int vui_aspect_ratio_idc;
  int vui_sar_width;
  int vui_sar_height;
  bool vui_overscan_info_present_flag;
  bool vui_overscan_appropriate_flag;
  bool vui_colour_description_present_flag;
  int vui_colour_primaries;
  int vui_transfer_characteristics;
  int vui_matrix_coeffs;
  bool vui_full_range_flag;
  bool vui_chroma_loc_info_present_flag;
  int vui_chroma_sample_loc_type_frame;
  int vui_chroma_sample_loc_type_top_field;
  int vui_chroma_sample_loc_type_bottom_field;
};

struct MEDIA_EXPORT H266SPS {
  H266SPS();
  ~H266SPS();

  // 7.4.2.2 nuh_layer_id of non-VCL NAL unit identifies the
  // layer it applies.
  int nuh_layer_id;

  // Syntax elements.
  int sps_seq_parameter_set_id;
  int sps_video_parameter_set_id;
  int sps_max_sublayers_minus1;
  int sps_chroma_format_idc;
  int sps_log2_ctu_size_minus5;
  bool sps_ptl_dpb_hrd_params_present_flag;
  H266ProfileTierLevel profile_tier_level;
  bool sps_gdr_enabled_flag;
  bool sps_ref_pic_resampling_enabled_flag;
  bool sps_res_change_in_clvs_allowed_flag;
  int sps_pic_width_max_in_luma_samples;
  int sps_pic_height_max_in_luma_samples;
  bool sps_conformance_window_flag;
  int sps_conf_win_left_offset;
  int sps_conf_win_right_offset;
  int sps_conf_win_top_offset;
  int sps_conf_win_bottom_offset;
  bool sps_subpic_info_present_flag;
  int sps_num_subpics_minus1;
  bool sps_independent_subpics_flag;
  bool sps_subpic_same_size_flag;
  int sps_subpic_ctu_top_left_x[kMaxSlices];
  int sps_subpic_ctu_top_left_y[kMaxSlices];
  int sps_subpic_width_minus1[kMaxSlices];
  int sps_subpic_height_minus1[kMaxSlices];
  bool sps_subpic_treated_as_pic_flag[kMaxSlices];
  bool sps_loop_filter_across_subpic_enabled_flag[kMaxSlices];
  int sps_subpic_id_len_minus1;
  bool sps_subpic_id_mapping_explicitly_signaled_flag;
  int sps_subpic_id_mapping_present_flag;
  bool sps_subpic_id[kMaxSlices];
  int sps_bitdepth_minus8;
  bool sps_entropy_coding_sync_enabled_flag;
  bool sps_entry_point_offsets_present_flag;
  int sps_log2_max_pic_order_cnt_lsb_minus4;
  bool sps_poc_msb_cycle_flag;
  int sps_poc_msb_cycle_len_minus1;
  int sps_num_extra_ph_bytes;
  bool sps_extra_ph_bit_present_flag[16];
  int sps_num_extra_sh_bytes;
  bool sps_extra_sh_bit_present_flag[16];
  bool sps_sublayer_dpb_params_flag;
  H266DPBParameters dpb_params;
  int sps_log2_min_luma_coding_block_size_minus2;
  bool sps_partition_constraints_override_enabled_flag;
  int sps_log2_diff_min_qt_min_cb_intra_slice_luma;
  int sps_max_mtt_hierarchy_depth_intra_slice_luma;
  int sps_log2_diff_max_bt_min_qt_intra_slice_luma;
  int sps_log2_diff_max_tt_min_qt_intra_slice_luma;
  bool sps_qtbtt_dual_tree_intra_flag;
  int sps_log2_diff_min_qt_min_cb_intra_slice_chroma;
  int sps_max_mtt_hierarchy_depth_intra_slice_chroma;
  int sps_log2_diff_max_bt_min_qt_intra_slice_chroma;
  int sps_log2_diff_max_tt_min_qt_intra_slice_chroma;
  int sps_log2_diff_min_qt_min_cb_inter_slice;
  int sps_max_mtt_hierarchy_depth_inter_slice;
  int sps_log2_diff_max_bt_min_qt_inter_slice;
  int sps_log2_diff_max_tt_min_qt_inter_slice;
  bool sps_max_luma_transform_size_64_flag;
  bool sps_transform_skip_enabled_flag;
  int sps_log2_transform_skip_max_size_minus2;
  bool sps_bdpcm_enabled_flag;
  bool sps_mts_enabled_flag;
  bool sps_explicit_mts_intra_enabled_flag;
  bool sps_explicit_mts_inter_enabled_flag;
  bool sps_lfnst_enabled_flag;
  bool sps_joint_cbcr_enabled_flag;
  bool sps_same_qp_table_for_chroma_flag;
  int sps_qp_table_start_minus26[3];
  int sps_num_points_in_qp_table_minus1[3];
  int sps_delta_qp_in_val_minus1[3][kMaxPointsInQpTable];
  int sps_delta_qp_diff_val[3][kMaxPointsInQpTable];
  bool sps_sao_enabled_flag;
  bool sps_alf_enabled_flag;
  bool sps_ccalf_enabled_flag;
  bool sps_lmcs_enabled_flag;
  bool sps_weighted_pred_flag;
  bool sps_weighted_bipred_flag;
  bool sps_long_term_ref_pics_flag;
  bool sps_inter_layer_prediction_enabled_flag;
  bool sps_idr_rpl_present_flag;
  bool sps_rpl1_same_as_rpl0_flag;
  int sps_num_ref_pic_lists[2];
  H266RefPicListStruct ref_pic_list_struct[2][kMaxRefPicLists];
  bool sps_ref_wraparound_enabled_flag;
  bool sps_temporal_mvp_enabled_flag;
  bool sps_sbtmvp_enabled_flag;
  bool sps_amvr_enabled_flag;
  bool sps_bdof_enabled_flag;
  bool sps_bdof_control_present_in_ph_flag;
  bool sps_smvd_enabled_flag;
  bool sps_dmvr_enabled_flag;
  bool sps_dmvr_control_present_in_ph_flag;
  bool sps_mmvd_enabled_flag;
  bool sps_mmvd_fullpel_only_enabled_flag;
  int sps_six_minus_max_num_merge_cand;
  bool sps_sbt_enabled_flag;
  bool sps_affine_enabled_flag;
  int sps_five_minus_max_num_subblock_merge_cand;
  bool sps_6param_affine_enabled_flag;
  bool sps_affine_amvr_enabled_flag;
  bool sps_affine_prof_enabled_flag;
  bool sps_prof_control_present_in_ph_flag;
  bool sps_bcw_enabled_flag;
  bool sps_ciip_enabled_flag;
  bool sps_gpm_enabled_flag;
  int sps_max_num_merge_cand_minus_max_num_gpm_cand;
  int sps_log2_parallel_merge_level_minus2;
  bool sps_isp_enabled_flag;
  bool sps_mrl_enabled_flag;
  bool sps_mip_enabled_flag;
  bool sps_cclm_enabled_flag;
  bool sps_chroma_horizontal_collocated_flag;
  bool sps_chroma_vertical_collocated_flag;
  bool sps_palette_enabled_flag;
  bool sps_act_enabled_flag;
  int sps_min_qp_prime_ts;
  bool sps_ibc_enabled_flag;
  int sps_six_minus_max_num_ibc_merge_cand;
  bool sps_ladf_enabled_flag;
  int sps_num_ladf_intervals_minus2;
  int sps_ladf_lowest_interval_qp_offset;
  int sps_ladf_qp_offset[4];
  int sps_ladf_delta_threshold_minus1[4];
  bool sps_explicit_scaling_list_enabled_flag;
  bool sps_scaling_matrix_for_lfnst_disabled_flag;
  bool sps_scaling_matrix_for_alternative_colour_space_disabled_flag;
  bool sps_scaling_matrix_designated_colour_space_flag;
  bool sps_dep_quant_enabled_flag;
  bool sps_sign_data_hiding_enabled_flag;
  bool sps_virtual_boundaries_enabled_flag;
  bool sps_virtual_boundaries_present_flag;
  int sps_num_ver_virtual_boundaries;
  int sps_virtual_boundary_pos_x_minus1[3];
  int sps_num_hor_virtual_boundaries;
  int sps_virtual_boundary_pos_y_minus1[3];
  bool sps_timing_hrd_params_present_flag;
  H266GeneralTimingHrdParameters general_timing_hrd_parameters;
  bool sps_sublayer_cpb_params_present_flag;
  H266OlsTimingHrdParameters ols_timing_hrd_parameters;
  bool sps_field_seq_flag;
  bool sps_vui_parameters_present_flag;
  int sps_vui_payload_size_minus1;
  H266VUIParameters vui_parameters;
  bool sps_extension_flag;
  bool sps_range_extension_flag;
  H266SPSRangeExtension sps_range_extension;
  // Skip possible other extensions

  // Calculated values
  int ctb_log2_size_y;
  int ctb_size_y;
  int min_cb_log2_size_y;
  int min_cb_size_y;
  int min_tb_size_y;
  int max_tb_size_y;
  int max_ts_size_y;
  int sub_width_c;
  int sub_height_c;
  int max_pic_order_cnt_lsb;
  int num_extra_ph_bits;
  int num_extra_sh_bits;
  int max_dpb_size;
  int qp_bd_offset;
  // For chrome_qp_table[i][k], k corresponds to key value -QpBdOffset in
  // spec.
  int chroma_qp_table[3][kMaxPointsInQpTable];

  // Helpers to compute frequently-used values. They do not verify that the
  // results are in-spec for the given profile or level.
  gfx::Size GetCodedSize() const;
  gfx::Rect GetVisibleRect() const;
  VideoColorSpace GetColorSpace() const;
  VideoChromaSampling GetChromaSampling() const;
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
  // VPSes/SPSes are owned by the parser class and the memory for
  // their structures is managed here, not by the caller, as they are
  // reused across NALUs.
  //
  // Parse a VPS NALU and save its data in the parser, returning id
  // of the parsed structure in |*vps_id|. To get a pointer
  // to a given VPS structure, use GetVPS(), passing
  // the returned |*vps_id| as parameter.
  Result ParseVPS(int* vps_id);

  // Parse a SPS NALU and save its data in the parser, returning id
  // of the parsed structure in |*sps_id|. To get a pointer
  // to a given SPS structure, use GetSPS(), passing
  // the returned |*sps_id| as parameter.
  Result ParseSPS(const H266NALU& nalu, int* sps_id);

  // Return a pointer to VPS with given |*vps_id| or
  // null if not present.
  const H266VPS* GetVPS(int vps_id) const;

  // Return a pointer to SPS with given |*sps_id| or
  // null if not present.
  const H266SPS* GetSPS(int sps_id) const;

  static VideoCodecProfile ProfileIDCToVideoCodecProfile(int profile_idc);

 private:
  Result ParseProfileTierLevel(bool profile_tier_present,
                               int max_num_sub_layers_minus1,
                               H266ProfileTierLevel* profile_tier_level);
  Result ParseDpbParameters(int max_sublayers_minus1,
                            bool sublayer_info_flag,
                            H266DPBParameters* dpb_parameters);
  Result ParseRefPicListStruct(int rpl_idx,
                               int list_idx,
                               const H266SPS& sps,
                               H266RefPicListStruct* ref_pic_list);

  Result ParseGeneralTimingHrdParameters(
      H266GeneralTimingHrdParameters* general_timing_hrd_parameters);

  Result ParseOlsTimingHrdParameters(
      int first_sublayer,
      int max_sublayer_val,
      const H266GeneralTimingHrdParameters& general_timing_hrd_parameters,
      H266OlsTimingHrdParameters* ols_timing_hrd_parameters);

  Result ParseSublayerHrdParameters(
      int sublayer_id,
      const H266GeneralTimingHrdParameters& general_timing_hrd_parameters,
      H266SublayerHrdParameters* sublayer_hrd_parameters);

  Result ParseVuiParameters(const H266SPS& sps, H266VUIParameters* vui);

  Result ParseVuiPayload(int payload_size,
                         const H266SPS& sps,
                         H266VUIParameters* vui);

  // VPSes/SPSes stored for future reference.
  base::flat_map<int, std::unique_ptr<H266VPS>> active_vps_;
  base::flat_map<int, std::unique_ptr<H266SPS>> active_sps_;
};

}  // namespace media
#endif  // MEDIA_VIDEO_H266_PARSER_H_
