// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains an implementation of an H265 Annex-B video stream parser.

#ifndef MEDIA_PARSERS_H265_PARSER_H_
#define MEDIA_PARSERS_H265_PARSER_H_

#include <stdint.h>
#include <sys/types.h>

#include <array>
#include <variant>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "media/base/media_export.h"
#include "media/base/ranges.h"
#include "media/base/video_color_space.h"
#include "media/base/video_types.h"
#include "media/parsers/h264_bit_reader.h"
#include "media/parsers/h264_parser.h"
#include "media/parsers/h265_nalu_parser.h"

namespace gfx {
struct HdrMetadataCta861_3;
struct HdrMetadataSmpteSt2086;
}  // namespace gfx

namespace media {

// For explanations of each struct and its members, see H.265 specification
// at http://www.itu.int/rec/T-REC-H.265.
enum {
  kMaxLongTermRefPicSets = 32,   // 7.4.3.2.1
  kMaxShortTermRefPicSets = 64,  // 7.4.3.2.1
  kMaxSubLayers = 7,  // 7.4.3.1 & 7.4.3.2.1 [v|s]ps_max_sub_layers_minus1 + 1
  kMaxDpbSize = 16,   // A.4.2
  kMaxRefIdxActive = 15,  // 7.4.7.1 num_ref_idx_l{0,1}_active_minus1 + 1
};

struct MEDIA_EXPORT H265ProfileTierLevel {
  H265ProfileTierLevel();

  enum H265ProfileIdc {
    kProfileIdcMain = 1,
    kProfileIdcMain10 = 2,
    kProfileIdcMainStill = 3,
    kProfileIdcRangeExtensions = 4,
    kProfileIdcHighThroughput = 5,
    kProfileIdcMultiviewMain = 6,
    kProfileIdcScalableMain = 7,
    kProfileIdc3dMain = 8,
    kProfileIdcScreenContentCoding = 9,
    kProfileIdcScalableRangeExtensions = 10,
    kProfileIdcHighThroughputScreenContentCoding = 11,
  };

  // Syntax elements.
  int general_profile_idc = 0;
  int general_level_idc = 0;  // 30x the actual level.
  uint32_t general_profile_compatibility_flags = 0;
  bool general_progressive_source_flag = false;
  bool general_interlaced_source_flag = false;
  bool general_non_packed_constraint_flag = false;
  bool general_frame_only_constraint_flag = false;
  bool general_one_picture_only_constraint_flag = false;

  // From Table A.8 - General tier and level limits.
  int GetMaxLumaPs() const;
  // From A.4.2 - Profile-specific level limits for the video profiles.
  size_t GetDpbMaxPicBuf() const;
};

struct MEDIA_EXPORT H265ScalingListData {
  H265ScalingListData();

  enum {
    kDefaultScalingListSize0Values = 16,  // Table 7-5, all values are 16
    kScalingListSizeId0Count = 16,        // 7.4.5
    kScalingListSizeId1To3Count = 64,     // 7.4.5
    kNumScalingListMatrices = 6,
  };

  // TODO(jkardatzke): Optimize storage of the 32x32 since only indices 0 and 3
  // are actually used. Also change it in the accelerator delegate if that is
  // done.
  // Syntax elements.
  std::array<uint8_t, kNumScalingListMatrices> scaling_list_dc_coef_16x16 = {};
  std::array<uint8_t, kNumScalingListMatrices> scaling_list_dc_coef_32x32 = {};
  std::array<std::array<uint8_t, kScalingListSizeId0Count>,
             kNumScalingListMatrices>
      scaling_list_4x4 = {};
  std::array<std::array<uint8_t, kScalingListSizeId1To3Count>,
             kNumScalingListMatrices>
      scaling_list_8x8 = {};
  std::array<std::array<uint8_t, kScalingListSizeId1To3Count>,
             kNumScalingListMatrices>
      scaling_list_16x16 = {};
  std::array<std::array<uint8_t, kScalingListSizeId1To3Count>,
             kNumScalingListMatrices>
      scaling_list_32x32 = {};

  // The following methods provide a raster scan order view into the matrix
  // represented by the corresponding |scaling_list_NxN[matrix_id]| array (which
  // is expected to be in up-right diagonal scan order per the specification).
  uint8_t GetScalingList4x4EntryInRasterOrder(size_t matrix_id,
                                              size_t raster_idx) const;
  uint8_t GetScalingList8x8EntryInRasterOrder(size_t matrix_id,
                                              size_t raster_idx) const;
  uint8_t GetScalingList16x16EntryInRasterOrder(size_t matrix_id,
                                                size_t raster_idx) const;
  uint8_t GetScalingList32x32EntryInRasterOrder(size_t matrix_id,
                                                size_t raster_idx) const;
};

struct MEDIA_EXPORT H265StRefPicSet {
  // Syntax elements.
  int num_negative_pics = 0;
  int num_positive_pics = 0;
  std::array<int, kMaxShortTermRefPicSets> delta_poc_s0 = {};
  std::array<int, kMaxShortTermRefPicSets> used_by_curr_pic_s0 = {};
  std::array<int, kMaxShortTermRefPicSets> delta_poc_s1 = {};
  std::array<int, kMaxShortTermRefPicSets> used_by_curr_pic_s1 = {};

  // Calculated fields.
  int num_delta_pocs = 0;
  int rps_idx_num_delta_pocs = 0;
};

struct MEDIA_EXPORT H265VUIParameters {
  H265VUIParameters();
  H265VUIParameters(H265VUIParameters&&) noexcept;

  // Syntax elements.
  int sar_width = 0;
  int sar_height = 0;
  bool video_full_range_flag = false;
  bool colour_description_present_flag = false;
  int colour_primaries = 0;
  int transfer_characteristics = 0;
  int matrix_coeffs = 0;
  int def_disp_win_left_offset = 0;
  int def_disp_win_right_offset = 0;
  int def_disp_win_top_offset = 0;
  int def_disp_win_bottom_offset = 0;
  bool bitstream_restriction_flag = false;
  int min_spatial_segmentation_idc = 0;
  int max_bytes_per_pic_denom = 0;
  int max_bits_per_min_cu_denom = 0;
  int log2_max_mv_length_horizontal = 0;
  int log2_max_mv_length_vertical = 0;
};

struct MEDIA_EXPORT H265VPS {
  H265VPS();
  H265VPS(H265VPS&&) noexcept;

  int vps_video_parameter_set_id = 0;
  bool vps_base_layer_internal_flag = false;
  bool vps_base_layer_available_flag = false;
  int vps_max_layers_minus1 = 0;
  int vps_max_sub_layers_minus1 = 0;
  bool vps_temporal_id_nesting_flag = false;
  H265ProfileTierLevel profile_tier_level;
  std::array<int, kMaxSubLayers> vps_max_dec_pic_buffering_minus1 = {};
  std::array<int, kMaxSubLayers> vps_max_num_reorder_pics = {};
  std::array<int, kMaxSubLayers> vps_max_latency_increase_plus1 = {};
  int vps_max_layer_id = 0;
  int vps_num_layer_sets_minus1 = 0;

  // Computed from ScalabilityId
  int aux_alpha_layer_id = 0;

  // skipped the rest
};

struct MEDIA_EXPORT H265SPS {
  H265SPS();
  H265SPS(H265SPS&&) noexcept;

  // Syntax elements.
  int sps_video_parameter_set_id = 0;
  int sps_max_sub_layers_minus1 = 0;
  bool sps_temporal_id_nesting_flag = false;
  H265ProfileTierLevel profile_tier_level;
  int sps_seq_parameter_set_id = 0;
  int chroma_format_idc = 0;
  bool separate_colour_plane_flag = false;
  int pic_width_in_luma_samples = 0;
  int pic_height_in_luma_samples = 0;
  int conf_win_left_offset = 0;
  int conf_win_right_offset = 0;
  int conf_win_top_offset = 0;
  int conf_win_bottom_offset = 0;
  int bit_depth_luma_minus8 = 0;
  int bit_depth_chroma_minus8 = 0;
  int log2_max_pic_order_cnt_lsb_minus4 = 0;
  std::array<int, kMaxSubLayers> sps_max_dec_pic_buffering_minus1 = {};
  std::array<int, kMaxSubLayers> sps_max_num_reorder_pics = {};
  std::array<uint32_t, kMaxSubLayers> sps_max_latency_increase_plus1 = {};
  int log2_min_luma_coding_block_size_minus3 = 0;
  int log2_diff_max_min_luma_coding_block_size = 0;
  int log2_min_luma_transform_block_size_minus2 = 0;
  int log2_diff_max_min_luma_transform_block_size = 0;
  int max_transform_hierarchy_depth_inter = 0;
  int max_transform_hierarchy_depth_intra = 0;
  bool scaling_list_enabled_flag = false;
  bool sps_scaling_list_data_present_flag = false;
  H265ScalingListData scaling_list_data;
  bool amp_enabled_flag = false;
  bool sample_adaptive_offset_enabled_flag = false;
  bool pcm_enabled_flag = false;
  int pcm_sample_bit_depth_luma_minus1 = {};
  int pcm_sample_bit_depth_chroma_minus1 = {};
  int log2_min_pcm_luma_coding_block_size_minus3 = 0;
  int log2_diff_max_min_pcm_luma_coding_block_size = 0;
  bool pcm_loop_filter_disabled_flag = false;
  int num_short_term_ref_pic_sets = 0;
  std::array<H265StRefPicSet, kMaxShortTermRefPicSets> st_ref_pic_set;
  bool long_term_ref_pics_present_flag = false;
  int num_long_term_ref_pics_sps = 0;
  std::array<int, kMaxLongTermRefPicSets> lt_ref_pic_poc_lsb_sps = {};
  std::array<bool, kMaxLongTermRefPicSets> used_by_curr_pic_lt_sps_flag = {};
  bool sps_temporal_mvp_enabled_flag = false;
  bool strong_intra_smoothing_enabled_flag = false;
  bool vui_parameters_present_flag = false;
  H265VUIParameters vui_parameters;

  // Extension extra elements.
  bool sps_extension_present_flag = false;
  bool sps_range_extension_flag = false;
  bool sps_multilayer_extension_flag = false;
  bool sps_3d_extension_flag = false;
  bool sps_scc_extension_flag = false;
  bool transform_skip_rotation_enabled_flag = false;
  bool transform_skip_context_enabled_flag = false;
  bool implicit_rdpcm_enabled_flag = false;
  bool explicit_rdpcm_enabled_flag = false;
  bool extended_precision_processing_flag = false;
  bool intra_smoothing_disabled_flag = false;
  bool high_precision_offsets_enabled_flag = false;
  bool persistent_rice_adaptation_enabled_flag = false;
  bool cabac_bypass_alignment_enabled_flag = false;

  // Calculated fields.
  int chroma_array_type = 0;
  int sub_width_c = 0;
  int sub_height_c = 0;
  size_t max_dpb_size = 0;
  int bit_depth_y = 0;
  int bit_depth_c = 0;
  int max_pic_order_cnt_lsb = 0;
  int ctb_log2_size_y = 0;
  int pic_width_in_ctbs_y = 0;
  int pic_height_in_ctbs_y = 0;
  int pic_size_in_ctbs_y = 0;
  int wp_offset_half_range_y = 0;
  int wp_offset_half_range_c = 0;
  std::array<uint32_t, kMaxSubLayers> sps_max_latency_pictures = {};

  // Helpers to compute frequently-used values. They do not verify that the
  // results are in-spec for the given profile or level.
  gfx::Size GetCodedSize() const;
  gfx::Rect GetVisibleRect() const;
  VideoColorSpace GetColorSpace() const;
  VideoChromaSampling GetChromaSampling() const;
};

struct MEDIA_EXPORT H265PPS {
  H265PPS();
  H265PPS(H265PPS&&) noexcept;

  enum {
    kMaxNumTileColumnWidth = 19,  // From VAAPI.
    kMaxNumTileRowHeight = 21,    // From VAAPI.
  };

  // Syntax elements.
  int pps_pic_parameter_set_id = 0;
  int pps_seq_parameter_set_id = 0;
  bool dependent_slice_segments_enabled_flag = false;
  bool output_flag_present_flag = false;
  int num_extra_slice_header_bits = 0;
  bool sign_data_hiding_enabled_flag = false;
  bool cabac_init_present_flag = false;
  int num_ref_idx_l0_default_active_minus1 = 0;
  int num_ref_idx_l1_default_active_minus1 = 0;
  int init_qp_minus26 = 0;
  bool constrained_intra_pred_flag = false;
  bool transform_skip_enabled_flag = false;
  bool cu_qp_delta_enabled_flag = false;
  int diff_cu_qp_delta_depth = 0;
  int pps_cb_qp_offset = 0;
  int pps_cr_qp_offset = 0;
  bool pps_slice_chroma_qp_offsets_present_flag = false;
  bool weighted_pred_flag = false;
  bool weighted_bipred_flag = false;
  bool transquant_bypass_enabled_flag = false;
  bool tiles_enabled_flag = false;
  bool entropy_coding_sync_enabled_flag = false;
  int num_tile_columns_minus1 = 0;
  int num_tile_rows_minus1 = 0;
  bool uniform_spacing_flag = false;
  std::array<int, kMaxNumTileColumnWidth> column_width_minus1 = {};
  std::array<int, kMaxNumTileRowHeight> row_height_minus1 = {};
  bool loop_filter_across_tiles_enabled_flag = false;
  bool pps_loop_filter_across_slices_enabled_flag = false;
  bool deblocking_filter_control_present_flag = false;
  bool deblocking_filter_override_enabled_flag = false;
  bool pps_deblocking_filter_disabled_flag = false;
  int pps_beta_offset_div2 = 0;
  int pps_tc_offset_div2 = 0;
  bool pps_scaling_list_data_present_flag = false;
  H265ScalingListData scaling_list_data;
  bool lists_modification_present_flag = false;
  int log2_parallel_merge_level_minus2 = 0;
  bool slice_segment_header_extension_present_flag = false;

  // Extension extra elements.
  bool pps_extension_present_flag = false;
  bool pps_range_extension_flag = false;
  bool pps_multilayer_extension_flag = false;
  bool pps_3d_extension_flag = false;
  bool pps_scc_extension_flag = false;
  int log2_max_transform_skip_block_size_minus2 = 0;
  bool cross_component_prediction_enabled_flag = false;
  bool chroma_qp_offset_list_enabled_flag = false;
  int diff_cu_chroma_qp_offset_depth = 0;
  int chroma_qp_offset_list_len_minus1 = 0;
  std::array<int, 6> cb_qp_offset_list = {};
  std::array<int, 6> cr_qp_offset_list = {};
  int log2_sao_offset_scale_luma = 0;
  int log2_sao_offset_scale_chroma = 0;

  // Calculated fields.
  int qp_bd_offset_y = 0;
};

struct MEDIA_EXPORT H265RefPicListsModifications {
  // Syntax elements.
  bool ref_pic_list_modification_flag_l0 = false;
  std::array<int, kMaxRefIdxActive> list_entry_l0 = {};
  bool ref_pic_list_modification_flag_l1 = false;
  std::array<int, kMaxRefIdxActive> list_entry_l1 = {};
};

struct MEDIA_EXPORT H265PredWeightTable {
  H265PredWeightTable();

  // Syntax elements.
  int luma_log2_weight_denom = 0;
  int delta_chroma_log2_weight_denom = 0;
  int chroma_log2_weight_denom = 0;
  std::array<int, kMaxRefIdxActive> delta_luma_weight_l0 = {};
  std::array<int, kMaxRefIdxActive> luma_offset_l0 = {};
  std::array<std::array<int, 2>, kMaxRefIdxActive> delta_chroma_weight_l0 = {};
  std::array<std::array<int, 2>, kMaxRefIdxActive> delta_chroma_offset_l0 = {};
  std::array<int, kMaxRefIdxActive> delta_luma_weight_l1 = {};
  std::array<int, kMaxRefIdxActive> luma_offset_l1 = {};
  std::array<std::array<int, 2>, kMaxRefIdxActive> delta_chroma_weight_l1 = {};
  std::array<std::array<int, 2>, kMaxRefIdxActive> delta_chroma_offset_l1 = {};
};

struct MEDIA_EXPORT H265SliceHeader {
  H265SliceHeader();

  enum {
    kSliceTypeB = 0,  // Table 7-7
    kSliceTypeP = 1,  // Table 7-7
    kSliceTypeI = 2,  // Table 7-7
  };

  int nal_unit_type = 0;                                 // from NAL header
  raw_ptr<const uint8_t, AllowPtrArithmetic> nalu_data;  // from NAL header
  size_t nalu_size = 0;                                  // from NAL header
  int temporal_id = 0;  // calculated from NAL header
  size_t header_size =
      0;  // calculated, not including emulation prevention bytes
  size_t header_emulation_prevention_bytes = 0;

  // Calculated, but needs to be preserved when we copy slice dependent data
  // so put it at the front.
  bool irap_pic = false;

  // Syntax elements.
  bool first_slice_segment_in_pic_flag = false;
  bool no_output_of_prior_pics_flag = false;
  int slice_pic_parameter_set_id = 0;
  bool dependent_slice_segment_flag = false;
  int slice_segment_address = 0;
  // Do not move any of the above fields below or vice-versa, everything after
  // this is copied as a block.
  int slice_type = 0;
  bool pic_output_flag = false;
  int colour_plane_id = 0;
  int slice_pic_order_cnt_lsb = 0;
  bool short_term_ref_pic_set_sps_flag = false;
  H265StRefPicSet st_ref_pic_set;
  // Do not change the order of the following fields up through
  // slice_sao_luma_flag. They are compared as a block.
  int short_term_ref_pic_set_idx = 0;
  int num_long_term_sps = 0;
  int num_long_term_pics = 0;
  std::array<int, kMaxLongTermRefPicSets> poc_lsb_lt = {};
  std::array<bool, kMaxLongTermRefPicSets> used_by_curr_pic_lt = {};
  std::array<bool, kMaxLongTermRefPicSets> delta_poc_msb_present_flag = {};
  std::array<int, kMaxLongTermRefPicSets> delta_poc_msb_cycle_lt = {};
  bool slice_temporal_mvp_enabled_flag = false;
  bool slice_sao_luma_flag = false;
  bool slice_sao_chroma_flag = false;
  bool num_ref_idx_active_override_flag = false;
  int num_ref_idx_l0_active_minus1 = 0;
  int num_ref_idx_l1_active_minus1 = 0;
  H265RefPicListsModifications ref_pic_lists_modification;
  bool mvd_l1_zero_flag = false;
  bool cabac_init_flag = false;
  bool collocated_from_l0_flag = false;
  int collocated_ref_idx = 0;
  H265PredWeightTable pred_weight_table;
  int five_minus_max_num_merge_cand = 0;
  int slice_qp_delta = 0;
  int slice_cb_qp_offset = 0;
  int slice_cr_qp_offset = 0;
  bool slice_deblocking_filter_disabled_flag = false;
  int slice_beta_offset_div2 = 0;
  int slice_tc_offset_div2 = 0;
  bool slice_loop_filter_across_slices_enabled_flag = false;

  // Calculated.
  int curr_rps_idx = 0;
  int num_pic_total_curr = 0;
  // Number of bits st_ref_pic_set takes after removing emulation prevention
  // bytes.
  int st_rps_bits = 0;
  // Number of bits lt_ref_pic_set takes after removing emulation prevention
  // bytes.
  int lt_rps_bits = 0;

  bool IsISlice() const;
  bool IsPSlice() const;
  bool IsBSlice() const;

  const H265StRefPicSet& GetStRefPicSet(const H265SPS* sps) const {
    if (curr_rps_idx == sps->num_short_term_ref_pic_sets)
      return st_ref_pic_set;

    return sps->st_ref_pic_set[curr_rps_idx];
  }
};

struct MEDIA_EXPORT H265SEIAlphaChannelInfo {
  bool alpha_channel_cancel_flag = false;
  int alpha_channel_use_idc = 0;
  int alpha_channel_bit_depth_minus8 = 0;
  int alpha_transparent_value = 0;
  int alpha_opaque_value = 0;
  bool alpha_channel_incr_flag = false;
  bool alpha_channel_clip_flag = false;
  bool alpha_channel_clip_type_flag = false;
};

struct MEDIA_EXPORT H265SEIContentLightLevelInfo {
  uint16_t max_content_light_level;
  uint16_t max_picture_average_light_level;

  gfx::HdrMetadataCta861_3 ToGfx() const;
};

struct MEDIA_EXPORT H265SEIMasteringDisplayInfo {
  enum {
    kNumDisplayPrimaries = 3,
    kDisplayPrimaryComponents = 2,
  };

  std::array<std::array<uint16_t, kDisplayPrimaryComponents>,
             kNumDisplayPrimaries>
      display_primaries;
  std::array<uint16_t, 2> white_points;
  uint32_t max_luminance;
  uint32_t min_luminance;

  gfx::HdrMetadataSmpteSt2086 ToGfx() const;
};

using H265SEIMessage = std::variant<std::monostate,
                                    H265SEIAlphaChannelInfo,
                                    H265SEIContentLightLevelInfo,
                                    H265SEIMasteringDisplayInfo>;

struct MEDIA_EXPORT H265SEI {
  H265SEI();
  ~H265SEI();

  std::vector<H265SEIMessage> msgs;
};

// Class to parse an Annex-B H.265 stream.
class MEDIA_EXPORT H265Parser : public H265NaluParser {
 public:
  H265Parser();

  H265Parser(const H265Parser&) = delete;
  H265Parser& operator=(const H265Parser&) = delete;

  ~H265Parser() override;

  // NALU-specific parsing functions.
  // These should be called after AdvanceToNextNALU().

  // VPSes, SPSes and PPSes are owned by the parser class and the memory for
  // their structures is managed here, not by the caller, as they are
  // reused across NALUs.
  //
  // Parse an VPS/SPS/PPS NALU and save their data in the parser, returning id
  // of the parsed structure in |*pps_id|/|*sps_id|/|*vps_id|. To get a pointer
  // to a given VPS/SPS/PPS structure, use GetVPS()/GetSPS()/GetPPS(), passing
  // the returned |*vps_id|/|*sps_id|/|*pps_id| as parameter.
  Result ParseVPS(int* vps_id);
  Result ParseSPS(int* sps_id);
  Result ParsePPS(const H265NALU& nalu, int* pps_id);

  // Return a pointer to VPS/SPS/PPS with given |*vps_id|/|sps_id|/|pps_id| or
  // null if not present.
  const H265VPS* GetVPS(int vps_id) const;
  const H265SPS* GetSPS(int sps_id) const;
  const H265PPS* GetPPS(int pps_id) const;

  // Slice headers are not used across NALUs by the parser and can be discarded
  // after current NALU, so the parser does not store them, nor does it manage
  // their memory. The caller has to provide and manage it instead.

  // Parse a slice header, returning it in |*shdr|. |*nalu| must be set to
  // the NALU returned from AdvanceToNextNALU() and corresponding to |*shdr|.
  // |prior_shdr| should be the last parsed header in decoding order for
  // handling dependent slice segments. If |prior_shdr| is null and this is a
  // dependent slice segment, an error will be returned.
  Result ParseSliceHeader(const H265NALU& nalu,
                          H265SliceHeader* shdr,
                          H265SliceHeader* prior_shdr);

  // Parse a slice header and return the associated picture parameter set ID.
  Result ParseSliceHeaderForPictureParameterSets(const H265NALU& nalu,
                                                 int* pps_id);

  // Parse a SEI, returning it in |*sei|, provided and managed by the caller.
  Result ParseSEI(H265SEI* sei);

  static VideoCodecProfile ProfileIDCToVideoCodecProfile(int profile_idc);

 private:
  Result ParseProfileTierLevel(bool profile_present,
                               int max_num_sub_layers_minus1,
                               H265ProfileTierLevel* profile_tier_level);
  Result ParseScalingListData(H265ScalingListData* scaling_list_data);
  Result ParseStRefPicSet(int st_rps_idx,
                          const H265SPS& sps,
                          H265StRefPicSet* st_ref_pic_set,
                          bool is_slice_hdr = false);
  Result ParseVuiParameters(const H265SPS& sps, H265VUIParameters* vui);
  Result ParseAndIgnoreHrdParameters(bool common_inf_present_flag,
                                     int max_num_sub_layers_minus1);
  Result ParseAndIgnoreSubLayerHrdParameters(
      int cpb_cnt,
      bool sub_pic_hrd_params_present_flag);
  Result ParseRefPicListsModifications(const H265SliceHeader& shdr,
                                       H265RefPicListsModifications* rpl_mod);
  Result ParsePredWeightTable(const H265SPS& sps,
                              const H265SliceHeader& shdr,
                              H265PredWeightTable* pred_weight_table);

  // VPSes, PPSes and SPSes stored for future reference.
  base::flat_map<int, std::unique_ptr<H265VPS>> active_vps_;
  base::flat_map<int, std::unique_ptr<H265SPS>> active_sps_;
  base::flat_map<int, std::unique_ptr<H265PPS>> active_pps_;
};

}  // namespace media

#endif  // MEDIA_PARSERS_H265_PARSER_H_
