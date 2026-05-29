// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/android/aaudio_input.h"

#include "base/metrics/histogram_functions.h"
#include "base/task/bind_post_task.h"
#include "media/audio/android/audio_device.h"
#include "media/audio/android/audio_manager_android.h"
#include "media/audio/audio_features.h"
#include "media/base/amplitude_peak_detector.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_sample_types.h"
#include "media/base/audio_timestamp_helper.h"

namespace media {

class AAudioInputDiscontinuityReporter {
 public:
  explicit AAudioInputDiscontinuityReporter(const AudioParameters& params)
      : buffer_duration_(
            AudioTimestampHelper::FramesToTime(params.frames_per_buffer(),
                                               params.sample_rate())),
        discontinuity_threshold_(buffer_duration_ / 10) {}

  void CheckAndRecordDiscontinuity(base::TimeTicks capture_time) {
    base::TimeTicks now = base::TimeTicks::Now();

    if (!last_capture_time_.has_value()) {
      last_capture_time_ = capture_time;
      last_report_time_ = now;
      return;
    }

    const base::TimeDelta delta = capture_time - *last_capture_time_;

    if ((delta - buffer_duration_).magnitude() > discontinuity_threshold_) {
      discontinuity_count_++;
    }

    last_capture_time_ = capture_time;

    if (now - last_report_time_ >= base::Seconds(10)) {
      base::UmaHistogramCounts100(
          "Media.Audio.Android.AAudio.TimestampDiscontinuitiesPer10s",
          discontinuity_count_);
      discontinuity_count_ = 0;
      last_report_time_ = now;
    }
  }

 private:
  const base::TimeDelta buffer_duration_;
  const base::TimeDelta discontinuity_threshold_;
  std::optional<base::TimeTicks> last_capture_time_;
  base::TimeTicks last_report_time_;
  int discontinuity_count_ = 0;
};

AAudioInputStream::AAudioInputStream(AudioManagerAndroid* manager,
                                     const AudioParameters& params,
                                     android::AudioDevice device)
    : audio_manager_(manager),
      params_(params),
      device_(std::move(device)),
      peak_detector_(base::BindRepeating(&AudioManager::TraceAmplitudePeak,
                                         base::Unretained(audio_manager_),
                                         /*trace_start=*/true)),
      timestamp_helper_(params_.sample_rate()),
      discontinuity_reporter_(
          std::make_unique<AAudioInputDiscontinuityReporter>(params_)) {
  CHECK(audio_manager_);

  handle_device_change_on_main_sequence_ =
      base::BindPostTaskToCurrentDefault(base::BindRepeating(
          &AAudioInputStream::HandleDeviceChange, weak_factory_.GetWeakPtr()));
}

AAudioInputStream::~AAudioInputStream() = default;

void AAudioInputStream::CreateStreamWrapper() {
  CHECK(!stream_wrapper_);
  stream_wrapper_ = std::make_unique<AAudioStreamWrapper>(
      this, AAudioStreamWrapper::StreamType::kInput, params_, device_,
      AAUDIO_USAGE_VOICE_COMMUNICATION);
}

AudioInputStream::OpenOutcome AAudioInputStream::Open() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CreateStreamWrapper();

  if (!stream_wrapper_->Open()) {
    return AudioInputStream::OpenOutcome::kFailed;
  }

  audio_bus_ = AudioBus::Create(params_);

  if (base::FeatureList::IsEnabled(features::kAAudioVariableSizedCallbacks)) {
    push_fifo_ = std::make_unique<AudioPushFifo>(base::BindRepeating(
        &AAudioInputStream::OnFifoFilled, base::Unretained(this)));
    push_fifo_->Reset(params_.frames_per_buffer());
    wrapper_bus_ = AudioBus::CreateWrapper(params_.channels());
  }

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

  // Acquire the Bluetooth SCO state before starting the stream. The Android
  // framework requires SCO to be active before the stream is started.
  audio_manager_->AcquireScoState(this);
  if (stream_wrapper_->Start()) {
    return;
  }
  // Release the SCO state if the stream failed to start.
  audio_manager_->ReleaseScoState(this);
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

  // Release the Bluetooth SCO state as the stream is stopping.
  audio_manager_->ReleaseScoState(this);

  if (!stream_wrapper_->Stop()) {
    temp_error_callback->OnError();
  }
}

void AAudioInputStream::Close() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (stream_wrapper_) {
    Stop();
    stream_wrapper_->Close();
  }

  // Note: This must be last, it will delete |this|.
  audio_manager_->ReleaseInputStream(this);
}

bool AAudioInputStream::OnAudioDataRequested(base::span<float> audio_data) {
  base::AutoLock al(lock_);
  if (!callback_) {
    // Stop() might have already been called, but there can still be pending
    // data callbacks in flight.
    std::ranges::fill(audio_data, 0.0);
    return false;
  }

  // The time at which the first frame in `audio_data` was captured.
  auto capture_timestamp = stream_wrapper_->GetCaptureTimestamp();

  if (push_fifo_) {
    const size_t channels = params_.channels();
    const size_t max_samples = audio_bus_->frames() * channels;

    timestamp_helper_.SetBaseTimestamp(capture_timestamp - base::TimeTicks());

    while (!audio_data.empty()) {
      const size_t samples_to_process =
          std::min(audio_data.size(), max_samples);
      const size_t frames_to_process = samples_to_process / channels;

      audio_bus_->FromInterleavedPartial<Float32SampleTypeTraits>(
          audio_data.take_first(samples_to_process), 0);

      wrapper_bus_->set_frames(frames_to_process);
      for (int ch = 0; ch < params_.channels(); ++ch) {
        wrapper_bus_->SetChannelData(
            ch, audio_bus_->channel(ch).first(frames_to_process));
      }

      push_fifo_->Push(*wrapper_bus_);
      timestamp_helper_.AddFrames(frames_to_process);
    }
    return true;
  }

  audio_bus_->FromInterleaved<Float32SampleTypeTraits>(audio_data);
  DeliverAudio(*audio_bus_, capture_timestamp);

  return true;
}

void AAudioInputStream::OnFifoFilled(const AudioBus& output_bus,
                                     int frame_delay) {
  // For the sake of simplicity, we assume that all data in the FIFO is
  // contiguous, and we do not track the timestamps of individual frame bursts.
  // If `frame_delay` is negative, the FIFO contained data from
  // previous push calls, and the capture time has to be decreased.
  // Since we break up large chunks of audio into multiple pushes into the FIFO,
  // `frame_delay` is never expected to be positive.
  const base::TimeDelta fifo_delay =
      AudioTimestampHelper::FramesToTime(frame_delay, params_.sample_rate());
  const base::TimeDelta capture_time =
      timestamp_helper_.GetTimestamp() + fifo_delay;

  DeliverAudio(output_bus, capture_time + base::TimeTicks());
}

void AAudioInputStream::DeliverAudio(const AudioBus& audio_bus,
                                     base::TimeTicks capture_time) {
  lock_.AssertAcquired();

  peak_detector_.FindPeak(&audio_bus);

  discontinuity_reporter_->CheckAndRecordDiscontinuity(capture_time);

  callback_->OnData(&audio_bus, capture_time, 0.0, {});
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

  {
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
  }

  // Notify AudioManager before starting the new stream so that global
  // audio routing (e.g. Bluetooth SCO) can be configured correctly.
  audio_manager_->OnAAudioInputStreamDeviceChanged(this);

  if (!stream_wrapper_->Start()) {
    // Release the SCO state as the stream failed to start.
    audio_manager_->ReleaseScoState(this);
    base::AutoLock al(lock_);
    if (callback_) {
      callback_->OnError();
    }
  }
}

bool AAudioInputStream::IsExplicitlyRequestingBluetoothSco() {
  return device_.GetType() == android::AudioDeviceType::kBluetoothSco;
}

std::optional<android::AudioDeviceId> AAudioInputStream::GetActualDeviceId() {
  if (!stream_wrapper_) {
    return std::nullopt;
  }
  return stream_wrapper_->GetActualDeviceId();
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
