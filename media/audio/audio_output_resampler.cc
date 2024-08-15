// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_output_resampler.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/not_fatal_until.h"
#include "base/numerics/safe_conversions.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_output_dispatcher_impl.h"
#include "media/audio/audio_output_proxy.h"
#if BUILDFLAG(IS_WIN)
#include "media/audio/win/core_audio_util_win.h"
#endif  // BUILDFLAG(IS_WIN)
#include "media/base/audio_converter.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/limits.h"
#include "media/base/sample_rates.h"

namespace media {

class OnMoreDataConverter
    : public AudioOutputStream::AudioSourceCallback,
      public AudioConverter::InputCallback {
 public:
  OnMoreDataConverter(const AudioParameters& input_params,
                      const AudioParameters& output_params,
                      std::unique_ptr<AudioDebugRecorder> debug_recorder);

  OnMoreDataConverter(const OnMoreDataConverter&) = delete;
  OnMoreDataConverter& operator=(const OnMoreDataConverter&) = delete;

  ~OnMoreDataConverter() override;

  // AudioSourceCallback interface.
  int OnMoreData(base::TimeDelta delay,
                 base::TimeTicks delay_timestamp,
                 const AudioGlitchInfo& glitch_info,
                 AudioBus* dest) override;
  void OnError(ErrorType type) override;

  // Sets |source_callback_|.  If this is not a new object, then Stop() must be
  // called before Start().
  void Start(AudioOutputStream::AudioSourceCallback* callback);

  // Clears |source_callback_| and flushes the resampler.
  void Stop();

  bool started() const { return source_callback_ != nullptr; }

  bool error_occurred() const { return error_occurred_; }

 private:
  // AudioConverter::InputCallback implementation.
  double ProvideInput(AudioBus* audio_bus,
                      uint32_t frames_delayed,
                      const AudioGlitchInfo& glitch_info) override;

  // Source callback.
  raw_ptr<AudioOutputStream::AudioSourceCallback> source_callback_;

  // Last |delay| and |delay_timestamp| received via OnMoreData(). Used to
  // correct playback delay in ProvideInput() before calling |source_callback_|.
  base::TimeDelta current_delay_;
  base::TimeTicks current_delay_timestamp_;

  const int input_samples_per_second_;

  // Handles resampling, buffering, and channel mixing between input and output
  // parameters.
  AudioConverter audio_converter_;

  // True if OnError() was ever called.  Should only be read if the underlying
  // stream has been stopped.
  bool error_occurred_;

  // Information about input and output buffer sizes to be traced.
  const int input_buffer_size_;
  const int output_buffer_size_;

  // For audio debug recordings.
  std::unique_ptr<AudioDebugRecorder> debug_recorder_;
};

namespace {

// Record UMA statistics for hardware output configuration.
static void RecordStats(const AudioParameters& output_params) {
  base::UmaHistogramEnumeration(
      "Media.HardwareAudioChannelLayout", output_params.channel_layout(),
      static_cast<ChannelLayout>(CHANNEL_LAYOUT_MAX + 1));
  base::UmaHistogramExactLinear("Media.HardwareAudioChannelCount",
                                output_params.channels(),
                                static_cast<int>(limits::kMaxChannels));

  AudioSampleRate asr;
  if (!ToAudioSampleRate(output_params.sample_rate(), &asr))
    return;

  base::UmaHistogramEnumeration(
      "Media.HardwareAudioSamplesPerSecond", asr,
      static_cast<AudioSampleRate>(kAudioSampleRateMax + 1));
}

// Only Windows has a high latency output driver that is not the same as the low
// latency path.
#if BUILDFLAG(IS_WIN)
// Converts low latency based |output_params| into high latency appropriate
// output parameters in error situations.
AudioParameters GetFallbackHighLatencyOutputParams(
    const AudioParameters& original_output_params) {
  DCHECK_EQ(original_output_params.format(),
            AudioParameters::AUDIO_PCM_LOW_LATENCY);
  // Choose AudioParameters appropriate for opening the device in high latency
  // mode.  |kMinLowLatencyFrameSize| is arbitrarily based on Pepper Flash's
  // MAXIMUM frame size for low latency.
  static const int kMinLowLatencyFrameSize = 2048;
  const int frames_per_buffer = std::max(
      original_output_params.frames_per_buffer(), kMinLowLatencyFrameSize);

  AudioParameters fallback_params(original_output_params);
  fallback_params.set_format(AudioParameters::AUDIO_PCM_LINEAR);
  fallback_params.set_frames_per_buffer(frames_per_buffer);
  return fallback_params;
}

// Get output parameter for low latency non-offload output, which is used for
// falling back from offload mode.
AudioParameters GetFallbackLowLatencyOutputParams(
    const std::string& device_id,
    const AudioParameters& original_output_params) {
  AudioParameters output_params;

  HRESULT hr = CoreAudioUtil::GetPreferredAudioParameters(
      device_id,
      /*is_output_device=*/true, &output_params, /*is_offload_stream=*/false);
  if (SUCCEEDED(hr)) {
    // Retain the channel layout and effects of the original output parameters.
    int effects = output_params.effects();
    effects |= original_output_params.effects();
    output_params.set_effects(effects);
    output_params.SetChannelLayoutConfig(
        original_output_params.channel_layout(),
        original_output_params.channels());
    return output_params;
  }
  // If we fail to get preferred audio parameters, return empty(invalid)
  // parameters so that fallbacking to linear mode will be attempted then.
  return AudioParameters();
}
#endif

// This enum must match the numbering for
// AudioOutputResamplerOpenLowLatencyStreamResult in enums.xml. Do not reorder
// or remove items, only add new items before OPEN_STREAM_MAX.
enum class OpenStreamResult {
  kFail = 0,
  kFallbackToFake = 1,
  kFallbackToLinear = 2,
  kSuccess = 3,
  kFallbackToFakeFail = 4,
  kFallbackToFakeSuccess = 5,
  kFallbackToLinearFail = 6,
  kFallbackToLinearSuccess = 7,
  kSubsequentFail = 8,
  kSubsequentSuccess = 9,
  kFallbackToLowLatencySuccess = 10,
  kOffloadSuccess = 11,
  kMaxValue = kOffloadSuccess,
};

OpenStreamResult GetSubsequentStreamCreationResultBucket(
    const AudioParameters& current_params,
    bool success) {
  switch (current_params.format()) {
    case AudioParameters::AUDIO_PCM_LOW_LATENCY:
      return success ? (current_params.RequireOffload()
                            ? OpenStreamResult::kOffloadSuccess
                            : OpenStreamResult::kSubsequentSuccess)
                     : OpenStreamResult::kSubsequentFail;
    case AudioParameters::AUDIO_PCM_LINEAR:
      return success ? OpenStreamResult::kFallbackToLinearSuccess
                     : OpenStreamResult::kFallbackToLinearFail;
    case AudioParameters::AUDIO_FAKE:
      return success ? OpenStreamResult::kFallbackToFakeSuccess
                     : OpenStreamResult::kFallbackToFakeFail;
    default:
      NOTREACHED();
  }
}

}  // namespace

AudioOutputResampler::AudioOutputResampler(
    AudioManager* audio_manager,
    const AudioParameters& input_params,
    const AudioParameters& output_params,
    const std::string& output_device_id,
    base::TimeDelta close_delay,
    const RegisterDebugRecordingSourceCallback&
        register_debug_recording_source_callback)
    : AudioOutputDispatcher(audio_manager),
      close_delay_(close_delay),
      input_params_(input_params),
      output_params_(output_params),
      original_output_params_(output_params),
      device_id_(output_device_id),
      reinitialize_timer_(
          FROM_HERE,
          close_delay_,
          base::BindRepeating(&AudioOutputResampler::Reinitialize,
                              base::Unretained(this))),
      register_debug_recording_source_callback_(
          register_debug_recording_source_callback) {
  DCHECK(audio_manager->GetTaskRunner()->BelongsToCurrentThread());
  DCHECK(input_params.IsValid());
  DCHECK(output_params.IsValid());
  DCHECK(output_params_.format() == AudioParameters::AUDIO_PCM_LOW_LATENCY ||
         output_params_.format() == AudioParameters::AUDIO_PCM_LINEAR);
  DCHECK(register_debug_recording_source_callback_);

  // Record UMA statistics for the hardware configuration.
  RecordStats(output_params);
}

AudioOutputResampler::~AudioOutputResampler() {
  DCHECK(audio_manager()->GetTaskRunner()->BelongsToCurrentThread());
  for (const auto& item : callbacks_) {
    if (item.second->started())
      StopStreamInternal(item);
  }
}

void AudioOutputResampler::Reinitialize() {
  DCHECK(audio_manager()->GetTaskRunner()->BelongsToCurrentThread());

  // We can only reinitialize the dispatcher if it has no active proxies. Check
  // if one has been created since the reinitialization timer was started.
  if (dispatcher_ && dispatcher_->HasOutputProxies())
    return;

  DCHECK(callbacks_.empty());

  // Log a trace event so we can get feedback in the field when this happens.
  TRACE_EVENT0("audio", "AudioOutputResampler::Reinitialize");

  output_params_ = original_output_params_;
  dispatcher_.reset();
}

std::unique_ptr<AudioOutputDispatcherImpl> AudioOutputResampler::MakeDispatcher(
    const std::string& output_device_id,
    const AudioParameters& params) {
  DCHECK(audio_manager()->GetTaskRunner()->BelongsToCurrentThread());
  DCHECK(callbacks_.empty());
  return std::make_unique<AudioOutputDispatcherImpl>(
      audio_manager(), params, output_device_id, close_delay_);
}

AudioOutputProxy* AudioOutputResampler::CreateStreamProxy() {
  DCHECK(audio_manager()->GetTaskRunner()->BelongsToCurrentThread());
  return new AudioOutputProxy(weak_factory_.GetWeakPtr());
}

bool AudioOutputResampler::OpenStream() {
  DCHECK(audio_manager()->GetTaskRunner()->BelongsToCurrentThread());

  bool first_stream = false;
  if (!dispatcher_) {
    first_stream = true;
    // No open streams => no fallback has happened.
    DCHECK(original_output_params_.Equals(output_params_));
    DCHECK(callbacks_.empty());
    dispatcher_ = MakeDispatcher(device_id_, output_params_);
  }

  constexpr char kFallbackHistogramName[] =
      "Media.FallbackToHighLatencyAudioPath2";
  constexpr char kOpenLowLatencyHistogramName[] =
      "Media.AudioOutputResampler.OpenLowLatencyStream2";
  constexpr char kOpenLowLatencyOffloadHistogramName[] =
      "Media.AudioOutputResampler.OpenLowLatencyStream2.Offload";

  if (dispatcher_->OpenStream()) {
    // Only record the UMA statistic if we didn't fallback during construction
    // and only for the first stream we open.
    if (original_output_params_.format() ==
        AudioParameters::AUDIO_PCM_LOW_LATENCY) {
      if (first_stream)
        base::UmaHistogramBoolean(kFallbackHistogramName, false);

      base::UmaHistogramEnumeration(
          kOpenLowLatencyHistogramName,
          first_stream
              ? (original_output_params_.RequireOffload()
                     ? OpenStreamResult::kOffloadSuccess
                     : OpenStreamResult::kSuccess)
              : GetSubsequentStreamCreationResultBucket(output_params_, true));
    }
    return true;
  }

  // Fallback is available for low latency streams only.
  if (original_output_params_.format() !=
      AudioParameters::AUDIO_PCM_LOW_LATENCY) {
    return false;
  }

  // If we have successfully opened a stream previously, there's nothing more to
  // be done.
  if (!first_stream) {
    if (original_output_params_.RequireOffload()) {
      base::UmaHistogramEnumeration(
          kOpenLowLatencyOffloadHistogramName,
          GetSubsequentStreamCreationResultBucket(output_params_, false));
      return false;
    }
    base::UmaHistogramEnumeration(
        kOpenLowLatencyHistogramName,
        GetSubsequentStreamCreationResultBucket(output_params_, false));
    return false;
  }

  // Only Windows has a high latency output driver that is not the same as the
  // low latency path; or it may originally be attempted to be initialized in
  // offload mode while rejected later due to resource limitation.
#if BUILDFLAG(IS_WIN)
  // If Open() fails with offload mode, first try to fallback to non-offload
  // mode.
  if (original_output_params_.RequireOffload() &&
      output_params_.RequireOffload()) {
    DLOG(ERROR)
        << "Unable to open device in offload mode. Attempt to fallback to "
        << "non-offloaded low latency mode.";
    output_params_ = GetFallbackLowLatencyOutputParams(
        device_id_.empty() ? CoreAudioUtil::GetDefaultOutputDeviceID()
                           : device_id_,
        output_params_);
    if (output_params_.IsValid()) {
      dispatcher_ = MakeDispatcher(device_id_, output_params_);
      if (dispatcher_->OpenStream()) {
        base::UmaHistogramEnumeration(
            kOpenLowLatencyHistogramName,
            OpenStreamResult::kFallbackToLowLatencySuccess);
        return true;
      }
    }
  }

  base::UmaHistogramBoolean(kFallbackHistogramName, true);

  DLOG(ERROR) << "Unable to open audio device in low latency mode.  Falling "
              << "back to high latency audio output.";

  output_params_ = GetFallbackHighLatencyOutputParams(original_output_params_);
  const std::string fallback_device_id = "";
  dispatcher_ = MakeDispatcher(fallback_device_id, output_params_);
  if (dispatcher_->OpenStream()) {
    base::UmaHistogramEnumeration(kOpenLowLatencyHistogramName,
                                  OpenStreamResult::kFallbackToLinear);
    return true;
  }
#endif

  DLOG(ERROR) << "Unable to open audio device in high latency mode.  Falling "
              << "back to fake audio output.";

  // Finally fall back to a fake audio output device.
  output_params_ = input_params_;
  output_params_.set_format(AudioParameters::AUDIO_FAKE);
  dispatcher_ = MakeDispatcher(device_id_, output_params_);
  if (dispatcher_->OpenStream()) {
    base::UmaHistogramEnumeration(kOpenLowLatencyHistogramName,
                                  OpenStreamResult::kFallbackToFake);
    return true;
  }

  // Resetting the malfunctioning dispatcher.
  Reinitialize();

  base::UmaHistogramEnumeration(kOpenLowLatencyHistogramName,
                                OpenStreamResult::kFail);
  return false;
}

bool AudioOutputResampler::StartStream(
    AudioOutputStream::AudioSourceCallback* callback,
    AudioOutputProxy* stream_proxy) {
  DCHECK(audio_manager()->GetTaskRunner()->BelongsToCurrentThread());
  DCHECK(dispatcher_);

  OnMoreDataConverter* resampler_callback = nullptr;
  auto it = callbacks_.find(stream_proxy);
  if (it == callbacks_.end()) {
    // If a register callback has been given, register and pass the returned
    // recoder to the converter. Data is fed to same recorder for the lifetime
    // of the converter, which is until the stream is closed.
    resampler_callback = new OnMoreDataConverter(
        input_params_, output_params_,
        register_debug_recording_source_callback_.Run(output_params_));
    callbacks_[stream_proxy] =
        base::WrapUnique<OnMoreDataConverter>(resampler_callback);
  } else {
    resampler_callback = it->second.get();
  }

  resampler_callback->Start(callback);
  bool result = dispatcher_->StartStream(resampler_callback, stream_proxy);
  if (!result)
    resampler_callback->Stop();
  return result;
}

void AudioOutputResampler::StreamVolumeSet(AudioOutputProxy* stream_proxy,
                                           double volume) {
  DCHECK(audio_manager()->GetTaskRunner()->BelongsToCurrentThread());
  DCHECK(dispatcher_);
  dispatcher_->StreamVolumeSet(stream_proxy, volume);
}

void AudioOutputResampler::StopStream(AudioOutputProxy* stream_proxy) {
  DCHECK(audio_manager()->GetTaskRunner()->BelongsToCurrentThread());

  auto it = callbacks_.find(stream_proxy);
  CHECK(it != callbacks_.end(), base::NotFatalUntil::M130);
  StopStreamInternal(*it);
}

void AudioOutputResampler::CloseStream(AudioOutputProxy* stream_proxy) {
  DCHECK(audio_manager()->GetTaskRunner()->BelongsToCurrentThread());
  DCHECK(dispatcher_);

  dispatcher_->CloseStream(stream_proxy);

  // We assume that StopStream() is always called prior to CloseStream(), so
  // that it is safe to delete the OnMoreDataConverter here.
  callbacks_.erase(stream_proxy);

  // Start the reinitialization timer if there are no active proxies and we're
  // not using the originally requested output parameters.  This allows us to
  // recover from transient output creation errors.
  if (!dispatcher_->HasOutputProxies() && callbacks_.empty() &&
      !output_params_.Equals(original_output_params_)) {
    reinitialize_timer_.Reset();
  }
}

void AudioOutputResampler::FlushStream(AudioOutputProxy* stream_proxy) {
  DCHECK(audio_manager()->GetTaskRunner()->BelongsToCurrentThread());
  DCHECK(dispatcher_);

  dispatcher_->FlushStream(stream_proxy);
}

void AudioOutputResampler::StopStreamInternal(
    const CallbackMap::value_type& item) {
  DCHECK(audio_manager()->GetTaskRunner()->BelongsToCurrentThread());
  DCHECK(dispatcher_);
  AudioOutputProxy* stream_proxy = item.first;
  OnMoreDataConverter* callback = item.second.get();
  DCHECK(callback->started());

  // Stop the underlying physical stream.
  dispatcher_->StopStream(stream_proxy);

  // Now that StopStream() has completed the underlying physical stream should
  // be stopped and no longer calling OnMoreData(), making it safe to Stop() the
  // OnMoreDataConverter.
  callback->Stop();

  // Destroy idle streams if any errors occurred during output; this ensures
  // bad streams will not be reused.  Note: Errors may occur during the Stop()
  // call above.
  if (callback->error_occurred())
    dispatcher_->CloseAllIdleStreams();
}

OnMoreDataConverter::OnMoreDataConverter(
    const AudioParameters& input_params,
    const AudioParameters& output_params,
    std::unique_ptr<AudioDebugRecorder> debug_recorder)
    : source_callback_(nullptr),
      input_samples_per_second_(input_params.sample_rate()),
      audio_converter_(input_params, output_params, false),
      error_occurred_(false),
      input_buffer_size_(input_params.frames_per_buffer()),
      output_buffer_size_(output_params.frames_per_buffer()),
      debug_recorder_(std::move(debug_recorder)) {}

OnMoreDataConverter::~OnMoreDataConverter() {
  // Ensure Stop() has been called so we don't end up with an AudioOutputStream
  // calling back into OnMoreData() after destruction.
  CHECK(!source_callback_);
}

void OnMoreDataConverter::Start(
    AudioOutputStream::AudioSourceCallback* callback) {
  CHECK(!source_callback_);
  CHECK(callback);
  source_callback_ = callback;

  // While AudioConverter can handle multiple inputs, we're using it only with
  // a single input currently.  Eventually this may be the basis for a browser
  // side mixer.
  audio_converter_.AddInput(this);
}

void OnMoreDataConverter::Stop() {
  CHECK(source_callback_);
  audio_converter_.RemoveInput(this);
  source_callback_ = nullptr;
}

int OnMoreDataConverter::OnMoreData(base::TimeDelta delay,
                                    base::TimeTicks delay_timestamp,
                                    const AudioGlitchInfo& glitch_info,
                                    AudioBus* dest) {
  TRACE_EVENT("audio", "OnMoreDataConverter::OnMoreData", "input buffer size",
              input_buffer_size_, "output buffer size", output_buffer_size_,
              "playout_delay (ms)", delay.InMillisecondsF(),
              "delay_timestamp (ms)",
              (delay_timestamp - base::TimeTicks()).InMillisecondsF());
  current_delay_ = delay;
  current_delay_timestamp_ = delay_timestamp;
  audio_converter_.ConvertWithInfo(0, glitch_info, dest);

  if (debug_recorder_)
    debug_recorder_->OnData(dest);

  // Always return the full number of frames requested, ProvideInput()
  // will pad with silence if it wasn't able to acquire enough data.
  return dest->frames();
}

double OnMoreDataConverter::ProvideInput(AudioBus* dest,
                                         uint32_t frames_delayed,
                                         const AudioGlitchInfo& glitch_info) {
  TRACE_EVENT("audio", "OnMoreDataConverter::ProvideInput", "delay (frames)",
              frames_delayed);
  base::TimeDelta new_delay =
      current_delay_ + AudioTimestampHelper::FramesToTime(
                           frames_delayed, input_samples_per_second_);
  // Retrieve data from the original callback.
  const int frames = source_callback_->OnMoreData(
      new_delay, current_delay_timestamp_, glitch_info, dest);

  // Zero any unfilled frames if anything was filled, otherwise we'll just
  // return a volume of zero and let AudioConverter drop the output.
  if (frames > 0 && frames < dest->frames())
    dest->ZeroFramesPartial(frames, dest->frames() - frames);
  return frames > 0 ? 1 : 0;
}

void OnMoreDataConverter::OnError(ErrorType type) {
  error_occurred_ = true;
  source_callback_->OnError(type);
}

}  // namespace media
