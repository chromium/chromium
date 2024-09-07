// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/vaapi/h264_vaapi_video_encoder_delegate.h"

#include <va/va.h>
#include <va/va_enc_h264.h>

#include <climits>
#include <utility>

#include "base/bits.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/numerics/checked_math.h"
#include "build/build_config.h"
#include "media/base/media_switches.h"
#include "media/base/video_bitrate_allocation.h"
#include "media/gpu/gpu_video_encode_accelerator_helpers.h"
#include "media/gpu/h264_builder.h"
#include "media/gpu/macros.h"
#include "media/gpu/vaapi/vaapi_common.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "media/parsers/h264_level_limits.h"
#include "media/video/video_encode_accelerator.h"

namespace media {
namespace {
// An IDR every 2048 frames (must be >= 16 per spec), no I frames and no B
// frames. We choose IDR period to equal MaxFrameNum so it must be a power of 2.
// Produce an IDR at least once per this many frames. Must be >= 16 (per spec).
constexpr uint32_t kIDRPeriod = 2048;
static_assert(kIDRPeriod >= 16u, "idr_period_frames must be >= 16");
// Produce an I frame at least once per this many frames.
constexpr uint32_t kIPeriod = 0;
// How often do we need to have either an I or a P frame in the stream.
// A period of 1 implies no B frames.
constexpr uint32_t kIPPeriod = 1;

// The qp range is 0-51 in H264. Select 26 because of the center value.
// WebRTC H264 encoder uses 1-51. We set the minimum QP to 1 for camera
// and 10 for screen sharing to mitigate the bitrate overshoot due
// to a scene, and maximum qp to 42 to pass the CTS test (b/354557852).
constexpr uint8_t kDefaultQP = 26;
constexpr uint8_t kMinQP = 1;
constexpr uint8_t kScreenMinQP = 10;
constexpr uint8_t kMaxQP = 42;

// Subjectively chosen bitrate window size for rate control, in ms.
constexpr uint32_t kCPBWindowSizeMs = 1500;

// Subjectively chosen.
// Generally use up to 2 reference frames.
constexpr size_t kMaxRefIdxL0Size = 2;

// HRD parameters (ch. E.2.2 in H264 spec).
constexpr int kBitRateScale = 0;  // bit_rate_scale for SPS HRD parameters.
constexpr int kCPBSizeScale = 0;  // cpb_size_scale for SPS HRD parameters.

// 4:2:0
constexpr int kChromaFormatIDC = 1;

constexpr uint8_t kMinSupportedH264TemporalLayers = 2;
constexpr uint8_t kMaxSupportedH264TemporalLayers = 3;

template <typename VAEncMiscParam>
VAEncMiscParam& AllocateMiscParameterBuffer(
    std::vector<uint8_t>& misc_buffer,
    VAEncMiscParameterType misc_param_type) {
  constexpr size_t buffer_size =
      sizeof(VAEncMiscParameterBuffer) + sizeof(VAEncMiscParam);
  misc_buffer.resize(buffer_size);
  auto* va_buffer =
      reinterpret_cast<VAEncMiscParameterBuffer*>(misc_buffer.data());
  va_buffer->type = misc_param_type;
  return *reinterpret_cast<VAEncMiscParam*>(va_buffer->data);
}

void CreateVAEncRateControlParams(uint32_t bps,
                                  uint32_t target_percentage,
                                  uint32_t window_size,
                                  uint32_t initial_qp,
                                  uint32_t min_qp,
                                  uint32_t max_qp,
                                  uint32_t framerate,
                                  uint32_t buffer_size,
                                  std::vector<uint8_t> misc_buffers[3]) {
  auto& rate_control_param =
      AllocateMiscParameterBuffer<VAEncMiscParameterRateControl>(
          misc_buffers[0], VAEncMiscParameterTypeRateControl);
  rate_control_param.bits_per_second = bps;
  rate_control_param.target_percentage = target_percentage;
  rate_control_param.window_size = window_size;
  rate_control_param.initial_qp = initial_qp;
  rate_control_param.min_qp = min_qp;
  rate_control_param.max_qp = max_qp;
  rate_control_param.rc_flags.bits.disable_frame_skip = true;

  auto& framerate_param =
      AllocateMiscParameterBuffer<VAEncMiscParameterFrameRate>(
          misc_buffers[1], VAEncMiscParameterTypeFrameRate);
  framerate_param.framerate = framerate;

  auto& hrd_param = AllocateMiscParameterBuffer<VAEncMiscParameterHRD>(
      misc_buffers[2], VAEncMiscParameterTypeHRD);
  hrd_param.buffer_size = buffer_size;
  hrd_param.initial_buffer_fullness = buffer_size / 2;
}

static void InitVAPictureH264(VAPictureH264* va_pic) {
  *va_pic = {};
  va_pic->picture_id = VA_INVALID_ID;
  va_pic->flags = VA_PICTURE_H264_INVALID;
}

// Updates |frame_num| as spec section 7.4.3 and sets it to |pic.frame_num|.
void UpdateAndSetFrameNum(H264Picture& pic, unsigned int& frame_num) {
  if (pic.idr)
    frame_num = 0;
  else if (pic.ref)
    frame_num++;
  DCHECK_LT(frame_num, kIDRPeriod);
  pic.frame_num = frame_num;
}

// Updates and fills variables in |pic|, |frame_num| and |ref_frame_idx| for
// temporal layer encoding. |frame_num| is the frame_num in H.264 spec for
// |pic|. |ref_frame_idx| is the index in |ref_pic_list0| of the frame
// referenced by |pic|.
void UpdatePictureForTemporalLayerEncoding(
    const size_t num_layers,
    H264Picture& pic,
    unsigned int& frame_num,
    std::optional<size_t>& ref_frame_idx,
    const unsigned int num_encoded_frames,
    const base::circular_deque<scoped_refptr<H264Picture>>& ref_pic_list0) {
  DCHECK_GE(num_layers, kMinSupportedH264TemporalLayers);
  DCHECK_LE(num_layers, kMaxSupportedH264TemporalLayers);
  constexpr size_t kTemporalLayerCycle = 4;
  constexpr std::pair<H264Metadata, bool>
      kFrameMetadata[][kTemporalLayerCycle] = {
          {
              // For two temporal layers.
              {{.temporal_idx = 0, .layer_sync = false}, true},
              {{.temporal_idx = 1, .layer_sync = true}, false},
              {{.temporal_idx = 0, .layer_sync = false}, true},
              {{.temporal_idx = 1, .layer_sync = true}, false},
          },
          {
              // For three temporal layers.
              {{.temporal_idx = 0, .layer_sync = false}, true},
              {{.temporal_idx = 2, .layer_sync = true}, false},
              {{.temporal_idx = 1, .layer_sync = true}, true},
              {{.temporal_idx = 2, .layer_sync = false}, false},
          }};

  // Fill |pic.metadata_for_encoding| and |pic.ref|.
  std::tie(pic.metadata_for_encoding.emplace(), pic.ref) =
      kFrameMetadata[num_layers - 2][num_encoded_frames % kTemporalLayerCycle];

  UpdateAndSetFrameNum(pic, frame_num);

  if (pic.idr)
    return;

  // Fill reference frame related variables in |pic| and |ref_frame_idx|.
  DCHECK_EQ(pic.ref_pic_list_modification_flag_l0, 0);
  DCHECK_EQ(pic.abs_diff_pic_num_minus1, 0);
  DCHECK(!ref_pic_list0.empty());

  if (pic.metadata_for_encoding->temporal_idx == 0) {
    ref_frame_idx = base::checked_cast<size_t>(ref_pic_list0.size() - 1);
  } else {
    ref_frame_idx = 0;
  }

  DCHECK_LT(*ref_frame_idx, ref_pic_list0.size());
  const H264Picture& ref_frame_pic = *ref_pic_list0[*ref_frame_idx];
  const int abs_diff_pic_num = pic.frame_num - ref_frame_pic.frame_num;
  if (*ref_frame_idx != 0 && abs_diff_pic_num > 0) {
    pic.ref_pic_list_modification_flag_l0 = 1;
    pic.abs_diff_pic_num_minus1 = abs_diff_pic_num - 1;
  }
}

scoped_refptr<H264Picture> GetH264Picture(
    const VaapiVideoEncoderDelegate::EncodeJob& job) {
  return base::WrapRefCounted(
      reinterpret_cast<H264Picture*>(job.picture().get()));
}

std::optional<H264RateControlConfigRTC> CreateRateControlConfig(
    const gfx::Size encode_size,
    const H264VaapiVideoEncoderDelegate::EncodeParams& encode_params,
    const VideoBitrateAllocation& bitrate_allocation,
    const size_t& num_temporal_layers) {
  // Limit max delay for intra frame with HRD buffer size (500ms-1s for camera
  // video, 1s-10s for desktop sharing).
  constexpr base::TimeDelta kHRDBufferDelayCamera = base::Milliseconds(1000);
  constexpr base::TimeDelta kHRDBufferDelayDisplay = base::Milliseconds(3000);
  H264RateControlConfigRTC rc_cfg{};
  // Coded width and heght.
  rc_cfg.frame_size = encode_size;
  // Maximum GOP duration in milliseconds. It is set to maximum value.
  rc_cfg.gop_max_duration = base::TimeDelta::Max();

  // Source frame rate.
  rc_cfg.frame_rate_max = static_cast<float>(encode_params.framerate);
  // Number of temopral layers.
  rc_cfg.num_temporal_layers = num_temporal_layers;
  // Type of the video content (camera or display).
  rc_cfg.content_type = encode_params.content_type;
  rc_cfg.fixed_delta_qp = false;
  rc_cfg.ease_hrd_reduction = true;

  // Fill temporal layers variables.
  uint32_t bitrate_sum = 0;
  for (size_t tid = 0; tid < num_temporal_layers; ++tid) {
    bitrate_sum += bitrate_allocation.GetBitrateBps(0u, tid);
    auto& layer_setting = rc_cfg.layer_settings.emplace_back();
    layer_setting.avg_bitrate = bitrate_sum;
    if (bitrate_allocation.GetMode() == Bitrate::Mode::kConstant) {
      layer_setting.peak_bitrate = bitrate_sum;
    } else {
      layer_setting.peak_bitrate = bitrate_sum * 3 / 2;
    }
    base::TimeDelta buffer_delay;
    if (rc_cfg.content_type ==
        VideoEncodeAccelerator::Config::ContentType::kDisplay) {
      buffer_delay = kHRDBufferDelayDisplay;
      layer_setting.min_qp = kScreenMinQP;
    } else {
      buffer_delay = kHRDBufferDelayCamera;
      layer_setting.min_qp = kMinQP;
    }
    layer_setting.max_qp = encode_params.max_qp;

    base::CheckedNumeric<size_t> buffer_size(layer_setting.avg_bitrate);
    buffer_size *= buffer_delay.InMilliseconds();
    buffer_size /= base::Seconds(8).InMilliseconds();

    if (!buffer_size.AssignIfValid(&layer_setting.hrd_buffer_size)) {
      DVLOGF(1) << "Invalid size for HRD buffer";
      return std::nullopt;
    }
    layer_setting.frame_rate = static_cast<float>(
        encode_params.framerate / (1u << (num_temporal_layers - tid - 1)));
  }
  return std::make_optional<H264RateControlConfigRTC>(rc_cfg);
}
}  // namespace

std::unique_ptr<H264RateControlWrapper> H264RateControlWrapper::Create(
    const H264RateControlConfigRTC& config) {
  auto impl = H264RateCtrlRTC::Create(config);
  if (!impl) {
    DLOG(ERROR) << "Failed creating video H264RateCtrlRTC";
    return nullptr;
  }
  return base::WrapUnique(new H264RateControlWrapper(std::move(impl)));
}

H264RateControlWrapper::H264RateControlWrapper() = default;

H264RateControlWrapper::H264RateControlWrapper(
    std::unique_ptr<H264RateCtrlRTC> impl)
    : impl_(std::move(impl)) {}

H264RateControlWrapper::~H264RateControlWrapper() = default;

void H264RateControlWrapper::UpdateRateControl(
    const H264RateControlConfigRTC& config) {
  DCHECK(impl_);
  impl_->UpdateRateControl(config);
}

H264RateCtrlRTC::FrameDropDecision H264RateControlWrapper::ComputeQP(
    const H264FrameParamsRTC& frame_params) {
  DCHECK(impl_);
  return impl_->ComputeQP(frame_params);
}

int H264RateControlWrapper::GetQP() const {
  return impl_->GetQP();
}

void H264RateControlWrapper::PostEncodeUpdate(
    uint64_t encoded_frame_size,
    const H264FrameParamsRTC& frame_params) {
  impl_->PostEncodeUpdate(encoded_frame_size, frame_params);
}

H264VaapiVideoEncoderDelegate::EncodeParams::EncodeParams()
    : framerate(0),
      cpb_window_size_ms(kCPBWindowSizeMs),
      cpb_size_bits(0),
      initial_qp(kDefaultQP),
      min_qp(kMinQP),
      max_qp(kMaxQP),
      max_num_ref_frames(kMaxRefIdxL0Size),
      max_ref_pic_list0_size(kMaxRefIdxL0Size) {}

H264VaapiVideoEncoderDelegate::H264VaapiVideoEncoderDelegate(
    scoped_refptr<VaapiWrapper> vaapi_wrapper,
    base::RepeatingClosure error_cb)
    : VaapiVideoEncoderDelegate(std::move(vaapi_wrapper), error_cb) {}

H264VaapiVideoEncoderDelegate::~H264VaapiVideoEncoderDelegate() = default;

void H264VaapiVideoEncoderDelegate::set_rate_ctrl_for_testing(
    std::unique_ptr<H264RateControlWrapper> rate_ctrl) {
  rate_ctrl_ = std::move(rate_ctrl);
}

bool H264VaapiVideoEncoderDelegate::Initialize(
    const VideoEncodeAccelerator::Config& config,
    const VaapiVideoEncoderDelegate::Config& ave_config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (config.output_profile) {
    case H264PROFILE_BASELINE:
    case H264PROFILE_MAIN:
    case H264PROFILE_HIGH:
      break;

    default:
      NOTIMPLEMENTED() << "Unsupported profile "
                       << GetProfileName(config.output_profile);
      return false;
  }

  if (config.input_visible_size.IsEmpty()) {
    DVLOGF(1) << "Input visible size could not be empty";
    return false;
  }

  if (config.HasSpatialLayer()) {
    DVLOGF(1) << "Spatial layer encoding is not supported";
    return false;
  }

  visible_size_ = config.input_visible_size;
  // For 4:2:0, the pixel sizes have to be even.
  if ((visible_size_.width() % 2 != 0) || (visible_size_.height() % 2 != 0)) {
    DVLOGF(1) << "The pixel sizes are not even: " << visible_size_.ToString();
    return false;
  }
  constexpr int kH264MacroblockSizeInPixels = 16;
  coded_size_ =
      gfx::Size(base::bits::AlignUpDeprecatedDoNotUse(
                    visible_size_.width(), kH264MacroblockSizeInPixels),
                base::bits::AlignUpDeprecatedDoNotUse(
                    visible_size_.height(), kH264MacroblockSizeInPixels));
  mb_width_ = coded_size_.width() / kH264MacroblockSizeInPixels;
  mb_height_ = coded_size_.height() / kH264MacroblockSizeInPixels;

  profile_ = config.output_profile;
  level_ = config.h264_output_level.value_or(H264SPS::kLevelIDC4p0);
  uint32_t framerate = config.framerate;

  // Checks if |level_| is valid. If it is invalid, set |level_| to a minimum
  // level that comforts Table A-1 in H.264 spec with specified bitrate,
  // framerate and dimension.
  if (!CheckH264LevelLimits(profile_, level_, config.bitrate.target_bps(),
                            framerate, mb_width_ * mb_height_)) {
    std::optional<uint8_t> valid_level =
        FindValidH264Level(profile_, config.bitrate.target_bps(), framerate,
                           mb_width_ * mb_height_);
    if (!valid_level) {
      VLOGF(1) << "Could not find a valid h264 level for"
               << " profile=" << profile_
               << " bitrate=" << config.bitrate.target_bps()
               << " framerate=" << framerate
               << " size=" << config.input_visible_size.ToString();
      return false;
    }
    level_ = *valid_level;
  }

  if (config.content_type ==
      VideoEncodeAccelerator::Config::ContentType::kDisplay) {
    curr_params_.min_qp = kScreenMinQP;
  }

  num_temporal_layers_ = 1;
  if (config.HasTemporalLayer()) {
    DCHECK(!config.spatial_layers.empty());
    num_temporal_layers_ = config.spatial_layers[0].num_of_temporal_layers;
    if (num_temporal_layers_ > kMaxSupportedH264TemporalLayers ||
        num_temporal_layers_ < kMinSupportedH264TemporalLayers) {
      DVLOGF(1) << "Unsupported number of temporal layers: "
                << base::strict_cast<size_t>(num_temporal_layers_);
      return false;
    }
  }

  curr_params_.max_ref_pic_list0_size =
      num_temporal_layers_ > 1u
          ? num_temporal_layers_ - 1
          : std::min(kMaxRefIdxL0Size, ave_config.max_num_ref_frames & 0xffff);
  curr_params_.max_num_ref_frames = curr_params_.max_ref_pic_list0_size;

  bool submit_packed_sps = false;
  bool submit_packed_pps = false;
  bool submit_packed_slice = false;
  if (!vaapi_wrapper_->GetSupportedPackedHeaders(
          config.output_profile, submit_packed_sps, submit_packed_pps,
          submit_packed_slice)) {
    DVLOGF(1) << "Failed getting supported packed headers";
    return false;
  }

  // Submit packed headers only if packed SPS, PPS and slice header all are
  // supported.
  submit_packed_headers_ =
      submit_packed_sps && submit_packed_pps && submit_packed_slice;
  if (submit_packed_headers_) {
    packed_sps_.emplace();
    packed_pps_.emplace();
  } else {
    DVLOGF(2) << "Packed headers are not submitted to a driver";
  }

  UpdateSPS();
  UpdatePPS();

  // If we don't set the stored BitrateAllocation to the right type, UpdateRates
  // will mistakenly reject the bitrate when the requested type in the config is
  // not the default (constant bitrate).
  curr_params_.bitrate_allocation =
      VideoBitrateAllocation(config.bitrate.mode());

  auto initial_bitrate_allocation = AllocateBitrateForDefaultEncoding(config);

  curr_params_.content_type = config.content_type;
  curr_params_.framerate = framerate;

  if (UseSoftwareRateController(config)) {
    if (!rate_ctrl_) {
      auto rc_config = CreateRateControlConfig(visible_size_, curr_params_,
                                               initial_bitrate_allocation,
                                               num_temporal_layers_);
      if (!rc_config) {
        DVLOGF(1) << "Failed creating rate control config";
        return false;
      }
      rate_ctrl_ = H264RateControlWrapper::Create(*rc_config);
    }
    if (!rate_ctrl_) {
      return false;
    }
  } else {
    CHECK(!rate_ctrl_);
  }

  return UpdateRates(AllocateBitrateForDefaultEncoding(config), framerate);
}

gfx::Size H264VaapiVideoEncoderDelegate::GetCodedSize() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!coded_size_.IsEmpty());

  return coded_size_;
}

size_t H264VaapiVideoEncoderDelegate::GetMaxNumOfRefFrames() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return curr_params_.max_num_ref_frames;
}

std::vector<gfx::Size> H264VaapiVideoEncoderDelegate::GetSVCLayerResolutions() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return {visible_size_};
}

bool H264VaapiVideoEncoderDelegate::UseSoftwareRateController(
    const VideoEncodeAccelerator::Config& config) {
  // TODO(b/362266573): Use the software bitrate controller for L1T2.
  uint8_t num_temporal_layers = 1;
  if (config.HasTemporalLayer()) {
    DCHECK(!config.spatial_layers.empty());
    num_temporal_layers = config.spatial_layers[0].num_of_temporal_layers;
  }
  const bool is_sw_bitrate_controller_enabled =
#if BUILDFLAG(IS_CHROMEOS)
      base::FeatureList::IsEnabled(kVaapiH264SWBitrateController);
#else
      false;
#endif  // BUILDFLAG(IS_CHROMEOS)
  return num_temporal_layers == 1 && is_sw_bitrate_controller_enabled;
}

BitstreamBufferMetadata H264VaapiVideoEncoderDelegate::GetMetadata(
    const EncodeJob& encode_job,
    size_t payload_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!encode_job.IsFrameDropped());
  CHECK_NE(payload_size, 0u);
  BitstreamBufferMetadata metadata(
      payload_size, encode_job.IsKeyframeRequested(), encode_job.timestamp());
  CHECK(metadata.end_of_picture());
  auto picture = GetH264Picture(encode_job);
  DCHECK(picture);

  metadata.h264 = picture->metadata_for_encoding;
  return metadata;
}

VaapiVideoEncoderDelegate::PrepareEncodeJobResult
H264VaapiVideoEncoderDelegate::PrepareEncodeJob(EncodeJob& encode_job) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  scoped_refptr<H264Picture> pic = GetH264Picture(encode_job);
  DCHECK(pic);

  if (encode_job.IsKeyframeRequested() || encoding_parameters_changed_)
    num_encoded_frames_ = 0;

  if (num_encoded_frames_ == 0) {
    pic->idr = true;
    // H264 spec mandates idr_pic_id to differ between two consecutive IDRs.
    idr_pic_id_ ^= 1;
    pic->idr_pic_id = idr_pic_id_;
    ref_pic_list0_.clear();

    encoding_parameters_changed_ = false;
    encode_job.ProduceKeyframe();
  }

  pic->type = pic->idr ? H264SliceHeader::kISlice : H264SliceHeader::kPSlice;

  std::optional<size_t> ref_frame_index;
  if (num_temporal_layers_ > 1u) {
    UpdatePictureForTemporalLayerEncoding(num_temporal_layers_, *pic,
                                          frame_num_, ref_frame_index,
                                          num_encoded_frames_, ref_pic_list0_);
  } else {
    pic->ref = true;
    UpdateAndSetFrameNum(*pic, frame_num_);
  }

  pic->pic_order_cnt = num_encoded_frames_ * 2;
  pic->top_field_order_cnt = pic->pic_order_cnt;
  pic->pic_order_cnt_lsb = pic->pic_order_cnt;

  DVLOGF(4) << "Starting a new frame, type: " << pic->type
            << (encode_job.IsKeyframeRequested() ? " (keyframe)" : "")
            << " frame_num: " << pic->frame_num
            << " POC: " << pic->pic_order_cnt;

  std::optional<int> qp;
  if (rate_ctrl_) {
    H264FrameParamsRTC frame_params{};
    frame_params.temporal_layer_id =
        pic->metadata_for_encoding
            ? base::strict_cast<int>(pic->metadata_for_encoding->temporal_idx)
            : 0;
    frame_params.keyframe = encode_job.IsKeyframeRequested();
    frame_params.timestamp = encode_job.timestamp();
    if (rate_ctrl_->ComputeQP(frame_params) ==
        H264RateCtrlRTC::FrameDropDecision::kDrop) {
      CHECK(!encode_job.IsKeyframeRequested());
      DVLOGF(3) << "Drop frame";
      return PrepareEncodeJobResult::kDrop;
    }
    qp = rate_ctrl_->GetQP();
    DVLOGF(4) << "qp=" << qp.value();
  }

  if (!SubmitFrameParameters(encode_job, curr_params_, current_sps_,
                             current_pps_, pic, ref_pic_list0_, ref_frame_index,
                             qp)) {
    DVLOGF(1) << "Failed submitting frame parameters";
    return PrepareEncodeJobResult::kFail;
  }

  if (pic->type == H264SliceHeader::kISlice && submit_packed_headers_) {
    // We always generate SPS and PPS with I(DR) frame. This will help for Seek
    // operation on the generated stream.
    if (!SubmitPackedHeaders(*packed_sps_, *packed_pps_)) {
      DVLOGF(1) << "Failed submitting keyframe headers";
      return PrepareEncodeJobResult::kFail;
    }
  }

  // Store the picture on the list of reference pictures and keep the list
  // below maximum size, dropping oldest references.
  if (pic->ref) {
    ref_pic_list0_.push_front(pic);
    ref_pic_list0_.resize(
        std::min(curr_params_.max_ref_pic_list0_size, ref_pic_list0_.size()));
  }

  num_encoded_frames_++;
  num_encoded_frames_ %= kIDRPeriod;
  return PrepareEncodeJobResult::kSuccess;
}

bool H264VaapiVideoEncoderDelegate::UpdateRates(
    const VideoBitrateAllocation& bitrate_allocation,
    uint32_t framerate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (bitrate_allocation.GetMode() !=
      curr_params_.bitrate_allocation.GetMode()) {
    DVLOGF(1) << "Unexpected bitrate mode, requested rate "
              << bitrate_allocation.GetSumBitrate().ToString()
              << ", expected mode to match "
              << curr_params_.bitrate_allocation.GetSumBitrate().ToString();
    return false;
  }

  uint32_t bitrate = bitrate_allocation.GetSumBps();
  if (bitrate == 0 || framerate == 0)
    return false;

  if (curr_params_.bitrate_allocation == bitrate_allocation &&
      curr_params_.framerate == framerate) {
    return true;
  }
  VLOGF(2) << "New bitrate allocation: " << bitrate_allocation.ToString()
           << ", New framerate: " << framerate;

  curr_params_.bitrate_allocation = bitrate_allocation;
  curr_params_.framerate = framerate;

  base::CheckedNumeric<uint32_t> cpb_size_bits(bitrate);
  cpb_size_bits /= 1000;
  cpb_size_bits *= curr_params_.cpb_window_size_ms;
  if (!cpb_size_bits.AssignIfValid(&curr_params_.cpb_size_bits)) {
    VLOGF(1) << "Too large bitrate: " << bitrate_allocation.GetSumBps();
    return false;
  }

  bool previous_encoding_parameters_changed = encoding_parameters_changed_;

  UpdateSPS();

  // If SPS parameters are updated, it is required to send the SPS with IDR
  // frame. However, as a special case, we do not generate IDR frame if only
  // bitrate and framerate parameters are updated. This is safe because these
  // will not make a difference on decoder processing. The updated SPS will be
  // sent a next periodic or requested I(DR) frame. On the other hand, bitrate
  // and framerate parameter
  // changes must be affected for encoding. UpdateSPS()+SubmitFrameParameters()
  // shall apply them to an encoder properly.
  encoding_parameters_changed_ = previous_encoding_parameters_changed;

  if (rate_ctrl_) {
    auto rc_config = CreateRateControlConfig(
        visible_size_, curr_params_, bitrate_allocation, num_temporal_layers_);
    if (!rc_config) {
      DVLOGF(1) << "Failed creating rate control config";
      return false;
    }
    rate_ctrl_->UpdateRateControl(*rc_config);
  }
  return true;
}

void H264VaapiVideoEncoderDelegate::UpdateSPS() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  memset(&current_sps_, 0, sizeof(H264SPS));

  // Spec A.2 and A.3.
  switch (profile_) {
    case H264PROFILE_BASELINE:
      // Due to https://crbug.com/345569, we don't distinguish between
      // constrained and non-constrained baseline profiles. Since many codecs
      // can't do non-constrained, and constrained is usually what we mean (and
      // it's a subset of non-constrained), default to it.
      current_sps_.profile_idc = H264SPS::kProfileIDCConstrainedBaseline;
      current_sps_.constraint_set0_flag = true;
      current_sps_.constraint_set1_flag = true;
      break;
    case H264PROFILE_MAIN:
      current_sps_.profile_idc = H264SPS::kProfileIDCMain;
      current_sps_.constraint_set1_flag = true;
      break;
    case H264PROFILE_HIGH:
      current_sps_.profile_idc = H264SPS::kProfileIDCHigh;
      break;
    default:
      NOTREACHED();
  }

  H264SPS::GetLevelConfigFromProfileLevel(profile_, level_,
                                          &current_sps_.level_idc,
                                          &current_sps_.constraint_set3_flag);

  current_sps_.seq_parameter_set_id = 0;
  current_sps_.chroma_format_idc = kChromaFormatIDC;

  current_sps_.log2_max_frame_num_minus4 =
      base::bits::Log2Ceiling(kIDRPeriod) - 4;
  current_sps_.pic_order_cnt_type = 0;
  current_sps_.log2_max_pic_order_cnt_lsb_minus4 =
      base::bits::Log2Ceiling(kIDRPeriod * 2) - 4;
  current_sps_.max_num_ref_frames = curr_params_.max_num_ref_frames;

  current_sps_.frame_mbs_only_flag = true;
  current_sps_.gaps_in_frame_num_value_allowed_flag = false;

  DCHECK_GT(mb_width_, 0u);
  DCHECK_GT(mb_height_, 0u);
  current_sps_.pic_width_in_mbs_minus1 = mb_width_ - 1;
  DCHECK(current_sps_.frame_mbs_only_flag);
  current_sps_.pic_height_in_map_units_minus1 = mb_height_ - 1;

  if (visible_size_ != coded_size_) {
    // Visible size differs from coded size, fill crop information.
    current_sps_.frame_cropping_flag = true;
    DCHECK(!current_sps_.separate_colour_plane_flag);
    // Spec table 6-1. Only 4:2:0 for now.
    DCHECK_EQ(current_sps_.chroma_format_idc, 1);
    // Spec 7.4.2.1.1. Crop is in crop units, which is 2 pixels for 4:2:0.
    const unsigned int crop_unit_x = 2;
    const unsigned int crop_unit_y = 2 * (2 - current_sps_.frame_mbs_only_flag);
    current_sps_.frame_crop_left_offset = 0;
    current_sps_.frame_crop_right_offset =
        (coded_size_.width() - visible_size_.width()) / crop_unit_x;
    current_sps_.frame_crop_top_offset = 0;
    current_sps_.frame_crop_bottom_offset =
        (coded_size_.height() - visible_size_.height()) / crop_unit_y;
  }

  current_sps_.vui_parameters_present_flag = true;
  current_sps_.timing_info_present_flag = true;
  current_sps_.num_units_in_tick = 1;
  current_sps_.time_scale =
      curr_params_.framerate * 2;  // See equation D-2 in spec.
  current_sps_.fixed_frame_rate_flag = true;

  current_sps_.nal_hrd_parameters_present_flag = true;
  // H.264 spec ch. E.2.2.
  current_sps_.cpb_cnt_minus1 = 0;
  current_sps_.bit_rate_scale = kBitRateScale;
  current_sps_.cpb_size_scale = kCPBSizeScale;
  // This implicitly converts from an unsigned rhs integer to a signed integer
  // lhs (|bit_rate_value_minus1|). This is safe because
  // |H264SPS::kBitRateScaleConstantTerm| is 6, so the bitshift is equivalent to
  // dividing by 2^6. Therefore the resulting value is guaranteed to be in the
  // range of a signed 32-bit integer.
  current_sps_.bit_rate_value_minus1[0] =
      (curr_params_.bitrate_allocation.GetSumBps() >>
       (kBitRateScale + H264SPS::kBitRateScaleConstantTerm)) -
      1;
  current_sps_.cpb_size_value_minus1[0] =
      (curr_params_.cpb_size_bits >>
       (kCPBSizeScale + H264SPS::kCPBSizeScaleConstantTerm)) -
      1;
  switch (curr_params_.bitrate_allocation.GetMode()) {
    case Bitrate::Mode::kConstant:
      current_sps_.cbr_flag[0] = true;
      break;
    case Bitrate::Mode::kVariable:
      current_sps_.cbr_flag[0] = false;
      break;
    case Bitrate::Mode::kExternal:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  current_sps_.initial_cpb_removal_delay_length_minus_1 =
      H264SPS::kDefaultInitialCPBRemovalDelayLength - 1;
  current_sps_.cpb_removal_delay_length_minus1 =
      H264SPS::kDefaultInitialCPBRemovalDelayLength - 1;
  current_sps_.dpb_output_delay_length_minus1 =
      H264SPS::kDefaultDPBOutputDelayLength - 1;
  current_sps_.time_offset_length = H264SPS::kDefaultTimeOffsetLength;
  current_sps_.low_delay_hrd_flag = false;

  if (submit_packed_headers_) {
    DCHECK(packed_sps_);
    packed_sps_->Reset();
    BuildPackedH264SPS(packed_sps_.value(), current_sps_);
  }
  encoding_parameters_changed_ = true;
}

void H264VaapiVideoEncoderDelegate::UpdatePPS() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  memset(&current_pps_, 0, sizeof(H264PPS));

  current_pps_.seq_parameter_set_id = current_sps_.seq_parameter_set_id;
  DCHECK_EQ(current_pps_.pic_parameter_set_id, 0);

  current_pps_.entropy_coding_mode_flag =
      current_sps_.profile_idc >= H264SPS::kProfileIDCMain;

  DCHECK_GT(curr_params_.max_ref_pic_list0_size, 0u);
  current_pps_.num_ref_idx_l0_default_active_minus1 =
      curr_params_.max_ref_pic_list0_size - 1;
  DCHECK_EQ(current_pps_.num_ref_idx_l1_default_active_minus1, 0);
  DCHECK_LE(curr_params_.initial_qp, 51u);
  current_pps_.pic_init_qp_minus26 =
      static_cast<int>(curr_params_.initial_qp) - 26;
  current_pps_.deblocking_filter_control_present_flag = true;
  current_pps_.transform_8x8_mode_flag =
      (current_sps_.profile_idc == H264SPS::kProfileIDCHigh);

  if (submit_packed_headers_) {
    DCHECK(packed_pps_);
    packed_pps_->Reset();
    BuildPackedH264PPS(packed_pps_.value(), current_sps_, current_pps_);
  }
  encoding_parameters_changed_ = true;
}

void H264VaapiVideoEncoderDelegate::GeneratePackedSliceHeader(
    H26xAnnexBBitstreamBuilder& packed_slice_header,
    const VAEncPictureParameterBufferH264& pic_param,
    const VAEncSliceParameterBufferH264& slice_param,
    const H264Picture& pic) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const bool is_idr = !!pic_param.pic_fields.bits.idr_pic_flag;
  const bool is_ref = !!pic_param.pic_fields.bits.reference_pic_flag;
  // IDR:3, Non-IDR I slice:2, P slice:1, non ref frame: 0.
  size_t nal_ref_idc = 0;
  H264NALU::Type nalu_type = H264NALU::Type::kUnspecified;
  if (slice_param.slice_type == H264SliceHeader::kISlice) {
    nal_ref_idc = is_idr ? 3 : 2;
    nalu_type = is_idr ? H264NALU::kIDRSlice : H264NALU::kNonIDRSlice;
  } else {
    // B frames is not used, so this is P frame.
    nal_ref_idc = is_ref;
    nalu_type = H264NALU::kNonIDRSlice;
  }
  packed_slice_header.BeginNALU(nalu_type, nal_ref_idc);

  packed_slice_header.AppendUE(
      slice_param.macroblock_address);  // first_mb_in_slice
  packed_slice_header.AppendUE(slice_param.slice_type);
  packed_slice_header.AppendUE(slice_param.pic_parameter_set_id);
  packed_slice_header.AppendBits(current_sps_.log2_max_frame_num_minus4 + 4,
                                 pic_param.frame_num);  // frame_num

  DCHECK(current_sps_.frame_mbs_only_flag);
  if (is_idr)
    packed_slice_header.AppendUE(slice_param.idr_pic_id);

  DCHECK_EQ(current_sps_.pic_order_cnt_type, 0);
  packed_slice_header.AppendBits(
      current_sps_.log2_max_pic_order_cnt_lsb_minus4 + 4,
      pic_param.CurrPic.TopFieldOrderCnt);
  DCHECK(!current_pps_.bottom_field_pic_order_in_frame_present_flag);
  DCHECK(!current_pps_.redundant_pic_cnt_present_flag);

  if (slice_param.slice_type == H264SliceHeader::kPSlice) {
    packed_slice_header.AppendBits(
        1, slice_param.num_ref_idx_active_override_flag);
    if (slice_param.num_ref_idx_active_override_flag)
      packed_slice_header.AppendUE(slice_param.num_ref_idx_l0_active_minus1);
  }

  if (slice_param.slice_type != H264SliceHeader::kISlice) {
    packed_slice_header.AppendBits(1, pic.ref_pic_list_modification_flag_l0);
    // modification flag for P slice.
    if (pic.ref_pic_list_modification_flag_l0) {
      // modification_of_pic_num_idc
      packed_slice_header.AppendUE(0);
      // abs_diff_pic_num_minus1
      packed_slice_header.AppendUE(pic.abs_diff_pic_num_minus1);
      // modification_of_pic_num_idc
      packed_slice_header.AppendUE(3);
    }
  }
  DCHECK_NE(slice_param.slice_type, H264SliceHeader::kBSlice);
  DCHECK(!pic_param.pic_fields.bits.weighted_pred_flag ||
         !(slice_param.slice_type == H264SliceHeader::kPSlice));

  // dec_ref_pic_marking
  if (nal_ref_idc != 0) {
    if (is_idr) {
      packed_slice_header.AppendBool(false);  // no_output_of_prior_pics_flag
      packed_slice_header.AppendBool(false);  // long_term_reference_flag
    } else {
      packed_slice_header.AppendBool(
          false);  // adaptive_ref_pic_marking_mode_flag
    }
  }

  if (pic_param.pic_fields.bits.entropy_coding_mode_flag &&
      slice_param.slice_type != H264SliceHeader::kISlice) {
    packed_slice_header.AppendUE(slice_param.cabac_init_idc);
  }

  packed_slice_header.AppendSE(slice_param.slice_qp_delta);

  if (pic_param.pic_fields.bits.deblocking_filter_control_present_flag) {
    packed_slice_header.AppendUE(slice_param.disable_deblocking_filter_idc);

    if (slice_param.disable_deblocking_filter_idc != 1) {
      packed_slice_header.AppendSE(slice_param.slice_alpha_c0_offset_div2);
      packed_slice_header.AppendSE(slice_param.slice_beta_offset_div2);
    }
  }

  packed_slice_header.Flush();
}

bool H264VaapiVideoEncoderDelegate::SubmitFrameParameters(
    EncodeJob& job,
    const H264VaapiVideoEncoderDelegate::EncodeParams& encode_params,
    const H264SPS& sps,
    const H264PPS& pps,
    scoped_refptr<H264Picture> pic,
    const base::circular_deque<scoped_refptr<H264Picture>>& ref_pic_list0,
    const std::optional<size_t>& ref_frame_index,
    const std::optional<int>& qp) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const Bitrate bitrate = encode_params.bitrate_allocation.GetSumBitrate();
  uint32_t bitrate_bps = bitrate.target_bps();
  uint32_t target_percentage = 100u;
  if (bitrate.mode() == Bitrate::Mode::kVariable) {
    // In VA-API, the sequence parameter's bits_per_second represents the
    // maximum bitrate. Above, we use the target_bps for |bitrate_bps|; this is
    // because 1) for constant bitrates, peak and target are equal, and 2)
    // |Bitrate| class does not store a peak_bps for constant bitrates. Here,
    // we use the peak, because it exists for variable bitrates.
    bitrate_bps = bitrate.peak_bps();
    DCHECK_NE(bitrate.peak_bps(), 0u);
    base::CheckedNumeric<uint32_t> checked_percentage =
        base::CheckDiv(base::CheckMul<uint32_t>(bitrate.target_bps(), 100u),
                       bitrate.peak_bps());
    if (!checked_percentage.AssignIfValid(&target_percentage)) {
      DVLOGF(1)
          << "Integer overflow while computing target percentage for bitrate.";
      return false;
    }
    target_percentage = checked_percentage.ValueOrDefault(100u);
  }
  VAEncSequenceParameterBufferH264 seq_param = {};

#define SPS_TO_SP(a) seq_param.a = sps.a;
  SPS_TO_SP(seq_parameter_set_id);
  SPS_TO_SP(level_idc);

  seq_param.intra_period = kIPeriod;
  seq_param.intra_idr_period = kIDRPeriod;
  seq_param.ip_period = kIPPeriod;
  seq_param.bits_per_second = bitrate_bps;

  SPS_TO_SP(max_num_ref_frames);
  std::optional<gfx::Size> coded_size = sps.GetCodedSize();
  if (!coded_size) {
    DVLOGF(1) << "Invalid coded size";
    return false;
  }
  constexpr int kH264MacroblockSizeInPixels = 16;
  seq_param.picture_width_in_mbs =
      coded_size->width() / kH264MacroblockSizeInPixels;
  seq_param.picture_height_in_mbs =
      coded_size->height() / kH264MacroblockSizeInPixels;

#define SPS_TO_SP_FS(a) seq_param.seq_fields.bits.a = sps.a;
  SPS_TO_SP_FS(chroma_format_idc);
  SPS_TO_SP_FS(frame_mbs_only_flag);
  SPS_TO_SP_FS(log2_max_frame_num_minus4);
  SPS_TO_SP_FS(pic_order_cnt_type);
  SPS_TO_SP_FS(log2_max_pic_order_cnt_lsb_minus4);
#undef SPS_TO_SP_FS

  SPS_TO_SP(bit_depth_luma_minus8);
  SPS_TO_SP(bit_depth_chroma_minus8);

  SPS_TO_SP(frame_cropping_flag);
  if (sps.frame_cropping_flag) {
    SPS_TO_SP(frame_crop_left_offset);
    SPS_TO_SP(frame_crop_right_offset);
    SPS_TO_SP(frame_crop_top_offset);
    SPS_TO_SP(frame_crop_bottom_offset);
  }

  SPS_TO_SP(vui_parameters_present_flag);
#define SPS_TO_SP_VF(a) seq_param.vui_fields.bits.a = sps.a;
  SPS_TO_SP_VF(timing_info_present_flag);
#undef SPS_TO_SP_VF
  SPS_TO_SP(num_units_in_tick);
  SPS_TO_SP(time_scale);
#undef SPS_TO_SP

  VAEncPictureParameterBufferH264 pic_param = {};

  auto va_surface_id = pic->AsVaapiH264Picture()->va_surface_id();
  pic_param.CurrPic.picture_id = va_surface_id;
  pic_param.CurrPic.TopFieldOrderCnt = pic->top_field_order_cnt;
  pic_param.CurrPic.BottomFieldOrderCnt = pic->bottom_field_order_cnt;
  pic_param.CurrPic.flags = 0;

  pic_param.coded_buf = job.coded_buffer_id();
  pic_param.pic_parameter_set_id = pps.pic_parameter_set_id;
  pic_param.seq_parameter_set_id = pps.seq_parameter_set_id;
  pic_param.frame_num = pic->frame_num;
  pic_param.pic_init_qp = pps.pic_init_qp_minus26 + 26;
  pic_param.num_ref_idx_l0_active_minus1 =
      pps.num_ref_idx_l0_default_active_minus1;

  pic_param.pic_fields.bits.idr_pic_flag = pic->idr;
  pic_param.pic_fields.bits.reference_pic_flag = pic->ref;
#define PPS_TO_PP_PF(a) pic_param.pic_fields.bits.a = pps.a;
  PPS_TO_PP_PF(entropy_coding_mode_flag);
  PPS_TO_PP_PF(transform_8x8_mode_flag);
  PPS_TO_PP_PF(deblocking_filter_control_present_flag);
#undef PPS_TO_PP_PF

  VAEncSliceParameterBufferH264 slice_param = {};

  slice_param.num_macroblocks =
      seq_param.picture_width_in_mbs * seq_param.picture_height_in_mbs;
  slice_param.macroblock_info = VA_INVALID_ID;
  slice_param.slice_type = pic->type;
  slice_param.pic_parameter_set_id = pps.pic_parameter_set_id;
  slice_param.idr_pic_id = pic->idr_pic_id;
  slice_param.pic_order_cnt_lsb = pic->pic_order_cnt_lsb;
  slice_param.num_ref_idx_active_override_flag = true;
  if (slice_param.slice_type == H264SliceHeader::kPSlice) {
    slice_param.num_ref_idx_l0_active_minus1 =
        ref_frame_index.has_value() ? 0 : ref_pic_list0.size() - 1;
  } else {
    slice_param.num_ref_idx_l0_active_minus1 = 0;
  }

  for (VAPictureH264& picture : pic_param.ReferenceFrames)
    InitVAPictureH264(&picture);

  for (VAPictureH264& picture : slice_param.RefPicList0)
    InitVAPictureH264(&picture);

  for (VAPictureH264& picture : slice_param.RefPicList1)
    InitVAPictureH264(&picture);

  for (size_t i = 0, j = 0; i < ref_pic_list0.size(); ++i) {
    H264Picture& ref_pic = *ref_pic_list0[i];
    VAPictureH264 va_pic_h264;
    InitVAPictureH264(&va_pic_h264);
    va_pic_h264.picture_id = ref_pic.AsVaapiH264Picture()->va_surface_id();
    va_pic_h264.flags = VA_PICTURE_H264_SHORT_TERM_REFERENCE;
    va_pic_h264.frame_idx = ref_pic.frame_num;
    va_pic_h264.TopFieldOrderCnt = ref_pic.top_field_order_cnt;
    va_pic_h264.BottomFieldOrderCnt = ref_pic.bottom_field_order_cnt;
    // Initialize the current entry on slice and picture reference lists to
    // |ref_pic| and advance list pointers.
    pic_param.ReferenceFrames[i] = va_pic_h264;
    if (!ref_frame_index || *ref_frame_index == i)
      slice_param.RefPicList0[j++] = va_pic_h264;
  }

  if (qp.has_value()) {
    slice_param.slice_qp_delta = base::checked_cast<int8_t>(qp.value() - 26);
  }

  std::vector<VaapiWrapper::VABufferDescriptor> va_buffers = {
      {VAEncSequenceParameterBufferType, sizeof(seq_param), &seq_param},
      {VAEncPictureParameterBufferType, sizeof(pic_param), &pic_param},
      {VAEncSliceParameterBufferType, sizeof(slice_param), &slice_param}};

  std::vector<uint8_t> misc_buffers[3];
  if (!qp.has_value()) {
    CHECK(!rate_ctrl_);
    CreateVAEncRateControlParams(
        bitrate_bps, target_percentage, encode_params.cpb_window_size_ms,
        base::strict_cast<uint32_t>(pic_param.pic_init_qp),
        base::strict_cast<uint32_t>(encode_params.min_qp),
        base::strict_cast<uint32_t>(encode_params.max_qp),
        encode_params.framerate,
        base::strict_cast<uint32_t>(encode_params.cpb_size_bits), misc_buffers);
    va_buffers.push_back({VAEncMiscParameterBufferType, misc_buffers[0].size(),
                          misc_buffers[0].data()});
    va_buffers.push_back({VAEncMiscParameterBufferType, misc_buffers[1].size(),
                          misc_buffers[1].data()});
    va_buffers.push_back({VAEncMiscParameterBufferType, misc_buffers[2].size(),
                          misc_buffers[2].data()});
  }

  H26xAnnexBBitstreamBuilder packed_slice_header;
  VAEncPackedHeaderParameterBuffer packed_slice_param_buffer;
  if (submit_packed_headers_) {
    GeneratePackedSliceHeader(packed_slice_header, pic_param, slice_param,
                              *pic);
    packed_slice_param_buffer.type = VAEncPackedHeaderSlice;
    packed_slice_param_buffer.bit_length = packed_slice_header.BitsInBuffer();
    packed_slice_param_buffer.has_emulation_bytes = 0;
    va_buffers.push_back({VAEncPackedHeaderParameterBufferType,
                          sizeof(packed_slice_param_buffer),
                          &packed_slice_param_buffer});
    va_buffers.push_back({VAEncPackedHeaderDataBufferType,
                          packed_slice_header.BytesInBuffer(),
                          packed_slice_header.data()});
  }

  return vaapi_wrapper_->SubmitBuffers(va_buffers);
}

bool H264VaapiVideoEncoderDelegate::SubmitPackedHeaders(
    const H26xAnnexBBitstreamBuilder& packed_sps,
    const H26xAnnexBBitstreamBuilder& packed_pps) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(submit_packed_headers_);

  // Submit SPS.
  VAEncPackedHeaderParameterBuffer packed_sps_param = {};
  packed_sps_param.type = VAEncPackedHeaderSequence;
  packed_sps_param.bit_length = packed_sps.BytesInBuffer() * CHAR_BIT;
  VAEncPackedHeaderParameterBuffer packed_pps_param = {};
  packed_pps_param.type = VAEncPackedHeaderPicture;
  packed_pps_param.bit_length = packed_pps.BytesInBuffer() * CHAR_BIT;

  return vaapi_wrapper_->SubmitBuffers(
      {{VAEncPackedHeaderParameterBufferType, sizeof(packed_sps_param),
        &packed_sps_param},
       {VAEncPackedHeaderDataBufferType, packed_sps.BytesInBuffer(),
        packed_sps.data()},
       {VAEncPackedHeaderParameterBufferType, sizeof(packed_pps_param),
        &packed_pps_param},
       {VAEncPackedHeaderDataBufferType, packed_pps.BytesInBuffer(),
        packed_pps.data()}});
}

void H264VaapiVideoEncoderDelegate::BitrateControlUpdate(
    const BitstreamBufferMetadata& metadata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!rate_ctrl_) {
    return;
  }

  H264FrameParamsRTC frame_params{};
  if (metadata.h264) {
    frame_params.temporal_layer_id =
        static_cast<int>(metadata.h264->temporal_idx);
  } else {
    frame_params.temporal_layer_id = 0;
  }
  frame_params.keyframe = metadata.key_frame;
  frame_params.timestamp = metadata.timestamp;

  DVLOGF(4) << "temporal_idx="
            << (metadata.h264 ? metadata.h264->temporal_idx : 0)
            << ", encoded chunk size=" << metadata.payload_size_bytes
            << ", timestamp=" << metadata.timestamp
            << ", keyframe=" << metadata.key_frame;

  CHECK_NE(metadata.payload_size_bytes, 0u);
  rate_ctrl_->PostEncodeUpdate(metadata.payload_size_bytes, frame_params);
}

}  // namespace media
