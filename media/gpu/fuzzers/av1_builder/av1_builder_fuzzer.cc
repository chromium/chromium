// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/av1_builder.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "base/numerics/safe_conversions.h"
#include "media/gpu/fuzzers/av1_builder/av1_builder_fuzzer_inputs.pb.h"
#include "testing/libfuzzer/proto/lpm_interface.h"

namespace {

media::AV1BitstreamBuilder::SequenceHeader ConvertToAV1BuilderSequenceHeader(
    const media::fuzzing::AV1SequenceHeader& sequence_header) {
  media::AV1BitstreamBuilder::SequenceHeader seq_hdr{};
  seq_hdr.profile = sequence_header.profile();
  // The `operating_points_cnt_minus_1` is always provided by VEA as 0, not
  // driver, so we clamp to `kMaxTemporalLayerNum - 1` if input is larger than
  // that, to avoid CHECK failure in av1_builder.
  seq_hdr.operating_points_cnt_minus_1 =
      std::min(sequence_header.operating_points_cnt_minus_1(),
               media::AV1BitstreamBuilder::kMaxTemporalLayerNum - 1);
  const size_t level_size =
      std::min(static_cast<size_t>(sequence_header.level_size()),
               std::size(seq_hdr.level));
  for (size_t i = 0; i < level_size; ++i) {
    seq_hdr.level[i] = sequence_header.level(i);
  }
  const size_t tier_size =
      std::min(static_cast<size_t>(sequence_header.tier_size()),
               std::size(seq_hdr.tier));
  for (size_t i = 0; i < tier_size; ++i) {
    seq_hdr.tier[i] = sequence_header.tier(i);
  }
  seq_hdr.frame_width_bits_minus_1 = sequence_header.frame_width_bits_minus_1();
  seq_hdr.frame_height_bits_minus_1 =
      sequence_header.frame_height_bits_minus_1();
  seq_hdr.width = sequence_header.width();
  seq_hdr.height = sequence_header.height();
  seq_hdr.use_128x128_superblock = sequence_header.use_128x128_superblock();
  seq_hdr.enable_filter_intra = sequence_header.enable_filter_intra();
  seq_hdr.enable_intra_edge_filter = sequence_header.enable_intra_edge_filter();
  seq_hdr.enable_interintra_compound =
      sequence_header.enable_interintra_compound();
  seq_hdr.enable_masked_compound = sequence_header.enable_masked_compound();
  seq_hdr.enable_warped_motion = sequence_header.enable_warped_motion();
  seq_hdr.enable_dual_filter = sequence_header.enable_dual_filter();
  seq_hdr.enable_order_hint = sequence_header.enable_order_hint();
  seq_hdr.enable_jnt_comp = sequence_header.enable_jnt_comp();
  seq_hdr.enable_ref_frame_mvs = sequence_header.enable_ref_frame_mvs();
  seq_hdr.order_hint_bits_minus_1 = sequence_header.order_hint_bits_minus_1();
  seq_hdr.enable_superres = sequence_header.enable_superres();
  seq_hdr.enable_cdef = sequence_header.enable_cdef();
  seq_hdr.enable_restoration = sequence_header.enable_restoration();

  return seq_hdr;
}

libgav1::FrameType ConvertToFrameType(media::fuzzing::FrameType frame_type) {
  switch (frame_type) {
    case media::fuzzing::FrameType::KEY:
      return libgav1::FrameType::kFrameKey;
    case media::fuzzing::FrameType::INTER:
      return libgav1::FrameType::kFrameInter;
    case media::fuzzing::FrameType::INTRAONLY:
      return libgav1::FrameType::kFrameIntraOnly;
    case media::fuzzing::FrameType::SWITCH:
      return libgav1::FrameType::kFrameSwitch;
  }
}

libgav1::LoopRestorationType ConvertToLoopRestorationType(
    media::fuzzing::LoopRestorationType restoration_type) {
  switch (restoration_type) {
    case media::fuzzing::LoopRestorationType::NONE:
      return libgav1::LoopRestorationType::kLoopRestorationTypeNone;
    case media::fuzzing::LoopRestorationType::SWITCHABLE:
      return libgav1::LoopRestorationType::kLoopRestorationTypeSwitchable;
    case media::fuzzing::LoopRestorationType::WIENER:
      return libgav1::LoopRestorationType::kLoopRestorationTypeWiener;
    case media::fuzzing::LoopRestorationType::SGRPROJ:
      return libgav1::LoopRestorationType::kLoopRestorationTypeSgrProj;
  }
}

media::AV1BitstreamBuilder::FrameHeader ConvertToAV1BuilderFrameHeader(
    const media::fuzzing::AV1FrameHeader& frame_header) {
  media::AV1BitstreamBuilder::FrameHeader pic_hdr{};
  pic_hdr.frame_type = ConvertToFrameType(frame_header.frame_type());
  pic_hdr.error_resilient_mode = frame_header.error_resilient_mode();
  pic_hdr.disable_cdf_update = frame_header.disable_cdf_update();

  pic_hdr.base_qindex = frame_header.base_qindex();
  pic_hdr.separate_uv_delta_q = frame_header.separate_uv_delta_q();
  pic_hdr.delta_q_y_dc = frame_header.delta_q_y_dc();
  pic_hdr.delta_q_u_dc = frame_header.delta_q_u_dc();
  pic_hdr.delta_q_u_ac = frame_header.delta_q_u_ac();
  pic_hdr.delta_q_v_dc = frame_header.delta_q_v_dc();
  pic_hdr.delta_q_v_ac = frame_header.delta_q_v_ac();
  pic_hdr.using_qmatrix = frame_header.using_qmatrix();
  pic_hdr.qm_y = frame_header.qm_y();
  pic_hdr.qm_u = frame_header.qm_u();
  pic_hdr.qm_v = frame_header.qm_v();

  pic_hdr.order_hint = frame_header.order_hint();

  const size_t tier_size =
      std::min(static_cast<size_t>(frame_header.filter_level_size()),
               std::size(pic_hdr.filter_level));
  for (size_t i = 0; i < tier_size; ++i) {
    pic_hdr.filter_level[i] = frame_header.filter_level(i);
  }
  pic_hdr.filter_level_u = frame_header.filter_level_u();
  pic_hdr.filter_level_v = frame_header.filter_level_v();
  pic_hdr.sharpness_level = frame_header.sharpness_level();
  pic_hdr.loop_filter_delta_enabled = frame_header.loop_filter_delta_enabled();
  pic_hdr.loop_filter_delta_update = frame_header.loop_filter_delta_update();
  pic_hdr.update_ref_delta = frame_header.update_ref_delta();

  const size_t ref_deltas_size =
      std::min(static_cast<size_t>(frame_header.loop_filter_ref_deltas_size()),
               std::size(pic_hdr.loop_filter_ref_deltas));
  for (size_t i = 0; i < ref_deltas_size; ++i) {
    pic_hdr.loop_filter_ref_deltas[i] = frame_header.loop_filter_ref_deltas(i);
  }
  pic_hdr.update_mode_delta = frame_header.update_mode_delta();
  const size_t mode_deltas_size =
      std::min(static_cast<size_t>(frame_header.loop_filter_mode_deltas_size()),
               std::size(pic_hdr.loop_filter_mode_deltas));
  for (size_t i = 0; i < mode_deltas_size; ++i) {
    pic_hdr.loop_filter_mode_deltas[i] =
        frame_header.loop_filter_mode_deltas(i);
  }
  pic_hdr.delta_lf_present = frame_header.delta_lf_present();
  pic_hdr.delta_lf_res = frame_header.delta_lf_res();
  pic_hdr.delta_lf_multi = frame_header.delta_lf_multi();
  pic_hdr.delta_q_present = frame_header.delta_q_present();
  pic_hdr.delta_q_res = frame_header.delta_q_res();

  pic_hdr.primary_ref_frame = frame_header.primary_ref_frame();
  pic_hdr.refresh_frame_flags = frame_header.refresh_frame_flags();
  const size_t ref_frame_idx_size =
      std::min(static_cast<size_t>(frame_header.ref_frame_idx_size()),
               std::size(pic_hdr.ref_frame_idx));
  for (size_t i = 0; i < ref_frame_idx_size; ++i) {
    pic_hdr.ref_frame_idx[i] = frame_header.ref_frame_idx(i);
  }
  const size_t ref_order_hint_size =
      std::min(static_cast<size_t>(frame_header.ref_order_hint_size()),
               std::size(pic_hdr.ref_order_hint));
  for (size_t i = 0; i < ref_order_hint_size; ++i) {
    pic_hdr.ref_order_hint[i] = frame_header.ref_order_hint(i);
  }
  const size_t restoration_type_size =
      std::min(static_cast<size_t>(frame_header.restoration_type_size()),
               std::size(pic_hdr.restoration_type));
  for (size_t i = 0; i < restoration_type_size; ++i) {
    pic_hdr.restoration_type[i] =
        ConvertToLoopRestorationType(frame_header.restoration_type(i));
  }
  pic_hdr.segment_number = frame_header.segment_number();
  pic_hdr.feature_data.fill({});
  const size_t feature_data_flat_size =
      std::min(static_cast<size_t>(frame_header.feature_data_size()),
               pic_hdr.feature_data.size() * pic_hdr.feature_data[0].size());
  for (size_t flat_idx = 0; flat_idx < feature_data_flat_size; ++flat_idx) {
    size_t j = flat_idx / pic_hdr.feature_data[0].size();
    size_t i = flat_idx % pic_hdr.feature_data[0].size();
    pic_hdr.feature_data[j][i] = frame_header.feature_data(flat_idx);
  }
  pic_hdr.segmentation_temporal_update =
      frame_header.segmentation_temporal_update();
  pic_hdr.segmentation_update_data = frame_header.segmentation_update_data();
  pic_hdr.segment_number = frame_header.segment_number();

  return pic_hdr;
}

}  // namespace

namespace media {
namespace fuzzing {

DEFINE_PROTO_FUZZER(const AV1FrameOBUList& obu_list) {
  for (const auto& obu : obu_list.frames()) {
    AV1BitstreamBuilder::SequenceHeader seq_hdr =
        ConvertToAV1BuilderSequenceHeader(obu.seq_hdr());

    AV1BitstreamBuilder::FrameHeader pic_hdr =
        ConvertToAV1BuilderFrameHeader(obu.frame_hdr());

    AV1BitstreamBuilder seq_obu =
        AV1BitstreamBuilder::BuildSequenceHeaderOBU(seq_hdr);

    AV1BitstreamBuilder frame_obu =
        AV1BitstreamBuilder::BuildFrameHeaderOBU(seq_hdr, pic_hdr);
  }
}

}  // namespace fuzzing
}  // namespace media
