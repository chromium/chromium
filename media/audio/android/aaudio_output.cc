// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/android/aaudio_output.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/checked_math.h"
#include "base/task/sequenced_task_runner.h"
#include "media/audio/android/audio_device.h"
#include "media/audio/android/audio_manager_android.h"
#include "media/audio/audio_features.h"
#include "media/audio/audio_manager.h"
#include "media/base/amplitude_peak_detector.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_pull_fifo.h"
#include "media/base/audio_sample_types.h"
#include "media/base/audio_timestamp_helper.h"

namespace media {

AAudioOutputStream::AAudioOutputStream(
    AudioManagerAndroid* manager,
    const AudioParameters& params,
    android::AudioDevice device,
    aaudio_usage_t usage,
    AmplitudePeakDetector::PeakDetectedCB peak_detected_cb)
    : audio_manager_(manager),
      params_(params),
      peak_detector_(std::move(peak_detected_cb)),
      delay_helper_(params_.sample_rate()),
      stream_wrapper_(this,
                      AAudioStreamWrapper::StreamType::kOutput,
                      params,
                      std::move(device),
                      usage) {
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

  if (base::FeatureList::IsEnabled(features::kAAudioVariableSizedCallbacks)) {
    pull_fifo_ = std::make_unique<AudioPullFifo>(
        params_.channels(), params_.frames_per_buffer(),
        base::BindRepeating(&AAudioOutputStream::RefillFifo,
                            base::Unretained(this)));
  }

  return true;
}

void AAudioOutputStream::Close() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  Stop();
  stream_wrapper_.Close();

  if (audio_manager_) {
    // Note: This must be last, it will delete |this|.
    audio_manager_->ReleaseOutputStream(this);
  }
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

bool AAudioOutputStream::OnAudioDataRequested(base::span<float> audio_data) {
  base::AutoLock al(lock_);
  if (!callback_) {
    // Stop() might have already been called, but there can still be pending
    // data callbacks in flight.
    std::ranges::fill(audio_data, 0.0);
    return false;
  }

  const base::TimeTicks delay_timestamp = base::TimeTicks::Now();
  const base::TimeDelta delay = stream_wrapper_.GetOutputDelay(delay_timestamp);

  if (pull_fifo_) {
    delay_helper_.SetBaseTimestamp(delay);
    delay_timestamp_ = delay_timestamp;

    const size_t channels = params_.channels();

    while (!audio_data.empty()) {
      const size_t frames_to_pull =
          std::min(audio_data.size() / channels,
                   static_cast<size_t>(audio_bus_->frames()));

      pull_fifo_->Consume(audio_bus_.get(), frames_to_pull);

      audio_bus_->Scale(muted_ ? 0.0 : volume_);
      audio_bus_->ToInterleavedPartial<Float32SampleTypeTraits>(
          0, audio_data.take_first(frames_to_pull * channels));

      delay_helper_.AddFrames(frames_to_pull);
    }

    return true;
  }

  if (!PullDataFromSource(delay, delay_timestamp, audio_bus_.get())) {
    std::ranges::fill(audio_data, 0.0);
    return true;
  }

  audio_bus_->Scale(muted_ ? 0.0 : volume_);
  audio_bus_->ToInterleaved<Float32SampleTypeTraits>(audio_data);

  return true;
}

void AAudioOutputStream::RefillFifo(int frame_delay, AudioBus* destination) {
  // Consuming data from `pull_fifo_` in `OnAudioDataRequested()` will
  // synchronously call this method, potentially splitting large requests into
  // multiple `Consume()` calls of at most `audio_bus_->frames()`.

  // `delay_helper_.GetTimestamp()` returns the current AAudio output delay.
  // `frame_delay` accounts for frames that were already consumed from
  // `pull_fifo_` before this method was called.
  const base::TimeDelta delay =
      delay_helper_.GetTimestamp() +
      AudioTimestampHelper::FramesToTime(frame_delay, params_.sample_rate());

  PullDataFromSource(delay, delay_timestamp_, destination);
}

bool AAudioOutputStream::PullDataFromSource(base::TimeDelta delay,
                                            base::TimeTicks delay_timestamp,
                                            AudioBus* destination) {
  lock_.AssertAcquired();

  const int frames_filled =
      callback_->OnMoreData(delay, delay_timestamp, {}, destination);

  if (!frames_filled) {
    destination->Zero();
    return false;
  }

  peak_detector_.FindPeak(destination);

  CHECK_EQ(frames_filled, destination->frames());
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
  if (audio_manager_ &&
      audio_manager_->HasOutputVolumeOverride(&volume_override)) {
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
