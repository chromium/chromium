// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vp8_encoder.h"

#include "base/bits.h"
#include "media/gpu/macros.h"

namespace media {

namespace {
// Keyframe period.
constexpr size_t kKFPeriod = 3000;

// Arbitrarily chosen bitrate window size for rate control, in ms.
constexpr int kCPBWindowSizeMs = 1500;

// Based on WebRTC's defaults.
constexpr int kMinQP = 4;
// b/110059922, crbug.com/1001900: Tuned 112->117 for bitrate issue in a lower
// resolution (180p).
constexpr int kMaxQP = 117;
constexpr int kDefaultQP = (3 * kMinQP + kMaxQP) / 4;
}  // namespace

VP8Encoder::EncodeParams::EncodeParams()
    : kf_period_frames(kKFPeriod),
      framerate(0),
      cpb_window_size_ms(kCPBWindowSizeMs),
      cpb_size_bits(0),
      initial_qp(kDefaultQP),
      min_qp(kMinQP),
      max_qp(kMaxQP),
      error_resilient_mode(false) {}

void VP8Encoder::Reset() {
  current_params_ = EncodeParams();
  reference_frames_.Clear();
  frame_num_ = 0;

  InitializeFrameHeader();
}

VP8Encoder::VP8Encoder(std::unique_ptr<Accelerator> accelerator)
    : accelerator_(std::move(accelerator)) {}

VP8Encoder::~VP8Encoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool VP8Encoder::Initialize(const VideoEncodeAccelerator::Config& config,
                            const AcceleratedVideoEncoder::Config& ave_config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (VideoCodecProfileToVideoCodec(config.output_profile) != kCodecVP8) {
    DVLOGF(1) << "Invalid profile: " << GetProfileName(config.output_profile);
    return false;
  }

  if (config.input_visible_size.IsEmpty()) {
    DVLOGF(1) << "Input visible size could not be empty";
    return false;
  }

  visible_size_ = config.input_visible_size;
  coded_size_ = gfx::Size(base::bits::Align(visible_size_.width(), 16),
                          base::bits::Align(visible_size_.height(), 16));

  Reset();

  VideoBitrateAllocation initial_bitrate_allocation;
  initial_bitrate_allocation.SetBitrate(0, 0, config.initial_bitrate);
  return UpdateRates(initial_bitrate_allocation,
                     config.initial_framerate.value_or(
                         VideoEncodeAccelerator::kDefaultFramerate));
}

gfx::Size VP8Encoder::GetCodedSize() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!coded_size_.IsEmpty());

  return coded_size_;
}

size_t VP8Encoder::GetMaxNumOfRefFrames() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return kNumVp8ReferenceBuffers;
}

bool VP8Encoder::PrepareEncodeJob(EncodeJob* encode_job) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (encode_job->IsKeyframeRequested())
    frame_num_ = 0;

  if (frame_num_ == 0)
    encode_job->ProduceKeyframe();

  frame_num_++;
  frame_num_ %= current_params_.kf_period_frames;

  scoped_refptr<VP8Picture> picture = accelerator_->GetPicture(encode_job);
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

  if (!accelerator_->SubmitFrameParameters(encode_job, current_params_, picture,
                                           reference_frames_,
                                           ref_frames_used)) {
    LOG(ERROR) << "Failed submitting frame parameters";
    return false;
  }

  UpdateReferenceFrames(picture);
  return true;
}

bool VP8Encoder::UpdateRates(const VideoBitrateAllocation& bitrate_allocation,
                             uint32_t framerate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (bitrate_allocation.GetSumBps() == 0 || framerate == 0)
    return false;

  if (current_params_.bitrate_allocation == bitrate_allocation &&
      current_params_.framerate == framerate) {
    return true;
  }

  current_params_.bitrate_allocation = bitrate_allocation;
  current_params_.framerate = framerate;

  current_params_.cpb_size_bits =
      current_params_.bitrate_allocation.GetSumBps() *
      current_params_.cpb_window_size_ms / 1000;

  return true;
}

void VP8Encoder::InitializeFrameHeader() {
  current_frame_hdr_ = {};
  DCHECK(!visible_size_.IsEmpty());
  current_frame_hdr_.width = visible_size_.width();
  current_frame_hdr_.height = visible_size_.height();
  // Since initial_qp is always kDefaultQP (=32), y_ac_qi should be 28
  // (the table index for kDefaultQP, see rfc 14.1. table ac_qlookup)
  static_assert(kDefaultQP == 32, "kDefault QP is not 32");
  DCHECK_EQ(current_params_.initial_qp, kDefaultQP);
  constexpr uint8_t kDefaultQPACQIndex = 28;
  current_frame_hdr_.quantization_hdr.y_ac_qi = kDefaultQPACQIndex;
  current_frame_hdr_.show_frame = true;
  // TODO(sprang): Make this dynamic. Value based on reference implementation
  // in libyami (https://github.com/intel/libyami).

  // A VA-API driver recommends to set forced_lf_adjustment on keyframe.
  // Set loop_filter_adj_enable to 1 here because forced_lf_adjustment is read
  // only when a macroblock level loop filter adjustment.
  current_frame_hdr_.loopfilter_hdr.loop_filter_adj_enable = 1;

  // Set mb_no_skip_coeff to 1 that some decoders (e.g. kepler) could not decode
  // correctly a stream encoded with mb_no_skip_coeff=0. It also enables an
  // encoder to produce a more optimized stream than when mb_no_skip_coeff=0.
  current_frame_hdr_.mb_no_skip_coeff = 1;
}

void VP8Encoder::UpdateFrameHeader(bool keyframe) {
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

void VP8Encoder::UpdateReferenceFrames(scoped_refptr<VP8Picture> picture) {
  reference_frames_.Refresh(picture);
}

}  // namespace media
