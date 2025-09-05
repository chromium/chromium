// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_AV1_BUILDER_H_
#define MEDIA_GPU_AV1_BUILDER_H_

#include <array>
#include <optional>
#include <vector>

#include "media/gpu/media_gpu_export.h"
#include "third_party/libgav1/src/src/gav1/decoder_buffer.h"
#include "third_party/libgav1/src/src/utils/constants.h"
#include "third_party/libgav1/src/src/utils/types.h"

namespace media {

// Helper class for AV1 to write packed bitstream data.
class MEDIA_GPU_EXPORT AV1BitstreamBuilder {
 public:
  struct SequenceHeader;
  struct FrameHeader;
  static constexpr uint32_t kMaxTemporalLayerNum = 3;

  AV1BitstreamBuilder();
  ~AV1BitstreamBuilder();
  AV1BitstreamBuilder(AV1BitstreamBuilder&&);

  // Pack sequence header OBU. Spec 5.5.
  static AV1BitstreamBuilder BuildSequenceHeaderOBU(
      const SequenceHeader& seq_hdr);
  // Pack frame OBU. Spec 5.9.
  static AV1BitstreamBuilder BuildFrameHeaderOBU(const SequenceHeader& seq_hdr,
                                                 const FrameHeader& pic_hdr);

  void Write(uint64_t val, int num_bits);
  void WriteBool(bool val);
  // Spec 5.3.2.
  void WriteOBUHeader(libgav1::ObuType type,
                      bool has_size,
                      bool extension_flag = false,
                      std::optional<uint8_t> temporal_id = std::nullopt);
  // Writes a value encoded in LEB128. Spec 4.10.5.
  void WriteValueInLeb128(uint32_t value,
                          std::optional<int> fixed_size = std::nullopt);
  // Writes a value encoded in SU(num_bits). Spec 4.10.6.
  void WriteSU(int16_t value, size_t num_bits);
  std::vector<uint8_t> Flush() &&;
  size_t OutstandingBits() const { return total_outstanding_bits_; }
  // Writes bits for the byte alignment. Spec 5.3.5.
  void PutAlignBits();
  // Writes trailing bits. Spec 5.3.4.
  void PutTrailingBits();
  void AppendBitstreamBuffer(AV1BitstreamBuilder buffer);

 private:
  std::vector<std::pair<uint64_t, int>> queued_writes_;
  size_t total_outstanding_bits_ = 0;
};

// Parameters used to build OBUs.
struct MEDIA_GPU_EXPORT AV1BitstreamBuilder::SequenceHeader {
  SequenceHeader();
  SequenceHeader(const SequenceHeader&);
  SequenceHeader& operator=(SequenceHeader&&) noexcept;

  uint32_t profile = 0;
  uint32_t operating_points_cnt_minus_1 = 0;
  std::array<uint32_t, kMaxTemporalLayerNum> level{};
  std::array<uint32_t, kMaxTemporalLayerNum> tier{};
  uint32_t frame_width_bits_minus_1 = 0;
  uint32_t frame_height_bits_minus_1 = 0;
  uint32_t width = 0;
  uint32_t height = 0;
  bool use_128x128_superblock = false;
  bool enable_filter_intra = false;
  bool enable_intra_edge_filter = false;
  bool enable_interintra_compound = false;
  bool enable_masked_compound = false;
  bool enable_warped_motion = false;
  bool enable_dual_filter = false;
  bool enable_order_hint = false;
  bool enable_jnt_comp = false;
  bool enable_ref_frame_mvs = false;
  uint32_t order_hint_bits_minus_1 = 0;
  bool enable_superres = false;
  bool enable_cdef = false;
  bool enable_restoration = false;
  bool color_description_present_flag = false;
  Libgav1ColorPrimary color_primaries = kLibgav1ColorPrimaryBt709;
  Libgav1TransferCharacteristics transfer_characteristics =
      kLibgav1TransferCharacteristicsBt709;
  Libgav1MatrixCoefficients matrix_coefficients =
      kLibgav1MatrixCoefficientsBt709;
  Libgav1ColorRange color_range = kLibgav1ColorRangeStudio;
  Libgav1ChromaSamplePosition chroma_sample_position =
      kLibgav1ChromaSamplePositionUnknown;
};

struct MEDIA_GPU_EXPORT AV1BitstreamBuilder::FrameHeader {
  FrameHeader();
  FrameHeader(FrameHeader&&) noexcept;

  libgav1::FrameType frame_type = libgav1::FrameType::kFrameKey;
  bool error_resilient_mode = false;
  bool disable_cdf_update = false;
  bool disable_frame_end_update_cdf = false;
  uint32_t base_qindex = 0;
  bool separate_uv_delta_q = true;
  int8_t delta_q_y_dc = 0;
  int8_t delta_q_u_dc = 0;
  int8_t delta_q_u_ac = 0;
  int8_t delta_q_v_dc = 0;
  int8_t delta_q_v_ac = 0;
  bool using_qmatrix = false;
  uint8_t qm_y = 0;
  uint8_t qm_u = 0;
  uint8_t qm_v = 0;
  uint8_t order_hint = 0;
  std::array<uint32_t, 2> filter_level{};
  uint32_t filter_level_u = 0;
  uint32_t filter_level_v = 0;
  uint32_t sharpness_level = 0;
  bool loop_filter_delta_enabled = false;
  bool loop_filter_delta_update = false;
  bool update_ref_delta = false;
  std::array<int8_t, 8> loop_filter_ref_deltas{};
  bool update_mode_delta = false;
  std::array<int8_t, 2> loop_filter_mode_deltas{};
  bool delta_lf_present = false;
  uint8_t delta_lf_res = 0;
  bool delta_lf_multi = false;
  bool delta_q_present = false;
  uint8_t delta_q_res = 0;
  uint8_t primary_ref_frame = 0;
  std::array<uint8_t, 7> ref_frame_idx{};
  uint8_t refresh_frame_flags = 0;
  std::array<uint32_t, 8> ref_order_hint{};
  uint8_t cdef_damping_minus_3 = 0;
  uint8_t cdef_bits = 0;
  std::array<uint8_t, 8> cdef_y_pri_strength{};
  std::array<uint8_t, 8> cdef_y_sec_strength{};
  std::array<uint8_t, 8> cdef_uv_pri_strength{};
  std::array<uint8_t, 8> cdef_uv_sec_strength{};
  std::array<libgav1::LoopRestorationType, 3 /*libgav1::kNumPlanes*/>
      restoration_type{};
  // lr_unit_shift and lr_uv_shift with use_128x128_superblock considered.
  uint8_t lr_unit_shift = 0;
  uint8_t lr_uv_shift = 0;
  uint8_t tx_mode = 0;
  bool reduced_tx_set = false;
  bool segmentation_enabled = false;
  bool segmentation_update_map = false;
  bool segmentation_temporal_update = false;
  bool segmentation_update_data = false;
  uint32_t segment_number = 0;
  std::array<std::array<bool, 8 /*libgav1::kSegmentFeatureMax*/>,
             8 /*libgav1::kMaxSegments*/>
      feature_enabled{};
  std::array<std::array<int16_t, 8 /*libgav1::kSegmentFeatureMax*/>,
             8 /*libgav1::kMaxSegments*/>
      feature_data{};
  bool allow_screen_content_tools = false;
  bool allow_intrabc = false;
  bool reference_select = false;
  libgav1::InterpolationFilter interpolation_filter =
      libgav1::InterpolationFilter::kInterpolationFilterEightTap;
};

}  // namespace media

#endif  // MEDIA_GPU_AV1_BUILDER_H_
