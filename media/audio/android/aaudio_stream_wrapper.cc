// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/android/aaudio_stream_wrapper.h"

#include <aaudio/AAudio.h>

#include <array>
#include <optional>
#include <string_view>

#include "base/android/device_info.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "base/trace_event/trace_event.h"
#include "media/audio/android/audio_device.h"
#include "media/audio/android/audio_device_id.h"
#include "media/base/audio_parameters.h"
#include "media/base/channel_layout.h"

// AAudioStreamBuilder_setChannelMask was not introduced until API version 32.
#define AAUDIO_CHANNEL_MASK_MIN_API 32

namespace media {

namespace {

constexpr char kAAudioBufferSizeInFramesMetricsPrefix[] =
    "Media.Audio.Android.AAudioBufferSizeInFrames.";
constexpr char kAAudioFramesPerDataCallbackMetricsPrefix[] =
    "Media.Audio.Android.AAudioFramesPerDataCallback.";
constexpr char kAAudioFramesPerBurstMetricsPrefix[] =
    "Media.Audio.Android.AAudioFramesPerBurst.";
constexpr char kAAudioFramesPerBurstChangedMetricsPrefix[] =
    "Media.Audio.Android.AAudioFramesPerBurstChanged.";

std::string_view StreamTypeToStringView(AAudioStreamWrapper::StreamType type) {
  return type == AAudioStreamWrapper::StreamType::kInput ? "Input" : "Output";
}

void LogSparseHistogram(std::string_view prefix,
                        AAudioStreamWrapper::StreamType type,
                        AudioLatency::Type latency_tag,
                        int32_t value) {
  const std::string_view direction = StreamTypeToStringView(type);
  base::UmaHistogramSparse(base::StrCat({prefix, direction}), value);
  base::UmaHistogramSparse(base::StrCat({prefix, direction, ".",
                                         AudioLatency::ToString(latency_tag)}),
                           value);
}

}  // namespace

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

// Matches the ordering of media::Channels.
static constexpr REQUIRES_ANDROID_API(
    AAUDIO_CHANNEL_MASK_MIN_API) auto kMediaChannelToAAudioChannel =
    std::to_array<uint32_t>({
        AAUDIO_CHANNEL_FRONT_LEFT,
        AAUDIO_CHANNEL_FRONT_RIGHT,
        AAUDIO_CHANNEL_FRONT_CENTER,
        AAUDIO_CHANNEL_LOW_FREQUENCY,
        AAUDIO_CHANNEL_BACK_LEFT,
        AAUDIO_CHANNEL_BACK_RIGHT,
        AAUDIO_CHANNEL_FRONT_LEFT_OF_CENTER,
        AAUDIO_CHANNEL_FRONT_RIGHT_OF_CENTER,
        AAUDIO_CHANNEL_BACK_CENTER,
        AAUDIO_CHANNEL_SIDE_LEFT,
        AAUDIO_CHANNEL_SIDE_RIGHT,
    });

REQUIRES_ANDROID_API(AAUDIO_CHANNEL_MASK_MIN_API)
std::optional<aaudio_channel_mask_t> ChannelMaskFromChannelLayout(
    ChannelLayout layout,
    int channels) {
  // Note: ChannelLayout comments define mono as Front Center, but AAudio's
  // AAUDIO_CHANNEL_MONO constant define it as Front Left. Returning Front
  // Center here breaks mono playback, so prefer AAudio's definition.
  if (layout == CHANNEL_LAYOUT_MONO) {
    return AAUDIO_CHANNEL_MONO;
  }

  // Fast path for common case.
  if (layout == CHANNEL_LAYOUT_STEREO) {
    return AAUDIO_CHANNEL_STEREO;
  }

  // Map to canonical AAUDIO_CHANNEL_QUAD channel mask for 4-channel
  // PCM MediaCodec decoded audio. This ensures compatibility with
  // Android devices for signaling 4-channel output.
  if (layout == CHANNEL_LAYOUT_QUAD) {
    return AAUDIO_CHANNEL_QUAD;
  }

  // Map to canonical AAUDIO_CHANNEL_PENTA channel mask for 5-channel
  // PCM MediaCodec decoded audio. This ensures compatibility with
  // Android devices for signaling 5-channel output.
  if (layout == CHANNEL_LAYOUT_5_0) {
    return AAUDIO_CHANNEL_PENTA;
  }

  // Map to canonical AAUDIO_CHANNEL_5POINT1 channel mask for 6-channel
  // PCM MediaCodec decoded audio. This ensures compatibility with
  // Android devices for signaling 6-channel output.
  if (layout == CHANNEL_LAYOUT_5_1) {
    return AAUDIO_CHANNEL_5POINT1;
  }

  if (layout == CHANNEL_LAYOUT_DISCRETE) {
    switch (channels) {
      case 10:
        // Map to canonical AAUDIO_CHANNEL_5POINT1POINT4 channel mask for
        // 10-channel PCM MediaCodec decoded audio. This ensures
        // compatibility with Android devices for signaling 10-channel output.
        return AAUDIO_CHANNEL_5POINT1POINT4;
      case 12:
        // Map to canonical AAUDIO_CHANNEL_7POINT1POINT4 channel mask for
        // 12-channel PCM MediaCodec decoded audio. This ensures
        // compatibility with Android devices for signaling 12-channel output.
        return AAUDIO_CHANNEL_7POINT1POINT4;
      default:
        return std::nullopt;
    }
  }

  aaudio_channel_mask_t mask = 0;

  for (int ch = 0; ch <= Channels::CHANNELS_MAX; ++ch) {
    // Ignore the ordering of the channels, only check whether a channel is
    // present in a given layout.
    if (ChannelOrder(layout, static_cast<Channels>(ch)) != -1) {
      mask |= kMediaChannelToAAudioChannel[ch];
    }
  }

  if (mask) {
    return mask;
  }

  return std::nullopt;
}

REQUIRES_ANDROID_API(AAUDIO_CHANNEL_MASK_MIN_API)
void SetChannelMask(AAudioStreamBuilder* builder,
                    const AudioParameters& params) {
  std::optional<aaudio_channel_mask_t> channel_mask =
      ChannelMaskFromChannelLayout(params.channel_layout(), params.channels());

  if (channel_mask.has_value()) {
    AAudioStreamBuilder_setChannelMask(builder, channel_mask.value());
  } else {
    AAudioStreamBuilder_setChannelCount(builder, params.channels());
  }
}

AAudioStreamWrapper::AAudioStreamWrapper(DataCallback* callback,
                                         StreamType stream_type,
                                         const AudioParameters& params,
                                         android::AudioDevice device,
                                         aaudio_usage_t usage)
    : params_(params),
      requested_device_(std::move(device)),
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
      // For multichannel PCM playback, do not use power saving
      // mode to allow direct multichannel PCM outputs to be opened
      // where available. Limit this to automotive devices only.
      if (params_.channels() > 2 &&
          base::android::device_info::is_automotive()) {
        performance_mode_ = AAUDIO_PERFORMANCE_MODE_NONE;
      } else {
        performance_mode_ = AAUDIO_PERFORMANCE_MODE_POWER_SAVING;
      }
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

  // On Android S+, |destruction_helper_| can be destroyed as part of the
  // normal class teardown.
  if (__builtin_available(android 31, *)) {
    return;
  }

  // In R and earlier, it is possible for callbacks to still be running even
  // after calling AAudioStream_close(). The code below is a mitigation to
  // work around this issue. See crbug.com/1183255.

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
  AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_FLOAT);
  AAudioStreamBuilder_setUsage(builder, usage_);
  AAudioStreamBuilder_setPerformanceMode(builder, performance_mode_);
  AAudioStreamBuilder_setFramesPerDataCallback(builder,
                                               params_.frames_per_buffer());
  AAudioStreamBuilder_setDeviceId(builder,
                                  requested_device_.GetId().ToAAudioDeviceId());

  if (__builtin_available(android AAUDIO_CHANNEL_MASK_MIN_API, *)) {
    SetChannelMask(builder, params_);
  } else {
    AAudioStreamBuilder_setChannelCount(builder, params_.channels());
  }

  if (stream_type_ == StreamType::kInput) {
    // Set AAUDIO_INPUT_PRESET_VOICE_COMMUNICATION when we need echo
    // cancellation. Otherwise, we use AAUDIO_INPUT_PRESET_CAMCORDER instead
    // of the platform default of AAUDIO_INPUT_PRESET_VOICE_RECOGNITION, since
    // it supposedly uses a wideband signal.
    //
    // We do not use AAUDIO_INPUT_PRESET_UNPROCESSED, even if
    // `params_.effects() == AudioParameters::NO_EFFECTS` because the lack of
    // automatic gain control results in quiet, sometimes silent, streams.
    AAudioStreamBuilder_setInputPreset(
        builder, params_.effects() & AudioParameters::ECHO_CANCELLER
                     ? AAUDIO_INPUT_PRESET_VOICE_COMMUNICATION
                     : AAUDIO_INPUT_PRESET_CAMCORDER);
  }

  // Callbacks
  AAudioStreamBuilder_setDataCallback(builder, OnAudioDataRequestedCallback,
                                      destruction_helper_.get());
  AAudioStreamBuilder_setErrorCallback(builder, OnStreamErrorCallback,
                                       destruction_helper_.get());

  result = AAudioStreamBuilder_openStream(builder,
                                          &aaudio_stream_.AsEphemeralRawAddr());

  AAudioStreamBuilder_delete(builder);

  if (AAUDIO_OK != result) {
    CHECK(!aaudio_stream_);
    return false;
  }

  CHECK_EQ(AAUDIO_FORMAT_PCM_FLOAT, AAudioStream_getFormat(aaudio_stream_));

  if (!requested_device_.GetId().IsDefault()) {
    // `AAudioStreamBuilder_setDeviceId` is not guaranteed to set the specified
    // device.
    const int32_t expected_device_id =
        requested_device_.GetId().ToAAudioDeviceId();
    const int32_t actual_device_id = AAudioStream_getDeviceId(aaudio_stream_);
    bool device_id_matches = expected_device_id == actual_device_id;
    EmitSetDeviceIdResultToHistogram(device_id_matches);
    if (!device_id_matches) {
      DLOG(WARNING) << "Failed to set device ID for AAudio stream. Expected: "
                    << expected_device_id << "; actual: " << actual_device_id;
      return false;
    }
  }

  // After opening the stream, sets the effective buffer size to 3X the burst
  // size to prevent glitching if the burst is small (e.g. < 128). On some
  // devices you can get by with 1X or 2X, but 3X is safer.
  const int32_t frames_per_burst =
      AAudioStream_getFramesPerBurst(aaudio_stream_);
  frames_per_burst_on_open_ = frames_per_burst;
  int32_t size_requested = frames_per_burst * (frames_per_burst < 128 ? 3 : 2);
  AAudioStream_setBufferSizeInFrames(aaudio_stream_, size_requested);

  TRACE_EVENT2("audio", "AAudioStreamWrapper::Open", "params",
               params_.AsHumanReadableString(), "requested buffer size",
               size_requested);

  const int32_t buffer_size =
      AAudioStream_getBufferSizeInFrames(aaudio_stream_);
  LogSparseHistogram(kAAudioBufferSizeInFramesMetricsPrefix, stream_type_,
                     params_.latency_tag(), buffer_size);

  const int32_t frames_per_data_callback =
      AAudioStream_getFramesPerDataCallback(aaudio_stream_);
  LogSparseHistogram(kAAudioFramesPerDataCallbackMetricsPrefix, stream_type_,
                     params_.latency_tag(), frames_per_data_callback);

  LogSparseHistogram(kAAudioFramesPerBurstMetricsPrefix, stream_type_,
                     params_.latency_tag(), frames_per_burst);

  return true;
}

void AAudioStreamWrapper::Close() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!is_closed_);

  if (aaudio_stream_) {
    LogFramesPerBurstChangesToUma();
  }

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

std::optional<android::AudioDeviceId> AAudioStreamWrapper::GetActualDeviceId() {
  if (!aaudio_stream_) {
    return std::nullopt;
  }
  int32_t raw_id = AAudioStream_getDeviceId(aaudio_stream_);

  std::optional<android::AudioDeviceId> id =
      android::AudioDeviceId::NonDefault(raw_id);
  if (!id.has_value()) {
    // Empirically, `AAudioStream_getDeviceId` is not expected to fail to
    // determine the actual device ID, but this is not guaranteed by the API.
    LOG(WARNING) << "AAudioStream_getDeviceId failed to return a non-default "
                    "device ID. Requested device ID: "
                 << requested_device_.GetId().ToAAudioDeviceId();
  }
  return id;
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

void AAudioStreamWrapper::EmitSetDeviceIdResultToHistogram(bool success) {
  std::string_view direction_string;
  switch (stream_type_) {
    case StreamType::kInput:
      direction_string = "Input";
      break;
    case StreamType::kOutput:
      direction_string = "Output";
      break;
  }

  std::string_view success_string = success ? "Success" : "Failure";

  std::string histogram_name =
      base::StrCat({"Media.Audio.Android.AAudioSetDeviceId.", direction_string,
                    ".", success_string});
  base::UmaHistogramEnumeration(histogram_name, requested_device_.GetType());
}

void AAudioStreamWrapper::LogFramesPerBurstChangesToUma() {
  const int32_t frames_per_burst_on_close =
      AAudioStream_getFramesPerBurst(aaudio_stream_);
  const std::string_view audio_direction = StreamTypeToStringView(stream_type_);

  const bool frames_per_burst_changed =
      frames_per_burst_on_close != frames_per_burst_on_open_;

  base::UmaHistogramBoolean(
      base::StrCat(
          {kAAudioFramesPerBurstChangedMetricsPrefix, audio_direction}),
      frames_per_burst_changed);

  base::UmaHistogramBoolean(
      base::StrCat({kAAudioFramesPerBurstChangedMetricsPrefix, audio_direction,
                    ".", AudioLatency::ToString(params_.latency_tag())}),
      frames_per_burst_changed);
}

}  // namespace media
