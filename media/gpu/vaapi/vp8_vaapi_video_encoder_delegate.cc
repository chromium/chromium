// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vp8_vaapi_video_encoder_delegate.h"

#include <va/va.h>
#include <va/va_enc_vp8.h>

#include "base/bits.h"
#include "base/memory/ref_counted_memory.h"
#include "media/gpu/macros.h"
#include "media/gpu/vaapi/vaapi_common.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "third_party/libvpx/source/libvpx/vp8/vp8_ratectrl_rtc.h"

namespace media {

namespace {
// Keyframe period.
constexpr size_t kKFPeriod = 3000;

// Quantization parameter. They are vp8 ac/dc indices and their ranges are
// 0-127. Based on WebRTC's defaults.
constexpr uint8_t kMinQP = 4;
// b/110059922, crbug.com/1001900: Tuned 112->117 for bitrate issue in a lower
// resolution (180p).
constexpr uint8_t kMaxQP = 117;

// The upper limitation of the quantization parameter for the software rate
// controller. This is larger than |kMaxQP| because a driver might ignore the
// specified maximum quantization parameter when the driver determines the
// value, but it doesn't ignore the quantization parameter by the software rate
// controller.
constexpr uint8_t kMaxQPForSoftwareRateCtrl = 127;

// Convert Qindex, whose range is 0-127, to the quantizer parameter used in
// libvpx vp8 rate control, whose range is 0-63.
// Cited from //third_party/libvpx/source/libvpx/vp8/vp8_ratectrl_rtc.cc
uint8_t QindexToQuantizer(uint8_t q_index) {
  constexpr uint8_t kQuantizerToQindex[] = {
      0,  1,  2,  3,  4,  5,  7,   8,   9,   10,  12,  13,  15,  17,  18,  19,
      20, 21, 23, 24, 25, 26, 27,  28,  29,  30,  31,  33,  35,  37,  39,  41,
      43, 45, 47, 49, 51, 53, 55,  57,  59,  61,  64,  67,  70,  73,  76,  79,
      82, 85, 88, 91, 94, 97, 100, 103, 106, 109, 112, 115, 118, 121, 124, 127,
  };

  for (size_t q = 0; q < base::size(kQuantizerToQindex); ++q) {
    if (kQuantizerToQindex[q] >= q_index)
      return q;
  }
  return base::size(kQuantizerToQindex) - 1;
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

libvpx::VP8RateControlRtcConfig CreateRateControlConfig(
    const gfx::Size encode_size,
    const VP8VaapiVideoEncoderDelegate::EncodeParams& encode_params,
    const VideoBitrateAllocation& bitrate_allocation) {
  libvpx::VP8RateControlRtcConfig rc_cfg{};
  rc_cfg.width = encode_size.width();
  rc_cfg.height = encode_size.height();
  rc_cfg.rc_mode = VPX_CBR;
  rc_cfg.max_quantizer = QindexToQuantizer(encode_params.max_qp);
  rc_cfg.min_quantizer = QindexToQuantizer(encode_params.min_qp);
  // libvpx::VP8RateControlRtcConfig is kbps.
  rc_cfg.target_bandwidth = encode_params.bitrate_allocation.GetSumBps() / 1000;
  // These default values come from
  // //third_party/webrtc/modules/video_coding/codecs/vp8/libvpx_vp8_encoder.cc
  rc_cfg.buf_initial_sz = 500;
  rc_cfg.buf_optimal_sz = 600;
  rc_cfg.buf_sz = 1000;
  rc_cfg.undershoot_pct = 100;
  rc_cfg.overshoot_pct = 15;
  rc_cfg.max_intra_bitrate_pct = MaxSizeOfKeyframeAsPercentage(
      rc_cfg.buf_optimal_sz, encode_params.framerate);
  rc_cfg.framerate = encode_params.framerate;

  // Fill temporal layers variables.
  rc_cfg.ts_number_layers = 1;
  rc_cfg.layer_target_bitrate[0] =
      bitrate_allocation.GetBitrateBps(0, 0) / 1000;
  rc_cfg.ts_rate_decimator[0] = 1u;
  return rc_cfg;
}

scoped_refptr<VP8Picture> GetVP8Picture(
    const VaapiVideoEncoderDelegate::EncodeJob& job) {
  return base::WrapRefCounted(
      reinterpret_cast<VP8Picture*>(job.picture().get()));
}

Vp8FrameHeader GetDefaultVp8FrameHeader(bool keyframe,
                                        const gfx::Size& visible_size) {
  Vp8FrameHeader hdr;
  DCHECK(!visible_size.IsEmpty());
  hdr.width = visible_size.width();
  hdr.height = visible_size.height();
  hdr.show_frame = true;
  hdr.frame_type =
      keyframe ? Vp8FrameHeader::KEYFRAME : Vp8FrameHeader::INTERFRAME;

  // TODO(sprang): Make this dynamic. Value based on reference implementation
  // in libyami (https://github.com/intel/libyami).

  // Sets the highest loop filter level.
  // TODO(b/188853141): Set a loop filter level computed by a rate controller
  // every frame once the rate controller supports it.
  hdr.loopfilter_hdr.level = 63;

  // A VA-API driver recommends to set forced_lf_adjustment on keyframe.
  // Set loop_filter_adj_enable to 1 here because forced_lf_adjustment is read
  // only when a macroblock level loop filter adjustment.
  hdr.loopfilter_hdr.loop_filter_adj_enable = true;

  // Set mb_no_skip_coeff to 1 that some decoders (e.g. kepler) could not decode
  // correctly a stream encoded with mb_no_skip_coeff=0. It also enables an
  // encoder to produce a more optimized stream than when mb_no_skip_coeff=0.
  hdr.mb_no_skip_coeff = true;

  return hdr;
}
}  // namespace

VP8VaapiVideoEncoderDelegate::EncodeParams::EncodeParams()
    : kf_period_frames(kKFPeriod),
      framerate(0),
      min_qp(kMinQP),
      max_qp(kMaxQP) {}

void VP8VaapiVideoEncoderDelegate::Reset() {
  current_params_ = EncodeParams();
  reference_frames_.Clear();
  frame_num_ = 0;
}

VP8VaapiVideoEncoderDelegate::VP8VaapiVideoEncoderDelegate(
    scoped_refptr<VaapiWrapper> vaapi_wrapper,
    base::RepeatingClosure error_cb)
    : VaapiVideoEncoderDelegate(std::move(vaapi_wrapper), error_cb) {}

VP8VaapiVideoEncoderDelegate::~VP8VaapiVideoEncoderDelegate() {
  // VP8VaapiVideoEncoderDelegate can be destroyed on any thread.
}

bool VP8VaapiVideoEncoderDelegate::Initialize(
    const VideoEncodeAccelerator::Config& config,
    const VaapiVideoEncoderDelegate::Config& ave_config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (VideoCodecProfileToVideoCodec(config.output_profile) !=
      VideoCodec::kVP8) {
    DVLOGF(1) << "Invalid profile: " << GetProfileName(config.output_profile);
    return false;
  }

  if (config.input_visible_size.IsEmpty()) {
    DVLOGF(1) << "Input visible size could not be empty";
    return false;
  }

  // Even though VP8VaapiVideoEncoderDelegate might support other bitrate
  // control modes, only the kConstantQuantizationParameter is used.
  if (ave_config.bitrate_control != VaapiVideoEncoderDelegate::BitrateControl::
                                        kConstantQuantizationParameter) {
    DVLOGF(1) << "Only CQ bitrate control is supported";
    return false;
  }

  native_input_mode_ = ave_config.native_input_mode;

  visible_size_ = config.input_visible_size;
  coded_size_ = gfx::Size(base::bits::AlignUp(visible_size_.width(), 16),
                          base::bits::AlignUp(visible_size_.height(), 16));

  Reset();

  VideoBitrateAllocation initial_bitrate_allocation;
  initial_bitrate_allocation.SetBitrate(0, 0, config.bitrate.target());

  current_params_.max_qp = kMaxQPForSoftwareRateCtrl;

  // |rate_ctrl_| might be injected for tests.
  if (!rate_ctrl_) {
    rate_ctrl_ = VP8RateControl::Create(CreateRateControlConfig(
        visible_size_, current_params_, initial_bitrate_allocation));
    if (!rate_ctrl_)
      return false;
  }

  return UpdateRates(initial_bitrate_allocation,
                     config.initial_framerate.value_or(
                         VideoEncodeAccelerator::kDefaultFramerate));
}

gfx::Size VP8VaapiVideoEncoderDelegate::GetCodedSize() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!coded_size_.IsEmpty());

  return coded_size_;
}

size_t VP8VaapiVideoEncoderDelegate::GetMaxNumOfRefFrames() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return kNumVp8ReferenceBuffers;
}

std::vector<gfx::Size> VP8VaapiVideoEncoderDelegate::GetSVCLayerResolutions() {
  return {visible_size_};
}

bool VP8VaapiVideoEncoderDelegate::PrepareEncodeJob(EncodeJob& encode_job) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (encode_job.IsKeyframeRequested())
    frame_num_ = 0;

  if (frame_num_ == 0)
    encode_job.ProduceKeyframe();

  frame_num_++;
  frame_num_ %= current_params_.kf_period_frames;

  scoped_refptr<VP8Picture> picture = GetVP8Picture(encode_job);
  DCHECK(picture);

  SetFrameHeader(*picture, encode_job.IsKeyframeRequested());

  // We only use |last_frame| for a reference frame. This follows the behavior
  // of libvpx encoder in chromium webrtc use case.
  std::array<bool, kNumVp8ReferenceBuffers> ref_frames_used{true, false, false};

  if (picture->frame_hdr->IsKeyframe()) {
    // A driver should ignore |ref_frames_used| values if keyframe is requested.
    // But we fill false in |ref_frames_used| just in case.
    std::fill(std::begin(ref_frames_used), std::end(ref_frames_used), false);
  }

  if (!SubmitFrameParameters(encode_job, current_params_, picture,
                             reference_frames_, ref_frames_used)) {
    LOG(ERROR) << "Failed submitting frame parameters";
    return false;
  }

  UpdateReferenceFrames(picture);
  return true;
}

void VP8VaapiVideoEncoderDelegate::BitrateControlUpdate(
    uint64_t encoded_chunk_size_bytes) {
  if (!rate_ctrl_) {
    DLOG(ERROR) << __func__ << "() is called when no bitrate controller exists";
    return;
  }

  DVLOGF(4) << "|encoded_chunk_size_bytes|=" << encoded_chunk_size_bytes;
  rate_ctrl_->PostEncodeUpdate(encoded_chunk_size_bytes);
}

bool VP8VaapiVideoEncoderDelegate::UpdateRates(
    const VideoBitrateAllocation& bitrate_allocation,
    uint32_t framerate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  uint32_t bitrate = bitrate_allocation.GetSumBps();
  if (bitrate == 0 || framerate == 0)
    return false;

  if (current_params_.bitrate_allocation == bitrate_allocation &&
      current_params_.framerate == framerate) {
    return true;
  }
  VLOGF(2) << "New bitrate: " << bitrate_allocation.ToString()
           << ", new framerate: " << framerate;

  current_params_.bitrate_allocation = bitrate_allocation;
  current_params_.framerate = framerate;

  rate_ctrl_->UpdateRateControl(CreateRateControlConfig(
      visible_size_, current_params_, bitrate_allocation));

  return true;
}

void VP8VaapiVideoEncoderDelegate::SetFrameHeader(VP8Picture& picture,
                                                  bool keyframe) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
 *picture.frame_hdr = GetDefaultVp8FrameHeader(keyframe, visible_size_);
  picture.frame_hdr->refresh_last = true;
  if (keyframe) {
    picture.frame_hdr->refresh_golden_frame = true;
    picture.frame_hdr->refresh_alternate_frame = true;
    picture.frame_hdr->copy_buffer_to_golden =
        Vp8FrameHeader::NO_GOLDEN_REFRESH;
    picture.frame_hdr->copy_buffer_to_alternate =
        Vp8FrameHeader::NO_ALT_REFRESH;
  } else {
    picture.frame_hdr->refresh_golden_frame = false;
    picture.frame_hdr->refresh_alternate_frame = false;
    picture.frame_hdr->copy_buffer_to_golden =
        Vp8FrameHeader::COPY_LAST_TO_GOLDEN;
    picture.frame_hdr->copy_buffer_to_alternate =
        Vp8FrameHeader::COPY_GOLDEN_TO_ALT;
  }

  libvpx::VP8FrameParamsQpRTC frame_params{};
  frame_params.frame_type =
      keyframe ? FRAME_TYPE::KEY_FRAME : FRAME_TYPE::INTER_FRAME;
  frame_params.temporal_layer_id = 0;

  rate_ctrl_->ComputeQP(frame_params);
  picture.frame_hdr->quantization_hdr.y_ac_qi = rate_ctrl_->GetQP();
  DVLOGF(4) << "qp="
            << static_cast<int>(picture.frame_hdr->quantization_hdr.y_ac_qi);
}

void VP8VaapiVideoEncoderDelegate::UpdateReferenceFrames(
    scoped_refptr<VP8Picture> picture) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  reference_frames_.Refresh(picture);
}

bool VP8VaapiVideoEncoderDelegate::SubmitFrameParameters(
    EncodeJob& job,
    const EncodeParams& encode_params,
    scoped_refptr<VP8Picture> pic,
    const Vp8ReferenceFrameVector& ref_frames,
    const std::array<bool, kNumVp8ReferenceBuffers>& ref_frames_used) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  VAEncSequenceParameterBufferVP8 seq_param = {};

  const auto& frame_header = pic->frame_hdr;
  seq_param.frame_width = frame_header->width;
  seq_param.frame_height = frame_header->height;
  seq_param.frame_width_scale = frame_header->horizontal_scale;
  seq_param.frame_height_scale = frame_header->vertical_scale;
  seq_param.error_resilient = 1;
  seq_param.bits_per_second = encode_params.bitrate_allocation.GetSumBps();
  seq_param.intra_period = encode_params.kf_period_frames;

  VAEncPictureParameterBufferVP8 pic_param = {};

  pic_param.reconstructed_frame = pic->AsVaapiVP8Picture()->GetVASurfaceID();
  DCHECK_NE(pic_param.reconstructed_frame, VA_INVALID_ID);

  auto last_frame = ref_frames.GetFrame(Vp8RefType::VP8_FRAME_LAST);
  pic_param.ref_last_frame =
      last_frame ? last_frame->AsVaapiVP8Picture()->GetVASurfaceID()
                 : VA_INVALID_ID;
  auto golden_frame = ref_frames.GetFrame(Vp8RefType::VP8_FRAME_GOLDEN);
  pic_param.ref_gf_frame =
      golden_frame ? golden_frame->AsVaapiVP8Picture()->GetVASurfaceID()
                   : VA_INVALID_ID;
  auto alt_frame = ref_frames.GetFrame(Vp8RefType::VP8_FRAME_ALTREF);
  pic_param.ref_arf_frame =
      alt_frame ? alt_frame->AsVaapiVP8Picture()->GetVASurfaceID()
                : VA_INVALID_ID;
  pic_param.coded_buf = job.coded_buffer_id();
  DCHECK_NE(pic_param.coded_buf, VA_INVALID_ID);
  pic_param.ref_flags.bits.no_ref_last =
      !ref_frames_used[Vp8RefType::VP8_FRAME_LAST];
  pic_param.ref_flags.bits.no_ref_gf =
      !ref_frames_used[Vp8RefType::VP8_FRAME_GOLDEN];
  pic_param.ref_flags.bits.no_ref_arf =
      !ref_frames_used[Vp8RefType::VP8_FRAME_ALTREF];

  if (frame_header->IsKeyframe()) {
    pic_param.ref_flags.bits.force_kf = true;
  }

  pic_param.pic_flags.bits.frame_type = frame_header->frame_type;
  pic_param.pic_flags.bits.version = frame_header->version;
  pic_param.pic_flags.bits.show_frame = frame_header->show_frame;
  pic_param.pic_flags.bits.loop_filter_type = frame_header->loopfilter_hdr.type;
  pic_param.pic_flags.bits.num_token_partitions =
      frame_header->num_of_dct_partitions;
  pic_param.pic_flags.bits.segmentation_enabled =
      frame_header->segmentation_hdr.segmentation_enabled;
  pic_param.pic_flags.bits.update_mb_segmentation_map =
      frame_header->segmentation_hdr.update_mb_segmentation_map;
  pic_param.pic_flags.bits.update_segment_feature_data =
      frame_header->segmentation_hdr.update_segment_feature_data;

  pic_param.pic_flags.bits.loop_filter_adj_enable =
      frame_header->loopfilter_hdr.loop_filter_adj_enable;

  pic_param.pic_flags.bits.refresh_entropy_probs =
      frame_header->refresh_entropy_probs;
  pic_param.pic_flags.bits.refresh_golden_frame =
      frame_header->refresh_golden_frame;
  pic_param.pic_flags.bits.refresh_alternate_frame =
      frame_header->refresh_alternate_frame;
  pic_param.pic_flags.bits.refresh_last = frame_header->refresh_last;
  pic_param.pic_flags.bits.copy_buffer_to_golden =
      frame_header->copy_buffer_to_golden;
  pic_param.pic_flags.bits.copy_buffer_to_alternate =
      frame_header->copy_buffer_to_alternate;
  pic_param.pic_flags.bits.sign_bias_golden = frame_header->sign_bias_golden;
  pic_param.pic_flags.bits.sign_bias_alternate =
      frame_header->sign_bias_alternate;
  pic_param.pic_flags.bits.mb_no_coeff_skip = frame_header->mb_no_skip_coeff;
  if (frame_header->IsKeyframe())
    pic_param.pic_flags.bits.forced_lf_adjustment = true;

  static_assert(std::extent<decltype(pic_param.loop_filter_level)>() ==
                        std::extent<decltype(pic_param.ref_lf_delta)>() &&
                    std::extent<decltype(pic_param.ref_lf_delta)>() ==
                        std::extent<decltype(pic_param.mode_lf_delta)>() &&
                    std::extent<decltype(pic_param.ref_lf_delta)>() ==
                        std::extent<decltype(
                            frame_header->loopfilter_hdr.ref_frame_delta)>() &&
                    std::extent<decltype(pic_param.mode_lf_delta)>() ==
                        std::extent<decltype(
                            frame_header->loopfilter_hdr.mb_mode_delta)>(),
                "Invalid loop filter array sizes");

  for (size_t i = 0; i < base::size(pic_param.loop_filter_level); ++i) {
    pic_param.loop_filter_level[i] = frame_header->loopfilter_hdr.level;
    pic_param.ref_lf_delta[i] = frame_header->loopfilter_hdr.ref_frame_delta[i];
    pic_param.mode_lf_delta[i] = frame_header->loopfilter_hdr.mb_mode_delta[i];
  }

  pic_param.sharpness_level = frame_header->loopfilter_hdr.sharpness_level;
  pic_param.clamp_qindex_high = encode_params.max_qp;
  pic_param.clamp_qindex_low = encode_params.min_qp;

  VAQMatrixBufferVP8 qmatrix_buf = {};
  for (auto& index : qmatrix_buf.quantization_index)
    index = frame_header->quantization_hdr.y_ac_qi;

  qmatrix_buf.quantization_index_delta[0] =
      frame_header->quantization_hdr.y_dc_delta;
  qmatrix_buf.quantization_index_delta[1] =
      frame_header->quantization_hdr.y2_dc_delta;
  qmatrix_buf.quantization_index_delta[2] =
      frame_header->quantization_hdr.y2_ac_delta;
  qmatrix_buf.quantization_index_delta[3] =
      frame_header->quantization_hdr.uv_dc_delta;
  qmatrix_buf.quantization_index_delta[4] =
      frame_header->quantization_hdr.uv_ac_delta;

  return vaapi_wrapper_->SubmitBuffer(VAEncSequenceParameterBufferType,
                                      &seq_param) &&
         vaapi_wrapper_->SubmitBuffer(VAEncPictureParameterBufferType,
                                      &pic_param) &&
         vaapi_wrapper_->SubmitBuffer(VAQMatrixBufferType, &qmatrix_buf);
}
}  // namespace media
