// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/av1_builder.h"

#include <algorithm>
#include <iterator>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/numerics/safe_conversions.h"
#include "third_party/libgav1/src/src/obu_parser.h"

namespace media {

namespace {
constexpr int kPrimaryReferenceNone = 7;
// Spec 6.10.2 REFS_PER_FRAME, and NUM_REF_FRAMES for the DPB.
constexpr int kRefsPerFrame = 7;
constexpr int kNumRefFrames = 8;
constexpr uint8_t kAllFrames = 0xFF;

// Spec 5.9.2, `FrameIsIntra` deduction in the uncompressed header syntax.
bool IsIntraFrame(libgav1::FrameType frame_type) {
  return frame_type == libgav1::FrameType::kFrameKey ||
         frame_type == libgav1::FrameType::kFrameIntraOnly;
}

// OrderHintBits. Spec 5.5.1. order_hint_bits_minus_1 is coded as f(3), mask so
// the decoder can only ever see the low three bits.
int OrderHintBits(const AV1BitstreamBuilder::SequenceHeader& seq_hdr) {
  return (seq_hdr.order_hint_bits_minus_1 & 0x7) + 1;
}

// get_relative_dist(). Spec 5.9.3.
int GetRelativeDist(uint32_t a,
                    uint32_t b,
                    const AV1BitstreamBuilder::SequenceHeader& seq_hdr) {
  if (!seq_hdr.enable_order_hint) {
    return 0;
  }

  const int order_hint_bits = OrderHintBits(seq_hdr);
  int diff = static_cast<int>(a) - static_cast<int>(b);
  const int m = 1 << (order_hint_bits - 1);
  diff = (diff & (m - 1)) - (diff & m);
  return diff;
}

// Spec 5.9.22. ref_frame_idx is coded as f(3), so the mask only keeps the
// caller-supplied value in range.
uint32_t RefOrderHint(const AV1BitstreamBuilder::FrameHeader& pic_hdr, int i) {
  return pic_hdr.ref_order_hint[pic_hdr.ref_frame_idx[i] & (kNumRefFrames - 1)];
}

// skipModeAllowed. Spec 5.9.22. The encoder never turns skip mode on, but
// whether the *bit* is present is decided by the reference structure, so this
// has to be evaluated or every syntax element after it shifts by one.
bool IsSkipModeAllowed(const AV1BitstreamBuilder::SequenceHeader& seq_hdr,
                       const AV1BitstreamBuilder::FrameHeader& pic_hdr) {
  if (IsIntraFrame(pic_hdr.frame_type) || !pic_hdr.reference_select ||
      !seq_hdr.enable_order_hint) {
    return false;
  }

  int forward_idx = -1;
  int backward_idx = -1;
  uint32_t forward_hint = 0;
  uint32_t backward_hint = 0;
  for (int i = 0; i < kRefsPerFrame; ++i) {
    const uint32_t ref_hint = RefOrderHint(pic_hdr, i);
    if (GetRelativeDist(ref_hint, pic_hdr.order_hint, seq_hdr) < 0) {
      if (forward_idx < 0 ||
          GetRelativeDist(ref_hint, forward_hint, seq_hdr) > 0) {
        forward_idx = i;
        forward_hint = ref_hint;
      }
    } else if (GetRelativeDist(ref_hint, pic_hdr.order_hint, seq_hdr) > 0) {
      if (backward_idx < 0 ||
          GetRelativeDist(ref_hint, backward_hint, seq_hdr) < 0) {
        backward_idx = i;
        backward_hint = ref_hint;
      }
    }
  }

  if (forward_idx < 0) {
    return false;
  }
  if (backward_idx >= 0) {
    return true;
  }
  // No backward reference, so look for a second, further out forward one.
  int second_forward_idx = -1;
  uint32_t second_forward_hint = 0;
  for (int i = 0; i < kRefsPerFrame; ++i) {
    const uint32_t ref_hint = RefOrderHint(pic_hdr, i);
    if (GetRelativeDist(ref_hint, forward_hint, seq_hdr) < 0) {
      if (second_forward_idx < 0 ||
          GetRelativeDist(ref_hint, second_forward_hint, seq_hdr) > 0) {
        second_forward_idx = i;
        second_forward_hint = ref_hint;
      }
    }
  }
  return second_forward_idx >= 0;
}

// CodedLossless. Spec 5.9.2, using get_qindex() with ignoreDeltaQ = 1 from
// spec 7.12.2.
//
// Limitation: when segmentation_update_data is 0 the decoder inherits the
// feature data from the primary reference frame rather than reading it here,
// and the builder has no view of that. Callers that enable segmentation
// without updating the data must keep FrameHeader::feature_* in sync with what
// the reference carries.
bool IsCodedLossless(const AV1BitstreamBuilder::SequenceHeader& seq_hdr,
                     const AV1BitstreamBuilder::FrameHeader& pic_hdr) {
  // The V component's delta Q values is read from driver post encode. As a
  // result, when separate_uv_delta_q is 0, we should use the U component's
  // delta Q values instead, to protect against drivers that return a random
  // delta Q for V.
  const int8_t delta_q_v_dc =
      seq_hdr.separate_uv_delta_q ? pic_hdr.delta_q_v_dc : pic_hdr.delta_q_u_dc;
  const int8_t delta_q_v_ac =
      seq_hdr.separate_uv_delta_q ? pic_hdr.delta_q_v_ac : pic_hdr.delta_q_u_ac;
  if (pic_hdr.delta_q_y_dc || pic_hdr.delta_q_u_dc || pic_hdr.delta_q_u_ac ||
      delta_q_v_dc || delta_q_v_ac) {
    return false;
  }
  // base_qindex is written as f(8); compare what the decoder will see.
  const int base_qindex = static_cast<int>(pic_hdr.base_qindex & 0xFF);
  for (size_t segment_id = 0; segment_id < libgav1::kMaxSegments;
       ++segment_id) {
    const auto& enabled = pic_hdr.feature_enabled[segment_id];
    const auto& data = pic_hdr.feature_data[segment_id];
    int qindex = base_qindex;
    if (pic_hdr.segmentation_enabled &&
        enabled[libgav1::kSegmentFeatureQuantizer]) {
      qindex =
          std::clamp(qindex + data[libgav1::kSegmentFeatureQuantizer], 0, 255);
    }
    if (qindex != 0) {
      return false;
    }
  }
  return true;
}

// Spec 5.9.13, the reverse process of read_delta_q().
void WriteDeltaQ(AV1BitstreamBuilder& builder, int8_t delta_q) {
  if (delta_q) {
    builder.WriteBool(true);
    builder.WriteSU(delta_q, 7);
  } else {
    builder.WriteBool(false);
  }
}

// frame_size() followed by render_size(). Spec 5.9.5, 5.9.6 and 5.9.8.
void WriteFrameAndRenderSize(AV1BitstreamBuilder& builder,
                             const AV1BitstreamBuilder::SequenceHeader& seq_hdr,
                             bool frame_size_override_flag) {
  if (frame_size_override_flag) {
    builder.Write(seq_hdr.width - 1, seq_hdr.frame_width_bits_minus_1 + 1);
    builder.Write(seq_hdr.height - 1, seq_hdr.frame_height_bits_minus_1 + 1);
  }
  if (seq_hdr.enable_superres) {
    builder.WriteBool(false);  // use_superres.
  }
  builder.WriteBool(false);  // render_and_frame_size_different.
}

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
  const bool high_bitdepth = seq_hdr.bit_depth > 8;
  ret.WriteBool(high_bitdepth);
  if (seq_hdr.profile == libgav1::BitstreamProfile::kProfile2 &&
      high_bitdepth) {
    ret.WriteBool(seq_hdr.bit_depth == 12);
  }
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
  // Chroma subsampling is implied by the profile, except for 12 bit profile 2
  // where it is signalled explicitly. We only ever emit 4:2:0 in that case.
  bool subsampling_x = true;
  bool subsampling_y = true;
  if (seq_hdr.profile == libgav1::BitstreamProfile::kProfile1) {
    // 4:4:4.
    subsampling_x = false;
    subsampling_y = false;
  } else if (seq_hdr.profile == libgav1::BitstreamProfile::kProfile2) {
    if (seq_hdr.bit_depth == 12) {
      ret.WriteBool(subsampling_x);
      ret.WriteBool(subsampling_y);
    } else {
      // 4:2:2.
      subsampling_y = false;
    }
  }
  if (subsampling_x && subsampling_y) {
    ret.Write(seq_hdr.chroma_sample_position, 2);
  }

  ret.WriteBool(seq_hdr.separate_uv_delta_q);
  ret.WriteBool(false);  // No film grain parameters present.

  ret.PutTrailingBits();
  return ret;
}

AV1BitstreamBuilder AV1BitstreamBuilder::BuildFrameHeaderOBU(
    const SequenceHeader& seq_hdr,
    const FrameHeader& pic_hdr) {
  AV1BitstreamBuilder ret;

  const bool frame_is_intra = IsIntraFrame(pic_hdr.frame_type);
  // show_frame is always written as 1 below, so for key and switch frames both
  // error_resilient_mode and refresh_frame_flags are implied rather than
  // coded. See spec 5.9.2.
  const bool implied_by_frame_type =
      pic_hdr.frame_type == libgav1::FrameType::kFrameSwitch ||
      pic_hdr.frame_type == libgav1::FrameType::kFrameKey;
  const bool error_resilient_mode =
      implied_by_frame_type || pic_hdr.error_resilient_mode;
  const uint8_t refresh_frame_flags =
      implied_by_frame_type ? kAllFrames : pic_hdr.refresh_frame_flags;
  // delta_q_present is only coded when base_q_idx is non-zero, so it is
  // inferred to 0 otherwise no matter what the caller asked for. Spec 5.9.17.
  const bool delta_q_present =
      pic_hdr.base_qindex > 0 && pic_hdr.delta_q_present;
  // Inferred to 1 for switch frames, coded as 0 for everything else, so
  // frame_size_with_refs() is never reached. Spec 5.9.2 and 5.9.5.
  const bool frame_size_override_flag =
      pic_hdr.frame_type == libgav1::FrameType::kFrameSwitch;
  const bool coded_lossless = IsCodedLossless(seq_hdr, pic_hdr);
  // AllLossless = CodedLossless && FrameWidth == UpscaledWidth. use_superres is
  // always written as 0, so UpscaledWidth == FrameWidth and the two are the
  // same. Spec 5.9.2.
  const bool all_lossless = coded_lossless;

  ret.WriteBool(false);  // For a frame OBU, the show_existing_frame flag is
                         // always set to 0.
  ret.Write(pic_hdr.frame_type, 2);
  ret.WriteBool(true);  // If this frame needs to be immediately output once
                        // decoded, show_frame flag should be true.
  if (!implied_by_frame_type) {
    ret.WriteBool(pic_hdr.error_resilient_mode);
  }
  ret.WriteBool(pic_hdr.disable_cdf_update);
  ret.WriteBool(pic_hdr.allow_screen_content_tools);
  // force_integer_mv is not coded: the sequence header writes
  // seq_choose_integer_mv = 0 and seq_force_integer_mv = 0, so it is never
  // SELECT_INTEGER_MV. Spec 5.9.2.
  if (!frame_size_override_flag) {
    ret.WriteBool(false);  // frame_size_override_flag.
  }
  if (seq_hdr.enable_order_hint) {
    ret.Write(pic_hdr.order_hint, OrderHintBits(seq_hdr));
  }

  if (!frame_is_intra && !error_resilient_mode) {
    ret.Write(pic_hdr.primary_ref_frame, 3);
  }
  if (!implied_by_frame_type) {
    ret.Write(refresh_frame_flags, 8);
  }
  if ((!frame_is_intra || refresh_frame_flags != kAllFrames) &&
      error_resilient_mode && seq_hdr.enable_order_hint) {
    // Set order hint for each reference frame.
    for (uint32_t order_hint : pic_hdr.ref_order_hint) {
      ret.Write(order_hint, OrderHintBits(seq_hdr));
    }
  }

  if (frame_is_intra) {
    // Spec 5.9.2 requires an intra only frame not to refresh every slot;
    // doing so would make the stream non-conformant.
    DCHECK(pic_hdr.frame_type != libgav1::FrameType::kFrameIntraOnly ||
           refresh_frame_flags != kAllFrames);
    WriteFrameAndRenderSize(ret, seq_hdr, frame_size_override_flag);
    // UpscaledWidth == FrameWidth because superres is never signalled, so the
    // second half of the spec condition is always true.
    if (pic_hdr.allow_screen_content_tools) {
      ret.WriteBool(pic_hdr.allow_intrabc);
    }
  } else {
    if (seq_hdr.enable_order_hint) {
      ret.WriteBool(false);  // Disable frame reference short signaling.
    }
    for (uint8_t ref_idx : pic_hdr.ref_frame_idx) {
      ret.Write(ref_idx, 3);
    }
    // frame_size_with_refs() is unreachable: it needs frame_size_override_flag
    // with error_resilient_mode clear, and the only frame type that sets the
    // former infers the latter to 1. Spec 5.9.2.
    WriteFrameAndRenderSize(ret, seq_hdr, frame_size_override_flag);
    ret.WriteBool(false);  // No allow high precision MV.
    bool is_switchable_interp =
        pic_hdr.interpolation_filter ==
        libgav1::InterpolationFilter::kInterpolationFilterSwitchable;
    ret.WriteBool(is_switchable_interp);
    if (!is_switchable_interp) {
      ret.Write(pic_hdr.interpolation_filter, 2);
    }
    ret.WriteBool(false);  // Motion not switchable.
    if (!error_resilient_mode && seq_hdr.enable_ref_frame_mvs) {
      ret.WriteBool(false);  // Do not use ref frame MVs.
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
  WriteDeltaQ(ret, pic_hdr.delta_q_y_dc);
  // NumPlanes is always 3: mono_chrome is never signalled. Only diff_uv_delta
  // is gated on separate_uv_delta_q - the U deltas are read whenever there is
  // chroma. Spec 5.9.12.
  bool diff_uv_delta = false;
  if (seq_hdr.separate_uv_delta_q) {
    diff_uv_delta = pic_hdr.delta_q_u_dc != pic_hdr.delta_q_v_dc ||
                    pic_hdr.delta_q_u_ac != pic_hdr.delta_q_v_ac;
    ret.WriteBool(diff_uv_delta);
  }
  WriteDeltaQ(ret, pic_hdr.delta_q_u_dc);
  WriteDeltaQ(ret, pic_hdr.delta_q_u_ac);
  if (diff_uv_delta) {
    WriteDeltaQ(ret, pic_hdr.delta_q_v_dc);
    WriteDeltaQ(ret, pic_hdr.delta_q_v_ac);
  }
  ret.WriteBool(pic_hdr.using_qmatrix);
  if (pic_hdr.using_qmatrix) {
    ret.Write(pic_hdr.qm_y, 4);
    ret.Write(pic_hdr.qm_u, 4);
    if (seq_hdr.separate_uv_delta_q) {
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
    ret.WriteBool(delta_q_present);
    if (delta_q_present) {
      ret.Write(pic_hdr.delta_q_res, 2);
    }
  }

  // Pack loop filter delta params. Refer to AV1 spec section 5.9.18.
  if (delta_q_present && !pic_hdr.allow_intrabc) {
    ret.WriteBool(pic_hdr.delta_lf_present);
    if (pic_hdr.delta_lf_present) {
      ret.Write(pic_hdr.delta_lf_res, 2);
      ret.WriteBool(pic_hdr.delta_lf_multi);
    }
  }

  // When the frame is coded losslessly the loop filter, CDEF and tx mode
  // syntax are all absent - the decoder infers them. Loop restoration keys
  // off AllLossless instead. Spec 5.9.11, 5.9.19, 5.9.20 and 5.9.21.
  if (!coded_lossless && !pic_hdr.allow_intrabc) {
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
  }

  // Pack loop restoration filter parameters. Refer to AV1 spec section 5.9.20.
  if (!all_lossless && !pic_hdr.allow_intrabc) {
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

  // TX mode syntax. Refer to AV1 spec section 5.9.21. TxMode is inferred to be
  // ONLY_4X4 when the frame is coded losslessly, so no bit is written.
  if (!coded_lossless) {
    ret.WriteBool(pic_hdr.tx_mode == libgav1::TxMode::kTxModeSelect);
  }

  // Frame reference mode. Refer to AV1 spec section 5.9.23.
  if (!frame_is_intra) {
    ret.WriteBool(pic_hdr.reference_select);
  }

  // Skip mode parameters. Refer to AV1 spec section 5.9.22. The encoder never
  // turns skip mode on, but whether the bit exists at all is decided by the
  // reference structure, so it has to be written whenever skipModeAllowed.
  if (IsSkipModeAllowed(seq_hdr, pic_hdr)) {
    ret.WriteBool(false);  // skip_mode_present.
  }

  // Refer to AV1 spec section 5.9.2. The encoder does not use warped motion,
  // but the bit is present whenever the sequence enables it on an inter frame.
  if (!frame_is_intra && !error_resilient_mode &&
      seq_hdr.enable_warped_motion) {
    ret.WriteBool(false);  // allow_warped_motion.
  }

  ret.WriteBool(pic_hdr.reduced_tx_set);

  // Global motion parameters. Refer to AV1 spec section 5.9.24.
  if (!frame_is_intra) {
    for (int i = 1 /*LAST_FRAME*/; i <= 7 /*ALTREF_FRAME*/; i++) {
      ret.WriteBool(false);  // Set is_global to all zeros.
    }
  }

  ret.PutAlignBits();
  return ret;
}

AV1BitstreamBuilder AV1BitstreamBuilder::BuildHDRCLLMetadataOBU(
    const Libgav1ObuMetadataHdrCll& hdr_cll) {
  AV1BitstreamBuilder ret;
  ret.WriteValueInLeb128(libgav1::kMetadataTypeHdrContentLightLevel);
  ret.Write(hdr_cll.max_cll, 16);
  ret.Write(hdr_cll.max_fall, 16);
  // A metadata OBU is neither a tile group, tile list nor frame OBU, so it
  // carries trailing bits. Refer to AV1 spec section 5.3.1.
  ret.PutTrailingBits();
  return ret;
}

AV1BitstreamBuilder AV1BitstreamBuilder::BuildHDRMDCVMetadataOBU(
    const Libgav1ObuMetadataHdrMdcv& hdr_mdcv) {
  AV1BitstreamBuilder ret;
  ret.WriteValueInLeb128(libgav1::kMetadataTypeHdrMasteringDisplayColorVolume);
  for (size_t i = 0; i < std::size(hdr_mdcv.primary_chromaticity_x); i++) {
    ret.Write(base::span(hdr_mdcv.primary_chromaticity_x)[i], 16);
    ret.Write(base::span(hdr_mdcv.primary_chromaticity_y)[i], 16);
  }
  ret.Write(hdr_mdcv.white_point_chromaticity_x, 16);
  ret.Write(hdr_mdcv.white_point_chromaticity_y, 16);
  ret.Write(hdr_mdcv.luminance_max, 32);
  ret.Write(hdr_mdcv.luminance_min, 32);
  ret.PutTrailingBits();
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
  const int num_bytes = fixed_size.value_or(5);
  DCHECK_GT(num_bytes, 0);
  // Spec 4.10.5 reads at most 8 bytes, and requires the eighth to terminate,
  // so anything longer cannot be parsed back.
  DCHECK_LE(num_bytes, 8);
  for (int i = 0; i < num_bytes; i++) {
    uint8_t curr_byte = value & 0x7F;
    value >>= 7;
    // The parse loop in spec 4.10.5 stops at the first byte whose most
    // significant bit is clear, so only the bytes *before* the last one may
    // set it. Padding a short value out to a fixed size is explicitly allowed
    // by that section, but the padding still has to terminate.
    const bool more_bytes = fixed_size ? i < num_bytes - 1 : value != 0;
    if (more_bytes) {
      curr_byte |= 0x80;
    }
    Write(curr_byte, 8);
    if (!more_bytes) {
      break;
    }
  }
  DCHECK_EQ(value, 0u) << "value does not fit in " << num_bytes << " bytes";
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
