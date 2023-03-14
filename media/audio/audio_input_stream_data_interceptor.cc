// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_input_stream_data_interceptor.h"

#include <utility>

#include "media/audio/audio_debug_recording_helper.h"

namespace media {

AudioInputStreamDataInterceptor::AudioInputStreamDataInterceptor(
    CreateDebugRecorderCB create_debug_recorder_cb,
    AudioInputStream* stream)
    : create_debug_recorder_cb_(std::move(create_debug_recorder_cb)),
      stream_(stream) {
  DCHECK(create_debug_recorder_cb_);
  DCHECK(stream_);
}

AudioInputStreamDataInterceptor::~AudioInputStreamDataInterceptor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// Implementation of AudioInputStream.
AudioInputStream::OpenOutcome AudioInputStreamDataInterceptor::Open() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return stream_->Open();
}

void AudioInputStreamDataInterceptor::Start(
    AudioInputStream::AudioInputCallback* callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  callback_ = callback;
  debug_recorder_ = create_debug_recorder_cb_.Run();
  stream_->Start(this);
}

void AudioInputStreamDataInterceptor::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  stream_->Stop();
  debug_recorder_.reset();
  callback_ = nullptr;
}

void AudioInputStreamDataInterceptor::Close() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  stream_->Close();
  delete this;
}

double AudioInputStreamDataInterceptor::GetMaxVolume() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return stream_->GetMaxVolume();
}

void AudioInputStreamDataInterceptor::SetVolume(double volume) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  stream_->SetVolume(volume);
}

double AudioInputStreamDataInterceptor::GetVolume() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return stream_->GetVolume();
}

bool AudioInputStreamDataInterceptor::IsMuted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return stream_->IsMuted();
}

bool AudioInputStreamDataInterceptor::SetAutomaticGainControl(bool enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return stream_->SetAutomaticGainControl(enabled);
}

bool AudioInputStreamDataInterceptor::GetAutomaticGainControl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return stream_->GetAutomaticGainControl();
}

void AudioInputStreamDataInterceptor::SetOutputDeviceForAec(
    const std::string& output_device_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return stream_->SetOutputDeviceForAec(output_device_id);
}

void AudioInputStreamDataInterceptor::OnData(
    const AudioBus* source,
    base::TimeTicks capture_time,
    double volume,
    const AudioGlitchInfo& audio_glitch_info) {
  callback_->OnData(source, capture_time, volume, audio_glitch_info);
  debug_recorder_->OnData(source);
}

void AudioInputStreamDataInterceptor::OnError() {
  callback_->OnError();
}

}  // namespace media
