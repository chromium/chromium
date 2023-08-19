// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/android/aaudio_stream_wrapper.h"

#include "base/android/build_info.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "base/trace_event/trace_event.h"
#include "media/audio/android/aaudio_stubs.h"

namespace media {

// Used to circumvent issues where the AAudio thread callbacks continue
// after AAudioStream_requestStop() completes. See crbug.com/1183255.
class LOCKABLE AAudioDestructionHelper {
 public:
  explicit AAudioDestructionHelper(AAudioStreamWrapper* wrapper)
      : wrapper_(wrapper) {}

  ~AAudioDestructionHelper() {
    CHECK(is_closing_);
    if (aaudio_stream_) {
      AAudioStream_close(aaudio_stream_);
    }
  }

  AAudioStreamWrapper* GetAndLockWrapper() EXCLUSIVE_LOCK_FUNCTION() {
    lock_.Acquire();
    return is_closing_ ? nullptr : wrapper_.get();
  }

  void UnlockWrapper() UNLOCK_FUNCTION() { lock_.Release(); }

  void DeferStreamClosure(AAudioStream* stream) {
    base::AutoLock al(lock_);
    CHECK(!is_closing_);

    is_closing_ = true;
    aaudio_stream_ = stream;
  }

 private:
  base::Lock lock_;
  const raw_ptr<AAudioStreamWrapper> wrapper_ GUARDED_BY(lock_) = nullptr;
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

  AAudioStreamWrapper* wrapper = destruction_helper->GetAndLockWrapper();

  aaudio_data_callback_result_t result = AAUDIO_CALLBACK_RESULT_STOP;
  if (wrapper) {
    result = wrapper->OnAudioDataRequested(audio_data, num_frames);
  }

  destruction_helper->UnlockWrapper();

  return result;
}

static void OnStreamErrorCallback(AAudioStream* stream,
                                  void* user_data,
                                  aaudio_result_t error) {
  AAudioDestructionHelper* destruction_helper =
      reinterpret_cast<AAudioDestructionHelper*>(user_data);

  AAudioStreamWrapper* wrapper = destruction_helper->GetAndLockWrapper();

  if (wrapper) {
    wrapper->OnStreamError(error);
  }

  destruction_helper->UnlockWrapper();
}

AAudioStreamWrapper::AAudioStreamWrapper(DataCallback* callback,
                                         StreamType stream_type,
                                         const AudioParameters& params,
                                         aaudio_usage_t usage)
    : params_(params),
      stream_type_(stream_type),
      usage_(usage),
      callback_(callback),
      ns_per_frame_(base::Time::kNanosecondsPerSecond /
                    static_cast<double>(params.sample_rate())),
      destruction_helper_(std::make_unique<AAudioDestructionHelper>(this)) {
  CHECK(params.IsValid());
  CHECK(callback_);

  switch (params.latency_tag()) {
    case AudioLatency::Type::kExactMS:
    case AudioLatency::Type::kInteractive:
    case AudioLatency::Type::kRtc:
      performance_mode_ = AAUDIO_PERFORMANCE_MODE_LOW_LATENCY;
      break;
    case AudioLatency::Type::kPlayback:
      performance_mode_ = AAUDIO_PERFORMANCE_MODE_POWER_SAVING;
      break;
    case AudioLatency::Type::kUnknown:
      performance_mode_ = AAUDIO_PERFORMANCE_MODE_NONE;
  }

  TRACE_EVENT2("audio", "AAudioStreamWrapper::AAudioStreamWrapper",
               "AAUDIO_PERFORMANCE_MODE_LOW_LATENCY",
               performance_mode_ == AAUDIO_PERFORMANCE_MODE_LOW_LATENCY
                   ? "true"
                   : "false",
               "frames_per_buffer", params_.frames_per_buffer());
}

AAudioStreamWrapper::~AAudioStreamWrapper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!is_closed_) {
    Close();
  }

  CHECK(!aaudio_stream_);

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

bool AAudioStreamWrapper::Open() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!is_closed_);

  AAudioStreamBuilder* builder;
  auto result = AAudio_createStreamBuilder(&builder);
  if (AAUDIO_OK != result) {
    return false;
  }

  // Parameters
  AAudioStreamBuilder_setDirection(
      builder, (stream_type_ == StreamType::kInput ? AAUDIO_DIRECTION_INPUT
                                                   : AAUDIO_DIRECTION_OUTPUT));
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

  if (AAUDIO_OK != result) {
    CHECK(!aaudio_stream_);
    return false;
  }

  // After opening the stream, sets the effective buffer size to 3X the burst
  // size to prevent glitching if the burst is small (e.g. < 128). On some
  // devices you can get by with 1X or 2X, but 3X is safer.
  int32_t frames_per_burst = AAudioStream_getFramesPerBurst(aaudio_stream_);
  int32_t size_requested = frames_per_burst * (frames_per_burst < 128 ? 3 : 2);
  AAudioStream_setBufferSizeInFrames(aaudio_stream_, size_requested);

  TRACE_EVENT2("audio", "AAudioStreamWrapper::Open", "params",
               params_.AsHumanReadableString(), "requested buffer size",
               size_requested);

  return true;
}

void AAudioStreamWrapper::Close() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!is_closed_);

  Stop();

  // |destruction_helper_->GetStreamAndLock()| will return nullptr after this.
  destruction_helper_->DeferStreamClosure(aaudio_stream_);

  // We shouldn't be accessing |aaudio_stream_| after it's stopped.
  aaudio_stream_ = nullptr;

  is_closed_ = true;
}

bool AAudioStreamWrapper::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(aaudio_stream_);
  CHECK(!is_closed_);

  auto result = AAudioStream_requestStart(aaudio_stream_);
  if (result != AAUDIO_OK) {
    DLOG(ERROR) << "Failed to start audio stream, result: "
                << AAudio_convertResultToText(result);
  }

  return result == AAUDIO_OK;
}

bool AAudioStreamWrapper::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!is_closed_);

  if (!aaudio_stream_) {
    return true;
  }

  // Note: This call may or may not be asynchronous, depending on the Android
  // version.
  auto result = AAudioStream_requestStop(aaudio_stream_);

  if (result != AAUDIO_OK) {
    DLOG(ERROR) << "Failed to stop audio stream, result: "
                << AAudio_convertResultToText(result);
    return false;
  }

  // Wait for AAUDIO_STREAM_STATE_STOPPED, but do not explicitly check for the
  // success of this wait.
  aaudio_stream_state_t current_state = AAUDIO_STREAM_STATE_STOPPING;
  aaudio_stream_state_t next_state = AAUDIO_STREAM_STATE_UNINITIALIZED;
  static const int64_t kTimeoutNanoseconds = 1e8;
  result = AAudioStream_waitForStateChange(aaudio_stream_, current_state,
                                           &next_state, kTimeoutNanoseconds);

  return true;
}

base::TimeDelta AAudioStreamWrapper::GetOutputDelay(
    base::TimeTicks delay_timestamp) {
  CHECK_EQ(stream_type_, AAudioStreamWrapper::StreamType::kOutput);

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

base::TimeTicks AAudioStreamWrapper::GetCaptureTimestamp() {
  CHECK_EQ(stream_type_, AAudioStreamWrapper::StreamType::kInput);

  // Get the time that at which the last known audio frame was captured.
  int64_t hw_capture_frame_index;
  int64_t hw_capture_frame_pts;
  auto result =
      AAudioStream_getTimestamp(aaudio_stream_, CLOCK_MONOTONIC,
                                &hw_capture_frame_index, &hw_capture_frame_pts);

  if (result != AAUDIO_OK) {
    DLOG(ERROR) << "Failed to get audio latency, result: "
                << AAudio_convertResultToText(result);
    return base::TimeTicks();
  }

  // Calculate the number of frames between our captured frame (the microphone
  // write head) and the current read index.
  const int64_t frame_index_delta =
      hw_capture_frame_index - AAudioStream_getFramesRead(aaudio_stream_);

  // Calculate the time at which the current frame (at the stream read head) was
  // captured.
  const base::TimeDelta current_frame_pts = base::Nanoseconds(
      hw_capture_frame_pts - frame_index_delta * ns_per_frame_);

  return current_frame_pts + base::TimeTicks();
}

aaudio_data_callback_result_t AAudioStreamWrapper::OnAudioDataRequested(
    void* audio_data,
    int32_t num_frames) {
  return callback_->OnAudioDataRequested(audio_data, num_frames)
             ? AAUDIO_CALLBACK_RESULT_CONTINUE
             : AAUDIO_CALLBACK_RESULT_STOP;
}

void AAudioStreamWrapper::OnStreamError(aaudio_result_t error) {
  if (error == AAUDIO_ERROR_DISCONNECTED) {
    callback_->OnDeviceChange();
  } else {
    callback_->OnError();
  }
}

}  // namespace media
