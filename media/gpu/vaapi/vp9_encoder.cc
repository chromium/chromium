// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vp9_encoder.h"

#include "base/bits.h"

#define DVLOGF(level) DVLOG(level) << __func__ << "(): "

namespace media {

namespace {
// Keyframe period.
constexpr size_t kKFPeriod = 3000;

// Arbitrarily chosen bitrate window size for rate control, in ms.
constexpr int kCPBWindowSizeMs = 500;

// Based on WebRTC's defaults.
constexpr int kMinQP = 4;
constexpr int kMaxQP = 112;
constexpr int kDefaultQP = (3 * kMinQP + kMaxQP) / 4;

// filter level may affect on quality at lower bitrates; for now,
// we set a constant value (== 10) which is what other VA-API
// implementations like libyami and gstreamer-vaapi are using.
constexpr uint8_t kDefaultLfLevel = 10;
}  // namespace

VP9Encoder::EncodeParams::EncodeParams()
    : kf_period_frames(kKFPeriod),
      framerate(0),
      cpb_window_size_ms(kCPBWindowSizeMs),
      cpb_size_bits(0),
      initial_qp(kDefaultQP),
      min_qp(kMinQP),
      max_qp(kMaxQP),
      error_resilient_mode(false) {}

void VP9Encoder::Reset() {
  current_params_ = EncodeParams();
  reference_frames_.Clear();
  frame_num_ = 0;
  InitializeFrameHeader();
}

VP9Encoder::VP9Encoder(std::unique_ptr<Accelerator> accelerator)
    : accelerator_(std::move(accelerator)) {}

VP9Encoder::~VP9Encoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool VP9Encoder::Initialize(const VideoEncodeAccelerator::Config& config,
                            const AcceleratedVideoEncoder::Config& ave_config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (VideoCodecProfileToVideoCodec(config.output_profile) != kCodecVP9) {
    DVLOGF(1) << "Invalid profile: " << GetProfileName(config.output_profile);
    return false;
  }

  if (config.input_visible_size.IsEmpty()) {
    DVLOGF(1) << "Input visible size could not be empty";
    return false;
  }
  // 4:2:0 format has to be 2-aligned.
  if ((config.input_visible_size.width() % 2 != 0) ||
      (config.input_visible_size.height() % 2 != 0)) {
    DVLOGF(1) << "The pixel sizes are not even: "
              << config.input_visible_size.ToString();
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

gfx::Size VP9Encoder::GetCodedSize() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!coded_size_.IsEmpty());

  return coded_size_;
}

size_t VP9Encoder::GetMaxNumOfRefFrames() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return kVp9NumRefFrames;
}

bool VP9Encoder::PrepareEncodeJob(EncodeJob* encode_job) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (encode_job->IsKeyframeRequested())
    frame_num_ = 0;

  if (frame_num_ == 0)
    encode_job->ProduceKeyframe();

  frame_num_++;
  frame_num_ %= current_params_.kf_period_frames;

  scoped_refptr<VP9Picture> picture = accelerator_->GetPicture(encode_job);
  DCHECK(picture);

  UpdateFrameHeader(encode_job->IsKeyframeRequested());

  *picture->frame_hdr = current_frame_hdr_;

  // Use last, golden and altref for references.
  constexpr std::array<bool, kVp9NumRefsPerFrame> ref_frames_used = {true, true,
                                                                     true};
  if (!accelerator_->SubmitFrameParameters(encode_job, current_params_, picture,
                                           reference_frames_,
                                           ref_frames_used)) {
    LOG(ERROR) << "Failed submitting frame parameters";
    return false;
  }

  UpdateReferenceFrames(picture);
  return true;
}

bool VP9Encoder::UpdateRates(const VideoBitrateAllocation& bitrate_allocation,
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

void VP9Encoder::InitializeFrameHeader() {
  current_frame_hdr_ = {};
  DCHECK(!visible_size_.IsEmpty());
  current_frame_hdr_.frame_width = visible_size_.width();
  current_frame_hdr_.frame_height = visible_size_.height();
  current_frame_hdr_.render_width = visible_size_.width();
  current_frame_hdr_.render_height = visible_size_.height();
  // Since initial_qp is always kDefaultQP (=31), base_q_idx should be 24
  // (the table index for kDefaultQP, see rfc 8.6.1 table ac_qlookup[3][256])
  // Note: This needs to be revisited once we have 10&12 bit encoder support
  DCHECK_EQ(current_params_.initial_qp, kDefaultQP);
  constexpr uint8_t kDefaultQPACQIndex = 24;
  current_frame_hdr_.quant_params.base_q_idx = kDefaultQPACQIndex;
  current_frame_hdr_.loop_filter.level = kDefaultLfLevel;
  current_frame_hdr_.show_frame = true;
}

void VP9Encoder::UpdateFrameHeader(bool keyframe) {
  if (keyframe) {
    current_frame_hdr_.frame_type = Vp9FrameHeader::KEYFRAME;
    current_frame_hdr_.refresh_frame_flags = 0xff;
    ref_frame_index_ = 0;
  } else {
    // TODO(crbug.com/811912): Add temporal layer support when there is a driver
    // support. Use the last three frames for reference.
    current_frame_hdr_.frame_type = Vp9FrameHeader::INTERFRAME;
    current_frame_hdr_.ref_frame_idx[0] = ref_frame_index_;
    current_frame_hdr_.ref_frame_idx[1] =
        (ref_frame_index_ - 1) & (kVp9NumRefFrames - 1);
    current_frame_hdr_.ref_frame_idx[2] =
        (ref_frame_index_ - 2) & (kVp9NumRefFrames - 1);
    ref_frame_index_ = (ref_frame_index_ + 1) % kVp9NumRefFrames;
    current_frame_hdr_.refresh_frame_flags = 1 << ref_frame_index_;
  }
}

void VP9Encoder::UpdateReferenceFrames(scoped_refptr<VP9Picture> picture) {
  reference_frames_.Refresh(picture);
}

}  // namespace media
