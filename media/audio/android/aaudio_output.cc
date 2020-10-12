// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/android/aaudio_output.h"

#include "base/logging.h"
#include "media/audio/android/aaudio_stubs.h"
#include "media/audio/android/audio_manager_android.h"
#include "media/base/audio_bus.h"

namespace media {

static aaudio_data_callback_result_t OnAudioDataRequestedCallback(
    AAudioStream* stream,
    void* user_data,
    void* audio_data,
    int32_t num_frames) {
  AAudioOutputStream* output_stream =
      reinterpret_cast<AAudioOutputStream*>(user_data);
  return output_stream->OnAudioDataRequested(audio_data, num_frames);
}

static void OnStreamErrorCallback(AAudioStream* stream,
                                  void* user_data,
                                  aaudio_result_t error) {
  AAudioOutputStream* output_stream =
      reinterpret_cast<AAudioOutputStream*>(user_data);
  output_stream->OnStreamError(error);
}

AAudioOutputStream::AAudioOutputStream(AudioManagerAndroid* manager,
                                       const AudioParameters& params,
                                       aaudio_usage_t usage)
    : audio_manager_(manager),
      params_(params),
      usage_(usage),
      performance_mode_(AAUDIO_PERFORMANCE_MODE_NONE),
      ns_per_frame_(base::Time::kNanosecondsPerSecond /
                    static_cast<double>(params.sample_rate())) {
  DCHECK(manager);
  DCHECK(params.IsValid());

  if (AudioManagerAndroid::SupportsPerformanceModeForOutput()) {
    if (params.latency_tag() == AudioLatency::LATENCY_PLAYBACK)
      performance_mode_ = AAUDIO_PERFORMANCE_MODE_POWER_SAVING;
    else if (params.latency_tag() == AudioLatency::LATENCY_RTC)
      performance_mode_ = AAUDIO_PERFORMANCE_MODE_LOW_LATENCY;
  }
}

AAudioOutputStream::~AAudioOutputStream() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void AAudioOutputStream::Flush() {}

bool AAudioOutputStream::Open() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  AAudioStreamBuilder* builder;
  auto result = AAudio_createStreamBuilder(&builder);
  if (AAUDIO_OK != result)
    return false;

  // Parameters
  AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
  AAudioStreamBuilder_setSampleRate(builder, params_.sample_rate());
  AAudioStreamBuilder_setChannelCount(builder, params_.channels());
  AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_FLOAT);
  AAudioStreamBuilder_setUsage(builder, usage_);
  AAudioStreamBuilder_setPerformanceMode(builder, performance_mode_);
  AAudioStreamBuilder_setBufferCapacityInFrames(builder,
                                                params_.frames_per_buffer());
  AAudioStreamBuilder_setFramesPerDataCallback(builder,
                                               params_.frames_per_buffer());

  // Callbacks
  AAudioStreamBuilder_setDataCallback(builder, OnAudioDataRequestedCallback,
                                      this);
  AAudioStreamBuilder_setErrorCallback(builder, OnStreamErrorCallback, this);

  result = AAudioStreamBuilder_openStream(builder, &aaudio_stream_);

  AAudioStreamBuilder_delete(builder);

  if (AAUDIO_OK != result)
    return false;

  audio_bus_ = AudioBus::Create(params_);

  return true;
}

void AAudioOutputStream::Close() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  Stop();
  if (aaudio_stream_) {
    const auto result = AAudioStream_close(aaudio_stream_);
    if (result != AAUDIO_OK) {
      DLOG(ERROR) << "Failed to close audio stream, result: "
                  << AAudio_convertResultToText(result);
    }
  }

  // Note: This must be last, it will delete |this|.
  audio_manager_->ReleaseOutputStream(this);
}

void AAudioOutputStream::Start(AudioSourceCallback* callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(aaudio_stream_);

  {
    base::AutoLock al(lock_);

    // The device might have been disconnected between Open() and Start().
    if (device_changed_) {
      callback->OnError(AudioSourceCallback::ErrorType::kDeviceChange);
      return;
    }

    DCHECK(!callback_);
    callback_ = callback;
  }

  auto result = AAudioStream_requestStart(aaudio_stream_);
  if (result != AAUDIO_OK) {
    DLOG(ERROR) << "Failed to start audio stream, result: "
                << AAudio_convertResultToText(result);

    // Lock is required in case a previous asynchronous requestStop() still has
    // not completed by the time we reach this point.
    base::AutoLock al(lock_);
    callback_->OnError(AudioSourceCallback::ErrorType::kUnknown);
    callback_ = nullptr;
  }
}

void AAudioOutputStream::Stop() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  {
    base::AutoLock al(lock_);
    if (!callback_ || !aaudio_stream_)
      return;
  }

  // Note: This call may be asynchronous, so we must clear |callback_| under
  // lock below to ensure no further calls occur after Stop(). Since it may
  // not always be asynchronous, we don't hold |lock_| while we call stop.
  auto result = AAudioStream_requestStop(aaudio_stream_);

  {
    base::AutoLock al(lock_);
    if (result != AAUDIO_OK) {
      DLOG(ERROR) << "Failed to stop audio stream, result: "
                  << AAudio_convertResultToText(result);
      callback_->OnError(AudioSourceCallback::ErrorType::kUnknown);
    }

    callback_ = nullptr;
  }

  // Wait for AAUDIO_STREAM_STATE_STOPPED, but do not explicitly check for the
  // success of this wait.
  aaudio_stream_state_t current_state = AAUDIO_STREAM_STATE_STOPPING;
  aaudio_stream_state_t next_state = AAUDIO_STREAM_STATE_UNINITIALIZED;
  static const int64_t kTimeoutNanoseconds = 1e8;
  result = AAudioStream_waitForStateChange(aaudio_stream_, current_state,
                                           &next_state, kTimeoutNanoseconds);
}

base::TimeDelta AAudioOutputStream::GetDelay(base::TimeTicks delay_timestamp) {
  // Get the time that a known audio frame was presented for playing.
  int64_t existing_frame_index;
  int64_t existing_frame_pts;
  auto result =
      AAudioStream_getTimestamp(aaudio_stream_, CLOCK_MONOTONIC,
                                &existing_frame_index, &existing_frame_pts);

  if (result != AAUDIO_OK) {
    DLOG(ERROR) << "Failed to get audio latency, result: "
                << AAudio_convertResultToText(result);
    return base::TimeDelta();
  }

  // Calculate the number of frames between our known frame and the write index.
  const int64_t frame_index_delta =
      AAudioStream_getFramesWritten(aaudio_stream_) - existing_frame_index;

  // Calculate the time which the next frame will be presented.
  const base::TimeDelta next_frame_pts = base::TimeDelta::FromNanosecondsD(
      existing_frame_pts + frame_index_delta * ns_per_frame_);

  // Calculate the latency between write time and presentation time. At startup
  // we may end up with negative values here.
  return std::max(base::TimeDelta(),
                  next_frame_pts - (delay_timestamp - base::TimeTicks()));
}

aaudio_data_callback_result_t AAudioOutputStream::OnAudioDataRequested(
    void* audio_data,
    int32_t num_frames) {
  // TODO(tguilbert): This can be downgraded to a DCHECK after we've launched.
  CHECK_EQ(num_frames, audio_bus_->frames());

  base::AutoLock al(lock_);
  if (!callback_)
    return AAUDIO_CALLBACK_RESULT_STOP;

  const base::TimeTicks delay_timestamp = base::TimeTicks::Now();
  const base::TimeDelta delay = GetDelay(delay_timestamp);

  const int frames_filled =
      callback_->OnMoreData(delay, delay_timestamp, 0, audio_bus_.get());

  audio_bus_->Scale(muted_ ? 0.0 : volume_);
  audio_bus_->ToInterleaved<Float32SampleTypeTraits>(
      frames_filled, reinterpret_cast<float*>(audio_data));
  return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

void AAudioOutputStream::OnStreamError(aaudio_result_t error) {
  base::AutoLock al(lock_);

  if (error == AAUDIO_ERROR_DISCONNECTED)
    device_changed_ = true;

  if (!callback_)
    return;

  if (device_changed_) {
    callback_->OnError(AudioSourceCallback::ErrorType::kDeviceChange);
    return;
  }

  // TODO(dalecurtis): Consider sending a translated |error| code.
  callback_->OnError(AudioSourceCallback::ErrorType::kUnknown);
}

void AAudioOutputStream::SetVolume(double volume) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  double volume_override = 0;
  if (audio_manager_->HasOutputVolumeOverride(&volume_override))
    volume = volume_override;

  if (volume < 0.0 || volume > 1.0)
    return;

  base::AutoLock al(lock_);
  volume_ = volume;
}

void AAudioOutputStream::GetVolume(double* volume) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::AutoLock al(lock_);
  *volume = volume_;
}

void AAudioOutputStream::SetMute(bool muted) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  base::AutoLock al(lock_);
  muted_ = muted;
}

}  // namespace media
