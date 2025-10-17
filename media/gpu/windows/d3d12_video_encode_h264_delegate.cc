// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d12_video_encode_h264_delegate.h"

#include "base/bits.h"
#include "base/containers/fixed_flat_map.h"
#include "base/strings/stringprintf.h"
#include "media/gpu/gpu_video_encode_accelerator_helpers.h"
#include "media/gpu/h264_builder.h"
#include "media/gpu/h264_rate_control_util.h"
#include "media/gpu/macros.h"
#include "media/gpu/windows/d3d12_video_helpers.h"
#include "media/gpu/windows/format_utils.h"
#include "media/gpu/windows/mf_video_encoder_util.h"

namespace media {

namespace {

using H264LevelIDC = H264SPS::H264LevelIDC;
constexpr auto kD3D12H264LevelToH264LevelIDCMap =
    base::MakeFixedFlatMap<D3D12_VIDEO_ENCODER_LEVELS_H264, uint8_t>({
        {D3D12_VIDEO_ENCODER_LEVELS_H264_1, H264LevelIDC::kLevelIDC1p0},
        {D3D12_VIDEO_ENCODER_LEVELS_H264_1b, H264LevelIDC::kLevelIDC1B},
        {D3D12_VIDEO_ENCODER_LEVELS_H264_11, H264LevelIDC::kLevelIDC1p1},
        {D3D12_VIDEO_ENCODER_LEVELS_H264_12, H264LevelIDC::kLevelIDC1p2},
        {D3D12_VIDEO_ENCODER_LEVELS_H264_13, H264LevelIDC::kLevelIDC1p3},
        {D3D12_VIDEO_ENCODER_LEVELS_H264_2, H264LevelIDC::kLevelIDC2p0},
        {D3D12_VIDEO_ENCODER_LEVELS_H264_21, H264LevelIDC::kLevelIDC2p1},
        {D3D12_VIDEO_ENCODER_LEVELS_H264_22, H264LevelIDC::kLevelIDC2p2},
        {D3D12_VIDEO_ENCODER_LEVELS_H264_3, H264LevelIDC::kLevelIDC3p0},
        {D3D12_VIDEO_ENCODER_LEVELS_H264_31, H264LevelIDC::kLevelIDC3p1},
        {D3D12_VIDEO_ENCODER_LEVELS_H264_32, H264LevelIDC::kLevelIDC3p2},
        {D3D12_VIDEO_ENCODER_LEVELS_H264_4, H264LevelIDC::kLevelIDC4p0},
        {D3D12_VIDEO_ENCODER_LEVELS_H264_41, H264LevelIDC::kLevelIDC4p1},
        {D3D12_VIDEO_ENCODER_LEVELS_H264_42, H264LevelIDC::kLevelIDC4p2},
        {D3D12_VIDEO_ENCODER_LEVELS_H264_5, H264LevelIDC::kLevelIDC5p0},
        {D3D12_VIDEO_ENCODER_LEVELS_H264_51, H264LevelIDC::kLevelIDC5p1},
        {D3D12_VIDEO_ENCODER_LEVELS_H264_52, H264LevelIDC::kLevelIDC5p2},
        {D3D12_VIDEO_ENCODER_LEVELS_H264_6, H264LevelIDC::kLevelIDC6p0},
        {D3D12_VIDEO_ENCODER_LEVELS_H264_61, H264LevelIDC::kLevelIDC6p1},
        {D3D12_VIDEO_ENCODER_LEVELS_H264_62, H264LevelIDC::kLevelIDC6p2},
    });

constexpr auto kVideoCodecProfileToD3D12Profile =
    base::MakeFixedFlatMap<VideoCodecProfile, D3D12_VIDEO_ENCODER_PROFILE_H264>(
        {
            {H264PROFILE_BASELINE, D3D12_VIDEO_ENCODER_PROFILE_H264_MAIN},
            {H264PROFILE_MAIN, D3D12_VIDEO_ENCODER_PROFILE_H264_MAIN},
            {H264PROFILE_HIGH, D3D12_VIDEO_ENCODER_PROFILE_H264_HIGH},
            {H264PROFILE_HIGH10PROFILE,
             D3D12_VIDEO_ENCODER_PROFILE_H264_HIGH_10},
        });

uint8_t D3D12VideoEncoderLevelsH264ToH264LevelIDC(
    D3D12_VIDEO_ENCODER_LEVELS_H264 level) {
  return kD3D12H264LevelToH264LevelIDCMap.at(level);
}

D3D12_VIDEO_ENCODER_LEVELS_H264 H264LevelIDCToD3D12VideoEncoderLevelsH264(
    uint8_t level_idc) {
  for (auto [level, idc] : kD3D12H264LevelToH264LevelIDCMap) {
    if (idc == level_idc) {
      return level;
    }
  }
  NOTREACHED();
}

}  // namespace

D3D12VideoEncodeH264ReferenceFrameManager::
    D3D12VideoEncodeH264ReferenceFrameManager() = default;

D3D12VideoEncodeH264ReferenceFrameManager::
    ~D3D12VideoEncodeH264ReferenceFrameManager() = default;

uint32_t
D3D12VideoEncodeH264ReferenceFrameManager::GetMaxLongTermFrameIndexPlus1()
    const {
  return max_long_term_frame_index_plus1_;
}

std::optional<uint32_t>
D3D12VideoEncodeH264ReferenceFrameManager::GetLongTermReferenceFrameResourceId(
    uint32_t long_term_frame_index) const {
  for (const auto& descriptor : descriptors_) {
    if (descriptor.IsLongTermReference &&
        descriptor.LongTermPictureIdx == long_term_frame_index) {
      return descriptor.ReconstructedPictureResourceIndex;
    }
  }
  return std::nullopt;
}

base::span<D3D12_VIDEO_ENCODER_REFERENCE_PICTURE_DESCRIPTOR_H264>
D3D12VideoEncodeH264ReferenceFrameManager::ToReferencePictureDescriptors() {
  return descriptors_;
}

void D3D12VideoEncodeH264ReferenceFrameManager::
    ProcessMemoryManagementControlOperation(
        const D3D12_VIDEO_ENCODER_PICTURE_CONTROL_CODEC_DATA_H264& pic_params) {
  CHECK(pic_params.adaptive_ref_pic_marking_mode_flag);
  if (pic_params.FrameType == D3D12_VIDEO_ENCODER_FRAME_TYPE_H264_IDR_FRAME) {
    max_long_term_frame_index_plus1_ = 1;
    SetCurrentFrameLongTermReference(pic_params.FrameDecodingOrderNumber,
                                     pic_params.PictureOrderCountNumber, 0);
  } else {
    // SAFETY: Callers should guarantee that |pRefPicMarkingOperationsCommands|
    // contains at least |RefPicMarkingOperationsCommandsCount| elements.
    for (auto& operation : UNSAFE_BUFFERS(
             base::span(pic_params.pRefPicMarkingOperationsCommands,
                        pic_params.RefPicMarkingOperationsCommandsCount))) {
      // Table 7-9 – Memory management control operation
      // (memory_management_control_operation) values
      switch (operation.memory_management_control_operation) {
        case 0:
          // 0 End memory_management_control_operation syntax element loop
          return;
        case 2: {
          // 2 Mark a long-term reference picture as "unused for reference"
          auto resource_id =
              GetLongTermReferenceFrameResourceId(operation.long_term_pic_num);
          CHECK_LT(resource_id.value(), size());
          EraseFrame(resource_id.value());
          descriptors_.erase(
              std::next(descriptors_.begin(), resource_id.value()));
          for (size_t i = resource_id.value(); i < descriptors_.size(); i++) {
            descriptors_[i].ReconstructedPictureResourceIndex = i;
          }
          break;
        }
        case 4:
          // 4 Specify the maximum long-term frame index and mark all long-term
          // reference pictures having long-term frame indices greater than the
          // maximum value as "unused for reference"
          CHECK_LE(operation.max_long_term_frame_idx_plus1, size());
          max_long_term_frame_index_plus1_ =
              operation.max_long_term_frame_idx_plus1;
          break;
        case 5:
          // 5 Mark all reference pictures as "unused for reference" and set the
          // MaxLongTermFrameIdx variable to "no long-term frame indices"
          descriptors_.clear();
          max_long_term_frame_index_plus1_ = 0;
          break;
        case 6:
          // 6 Mark the current picture as "used for long-term reference" and
          // assign a long-term frame index to it
          CHECK_LT(operation.long_term_frame_idx,
                   max_long_term_frame_index_plus1_);
          SetCurrentFrameLongTermReference(pic_params.FrameDecodingOrderNumber,
                                           pic_params.PictureOrderCountNumber,
                                           operation.long_term_frame_idx);
          break;
        default:
          // memory_management_control_operation being 1 and 3 is not used.
          // 1 Mark a short-term reference picture as "unused for reference"
          // 3 Mark a short-term reference picture as "used for long-term
          // reference" and assign a long-term frame index to it
          NOTREACHED();
      }
    }
    NOTREACHED() << "RefPicMarkingOperations must end with "
                    "memory_management_control_operation = 0";
  }
}

void D3D12VideoEncodeH264ReferenceFrameManager::
    SetCurrentFrameLongTermReference(uint32_t frame_num,
                                     uint32_t pic_order_cnt,
                                     uint32_t long_term_frame_index) {
  CHECK_LT(long_term_frame_index, size());
  for (auto& descriptor : descriptors_) {
    if (descriptor.IsLongTermReference &&
        descriptor.LongTermPictureIdx == long_term_frame_index) {
      ReplaceWithCurrentFrame(descriptor.ReconstructedPictureResourceIndex);
      descriptor.FrameDecodingOrderNumber = frame_num;
      descriptor.PictureOrderCountNumber = pic_order_cnt;
      return;
    }
  }

  CHECK_LT(descriptors_.size(), size());
  InsertCurrentFrame(descriptors_.size());
  descriptors_.push_back({
      .ReconstructedPictureResourceIndex =
          static_cast<UINT>(descriptors_.size()),
      .IsLongTermReference = true,
      .LongTermPictureIdx = long_term_frame_index,
      .PictureOrderCountNumber = pic_order_cnt,
      .FrameDecodingOrderNumber = frame_num,
  });
}

// static
std::vector<std::pair<VideoCodecProfile, std::vector<VideoPixelFormat>>>
D3D12VideoEncodeH264Delegate::GetSupportedProfiles(
    ID3D12VideoDevice3* video_device) {
  CHECK(video_device);
  std::vector<std::pair<VideoCodecProfile, std::vector<VideoPixelFormat>>>
      profiles;
  for (auto [video_codec_profile, h264_profile] :
       kVideoCodecProfileToD3D12Profile) {
    D3D12_VIDEO_ENCODER_LEVELS_H264 min_level;
    D3D12_VIDEO_ENCODER_LEVELS_H264 max_level;
    D3D12_FEATURE_DATA_VIDEO_ENCODER_PROFILE_LEVEL profile_level{
        .Codec = D3D12_VIDEO_ENCODER_CODEC_H264,
        .Profile = {.DataSize = sizeof(h264_profile),
                    .pH264Profile = &h264_profile},
        .MinSupportedLevel = {.DataSize = sizeof(min_level),
                              .pH264LevelSetting = &min_level},
        .MaxSupportedLevel = {.DataSize = sizeof(max_level),
                              .pH264LevelSetting = &max_level},
    };
    if (!CheckD3D12VideoEncoderProfileLevel(video_device, &profile_level)
             .is_ok()) {
      continue;
    }
    std::vector<VideoPixelFormat> formats;
    for (VideoPixelFormat format : {PIXEL_FORMAT_NV12, PIXEL_FORMAT_P010LE}) {
      D3D12_FEATURE_DATA_VIDEO_ENCODER_INPUT_FORMAT input_format{
          .Codec = D3D12_VIDEO_ENCODER_CODEC_H264,
          .Profile = profile_level.Profile,
          .Format = VideoPixelFormatToDxgiFormat(format),
      };
      if (CheckD3D12VideoEncoderInputFormat(video_device, &input_format)
              .is_ok()) {
        formats.push_back(format);
      }
    }
    if (!formats.empty()) {
      profiles.emplace_back(video_codec_profile, formats);
    }
  }
  return profiles;
}

D3D12VideoEncodeH264Delegate::D3D12VideoEncodeH264Delegate(
    Microsoft::WRL::ComPtr<ID3D12VideoDevice3> video_device,
    bool disable_non_reference_frames)
    : D3D12VideoEncodeDelegate(std::move(video_device)),
      disable_non_reference_frames_(disable_non_reference_frames) {
  // We always do add-one before encoding, so we assign them to be -1 to make it
  // start with 0.
  pic_params_.idr_pic_id = -1;
  pic_params_.FrameDecodingOrderNumber = -1;
  pic_params_.adaptive_ref_pic_marking_mode_flag = 1;
  input_arguments_.SequenceControlDesc.CodecGopSequence = {
      .DataSize = sizeof(gop_structure_),
      .pH264GroupOfPictures = &gop_structure_,
  };
  input_arguments_.PictureControlDesc.PictureControlCodecData = {
      .DataSize = sizeof(pic_params_),
      .pH264PicData = &pic_params_,
  };
  metadata_.h264.emplace();
}

D3D12VideoEncodeH264Delegate::~D3D12VideoEncodeH264Delegate() = default;

size_t D3D12VideoEncodeH264Delegate::GetMaxNumOfRefFrames() const {
  return max_num_ref_frames_;
}

size_t D3D12VideoEncodeH264Delegate::GetMaxNumOfManualRefBuffers() const {
  // We should have initialized.
  CHECK_GT(max_num_ref_frames_, 1u);

  // Same as L1Tx modes, we must reserve 1 DPB slot internally for handling
  // frame_num gap.
  if (disable_non_reference_frames_) {
    return max_num_ref_frames_ - 2;
  }
  return max_num_ref_frames_ - 1;
}

bool D3D12VideoEncodeH264Delegate::SupportsRateControlReconfiguration() const {
  return encoder_support_flags_ &
         D3D12_VIDEO_ENCODER_SUPPORT_FLAG_RATE_CONTROL_RECONFIGURATION_AVAILABLE;
}

bool D3D12VideoEncodeH264Delegate::ReportsAverageQp() const {
  return current_rate_control_.GetMode() ==
         D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_CQP;
}

bool D3D12VideoEncodeH264Delegate::UpdateRateControl(
    const VideoBitrateAllocation& bitrate_allocation,
    uint32_t framerate) {
  if (software_rate_controller_) {
    if (bitrate_allocation.GetMode() != Bitrate::Mode::kConstant &&
        bitrate_allocation.GetMode() != Bitrate::Mode::kVariable) {
      return false;
    }

    framerate_ = framerate;
    bitrate_allocation_ = bitrate_allocation;
    if (bitrate_allocation.GetSumBps() == 0) {
      return false;
    }
    float peak_target_ratio =
        1.f * bitrate_allocation.GetPeakBps() / bitrate_allocation.GetSumBps();
    uint32_t sum_bitrate = 0;
    if (framerate != rate_controller_settings_.frame_rate_max) {
      // Frame rate has changed, resetting the rate controller.
      rate_controller_settings_.frame_rate_max = framerate;
      CHECK_GT(framerate, 0u);
      rate_controller_settings_.gop_max_duration =
          base::Seconds((gop_structure_.GOPLength + framerate - 1) / framerate);
      for (size_t i = 0; i < rate_controller_settings_.layer_settings.size();
           i++) {
        H264RateControllerLayerSettings& layer_settings =
            rate_controller_settings_.layer_settings[i];
        sum_bitrate += bitrate_allocation.GetBitrateBps(0, i);
        layer_settings.avg_bitrate = sum_bitrate;
        layer_settings.peak_bitrate =
            bitrate_allocation.GetMode() == Bitrate::Mode::kConstant
                ? sum_bitrate
                : base::saturated_cast<uint32_t>(sum_bitrate *
                                                 peak_target_ratio);
        layer_settings.frame_rate = framerate / static_cast<float>(1u << i);
      }
      software_rate_controller_.emplace(rate_controller_settings_);
    } else {
      // Frame rate has not changed, updating the bitrate.
      for (size_t i = 0; i < GetNumTemporalLayers(); i++) {
        sum_bitrate += bitrate_allocation.GetBitrateBps(0, i);
        software_rate_controller_->temporal_layers(i).SetBufferParameters(
            rate_controller_settings_.layer_settings[i].hrd_buffer_size,
            sum_bitrate,
            bitrate_allocation.GetMode() == Bitrate::Mode::kConstant
                ? sum_bitrate
                : base::saturated_cast<uint32_t>(sum_bitrate *
                                                 peak_target_ratio),
            rate_controller_settings_.ease_hrd_reduction);
      }
    }
    return true;
  }

  return D3D12VideoEncodeDelegate::UpdateRateControl(bitrate_allocation,
                                                     framerate);
}

EncoderStatus D3D12VideoEncodeH264Delegate::EncodeImpl(
    ID3D12Resource* input_frame,
    UINT input_frame_subresource,
    const VideoEncoder::EncodeOptions& options,
    const gfx::ColorSpace& input_color_space) {
  // Filling the |input_arguments_| according to
  // https://github.com/microsoft/DirectX-Specs/blob/master/d3d/D3D12VideoEncoding.md#6120-struct-d3d12_video_encoder_input_arguments

  // Frame type, idr_pic_id, decoding order number, and reference frames.
  if (++pic_params_.FrameDecodingOrderNumber == gop_structure_.GOPLength ||
      options.key_frame) {
    pic_params_.FrameDecodingOrderNumber = 0;
  }
  pic_params_.PictureOrderCountNumber =
      pic_params_.FrameDecodingOrderNumber * 2;
  bool is_keyframe = pic_params_.FrameDecodingOrderNumber == 0;

  absl::InlinedVector<uint8_t, 4> reference_buffers;
  std::optional<uint8_t> update_buffer;
  std::optional<uint8_t> destroy_buffer;
  if (svc_layers_) {
    if (is_keyframe) {
      svc_layers_->Reset();
    }
    SVCLayers::PictureParam pic_param;
    SVCGenericMetadata metadata{};
    svc_layers_->GetPictureParamAndMetadata(pic_param, &metadata);
    metadata_.h264->temporal_idx = metadata.temporal_idx;
    CHECK_LE(pic_param.reference_frame_indices.size(), 1u);
    if (!pic_param.reference_frame_indices.empty()) {
      reference_buffers.push_back(pic_param.reference_frame_indices[0]);
    }
    if (is_keyframe) {
      // SVCLayers returns 0xff for AV1, we only need one at zero slot for H26x.
      pic_param.refresh_frame_flags = 1;
    }
    CHECK_LE(std::popcount(pic_param.refresh_frame_flags), 1);
    if (pic_param.refresh_frame_flags) {
      update_buffer = std::countr_zero(pic_param.refresh_frame_flags);
    }
    // In L1T3 SVC mode, the slot 1 is not used for reference once it is used
    // at the last frame of each 4 frames, so we need to manually remove it.
    // In L1Tx, The slot 0 is always replaced by the current frame when it is
    // referenced for the last time, so we don't need to explicitly remove it.
    if (GetNumTemporalLayers() == 3 &&
        pic_params_.FrameDecodingOrderNumber % 4 == 3) {
      destroy_buffer = 1;
    }
  } else {
    reference_buffers = options.reference_buffers;
    update_buffer = options.update_buffer;
  }

  if (disable_non_reference_frames_) {
    // Currently it is not supported by some hardware to set current frame
    // not-referenced. So we use slot 1 for non-referenced frame and let other
    // long term frames' index added by one if it is not 0.
    for (uint8_t& reference_id : reference_buffers) {
      if (reference_id > 0) {
        ++reference_id;
      }
    }
    if (update_buffer.has_value()) {
      if (update_buffer.value() > 0) {
        update_buffer = update_buffer.value() + 1;
      }
    } else {
      update_buffer = 1;
    }
    if (destroy_buffer.value_or(0) > 0) {
      destroy_buffer = destroy_buffer.value() + 1;
    }
  }

  if (update_buffer.has_value() &&
      update_buffer.value() >= max_num_ref_frames_) {
    return {EncoderStatus::Codes::kBadReferenceBuffer,
            base::StringPrintf("Update buffer index %d is out of range [0, %d)",
                               update_buffer.value(), max_num_ref_frames_)};
  }
  if (destroy_buffer.has_value() &&
      !reference_frame_manager_.GetLongTermReferenceFrameResourceId(
          destroy_buffer.value())) {
    return {EncoderStatus::Codes::kBadReferenceBuffer,
            base::StringPrintf("Destroy buffer index %d is not found",
                               destroy_buffer.value())};
  }

  // at most 5 operations: 4 operations for each reference buffer, 1 operation
  // for ending op-0.
  absl::InlinedVector<
      D3D12_VIDEO_ENCODER_PICTURE_CONTROL_CODEC_DATA_H264_REFERENCE_PICTURE_LIST_MODIFICATION_OPERATION,
      5>
      reordering_flags;
  // at most 4 operations: op-2, op-4, op-6, op-0
  absl::InlinedVector<
      D3D12_VIDEO_ENCODER_PICTURE_CONTROL_CODEC_DATA_H264_REFERENCE_PICTURE_MARKING_OPERATION,
      4>
      mmco;
  if (is_keyframe) {
    H264SPS sps = ToSPS();
    H264PPS pps = ToPPS(sps);
    packed_header_.Reset();
    BuildPackedH264SPS(packed_header_, sps);
    BuildPackedH264PPS(packed_header_, sps, pps);

    input_arguments_.PictureControlDesc.ReferenceFrames = {};
    pic_params_.FrameType = D3D12_VIDEO_ENCODER_FRAME_TYPE_H264_IDR_FRAME;
    ++pic_params_.idr_pic_id;
    pic_params_.ReferenceFramesReconPictureDescriptorsCount = 0;
    pic_params_.pReferenceFramesReconPictureDescriptors = nullptr;
    pic_params_.List0ReferenceFramesCount = 0;
    pic_params_.pList0ReferenceFrames = nullptr;
    pic_params_.List0RefPicModificationsCount = 0;
    pic_params_.pList0RefPicModifications = nullptr;
    // Alternatively, if encoding an IDR frame and setting
    // adaptive_ref_pic_marking_mode_flag = 1, the driver will assume that the
    // client is attempting to set the H264 slice header
    // long_term_reference_flag and will do so in the output bitstream for the
    // EncodeFrame call.
    // https://learn.microsoft.com/en-us/windows/win32/api/d3d12video/ns-d3d12video-d3d12_video_encoder_picture_control_codec_data_h264_reference_picture_marking_operation#remarks
  } else {
    pic_params_.FrameType = D3D12_VIDEO_ENCODER_FRAME_TYPE_H264_P_FRAME;
    for (size_t i = 0; i < reference_buffers.size(); i++) {
      std::optional<uint32_t> descriptor_index =
          reference_frame_manager_.GetLongTermReferenceFrameResourceId(
              reference_buffers[i]);
      if (!descriptor_index.has_value()) {
        return {EncoderStatus::Codes::kBadReferenceBuffer,
                base::StringPrintf(
                    "Long term reference frame index %d is not found",
                    reference_buffers[i])};
      }
      reordering_flags.push_back({.modification_of_pic_nums_idc = 2,
                                  .long_term_pic_num = reference_buffers[i]});
      list0_reference_frames_[i] = descriptor_index.value();
    }
    if (!reordering_flags.empty()) {
      reordering_flags.push_back({.modification_of_pic_nums_idc = 3});
      pic_params_.List0RefPicModificationsCount = reordering_flags.size();
      pic_params_.pList0RefPicModifications = reordering_flags.data();
    } else {
      pic_params_.List0RefPicModificationsCount = 0;
      pic_params_.pList0RefPicModifications = nullptr;
    }
    pic_params_.List0ReferenceFramesCount = reference_buffers.size();
    pic_params_.pList0ReferenceFrames = list0_reference_frames_.data();
    base::span<D3D12_VIDEO_ENCODER_REFERENCE_PICTURE_DESCRIPTOR_H264>
        descriptors = reference_frame_manager_.ToReferencePictureDescriptors();
    pic_params_.ReferenceFramesReconPictureDescriptorsCount =
        descriptors.size();
    pic_params_.pReferenceFramesReconPictureDescriptors = descriptors.data();
    input_arguments_.PictureControlDesc.ReferenceFrames =
        reference_frame_manager_.ToD3D12VideoEncodeReferenceFrames();
    input_arguments_.PictureControlDesc.ReferenceFrames.NumTexture2Ds =
        descriptors.size();
  }
  if (destroy_buffer.has_value()) {
    mmco.push_back({.memory_management_control_operation = 2,
                    .long_term_pic_num = destroy_buffer.value()});
  }
  if (update_buffer.has_value()) {
    if (update_buffer.value() >=
        reference_frame_manager_.GetMaxLongTermFrameIndexPlus1()) {
      mmco.push_back({.memory_management_control_operation = 4,
                      .max_long_term_frame_idx_plus1 =
                          static_cast<UINT>(update_buffer.value()) + 1});
    }
    mmco.push_back({.memory_management_control_operation = 6,
                    .long_term_frame_idx = update_buffer.value()});
  }
  mmco.push_back({.memory_management_control_operation = 0});
  // The adaptive_ref_pic_marking_mode_flag has been set in the constructor.
  pic_params_.pRefPicMarkingOperationsCommands = mmco.data();
  pic_params_.RefPicMarkingOperationsCommandsCount = mmco.size();

  // Rate control.
  int qp = -1;
  if (software_rate_controller_) {
    CHECK(metadata_.h264);
    software_rate_controller_->temporal_layers(metadata_.h264->temporal_idx)
        .ShrinkHRDBuffer(rate_controller_timestamp_);
    if (is_keyframe) {
      software_rate_controller_->EstimateIntraFrameQP(
          rate_controller_timestamp_);
    } else {
      software_rate_controller_->EstimateInterFrameQP(
          metadata_.h264->temporal_idx, rate_controller_timestamp_);
    }
    qp =
        software_rate_controller_->temporal_layers(metadata_.h264->temporal_idx)
            .curr_frame_qp();
  } else if (options.quantizer.has_value()) {
    qp = options.quantizer.value();
  }
  if (qp != -1) {
    CHECK_EQ(input_arguments_.SequenceControlDesc.RateControl.Mode,
             D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_CQP);
    current_rate_control_.SetCQP(
        is_keyframe ? D3D12VideoEncoderRateControl::FrameType::kIntra
                    : D3D12VideoEncoderRateControl::FrameType::kInterPrev,
        qp);
    input_arguments_.SequenceControlDesc.RateControl =
        current_rate_control_.GetD3D12VideoEncoderRateControl();
  } else if (rate_control_ != current_rate_control_) {
    if (rate_control_.GetMode() != current_rate_control_.GetMode()) {
      CHECK(SupportsRateControlReconfiguration());
      input_arguments_.SequenceControlDesc.Flags |=
          D3D12_VIDEO_ENCODER_SEQUENCE_CONTROL_FLAG_RATE_CONTROL_CHANGE;
    }
    current_rate_control_ = rate_control_;
    input_arguments_.SequenceControlDesc.RateControl =
        current_rate_control_.GetD3D12VideoEncoderRateControl();
  }

  // Input and output textures.
  input_arguments_.pInputFrame = input_frame;
  input_arguments_.InputFrameSubresource = input_frame_subresource;
  D3D12_VIDEO_ENCODER_RECONSTRUCTED_PICTURE output_arguments{};
  if (update_buffer) {
    input_arguments_.PictureControlDesc.Flags =
        D3D12_VIDEO_ENCODER_PICTURE_CONTROL_FLAG_USED_AS_REFERENCE_PICTURE;
    D3D12PictureBuffer reconstructed_picture =
        reference_frame_manager_.GetCurrentFrame();
    output_arguments.pReconstructedPicture = reconstructed_picture.resource_;
    output_arguments.ReconstructedPictureSubresource =
        reconstructed_picture.subresource_;
  } else {
    input_arguments_.PictureControlDesc.Flags =
        D3D12_VIDEO_ENCODER_PICTURE_CONTROL_FLAG_NONE;
  }
  EncoderStatus result =
      video_encoder_wrapper_->Encode(input_arguments_, output_arguments);
  if (!result.is_ok()) {
    return result;
  }

  reference_frame_manager_.ProcessMemoryManagementControlOperation(pic_params_);
  if (svc_layers_) {
    svc_layers_->PostEncode(0);
  }

  metadata_.key_frame = is_keyframe;
  metadata_.qp = qp;
  return EncoderStatus::Codes::kOk;
}

EncoderStatus D3D12VideoEncodeH264Delegate::InitializeVideoEncoder(
    const VideoEncodeAccelerator::Config& config) {
  CHECK_EQ(VideoCodecProfileToVideoCodec(config.output_profile),
           VideoCodec::kH264);

  D3D12_VIDEO_ENCODER_CODEC_PICTURE_CONTROL_SUPPORT_H264
  picture_control_support_h264{};
  D3D12_FEATURE_DATA_VIDEO_ENCODER_CODEC_PICTURE_CONTROL_SUPPORT
  picture_control_support{
      .Codec = D3D12_VIDEO_ENCODER_CODEC_H264,
      .Profile = {.DataSize = sizeof(h264_profile_),
                  .pH264Profile = &h264_profile_},
      .PictureSupport = {.DataSize = sizeof(picture_control_support_h264),
                         .pH264Support = &picture_control_support_h264},
  };
  EncoderStatus status = CheckD3D12VideoEncoderCodecPictureControlSupport(
      video_device_.Get(), &picture_control_support);
  if (!status.is_ok()) {
    return status;
  }

  if (picture_control_support_h264.MaxLongTermReferences < 1) {
    return {EncoderStatus::Codes::kEncoderUnsupportedConfig,
            "D3D12VideoEncoder doesn't support long term reference for H264"};
  }

  if (svc_layers_.has_value()) {
    max_num_ref_frames_ = GetNumTemporalLayers() == 3 ? 2 : 1;
    if (disable_non_reference_frames_) {
      // Currently it is not supported by some hardware to set current frame
      // not-referenced. So we add a space for such case.
      ++max_num_ref_frames_;
    }

    // For H.264, when decoder selects to decode "base-layer" frames, there may
    // be frame_num gaps during decoding. Although during encoding we use long
    // references only, we must signal 1 extra DPB slot besides those used by
    // LTRPs, to allow the sliding window picture marking for the non-existing
    // reference pictures.
    ++max_num_ref_frames_;

    if (picture_control_support_h264.MaxDPBCapacity < max_num_ref_frames_) {
      return {EncoderStatus::Codes::kEncoderUnsupportedConfig,
              base::StringPrintf(
                  "D3D12VideoEncoder only support DPB capacity %u, got %u",
                  picture_control_support_h264.MaxDPBCapacity,
                  max_num_ref_frames_)};
    }
  } else {
    if (picture_control_support_h264.MaxDPBCapacity < 2) {
      return {EncoderStatus::Codes::kEncoderUnsupportedConfig,
              base::StringPrintf("D3D12VideoEncoder require DPB size >=2 to "
                                 "support manual reference control, got %u",
                                 picture_control_support_h264.MaxDPBCapacity)};
    }
    max_num_ref_frames_ = picture_control_support_h264.MaxDPBCapacity;
  }

  if ((config.bitrate.mode() == Bitrate::Mode::kConstant ||
       config.bitrate.mode() == Bitrate::Mode::kVariable) &&
      GetNumTemporalLayers() <= h264_rate_control_util::kMaxNumTemporalLayers) {
    constexpr uint32_t kDefaultQp = 26;
    rate_control_ = D3D12VideoEncoderRateControl::CreateCqp(
        kDefaultQp, kDefaultQp, kDefaultQp);
    rate_controller_settings_.content_type = config.content_type;
    rate_controller_settings_.frame_size = config.input_visible_size;
    rate_controller_settings_.frame_rate_max = config.framerate;
    rate_controller_settings_.gop_max_duration = base::Seconds(
        (config.gop_length.value() + config.framerate - 1) / config.framerate);
    rate_controller_settings_.fixed_delta_qp = false;
    rate_controller_settings_.ease_hrd_reduction = false;
    rate_controller_settings_.num_temporal_layers = GetNumTemporalLayers();
    H264RateControllerLayerSettings layer_settings;
    constexpr size_t kHRDBufferSize = 40000;
    layer_settings.hrd_buffer_size = kHRDBufferSize;
    layer_settings.min_qp = kH264MinQuantizer;
    layer_settings.max_qp = kH264MaxQuantizer;
    VideoBitrateAllocation bitrate_allocation =
        AllocateBitrateForDefaultEncoding(config);
    if (bitrate_allocation.GetSumBps() == 0) {
      return {EncoderStatus::Codes::kEncoderUnsupportedConfig,
              "Bitrate is zero"};
    }
    float peak_target_ratio =
        1.f * bitrate_allocation.GetPeakBps() / bitrate_allocation.GetSumBps();
    uint32_t sum_bitrate = 0;
    for (size_t i = 0; i < rate_controller_settings_.num_temporal_layers; i++) {
      sum_bitrate += bitrate_allocation.GetBitrateBps(0, i);
      layer_settings.avg_bitrate = sum_bitrate;
      layer_settings.peak_bitrate =
          config.bitrate.mode() == Bitrate::Mode::kConstant
              ? sum_bitrate
              : base::saturated_cast<uint32_t>(sum_bitrate * peak_target_ratio);
      layer_settings.frame_rate =
          config.framerate / static_cast<float>(1u << i);
      rate_controller_settings_.layer_settings.push_back(layer_settings);
    }
    software_rate_controller_.emplace(rate_controller_settings_);
  }

  D3D12_FEATURE_DATA_VIDEO_ENCODER_CODEC codec{
      .Codec = D3D12_VIDEO_ENCODER_CODEC_H264};
  status = CheckD3D12VideoEncoderCodec(video_device_.Get(), &codec);
  if (!status.is_ok()) {
    return status;
  }

  if (!kVideoCodecProfileToD3D12Profile.contains(config.output_profile)) {
    return {EncoderStatus::Codes::kEncoderUnsupportedProfile,
            base::StringPrintf("D3D12VideoEncoder only support H264 "
                               "baseline/main/high/high10 profile, got %s",
                               GetProfileName(config.output_profile))};
  }

  h264_profile_ = kVideoCodecProfileToD3D12Profile.at(config.output_profile);
  D3D12_VIDEO_ENCODER_LEVELS_H264 min_level;
  D3D12_VIDEO_ENCODER_LEVELS_H264 max_level;
  D3D12_FEATURE_DATA_VIDEO_ENCODER_PROFILE_LEVEL profile_level{
      .Codec = D3D12_VIDEO_ENCODER_CODEC_H264,
      .Profile = {.DataSize = sizeof(h264_profile_),
                  .pH264Profile = &h264_profile_},
      .MinSupportedLevel = {.DataSize = sizeof(min_level),
                            .pH264LevelSetting = &min_level},
      .MaxSupportedLevel = {.DataSize = sizeof(max_level),
                            .pH264LevelSetting = &max_level},
  };
  status =
      CheckD3D12VideoEncoderProfileLevel(video_device_.Get(), &profile_level);
  if (!status.is_ok()) {
    return status;
  }

  if (config.h264_output_level.has_value()) {
    uint8_t output_level_idc = config.h264_output_level.value();
    uint8_t min_level_idc =
        D3D12VideoEncoderLevelsH264ToH264LevelIDC(min_level);
    uint8_t max_level_idc =
        D3D12VideoEncoderLevelsH264ToH264LevelIDC(max_level);
    if (output_level_idc < min_level_idc || output_level_idc > max_level_idc) {
      return {
          EncoderStatus::Codes::kEncoderUnsupportedConfig,
          base::StringPrintf(
              "D3D12VideoEncoder does not support level %d, expected %d to %d",
              config.h264_output_level.value(), min_level_idc, max_level_idc)};
    }
  }

  D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT_H264 config_support_h264;
  D3D12_FEATURE_DATA_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT config_support{
      .Codec = D3D12_VIDEO_ENCODER_CODEC_H264,
      .Profile = {.DataSize = sizeof(h264_profile_),
                  .pH264Profile = &h264_profile_},
      .CodecSupportLimits = {.DataSize = sizeof(config_support_h264),
                             .pH264Support = &config_support_h264},
  };
  status = CheckD3D12VideoEncoderCodecConfigurationSupport(video_device_.Get(),
                                                           &config_support);
  if (!status.is_ok()) {
    return status;
  }

  // Enable entropy_coding_mode_flag when allowed and supported.
  if (config.output_profile != H264PROFILE_BASELINE &&
      config_support_h264.SupportFlags &
          D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT_H264_FLAG_CABAC_ENCODING_SUPPORT) {
    codec_config_h264_.ConfigurationFlags |=
        D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_H264_FLAG_ENABLE_CABAC_ENCODING;
  }

  uint32_t gop_length = config.gop_length.value();
  // The value of log2_max_frame_num_minus4 shall be in the range of 0 to 12,
  // inclusive. See
  // https://learn.microsoft.com/en-us/windows/win32/api/d3d12video/ns-d3d12video-d3d12_video_encoder_sequence_gop_structure_h264
  constexpr uint32_t kMaxGopLength = 1 << (12 + 4);
  if (gop_length > kMaxGopLength) {
    gop_length = kMaxGopLength;
  }
  UCHAR log2_max_frame_num_minus4 =
      std::max(base::bits::Log2Ceiling(gop_length) - 4, 0);
  gop_structure_ = {
      .GOPLength = gop_length,
      .PPicturePeriod = 1,
      .pic_order_cnt_type = 2,
      .log2_max_frame_num_minus4 = log2_max_frame_num_minus4,
      .log2_max_pic_order_cnt_lsb_minus4 = 0,
  };

  D3D12_VIDEO_ENCODER_PROFILE_H264 suggested_profile;
  D3D12_VIDEO_ENCODER_LEVELS_H264 suggested_level;
  D3D12_FEATURE_DATA_VIDEO_ENCODER_RESOLUTION_SUPPORT_LIMITS
  resolution_support_limits;
  D3D12_FEATURE_DATA_VIDEO_ENCODER_SUPPORT support{
      .Codec = D3D12_VIDEO_ENCODER_CODEC_H264,
      .InputFormat = input_format_,
      .CodecConfiguration = {.DataSize = sizeof(codec_config_h264_),
                             .pH264Config = &codec_config_h264_},
      .CodecGopSequence = {.DataSize = sizeof(gop_structure_),
                           .pH264GroupOfPictures = &gop_structure_},
      .RateControl = rate_control_.GetD3D12VideoEncoderRateControl(),
      .IntraRefresh = D3D12_VIDEO_ENCODER_INTRA_REFRESH_MODE_NONE,
      .SubregionFrameEncoding =
          D3D12_VIDEO_ENCODER_FRAME_SUBREGION_LAYOUT_MODE_FULL_FRAME,
      .ResolutionsListCount = 1,
      .pResolutionList = &input_size_,
      .MaxReferenceFramesInDPB = max_num_ref_frames_,
      .SuggestedProfile = {.DataSize = sizeof(suggested_profile),
                           .pH264Profile = &suggested_profile},
      .SuggestedLevel = {.DataSize = sizeof(suggested_level),
                         .pH264LevelSetting = &suggested_level},
      .pResolutionDependentSupport = &resolution_support_limits,
  };
  status = CheckD3D12VideoEncoderSupport(video_device_.Get(), &support);
  if (!status.is_ok()) {
    return status;
  }
  encoder_support_flags_ = support.SupportFlags;

  h264_level_ = config.h264_output_level.has_value()
                    ? H264LevelIDCToD3D12VideoEncoderLevelsH264(
                          config.h264_output_level.value())
                    : suggested_level;
  // According to H.264 spec Annex A.2, only for high profile and above, that we
  // can support adaptive 8x8 transform.
  if (h264_profile_ >= D3D12_VIDEO_ENCODER_PROFILE_H264_HIGH &&
      (config_support_h264.SupportFlags &
       D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT_H264_FLAG_ADAPTIVE_8x8_TRANSFORM_ENCODING_SUPPORT)) {
    codec_config_h264_.ConfigurationFlags |=
        D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_H264_FLAG_USE_ADAPTIVE_8x8_TRANSFORM;
  }

  bool use_texture_array =
      encoder_support_flags_ &
      D3D12_VIDEO_ENCODER_SUPPORT_FLAG_RECONSTRUCTED_FRAMES_REQUIRE_TEXTURE_ARRAYS;
  if (!reference_frame_manager_.InitializeTextureResources(
          device_.Get(), config.input_visible_size, input_format_,
          max_num_ref_frames_, use_texture_array)) {
    return {EncoderStatus::Codes::kEncoderInitializationError,
            "Failed to initialize DPB"};
  }

  video_encoder_wrapper_ = video_encoder_wrapper_factory_.Run(
      video_device_.Get(), D3D12_VIDEO_ENCODER_CODEC_H264,
      {.DataSize = sizeof(h264_profile_), .pH264Profile = &h264_profile_},
      {.DataSize = sizeof(h264_level_), .pH264LevelSetting = &h264_level_},
      input_format_,
      {.DataSize = sizeof(codec_config_h264_),
       .pH264Config = &codec_config_h264_},
      input_size_);
  // We use full frame mode so the number of subregions is always 1.
  if (!video_encoder_wrapper_->Initialize(/*max_subregions_number=*/1)) {
    return EncoderStatus::Codes::kEncoderInitializationError;
  }

  rate_controller_timestamp_ = base::TimeDelta();
  current_rate_control_ = rate_control_;
  input_arguments_.SequenceControlDesc.RateControl =
      current_rate_control_.GetD3D12VideoEncoderRateControl();
  input_arguments_.SequenceControlDesc.PictureTargetResolution = input_size_;
  return EncoderStatus::Codes::kOk;
}

EncoderStatus::Or<size_t> D3D12VideoEncodeH264Delegate::ReadbackBitstream(
    base::span<uint8_t> bitstream_buffer) {
  size_t packed_header_size = packed_header_.BytesInBuffer();
  // The |bitstream_buffer| is from outer shared memory, and the
  // |packed_header_| is created in this class, so they won't overlap.
  bitstream_buffer.first(packed_header_size)
      .copy_from_nonoverlapping(packed_header_.data());
  packed_header_.Reset();
  auto size_or_error = D3D12VideoEncodeDelegate::ReadbackBitstream(
      bitstream_buffer.subspan(packed_header_size));
  if (!size_or_error.has_value()) {
    return std::move(size_or_error).error();
  }
  size_t payload_size = packed_header_size + std::move(size_or_error).value();
  if (software_rate_controller_) {
    // Update the software rate controller here since we do not know the payload
    // size until now.
    if (metadata_.key_frame) {
      software_rate_controller_->FinishIntraFrame(payload_size,
                                                  rate_controller_timestamp_);
    } else {
      software_rate_controller_->FinishInterFrame(
          metadata_.h264 ? metadata_.h264->temporal_idx : 0, payload_size,
          rate_controller_timestamp_);
    }
    // The next frame should be decoded at (1 / frame_rate) seconds later.
    rate_controller_timestamp_ +=
        base::Seconds(1) /
        rate_controller_settings_.layer_settings[0].frame_rate;
  }
  return payload_size;
}

H264SPS D3D12VideoEncodeH264Delegate::ToSPS() const {
  // H264 Sequence Parameter Set
  // https://microsoft.github.io/DirectX-Specs/d3d/D3D12VideoEncoding.html#h264-sequence-parameter-set-expected-values
  H264SPS sps;
  switch (output_profile_) {
    case H264PROFILE_BASELINE:
      sps.profile_idc = H264SPS::H264ProfileIDC::kProfileIDCBaseline;
      break;
    case H264PROFILE_MAIN:
      sps.profile_idc = H264SPS::H264ProfileIDC::kProfileIDCMain;
      break;
    case H264PROFILE_HIGH:
      sps.profile_idc = H264SPS::H264ProfileIDC::kProfileIDCHigh;
      break;
    case H264PROFILE_HIGH10PROFILE:
      sps.profile_idc = H264SPS::H264ProfileIDC::kProfileIDHigh10;
      break;
    default:
      NOTREACHED();
  }
  sps.constraint_set1_flag =
      sps.profile_idc == H264SPS::H264ProfileIDC::kProfileIDCMain;
  sps.level_idc = D3D12VideoEncoderLevelsH264ToH264LevelIDC(h264_level_);
  sps.seq_parameter_set_id = 0;
  sps.chroma_format_idc = 1;
  sps.bit_depth_luma_minus8 =
      sps.profile_idc == H264SPS::H264ProfileIDC::kProfileIDHigh10 ? 2 : 0;
  sps.bit_depth_chroma_minus8 =
      sps.profile_idc == H264SPS::H264ProfileIDC::kProfileIDHigh10 ? 2 : 0;
  sps.log2_max_frame_num_minus4 = gop_structure_.log2_max_frame_num_minus4;
  sps.pic_order_cnt_type = gop_structure_.pic_order_cnt_type;
  sps.log2_max_pic_order_cnt_lsb_minus4 =
      gop_structure_.log2_max_pic_order_cnt_lsb_minus4;
  sps.max_num_ref_frames = max_num_ref_frames_;
  sps.gaps_in_frame_num_value_allowed_flag = true;
  constexpr int kMbSize = 16;
  sps.pic_width_in_mbs_minus1 = (input_size_.Width + kMbSize - 1) / kMbSize - 1;
  sps.pic_height_in_map_units_minus1 =
      (input_size_.Height + kMbSize - 1) / kMbSize - 1;
  sps.frame_mbs_only_flag = true;
  // According to H.264 spec Table A-4, for level 3.0 and higher,
  // direct_8x8_inference_flag should be equal to 1 for profiles that allow
  // B-Frame.
  sps.direct_8x8_inference_flag =
      h264_level_ >= D3D12_VIDEO_ENCODER_LEVELS_H264_3;
  if (input_size_.Width % kMbSize != 0 || input_size_.Height % kMbSize != 0) {
    // Spec 7.4.2.1.1. Crop is in crop units, which is 2 pixels for 4:2:0.
    const int kCropUnitX = 2;
    const int kCropUnitY = 2 * (2 - sps.frame_mbs_only_flag);
    sps.frame_cropping_flag = true;
    if (input_size_.Width % kMbSize != 0) {
      sps.frame_crop_right_offset =
          (kMbSize - input_size_.Width % kMbSize) / kCropUnitX;
    }
    if (input_size_.Height % kMbSize != 0) {
      sps.frame_crop_bottom_offset =
          (kMbSize - input_size_.Height % kMbSize) / kCropUnitY;
    }
  }
  return sps;
}

H264PPS D3D12VideoEncodeH264Delegate::ToPPS(const H264SPS& sps) const {
  // H264 Picture Parameter Set
  // https://microsoft.github.io/DirectX-Specs/d3d/D3D12VideoEncoding.html#h264-picture-parameter-set-expected-values
  H264PPS pps;
  pps.seq_parameter_set_id = sps.seq_parameter_set_id;
  pps.pic_parameter_set_id = 0;
  pps.entropy_coding_mode_flag = static_cast<bool>(
      codec_config_h264_.ConfigurationFlags &
      D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_H264_FLAG_ENABLE_CABAC_ENCODING);
  pps.deblocking_filter_control_present_flag = true;
  // We don't use
  // D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_H264_FLAG_USE_CONSTRAINED_INTRAPREDICTION
  // yet. So let constrained_intra_pred_flag be false by default.
  pps.transform_8x8_mode_flag = static_cast<bool>(
      codec_config_h264_.ConfigurationFlags &
      D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_H264_FLAG_USE_ADAPTIVE_8x8_TRANSFORM);
  return pps;
}

}  // namespace media
