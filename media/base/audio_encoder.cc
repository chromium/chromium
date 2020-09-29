// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_encoder.h"

#include "base/logging.h"
#include "base/time/time.h"
#include "media/base/audio_timestamp_helper.h"

namespace media {

// -----------------------------------------------------------------------------
// EncodedAudioBuffer:

EncodedAudioBuffer::EncodedAudioBuffer(const AudioParameters& params,
                                       std::unique_ptr<uint8_t[]> data,
                                       size_t size,
                                       base::TimeTicks timestamp)
    : params(params),
      encoded_data(std::move(data)),
      encoded_data_size(size),
      timestamp(timestamp) {}

EncodedAudioBuffer::EncodedAudioBuffer(EncodedAudioBuffer&&) = default;

EncodedAudioBuffer::~EncodedAudioBuffer() = default;

// -----------------------------------------------------------------------------
// AudioEncoder:

AudioEncoder::AudioEncoder(const AudioParameters& input_params,
                           EncodeCB encode_callback,
                           StatusCB status_callback)
    : audio_input_params_(input_params),
      encode_callback_(std::move(encode_callback)),
      status_callback_(std::move(status_callback)) {
  DCHECK(audio_input_params_.IsValid());
  DCHECK(!encode_callback_.is_null());
  DCHECK(!status_callback_.is_null());
  DETACH_FROM_THREAD(thread_checker_);
}

AudioEncoder::~AudioEncoder() = default;

void AudioEncoder::EncodeAudio(const AudioBus& audio_bus,
                               base::TimeTicks capture_time) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(audio_bus.channels(), audio_input_params_.channels());
  DCHECK(!capture_time.is_null());

  DLOG_IF(ERROR,
          !last_capture_time_.is_null() &&
              ((capture_time - last_capture_time_).InSecondsF() >
               1.5f * audio_bus.frames() / audio_input_params().sample_rate()))
      << "Possibly frames were skipped, which may result in inaccuarate "
         "timestamp calculation.";

  last_capture_time_ = capture_time;

  EncodeAudioImpl(audio_bus, capture_time);
}

base::TimeTicks AudioEncoder::ComputeTimestamp(
    int num_frames,
    base::TimeTicks capture_time) const {
  return capture_time - AudioTimestampHelper::FramesToTime(
                            num_frames, audio_input_params_.sample_rate());
}

}  // namespace media
