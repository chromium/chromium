// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/android/aaudio_output.h"

#include "base/android/build_info.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "base/trace_event/trace_event.h"
#include "media/audio/android/aaudio_stubs.h"
#include "media/audio/android/audio_manager_android.h"
#include "media/base/audio_bus.h"

namespace media {

// Used to circumvent issues where the AAudio thread callbacks continue
// after AAudioStream_requestStop() completes. See crbug.com/1183255.
class LOCKABLE AAudioDestructionHelper {
 public:
  explicit AAudioDestructionHelper(AAudioOutputStream* stream)
      : output_stream_(stream) {}

  ~AAudioDestructionHelper() {
    DCHECK(is_closing_);
    if (aaudio_stream_)
      AAudioStream_close(aaudio_stream_);
  }

  AAudioOutputStream* GetAndLockStream() EXCLUSIVE_LOCK_FUNCTION() {
    lock_.Acquire();
    return is_closing_ ? nullptr : output_stream_.get();
  }

  void UnlockStream() UNLOCK_FUNCTION() { lock_.Release(); }

  void DeferStreamClosure(AAudioStream* stream) {
    base::AutoLock al(lock_);
    DCHECK(!is_closing_);

    is_closing_ = true;
    aaudio_stream_ = stream;
  }

 private:
  base::Lock lock_;
  raw_ptr<AAudioOutputStream> output_stream_ GUARDED_BY(lock_) = nullptr;
  raw_ptr<AAudioStream> aaudio_stream_ GUARDED_BY(lock_) = nullptr;
  bool is_closing_ GUARDED_BY(lock_) = false;
};

static aaudio_data_callback_result_t OnAudioDataRequestedCallback(
    AAudioStream* stream,
    void* user_data,
    void* audio_data,
    int32_t num_frames) {
  AAudioDestructionHelper* destruction_helper =
      reinterpret_cast<AAudioDestructionHelper*>(user_data);

  AAudioOutputStream* output_stream = destruction_helper->GetAndLockStream();

  aaudio_data_callback_result_t result = AAUDIO_CALLBACK_RESULT_STOP;
  if (output_stream)
    result = output_stream->OnAudioDataRequested(audio_data, num_frames);

  destruction_helper->UnlockStream();

  return result;
}

static void OnStreamErrorCallback(AAudioStream* stream,
                                  void* user_data,
                                  aaudio_result_t error) {
  AAudioDestructionHelper* destruction_helper =
      reinterpret_cast<AAudioDestructionHelper*>(user_data);

  AAudioOutputStream* output_stream = destruction_helper->GetAndLockStream();

  if (output_stream)
    output_stream->OnStreamError(error);

  destruction_helper->UnlockStream();
}

AAudioOutputStream::AAudioOutputStream(AudioManagerAndroid* manager,
                                       const AudioParameters& params,
                                       aaudio_usage_t usage)
    : audio_manager_(manager),
      params_(params),
      usage_(usage),
      performance_mode_(AAUDIO_PERFORMANCE_MODE_NONE),
      ns_per_frame_(base::Time::kNanosecondsPerSecond /
                    static_cast<double>(params.sample_rate())),
      destruction_helper_(std::make_unique<AAudioDestructionHelper>(this)) {
  DCHECK(manager);
  DCHECK(params.IsValid());

  if (AudioManagerAndroid::SupportsPerformanceModeForOutput()) {
    switch (params.latency_tag()) {
      case AudioLatency::LATENCY_EXACT_MS:
      case AudioLatency::LATENCY_INTERACTIVE:
      case AudioLatency::LATENCY_RTC:
        performance_mode_ = AAUDIO_PERFORMANCE_MODE_LOW_LATENCY;
        break;
      case AudioLatency::LATENCY_PLAYBACK:
        performance_mode_ = AAUDIO_PERFORMANCE_MODE_POWER_SAVING;
        break;
      default:
        performance_mode_ = AAUDIO_PERFORMANCE_MODE_NONE;
    }
  }

  TRACE_EVENT2("audio", "AAudioOutputStream::AAudioOutputStream",
               "AAUDIO_PERFORMANCE_MODE_LOW_LATENCY",
               performance_mode_ == AAUDIO_PERFORMANCE_MODE_LOW_LATENCY
                   ? "true" : "false",
               "frames_per_buffer", params.frames_per_buffer());
}

AAudioOutputStream::~AAudioOutputStream() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (base::android::SdkVersion::SDK_VERSION_S >=
      base::android::BuildInfo::GetInstance()->sdk_int()) {
    // On Android S+, |destruction_helper_| can be destroyed as part of the
    // normal class teardown.
    return;
  }

  // In R and earlier, it is possible for callbacks to still be running even
  // after calling AAudioStream_close(). The code below is a mitigation to work
  // around this issue. See crbug.com/1183255.

  // Keep |destruction_helper_| alive longer than |this|, so the |user_data|
  // bound to the callback stays valid, until the callbacks stop.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::DoNothingWithBoundArgs(std::move(destruction_helper_)),
      base::Seconds(1));
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
  AAudioStreamBuilder_setFramesPerDataCallback(builder,
                                               params_.frames_per_buffer());

  // Callbacks
  AAudioStreamBuilder_setDataCallback(builder, OnAudioDataRequestedCallback,
                                      destruction_helper_.get());
  AAudioStreamBuilder_setErrorCallback(builder, OnStreamErrorCallback,
                                       destruction_helper_.get());

  result = AAudioStreamBuilder_openStream(builder, &aaudio_stream_);

  AAudioStreamBuilder_delete(builder);

  if (AAUDIO_OK != result)
    return false;

  // After opening the stream, sets the effective buffer size to 3X the burst
  // size to prevent glitching if the burst is small (e.g. < 128). On some
  // devices you can get by with 1X or 2X, but 3X is safer.
  int32_t framesPerBurst = AAudioStream_getFramesPerBurst(aaudio_stream_);
  int32_t sizeRequested = framesPerBurst * (framesPerBurst < 128 ? 3 : 2);
  AAudioStream_setBufferSizeInFrames(aaudio_stream_, sizeRequested);

  audio_bus_ = AudioBus::Create(params_);

  TRACE_EVENT2("audio", "AAudioOutputStream::Open",
               "params_", params_.AsHumanReadableString(),
               "requested BufferSizeInFrames", sizeRequested);

  return true;
}

void AAudioOutputStream::Close() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  Stop();

  // |destruction_helper_->GetStreamAndLock()| will return nullptr after this.
  destruction_helper_->DeferStreamClosure(aaudio_stream_);

  // We shouldn't be acessing |aaudio_stream_| after it's stopped.
  aaudio_stream_ = nullptr;

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
  const base::TimeDelta next_frame_pts =
      base::Nanoseconds(existing_frame_pts + frame_index_delta * ns_per_frame_);

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
      callback_->OnMoreData(delay, delay_timestamp, {}, audio_bus_.get());

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
