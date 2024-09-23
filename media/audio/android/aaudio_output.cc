// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/android/aaudio_output.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "media/audio/android/audio_manager_android.h"
#include "media/audio/audio_manager.h"
#include "media/base/amplitude_peak_detector.h"
#include "media/base/audio_bus.h"

namespace media {

AAudioOutputStream::AAudioOutputStream(AudioManagerAndroid* manager,
                                       const AudioParameters& params,
                                       aaudio_usage_t usage)
    : audio_manager_(manager),
      params_(params),
      peak_detector_(base::BindRepeating(&AudioManager::TraceAmplitudePeak,
                                         base::Unretained(audio_manager_),
                                         /*trace_start=*/false)),
      stream_wrapper_(this,
                      AAudioStreamWrapper::StreamType::kOutput,
                      params,
                      usage) {
  CHECK(manager);
  CHECK(params_.IsValid());
}

AAudioOutputStream::~AAudioOutputStream() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void AAudioOutputStream::Flush() {}

bool AAudioOutputStream::Open() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!stream_wrapper_.Open()) {
    return false;
  }

  CHECK(!audio_bus_);
  audio_bus_ = AudioBus::Create(params_);

  return true;
}

void AAudioOutputStream::Close() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  stream_wrapper_.Close();

  // Note: This must be last, it will delete |this|.
  audio_manager_->ReleaseOutputStream(this);
}

void AAudioOutputStream::Start(AudioSourceCallback* callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  {
    base::AutoLock al(lock_);

    // The device might have been disconnected between Open() and Start().
    if (device_changed_) {
      callback->OnError(AudioSourceCallback::ErrorType::kDeviceChange);
      return;
    }

    CHECK(!callback_);
    callback_ = callback;
  }

  if (stream_wrapper_.Start()) {
    // Successfully started `stream_wrapper_`.
    return;
  }

  {
    base::AutoLock al(lock_);
    callback_->OnError(AudioSourceCallback::ErrorType::kUnknown);
    callback_ = nullptr;
  }
}

void AAudioOutputStream::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  AudioSourceCallback* temp_error_callback;
  {
    base::AutoLock al(lock_);
    if (!callback_) {
      return;
    }

    // Save a copy of copy of the callback for error reporting.
    temp_error_callback = callback_;

    // OnAudioDataRequested should no longer provide data from this point on.
    callback_ = nullptr;
  }

  if (!stream_wrapper_.Stop()) {
    temp_error_callback->OnError(AudioSourceCallback::ErrorType::kUnknown);
  }
}

bool AAudioOutputStream::OnAudioDataRequested(void* audio_data,
                                              int32_t num_frames) {
  CHECK_EQ(num_frames, audio_bus_->frames());

  base::AutoLock al(lock_);
  if (!callback_) {
    // Stop() might have already been called, but there can still be pending
    // data callbacks in flight.
    return false;
  }

  const base::TimeTicks delay_timestamp = base::TimeTicks::Now();
  const base::TimeDelta delay = stream_wrapper_.GetOutputDelay(delay_timestamp);

  const int frames_filled =
      callback_->OnMoreData(delay, delay_timestamp, {}, audio_bus_.get());

  peak_detector_.FindPeak(audio_bus_.get());

  audio_bus_->Scale(muted_ ? 0.0 : volume_);
  audio_bus_->ToInterleaved<Float32SampleTypeTraits>(
      frames_filled, reinterpret_cast<float*>(audio_data));

  return true;
}

void AAudioOutputStream::OnDeviceChange() {
  base::AutoLock al(lock_);

  device_changed_ = true;

  if (!callback_) {
    // Report the device change in Start() instead.
    return;
  }

  callback_->OnError(AudioSourceCallback::ErrorType::kDeviceChange);
}

void AAudioOutputStream::OnError() {
  base::AutoLock al(lock_);

  if (!callback_) {
    return;
  }

  if (device_changed_) {
    // We should have already reported a device change error, either in
    // OnDeviceChange() or in Start(). In both cases, `this` should be closed
    // and deleted soon, so silently ignore additional error reporting.
    return;
  }

  callback_->OnError(AudioSourceCallback::ErrorType::kUnknown);
}

void AAudioOutputStream::SetVolume(double volume) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  double volume_override = 0;
  if (audio_manager_->HasOutputVolumeOverride(&volume_override)) {
    volume = volume_override;
  }

  if (volume < 0.0 || volume > 1.0) {
    return;
  }

  base::AutoLock al(lock_);
  volume_ = volume;
}

void AAudioOutputStream::GetVolume(double* volume) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::AutoLock al(lock_);
  *volume = volume_;
}

void AAudioOutputStream::SetMute(bool muted) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::AutoLock al(lock_);
  muted_ = muted;
}

}  // namespace media
