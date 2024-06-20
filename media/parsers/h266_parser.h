// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains an implementation of an H266 Annex-B video stream parser
// conforming to 04/2022 version of VVC spec at
// https://www.itu.int/rec/recommendation.asp?lang=en&parent=T-REC-H.266-202204-I

#ifndef MEDIA_PARSERS_H266_PARSER_H_
#define MEDIA_PARSERS_H266_PARSER_H_

#include <stdint.h>
#include <sys/types.h>

#include <optional>
#include <variant>
#include <vector>

#include "base/containers/flat_map.h"
#include "media/base/media_export.h"
#include "media/base/ranges.h"
#include "media/base/video_color_space.h"
#include "media/base/video_types.h"
#include "media/parsers/h264_bit_reader.h"
#include "media/parsers/h264_parser.h"
#include "media/parsers/h266_nalu_parser.h"

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

  kMaxTiles = 990,       // A.4.1 Table A.1
  kMaxTileRows = 990,    // 990 tile rows and 1 column.
  kMaxTileColumns = 30,  // A.4.1  Table A.1
  kNumAlfFilters = 25,   // 7.3.18 Adaptive loop filter semantics
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
  // Be noted below three members actually have their
  // second dimension indexed within 0 to associated
  // ref_pic_list_struct's NumLtrpEntries - 1.
  int poc_lsb_lt[2][kMaxRefEntries];
  bool delta_poc_msb_cycle_present_flag[2][kMaxRefEntries];
  int delta_poc_msb_cycle_lt[2][kMaxRefEntries];

  // Calculated values.
  int rpls_idx[2];
  // The second dimension is indexed within 0 to associated
  // ref_pic_list's NumLtrpEntries - 1.
  int unpacked_delta_poc_msb_cycle_lt[2][kMaxRefEntries];
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

struct MEDIA_EXPORT H266PPS {
  H266PPS();

  // Syntax elements.
  int pps_pic_parameter_set_id;
  int pps_seq_parameter_set_id;
  bool pps_mixed_nalu_types_in_pic_flag;
  int pps_pic_width_in_luma_samples;
  int pps_pic_height_in_luma_samples;
  bool pps_conformance_window_flag;
  int pps_conf_win_left_offset;
  int pps_conf_win_right_offset;
  int pps_conf_win_top_offset;
  int pps_conf_win_bottom_offset;
  bool pps_scaling_window_explicit_signaling_flag;
  int pps_scaling_win_left_offset;
  int pps_scaling_win_right_offset;
  int pps_scaling_win_top_offset;
  int pps_scaling_win_bottom_offset;
  bool pps_output_flag_present_flag;
  bool pps_no_pic_partition_flag;
  bool pps_subpic_id_mapping_present_flag;
  int pps_num_subpics_minus1;
  int pps_subpic_id_len_minus1;
  int pps_subpic_id[kMaxSlices];
  int pps_log2_ctu_size_minus5;
  int pps_num_exp_tile_columns_minus1;
  int pps_num_exp_tile_rows_minus1;
  int pps_tile_column_width_minus1[kMaxTileColumns];
  int pps_tile_row_height_minus1[kMaxTileRows];
  bool pps_loop_filter_across_tiles_enabled_flag;
  bool pps_rect_slice_flag;
  bool pps_single_slice_per_subpic_flag;
  int pps_num_slices_in_pic_minus1;
  bool pps_tile_idx_delta_present_flag;
  int pps_slice_width_in_tiles_minus1[kMaxSlices];
  int pps_slice_height_in_tiles_minus1[kMaxSlices];
  int pps_num_exp_slices_in_tile[kMaxSlices];
  int pps_exp_slice_height_in_ctus_minus1[kMaxSlices][kMaxTileRows];
  int pps_tile_idx_delta_val[kMaxSlices];
  bool pps_loop_filter_across_slices_enabled_flag;
  bool pps_cabac_init_present_flag;
  int pps_num_ref_idx_default_active_minus1[2];
  bool pps_rpl1_idx_present_flag;
  bool pps_weighted_pred_flag;
  bool pps_weighted_bipred_flag;
  bool pps_ref_wraparound_enabled_flag;
  int pps_pic_width_minus_wraparound_offset;
  int pps_init_qp_minus26;
  bool pps_cu_qp_delta_enabled_flag;
  bool pps_chroma_tool_offsets_present_flag;
  int pps_cb_qp_offset;
  int pps_cr_qp_offset;
  bool pps_joint_cbcr_qp_offset_present_flag;
  int pps_joint_cbcr_qp_offset_value;
  bool pps_slice_chroma_qp_offsets_present_flag;
  int pps_cu_chroma_qp_offset_list_enabled_flag;
  int pps_chroma_qp_offset_list_len_minus1;
  int pps_cb_qp_offset_list[6];
  int pps_cr_qp_offset_list[6];
  int pps_joint_cbcr_qp_offset_list[6];
  bool pps_deblocking_filter_control_present_flag;
  bool pps_deblocking_filter_override_enabled_flag;
  bool pps_deblocking_filter_disabled_flag;
  bool pps_dbf_info_in_ph_flag;
  int pps_luma_beta_offset_div2;
  int pps_luma_tc_offset_div2;
  int pps_cb_beta_offset_div2;
  int pps_cb_tc_offset_div2;
  int pps_cr_beta_offset_div2;
  int pps_cr_tc_offset_div2;
  bool pps_rpl_info_in_ph_flag;
  bool pps_sao_info_in_ph_flag;
  bool pps_alf_info_in_ph_flag;
  bool pps_wp_info_in_ph_flag;
  bool pps_qp_delta_info_in_ph_flag;
  bool pps_picture_header_extension_present_flag;
  bool pps_slice_header_extension_present_flag;
  bool pps_extension_flag;
  // Skip possible extensions

  // Calculated values
  int pic_width_in_ctbs_y;
  int pic_height_in_ctbs_y;
  int pic_size_in_ctbs_y;
  int pic_width_in_min_cbs_y;
  int pic_height_in_min_cbs_y;
  int pic_size_in_min_cbs_y;
  int pic_size_in_samples_y;
  int pic_width_in_samples_c;
  int pic_height_in_samples_c;
  int num_tiles_in_pic;
  int num_tile_columns;
  int num_tile_rows;
  int slice_height_in_ctus[kMaxSlices];
  int num_slices_in_subpic[kMaxSlices];
};

// 7.3.2.18
struct MEDIA_EXPORT H266AlfData {
  H266AlfData();

  // Syntax element
  bool alf_luma_filter_signal_flag;
  bool alf_chroma_filter_signal_flag;
  bool alf_cc_cb_filter_signal_flag;
  bool alf_cc_cr_filter_signal_flag;
  bool alf_luma_clip_flag;
  int alf_luma_num_filters_signalled_minus1;
  int alf_luma_coeff_delta_idx[25];
  int alf_luma_coeff_abs[25][12];
  bool alf_luma_coeff_sign[25][12];
  int alf_luma_clip_idx[25][12];
  bool alf_chroma_clip_flag;
  int alf_chroma_num_alt_filters_minus1;
  int alf_chroma_coeff_abs[8][6];
  bool alf_chroma_coeff_sign[8][6];
  int alf_chroma_clip_idx[8][6];
  int alf_cc_cb_filters_signalled_minus1;
  int alf_cc_cb_mapped_coeff_abs[4][7];
  bool alf_cc_cb_coeff_sign[4][7];
  int alf_cc_cr_filters_signalled_minus1;
  int alf_cc_cr_mapped_coeff_abs[4][7];
  bool alf_cc_cr_coeff_sign[4][7];
};

// 7.3.2.19
struct MEDIA_EXPORT H266LmcsData {
  H266LmcsData();

  // Syntax elements.
  int lmcs_min_bin_idx;
  int lmcs_delta_max_bin_idx;
  int lmcs_delta_cw_prec_minus1;
  int lmcs_delta_abs_cw[16];
  bool lmcs_delta_sign_cw_flag[16];
  int lmcs_delta_abs_crs;
  bool lmcs_delta_sign_crs_flag;
};

// 7.3.2.20
struct MEDIA_EXPORT H266ScalingListData {
  H266ScalingListData();

  // Syntax elements.
  bool scaling_list_copy_mode_flag[28];
  bool scaling_list_pred_mode_flag[28];
  int scaling_list_pred_id_delta[28];
  // dc coef for id in [14, 27]
  int scaling_list_dc_coef[14];
  int scaling_list_delta_coef[28][64];

  // Calculated values.
  int scaling_matrix_dc_rec[14];
  int scaling_list_2x2[2][4];
  int scaling_list_4x4[6][16];
  int scaling_list_8x8[20][64];
  int scaling_matrix_rec_2x2[2][2][2];
  int scaling_matrix_rec_4x4[6][4][4];
  int scaling_matrix_rec_8x8[20][8][8];
};

// 7.3.2.6 Adaptation parameter set
struct MEDIA_EXPORT H266APS {
  H266APS(int aps_type);
  ~H266APS();

  // Table 6.
  enum ParamType {
    kAlf = 0,
    kLmcs = 1,
    kScalingList = 2,
  };

  // Use to track if current APS is from PREFIX_APS or SUFFIX_APS,
  // and the layer id this APS is supposed to apply to.
  int nal_unit_type;
  int nuh_layer_id;

  // Syntax elements
  int aps_params_type;
  int aps_adaptation_parameter_set_id;
  bool aps_chroma_present_flag;
  bool aps_extension_flag;

  std::variant<H266AlfData, H266LmcsData, H266ScalingListData> data;
};

// 7.3.8. PredWeightTable could exist in picture header or slice header.
struct MEDIA_EXPORT H266PredWeightTable {
  H266PredWeightTable();

  // Syntax elements.
  int luma_log2_weight_denom;
  int delta_chroma_log2_weight_denom;
  int num_l0_weights;
  bool luma_weight_l0_flag[15];
  bool chroma_weight_l0_flag[15];
  int delta_luma_weight_l0[15];
  int luma_offset_l0[15];
  int delta_chroma_weight_l0[2][15];
  int delta_chroma_offset_l0[2][15];
  int num_l1_weights;
  bool luma_weight_l1_flag[15];
  bool chroma_weight_l1_flag[15];
  int delta_luma_weight_l1[15];
  int luma_offset_l1[15];
  int delta_chroma_weight_l1[2][15];
  int delta_chroma_offset_l1[2][15];

  // Calculated values.
  int chroma_log2_weight_denom;
  int num_weights_l0;
  int num_weights_l1;
};

// 7.3.2.8
struct MEDIA_EXPORT H266PictureHeader {
  H266PictureHeader();

  // Implies whether the picture header structure
  // is within PH_NUT or slice header.
  int nal_unit_type;

  // Syntax elements.
  bool ph_gdr_or_irap_pic_flag;
  bool ph_non_ref_pic_flag;
  bool ph_gdr_pic_flag;
  bool ph_inter_slice_allowed_flag;
  bool ph_intra_slice_allowed_flag;
  int ph_pic_parameter_set_id;
  int ph_pic_order_cnt_lsb;
  int ph_recovery_poc_cnt;
  bool ph_poc_msb_cycle_present_flag;
  int ph_poc_msb_cycle_val;
  bool ph_alf_enabled_flag;
  int ph_num_alf_aps_ids_luma;
  int ph_alf_aps_id_luma[7];
  bool ph_alf_cb_enabled_flag;
  bool ph_alf_cr_enabled_flag;
  int ph_alf_aps_id_chroma;
  bool ph_alf_cc_cb_enabled_flag;
  int ph_alf_cc_cb_aps_id;
  bool ph_alf_cc_cr_enabled_flag;
  int ph_alf_cc_cr_aps_id;
  bool ph_lmcs_enabled_flag;
  int ph_lmcs_aps_id;
  bool ph_chroma_residual_scale_flag;
  bool ph_explicit_scaling_list_enabled_flag;
  int ph_scaling_list_aps_id;
  bool ph_virtual_boundaries_present_flag;
  int ph_num_ver_virtual_boundaries;
  int ph_virtual_boundary_pos_x_minus1[3];
  int ph_num_hor_virtual_boundaries;
  int ph_virtual_boundary_pos_y_minus1[3];
  bool ph_pic_output_flag;
  H266RefPicLists ref_pic_lists;
  bool ph_partition_constraints_override_flag;
  int ph_log2_diff_min_qt_min_cb_intra_slice_luma;
  int ph_max_mtt_hierarchy_depth_intra_slice_luma;
  int ph_log2_diff_max_bt_min_qt_intra_slice_luma;
  int ph_log2_diff_max_tt_min_qt_intra_slice_luma;
  int ph_log2_diff_min_qt_min_cb_intra_slice_chroma;
  int ph_max_mtt_hierarchy_depth_intra_slice_chroma;
  int ph_log2_diff_max_bt_min_qt_intra_slice_chroma;
  int ph_log2_diff_max_tt_min_qt_intra_slice_chroma;
  int ph_cu_qp_delta_subdiv_intra_slice;
  int ph_cu_chroma_qp_offset_subdiv_intra_slice;
  int ph_log2_diff_min_qt_min_cb_inter_slice;
  int ph_max_mtt_hierarchy_depth_inter_slice;
  int ph_log2_diff_max_bt_min_qt_inter_slice;
  int ph_log2_diff_max_tt_min_qt_inter_slice;
  int ph_cu_qp_delta_subdiv_inter_slice;
  int ph_cu_chroma_qp_offset_subdiv_inter_slice;
  bool ph_temporal_mvp_enabled_flag;
  bool ph_collocated_from_l0_flag;
  int ph_collocated_ref_idx;
  bool ph_mmvd_fullpel_only_flag;
  bool ph_mvd_l1_zero_flag;
  bool ph_bdof_disabled_flag;
  bool ph_dmvr_disabled_flag;
  bool ph_prof_disabled_flag;
  H266PredWeightTable pred_weight_table;
  int ph_qp_delta;
  bool ph_joint_cbcr_sign_flag;
  bool ph_sao_luma_enabled_flag;
  bool ph_sao_chroma_enabled_flag;
  bool ph_deblocking_params_present_flag;
  bool ph_deblocking_filter_disabled_flag;
  int ph_luma_beta_offset_div2;
  int ph_luma_tc_offset_div2;
  int ph_cb_beta_offset_div2;
  int ph_cb_tc_offset_div2;
  int ph_cr_beta_offset_div2;
  int ph_cr_tc_offset_div2;
  int ph_extension_length;
  // Skip any possible extension bytes.

  // Calculated values.
  bool virtual_boundaries_present_flag;
  int num_ver_virtual_boundaries;
  int num_hor_virtual_boundaries;
  int min_qt_log2_size_intra_y;
  int min_qt_log2_size_intra_c;
  int min_qt_log2_size_inter_y;
  int slice_qp_y;
};

// 7.3.7
struct MEDIA_EXPORT H266SliceHeader {
  H266SliceHeader();
  ~H266SliceHeader();

  // Table 9
  enum {
    kSliceTypeB = 0,
    kSliceTypeP = 1,
    kSliceTypeI = 2,
  };

  // These are from NAL headers
  int nal_unit_type;
  int nuh_layer_id;
  int temporal_id;
  const uint8_t* nalu_data;
  size_t nalu_size;
  size_t header_size;  // calculated, not including emulation prevention bytes
  size_t header_emulation_prevention_bytes;

  // Syntax elements.
  bool sh_picture_header_in_slice_header_flag;
  H266PictureHeader picture_header;
  int sh_subpic_id;
  int sh_slice_address;
  // 16-bit sh_extra_bit to be skipped
  int sh_num_tiles_in_slice_minus1;
  int sh_slice_type;
  bool sh_no_output_of_prior_pics_flag;
  bool sh_alf_enabled_flag;
  int sh_num_alf_aps_ids_luma;
  int sh_alf_aps_id_luma[7];
  bool sh_alf_cb_enabled_flag;
  bool sh_alf_cr_enabled_flag;
  int sh_alf_aps_id_chroma;
  bool sh_alf_cc_cb_enabled_flag;
  int sh_alf_cc_cb_aps_id;
  bool sh_alf_cc_cr_enabled_flag;
  int sh_alf_cc_cr_aps_id;
  bool sh_lmcs_used_flag;
  bool sh_explicit_scaling_list_used_flag;
  H266RefPicLists ref_pic_lists;
  bool sh_num_ref_idx_active_override_flag;
  int sh_num_ref_idx_active_minus1[2];
  bool sh_cabac_init_flag;
  bool sh_collocated_from_l0_flag;
  int sh_collocated_ref_idx;
  H266PredWeightTable sh_pred_weight_table;
  int sh_qp_delta;
  int sh_cb_qp_offset;
  int sh_cr_qp_offset;
  int sh_joint_cbcr_qp_offset;
  bool sh_cu_chroma_qp_offset_enabled_flag;
  bool sh_sao_luma_used_flag;
  bool sh_sao_chroma_used_flag;
  bool sh_deblocking_params_present_flag;
  bool sh_deblocking_filter_disabled_flag;
  int sh_luma_beta_offset_div2;
  int sh_luma_tc_offset_div2;
  int sh_cb_beta_offset_div2;
  int sh_cb_tc_offset_div2;
  int sh_cr_beta_offset_div2;
  int sh_cr_tc_offset_div2;
  bool sh_dep_quant_used_flag;
  bool sh_sign_data_hiding_used_flag;
  bool sh_ts_residual_coding_disabled_flag;
  int sh_ts_residual_coding_rice_idx_minus1;
  bool sh_reverse_last_sig_coeff_flag;
  int sh_slice_header_extension_length;
  // Skip possible extension bytes.
  int sh_entry_offset_len_minus1;
  std::vector<int> sh_entry_point_offset_minus1;

  // Calculated values
  int num_ref_idx_active[2];
  int slice_qp_y;
  int num_ctus_in_curr_slice;

  // 8.1.1 This only applies to IRAP or GDR picture, and
  // will be configured by parser depending on the slice
  // type and whether it is the first picture of a layer
  // that follows an EOS NAL unit.
  bool no_output_before_recovery_flag;
  // 8.1.1 Below two are not parsed from bitstream. They
  // are knobs provided to decoder to explicitly control
  // whether we handle CRA and GDR as CLVS start, and
  std::optional<bool> handle_cra_as_clvs_start_flag;
  std::optional<bool> handle_gdr_as_clvs_start_flag;
  // Actual used flags by parser.
  bool cra_as_clvs_start_flag;
  bool gdr_as_clvs_start_flag;

  bool IsISlice() const;
  bool IsPSlice() const;
  bool IsBSlice() const;
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

  // Parse a PPS NALU and save its data in the parser, returning id
  // of the parsed structure in |*pps_id|. To get a pointer
  // to a given PPS structure, use GetPPS(), passing
  // the returned |*pps_id| as parameter.
  Result ParsePPS(const H266NALU& nalu, int* pps_id);

  // Parse an APS NALU and save its data in the parser, returning id
  // of the parsed structure in |*aps_id| and APS type in |*type|. To get a
  // pointer to a given APS structure, use GetAPS(), passing the returned
  // |*aps_id| and |*type| as parameter.
  Result ParseAPS(const H266NALU& nalu, int* aps_id, H266APS::ParamType* type);

  // Parse a picture header(PH) NALU and return the parsed structure
  // in |*ph|.
  Result ParsePHNut(const H266NALU& nalu, H266PictureHeader* ph);

  // Slice headers are not used across NALUs by the parser and can be discarded
  // after current NALU, so the parser does not store them, nor does it manage
  // their memory. The caller has to provide and manage it instead.
  // Parse a slice header, returning it in |*shdr|. |nalu| must be set to
  // the NALU returned from AdvanceToNextNALU() and corresponding to |*shdr|.
  // |first_picture| indicates if the picture current slice belongs to is the
  // first picture of a layer in decoding order, or the first picture of a layer
  // following EOS_NUT.
  Result ParseSliceHeader(const H266NALU& nalu,
                          bool first_picture,
                          const H266PictureHeader* ph,
                          H266SliceHeader* shdr);

  // Return a pointer to VPS with given |vps_id| or
  // null if not present.
  const H266VPS* GetVPS(int vps_id) const;

  // Return a pointer to SPS with given |sps_id| or null if not present.
  const H266SPS* GetSPS(int sps_id) const;

  // Return a pointer to PPS with given |pps_id| or null if not present.
  const H266PPS* GetPPS(int pps_id) const;

  // Return a pointer to APS with given |aps_id| and |type| or null if not
  // present.
  const H266APS* GetAPS(const H266APS::ParamType& type, int aps_id) const;

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

  Result ParseRefPicLists(const H266SPS& sps,
                          const H266PPS& pps,
                          H266RefPicLists* ref_pic_lists);

  Result ParsePredWeightTable(const H266SPS& sps,
                              const H266PPS& pps,
                              const H266RefPicLists& ref_pic_lists,
                              int num_ref_idx_active[2],
                              H266PredWeightTable* pred_weight_table);

  // Called when picture header structure is in slice header.
  Result ParsePHInSlice(const H266NALU& nalu, H266PictureHeader* ph);

  // Shared picture header structure parser for PH structure in PH_NUT and slice
  // header.
  Result ParsePictureHeaderStructure(const H266NALU& nalu,
                                     H266PictureHeader* ph);

  // VPSes/SPSes/PPSes stored for future reference.
  base::flat_map<int, std::unique_ptr<H266VPS>> active_vps_;
  base::flat_map<int, std::unique_ptr<H266SPS>> active_sps_;
  base::flat_map<int, std::unique_ptr<H266PPS>> active_pps_;
  // APS of same aps_params_type share the same value space of APS ID regardless
  // of their NALU type or nuh_layer_id.
  base::flat_map<int, std::unique_ptr<H266APS>> active_alf_aps_;
  base::flat_map<int, std::unique_ptr<H266APS>> active_lmcs_aps_;
  base::flat_map<int, std::unique_ptr<H266APS>> active_scaling_list_aps_;
};

}  // namespace media
#endif  // MEDIA_PARSERS_H266_PARSER_H_
