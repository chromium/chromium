// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/encoding/fake_software_video_encoder.h"

#include <stddef.h>

#include "base/json/json_writer.h"
#include "base/values.h"
#include "media/base/video_frame.h"
#include "media/cast/common/frame_id.h"
#include "media/cast/common/openscreen_conversion_helpers.h"
#include "media/cast/common/rtp_time.h"
#include "media/cast/common/sender_encoded_frame.h"
#include "media/cast/constants.h"

namespace media::cast {

FakeSoftwareVideoEncoder::FakeSoftwareVideoEncoder(
    const FrameSenderConfig& video_config,
    std::unique_ptr<VideoEncoderMetricsProvider> metrics_provider)
    : video_config_(video_config),
      metrics_provider_(std::move(metrics_provider)) {
  CHECK_GT(video_config_.max_frame_rate, 0);
}

FakeSoftwareVideoEncoder::~FakeSoftwareVideoEncoder() = default;

void FakeSoftwareVideoEncoder::Initialize() {}

void FakeSoftwareVideoEncoder::Encode(
    scoped_refptr<media::VideoFrame> video_frame,
    base::TimeTicks reference_time,
    SenderEncodedFrame* encoded_frame) {
  CHECK(encoded_frame);

  if (video_frame->visible_rect().size() != last_frame_size_) {
    next_frame_is_key_ = true;
    last_frame_size_ = video_frame->visible_rect().size();
    metrics_provider_->Initialize(VIDEO_CODEC_PROFILE_UNKNOWN, last_frame_size_,
                                  /*is_hardware_encoder=*/false);
  }

  encoded_frame->frame_id = frame_id_++;
  encoded_frame->is_key_frame = next_frame_is_key_;
  if (next_frame_is_key_) {
    encoded_frame->referenced_frame_id = encoded_frame->frame_id;
    next_frame_is_key_ = false;
  } else {
    encoded_frame->referenced_frame_id = encoded_frame->frame_id - 1;
  }
  encoded_frame->rtp_timestamp =
      ToRtpTimeTicks(video_frame->timestamp(), kVideoFrequency);
  encoded_frame->reference_time = reference_time;

  const auto values =
      base::Value::Dict()
          .Set("key", encoded_frame->is_key_frame)
          .Set("ref", static_cast<int>(
                          encoded_frame->referenced_frame_id.lower_32_bits()))
          .Set("id", static_cast<int>(encoded_frame->frame_id.lower_32_bits()))
          .Set("size", frame_size_);

  std::string raw_data = base::WriteJson(values).value_or("");
  if (static_cast<size_t>(frame_size_) > raw_data.size()) {
    raw_data.append(' ', frame_size_ - raw_data.size());
  }
  encoded_frame->data = base::HeapArray<uint8_t>::CopiedFrom(
      base::as_bytes(base::span(raw_data)));

  if (encoded_frame->is_key_frame) {
    encoded_frame->encoder_utilization = 1.0;
    encoded_frame->lossiness = 6.0;
  } else {
    encoded_frame->encoder_utilization = 0.8;
    encoded_frame->lossiness = 0.8;
  }

  metrics_provider_->IncrementEncodedFrameCount();
}

void FakeSoftwareVideoEncoder::UpdateRates(uint32_t new_bitrate) {
  frame_size_ = new_bitrate / video_config_.max_frame_rate / 8;
}

void FakeSoftwareVideoEncoder::GenerateKeyFrame() {
  next_frame_is_key_ = true;
}

}  // namespace media::cast
