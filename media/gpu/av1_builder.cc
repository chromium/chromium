// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/av1_builder.h"

#include "base/check_op.h"

namespace media {

namespace {
constexpr int kPrimaryReferenceNone = 7;
}  // namespace

AV1BitstreamBuilder::AV1BitstreamBuilder() = default;
AV1BitstreamBuilder::~AV1BitstreamBuilder() = default;
AV1BitstreamBuilder::AV1BitstreamBuilder(AV1BitstreamBuilder&&) = default;

AV1BitstreamBuilder AV1BitstreamBuilder::BuildSequenceHeaderOBU(
    const SequenceHeader& seq_hdr) {
  AV1BitstreamBuilder ret;
  ret.Write(seq_hdr.profile, 3);
  ret.WriteBool(false);  // Still picture default 0.
  ret.WriteBool(false);  // Disable reduced still picture.
  ret.WriteBool(false);  // No timing info present.
  ret.WriteBool(false);  // No initial display delay.
  ret.Write(0, 5);       // No operating point.
  ret.Write(0, 12);  // No scalability information (operating_point_idc[0] = 0)
  ret.Write(seq_hdr.level, 5);
  if (seq_hdr.level > 7) {
    ret.WriteBool(seq_hdr.tier);
  }

  ret.Write(seq_hdr.frame_width_bits_minus_1, 4);
  ret.Write(seq_hdr.frame_height_bits_minus_1, 4);
  ret.Write(seq_hdr.width - 1, seq_hdr.frame_width_bits_minus_1 + 1);
  ret.Write(seq_hdr.height - 1, seq_hdr.frame_height_bits_minus_1 + 1);
  ret.WriteBool(false);  // No frame id numbers present.
  ret.WriteBool(seq_hdr.use_128x128_superblock);
  ret.WriteBool(seq_hdr.enable_filter_intra);
  ret.WriteBool(seq_hdr.enable_intra_edge_filter);
  ret.WriteBool(seq_hdr.enable_interintra_compound);
  ret.WriteBool(seq_hdr.enable_masked_compound);
  ret.WriteBool(seq_hdr.enable_warped_motion);
  ret.WriteBool(seq_hdr.enable_dual_filter);
  ret.WriteBool(seq_hdr.enable_order_hint);
  if (seq_hdr.enable_order_hint) {
    ret.WriteBool(seq_hdr.enable_jnt_comp);
    ret.WriteBool(seq_hdr.enable_ref_frame_mvs);
  }

  ret.WriteBool(true);   // Enable sequence choose screen content tools.
  ret.WriteBool(false);  // Disable sequence choose integer MV.
  ret.WriteBool(false);  // Disable sequence force integer MV.
  if (seq_hdr.enable_order_hint) {
    ret.Write(seq_hdr.order_hint_bits_minus_1, 3);
  }
  ret.WriteBool(seq_hdr.enable_superres);
  ret.WriteBool(seq_hdr.enable_cdef);
  ret.WriteBool(seq_hdr.enable_restoration);

  ret.WriteBool(false);  // Disable high bitdepth.

  ret.WriteBool(false);  // Disable monochrome.
  ret.WriteBool(false);  // Disable color description present.
  ret.WriteBool(false);  // No color range.
  ret.Write(0, 2);       // Chroma sample position = 0.

  ret.WriteBool(true);   // Separate uv delta q.
  ret.WriteBool(false);  // No film grain parameters present.

  ret.PutTrailingBits();
  return ret;
}

AV1BitstreamBuilder AV1BitstreamBuilder::BuildFrameHeaderOBU(
    const SequenceHeader& seq_hdr,
    const FrameHeader& pic_hdr) {
  AV1BitstreamBuilder ret;
  ret.WriteBool(false);  // For a frame OBU, the show_existing_frame flag is
                         // always set to 0.
  ret.Write(pic_hdr.frame_type, 2);
  ret.WriteBool(true);  // If this frame needs to be immediately output once
                        // decoded, show_frame flag should be true.
  if (pic_hdr.frame_type != libgav1::FrameType::kFrameKey) {
    ret.WriteBool(pic_hdr.error_resilient_mode);
  }
  ret.WriteBool(pic_hdr.disable_cdf_update);
  ret.WriteBool(false);  // Disable allow screen content tools.
  ret.WriteBool(false);  // Disable frame size override flag.
  ret.Write(pic_hdr.order_hint, seq_hdr.order_hint_bits_minus_1 + 1);

  if (pic_hdr.frame_type != libgav1::FrameType::kFrameKey) {
    if (!pic_hdr.error_resilient_mode) {
      ret.Write(pic_hdr.primary_ref_frame, 3);
    }
    ret.Write(pic_hdr.refresh_frame_flags, 8);

    if (pic_hdr.error_resilient_mode && seq_hdr.enable_order_hint) {
      // Set order hint for each reference frame.
      for (uint32_t order_hint : pic_hdr.ref_order_hint) {
        ret.Write(order_hint, seq_hdr.order_hint_bits_minus_1 + 1);
      }
    }
    if (seq_hdr.enable_order_hint) {
      ret.WriteBool(false);  // Disable frame reference short signaling.
    }
    for (uint8_t ref_idx : pic_hdr.ref_frame_idx) {
      ret.Write(ref_idx, 3);
    }
    ret.WriteBool(false);  // Render and frame size are the same.
    ret.WriteBool(false);  // No allow high precision MV.
    ret.WriteBool(false);  // Filter not switchable.
    ret.Write(0, 2);       // Set interpolation filter to 0.
    ret.WriteBool(false);  // Motion not switchable.
  } else {
    ret.WriteBool(false);  // Render and frame size are the same.
  }
  if (!pic_hdr.disable_cdf_update) {
    ret.WriteBool(pic_hdr.disable_frame_end_update_cdf);
  }
  // Pack tile info
  ret.WriteBool(true);   // Uniform tile spacing.
  ret.WriteBool(false);  // Don't increment log2 of tile cols.
  ret.WriteBool(false);  // Don't increment log2 of tile rows.

  // Pack quantization parameters.
  ret.Write(pic_hdr.base_qindex, 8);
  ret.WriteBool(false);  // No DC Y delta Q.
  ret.WriteBool(false);  // No UV delta Q.
  ret.WriteBool(false);  // No DC U delta Q.
  ret.WriteBool(false);  // No AC U delta Q.
  ret.WriteBool(false);  // No Qmatrix.

  // Pack segmentation parameters.
  ret.WriteBool(pic_hdr.segmentation_enabled);
  if (pic_hdr.segmentation_enabled) {
    if (pic_hdr.primary_ref_frame != kPrimaryReferenceNone) {
      ret.WriteBool(pic_hdr.segmentation_update_map);
      ret.WriteBool(pic_hdr.segmentation_temporal_update);
      ret.WriteBool(pic_hdr.segmentation_update_data);
    }
    for (uint32_t i = 0; i < libgav1::kMaxSegments; i++) {
      for (uint32_t j = 0; j < libgav1::kSegmentFeatureMax; j++) {
        bool feature_enabled = (i < pic_hdr.segment_number &&
                                (pic_hdr.feature_mask[i] & (1u << j)));
        ret.WriteBool(feature_enabled);
        if (feature_enabled) {
          int delta_q = pic_hdr.feature_data[i][j];
          ret.WriteBool(delta_q < 0);  // Sign bit.
          if (delta_q < 0) {
            delta_q += 2 * (1 << 8);
          }
          ret.Write(delta_q, 8);  // Write the unsigned value.
        }
      }
    }
  }

  if (pic_hdr.base_qindex > 0) {
    ret.WriteBool(false);  // No delta q present.
  }

  // Pack loop filter parameters.
  ret.Write(pic_hdr.filter_level[0], 6);
  ret.Write(pic_hdr.filter_level[1], 6);
  if (pic_hdr.filter_level[0] || pic_hdr.filter_level[1]) {
    ret.Write(pic_hdr.filter_level_u, 6);
    ret.Write(pic_hdr.filter_level_v, 6);
  }
  ret.Write(pic_hdr.sharpness_level, 3);
  ret.WriteBool(pic_hdr.loop_filter_delta_enabled);

  // Pack CDEF parameters.
  if (seq_hdr.enable_cdef) {
    uint8_t num_planes = 3;  // mono_chrome not supported.
    ret.Write(2, 2);         // Set CDEF damping minus 3 to 5 - 3.
    ret.Write(3, 2);         // Set cdef_bits to 3.
    for (size_t i = 0; i < (1 << num_planes); i++) {
      ret.Write(pic_hdr.cdef_y_pri_strength[i], 4);
      ret.Write(pic_hdr.cdef_y_sec_strength[i], 2);
      ret.Write(pic_hdr.cdef_uv_pri_strength[i], 4);
      ret.Write(pic_hdr.cdef_uv_sec_strength[i], 2);
    }
  }
  ret.WriteBool(true);  // TxMode TX_MODE_SELECT.
  if (pic_hdr.frame_type != libgav1::FrameType::kFrameKey) {
    ret.WriteBool(false);  // Disable reference select.
  }
  ret.WriteBool(pic_hdr.reduced_tx_set);

  if (pic_hdr.frame_type != libgav1::FrameType::kFrameKey) {
    for (int i = 1 /*LAST_FRAME*/; i <= 7 /*ALTREF_FRAME*/; i++) {
      ret.WriteBool(false);  // Set is_global to all zeros.
    }
  }

  ret.PutAlignBits();
  return ret;
}

void AV1BitstreamBuilder::Write(uint64_t val, int num_bits) {
  queued_writes_.emplace_back(val, num_bits);
  total_outstanding_bits_ += num_bits;
}

void AV1BitstreamBuilder::WriteBool(bool val) {
  Write(val, 1);
}

std::vector<uint8_t> AV1BitstreamBuilder::Flush() && {
  std::vector<uint8_t> ret;
  uint8_t curr_byte = 0;
  int rem_bits_in_byte = 8;
  for (auto queued_write : queued_writes_) {
    uint64_t val = queued_write.first;
    int outstanding_bits = queued_write.second;
    while (outstanding_bits) {
      if (rem_bits_in_byte >= outstanding_bits) {
        curr_byte |= val << (rem_bits_in_byte - outstanding_bits);
        rem_bits_in_byte -= outstanding_bits;
        outstanding_bits = 0;
      } else {
        curr_byte |= (val >> (outstanding_bits - rem_bits_in_byte)) &
                     ((1 << rem_bits_in_byte) - 1);
        outstanding_bits -= rem_bits_in_byte;
        rem_bits_in_byte = 0;
      }
      if (!rem_bits_in_byte) {
        ret.push_back(curr_byte);
        curr_byte = 0;
        rem_bits_in_byte = 8;
      }
    }
  }

  if (rem_bits_in_byte != 8) {
    ret.push_back(curr_byte);
  }

  queued_writes_.clear();
  total_outstanding_bits_ = 0;

  return ret;
}

void AV1BitstreamBuilder::PutAlignBits() {
  int misalignment = total_outstanding_bits_ % 8;
  if (misalignment != 0) {
    int num_zero_bits = 8 - misalignment;
    Write(0, num_zero_bits);
  }
}

void AV1BitstreamBuilder::PutTrailingBits() {
  WriteBool(true);  // trialing one bit.
  PutAlignBits();
}

void AV1BitstreamBuilder::WriteOBUHeader(libgav1::ObuType type,
                                         bool extension_flag,
                                         bool has_size) {
  DCHECK_LE(1, type);
  DCHECK_LE(type, 8);
  WriteBool(false);  // forbidden bit must be set to 0.
  Write(static_cast<uint64_t>(type), 4);
  WriteBool(extension_flag);
  WriteBool(has_size);
  WriteBool(false);  // reserved bit must be set to 0.
}

// Encode a variable length unsigned integer of up to 4 bytes.
// Most significant bit of each byte indicates if parsing should continue, and
// the 7 least significant bits hold the actual data. So the encoded length
// may be 5 bytes under some circumstances.
// This function also has a fixed size mode where we pass in a fixed size for
// the data and the function zero pads up to that size.
// See section 4.10.5 of the AV1 specification.
void AV1BitstreamBuilder::WriteValueInLeb128(uint32_t value,
                                             std::optional<int> fixed_size) {
  for (int i = 0; i < fixed_size.value_or(5); i++) {
    uint8_t curr_byte = value & 0x7F;
    value >>= 7;
    if (value || fixed_size) {
      curr_byte |= 0x80;
      Write(curr_byte, 8);
    } else {
      Write(curr_byte, 8);
      break;
    }
  }
}

void AV1BitstreamBuilder::AppendBitstreamBuffer(AV1BitstreamBuilder buffer) {
  queued_writes_.insert(queued_writes_.end(),
                        std::make_move_iterator(buffer.queued_writes_.begin()),
                        std::make_move_iterator(buffer.queued_writes_.end()));
  total_outstanding_bits_ += buffer.total_outstanding_bits_;
}

}  // namespace media
