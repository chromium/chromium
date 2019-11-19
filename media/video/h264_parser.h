// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains an implementation of an H264 Annex-B video stream parser.

#ifndef MEDIA_VIDEO_H264_PARSER_H_
#define MEDIA_VIDEO_H264_PARSER_H_

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "media/base/media_export.h"
#include "media/base/ranges.h"
#include "media/base/video_codecs.h"
#include "media/base/video_color_space.h"
#include "media/video/h264_bit_reader.h"

namespace gfx {
class Rect;
class Size;
}  // namespace gfx

namespace media {

struct SubsampleEntry;

// For explanations of each struct and its members, see H.264 specification
// at http://www.itu.int/rec/T-REC-H.264.
struct MEDIA_EXPORT H264NALU {
  H264NALU();

  enum Type {
    kUnspecified = 0,
    kNonIDRSlice = 1,
    kSliceDataA = 2,
    kSliceDataB = 3,
    kSliceDataC = 4,
    kIDRSlice = 5,
    kSEIMessage = 6,
    kSPS = 7,
    kPPS = 8,
    kAUD = 9,
    kEOSeq = 10,
    kEOStream = 11,
    kFiller = 12,
    kSPSExt = 13,
    kReserved14 = 14,
    kReserved15 = 15,
    kReserved16 = 16,
    kReserved17 = 17,
    kReserved18 = 18,
    kCodedSliceAux = 19,
    kCodedSliceExtension = 20,
  };

  // After (without) start code; we don't own the underlying memory
  // and a shallow copy should be made when copying this struct.
  const uint8_t* data;
  off_t size;  // From after start code to start code of next NALU (or EOS).

  int nal_ref_idc;
  int nal_unit_type;
};

enum {
  kH264ScalingList4x4Length = 16,
  kH264ScalingList8x8Length = 64,
};

struct MEDIA_EXPORT H264SPS {
  H264SPS();

  enum H264ProfileIDC {
    kProfileIDCBaseline = 66,
    kProfileIDCConstrainedBaseline = kProfileIDCBaseline,
    kProfileIDCMain = 77,
    kProfileIDScalableBaseline = 83,
    kProfileIDScalableHigh = 86,
    kProfileIDCHigh = 100,
    kProfileIDHigh10 = 110,
    kProfileIDSMultiviewHigh = 118,
    kProfileIDHigh422 = 122,
    kProfileIDStereoHigh = 128,
    kProfileIDHigh444Predictive = 244,
  };

  enum H264LevelIDC : uint8_t {
    kLevelIDC1p0 = 10,
    kLevelIDC1B = 9,
    kLevelIDC1p1 = 11,
    kLevelIDC1p2 = 12,
    kLevelIDC1p3 = 13,
    kLevelIDC2p0 = 20,
    kLevelIDC2p1 = 21,
    kLevelIDC2p2 = 22,
    kLevelIDC3p0 = 30,
    kLevelIDC3p1 = 31,
    kLevelIDC3p2 = 32,
    kLevelIDC4p0 = 40,
    kLevelIDC4p1 = 41,
    kLevelIDC4p2 = 42,
    kLevelIDC5p0 = 50,
    kLevelIDC5p1 = 51,
    kLevelIDC5p2 = 52,
    kLevelIDC6p0 = 60,
    kLevelIDC6p1 = 61,
    kLevelIDC6p2 = 62,
  };

  enum AspectRatioIdc {
    kExtendedSar = 255,
  };

  enum {
    // Constants for HRD parameters (spec ch. E.2.2).
    kBitRateScaleConstantTerm = 6,  // Equation E-37.
    kCPBSizeScaleConstantTerm = 4,  // Equation E-38.
    kDefaultInitialCPBRemovalDelayLength = 24,
    kDefaultDPBOutputDelayLength = 24,
    kDefaultTimeOffsetLength = 24,
  };

  int profile_idc;
  bool constraint_set0_flag;
  bool constraint_set1_flag;
  bool constraint_set2_flag;
  bool constraint_set3_flag;
  bool constraint_set4_flag;
  bool constraint_set5_flag;
  int level_idc;
  int seq_parameter_set_id;

  int chroma_format_idc;
  bool separate_colour_plane_flag;
  int bit_depth_luma_minus8;
  int bit_depth_chroma_minus8;
  bool qpprime_y_zero_transform_bypass_flag;

  bool seq_scaling_matrix_present_flag;
  int scaling_list4x4[6][kH264ScalingList4x4Length];
  int scaling_list8x8[6][kH264ScalingList8x8Length];

  int log2_max_frame_num_minus4;
  int pic_order_cnt_type;
  int log2_max_pic_order_cnt_lsb_minus4;
  bool delta_pic_order_always_zero_flag;
  int offset_for_non_ref_pic;
  int offset_for_top_to_bottom_field;
  int num_ref_frames_in_pic_order_cnt_cycle;
  int expected_delta_per_pic_order_cnt_cycle;  // calculated
  int offset_for_ref_frame[255];
  int max_num_ref_frames;
  bool gaps_in_frame_num_value_allowed_flag;
  int pic_width_in_mbs_minus1;
  int pic_height_in_map_units_minus1;
  bool frame_mbs_only_flag;
  bool mb_adaptive_frame_field_flag;
  bool direct_8x8_inference_flag;
  bool frame_cropping_flag;
  int frame_crop_left_offset;
  int frame_crop_right_offset;
  int frame_crop_top_offset;
  int frame_crop_bottom_offset;

  bool vui_parameters_present_flag;
  int sar_width;   // Set to 0 when not specified.
  int sar_height;  // Set to 0 when not specified.
  bool bitstream_restriction_flag;
  int max_num_reorder_frames;
  int max_dec_frame_buffering;
  bool timing_info_present_flag;
  int num_units_in_tick;
  int time_scale;
  bool fixed_frame_rate_flag;

  bool video_signal_type_present_flag;
  int video_format;
  bool video_full_range_flag;
  bool colour_description_present_flag;
  int colour_primaries;
  int transfer_characteristics;
  int matrix_coefficients;

  // TODO(posciak): actually parse these instead of ParseAndIgnoreHRDParameters.
  bool nal_hrd_parameters_present_flag;
  int cpb_cnt_minus1;
  int bit_rate_scale;
  int cpb_size_scale;
  int bit_rate_value_minus1[32];
  int cpb_size_value_minus1[32];
  bool cbr_flag[32];
  int initial_cpb_removal_delay_length_minus_1;
  int cpb_removal_delay_length_minus1;
  int dpb_output_delay_length_minus1;
  int time_offset_length;

  bool low_delay_hrd_flag;

  int chroma_array_type;

  // Get corresponding SPS |level_idc| and |constraint_set3_flag| value from
  // requested |profile| and |level| (see Spec A.3.1).
  static void GetLevelConfigFromProfileLevel(VideoCodecProfile profile,
                                             uint8_t level,
                                             int* level_idc,
                                             bool* constraint_set3_flag);

  // Helpers to compute frequently-used values. These methods return
  // base::nullopt if they encounter integer overflow. They do not verify that
  // the results are in-spec for the given profile or level.
  base::Optional<gfx::Size> GetCodedSize() const;
  base::Optional<gfx::Rect> GetVisibleRect() const;
  VideoColorSpace GetColorSpace() const;

  // Helper to compute indicated level from parsed SPS data. The value of
  // indicated level would be included in H264LevelIDC enum representing the
  // level as in name.
  uint8_t GetIndicatedLevel() const;
  // Helper to check if indicated level is lower than or equal to
  // |target_level|.
  bool CheckIndicatedLevelWithinTarget(uint8_t target_level) const;
};

struct MEDIA_EXPORT H264PPS {
  H264PPS();

  int pic_parameter_set_id;
  int seq_parameter_set_id;
  bool entropy_coding_mode_flag;
  bool bottom_field_pic_order_in_frame_present_flag;
  int num_slice_groups_minus1;
  // TODO(posciak): Slice groups not implemented, could be added at some point.
  int num_ref_idx_l0_default_active_minus1;
  int num_ref_idx_l1_default_active_minus1;
  bool weighted_pred_flag;
  int weighted_bipred_idc;
  int pic_init_qp_minus26;
  int pic_init_qs_minus26;
  int chroma_qp_index_offset;
  bool deblocking_filter_control_present_flag;
  bool constrained_intra_pred_flag;
  bool redundant_pic_cnt_present_flag;
  bool transform_8x8_mode_flag;

  bool pic_scaling_matrix_present_flag;
  int scaling_list4x4[6][kH264ScalingList4x4Length];
  int scaling_list8x8[6][kH264ScalingList8x8Length];

  int second_chroma_qp_index_offset;
};

struct MEDIA_EXPORT H264ModificationOfPicNum {
  int modification_of_pic_nums_idc;
  union {
    int abs_diff_pic_num_minus1;
    int long_term_pic_num;
  };
};

struct MEDIA_EXPORT H264WeightingFactors {
  bool luma_weight_flag;
  bool chroma_weight_flag;
  int luma_weight[32];
  int luma_offset[32];
  int chroma_weight[32][2];
  int chroma_offset[32][2];
};

struct MEDIA_EXPORT H264DecRefPicMarking {
  int memory_mgmnt_control_operation;
  int difference_of_pic_nums_minus1;
  int long_term_pic_num;
  int long_term_frame_idx;
  int max_long_term_frame_idx_plus1;
};

struct MEDIA_EXPORT H264SliceHeader {
  H264SliceHeader();

  enum { kRefListSize = 32, kRefListModSize = kRefListSize };

  enum Type {
    kPSlice = 0,
    kBSlice = 1,
    kISlice = 2,
    kSPSlice = 3,
    kSISlice = 4,
  };

  bool IsPSlice() const;
  bool IsBSlice() const;
  bool IsISlice() const;
  bool IsSPSlice() const;
  bool IsSISlice() const;

  bool idr_pic_flag;         // from NAL header
  int nal_ref_idc;           // from NAL header
  const uint8_t* nalu_data;  // from NAL header
  off_t nalu_size;           // from NAL header
  off_t header_bit_size;     // calculated

  int first_mb_in_slice;
  int slice_type;
  int pic_parameter_set_id;
  int colour_plane_id;  // TODO(posciak): use this!  http://crbug.com/139878
  int frame_num;
  bool field_pic_flag;
  bool bottom_field_flag;
  int idr_pic_id;
  int pic_order_cnt_lsb;
  int delta_pic_order_cnt_bottom;
  int delta_pic_order_cnt0;
  int delta_pic_order_cnt1;
  int redundant_pic_cnt;
  bool direct_spatial_mv_pred_flag;

  bool num_ref_idx_active_override_flag;
  int num_ref_idx_l0_active_minus1;
  int num_ref_idx_l1_active_minus1;
  bool ref_pic_list_modification_flag_l0;
  bool ref_pic_list_modification_flag_l1;
  H264ModificationOfPicNum ref_list_l0_modifications[kRefListModSize];
  H264ModificationOfPicNum ref_list_l1_modifications[kRefListModSize];

  int luma_log2_weight_denom;
  int chroma_log2_weight_denom;

  bool luma_weight_l0_flag;
  bool chroma_weight_l0_flag;
  H264WeightingFactors pred_weight_table_l0;

  bool luma_weight_l1_flag;
  bool chroma_weight_l1_flag;
  H264WeightingFactors pred_weight_table_l1;

  bool no_output_of_prior_pics_flag;
  bool long_term_reference_flag;

  bool adaptive_ref_pic_marking_mode_flag;
  H264DecRefPicMarking ref_pic_marking[kRefListSize];

  int cabac_init_idc;
  int slice_qp_delta;
  bool sp_for_switch_flag;
  int slice_qs_delta;
  int disable_deblocking_filter_idc;
  int slice_alpha_c0_offset_div2;
  int slice_beta_offset_div2;

  // Calculated.
  // Size in bits of dec_ref_pic_marking() syntax element.
  size_t dec_ref_pic_marking_bit_size;
  size_t pic_order_cnt_bit_size;
};

struct H264SEIRecoveryPoint {
  int recovery_frame_cnt;
  bool exact_match_flag;
  bool broken_link_flag;
  int changing_slice_group_idc;
};

struct MEDIA_EXPORT H264SEIMessage {
  H264SEIMessage();

  enum Type {
    kSEIRecoveryPoint = 6,
  };

  int type;
  int payload_size;
  union {
    // Placeholder; in future more supported types will contribute to more
    // union members here.
    H264SEIRecoveryPoint recovery_point;
  };
};

// Class to parse an Annex-B H.264 stream,
// as specified in chapters 7 and Annex B of the H.264 spec.
class MEDIA_EXPORT H264Parser {
 public:
  enum Result {
    kOk,
    kInvalidStream,      // error in stream
    kUnsupportedStream,  // stream not supported by the parser
    kEOStream,           // end of stream
  };

  // Find offset from start of data to next NALU start code
  // and size of found start code (3 or 4 bytes).
  // If no start code is found, offset is pointing to the first unprocessed byte
  // (i.e. the first byte that was not considered as a possible start of a start
  // code) and |*start_code_size| is set to 0.
  // Preconditions:
  // - |data_size| >= 0
  // Postconditions:
  // - |*offset| is between 0 and |data_size| included.
  //   It is strictly less than |data_size| if |data_size| > 0.
  // - |*start_code_size| is either 0, 3 or 4.
  static bool FindStartCode(const uint8_t* data,
                            off_t data_size,
                            off_t* offset,
                            off_t* start_code_size);

  // Wrapper for FindStartCode() that skips over start codes that
  // may appear inside of |encrypted_ranges_|.
  // Returns true if a start code was found. Otherwise returns false.
  static bool FindStartCodeInClearRanges(const uint8_t* data,
                                         off_t data_size,
                                         const Ranges<const uint8_t*>& ranges,
                                         off_t* offset,
                                         off_t* start_code_size);

  static VideoCodecProfile ProfileIDCToVideoCodecProfile(int profile_idc);

  // Parses the input stream and returns all the NALUs through |nalus|. Returns
  // false if the stream is invalid.
  static bool ParseNALUs(const uint8_t* stream,
                         size_t stream_size,
                         std::vector<H264NALU>* nalus);

  H264Parser();
  ~H264Parser();

  void Reset();
  // Set current stream pointer to |stream| of |stream_size| in bytes,
  // |stream| owned by caller.
  // |subsamples| contains information about what parts of |stream| are
  // encrypted.
  void SetStream(const uint8_t* stream, off_t stream_size);
  void SetEncryptedStream(const uint8_t* stream,
                          off_t stream_size,
                          const std::vector<SubsampleEntry>& subsamples);

  // Read the stream to find the next NALU, identify it and return
  // that information in |*nalu|. This advances the stream to the beginning
  // of this NALU, but not past it, so subsequent calls to NALU-specific
  // parsing functions (ParseSPS, etc.)  will parse this NALU.
  // If the caller wishes to skip the current NALU, it can call this function
  // again, instead of any NALU-type specific parse functions below.
  Result AdvanceToNextNALU(H264NALU* nalu);

  // NALU-specific parsing functions.
  // These should be called after AdvanceToNextNALU().

  // SPSes and PPSes are owned by the parser class and the memory for their
  // structures is managed here, not by the caller, as they are reused
  // across NALUs.
  //
  // Parse an SPS/PPS NALU and save their data in the parser, returning id
  // of the parsed structure in |*pps_id|/|*sps_id|.
  // To get a pointer to a given SPS/PPS structure, use GetSPS()/GetPPS(),
  // passing the returned |*sps_id|/|*pps_id| as parameter.
  // TODO(posciak,fischman): consider replacing returning Result from Parse*()
  // methods with a scoped_ptr and adding an AtEOS() function to check for EOS
  // if Parse*() return NULL.
  Result ParseSPS(int* sps_id);
  Result ParsePPS(int* pps_id);

  // Parses the SPS ID from the SPSExt, but otherwise does nothing.
  Result ParseSPSExt(int* sps_id);

  // Return a pointer to SPS/PPS with given |sps_id|/|pps_id| or NULL if not
  // present.
  const H264SPS* GetSPS(int sps_id) const;
  const H264PPS* GetPPS(int pps_id) const;

  // Slice headers and SEI messages are not used across NALUs by the parser
  // and can be discarded after current NALU, so the parser does not store
  // them, nor does it manage their memory.
  // The caller has to provide and manage it instead.

  // Parse a slice header, returning it in |*shdr|. |*nalu| must be set to
  // the NALU returned from AdvanceToNextNALU() and corresponding to |*shdr|.
  Result ParseSliceHeader(const H264NALU& nalu, H264SliceHeader* shdr);

  // Parse a SEI message, returning it in |*sei_msg|, provided and managed
  // by the caller.
  Result ParseSEI(H264SEIMessage* sei_msg);

  // The return value of this method changes for every successful call to
  // AdvanceToNextNALU().
  // This returns the subsample information for the last NALU that was output
  // from AdvanceToNextNALU().
  std::vector<SubsampleEntry> GetCurrentSubsamples();

 private:
  // Move the stream pointer to the beginning of the next NALU,
  // i.e. pointing at the next start code.
  // Return true if a NALU has been found.
  // If a NALU is found:
  // - its size in bytes is returned in |*nalu_size| and includes
  //   the start code as well as the trailing zero bits.
  // - the size in bytes of the start code is returned in |*start_code_size|.
  bool LocateNALU(off_t* nalu_size, off_t* start_code_size);

  // Exp-Golomb code parsing as specified in chapter 9.1 of the spec.
  // Read one unsigned exp-Golomb code from the stream and return in |*val|.
  Result ReadUE(int* val);

  // Read one signed exp-Golomb code from the stream and return in |*val|.
  Result ReadSE(int* val);

  // Parse scaling lists (see spec).
  Result ParseScalingList(int size, int* scaling_list, bool* use_default);
  Result ParseSPSScalingLists(H264SPS* sps);
  Result ParsePPSScalingLists(const H264SPS& sps, H264PPS* pps);

  // Parse optional VUI parameters in SPS (see spec).
  Result ParseVUIParameters(H264SPS* sps);
  // Set |hrd_parameters_present| to true only if they are present.
  Result ParseAndIgnoreHRDParameters(bool* hrd_parameters_present);

  // Parse reference picture lists' modifications (see spec).
  Result ParseRefPicListModifications(H264SliceHeader* shdr);
  Result ParseRefPicListModification(int num_ref_idx_active_minus1,
                                     H264ModificationOfPicNum* ref_list_mods);

  // Parse prediction weight table (see spec).
  Result ParsePredWeightTable(const H264SPS& sps, H264SliceHeader* shdr);

  // Parse weighting factors (see spec).
  Result ParseWeightingFactors(int num_ref_idx_active_minus1,
                               int chroma_array_type,
                               int luma_log2_weight_denom,
                               int chroma_log2_weight_denom,
                               H264WeightingFactors* w_facts);

  // Parse decoded reference picture marking information (see spec).
  Result ParseDecRefPicMarking(H264SliceHeader* shdr);

  // Pointer to the current NALU in the stream.
  const uint8_t* stream_;

  // Bytes left in the stream after the current NALU.
  off_t bytes_left_;

  H264BitReader br_;

  // PPSes and SPSes stored for future reference.
  std::map<int, std::unique_ptr<H264SPS>> active_SPSes_;
  std::map<int, std::unique_ptr<H264PPS>> active_PPSes_;

  // Ranges of encrypted bytes in the buffer passed to
  // SetEncryptedStream().
  Ranges<const uint8_t*> encrypted_ranges_;

  // This contains the range of the previous NALU found in
  // AdvanceToNextNalu(). Holds exactly one range.
  Ranges<const uint8_t*> previous_nalu_range_;

  DISALLOW_COPY_AND_ASSIGN(H264Parser);
};

}  // namespace media

#endif  // MEDIA_VIDEO_H264_PARSER_H_
