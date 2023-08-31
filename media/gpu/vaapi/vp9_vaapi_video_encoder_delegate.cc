// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vp9_vaapi_video_encoder_delegate.h"

#include <algorithm>
#include <numeric>

#include <va/va.h>

#include "base/bits.h"
#include "base/memory/ref_counted_memory.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "media/gpu/gpu_video_encode_accelerator_helpers.h"
#include "media/gpu/macros.h"
#include "media/gpu/vaapi/vaapi_common.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "media/gpu/vp9_svc_layers.h"
#include "third_party/libvpx/source/libvpx/vp9/ratectrl_rtc.h"

namespace media {

namespace {
// Keyframe period.
constexpr size_t kKFPeriod = 3000;

// Quantization parameter. They are vp9 ac/dc indices and their ranges are
// 0-255. These are based on WebRTC's defaults.
constexpr uint8_t kMinQP = 8;
constexpr uint8_t kMaxQP = 208;
constexpr uint8_t kScreenMinQP = 32;
constexpr uint8_t kScreenMaxQP = kMaxQP;

// Convert Qindex, whose range is 0-255, to the quantizer parameter used in
// libvpx vp9 rate control, whose range is 0-63.
// Cited from //third_party/libvpx/source/libvpx/vp9/encoder/vp9_quantize.cc.
uint8_t QindexToQuantizer(uint8_t q_index) {
  constexpr uint8_t kQuantizerToQindex[] = {
      0,   4,   8,   12,  16,  20,  24,  28,  32,  36,  40,  44,  48,
      52,  56,  60,  64,  68,  72,  76,  80,  84,  88,  92,  96,  100,
      104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 144, 148, 152,
      156, 160, 164, 168, 172, 176, 180, 184, 188, 192, 196, 200, 204,
      208, 212, 216, 220, 224, 228, 232, 236, 240, 244, 249, 255,
  };

  for (size_t q = 0; q < std::size(kQuantizerToQindex); ++q) {
    if (kQuantizerToQindex[q] >= q_index)
      return q;
  }
  return std::size(kQuantizerToQindex) - 1;
}

// TODO(crbug.com/752720): remove this in favor of std::gcd if c++17 is enabled
// to use.
int GCD(int a, int b) {
  return a == 0 ? b : GCD(b % a, a);
}

// The return value is expressed as a percentage of the average. For example,
// to allocate no more than 4.5 frames worth of bitrate to a keyframe, the
// return value is 450.
uint32_t MaxSizeOfKeyframeAsPercentage(uint32_t optimal_buffer_size,
                                       uint32_t max_framerate) {
  // Set max to the optimal buffer level (normalized by target BR),
  // and scaled by a scale_par.
  // Max target size = scale_par * optimal_buffer_size * targetBR[Kbps].
  // This value is presented in percentage of perFrameBw:
  // perFrameBw = targetBR[Kbps] * 1000 / framerate.
  // The target in % is as follows:
  const double target_size_byte_per_frame = optimal_buffer_size * 0.5;
  const uint32_t target_size_kbyte =
      target_size_byte_per_frame * max_framerate / 1000;
  const uint32_t target_size_kbyte_as_percent = target_size_kbyte * 100;

  // Don't go below 3 times the per frame bandwidth.
  constexpr uint32_t kMinIntraSizePercentage = 300u;
  return std::max(kMinIntraSizePercentage, target_size_kbyte_as_percent);
}

libvpx::VP9RateControlRtcConfig CreateRateControlConfig(
    const VP9VaapiVideoEncoderDelegate::EncodeParams& encode_params,
    const VideoBitrateAllocation& bitrate_allocation,
    const size_t num_temporal_layers,
    const std::vector<gfx::Size>& spatial_layer_resolutions) {
  DCHECK(!spatial_layer_resolutions.empty());
  const gfx::Size& encode_size = spatial_layer_resolutions.back();
  const size_t num_spatial_layers = spatial_layer_resolutions.size();
  libvpx::VP9RateControlRtcConfig rc_cfg{};
  rc_cfg.rc_mode = VPX_CBR;
  rc_cfg.width = encode_size.width();
  rc_cfg.height = encode_size.height();
  rc_cfg.max_quantizer = QindexToQuantizer(encode_params.max_qp);
  rc_cfg.min_quantizer = QindexToQuantizer(encode_params.min_qp);
  // libvpx::VP9RateControlRtcConfig is kbps.
  rc_cfg.target_bandwidth = encode_params.bitrate_allocation.GetSumBps() / 1000;
  // These default values come from
  // //third_party/webrtc/modules/video_coding/codecs/vp9/vp9_impl.cc.
  rc_cfg.buf_initial_sz = 500;
  rc_cfg.buf_optimal_sz = 600;
  rc_cfg.buf_sz = 1000;
  rc_cfg.undershoot_pct = 50;
  rc_cfg.overshoot_pct = 50;
  rc_cfg.max_intra_bitrate_pct = MaxSizeOfKeyframeAsPercentage(
      rc_cfg.buf_optimal_sz, encode_params.framerate);
  rc_cfg.framerate = encode_params.framerate;

  // Fill spatial/temporal layers variables.
  rc_cfg.ss_number_layers = num_spatial_layers;
  rc_cfg.ts_number_layers = num_temporal_layers;
  for (size_t tid = 0; tid < num_temporal_layers; ++tid) {
    rc_cfg.ts_rate_decimator[tid] = 1u << (num_temporal_layers - tid - 1);
  }
  for (size_t sid = 0; sid < num_spatial_layers; ++sid) {
    int gcd =
        GCD(encode_size.height(), spatial_layer_resolutions[sid].height());
    rc_cfg.scaling_factor_num[sid] =
        spatial_layer_resolutions[sid].height() / gcd;
    rc_cfg.scaling_factor_den[sid] = encode_size.height() / gcd;
    int bitrate_sum = 0;
    for (size_t tid = 0; tid < num_temporal_layers; ++tid) {
      size_t idx = sid * num_temporal_layers + tid;
      rc_cfg.max_quantizers[idx] = rc_cfg.max_quantizer;
      rc_cfg.min_quantizers[idx] = rc_cfg.min_quantizer;
      bitrate_sum += bitrate_allocation.GetBitrateBps(sid, tid);
      rc_cfg.layer_target_bitrate[idx] = bitrate_sum / 1000;
    }
  }
  return rc_cfg;
}

scoped_refptr<VP9Picture> GetVP9Picture(
    const VaapiVideoEncoderDelegate::EncodeJob& job) {
  return base::WrapRefCounted(
      reinterpret_cast<VP9Picture*>(job.picture().get()));
}
}  // namespace

std::unique_ptr<VP9RateControlWrapper> VP9RateControlWrapper::Create(
    const libvpx::VP9RateControlRtcConfig& config) {
  auto impl = libvpx::VP9RateControlRTC::Create(config);
  if (!impl) {
    DLOG(ERROR) << "Failed creating video RateControlRTC";
    return nullptr;
  }
  return std::make_unique<VP9RateControlWrapper>(std::move(impl));
}

VP9RateControlWrapper::VP9RateControlWrapper() = default;
VP9RateControlWrapper::VP9RateControlWrapper(
    std::unique_ptr<libvpx::VP9RateControlRTC> impl)
    : impl_(std::move(impl)) {}

void VP9RateControlWrapper::UpdateRateControl(
    const libvpx::VP9RateControlRtcConfig& rate_control_config) {
  impl_->UpdateRateControl(rate_control_config);
}

VP9RateControlWrapper::~VP9RateControlWrapper() = default;

int VP9RateControlWrapper::ComputeQP(
    const libvpx::VP9FrameParamsQpRTC& frame_params) {
  impl_->ComputeQP(frame_params);
  return impl_->GetQP();
}

void VP9RateControlWrapper::PostEncodeUpdate(
    uint64_t encoded_frame_size,
    const libvpx::VP9FrameParamsQpRTC& frame_params) {
  impl_->PostEncodeUpdate(encoded_frame_size, frame_params);
}

int VP9RateControlWrapper::GetLoopfilterLevel() const {
  return impl_->GetLoopfilterLevel();
}

VP9VaapiVideoEncoderDelegate::EncodeParams::EncodeParams()
    : kf_period_frames(kKFPeriod),
      framerate(0),
      min_qp(kMinQP),
      max_qp(kMaxQP) {}

void VP9VaapiVideoEncoderDelegate::set_rate_ctrl_for_testing(
    std::unique_ptr<VP9RateControlWrapper> rate_ctrl) {
  rate_ctrl_ = std::move(rate_ctrl);
}

VP9VaapiVideoEncoderDelegate::VP9VaapiVideoEncoderDelegate(
    scoped_refptr<VaapiWrapper> vaapi_wrapper,
    base::RepeatingClosure error_cb)
    : VaapiVideoEncoderDelegate(std::move(vaapi_wrapper), error_cb) {}

VP9VaapiVideoEncoderDelegate::~VP9VaapiVideoEncoderDelegate() = default;

bool VP9VaapiVideoEncoderDelegate::Initialize(
    const VideoEncodeAccelerator::Config& config,
    const VaapiVideoEncoderDelegate::Config& ave_config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (VideoCodecProfileToVideoCodec(config.output_profile) !=
      VideoCodec::kVP9) {
    DVLOGF(1) << "Invalid profile: " << GetProfileName(config.output_profile);
    return false;
  }

  if (config.input_visible_size.IsEmpty()) {
    DVLOGF(1) << "Input visible size could not be empty";
    return false;
  }

  if (config.bitrate.mode() == Bitrate::Mode::kVariable) {
    DVLOGF(1) << "Invalid configuraiton. VBR is not supported for VP9.";
    return false;
  }

  visible_size_ = config.input_visible_size;
  coded_size_ = gfx::Size(base::bits::AlignUp(visible_size_.width(), 16),
                          base::bits::AlignUp(visible_size_.height(), 16));
  current_params_ = EncodeParams();
  if (config.content_type ==
      VideoEncodeAccelerator::Config::ContentType::kDisplay) {
    current_params_.min_qp = kScreenMinQP;
    current_params_.max_qp = kScreenMaxQP;
  }

  reference_frames_.Clear();
  frame_num_ = 0;

  size_t num_temporal_layers = 1;
  size_t num_spatial_layers = 1;
  std::vector<gfx::Size> spatial_layer_resolutions;
  if (config.HasTemporalLayer() || config.HasSpatialLayer()) {
    num_spatial_layers = config.spatial_layers.size();
    num_temporal_layers = config.spatial_layers[0].num_of_temporal_layers;
    DCHECK(num_spatial_layers != 1 || num_temporal_layers != 1);
    for (size_t sid = 1; sid < num_spatial_layers; ++sid) {
      if (num_temporal_layers !=
          config.spatial_layers[sid].num_of_temporal_layers) {
        VLOGF(1) << "The temporal layer sizes among spatial layers must be "
                    "identical";
        return false;
      }
    }
    if (num_spatial_layers > VP9SVCLayers::kMaxSpatialLayers ||
        num_temporal_layers > VP9SVCLayers::kMaxSupportedTemporalLayers) {
      VLOGF(1) << "Unsupported amount of spatial/temporal layers: "
               << ", Spatial layer number: " << num_spatial_layers
               << ", Temporal layer number: " << num_temporal_layers;
      return false;
    }
    if (num_spatial_layers > 1 &&
        config.inter_layer_pred != SVCInterLayerPredMode::kOnKeyPic) {
      std::string inter_layer_pred;
      if (config.inter_layer_pred == SVCInterLayerPredMode::kOn) {
        inter_layer_pred = base::StringPrintf("InterLayerPredMode::kOn");
      } else {
        inter_layer_pred = base::StringPrintf("InterLayerPredMode::kOff");
      }
      VLOGF(1) << "Support only k-SVC encoding. inter_layer_pred="
               << inter_layer_pred;
      return false;
    }
    for (const auto& spatial_layer : config.spatial_layers) {
      spatial_layer_resolutions.emplace_back(
          gfx::Size(spatial_layer.width, spatial_layer.height));
    }
    svc_layers_ = std::make_unique<VP9SVCLayers>(config.spatial_layers);

    current_params_.error_resilident_mode = true;
  }

  // Store layer size for vp9 simple stream.
  if (spatial_layer_resolutions.empty())
    spatial_layer_resolutions.push_back(visible_size_);

  auto initial_bitrate_allocation = AllocateBitrateForDefaultEncoding(config);

  // |rate_ctrl_| might be injected for tests.
  if (!rate_ctrl_) {
    rate_ctrl_ = VP9RateControlWrapper::Create(CreateRateControlConfig(
        current_params_, initial_bitrate_allocation, num_temporal_layers,
        spatial_layer_resolutions));
  }
  if (!rate_ctrl_)
    return false;

  DCHECK(!pending_update_rates_);
  pending_update_rates_ =
      std::make_pair(initial_bitrate_allocation,
                     config.initial_framerate.value_or(
                         VideoEncodeAccelerator::kDefaultFramerate));

  return ApplyPendingUpdateRates();
}

gfx::Size VP9VaapiVideoEncoderDelegate::GetCodedSize() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!coded_size_.IsEmpty());

  return coded_size_;
}

size_t VP9VaapiVideoEncoderDelegate::GetMaxNumOfRefFrames() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return kVp9NumRefFrames;
}

bool VP9VaapiVideoEncoderDelegate::PrepareEncodeJob(EncodeJob& encode_job) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (svc_layers_) {
    if (svc_layers_->UpdateEncodeJob(encode_job.IsKeyframeRequested(),
                                     current_params_.kf_period_frames)) {
      encode_job.ProduceKeyframe();
    }
  } else {
    if (encode_job.IsKeyframeRequested())
      frame_num_ = 0;

    if (frame_num_ == 0)
      encode_job.ProduceKeyframe();

    frame_num_++;
    frame_num_ %= current_params_.kf_period_frames;
  }

  scoped_refptr<VP9Picture> picture = GetVP9Picture(encode_job);
  DCHECK(picture);

  std::array<bool, kVp9NumRefsPerFrame> ref_frames_used = {false, false, false};
  SetFrameHeader(encode_job.IsKeyframeRequested(), picture.get(),
                 &ref_frames_used);
  if (!SubmitFrameParameters(encode_job, current_params_, picture,
                             reference_frames_, ref_frames_used)) {
    LOG(ERROR) << "Failed submitting frame parameters";
    return false;
  }

  UpdateReferenceFrames(picture);
  return true;
}

BitstreamBufferMetadata VP9VaapiVideoEncoderDelegate::GetMetadata(
    const EncodeJob& encode_job,
    size_t payload_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto metadata =
      VaapiVideoEncoderDelegate::GetMetadata(encode_job, payload_size);
  auto picture = GetVP9Picture(encode_job);
  DCHECK(picture);
  metadata.vp9 = picture->metadata_for_encoding;
  metadata.qp =
      base::strict_cast<int32_t>(picture->frame_hdr->quant_params.base_q_idx);
  return metadata;
}

std::vector<gfx::Size> VP9VaapiVideoEncoderDelegate::GetSVCLayerResolutions() {
  if (!ApplyPendingUpdateRates()) {
    DLOG(ERROR) << __func__ << " ApplyPendingUpdateRates failed";
    return {};
  }
  if (svc_layers_) {
    return svc_layers_->active_spatial_layer_resolutions();
  } else {
    return {visible_size_};
  }
}

void VP9VaapiVideoEncoderDelegate::BitrateControlUpdate(
    const BitstreamBufferMetadata& metadata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(rate_ctrl_);

  libvpx::VP9FrameParamsQpRTC frame_params{};
  frame_params.frame_type = metadata.key_frame
                                ? libvpx::RcFrameType::kKeyFrame
                                : libvpx::RcFrameType::kInterFrame;
  if (metadata.vp9) {
    frame_params.spatial_layer_id =
        base::saturated_cast<int>(metadata.vp9->spatial_idx);
    frame_params.temporal_layer_id =
        base::saturated_cast<int>(metadata.vp9->temporal_idx);
  }
  DVLOGF(4) << "spatial_idx=" << (metadata.vp9 ? metadata.vp9->spatial_idx : 0)
            << ", temporal_idx="
            << (metadata.vp9 ? metadata.vp9->temporal_idx : 0)
            << ", encoded chunk size=" << metadata.payload_size_bytes;
  rate_ctrl_->PostEncodeUpdate(metadata.payload_size_bytes, frame_params);
}

bool VP9VaapiVideoEncoderDelegate::ApplyPendingUpdateRates() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!pending_update_rates_)
    return true;

  DVLOGF(2) << "New bitrate: " << pending_update_rates_->first.ToString()
            << ", new framerate: " << pending_update_rates_->second;

  current_params_.bitrate_allocation = pending_update_rates_->first;
  current_params_.framerate = pending_update_rates_->second;
  pending_update_rates_.reset();

  // Update active layer status in |svc_layers_|, and key frame is produced when
  // active layer changed.
  if (svc_layers_) {
    if (!svc_layers_->MaybeUpdateActiveLayer(
            &current_params_.bitrate_allocation)) {
      return false;
    }
  } else {
    // Simple stream encoding.
    if (current_params_.bitrate_allocation.GetSumBps() !=
        current_params_.bitrate_allocation.GetBitrateBps(0, 0)) {
      return false;
    }
  }

  CHECK(rate_ctrl_);

  const size_t num_temporal_layers =
      svc_layers_ ? svc_layers_->num_temporal_layers() : 1u;
  std::vector<gfx::Size> spatial_layer_resolutions = {visible_size_};
  if (svc_layers_)
    spatial_layer_resolutions = svc_layers_->active_spatial_layer_resolutions();
  rate_ctrl_->UpdateRateControl(CreateRateControlConfig(
      current_params_, current_params_.bitrate_allocation, num_temporal_layers,
      spatial_layer_resolutions));
  return true;
}

bool VP9VaapiVideoEncoderDelegate::UpdateRates(
    const VideoBitrateAllocation& bitrate_allocation,
    uint32_t framerate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (bitrate_allocation.GetMode() != Bitrate::Mode::kConstant) {
    DLOG(ERROR) << "VBR is not supported for VP9 but was requested.";
    return false;
  }

  if (bitrate_allocation.GetSumBps() == 0u || framerate == 0)
    return false;

  pending_update_rates_ = std::make_pair(bitrate_allocation, framerate);
  if (current_params_.bitrate_allocation == pending_update_rates_->first &&
      current_params_.framerate == pending_update_rates_->second) {
    pending_update_rates_.reset();
  }
  return true;
}

Vp9FrameHeader VP9VaapiVideoEncoderDelegate::GetDefaultFrameHeader(
    const bool keyframe) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  Vp9FrameHeader hdr{};
  DCHECK(!visible_size_.IsEmpty());
  hdr.frame_width = visible_size_.width();
  hdr.frame_height = visible_size_.height();
  hdr.render_width = visible_size_.width();
  hdr.render_height = visible_size_.height();
  hdr.show_frame = true;
  hdr.frame_type =
      keyframe ? Vp9FrameHeader::KEYFRAME : Vp9FrameHeader::INTERFRAME;
  return hdr;
}

void VP9VaapiVideoEncoderDelegate::SetFrameHeader(
    bool keyframe,
    VP9Picture* picture,
    std::array<bool, kVp9NumRefsPerFrame>* ref_frames_used) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(picture);
  DCHECK(ref_frames_used);

  *picture->frame_hdr = GetDefaultFrameHeader(keyframe);
  picture->frame_hdr->refresh_frame_context =
      !current_params_.error_resilident_mode;
  if (svc_layers_) {
    // Reference frame settings for k-SVC stream.
    svc_layers_->FillUsedRefFramesAndMetadata(picture, ref_frames_used);
  } else {
    // Reference frame settings for simple stream.
    if (keyframe) {
      picture->frame_hdr->refresh_frame_flags = 0xff;
      ref_frame_index_ = 0;

      // TODO(b/297226972): Remove the workaround once the iHD driver is fixed.
      // Consecutive key frames must not refresh the frame context in iHD-VP9 to
      // avoid corruption.
      if (VaapiWrapper::GetImplementationType() ==
              VAImplementation::kIntelIHD &&
          is_last_encoded_key_frame_) {
        picture->frame_hdr->refresh_frame_context = false;
      }
    } else {
      picture->frame_hdr->ref_frame_idx[0] = ref_frame_index_;
      picture->frame_hdr->ref_frame_idx[1] =
          (ref_frame_index_ - 1) & (kVp9NumRefFrames - 1);
      picture->frame_hdr->ref_frame_idx[2] =
          (ref_frame_index_ - 2) & (kVp9NumRefFrames - 1);
      ref_frame_index_ = (ref_frame_index_ + 1) % kVp9NumRefFrames;
      picture->frame_hdr->refresh_frame_flags = 1 << ref_frame_index_;
      // Use last, golden, alt frames.
      ref_frames_used->fill(true);
    }
  }

  CHECK(rate_ctrl_);
  libvpx::VP9FrameParamsQpRTC frame_params{};
  frame_params.frame_type = keyframe ? libvpx::RcFrameType::kKeyFrame
                                     : libvpx::RcFrameType::kInterFrame;
  if (picture->metadata_for_encoding) {
    frame_params.temporal_layer_id =
        picture->metadata_for_encoding->temporal_idx;
    frame_params.spatial_layer_id = picture->metadata_for_encoding->spatial_idx;
  }
  picture->frame_hdr->quant_params.base_q_idx =
      rate_ctrl_->ComputeQP(frame_params);
  picture->frame_hdr->loop_filter.level = rate_ctrl_->GetLoopfilterLevel();
  DVLOGF(4) << "qp="
            << static_cast<int>(picture->frame_hdr->quant_params.base_q_idx)
            << ", filter_level="
            << static_cast<int>(picture->frame_hdr->loop_filter.level)
            << (keyframe ? " (keyframe)" : "")
            << (picture->metadata_for_encoding
                    ? (" spatial_id=" +
                       base::NumberToString(frame_params.spatial_layer_id) +
                       ", temporal_id=" +
                       base::NumberToString(frame_params.temporal_layer_id))
                    : "");

  is_last_encoded_key_frame_ = keyframe;
}

void VP9VaapiVideoEncoderDelegate::UpdateReferenceFrames(
    scoped_refptr<VP9Picture> picture) {
  reference_frames_.Refresh(picture);
}

bool VP9VaapiVideoEncoderDelegate::SubmitFrameParameters(
    EncodeJob& job,
    const EncodeParams& encode_params,
    scoped_refptr<VP9Picture> pic,
    const Vp9ReferenceFrameVector& ref_frames,
    const std::array<bool, kVp9NumRefsPerFrame>& ref_frames_used) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  VAEncSequenceParameterBufferVP9 seq_param = {};

  const auto& frame_header = pic->frame_hdr;
  // TODO(crbug.com/811912): Double check whether the
  // max_frame_width or max_frame_height affects any of the memory
  // allocation and tighten these values based on that.
  constexpr gfx::Size kMaxFrameSize(4096, 4096);
  seq_param.max_frame_width = kMaxFrameSize.height();
  seq_param.max_frame_height = kMaxFrameSize.width();
  seq_param.bits_per_second = encode_params.bitrate_allocation.GetSumBps();
  seq_param.intra_period = encode_params.kf_period_frames;

  VAEncPictureParameterBufferVP9 pic_param = {};

  pic_param.frame_width_src = frame_header->frame_width;
  pic_param.frame_height_src = frame_header->frame_height;
  pic_param.frame_width_dst = frame_header->render_width;
  pic_param.frame_height_dst = frame_header->render_height;

  pic_param.reconstructed_frame = pic->AsVaapiVP9Picture()->GetVASurfaceID();
  DCHECK_NE(pic_param.reconstructed_frame, VA_INVALID_ID);

  for (size_t i = 0; i < kVp9NumRefFrames; i++) {
    auto ref_pic = ref_frames.GetFrame(i);
    pic_param.reference_frames[i] =
        ref_pic ? ref_pic->AsVaapiVP9Picture()->GetVASurfaceID()
                : VA_INVALID_ID;
  }

  pic_param.coded_buf = job.coded_buffer_id();
  DCHECK_NE(pic_param.coded_buf, VA_INVALID_ID);

  if (frame_header->IsKeyframe()) {
    pic_param.ref_flags.bits.force_kf = true;
  } else {
    // Non-key frame mode, the frame has at least 1 reference frames.
    size_t first_used_ref_frame = 3;
    for (size_t i = 0; i < kVp9NumRefsPerFrame; i++) {
      if (ref_frames_used[i]) {
        first_used_ref_frame = std::min(first_used_ref_frame, i);
        pic_param.ref_flags.bits.ref_frame_ctrl_l0 |= (1 << i);
      }
    }
    CHECK_LT(first_used_ref_frame, 3u);

    pic_param.ref_flags.bits.ref_last_idx =
        ref_frames_used[0] ? frame_header->ref_frame_idx[0]
                           : frame_header->ref_frame_idx[first_used_ref_frame];
    pic_param.ref_flags.bits.ref_gf_idx =
        ref_frames_used[1] ? frame_header->ref_frame_idx[1]
                           : frame_header->ref_frame_idx[first_used_ref_frame];
    pic_param.ref_flags.bits.ref_arf_idx =
        ref_frames_used[2] ? frame_header->ref_frame_idx[2]
                           : frame_header->ref_frame_idx[first_used_ref_frame];
  }

  pic_param.pic_flags.bits.frame_type = frame_header->frame_type;
  pic_param.pic_flags.bits.show_frame = frame_header->show_frame;
  pic_param.pic_flags.bits.error_resilient_mode =
      encode_params.error_resilident_mode;
  pic_param.pic_flags.bits.intra_only = frame_header->intra_only;
  pic_param.pic_flags.bits.allow_high_precision_mv =
      frame_header->allow_high_precision_mv;
  pic_param.pic_flags.bits.mcomp_filter_type =
      frame_header->interpolation_filter;
  pic_param.pic_flags.bits.frame_parallel_decoding_mode =
      frame_header->frame_parallel_decoding_mode;
  pic_param.pic_flags.bits.reset_frame_context =
      frame_header->reset_frame_context;
  pic_param.pic_flags.bits.refresh_frame_context =
      frame_header->refresh_frame_context;
  pic_param.pic_flags.bits.frame_context_idx = frame_header->frame_context_idx;

  pic_param.refresh_frame_flags = frame_header->refresh_frame_flags;

  pic_param.luma_ac_qindex = frame_header->quant_params.base_q_idx;
  pic_param.luma_dc_qindex_delta = frame_header->quant_params.delta_q_y_dc;
  pic_param.chroma_ac_qindex_delta = frame_header->quant_params.delta_q_uv_ac;
  pic_param.chroma_dc_qindex_delta = frame_header->quant_params.delta_q_uv_dc;
  pic_param.filter_level = frame_header->loop_filter.level;
  pic_param.log2_tile_rows = frame_header->tile_rows_log2;
  pic_param.log2_tile_columns = frame_header->tile_cols_log2;

  return vaapi_wrapper_->SubmitBuffers(
      {{VAEncSequenceParameterBufferType, sizeof(seq_param), &seq_param},
       {VAEncPictureParameterBufferType, sizeof(pic_param), &pic_param}});
}

}  // namespace media
