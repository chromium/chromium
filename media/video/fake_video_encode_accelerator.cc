// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/fake_video_encode_accelerator.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"

namespace media {

static const unsigned int kMinimumInputCount = 1;

FakeVideoEncodeAccelerator::FakeVideoEncodeAccelerator(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
    : task_runner_(task_runner),
      will_initialization_succeed_(true),
      client_(NULL),
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

  profile.profile = media::H264PROFILE_MAIN;
  profiles.push_back(profile);
  profile.profile = media::VP8PROFILE_ANY;
  profiles.push_back(profile);
  return profiles;
}

bool FakeVideoEncodeAccelerator::Initialize(const Config& config,
                                            Client* client) {
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
  queued_frames_.push(force_keyframe);
  EncodeTask();
}

void FakeVideoEncodeAccelerator::UseOutputBitstreamBuffer(
    BitstreamBuffer buffer) {
  available_buffers_.push_back(std::move(buffer));
  EncodeTask();
}

void FakeVideoEncodeAccelerator::RequestEncodingParametersChange(
    uint32_t bitrate,
    uint32_t framerate) {
  stored_bitrates_.push_back(bitrate);
}

void FakeVideoEncodeAccelerator::RequestEncodingParametersChange(
    const VideoBitrateAllocation& bitrate,
    uint32_t framerate) {
  stored_bitrate_allocations_.push_back(bitrate);
}

void FakeVideoEncodeAccelerator::Destroy() { delete this; }

void FakeVideoEncodeAccelerator::SendDummyFrameForTesting(bool key_frame) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeVideoEncodeAccelerator::DoBitstreamBufferReady,
                     weak_this_factory_.GetWeakPtr(), 0, 23, key_frame));
}

void FakeVideoEncodeAccelerator::SetWillInitializationSucceed(
    bool will_initialization_succeed) {
  will_initialization_succeed_ = will_initialization_succeed;
}

void FakeVideoEncodeAccelerator::DoRequireBitstreamBuffers(
    unsigned int input_count,
    const gfx::Size& input_coded_size,
    size_t output_buffer_size) const {
  client_->RequireBitstreamBuffers(
      input_count, input_coded_size, output_buffer_size);
}

void FakeVideoEncodeAccelerator::EncodeTask() {
  while (!queued_frames_.empty() && !available_buffers_.empty()) {
    bool force_key_frame = queued_frames_.front();
    queued_frames_.pop();
    int32_t bitstream_buffer_id = available_buffers_.front().id();
    available_buffers_.pop_front();
    bool key_frame = next_frame_is_first_frame_ || force_key_frame;
    next_frame_is_first_frame_ = false;
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&FakeVideoEncodeAccelerator::DoBitstreamBufferReady,
                       weak_this_factory_.GetWeakPtr(), bitstream_buffer_id,
                       kMinimumOutputBufferSize, key_frame));
  }
}

void FakeVideoEncodeAccelerator::DoBitstreamBufferReady(
    int32_t bitstream_buffer_id,
    size_t payload_size,
    bool key_frame) const {
  client_->BitstreamBufferReady(
      bitstream_buffer_id,
      BitstreamBufferMetadata(payload_size, key_frame,
                              base::Time::Now().since_origin()));
}

}  // namespace media
