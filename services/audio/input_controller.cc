// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/audio/input_controller.h"

#include <inttypes.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_manager.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_processing.h"
#include "media/base/media_switches.h"
#include "media/base/user_input_monitor.h"
#include "services/audio/audio_manager_power_user.h"
#include "services/audio/device_output_listener.h"
#include "services/audio/output_tapper.h"
#include "services/audio/processing_audio_fifo.h"
#include "services/audio/reference_output.h"

#if BUILDFLAG(CHROME_WIDE_ECHO_CANCELLATION)
#include "services/audio/audio_processor_handler.h"
#endif

namespace audio {
namespace {

using OpenOutcome = media::AudioInputStream::OpenOutcome;

const int kMaxInputChannels = 3;
constexpr base::TimeDelta kCheckMutedStateInterval = base::Seconds(1);

#if defined(AUDIO_POWER_MONITORING)
// Time in seconds between two successive measurements of audio power levels.
constexpr base::TimeDelta kPowerMonitorLogInterval = base::Seconds(15);

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

const char* SilenceStateToString(InputController::SilenceState state) {
  switch (state) {
    case InputController::SILENCE_STATE_NO_MEASUREMENT:
      return "SILENCE_STATE_NO_MEASUREMENT";
    case InputController::SILENCE_STATE_ONLY_AUDIO:
      return "SILENCE_STATE_ONLY_AUDIO";
    case InputController::SILENCE_STATE_ONLY_SILENCE:
      return "SILENCE_STATE_ONLY_SILENCE";
    case InputController::SILENCE_STATE_AUDIO_AND_SILENCE:
      return "SILENCE_STATE_AUDIO_AND_SILENCE";
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return "INVALID";
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
      std::clamp(sum_power / (frames * channels), 0.0f, 1.0f);

  // Convert average power level to dBFS units, and pin it down to zero if it
  // is insignificantly small.
  const float kInsignificantPower = 1.0e-10f;  // -100 dBFS
  const float power_dbfs = average_power < kInsignificantPower
                               ? -std::numeric_limits<float>::infinity()
                               : 10.0f * log10f(average_power);

  return power_dbfs;
}
#endif  // AUDIO_POWER_MONITORING

}  // namespace

// This class implements the AudioInputCallback interface in place of the
// InputController (AIC), so that
// - The AIC itself does not publicly inherit AudioInputCallback.
// - The lifetime of the AudioCallback (shorter than the AIC) matches the
//   interval during which hardware callbacks come.
// - The callback class can gather information on what happened during capture
//   and store it in a state that can be fetched after stopping capture
//   (received_callback(), error_during_callback()).
class AudioCallback : public media::AudioInputStream::AudioInputCallback {
 public:
  using OnDataCallback =
      base::RepeatingCallback<void(const media::AudioBus*,
                                   base::TimeTicks,
                                   double volume,
                                   const media::AudioGlitchInfo& glitch_info)>;
  using OnFirstDataCallback = base::OnceCallback<void()>;
  using OnErrorCallback = base::RepeatingCallback<void()>;

  // All callbacks are called on the hw callback thread.
  AudioCallback(OnDataCallback on_data_callback,
                OnFirstDataCallback on_first_data_callback,
                OnErrorCallback on_error_callback)
      : on_data_callback_(std::move(on_data_callback)),
        on_first_data_callback_(std::move(on_first_data_callback)),
        on_error_callback_(std::move(on_error_callback)) {
    DCHECK(on_data_callback_);
    DCHECK(on_first_data_callback_);
    DCHECK(on_error_callback_);
  }
  ~AudioCallback() override = default;

  // These should not be called when the stream is live.
  bool received_callback() const { return !on_first_data_callback_; }
  bool error_during_callback() const { return error_during_callback_; }

 private:
  void OnData(const media::AudioBus* source,
              base::TimeTicks capture_time,
              double volume,
              const media::AudioGlitchInfo& glitch_info) override {
    if (on_first_data_callback_) {
      // Mark the stream as alive at first audio callback. Currently only used
      // for logging purposes.
      std::move(on_first_data_callback_).Run();
    }
    on_data_callback_.Run(source, capture_time, volume, glitch_info);
  }

  void OnError() override {
    error_during_callback_ = true;
    on_error_callback_.Run();
  }

  const OnDataCallback on_data_callback_;
  OnFirstDataCallback on_first_data_callback_;
  const OnErrorCallback on_error_callback_;
  bool error_during_callback_ = false;
};

InputController::InputController(
    EventHandler* event_handler,
    SyncWriter* sync_writer,
    media::UserInputMonitor* user_input_monitor,
    DeviceOutputListener* device_output_listener,
    media::AecdumpRecordingManager* aecdump_recording_manager,
    media::mojom::AudioProcessingConfigPtr processing_config,
    const media::AudioParameters& output_params,
    const media::AudioParameters& device_params,
    StreamType type)
    : task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      event_handler_(event_handler),
      stream_(nullptr),
      sync_writer_(sync_writer),
      type_(type),
      user_input_monitor_(user_input_monitor) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(event_handler_);
  DCHECK(sync_writer_);
  weak_this_ = weak_ptr_factory_.GetWeakPtr();

#if BUILDFLAG(CHROME_WIDE_ECHO_CANCELLATION)
  MaybeSetUpAudioProcessing(std::move(processing_config), output_params,
                            device_params, device_output_listener,
                            aecdump_recording_manager);
#endif

  if (!user_input_monitor_) {
    event_handler_->OnLog(
        "AIC::InputController() => (WARNING: keypress monitoring is disabled)");
  }
}

#if BUILDFLAG(CHROME_WIDE_ECHO_CANCELLATION)
void InputController::MaybeSetUpAudioProcessing(
    media::mojom::AudioProcessingConfigPtr processing_config,
    const media::AudioParameters& processing_output_params,
    const media::AudioParameters& device_params,
    DeviceOutputListener* device_output_listener,
    media::AecdumpRecordingManager* aecdump_recording_manager) {
  if (!device_output_listener)
    return;

  if (!(processing_config &&
        processing_config->settings.NeedAudioModification())) {
    return;
  }

  std::optional<media::AudioParameters> processing_input_params =
      media::AudioProcessor::ComputeInputFormat(device_params,
                                                processing_config->settings);
  if (!processing_input_params) {
    event_handler_->OnLog(base::StringPrintf(
        "AIC::MaybeSetupAudioProcessing() => (Unsupported device_params=%s, "
        "cannot do audio processing)",
        device_params.AsHumanReadableString().c_str()));
    return;
  }

  // In case fake audio input is requested.
  processing_input_params->set_format(processing_output_params.format());

  // Unretained() is safe, since |this| and |event_handler_| outlive
  // |audio_processor_handler_|.
  audio_processor_handler_ = std::make_unique<AudioProcessorHandler>(
      processing_config->settings, *processing_input_params,
      processing_output_params,
      base::BindRepeating(&EventHandler::OnLog,
                          base::Unretained(event_handler_)),
      base::BindRepeating(&InputController::DeliverProcessedAudio,
                          base::Unretained(this)),
      std::move(processing_config->controls_receiver),
      aecdump_recording_manager);

  // If the required processing is lightweight, there is no need to offload work
  // to a new thread.
  if (!audio_processor_handler_->needs_playout_reference()) {
    return;
  }

  int fifo_size = media::GetProcessingAudioFifoSize();

  // Only use the FIFO/new thread if its size is explicitly set.
  if (fifo_size) {
    // base::Unretained() is safe since both |audio_processor_handler_| and
    // |event_handler_| outlive |processing_fifo_|.
    processing_fifo_ = std::make_unique<ProcessingAudioFifo>(
        *processing_input_params, fifo_size,
        base::BindRepeating(&AudioProcessorHandler::ProcessCapturedAudio,
                            base::Unretained(audio_processor_handler_.get())),
        base::BindRepeating(&EventHandler::OnLog,
                            base::Unretained(event_handler_.get())));
  }

  // Unretained() is safe, since |event_handler_| outlives |output_tapper_|.
  output_tapper_ = std::make_unique<OutputTapper>(
      device_output_listener, audio_processor_handler_.get(),
      base::BindRepeating(&EventHandler::OnLog,
                          base::Unretained(event_handler_)));
}
#endif

InputController::~InputController() {
  DCHECK(task_runner_->BelongsToCurrentThread());
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
    DeviceOutputListener* device_output_listener,
    media::AecdumpRecordingManager* aecdump_recording_manager,
    media::mojom::AudioProcessingConfigPtr processing_config,
    const media::AudioParameters& params,
    const std::string& device_id,
    bool enable_agc) {
  DCHECK(audio_manager);
  DCHECK(audio_manager->GetTaskRunner()->BelongsToCurrentThread());
  DCHECK(sync_writer);
  DCHECK(event_handler);
  DCHECK(params.IsValid());

  if (params.channels() > kMaxInputChannels)
    return nullptr;

  const media::AudioParameters device_params =
      AudioManagerPowerUser(audio_manager).GetInputStreamParameters(device_id);

  // Create the InputController object and ensure that it runs on
  // the audio-manager thread.
  // Using `new` to access a non-public constructor.
  std::unique_ptr<InputController> controller = base::WrapUnique(
      new InputController(event_handler, sync_writer, user_input_monitor,
                          device_output_listener, aecdump_recording_manager,
                          std::move(processing_config), params, device_params,
                          ParamsToStreamType(params)));

  controller->DoCreate(audio_manager, params, device_id, enable_agc);
  return controller;
}

void InputController::Record() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  SCOPED_UMA_HISTOGRAM_TIMER("Media.AudioInputController.RecordTime");

  if (!stream_ || audio_callback_)
    return;

  event_handler_->OnLog("AIC::Record()");

  if (user_input_monitor_) {
    user_input_monitor_->EnableKeyPressMonitoring();
    prev_key_down_count_ = user_input_monitor_->GetKeyPressCount();
  }

  stream_create_time_ = base::TimeTicks::Now();

  // Unretained() is safe, since |this| outlives |audio_callback_|.
  // |on_first_data_callback| and |on_error_callback| calls are posted on the
  // audio thread, since all AudioCallback callbacks run on the hw callback
  // thread.
  audio_callback_ = std::make_unique<AudioCallback>(
      /*on_data_callback=*/base::BindRepeating(&InputController::OnData,
                                               base::Unretained(this)),
      /*on_first_data_callback=*/
      base::BindPostTask(
          task_runner_,
          base::BindOnce(&InputController::ReportIsAlive, weak_this_)),
      /*on_error_callback=*/
      base::BindPostTask(
          task_runner_,
          base::BindRepeating(&InputController::DoReportError, weak_this_)));

#if BUILDFLAG(CHROME_WIDE_ECHO_CANCELLATION)
  if (processing_fifo_)
    processing_fifo_->Start();

  if (output_tapper_)
    output_tapper_->Start();
#endif

  stream_->Start(audio_callback_.get());
  return;
}

void InputController::Close() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  SCOPED_UMA_HISTOGRAM_TIMER("Media.AudioInputController.CloseTime");

  if (!stream_)
    return;

  check_muted_state_timer_.AbandonAndStop();

  std::string log_string;
  static const char kLogStringPrefix[] = "AIC::Close => ";

  // Allow calling unconditionally and bail if we don't have a stream to close.
  if (audio_callback_) {
    // Calls to OnData() should stop beyond this point.
    stream_->Stop();

#if BUILDFLAG(CHROME_WIDE_ECHO_CANCELLATION)
    if (output_tapper_)
      output_tapper_->Stop();

    if (processing_fifo_) {
      // Stop the FIFO after |stream_| is stopped, to guarantee there are no
      // more calls to OnData().
      // Note: destroying the FIFO will synchronously wait for the processing
      // thread to stop.
      processing_fifo_.reset();
    }
#endif

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

    log_string = base::StringPrintf("%s(stream duration=%" PRId64 " seconds%s",
                                    kLogStringPrefix, duration.InSeconds(),
                                    audio_callback_->received_callback()
                                        ? ")"
                                        : " - no callbacks received)");

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
    log_string = base::StringPrintf("%s(WARNING: recording never started)",
                                    kLogStringPrefix);
  }

  event_handler_->OnLog(log_string);

  stream_->Close();
  stream_ = nullptr;

  sync_writer_->Close();

#if defined(AUDIO_POWER_MONITORING)
  // Send stats if enabled.
  if (power_measurement_is_enabled_) {
    log_string = base::StringPrintf("%s(silence_state=%s)", kLogStringPrefix,
                                    SilenceStateToString(silence_state_));
    event_handler_->OnLog(log_string);
  }
#endif

  max_volume_ = 0.0;
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void InputController::SetVolume(double volume) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_GE(volume, 0);
  DCHECK_LE(volume, 1.0);

  if (!stream_)
    return;

  event_handler_->OnLog(
      base::StringPrintf("AIC::SetVolume({volume=%.2f})", volume));

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
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (stream_)
    stream_->SetOutputDeviceForAec(output_device_id);

#if BUILDFLAG(CHROME_WIDE_ECHO_CANCELLATION)
  if (output_tapper_)
    output_tapper_->SetOutputDeviceForAec(output_device_id);
#endif
}

void InputController::OnStreamActive(Snoopable* output_stream) {
  DCHECK(task_runner_->BelongsToCurrentThread());
}

void InputController::OnStreamInactive(Snoopable* output_stream) {
  DCHECK(task_runner_->BelongsToCurrentThread());
}

InputController::ErrorCode MapOpenOutcomeToErrorCode(OpenOutcome outcome) {
  switch (outcome) {
    case OpenOutcome::kFailedSystemPermissions:
      return InputController::STREAM_OPEN_SYSTEM_PERMISSIONS_ERROR;
    case OpenOutcome::kFailedInUse:
      return InputController::STREAM_OPEN_DEVICE_IN_USE_ERROR;
    default:
      return InputController::STREAM_OPEN_ERROR;
  }
}

void InputController::DoCreate(media::AudioManager* audio_manager,
                               const media::AudioParameters& params,
                               const std::string& device_id,
                               bool enable_agc) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!stream_);
  SCOPED_UMA_HISTOGRAM_TIMER("Media.AudioInputController.CreateTime");
  event_handler_->OnLog("AIC::DoCreate({device_id=" + device_id + "})");

#if defined(AUDIO_POWER_MONITORING)
  // We only do power measurements for UMA stats for low latency streams, and
  // only if agc is requested, to avoid adding logs and UMA for non-WebRTC
  // clients.
  power_measurement_is_enabled_ = (type_ == LOW_LATENCY && enable_agc);
  last_audio_level_log_time_ = base::TimeTicks::Now();
#endif

  const media::AudioParameters audio_input_stream_params =
#if BUILDFLAG(CHROME_WIDE_ECHO_CANCELLATION)
      audio_processor_handler_ ? audio_processor_handler_->input_format() :
#endif
                               params;

  // Unretained is safe since |this| owns |stream|.
  auto* stream = audio_manager->MakeAudioInputStream(
      audio_input_stream_params, device_id,
      base::BindRepeating(&InputController::LogMessage,
                          base::Unretained(this)));

  if (!stream) {
    LogCaptureStartupResult(CAPTURE_STARTUP_CREATE_STREAM_FAILED);
    event_handler_->OnError(STREAM_CREATE_ERROR);
    return;
  }

  auto open_outcome = stream->Open();
  if (open_outcome != OpenOutcome::kSuccess) {
    stream->Close();
    LogCaptureStartupResult(CAPTURE_STARTUP_OPEN_STREAM_FAILED);
    event_handler_->OnError(MapOpenOutcomeToErrorCode(open_outcome));
    return;
  }

#if defined(AUDIO_POWER_MONITORING)
  bool agc_is_supported = stream->SetAutomaticGainControl(enable_agc);
  // Disable power measurements on platforms that does not support AGC at a
  // lower level. AGC can fail on platforms where we don't support the
  // functionality to modify the input volume slider. One such example is
  // Windows XP.
  power_measurement_is_enabled_ &= agc_is_supported;
  event_handler_->OnLog(
      base::StringPrintf("AIC::DoCreate => (power_measurement_is_enabled=%d)",
                         power_measurement_is_enabled_));
#else
  stream->SetAutomaticGainControl(enable_agc);
#endif

  // Finally, keep the stream pointer around, update the state and notify.
  stream_ = stream;

  // Send initial muted state along with OnCreated, to avoid races.
  is_muted_ = stream_->IsMuted();
  event_handler_->OnCreated(is_muted_);
  check_muted_state_timer_.Start(FROM_HERE, kCheckMutedStateInterval, this,
                                 &InputController::CheckMutedState);
  DCHECK(check_muted_state_timer_.IsRunning());
}

void InputController::DoReportError() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  event_handler_->OnError(STREAM_ERROR);
}

void InputController::DoLogAudioLevels(float level_dbfs,
                                       int microphone_volume_percent) {
#if defined(AUDIO_POWER_MONITORING)
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (!stream_)
    return;

  // Detect if the user has enabled hardware mute by pressing the mute
  // button in audio settings for the selected microphone.
  const bool microphone_is_muted = stream_->IsMuted();
  if (microphone_is_muted) {
    LogMicrophoneMuteResult(MICROPHONE_IS_MUTED);
    event_handler_->OnLog("AIC::OnData => (microphone is muted)");
  } else {
    LogMicrophoneMuteResult(MICROPHONE_IS_NOT_MUTED);
  }

  std::string log_string = base::StringPrintf(
      "AIC::OnData => (average audio level=%.2f dBFS", level_dbfs);
  static const float kSilenceThresholdDBFS = -72.24719896f;
  if (level_dbfs < kSilenceThresholdDBFS)
    log_string += " <=> low audio input level";
  event_handler_->OnLog(log_string + ")");

  if (!microphone_is_muted) {
    UpdateSilenceState(level_dbfs < kSilenceThresholdDBFS);
  }

  log_string = base::StringPrintf("AIC::OnData => (microphone volume=%d%%",
                                  microphone_volume_percent);
  if (microphone_volume_percent < kLowLevelMicrophoneLevelPercent)
    log_string += " <=> low microphone level";
  event_handler_->OnLog(log_string + ")");
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

#endif

void InputController::LogCaptureStartupResult(CaptureStartupResult result) {
  if (type_ != LOW_LATENCY)
    return;
  UMA_HISTOGRAM_ENUMERATION("Media.LowLatencyAudioCaptureStartupSuccess",
                            result, CAPTURE_STARTUP_RESULT_MAX + 1);
}

void InputController::LogCallbackError() {
  if (type_ != LOW_LATENCY)
    return;

  UMA_HISTOGRAM_BOOLEAN("Media.Audio.Capture.LowLatencyCallbackError",
                        audio_callback_->error_during_callback());
}

void InputController::LogMessage(const std::string& message) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  event_handler_->OnLog(message);
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
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(stream_);
  const bool new_state = stream_->IsMuted();
  if (new_state != is_muted_) {
    is_muted_ = new_state;
    event_handler_->OnMuted(is_muted_);
    std::string log_string =
        base::StringPrintf("AIC::OnMuted({is_muted=%d})", is_muted_);
    event_handler_->OnLog(log_string);
  }
}

void InputController::ReportIsAlive() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(stream_);
  // Don't store any state, just log the event for now.
  event_handler_->OnLog("AIC::OnData => (stream is alive)");
}

void InputController::OnData(const media::AudioBus* source,
                             base::TimeTicks capture_time,
                             double volume,
                             const media::AudioGlitchInfo& glitch_info) {
  TRACE_EVENT("audio", "InputController::OnData", "this",
              static_cast<void*>(this), "timestamp (ms)",
              (capture_time - base::TimeTicks()).InMillisecondsF(),
              "capture_delay (ms)",
              (base::TimeTicks::Now() - capture_time).InMillisecondsF());
  const bool key_pressed = CheckForKeyboardInput();
#if BUILDFLAG(CHROME_WIDE_ECHO_CANCELLATION)
  if (processing_fifo_) {
    DCHECK(audio_processor_handler_);
    processing_fifo_->PushData(source, capture_time, volume, key_pressed,
                               glitch_info);
  } else if (audio_processor_handler_) {
    audio_processor_handler_->ProcessCapturedAudio(
        *source, capture_time, volume, key_pressed, glitch_info);
  } else
#endif
  {
    sync_writer_->Write(source, volume, key_pressed, capture_time, glitch_info);
  }

  float average_power_dbfs;
  int mic_volume_percent;
  if (CheckAudioPower(source, volume, &average_power_dbfs,
                      &mic_volume_percent)) {
    // Use event handler on the audio thread to relay a message to the ARIH
    // in content which does the actual logging on the IO thread.
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&InputController::DoLogAudioLevels, weak_this_,
                       average_power_dbfs, mic_volume_percent));
  }
}

#if BUILDFLAG(CHROME_WIDE_ECHO_CANCELLATION)
void InputController::DeliverProcessedAudio(
    const media::AudioBus& audio_bus,
    base::TimeTicks audio_capture_time,
    std::optional<double> new_volume,
    const media::AudioGlitchInfo& glitch_info) {
  // When processing is performed in the audio service, the consumer is not
  // expected to use the input volume and keypress information.
  sync_writer_->Write(&audio_bus, /*volume=*/1.0,
                      /*key_pressed=*/false, audio_capture_time, glitch_info);
  if (new_volume) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&InputController::SetVolume, weak_this_, *new_volume));
  }
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
