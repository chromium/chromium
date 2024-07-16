// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_encoder.h"

#include "base/containers/heap_array.h"
#include "base/logging.h"
#include "base/task/bind_post_task.h"
#include "base/time/time.h"
#include "media/base/audio_timestamp_helper.h"

namespace media {

AudioEncoder::Options::Options() = default;
AudioEncoder::Options::Options(const Options&) = default;
AudioEncoder::Options::~Options() = default;

EncodedAudioBuffer::EncodedAudioBuffer() = default;
EncodedAudioBuffer::EncodedAudioBuffer(const AudioParameters& params,
                                       base::HeapArray<uint8_t> data,
                                       base::TimeTicks timestamp,
                                       base::TimeDelta duration)
    : params(params),
      encoded_data(std::move(data)),
      timestamp(timestamp),
      duration(duration) {}

EncodedAudioBuffer::EncodedAudioBuffer(EncodedAudioBuffer&&) = default;
EncodedAudioBuffer& EncodedAudioBuffer::operator=(EncodedAudioBuffer&&) =
    default;

EncodedAudioBuffer::~EncodedAudioBuffer() = default;

AudioEncoder::AudioEncoder() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

AudioEncoder::~AudioEncoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void AudioEncoder::DisablePostedCallbacks() {
  post_callbacks_ = false;
}

AudioEncoder::OutputCB AudioEncoder::BindCallbackToCurrentLoopIfNeeded(
    OutputCB&& callback) {
  return post_callbacks_
             ? base::BindPostTaskToCurrentDefault(std::move(callback))
             : std::move(callback);
}

AudioEncoder::EncoderStatusCB AudioEncoder::BindCallbackToCurrentLoopIfNeeded(
    EncoderStatusCB&& callback) {
  return post_callbacks_
             ? base::BindPostTaskToCurrentDefault(std::move(callback))
             : std::move(callback);
}

}  // namespace media
