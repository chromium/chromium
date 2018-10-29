// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/input_controller.h"

#include <inttypes.h>

#include <algorithm>
#include <limits>
#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_manager.h"
#include "media/base/audio_bus.h"
#include "media/base/user_input_monitor.h"
#if defined(AUDIO_PROCESSING_IN_AUDIO_SERVICE)
#include "media/webrtc/audio_processor.h"
#include "media/webrtc/webrtc_switches.h"
#endif

namespace audio {
namespace {

const int kMaxInputChannels = 3;
constexpr base::TimeDelta kCheckMutedStateInterval =
    base::TimeDelta::FromSeconds(1);

#if defined(AUDIO_POWER_MONITORING)
// Time in seconds between two successive measurements of audio power levels.
constexpr base::TimeDelta kPowerMonitorLogInterval =
    base::TimeDelta::FromSeconds(15);

// A warning will be logged when the microphone audio volume is below this
// threshold.
const int kLowLevelMicrophoneLevelPercent = 10;

// Logs if the user has enabled the microphone mute or not. This is normally
// done by marking a checkbox in an audio-settings UI which is unique for each
// platform. Elements in this enum should not be added, deleted or rearranged.
enum MicrophoneMuteResult {
  MICROPHONE_IS_MUTED = 0,
  MICROPHONE_IS_NOT_MUTED = 1,
  MICROPHONE_MUTE_MAX = MICROPHONE_IS_NOT_MUTED
};

void LogMicrophoneMuteResult(MicrophoneMuteResult result) {
  UMA_HISTOGRAM_ENUMERATION("Media.MicrophoneMuted", result,
                            MICROPHONE_MUTE_MAX + 1);
}

// Helper method which calculates the average power of an audio bus. Unit is in
// dBFS, where 0 dBFS corresponds to all channels and samples equal to 1.0.
float AveragePower(const media::AudioBus& buffer) {
  const int frames = buffer.frames();
  const int channels = buffer.channels();
  if (frames <= 0 || channels <= 0)
    return 0.0f;

  // Scan all channels and accumulate the sum of squares for all samples.
  float sum_power = 0.0f;
  for (int ch = 0; ch < channels; ++ch) {
    const float* channel_data = buffer.channel(ch);
    for (int i = 0; i < frames; i++) {
      const float sample = channel_data[i];
      sum_power += sample * sample;
    }
  }

  // Update accumulated average results, with clamping for sanity.
  const float average_power =
      std::max(0.0f, std::min(1.0f, sum_power / (frames * channels)));

  // Convert average power level to dBFS units, and pin it down to zero if it
  // is insignificantly small.
  const float kInsignificantPower = 1.0e-10f;  // -100 dBFS
  const float power_dbfs = average_power < kInsignificantPower
                               ? -std::numeric_limits<float>::infinity()
                               : 10.0f * log10f(average_power);

  return power_dbfs;
}
#endif  // AUDIO_POWER_MONITORING

#if defined(AUDIO_PROCESSING_IN_AUDIO_SERVICE)
bool CanRunApm() {
  return base::FeatureList::IsEnabled(features::kWebRtcApmInAudioService);
}
#endif  // defined(AUDIO_PROCESSING_IN_AUDIO_SERVICE)

}  // namespace

#if defined(AUDIO_PROCESSING_IN_AUDIO_SERVICE)
InputController::ProcessingHelper::ProcessingHelper(
    const media::AudioParameters& params,
    media::AudioProcessingSettings processing_settings,
    mojom::AudioProcessorControlsRequest controls_request)
    : binding_(this, std::move(controls_request)),
      params_(params),
      audio_processor_(
          std::make_unique<media::AudioProcessor>(params,
                                                  processing_settings)) {}

InputController::ProcessingHelper::~ProcessingHelper() {
  DCHECK_CALLED_ON_VALID_THREAD(owning_thread_);
}

void InputController::ProcessingHelper::ChangeMonitoredStream(
    Snoopable* stream) {
  DCHECK_CALLED_ON_VALID_THREAD(owning_thread_);
  TRACE_EVENT1("audio", "AIC ChangeMonitoredStream", "stream", stream);
  if (!audio_processor_)
    return;
  if (monitored_output_stream_ == stream)
    return;

  if (monitored_output_stream_) {
    monitored_output_stream_->StopSnooping(this,
                                           Snoopable::SnoopingMode::kRealtime);
    if (!stream) {
      audio_processor_->set_has_reverse_stream(false);
    }
  }
  monitored_output_stream_ = stream;
  if (!monitored_output_stream_) {
    output_params_ = media::AudioParameters();
    return;
  }
  output_params_ = monitored_output_stream_->GetAudioParameters();
  audio_processor_->set_has_reverse_stream(true);
  monitored_output_stream_->StartSnooping(this,
                                          Snoopable::SnoopingMode::kRealtime);
}

void InputController::ProcessingHelper::OnData(const media::AudioBus& audio_bus,
                                               base::TimeTicks reference_time,
                                               double volume) {
  TRACE_EVENT0("audio", "APM AnalyzePlayout");
  // OnData gets called when the InputController is snooping on an output stream
  // for audio processing purposes. |audio_bus| contains the data from the
  // snooped-upon output stream, not the input stream's data.
  // |volume| is applied in the WebRTC mixer in the renderer, so we don't have
  // to inform the |audio_processor_| of the new volume.
  audio_processor_->AnalyzePlayout(audio_bus, output_params_, reference_time);
}

void InputController::ProcessingHelper::GetStats(GetStatsCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(owning_thread_);
  DCHECK(audio_processor_);
  TRACE_EVENT0("audio", "APM GetStats");
  audio_processor_->GetStats(std::move(callback));
}

void InputController::ProcessingHelper::StartEchoCancellationDump(
    base::File file) {
  DCHECK_CALLED_ON_VALID_THREAD(owning_thread_);
  DCHECK(audio_processor_);
  audio_processor_->StartEchoCancellationDump(std::move(file));
}

void InputController::ProcessingHelper::StopEchoCancellationDump() {
  DCHECK_CALLED_ON_VALID_THREAD(owning_thread_);
  DCHECK(audio_processor_);
  audio_processor_->StopEchoCancellationDump();
}

media::AudioProcessor* InputController::ProcessingHelper::GetAudioProcessor() {
  DCHECK_CALLED_ON_VALID_THREAD(owning_thread_);
  DCHECK(audio_processor_);
  return audio_processor_.get();
}

void InputController::ProcessingHelper::StartMonitoringStream(
    Snoopable* output_stream) {
  DCHECK_CALLED_ON_VALID_THREAD(owning_thread_);
  DCHECK(audio_processor_);
  ChangeMonitoredStream(output_stream);
}

void InputController::ProcessingHelper::StopMonitoringStream(
    Snoopable* output_stream) {
  DCHECK_CALLED_ON_VALID_THREAD(owning_thread_);
  DCHECK(audio_processor_);
  if (output_stream == monitored_output_stream_)
    ChangeMonitoredStream(nullptr);
}

void InputController::ProcessingHelper::StopAllStreamMonitoring() {
  DCHECK_CALLED_ON_VALID_THREAD(owning_thread_);
  DCHECK(audio_processor_);
  ChangeMonitoredStream(nullptr);
}

#endif  // defined(AUDIO_PROCESSING_IN_AUDIO_SERVICE)

// Private subclass of AIC that covers the state while capturing audio.
// This class implements the callback interface from the lower level audio
// layer and gets called back on the audio hw thread.
// We implement this in a sub class instead of directly in the AIC so that
// - The AIC itself is not an AudioInputCallback.
// - The lifetime of the AudioCallback is shorter than the AIC
// - How tasks are posted to the AIC from the hw callback thread, is different
//   than how tasks are posted from the AIC to itself from the main thread.
//   So, this difference is isolated to the subclass (see below).
// - The callback class can gather information on what happened during capture
//   and store it in a state that can be fetched after stopping capture
//   (received_callback, error_during_callback).
// The AIC itself must not be AddRef-ed on the hw callback thread so that we
// can be guaranteed to not receive callbacks generated by the hw callback
// thread after Close() has been called on the audio manager thread and
// the callback object deleted. To avoid AddRef-ing the AIC and to cancel
// potentially pending tasks, we use a weak pointer to the AIC instance
// when posting.
class InputController::AudioCallback
    : public media::AudioInputStream::AudioInputCallback {
 public:
  AudioCallback(
#if defined(AUDIO_PROCESSING_IN_AUDIO_SERVICE)
      media::AudioProcessor* audio_processor,
#endif  // defined(AUDIO_PROCESSING_IN_AUDIO_SERVICE)
      InputController* controller)
      : task_runner_(base::ThreadTaskRunnerHandle::Get()),
#if defined(AUDIO_PROCESSING_IN_AUDIO_SERVICE)
        audio_processor_(audio_processor),
#endif  // defined(AUDIO_PROCESSING_IN_AUDIO_SERVICE)
        controller_(controller),
        weak_controller_(controller->weak_ptr_factory_.GetWeakPtr()) {
  }
  ~AudioCallback() override = default;

  // These should not be called when the stream is live.
  bool received_callback() const { return received_callback_; }
  bool error_during_callback() const { return error_during_callback_; }

 private:
  void OnData(const media::AudioBus* source,
              base::TimeTicks capture_time,
              double volume) override {
    TRACE_EVENT1("audio", "InputController::OnData", "capture time (ms)",
                 (capture_time - base::TimeTicks()).InMillisecondsF());

    received_callback_ = true;

    DeliverDataToSyncWriter(source, capture_time, volume);
  }

  void OnError() override {
    error_during_callback_ = true;
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&InputController::DoReportError, weak_controller_));
  }

  void DeliverDataToSyncWriter(const media::AudioBus* source,
                               base::TimeTicks capture_time,
                               double volume) {
    const bool key_pressed = controller_->CheckForKeyboardInput();

#if defined(AUDIO_PROCESSING_IN_AUDIO_SERVICE)
    base::Optional<double> new_volume;
    if (audio_processor_) {
      TRACE_EVENT0("audio", "APM ProcessCapture");
      auto result = audio_processor_->ProcessCapture(*source, capture_time,
                                                     volume, key_pressed);
      source = &result.audio;
      new_volume = result.new_volume;
    }
#endif  // defined(AUDIO_PROCESSING_IN_AUDIO_SERVICE)

    controller_->sync_writer_->Write(source, volume, key_pressed, capture_time);

    // The way the two classes interact here, could be done in a nicer way.
    // As is, we call the AIC here to check the audio power, return and then
    // post a task to the AIC based on what the AIC said.
    // The reason for this is to keep all PostTask calls from the hw callback
    // thread to the AIC, that use a weak pointer, in the same class.
    float average_power_dbfs;
    int mic_volume_percent;
    if (controller_->CheckAudioPower(source, volume, &average_power_dbfs,
                                     &mic_volume_percent)) {
      // Use event handler on the audio thread to relay a message to the ARIH
      // in content which does the actual logging on the IO thread.
      task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&InputController::DoLogAudioLevels, weak_controller_,
                         average_power_dbfs, mic_volume_percent));
    }

#if defined(AUDIO_PROCESSING_IN_AUDIO_SERVICE)
    // Updates APM stats and stream volume (if needed). Post through
    // weak_controller, in case we're just shutting down.
    if (audio_processor_) {
      task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&InputController::UpdateVolumeAndAPMStats,
                                    weak_controller_, new_volume));
    }
#endif  // defined(AUDIO_PROCESSING_IN_AUDIO_SERVICE)
  }

  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
#if defined(AUDIO_PROCESSING_IN_AUDIO_SERVICE)
  media::AudioProcessor* const audio_processor_;
#endif
  InputController* const controller_;
  // We do not want any pending posted tasks generated from the callback class
  // to keep the controller object alive longer than it should. So we use
  // a weak ptr whenever we post, we use this weak pointer.
  base::WeakPtr<InputController> weak_controller_;
  bool received_callback_ = false;
  bool error_during_callback_ = false;
};

InputController::InputController(
    EventHandler* handler,
    SyncWriter* sync_writer,
    media::UserInputMonitor* user_input_monitor,
    const media::AudioParameters& params,
    StreamType type,
    StreamMonitorCoordinator* stream_monitor_coordinator,
    mojom::AudioProcessingConfigPtr processing_config)
    : handler_(handler),
      stream_(nullptr),
      sync_writer_(sync_writer),
      type_(type),
      user_input_monitor_(user_input_monitor),
      stream_monitor_coordinator_(stream_monitor_coordinator),
      processing_config_(std::move(processing_config)),
      weak_ptr_factory_(this) {
  DCHECK_CALLED_ON_VALID_THREAD(owning_thread_);
  DCHECK(handler_);
  DCHECK(sync_writer_);
  DCHECK(stream_monitor_coordinator);

#if defined(AUDIO_PROCESSING_IN_AUDIO_SERVICE)
  if (processing_config_) {
    if (processing_config_->settings.requires_apm() && CanRunApm()) {
      processing_helper_.emplace(
          params, processing_config_->settings,
          std::move(processing_config_->controls_request));
    } else {
      processing_config_->controls_request.ResetWithReason(0, "");
    }
  }
#endif  // defined(AUDIO_PROCESSING_IN_AUDIO_SERVICE)
}

InputController::~InputController() {
  DCHECK_CALLED_ON_VALID_THREAD(owning_thread_);
  DCHECK(!audio_callback_);
  DCHECK(!stream_);
  DCHECK(!check_muted_state_timer_.IsRunning());
}

// static
std::unique_ptr<InputController> InputController::Create(
    media::AudioManager* audio_manager,
    EventHandler* event_handler,
    SyncWriter* sync_writer,
    media::UserInputMonitor* user_input_monitor,
    const media::AudioParameters& params,
    const std::string& device_id,
    bool enable_agc,
    StreamMonitorCoordinator* stream_monitor_coordinator,
    mojom::AudioProcessingConfigPtr processing_config) {
  DCHECK(audio_manager);
  DCHECK(audio_manager->GetTaskRunner()->BelongsToCurrentThread());
  DCHECK(sync_writer);
  DCHECK(event_handler);
  DCHECK(params.IsValid());

  if (params.channels() > kMaxInputChannels)
    return nullptr;

  // Create the InputController object and ensure that it runs on
  // the audio-manager thread.
  std::unique_ptr<InputController> controller(new InputController(
      event_handler, sync_writer, user_input_monitor, params,
      ParamsToStreamType(params), stream_monitor_coordinator,
      std::move(processing_config)));

  controller->DoCreate(audio_manager, params, device_id, enable_agc);
  return controller;
}

void InputController::Record() {
  DCHECK_CALLED_ON_VALID_THREAD(owning_thread_);
  SCOPED_UMA_HISTOGRAM_TIMER("Media.AudioInputController.RecordTime");

  if (!stream_ || audio_callback_)
    return;

  handler_->OnLog("AIC::Record");

  if (user_input_monitor_) {
    user_input_monitor_->EnableKeyPressMonitoring();
    prev_key_down_count_ = user_input_monitor_->GetKeyPressCount();
  }

  stream_create_time_ = base::TimeTicks::Now();

  audio_callback_.reset(new AudioCallback(
#if defined(AUDIO_PROCESSING_IN_AUDIO_SERVICE)
      processing_helper_ ? processing_helper_->GetAudioProcessor() : nullptr,
#endif
      this));
  if (ShouldRegisterWithStreamMonitorCoordinator()) {
    stream_monitor_coordinator_->RegisterMember(
        processing_config_->processing_id, this);
    registered_to_coordinator_ = true;
  }
  stream_->Start(audio_callback_.get());
  return;
}

void InputController::Close() {
  DCHECK_CALLED_ON_VALID_THREAD(owning_thread_);
  SCOPED_UMA_HISTOGRAM_TIMER("Media.AudioInputController.CloseTime");

  if (!stream_)
    return;

  check_muted_state_timer_.AbandonAndStop();

  if (registered_to_coordinator_) {
    // We should only unregister ourselves from the coordinator if we previously
    // registered.
    stream_monitor_coordinator_->UnregisterMember(
        processing_config_->processing_id, this);
    registered_to_coordinator_ = false;
  }

#if defined(AUDIO_PROCESSING_IN_AUDIO_SERVICE)
  // Disconnect from any output stream, so we don't get called when we're gone.
  if (processing_helper_)
    processing_helper_->StopAllStreamMonitoring();
#endif

  std::string log_string;
  static const char kLogStringPrefix[] = "AIC::DoClose:";

  // Allow calling unconditionally and bail if we don't have a stream to close.
  if (audio_callback_) {
    stream_->Stop();

    // Sometimes a stream (and accompanying audio track) is created and
    // immediately closed or discarded. In this case they are registered as
    // 'stopped early' rather than 'never got data'.
    const base::TimeDelta duration =
        base::TimeTicks::Now() - stream_create_time_;
    CaptureStartupResult capture_startup_result =
        audio_callback_->received_callback()
            ? CAPTURE_STARTUP_OK
            : (duration.InMilliseconds() < 500
                   ? CAPTURE_STARTUP_STOPPED_EARLY
                   : CAPTURE_STARTUP_NEVER_GOT_DATA);
    LogCaptureStartupResult(capture_startup_result);
    LogCallbackError();

    log_string = base::StringPrintf(
        "%s stream duration=%" PRId64 " seconds%s", kLogStringPrefix,
        duration.InSeconds(),
        audio_callback_->received_callback() ? "" : " (no callbacks received)");

    if (type_ == LOW_LATENCY) {
      if (audio_callback_->received_callback()) {
        UMA_HISTOGRAM_LONG_TIMES("Media.InputStreamDuration", duration);
      } else {
        UMA_HISTOGRAM_LONG_TIMES("Media.InputStreamDurationWithoutCallback",
                                 duration);
      }
    }

    if (user_input_monitor_)
      user_input_monitor_->DisableKeyPressMonitoring();

    audio_callback_.reset();
  } else {
    log_string =
        base::StringPrintf("%s recording never started", kLogStringPrefix);
  }

  handler_->OnLog(log_string);

  stream_->Close();
  stream_ = nullptr;

  sync_writer_->Close();

#if defined(AUDIO_POWER_MONITORING)
  // Send UMA stats if enabled.
  if (power_measurement_is_enabled_)
    LogSilenceState(silence_state_);
#endif

  max_volume_ = 0.0;
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void InputController::SetVolume(double volume) {
  DCHECK_CALLED_ON_VALID_THREAD(owning_thread_);
  DCHECK_GE(volume, 0);
  DCHECK_LE(volume, 1.0);

  if (!stream_)
    return;

  // Only ask for the maximum volume at first call and use cached value
  // for remaining function calls.
  if (!max_volume_) {
    max_volume_ = stream_->GetMaxVolume();
  }

  if (max_volume_ == 0.0) {
    DLOG(WARNING) << "Failed to access input volume control";
    return;
  }

  // Set the stream volume and scale to a range matched to the platform.
  stream_->SetVolume(max_volume_ * volume);
}

void InputController::SetOutputDeviceForAec(
    const std::string& output_device_id) {
  DCHECK_CALLED_ON_VALID_THREAD(owning_thread_);
  if (stream_)
    stream_->SetOutputDeviceForAec(output_device_id);
}

bool InputController::ShouldRegisterWithStreamMonitorCoordinator() const {
  // We register with the coordinator if we need it for AEC and we have a
  // processing_id to monitor.
  return processing_config_ && !processing_config_->processing_id.is_empty() &&
         processing_config_->settings.echo_cancellation !=
             media::EchoCancellationType::kDisabled;
}

void InputController::OnStreamActive(Snoopable* output_stream) {
  DCHECK_CALLED_ON_VALID_THREAD(owning_thread_);
  // Always pick the last stream that becomes active. There should be just one
  // active at a time, but in-case the creation of and old stream overlaps with
  // the destruction of a new stream, we still want to be ok.
  switch (processing_config_->settings.echo_cancellation) {
    case media::EchoCancellationType::kSystemAec:
      if (output_stream)
        stream_->SetOutputDeviceForAec(output_stream->GetDeviceId());
      break;
    case media::EchoCancellationType::kAec2:
    case media::EchoCancellationType::kAec3:
#if defined(AUDIO_PROCESSING_IN_AUDIO_SERVICE)
      if (processing_helper_)
        processing_helper_->StartMonitoringStream(output_stream);
#endif
      break;
    case media::EchoCancellationType::kDisabled:
      // Do nothing.
      break;
  }
}

void InputController::OnStreamInactive(Snoopable* output_stream) {
  DCHECK_CALLED_ON_VALID_THREAD(owning_thread_);
#if defined(AUDIO_PROCESSING_IN_AUDIO_SERVICE)
  if (processing_helper_)
    processing_helper_->StopMonitoringStream(output_stream);
#endif
}

void InputController::DoCreate(media::AudioManager* audio_manager,
                               const media::AudioParameters& params,
                               const std::string& device_id,
                               bool enable_agc) {
  DCHECK_CALLED_ON_VALID_THREAD(owning_thread_);
  DCHECK(!stream_);
  SCOPED_UMA_HISTOGRAM_TIMER("Media.AudioInputController.CreateTime");
  handler_->OnLog("AIC::DoCreate");

#if defined(AUDIO_POWER_MONITORING)
  // We only do power measurements for UMA stats for low latency streams, and
  // only if agc is requested, to avoid adding logs and UMA for non-WebRTC
  // clients.
  power_measurement_is_enabled_ = (type_ == LOW_LATENCY && enable_agc);
  last_audio_level_log_time_ = base::TimeTicks::Now();
#endif

  // Unretained is safe since |this| owns |stream|.
  auto* stream = audio_manager->MakeAudioInputStream(
      params, device_id,
      base::BindRepeating(&InputController::LogMessage,
                          base::Unretained(this)));

  if (!stream) {
    LogCaptureStartupResult(CAPTURE_STARTUP_CREATE_STREAM_FAILED);
    handler_->OnError(STREAM_CREATE_ERROR);
    return;
  }

  if (!stream->Open()) {
    stream->Close();
    LogCaptureStartupResult(CAPTURE_STARTUP_OPEN_STREAM_FAILED);
    handler_->OnError(STREAM_OPEN_ERROR);
    return;
  }

#if defined(AUDIO_POWER_MONITORING)
  bool agc_is_supported = stream->SetAutomaticGainControl(enable_agc);
  // Disable power measurements on platforms that does not support AGC at a
  // lower level. AGC can fail on platforms where we don't support the
  // functionality to modify the input volume slider. One such example is
  // Windows XP.
  power_measurement_is_enabled_ &= agc_is_supported;
#else
  stream->SetAutomaticGainControl(enable_agc);
#endif

  // Finally, keep the stream pointer around, update the state and notify.
  stream_ = stream;

  // Send initial muted state along with OnCreated, to avoid races.
  is_muted_ = stream_->IsMuted();
  handler_->OnCreated(is_muted_);

  check_muted_state_timer_.Start(FROM_HERE, kCheckMutedStateInterval, this,
                                 &InputController::CheckMutedState);
  DCHECK(check_muted_state_timer_.IsRunning());
}

void InputController::DoReportError() {
  DCHECK_CALLED_ON_VALID_THREAD(owning_thread_);
  handler_->OnError(STREAM_ERROR);
}

void InputController::DoLogAudioLevels(float level_dbfs,
                                       int microphone_volume_percent) {
#if defined(AUDIO_POWER_MONITORING)
  DCHECK_CALLED_ON_VALID_THREAD(owning_thread_);
  if (!stream_)
    return;

  // Detect if the user has enabled hardware mute by pressing the mute
  // button in audio settings for the selected microphone.
  const bool microphone_is_muted = stream_->IsMuted();
  if (microphone_is_muted) {
    LogMicrophoneMuteResult(MICROPHONE_IS_MUTED);
    handler_->OnLog("AIC::OnData: microphone is muted!");
    // Return early if microphone is muted. No need to adding logs and UMA stats
    // of audio levels if we know that the micropone is muted.
    return;
  }

  LogMicrophoneMuteResult(MICROPHONE_IS_NOT_MUTED);

  std::string log_string = base::StringPrintf(
      "AIC::OnData: average audio level=%.2f dBFS", level_dbfs);
  static const float kSilenceThresholdDBFS = -72.24719896f;
  if (level_dbfs < kSilenceThresholdDBFS)
    log_string += " <=> low audio input level!";
  handler_->OnLog(log_string);

  UpdateSilenceState(level_dbfs < kSilenceThresholdDBFS);

  log_string = base::StringPrintf("AIC::OnData: microphone volume=%d%%",
                                  microphone_volume_percent);
  if (microphone_volume_percent < kLowLevelMicrophoneLevelPercent)
    log_string += " <=> low microphone level!";
  handler_->OnLog(log_string);
#endif
}

#if defined(AUDIO_POWER_MONITORING)
void InputController::UpdateSilenceState(bool silence) {
  if (silence) {
    if (silence_state_ == SILENCE_STATE_NO_MEASUREMENT) {
      silence_state_ = SILENCE_STATE_ONLY_SILENCE;
    } else if (silence_state_ == SILENCE_STATE_ONLY_AUDIO) {
      silence_state_ = SILENCE_STATE_AUDIO_AND_SILENCE;
    } else {
      DCHECK(silence_state_ == SILENCE_STATE_ONLY_SILENCE ||
             silence_state_ == SILENCE_STATE_AUDIO_AND_SILENCE);
    }
  } else {
    if (silence_state_ == SILENCE_STATE_NO_MEASUREMENT) {
      silence_state_ = SILENCE_STATE_ONLY_AUDIO;
    } else if (silence_state_ == SILENCE_STATE_ONLY_SILENCE) {
      silence_state_ = SILENCE_STATE_AUDIO_AND_SILENCE;
    } else {
      DCHECK(silence_state_ == SILENCE_STATE_ONLY_AUDIO ||
             silence_state_ == SILENCE_STATE_AUDIO_AND_SILENCE);
    }
  }
}

void InputController::LogSilenceState(SilenceState value) {
  UMA_HISTOGRAM_ENUMERATION("Media.AudioInputControllerSessionSilenceReport",
                            value, SILENCE_STATE_MAX + 1);
}
#endif

void InputController::LogCaptureStartupResult(CaptureStartupResult result) {
  switch (type_) {
    case LOW_LATENCY:
      UMA_HISTOGRAM_ENUMERATION("Media.LowLatencyAudioCaptureStartupSuccess",
                                result, CAPTURE_STARTUP_RESULT_MAX + 1);
      break;
    case HIGH_LATENCY:
      UMA_HISTOGRAM_ENUMERATION("Media.HighLatencyAudioCaptureStartupSuccess",
                                result, CAPTURE_STARTUP_RESULT_MAX + 1);
      break;
    case VIRTUAL:
      UMA_HISTOGRAM_ENUMERATION("Media.VirtualAudioCaptureStartupSuccess",
                                result, CAPTURE_STARTUP_RESULT_MAX + 1);
      break;
    default:
      break;
  }
}

void InputController::LogCallbackError() {
  bool error_during_callback = audio_callback_->error_during_callback();
  switch (type_) {
    case LOW_LATENCY:
      UMA_HISTOGRAM_BOOLEAN("Media.Audio.Capture.LowLatencyCallbackError",
                            error_during_callback);
      break;
    case HIGH_LATENCY:
      UMA_HISTOGRAM_BOOLEAN("Media.Audio.Capture.HighLatencyCallbackError",
                            error_during_callback);
      break;
    case VIRTUAL:
      UMA_HISTOGRAM_BOOLEAN("Media.Audio.Capture.VirtualCallbackError",
                            error_during_callback);
      break;
    default:
      break;
  }
}

void InputController::LogMessage(const std::string& message) {
  DCHECK_CALLED_ON_VALID_THREAD(owning_thread_);
  handler_->OnLog(message);
}

bool InputController::CheckForKeyboardInput() {
  if (!user_input_monitor_)
    return false;

  const size_t current_count = user_input_monitor_->GetKeyPressCount();
  const bool key_pressed = current_count != prev_key_down_count_;
  prev_key_down_count_ = current_count;
  DVLOG_IF(6, key_pressed) << "Detected keypress.";

  return key_pressed;
}

bool InputController::CheckAudioPower(const media::AudioBus* source,
                                      double volume,
                                      float* average_power_dbfs,
                                      int* mic_volume_percent) {
#if defined(AUDIO_POWER_MONITORING)
  // Only do power-level measurements if DoCreate() has been called. It will
  // ensure that logging will mainly be done for WebRTC and WebSpeech
  // clients.
  if (!power_measurement_is_enabled_)
    return false;

  // Perform periodic audio (power) level measurements.
  const auto now = base::TimeTicks::Now();
  if (now - last_audio_level_log_time_ <= kPowerMonitorLogInterval) {
    return false;
  }

  *average_power_dbfs = AveragePower(*source);
  *mic_volume_percent = static_cast<int>(100.0 * volume);

  last_audio_level_log_time_ = now;

  return true;
#else
  return false;
#endif
}

void InputController::CheckMutedState() {
  DCHECK_CALLED_ON_VALID_THREAD(owning_thread_);
  DCHECK(stream_);
  const bool new_state = stream_->IsMuted();
  if (new_state != is_muted_) {
    is_muted_ = new_state;
    handler_->OnMuted(is_muted_);
  }
}

#if defined(AUDIO_PROCESSING_IN_AUDIO_SERVICE)
void InputController::UpdateVolumeAndAPMStats(
    base::Optional<double> new_volume) {
  DCHECK_CALLED_ON_VALID_THREAD(owning_thread_);
  processing_helper_->GetAudioProcessor()->UpdateInternalStats();
  if (new_volume)
    SetVolume(*new_volume);
}
#endif

// static
InputController::StreamType InputController::ParamsToStreamType(
    const media::AudioParameters& params) {
  switch (params.format()) {
    case media::AudioParameters::Format::AUDIO_PCM_LINEAR:
      return InputController::StreamType::HIGH_LATENCY;
    case media::AudioParameters::Format::AUDIO_PCM_LOW_LATENCY:
      return InputController::StreamType::LOW_LATENCY;
    default:
      // Currently, the remaining supported type is fake. Reconsider if other
      // formats become supported.
      return InputController::StreamType::FAKE;
  }
}

}  // namespace audio
