// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/fake_video_encode_accelerator.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/media_log.h"
#include "media/base/video_frame.h"

namespace media {

static const unsigned int kMinimumInputCount = 1;

FakeVideoEncodeAccelerator::FrameToEncode::FrameToEncode() = default;
FakeVideoEncodeAccelerator::FrameToEncode::FrameToEncode(
    const FakeVideoEncodeAccelerator::FrameToEncode&) = default;
FakeVideoEncodeAccelerator::FrameToEncode::~FrameToEncode() = default;

FakeVideoEncodeAccelerator::FakeVideoEncodeAccelerator(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : task_runner_(task_runner),
      will_initialization_succeed_(true),
      will_encoding_succeed_(true),
      client_(nullptr),
      next_frame_is_first_frame_(true) {}

FakeVideoEncodeAccelerator::~FakeVideoEncodeAccelerator() {
  weak_this_factory_.InvalidateWeakPtrs();
}

VideoEncodeAccelerator::SupportedProfiles
FakeVideoEncodeAccelerator::GetSupportedProfiles() {
  SupportedProfiles profiles;
  SupportedProfile profile;
  profile.max_resolution.SetSize(1920, 1088);
  profile.max_framerate_numerator = 30;
  profile.max_framerate_denominator = 1;
  profile.rate_control_modes = media::VideoEncodeAccelerator::kConstantMode;

  profile.profile = media::H264PROFILE_MAIN;
  profiles.push_back(profile);
  profile.profile = media::VP8PROFILE_ANY;
  profiles.push_back(profile);
  return profiles;
}

bool FakeVideoEncodeAccelerator::Initialize(
    const Config& config,
    Client* client,
    std::unique_ptr<MediaLog> media_log) {
  if (!will_initialization_succeed_) {
    return false;
  }
  if (config.output_profile == VIDEO_CODEC_PROFILE_UNKNOWN ||
      config.output_profile > VIDEO_CODEC_PROFILE_MAX) {
    return false;
  }
  client_ = client;
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeVideoEncodeAccelerator::DoRequireBitstreamBuffers,
                     weak_this_factory_.GetWeakPtr(), kMinimumInputCount,
                     config.input_visible_size, kMinimumOutputBufferSize));
  return true;
}

void FakeVideoEncodeAccelerator::Encode(scoped_refptr<VideoFrame> frame,
                                        bool force_keyframe) {
  DCHECK(client_);
  FrameToEncode encode;
  encode.frame = frame;
  encode.force_keyframe = force_keyframe;
  queued_frames_.push(encode);
  EncodeTask();
}

void FakeVideoEncodeAccelerator::UseOutputBitstreamBuffer(
    BitstreamBuffer buffer) {
  available_buffers_.push_back(std::move(buffer));
  EncodeTask();
}

void FakeVideoEncodeAccelerator::RequestEncodingParametersChange(
    const Bitrate& bitrate,
    uint32_t framerate) {
  // Reject bitrate mode changes.
  if (stored_bitrates_.empty() ||
      stored_bitrates_.back().mode() == bitrate.mode()) {
    stored_bitrates_.push_back(bitrate);
  }
}

void FakeVideoEncodeAccelerator::RequestEncodingParametersChange(
    const VideoBitrateAllocation& bitrate,
    uint32_t framerate) {
  stored_bitrate_allocations_.push_back(bitrate);
}

void FakeVideoEncodeAccelerator::Destroy() {
  delete this;
}

void FakeVideoEncodeAccelerator::SetWillInitializationSucceed(
    bool will_initialization_succeed) {
  will_initialization_succeed_ = will_initialization_succeed;
}

void FakeVideoEncodeAccelerator::SetWillEncodingSucceed(
    bool will_encoding_succeed) {
  will_encoding_succeed_ = will_encoding_succeed;
}

void FakeVideoEncodeAccelerator::NotifyEncoderInfoChange(
    const VideoEncoderInfo& info) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeVideoEncodeAccelerator::DoNotifyEncoderInfoChange,
                     weak_this_factory_.GetWeakPtr(), info));
}

void FakeVideoEncodeAccelerator::DoNotifyEncoderInfoChange(
    const VideoEncoderInfo& info) {
  client_->NotifyEncoderInfoChange(info);
}

void FakeVideoEncodeAccelerator::DoRequireBitstreamBuffers(
    unsigned int input_count,
    const gfx::Size& input_coded_size,
    size_t output_buffer_size) const {
  client_->RequireBitstreamBuffers(input_count, input_coded_size,
                                   output_buffer_size);
}

void FakeVideoEncodeAccelerator::EncodeTask() {
  while (!queued_frames_.empty() && !available_buffers_.empty()) {
    FrameToEncode frame_to_encode = queued_frames_.front();
    BitstreamBuffer buffer = std::move(available_buffers_.front());
    available_buffers_.pop_front();
    queued_frames_.pop();

    if (next_frame_is_first_frame_) {
      frame_to_encode.force_keyframe = true;
      next_frame_is_first_frame_ = false;
    }

    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&FakeVideoEncodeAccelerator::DoBitstreamBufferReady,
                       weak_this_factory_.GetWeakPtr(), std::move(buffer),
                       frame_to_encode));
  }
}

void FakeVideoEncodeAccelerator::DoBitstreamBufferReady(
    BitstreamBuffer buffer,
    FrameToEncode frame_to_encode) const {
  if (!will_encoding_succeed_) {
    client_->NotifyErrorStatus(EncoderStatus::Codes::kEncoderFailedEncode);
    return;
  }

  BitstreamBufferMetadata metadata(kMinimumOutputBufferSize,
                                   frame_to_encode.force_keyframe,
                                   frame_to_encode.frame->timestamp());

  if (!encoding_callback_.is_null())
    metadata = encoding_callback_.Run(buffer, frame_to_encode.force_keyframe,
                                      frame_to_encode.frame);

  client_->BitstreamBufferReady(buffer.id(), metadata);
}

bool FakeVideoEncodeAccelerator::IsGpuFrameResizeSupported() {
  return resize_supported_;
}

}  // namespace media
