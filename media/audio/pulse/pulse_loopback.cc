// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pulse_loopback.h"

#include "audio_manager_pulse.h"
#include "pulse_input.h"
#include "pulse_util.h"

namespace media {

PulseLoopbackAudioStream::PulseLoopbackAudioStream(
    ReleaseStreamCallback release_stream_callback,
    const std::string& source_name,
    const AudioParameters& params,
    pa_threaded_mainloop* mainloop,
    pa_context* context,
    AudioManager::LogCallback log_callback,
    bool mute_system_audio)
    : release_stream_callback_(std::move(release_stream_callback)),
      params_(params),
      mainloop_(mainloop),
      context_(context),
      log_callback_(std::move(log_callback)),
      mute_system_audio_(mute_system_audio),
      sink_(nullptr),
      stream_(new PulseAudioInputStream(nullptr,
                                        source_name,
                                        params,
                                        mainloop,
                                        context,
                                        log_callback_)) {
  CHECK(stream_);
}

PulseLoopbackAudioStream::~PulseLoopbackAudioStream() {
  CHECK(!stream_);
}

AudioInputStream::OpenOutcome PulseLoopbackAudioStream::Open() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  OpenOutcome open_outcome = stream_->Open();
  if (open_outcome == OpenOutcome::kSuccess) {
    stream_opened_ = true;
    if (mute_system_audio_) {
      std::string default_sink_name = media::pulse::GetRealDefaultDeviceId(
          mainloop_, context_, pulse::RequestType::OUTPUT);
      std::string monitor_source_name =
          media::pulse::GetMonitorSourceNameForSink(mainloop_, context_,
                                                    default_sink_name);
      pulse::MuteAllSinksExcept(mainloop_, context_, monitor_source_name);
    }
  }

  return open_outcome;
}

void PulseLoopbackAudioStream::Start(AudioInputCallback* callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!sink_);
  sink_ = callback;
  stream_->Start(callback);
}

void PulseLoopbackAudioStream::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  stream_->Stop();
  sink_ = nullptr;
}

void PulseLoopbackAudioStream::Close() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!sink_);
  if (mute_system_audio_) {
    pulse::UnmuteAllSinks(mainloop_, context_);
  }
  CloseWrappedStream();
  std::move(release_stream_callback_).Run(this);
}

double PulseLoopbackAudioStream::GetMaxVolume() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return stream_->GetMaxVolume();
}

void PulseLoopbackAudioStream::SetVolume(double volume) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  stream_->SetVolume(volume);
}

double PulseLoopbackAudioStream::GetVolume() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return stream_->GetVolume();
}

bool PulseLoopbackAudioStream::IsMuted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return stream_->IsMuted();
}

void PulseLoopbackAudioStream::SetOutputDeviceForAec(
    const std::string& output_device_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  stream_->SetOutputDeviceForAec(output_device_id);
}

void PulseLoopbackAudioStream::ChangeStreamSource(
    const std::string& source_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  stream_->Stop();
  CloseWrappedStream();

  stream_ = new PulseAudioInputStream(nullptr, source_name, params_, mainloop_,
                                      context_, log_callback_);
  CHECK(stream_);

  // Open the new stream iff the old one was open.
  if (!stream_opened_) {
    return;
  }

  if (stream_->Open() != OpenOutcome::kSuccess) {
    stream_opened_ = false;
    if (sink_) {
      sink_->OnError();
    }
    return;
  }

  // Start the new stream iff the old one was started.
  if (sink_) {
    stream_->Start(sink_);
  }
}

void PulseLoopbackAudioStream::CloseWrappedStream() {
  // Avoid dangling pointers.
  auto* stream = stream_.get();
  stream_ = nullptr;
  stream->Close();
  delete stream;
}

}  // namespace media
