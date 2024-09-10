// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains an implementation of an H265 Annex-B video stream parser.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef MEDIA_PARSERS_H265_PARSER_H_
#define MEDIA_PARSERS_H265_PARSER_H_

#include <stdint.h>
#include <sys/types.h>

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
  int general_profile_idc;
  int general_level_idc;  // 30x the actual level.
  uint32_t general_profile_compatibility_flags;
  bool general_progressive_source_flag;
  bool general_interlaced_source_flag;
  bool general_non_packed_constraint_flag;
  bool general_frame_only_constraint_flag;
  bool general_one_picture_only_constraint_flag;

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
  uint8_t scaling_list_dc_coef_16x16[kNumScalingListMatrices];
  uint8_t scaling_list_dc_coef_32x32[kNumScalingListMatrices];
  uint8_t scaling_list_4x4[kNumScalingListMatrices][kScalingListSizeId0Count];
  uint8_t scaling_list_8x8[kNumScalingListMatrices]
                          [kScalingListSizeId1To3Count];
  uint8_t scaling_list_16x16[kNumScalingListMatrices]
                            [kScalingListSizeId1To3Count];
  uint8_t scaling_list_32x32[kNumScalingListMatrices]
                            [kScalingListSizeId1To3Count];

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
  H265StRefPicSet();

  // Syntax elements.
  int num_negative_pics;
  int num_positive_pics;
  int delta_poc_s0[kMaxShortTermRefPicSets];
  int used_by_curr_pic_s0[kMaxShortTermRefPicSets];
  int delta_poc_s1[kMaxShortTermRefPicSets];
  int used_by_curr_pic_s1[kMaxShortTermRefPicSets];

  // Calculated fields.
  int num_delta_pocs;
  int rps_idx_num_delta_pocs;
};

struct MEDIA_EXPORT H265VUIParameters {
  H265VUIParameters();

  // Syntax elements.
  int sar_width;
  int sar_height;
  bool video_full_range_flag;
  bool colour_description_present_flag;
  int colour_primaries;
  int transfer_characteristics;
  int matrix_coeffs;
  int def_disp_win_left_offset;
  int def_disp_win_right_offset;
  int def_disp_win_top_offset;
  int def_disp_win_bottom_offset;
  bool bitstream_restriction_flag;
  int min_spatial_segmentation_idc;
  int max_bytes_per_pic_denom;
  int max_bits_per_min_cu_denom;
  int log2_max_mv_length_horizontal;
  int log2_max_mv_length_vertical;
};

struct MEDIA_EXPORT H265VPS {
  H265VPS();

  int vps_video_parameter_set_id;
  bool vps_base_layer_internal_flag;
  bool vps_base_layer_available_flag;
  int vps_max_layers_minus1;
  int vps_max_sub_layers_minus1;
  bool vps_temporal_id_nesting_flag;
  H265ProfileTierLevel profile_tier_level;
  int vps_max_dec_pic_buffering_minus1[kMaxSubLayers];
  int vps_max_num_reorder_pics[kMaxSubLayers];
  int vps_max_latency_increase_plus1[kMaxSubLayers];
  int vps_max_layer_id;
  int vps_num_layer_sets_minus1;

  // Computed from ScalabilityId
  int aux_alpha_layer_id;

  // skipped the rest
};

struct MEDIA_EXPORT H265SPS {
  H265SPS();

  // Syntax elements.
  int sps_video_parameter_set_id;
  int sps_max_sub_layers_minus1;
  bool sps_temporal_id_nesting_flag;
  H265ProfileTierLevel profile_tier_level;
  int sps_seq_parameter_set_id;
  int chroma_format_idc;
  bool separate_colour_plane_flag;
  int pic_width_in_luma_samples;
  int pic_height_in_luma_samples;
  int conf_win_left_offset;
  int conf_win_right_offset;
  int conf_win_top_offset;
  int conf_win_bottom_offset;
  int bit_depth_luma_minus8;
  int bit_depth_chroma_minus8;
  int log2_max_pic_order_cnt_lsb_minus4;
  int sps_max_dec_pic_buffering_minus1[kMaxSubLayers];
  int sps_max_num_reorder_pics[kMaxSubLayers];
  uint32_t sps_max_latency_increase_plus1[kMaxSubLayers];
  int log2_min_luma_coding_block_size_minus3;
  int log2_diff_max_min_luma_coding_block_size;
  int log2_min_luma_transform_block_size_minus2;
  int log2_diff_max_min_luma_transform_block_size;
  int max_transform_hierarchy_depth_inter;
  int max_transform_hierarchy_depth_intra;
  bool scaling_list_enabled_flag;
  bool sps_scaling_list_data_present_flag;
  H265ScalingListData scaling_list_data;
  bool amp_enabled_flag;
  bool sample_adaptive_offset_enabled_flag;
  bool pcm_enabled_flag;
  int pcm_sample_bit_depth_luma_minus1;
  int pcm_sample_bit_depth_chroma_minus1;
  int log2_min_pcm_luma_coding_block_size_minus3;
  int log2_diff_max_min_pcm_luma_coding_block_size;
  bool pcm_loop_filter_disabled_flag;
  int num_short_term_ref_pic_sets;
  H265StRefPicSet st_ref_pic_set[kMaxShortTermRefPicSets];
  bool long_term_ref_pics_present_flag;
  int num_long_term_ref_pics_sps;
  int lt_ref_pic_poc_lsb_sps[kMaxLongTermRefPicSets];
  bool used_by_curr_pic_lt_sps_flag[kMaxLongTermRefPicSets];
  bool sps_temporal_mvp_enabled_flag;
  bool strong_intra_smoothing_enabled_flag;
  H265VUIParameters vui_parameters;

  // Extension extra elements.
  bool sps_extension_present_flag;
  bool sps_range_extension_flag;
  bool sps_multilayer_extension_flag;
  bool sps_3d_extension_flag;
  bool sps_scc_extension_flag;
  bool transform_skip_rotation_enabled_flag;
  bool transform_skip_context_enabled_flag;
  bool implicit_rdpcm_enabled_flag;
  bool explicit_rdpcm_enabled_flag;
  bool extended_precision_processing_flag;
  bool intra_smoothing_disabled_flag;
  bool high_precision_offsets_enabled_flag;
  bool persistent_rice_adaptation_enabled_flag;
  bool cabac_bypass_alignment_enabled_flag;

  // Calculated fields.
  int chroma_array_type;
  int sub_width_c;
  int sub_height_c;
  size_t max_dpb_size;
  int bit_depth_y;
  int bit_depth_c;
  int max_pic_order_cnt_lsb;
  int ctb_log2_size_y;
  int pic_width_in_ctbs_y;
  int pic_height_in_ctbs_y;
  int pic_size_in_ctbs_y;
  int wp_offset_half_range_y;
  int wp_offset_half_range_c;
  uint32_t sps_max_latency_pictures[kMaxSubLayers];

  // Helpers to compute frequently-used values. They do not verify that the
  // results are in-spec for the given profile or level.
  gfx::Size GetCodedSize() const;
  gfx::Rect GetVisibleRect() const;
  VideoColorSpace GetColorSpace() const;
  VideoChromaSampling GetChromaSampling() const;
};

struct MEDIA_EXPORT H265PPS {
  H265PPS();

  enum {
    kMaxNumTileColumnWidth = 19,  // From VAAPI.
    kMaxNumTileRowHeight = 21,    // From VAAPI.
  };

  // Syntax elements.
  int pps_pic_parameter_set_id;
  int pps_seq_parameter_set_id;
  bool dependent_slice_segments_enabled_flag;
  bool output_flag_present_flag;
  int num_extra_slice_header_bits;
  bool sign_data_hiding_enabled_flag;
  bool cabac_init_present_flag;
  int num_ref_idx_l0_default_active_minus1;
  int num_ref_idx_l1_default_active_minus1;
  int init_qp_minus26;
  bool constrained_intra_pred_flag;
  bool transform_skip_enabled_flag;
  bool cu_qp_delta_enabled_flag;
  int diff_cu_qp_delta_depth;
  int pps_cb_qp_offset;
  int pps_cr_qp_offset;
  bool pps_slice_chroma_qp_offsets_present_flag;
  bool weighted_pred_flag;
  bool weighted_bipred_flag;
  bool transquant_bypass_enabled_flag;
  bool tiles_enabled_flag;
  bool entropy_coding_sync_enabled_flag;
  int num_tile_columns_minus1;
  int num_tile_rows_minus1;
  bool uniform_spacing_flag;
  int column_width_minus1[kMaxNumTileColumnWidth];
  int row_height_minus1[kMaxNumTileRowHeight];
  bool loop_filter_across_tiles_enabled_flag;
  bool pps_loop_filter_across_slices_enabled_flag;
  bool deblocking_filter_control_present_flag;
  bool deblocking_filter_override_enabled_flag;
  bool pps_deblocking_filter_disabled_flag;
  int pps_beta_offset_div2;
  int pps_tc_offset_div2;
  bool pps_scaling_list_data_present_flag;
  H265ScalingListData scaling_list_data;
  bool lists_modification_present_flag;
  int log2_parallel_merge_level_minus2;
  bool slice_segment_header_extension_present_flag;

  // Extension extra elements.
  bool pps_extension_present_flag;
  bool pps_range_extension_flag;
  bool pps_multilayer_extension_flag;
  bool pps_3d_extension_flag;
  bool pps_scc_extension_flag;
  int log2_max_transform_skip_block_size_minus2;
  bool cross_component_prediction_enabled_flag;
  bool chroma_qp_offset_list_enabled_flag;
  int diff_cu_chroma_qp_offset_depth;
  int chroma_qp_offset_list_len_minus1;
  int cb_qp_offset_list[6];
  int cr_qp_offset_list[6];
  int log2_sao_offset_scale_luma;
  int log2_sao_offset_scale_chroma;

  // Calculated fields.
  int qp_bd_offset_y;
};

struct MEDIA_EXPORT H265RefPicListsModifications {
  H265RefPicListsModifications();

  // Syntax elements.
  bool ref_pic_list_modification_flag_l0;
  int list_entry_l0[kMaxRefIdxActive];
  bool ref_pic_list_modification_flag_l1;
  int list_entry_l1[kMaxRefIdxActive];
};

struct MEDIA_EXPORT H265PredWeightTable {
  H265PredWeightTable();

  // Syntax elements.
  int luma_log2_weight_denom;
  int delta_chroma_log2_weight_denom;
  int chroma_log2_weight_denom;
  int delta_luma_weight_l0[kMaxRefIdxActive];
  int luma_offset_l0[kMaxRefIdxActive];
  int delta_chroma_weight_l0[kMaxRefIdxActive][2];
  int delta_chroma_offset_l0[kMaxRefIdxActive][2];
  int delta_luma_weight_l1[kMaxRefIdxActive];
  int luma_offset_l1[kMaxRefIdxActive];
  int delta_chroma_weight_l1[kMaxRefIdxActive][2];
  int delta_chroma_offset_l1[kMaxRefIdxActive][2];
};

struct MEDIA_EXPORT H265SliceHeader {
  H265SliceHeader();

  enum {
    kSliceTypeB = 0,  // Table 7-7
    kSliceTypeP = 1,  // Table 7-7
    kSliceTypeI = 2,  // Table 7-7
  };

  int nal_unit_type;         // from NAL header
  raw_ptr<const uint8_t, AllowPtrArithmetic> nalu_data;  // from NAL header
  size_t nalu_size;          // from NAL header
  int temporal_id;           // calculated from NAL header
  size_t header_size;  // calculated, not including emulation prevention bytes
  size_t header_emulation_prevention_bytes;

  // Calculated, but needs to be preserved when we copy slice dependent data
  // so put it at the front.
  bool irap_pic;

  // Syntax elements.
  bool first_slice_segment_in_pic_flag;
  bool no_output_of_prior_pics_flag;
  int slice_pic_parameter_set_id;
  bool dependent_slice_segment_flag;
  int slice_segment_address;
  // Do not move any of the above fields below or vice-versa, everything after
  // this is copied as a block.
  int slice_type;
  bool pic_output_flag;
  int colour_plane_id;
  int slice_pic_order_cnt_lsb;
  bool short_term_ref_pic_set_sps_flag;
  H265StRefPicSet st_ref_pic_set;
  // Do not change the order of the following fields up through
  // slice_sao_luma_flag. They are compared as a block.
  int short_term_ref_pic_set_idx;
  int num_long_term_sps;
  int num_long_term_pics;
  int poc_lsb_lt[kMaxLongTermRefPicSets];
  bool used_by_curr_pic_lt[kMaxLongTermRefPicSets];
  bool delta_poc_msb_present_flag[kMaxLongTermRefPicSets];
  int delta_poc_msb_cycle_lt[kMaxLongTermRefPicSets];
  bool slice_temporal_mvp_enabled_flag;
  bool slice_sao_luma_flag;
  bool slice_sao_chroma_flag;
  bool num_ref_idx_active_override_flag;
  int num_ref_idx_l0_active_minus1;
  int num_ref_idx_l1_active_minus1;
  H265RefPicListsModifications ref_pic_lists_modification;
  bool mvd_l1_zero_flag;
  bool cabac_init_flag;
  bool collocated_from_l0_flag;
  int collocated_ref_idx;
  H265PredWeightTable pred_weight_table;
  int five_minus_max_num_merge_cand;
  int slice_qp_delta;
  int slice_cb_qp_offset;
  int slice_cr_qp_offset;
  bool slice_deblocking_filter_disabled_flag;
  int slice_beta_offset_div2;
  int slice_tc_offset_div2;
  bool slice_loop_filter_across_slices_enabled_flag;

  // Calculated.
  int curr_rps_idx;
  int num_pic_total_curr;
  // Number of bits st_ref_pic_set takes after removing emulation prevention
  // bytes.
  int st_rps_bits;
  // Number of bits lt_ref_pic_set takes after removing emulation prevention
  // bytes.
  int lt_rps_bits;

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
  bool alpha_channel_cancel_flag;
  int alpha_channel_use_idc;
  int alpha_channel_bit_depth_minus8;
  int alpha_transparent_value;
  int alpha_opaque_value;
  bool alpha_channel_incr_flag;
  bool alpha_channel_clip_flag;
  bool alpha_channel_clip_type_flag;
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

  uint16_t display_primaries[kNumDisplayPrimaries][kDisplayPrimaryComponents];
  uint16_t white_points[2];
  uint32_t max_luminance;
  uint32_t min_luminance;

  gfx::HdrMetadataSmpteSt2086 ToGfx() const;
};

struct MEDIA_EXPORT H265SEIMessage {
  H265SEIMessage();

  enum Type {
    kSEIMasteringDisplayInfo = 137,
    kSEIContentLightLevelInfo = 144,
    kSEIAlphaChannelInfo = 165,
  };

  int type;
  int payload_size;
  union {
    // Placeholder; in future more supported types will contribute to more
    // union members here.
    H265SEIAlphaChannelInfo alpha_channel_info;
    H265SEIContentLightLevelInfo content_light_level_info;
    H265SEIMasteringDisplayInfo mastering_display_info;
  };
};

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
