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

namespace media {

namespace {
// Keyframe period.
constexpr size_t kKFPeriod = 3000;

// Arbitrarily chosen bitrate window size for rate control, in ms.
constexpr int kCPBWindowSizeMs = 1500;

// Quantization parameter. They are vp8 ac/dc indices and their ranges are
// 0-127. Based on WebRTC's defaults.
constexpr uint8_t kMinQP = 4;
// b/110059922, crbug.com/1001900: Tuned 112->117 for bitrate issue in a lower
// resolution (180p).
constexpr uint8_t kMaxQP = 117;
// This stands for 32 as a real ac value (see rfc 14.1. table ac_qlookup).
constexpr uint8_t kDefaultQP = 28;

void FillVAEncRateControlParams(
    uint32_t bps,
    uint32_t window_size,
    uint32_t initial_qp,
    uint32_t min_qp,
    uint32_t max_qp,
    uint32_t framerate,
    uint32_t buffer_size,
    VAEncMiscParameterRateControl& rate_control_param,
    VAEncMiscParameterFrameRate& framerate_param,
    VAEncMiscParameterHRD& hrd_param) {
  memset(&rate_control_param, 0, sizeof(rate_control_param));
  rate_control_param.bits_per_second = bps;
  rate_control_param.window_size = window_size;
  rate_control_param.initial_qp = initial_qp;
  rate_control_param.min_qp = min_qp;
  rate_control_param.max_qp = max_qp;
  rate_control_param.rc_flags.bits.disable_frame_skip = true;

  memset(&framerate_param, 0, sizeof(framerate_param));
  framerate_param.framerate = framerate;

  memset(&hrd_param, 0, sizeof(hrd_param));
  hrd_param.buffer_size = buffer_size;
  hrd_param.initial_buffer_fullness = buffer_size / 2;
}

static scoped_refptr<base::RefCountedBytes> MakeRefCountedBytes(void* ptr,
                                                                size_t size) {
  return base::MakeRefCounted<base::RefCountedBytes>(
      reinterpret_cast<uint8_t*>(ptr), size);
}

}  // namespace

VP8VaapiVideoEncoderDelegate::EncodeParams::EncodeParams()
    : kf_period_frames(kKFPeriod),
      framerate(0),
      cpb_window_size_ms(kCPBWindowSizeMs),
      cpb_size_bits(0),
      initial_qp(kDefaultQP),
      min_qp(kMinQP),
      max_qp(kMaxQP),
      error_resilient_mode(false) {}

void VP8VaapiVideoEncoderDelegate::Reset() {
  current_params_ = EncodeParams();
  reference_frames_.Clear();
  frame_num_ = 0;

  InitializeFrameHeader();
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

  visible_size_ = config.input_visible_size;
  coded_size_ = gfx::Size(base::bits::AlignUp(visible_size_.width(), 16),
                          base::bits::AlignUp(visible_size_.height(), 16));

  Reset();

  VideoBitrateAllocation initial_bitrate_allocation;
  initial_bitrate_allocation.SetBitrate(0, 0, config.bitrate.target());
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

bool VP8VaapiVideoEncoderDelegate::PrepareEncodeJob(EncodeJob* encode_job) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (encode_job->IsKeyframeRequested())
    frame_num_ = 0;

  if (frame_num_ == 0)
    encode_job->ProduceKeyframe();

  frame_num_++;
  frame_num_ %= current_params_.kf_period_frames;

  scoped_refptr<VP8Picture> picture = GetPicture(encode_job);
  DCHECK(picture);

  UpdateFrameHeader(encode_job->IsKeyframeRequested());
  *picture->frame_hdr = current_frame_hdr_;

  // We only use |last_frame| for a reference frame. This follows the behavior
  // of libvpx encoder in chromium webrtc use case.
  std::array<bool, kNumVp8ReferenceBuffers> ref_frames_used{true, false, false};

  if (current_frame_hdr_.IsKeyframe()) {
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

  base::CheckedNumeric<uint32_t> cpb_size_bits(bitrate);
  cpb_size_bits /= 1000;
  cpb_size_bits *= current_params_.cpb_window_size_ms;
  if (!cpb_size_bits.AssignIfValid(&current_params_.cpb_size_bits)) {
    VLOGF(1) << "Too large bitrate: " << bitrate_allocation.GetSumBps();
    return false;
  }

  return true;
}

void VP8VaapiVideoEncoderDelegate::InitializeFrameHeader() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  current_frame_hdr_ = {};
  DCHECK(!visible_size_.IsEmpty());
  current_frame_hdr_.width = visible_size_.width();
  current_frame_hdr_.height = visible_size_.height();
  current_frame_hdr_.quantization_hdr.y_ac_qi = kDefaultQP;
  current_frame_hdr_.show_frame = true;
  // TODO(sprang): Make this dynamic. Value based on reference implementation
  // in libyami (https://github.com/intel/libyami).

  // A VA-API driver recommends to set forced_lf_adjustment on keyframe.
  // Set loop_filter_adj_enable to 1 here because forced_lf_adjustment is read
  // only when a macroblock level loop filter adjustment.
  current_frame_hdr_.loopfilter_hdr.loop_filter_adj_enable = true;

  // Set mb_no_skip_coeff to 1 that some decoders (e.g. kepler) could not decode
  // correctly a stream encoded with mb_no_skip_coeff=0. It also enables an
  // encoder to produce a more optimized stream than when mb_no_skip_coeff=0.
  current_frame_hdr_.mb_no_skip_coeff = true;
}

void VP8VaapiVideoEncoderDelegate::UpdateFrameHeader(bool keyframe) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (keyframe) {
    current_frame_hdr_.frame_type = Vp8FrameHeader::KEYFRAME;
    current_frame_hdr_.refresh_last = true;
    current_frame_hdr_.refresh_golden_frame = true;
    current_frame_hdr_.refresh_alternate_frame = true;
    current_frame_hdr_.copy_buffer_to_golden =
        Vp8FrameHeader::NO_GOLDEN_REFRESH;
    current_frame_hdr_.copy_buffer_to_alternate =
        Vp8FrameHeader::NO_ALT_REFRESH;
  } else {
    current_frame_hdr_.frame_type = Vp8FrameHeader::INTERFRAME;
    // TODO(sprang): Add temporal layer support.
    current_frame_hdr_.refresh_last = true;
    current_frame_hdr_.refresh_golden_frame = false;
    current_frame_hdr_.refresh_alternate_frame = false;
    current_frame_hdr_.copy_buffer_to_golden =
        Vp8FrameHeader::COPY_LAST_TO_GOLDEN;
    current_frame_hdr_.copy_buffer_to_alternate =
        Vp8FrameHeader::COPY_GOLDEN_TO_ALT;
  }
}

void VP8VaapiVideoEncoderDelegate::UpdateReferenceFrames(
    scoped_refptr<VP8Picture> picture) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  reference_frames_.Refresh(picture);
}

scoped_refptr<VP8Picture> VP8VaapiVideoEncoderDelegate::GetPicture(
    VaapiVideoEncoderDelegate::EncodeJob* job) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return base::WrapRefCounted(
      reinterpret_cast<VP8Picture*>(job->picture().get()));
}

bool VP8VaapiVideoEncoderDelegate::SubmitFrameParameters(
    VaapiVideoEncoderDelegate::EncodeJob* job,
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
  pic_param.coded_buf = job->coded_buffer_id();
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
  for (auto index : qmatrix_buf.quantization_index)
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

  VAEncMiscParameterRateControl rate_control_param;
  VAEncMiscParameterFrameRate framerate_param;
  VAEncMiscParameterHRD hrd_param;
  FillVAEncRateControlParams(
      base::checked_cast<uint32_t>(
          encode_params.bitrate_allocation.GetSumBps()),
      base::strict_cast<uint32_t>(encode_params.cpb_window_size_ms),
      base::strict_cast<uint32_t>(encode_params.initial_qp),
      base::strict_cast<uint32_t>(encode_params.min_qp),
      base::strict_cast<uint32_t>(encode_params.max_qp),
      encode_params.framerate,
      base::strict_cast<uint32_t>(encode_params.cpb_size_bits),
      rate_control_param, framerate_param, hrd_param);

  job->AddSetupCallback(
      base::BindOnce(&VaapiVideoEncoderDelegate::SubmitBuffer,
                     base::Unretained(this), VAEncSequenceParameterBufferType,
                     MakeRefCountedBytes(&seq_param, sizeof(seq_param))));

  job->AddSetupCallback(
      base::BindOnce(&VaapiVideoEncoderDelegate::SubmitBuffer,
                     base::Unretained(this), VAEncPictureParameterBufferType,
                     MakeRefCountedBytes(&pic_param, sizeof(pic_param))));

  job->AddSetupCallback(
      base::BindOnce(&VaapiVideoEncoderDelegate::SubmitBuffer,
                     base::Unretained(this), VAQMatrixBufferType,
                     MakeRefCountedBytes(&qmatrix_buf, sizeof(qmatrix_buf))));

  job->AddSetupCallback(base::BindOnce(
      &VaapiVideoEncoderDelegate::SubmitVAEncMiscParamBuffer,
      base::Unretained(this), VAEncMiscParameterTypeRateControl,
      MakeRefCountedBytes(&rate_control_param, sizeof(rate_control_param))));

  job->AddSetupCallback(base::BindOnce(
      &VaapiVideoEncoderDelegate::SubmitVAEncMiscParamBuffer,
      base::Unretained(this), VAEncMiscParameterTypeFrameRate,
      MakeRefCountedBytes(&framerate_param, sizeof(framerate_param))));

  job->AddSetupCallback(
      base::BindOnce(&VaapiVideoEncoderDelegate::SubmitVAEncMiscParamBuffer,
                     base::Unretained(this), VAEncMiscParameterTypeHRD,
                     MakeRefCountedBytes(&hrd_param, sizeof(hrd_param))));

  return true;
}
}  // namespace media
