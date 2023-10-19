// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/android/aaudio_input.h"

#include "base/task/bind_post_task.h"
#include "media/audio/android/audio_manager_android.h"
#include "media/base/amplitude_peak_detector.h"
#include "media/base/audio_bus.h"

namespace media {

AAudioInputStream::AAudioInputStream(AudioManagerAndroid* manager,
                                     const AudioParameters& params)
    : audio_manager_(manager),
      params_(params),
      peak_detector_(base::BindRepeating(&AudioManager::TraceAmplitudePeak,
                                         base::Unretained(audio_manager_),
                                         /*trace_start=*/true)) {
  CHECK(audio_manager_);

  handle_device_change_on_main_sequence_ =
      base::BindPostTaskToCurrentDefault(base::BindRepeating(
          &AAudioInputStream::HandleDeviceChange, weak_factory_.GetWeakPtr()));
}

AAudioInputStream::~AAudioInputStream() = default;

void AAudioInputStream::CreateStreamWrapper() {
  CHECK(!stream_wrapper_);
  stream_wrapper_ = std::make_unique<AAudioStreamWrapper>(
      this, AAudioStreamWrapper::StreamType::kInput, params_,
      AAUDIO_USAGE_VOICE_COMMUNICATION);
}

AudioInputStream::OpenOutcome AAudioInputStream::Open() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CreateStreamWrapper();

  if (!stream_wrapper_->Open()) {
    return AudioInputStream::OpenOutcome::kFailed;
  }

  audio_bus_ = AudioBus::Create(params_);

  return AudioInputStream::OpenOutcome::kSuccess;
}

void AAudioInputStream::Start(AudioInputCallback* callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK(stream_wrapper_);

  {
    base::AutoLock al(lock_);

    if (error_during_device_change_) {
      // Report the error that came up in HandleDeviceChange().
      callback->OnError();
      return;
    }

    CHECK(!callback_);
    callback_ = callback;
  }

  if (stream_wrapper_->Start()) {
    // Successfully started `stream_wrapper_`.
    return;
  }

  {
    base::AutoLock al(lock_);
    callback_->OnError();
    callback_ = nullptr;
  }
}

void AAudioInputStream::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK(stream_wrapper_);

  AudioInputCallback* temp_error_callback;
  {
    base::AutoLock al(lock_);
    if (!callback_) {
      return;
    }

    // Save a copy of copy of the callback for error reporting.
    temp_error_callback = callback_;

    // OnAudioDataRequested() should no longer provide data from this point on.
    callback_ = nullptr;
  }

  if (!stream_wrapper_->Stop()) {
    temp_error_callback->OnError();
  }
}

void AAudioInputStream::Close() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (stream_wrapper_) {
    stream_wrapper_->Close();
  }

  // Note: This must be last, it will delete |this|.
  audio_manager_->ReleaseInputStream(this);
}

bool AAudioInputStream::OnAudioDataRequested(void* audio_data,
                                             int32_t num_frames) {
  CHECK_EQ(num_frames, audio_bus_->frames());

  base::AutoLock al(lock_);
  if (!callback_) {
    // Stop() might have already been called, but there can still be pending
    // data callbacks in flight.
    return false;
  }

  audio_bus_->FromInterleaved<Float32SampleTypeTraits>(
      reinterpret_cast<float*>(audio_data), num_frames);

  peak_detector_.FindPeak(audio_bus_.get());

  const base::TimeTicks capture_time = stream_wrapper_->GetCaptureTimestamp();

  callback_->OnData(audio_bus_.get(), capture_time, 0.0, {});

  return true;
}

void AAudioInputStream::OnError() {
  base::AutoLock al(lock_);
  if (callback_) {
    callback_->OnError();
  }
}

void AAudioInputStream::OnDeviceChange() {
  // This will post to the owning sequence.
  handle_device_change_on_main_sequence_.Run();
}

void AAudioInputStream::HandleDeviceChange() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // We should only receive this call if we already had a stream.
  CHECK(stream_wrapper_);

  stream_wrapper_->Close();
  stream_wrapper_.reset();
  CreateStreamWrapper();

  bool open_success = stream_wrapper_->Open();

  base::AutoLock al(lock_);
  if (!open_success) {
    if (callback_) {
      callback_->OnError();
    } else {
      // Report this error at the next start() call.
      error_during_device_change_ = true;
    }
    return;
  }

  if (!callback_) {
    // `this` might have been stopped between OnDeviceChange() and now.
    return;
  }

  if (!stream_wrapper_->Start()) {
    callback_->OnError();
  }
}

double AAudioInputStream::GetMaxVolume() {
  return 0.0;
}

void AAudioInputStream::SetVolume(double volume) {}

double AAudioInputStream::GetVolume() {
  return 0.0;
}

bool AAudioInputStream::SetAutomaticGainControl(bool enabled) {
  return false;
}

bool AAudioInputStream::GetAutomaticGainControl() {
  return false;
}

bool AAudioInputStream::IsMuted() {
  return false;
}

void AAudioInputStream::SetOutputDeviceForAec(
    const std::string& output_device_id) {
  // Not supported. Do nothing.
}

}  // namespace media
