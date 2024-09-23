// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_AV1_BUILDER_H_
#define MEDIA_GPU_AV1_BUILDER_H_

#include <optional>
#include <vector>

#include "media/gpu/media_gpu_export.h"
#include "third_party/libgav1/src/src/utils/constants.h"
#include "third_party/libgav1/src/src/utils/types.h"

namespace media {

// Helper class for AV1 to write packed bitstream data.
class MEDIA_GPU_EXPORT AV1BitstreamBuilder {
 public:
  struct SequenceHeader;
  struct FrameHeader;

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
                      bool extension_flag,
                      bool has_size);
  // Writes a value encoded in LEB128. Spec 4.10.5.
  void WriteValueInLeb128(uint32_t value,
                          std::optional<int> fixed_size = std::nullopt);
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
struct AV1BitstreamBuilder::SequenceHeader {
  uint32_t profile;
  uint32_t level;
  uint32_t tier;
  uint32_t frame_width_bits_minus_1;
  uint32_t frame_height_bits_minus_1;
  uint32_t width;
  uint32_t height;
  bool use_128x128_superblock;
  bool enable_filter_intra;
  bool enable_intra_edge_filter;
  bool enable_interintra_compound;
  bool enable_masked_compound;
  bool enable_warped_motion;
  bool enable_dual_filter;
  bool enable_order_hint;
  bool enable_jnt_comp;
  bool enable_ref_frame_mvs;
  uint32_t order_hint_bits_minus_1;
  bool enable_superres;
  bool enable_cdef;
  bool enable_restoration;
};

struct AV1BitstreamBuilder::FrameHeader {
  libgav1::FrameType frame_type;
  bool error_resilient_mode;
  bool disable_cdf_update;
  bool disable_frame_end_update_cdf;
  uint32_t base_qindex;
  uint8_t order_hint;
  uint32_t filter_level[2];
  uint32_t filter_level_u;
  uint32_t filter_level_v;
  uint32_t sharpness_level;
  bool loop_filter_delta_enabled;
  uint8_t primary_ref_frame;
  uint8_t ref_frame_idx[7];
  uint8_t refresh_frame_flags;
  uint32_t ref_order_hint[8];
  uint8_t cdef_y_pri_strength[8];
  uint8_t cdef_y_sec_strength[8];
  uint8_t cdef_uv_pri_strength[8];
  uint8_t cdef_uv_sec_strength[8];
  bool reduced_tx_set;
  bool segmentation_enabled;
  bool segmentation_update_map;
  bool segmentation_temporal_update;
  bool segmentation_update_data;
  uint32_t segment_number;
  uint32_t feature_mask[8 /*libgav1::kMaxSegments*/];
  uint32_t feature_data[8 /*libgav1::kMaxSegments*/]
                       [8 /*libgav1::kSegmentFeatureMax*/];
};

}  // namespace media

#endif  // MEDIA_GPU_AV1_BUILDER_H_
