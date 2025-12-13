// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/av1_builder.h"

#include <iterator>

#include "base/check_op.h"
#include "base/numerics/safe_conversions.h"
#include "third_party/libgav1/src/src/obu_parser.h"

namespace media {

namespace {
constexpr int kPrimaryReferenceNone = 7;
}  // namespace

AV1BitstreamBuilder::SequenceHeader::SequenceHeader() = default;
AV1BitstreamBuilder::SequenceHeader::SequenceHeader(
    const AV1BitstreamBuilder::SequenceHeader&) = default;
AV1BitstreamBuilder::SequenceHeader&
AV1BitstreamBuilder::SequenceHeader::operator=(
    AV1BitstreamBuilder::SequenceHeader&&) noexcept = default;

AV1BitstreamBuilder::FrameHeader::FrameHeader() = default;
AV1BitstreamBuilder::FrameHeader::FrameHeader(
    AV1BitstreamBuilder::FrameHeader&&) noexcept = default;

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

  CHECK_LT(seq_hdr.operating_points_cnt_minus_1, kMaxTemporalLayerNum);
  ret.Write(seq_hdr.operating_points_cnt_minus_1, 5);
  for (uint8_t i = 0; i <= seq_hdr.operating_points_cnt_minus_1; i++) {
    if (seq_hdr.operating_points_cnt_minus_1 == 0) {
      ret.Write(0, 12);  // No scalability information.
    } else {
      ret.Write(1, 4);  // Spatial layer 1 should be decoded.
      ret.Write((1 << (seq_hdr.operating_points_cnt_minus_1 + 1 - i)) - 1, 8);
    }
    ret.Write(seq_hdr.level[i], 5);
    if (seq_hdr.level[i] > 7) {
      ret.WriteBool(seq_hdr.tier[i]);
    }
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

  // AV1 spec section 5.5.2, color config syntax.
  ret.WriteBool(false);  // Disable high bitdepth.
  if (seq_hdr.profile != libgav1::BitstreamProfile::kProfile1) {
    ret.WriteBool(false);  // Disable monochrome.
  }

  if (seq_hdr.color_description_present_flag) {
    ret.WriteBool(true);  // Color description present.
    ret.Write(seq_hdr.color_primaries, 8);
    ret.Write(seq_hdr.transfer_characteristics, 8);
    ret.Write(seq_hdr.matrix_coefficients, 8);
  } else {
    ret.WriteBool(false);  // No color description present.
  }

  // We won't skip color range syntax unless the color primariy is
  // Rec.709, transfer is sRGB and at the same time the identity
  // matrix is used.
  ret.WriteBool(seq_hdr.color_range);
  if (seq_hdr.profile != libgav1::BitstreamProfile::kProfile1) {
    ret.Write(0, 2);  // Chroma sample position = 0.
  }

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
  ret.WriteBool(pic_hdr.allow_screen_content_tools);
  ret.WriteBool(false);  // Disable frame size override flag.
  if (seq_hdr.enable_order_hint) {
    ret.Write(pic_hdr.order_hint, seq_hdr.order_hint_bits_minus_1 + 1);
  }

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
    bool is_switchable_interp =
        pic_hdr.interpolation_filter ==
        libgav1::InterpolationFilter::kInterpolationFilterSwitchable;
    ret.WriteBool(is_switchable_interp);
    if (!is_switchable_interp) {
      ret.Write(pic_hdr.interpolation_filter, 2);
    }
    ret.WriteBool(false);  // Motion not switchable.
    if (seq_hdr.enable_ref_frame_mvs) {
      ret.WriteBool(false);  // Do not use ref frame MVs.
    }
  } else {
    ret.WriteBool(false);  // Render and frame size are the same.
    if (pic_hdr.allow_screen_content_tools) {
      ret.WriteBool(pic_hdr.allow_intrabc);
    }
  }
  if (!pic_hdr.disable_cdf_update) {
    ret.WriteBool(pic_hdr.disable_frame_end_update_cdf);
  }
  // Pack tile info
  ret.WriteBool(true);   // Uniform tile spacing.
  ret.WriteBool(false);  // Don't increment log2 of tile cols.
  ret.WriteBool(false);  // Don't increment log2 of tile rows.

  // Pack quantization params. Refer to AV1 spec section 5.9.12.
  ret.Write(pic_hdr.base_qindex, 8);
  if (pic_hdr.delta_q_y_dc) {
    ret.WriteBool(true);
    ret.WriteSU(pic_hdr.delta_q_y_dc, 7);
  } else {
    ret.WriteBool(false);
  }
  if (pic_hdr.separate_uv_delta_q) {
    bool diff_uv_delta = false;
    if (pic_hdr.delta_q_u_dc != pic_hdr.delta_q_v_dc ||
        pic_hdr.delta_q_u_ac != pic_hdr.delta_q_v_ac) {
      diff_uv_delta = true;
    }
    ret.WriteBool(diff_uv_delta);
    for (const auto& delta_q : {pic_hdr.delta_q_u_dc, pic_hdr.delta_q_u_ac}) {
      if (delta_q) {
        ret.WriteBool(true);
        ret.WriteSU(delta_q, 7);
      } else {
        ret.WriteBool(false);
      }
    }
    if (diff_uv_delta) {
      for (const auto& delta_q : {pic_hdr.delta_q_v_dc, pic_hdr.delta_q_v_ac}) {
        if (delta_q) {
          ret.WriteBool(true);
          ret.WriteSU(delta_q, 7);
        } else {
          ret.WriteBool(false);
        }
      }
    }
  }
  ret.WriteBool(pic_hdr.using_qmatrix);
  if (pic_hdr.using_qmatrix) {
    ret.Write(pic_hdr.qm_y, 4);
    ret.Write(pic_hdr.qm_u, 4);
    if (pic_hdr.separate_uv_delta_q) {
      ret.Write(pic_hdr.qm_v, 4);
    }
  }

  // Pack segmentation params. Refer to AV1 spec section 5.9.14.
  ret.WriteBool(pic_hdr.segmentation_enabled);
  if (pic_hdr.segmentation_enabled) {
    bool segmentation_update_data = true;
    if (pic_hdr.primary_ref_frame != kPrimaryReferenceNone) {
      const bool segmentation_update_map = pic_hdr.segmentation_update_map;
      ret.WriteBool(segmentation_update_map);
      if (segmentation_update_map) {
        ret.WriteBool(pic_hdr.segmentation_temporal_update);
      }
      segmentation_update_data = pic_hdr.segmentation_update_data;
      ret.WriteBool(segmentation_update_data);
    }
    if (segmentation_update_data) {
      static constexpr std::array<uint8_t, libgav1::kSegmentFeatureMax>
          kSegmentaionFeatureBits = {8, 6, 6, 6, 6, 3, 0, 0};
      static constexpr std::array<bool, libgav1::kSegmentFeatureMax>
          kSegmentFeatureSigned = {true, true,  true,  true,
                                   true, false, false, false};
      for (uint32_t i = 0; i < libgav1::kMaxSegments; i++) {
        for (uint32_t j = 0; j < libgav1::kSegmentFeatureMax; j++) {
          const bool feature_enabled = pic_hdr.feature_enabled[i][j];
          ret.WriteBool(feature_enabled);
          if (feature_enabled) {
            const size_t bits_to_write = kSegmentaionFeatureBits[j];
            const int16_t feature_data = pic_hdr.feature_data[i][j];
            if (kSegmentFeatureSigned[j]) {
              ret.WriteSU(feature_data, bits_to_write + 1);
            } else if (bits_to_write > 0) {
              ret.Write(feature_data, bits_to_write);
            }
          }
        }
      }
    }
  }

  // Pack quantization index delta params. Refer to AV1 spec section 5.9.17.
  if (pic_hdr.base_qindex > 0) {
    ret.WriteBool(pic_hdr.delta_q_present);
    if (pic_hdr.delta_q_present) {
      ret.Write(pic_hdr.delta_q_res, 2);
    }
  }

  // Pack loop filter delta params. Refer to AV1 spec section 5.9.18.
  if (pic_hdr.delta_q_present && !pic_hdr.allow_intrabc) {
    ret.WriteBool(pic_hdr.delta_lf_present);
    if (pic_hdr.delta_lf_present) {
      ret.Write(pic_hdr.delta_lf_res, 2);
      ret.WriteBool(pic_hdr.delta_lf_multi);
    }
  }

  if (!pic_hdr.allow_intrabc) {
    // Pack loop filter parameters. Refer to AV1 spec section 5.9.11.
    ret.Write(pic_hdr.filter_level[0], 6);
    ret.Write(pic_hdr.filter_level[1], 6);
    if (pic_hdr.filter_level[0] || pic_hdr.filter_level[1]) {
      ret.Write(pic_hdr.filter_level_u, 6);
      ret.Write(pic_hdr.filter_level_v, 6);
    }
    ret.Write(pic_hdr.sharpness_level, 3);
    ret.WriteBool(pic_hdr.loop_filter_delta_enabled);
    if (pic_hdr.loop_filter_delta_enabled) {
      ret.WriteBool(pic_hdr.loop_filter_delta_update);
      if (pic_hdr.loop_filter_delta_update) {
        for (const auto& delta : pic_hdr.loop_filter_ref_deltas) {
          if (delta) {
            ret.WriteBool(true);
            ret.WriteSU(delta, 7);
          } else {
            ret.WriteBool(false);
          }
        }
        ret.WriteBool(pic_hdr.update_mode_delta);
        for (const auto& delta : pic_hdr.loop_filter_mode_deltas) {
          if (delta) {
            ret.WriteBool(true);
            ret.WriteSU(delta, 7);
          } else {
            ret.WriteBool(false);
          }
        }
      }
    }

    // Pack CDEF parameters. Refer to AV1 spec section 5.9.19.
    if (seq_hdr.enable_cdef) {
      ret.Write(pic_hdr.cdef_damping_minus_3, 2);
      ret.Write(pic_hdr.cdef_bits, 2);
      for (uint32_t i = 0; i < (1 << pic_hdr.cdef_bits); i++) {
        ret.Write(pic_hdr.cdef_y_pri_strength[i], 4);
        ret.Write(pic_hdr.cdef_y_sec_strength[i], 2);
        ret.Write(pic_hdr.cdef_uv_pri_strength[i], 4);
        ret.Write(pic_hdr.cdef_uv_sec_strength[i], 2);
      }
    }

    // Pack loop restoration filter parameters. Refer to AV1 spec
    // section 5.9.20.
    if (seq_hdr.enable_restoration) {
      constexpr int kNumPlanes = 3;
      bool use_lr = false;
      bool use_chroma_lr = false;
      for (int i = 0; i < kNumPlanes; i++) {
        ret.Write(pic_hdr.restoration_type[i], 2);
        if (pic_hdr.restoration_type[i] !=
            libgav1::LoopRestorationType::kLoopRestorationTypeNone) {
          use_lr = true;
          if (i > 0) {
            use_chroma_lr = true;
          }
        }
      }
      if (use_lr) {
        uint8_t lr_unit_shift = pic_hdr.lr_unit_shift;
        if (seq_hdr.use_128x128_superblock) {
          ret.WriteBool(lr_unit_shift > 0);
        } else {
          ret.WriteBool(lr_unit_shift > 0);
          if (lr_unit_shift) {
            ret.WriteBool(lr_unit_shift > 1);
          }
        }

        if (use_chroma_lr) {
          ret.WriteBool(!!pic_hdr.lr_uv_shift);
        }
      }
    }
  }

  // TX mode syntax. Refer to AV1 spec section 5.9.21
  ret.WriteBool(pic_hdr.tx_mode == libgav1::TxMode::kTxModeSelect);

  // Skip mode parameters are not present, as the encoder will only enable
  // single prediction. Refer to AV1 spec section 5.9.22.

  // Frame reference mode. Refer to AV1 spec section 5.9.23.
  if (pic_hdr.frame_type != libgav1::FrameType::kFrameKey) {
    ret.WriteBool(pic_hdr.reference_select);
  }
  ret.WriteBool(pic_hdr.reduced_tx_set);

  // Global motion parameters. Refer to AV1 spec section 5.9.24.
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
                                         bool has_size,
                                         bool extension_flag,
                                         std::optional<uint8_t> temporal_id) {
  DCHECK_LE(1, type);
  DCHECK_LE(type, 8);
  WriteBool(false);  // forbidden bit must be set to 0.
  Write(static_cast<uint64_t>(type), 4);
  WriteBool(extension_flag);
  WriteBool(has_size);
  WriteBool(false);  // reserved bit must be set to 0.
  if (extension_flag) {
    CHECK(temporal_id.has_value());
    Write(temporal_id.value(), 3);
    Write(0, 2);  // spatial layer must be zero.
    Write(0, 3);  // reserved bits must be set to 0.
  }
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

void AV1BitstreamBuilder::WriteSU(int16_t value, size_t num_bits) {
  // Encode a signed integer in SU(num_bits) format.
  // See section 4.10.6 of the AV1 specification.
  Write(value & ((1 << num_bits) - 1), num_bits);
}

void AV1BitstreamBuilder::AppendBitstreamBuffer(AV1BitstreamBuilder buffer) {
  queued_writes_.insert(queued_writes_.end(),
                        std::make_move_iterator(buffer.queued_writes_.begin()),
                        std::make_move_iterator(buffer.queued_writes_.end()));
  total_outstanding_bits_ += buffer.total_outstanding_bits_;
}

}  // namespace media
