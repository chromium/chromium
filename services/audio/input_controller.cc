// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/input_controller.h"

#include <inttypes.h>

#include <algorithm>
#include <cstdarg>
#include <limits>
#include <memory>
#include <numeric>
#include <utility>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
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
#include "services/audio/audio_manager_power_user.h"
#include "services/audio/output_tapper.h"
#include "services/audio/processing_audio_fifo.h"
#include "services/audio/reference_output.h"
#include "services/audio/reference_signal_provider.h"

#if BUILDFLAG(CHROME_WIDE_ECHO_CANCELLATION)
#include "services/audio/audio_processor_handler.h"
#endif

namespace audio {
namespace {

using OpenOutcome = media::AudioInputStream::OpenOutcome;

const int kMaxInputChannels = 3;
constexpr base::TimeDelta kCheckMutedStateInterval = base::Seconds(1);

#if BUILDFLAG(CHROME_WIDE_ECHO_CANCELLATION)
using ReferenceOpenOutcome = ReferenceSignalProvider::ReferenceOpenOutcome;

InputController::ErrorCode MapReferenceOpenOutcomeToInputErrorCode(
    ReferenceOpenOutcome open_outcome) {
  CHECK(open_outcome != ReferenceOpenOutcome::SUCCESS);
  switch (open_outcome) {
    case ReferenceOpenOutcome::STREAM_CREATE_ERROR:
      return InputController::REFERENCE_STREAM_CREATE_ERROR;
    case ReferenceOpenOutcome::STREAM_OPEN_ERROR:
      return InputController::REFERENCE_STREAM_OPEN_ERROR;
    case ReferenceOpenOutcome::STREAM_OPEN_SYSTEM_PERMISSIONS_ERROR:
      return InputController::REFERENCE_STREAM_OPEN_SYSTEM_PERMISSIONS_ERROR;
    case ReferenceOpenOutcome::STREAM_OPEN_DEVICE_IN_USE_ERROR:
      return InputController::REFERENCE_STREAM_OPEN_DEVICE_IN_USE_ERROR;
    case ReferenceOpenOutcome::STREAM_PREVIOUS_ERROR:
      return InputController::REFERENCE_STREAM_ERROR;
    default:
      NOTREACHED();
  }
}
#endif

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
      NOTREACHED();
  }
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
  for (auto channel : buffer.AllChannels()) {
    sum_power += std::inner_product(channel.begin(), channel.end(),
                                    channel.begin(), 0.0f);
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

constexpr base::TimeDelta kMinDelay = base::Milliseconds(1);
constexpr base::TimeDelta kMaxDelay = base::Milliseconds(1000);
constexpr int kBucketCount = 50;

void LogNoAudioServiceAECDelay(base::TimeDelta delay) {
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "Media.Audio.InputController.Delay.NoAudioServiceAEC", delay, kMinDelay,
      kMaxDelay, kBucketCount);
}

void LogChromeWideAECDelay(base::TimeDelta delay) {
  UMA_HISTOGRAM_CUSTOM_TIMES("Media.Audio.InputController.Delay.ChromeWideAEC",
                             delay, kMinDelay, kMaxDelay, kBucketCount);
}

void LogLoopbackAECDelay(base::TimeDelta delay) {
  UMA_HISTOGRAM_CUSTOM_TIMES("Media.Audio.InputController.Delay.LoopbackAEC",
                             delay, kMinDelay, kMaxDelay, kBucketCount);
}

}  // namespace

// A helper class to report capture delay UMA stats from the InputController.
class InputController::DelayReporter {
 public:
  enum class AECType {
    kNoAudioServiceAEC,
    kChromeWideAEC,
    kLoopbackAEC,
  };

  using OnReportCallback = base::RepeatingCallback<void(base::TimeDelta)>;

  explicit DelayReporter(
      const ReferenceSignalProvider* reference_signal_provider)
      : aec_type_(GetAecTypeFromReferenceSignal(reference_signal_provider)),
        report_cb_(GetOnReportCallback(aec_type_)) {}

  DelayReporter(const DelayReporter&) = delete;
  DelayReporter& operator=(const DelayReporter&) = delete;

  // Calculates and records the capture delay to a UMA histogram based on the
  // active AEC type.
  void ReportDelay(base::TimeTicks audio_capture_time) {
    report_cb_.Run(base::TimeTicks::Now() - audio_capture_time);
  }

  AECType GetAecType() const { return aec_type_; }

  const char* GetAECTypeAsString() const {
    switch (aec_type_) {
      case AECType::kNoAudioServiceAEC:
        return "NoAudioServiceAEC";
      case AECType::kChromeWideAEC:
        return "ChromeWideAEC";
      case AECType::kLoopbackAEC:
        return "LoopbackAEC";
    }
    NOTREACHED();
  }

 private:
  // Determine the AEC type which is used to select callback method.
  static AECType GetAecTypeFromReferenceSignal(
      const ReferenceSignalProvider* reference_signal_provider) {
    if (!reference_signal_provider) {
      return AECType::kNoAudioServiceAEC;
    }
    // Map kOutputDeviceMixer -> kChromeWideAEC, kLoopbackReference ->
    // kLoopbackAEC.
    if (reference_signal_provider->GetType() ==
        ReferenceSignalProvider::Type::kOutputDeviceMixer) {
      return AECType::kChromeWideAEC;
    }
    return AECType::kLoopbackAEC;
  }

  // Determine which callback to use when reporting the delay UMA.
  static OnReportCallback GetOnReportCallback(AECType aec_type) {
    switch (aec_type) {
      case AECType::kNoAudioServiceAEC:
        return base::BindRepeating(&LogNoAudioServiceAECDelay);
      case AECType::kChromeWideAEC:
        return base::BindRepeating(&LogChromeWideAECDelay);
      case AECType::kLoopbackAEC:
        return base::BindRepeating(&LogLoopbackAECDelay);
    }
  }

  const AECType aec_type_;
  const OnReportCallback report_cb_;
};

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
    std::unique_ptr<ReferenceSignalProvider> reference_signal_provider,
    media::AecdumpRecordingManager* aecdump_recording_manager,
    raw_ptr<MlModelManager> ml_model_manager,
    media::mojom::AudioProcessingConfigPtr processing_config,
    const media::AudioParameters& output_params,
    const media::AudioParameters& device_params,
    StreamType type)
    : task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      event_handler_(event_handler),
      stream_(nullptr),
      sync_writer_(sync_writer),
      type_(type),
      delay_reporter_(
          std::make_unique<DelayReporter>(reference_signal_provider.get())) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(event_handler_);
  DCHECK(sync_writer_);
  weak_this_ = weak_ptr_factory_.GetWeakPtr();
  SendLogMessage("%s => (delay reporter uses %s as AEC type)", __func__,
                 delay_reporter_->GetAECTypeAsString());

#if BUILDFLAG(CHROME_WIDE_ECHO_CANCELLATION)
  MaybeSetUpAudioProcessing(std::move(processing_config), output_params,
                            device_params, std::move(reference_signal_provider),
                            aecdump_recording_manager, ml_model_manager);
#endif
}

#if BUILDFLAG(CHROME_WIDE_ECHO_CANCELLATION)
void InputController::MaybeSetUpAudioProcessing(
    media::mojom::AudioProcessingConfigPtr processing_config,
    const media::AudioParameters& processing_output_params,
    const media::AudioParameters& device_params,
    std::unique_ptr<ReferenceSignalProvider> reference_signal_provider,
    media::AecdumpRecordingManager* aecdump_recording_manager,
    raw_ptr<MlModelManager> ml_model_manager) {
  SendLogMessage(
      "%s({processing_config=[%s]}, {processing_output_params=[%s]}, "
      "{device_params=[%s]})",
      __func__,
      processing_config ? processing_config->settings.ToString().c_str()
                        : "nullptr",
      processing_output_params.AsHumanReadableString().c_str(),
      device_params.AsHumanReadableString().c_str());
  if (!processing_config) {
    SendLogMessage("%s => (WARNING: undefined audio processing config)",
                   __func__);
    return;
  }
  // If audio processing is configured there should always be a
  // ReferenceSignalProvider in case AEC is requested.
  CHECK(reference_signal_provider);
  const bool needs_webrtc_audio_processing =
      processing_config->settings.NeedWebrtcAudioProcessing();
  SendLogMessage("%s => (needs WebRTC audio processing: %s)", __func__,
                 needs_webrtc_audio_processing ? "true" : "false");
  if (!needs_webrtc_audio_processing) {
    return;
  }

  std::optional<media::AudioParameters> processing_input_params =
      media::AudioProcessor::ComputeInputFormat(device_params,
                                                processing_config->settings);
  if (!processing_input_params) {
    SendLogMessage(
        "%s => (WARNING: unsupported device parameters, "
        "cannot do audio processing)",
        __func__);
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
      // AudioProcessorHandler delivers errors on the main thread.
      base::BindRepeating(&InputController::DoReportError, weak_this_,
                          REFERENCE_STREAM_ERROR),
      std::move(processing_config->controls_receiver),
      aecdump_recording_manager, ml_model_manager);

  // If we are not running echo cancellation the processing is lightweight, so
  // there is no need to offload work to a new thread.
  const bool echo_cancellation_is_enabled =
      audio_processor_handler_->needs_playout_reference();
  SendLogMessage("%s => (echo cancellation is: %s)", __func__,
                 (echo_cancellation_is_enabled ? "enabled" : "disabled"));
  if (!echo_cancellation_is_enabled) {
    return;
  }

  // base::Unretained() is safe since both |audio_processor_handler_| and
  // |event_handler_| outlive |processing_fifo_|.
  processing_fifo_ = std::make_unique<ProcessingAudioFifo>(
      *processing_input_params, kProcessingFifoSize,
      base::BindRepeating(&AudioProcessorHandler::ProcessCapturedAudio,
                          base::Unretained(audio_processor_handler_.get())),
      base::BindRepeating(&EventHandler::OnLog,
                          base::Unretained(event_handler_.get())));

  // Unretained() is safe, since |event_handler_| outlives |output_tapper_|.
  output_tapper_ = std::make_unique<OutputTapper>(
      std::move(reference_signal_provider), audio_processor_handler_.get(),
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
    std::unique_ptr<ReferenceSignalProvider> reference_signal_provider,
    media::AecdumpRecordingManager* aecdump_recording_manager,
    raw_ptr<MlModelManager> ml_model_manager,
    media::mojom::AudioProcessingConfigPtr processing_config,
    LoopbackMixin::MaybeCreateCallback maybe_create_loopback_mixin_cb,
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
  std::unique_ptr<InputController> controller =
      base::WrapUnique(new InputController(
          event_handler, sync_writer, std::move(reference_signal_provider),
          aecdump_recording_manager, ml_model_manager,
          std::move(processing_config), params, device_params,
          ParamsToStreamType(params)));

  controller->DoCreate(audio_manager, params, device_id, enable_agc,
                       std::move(maybe_create_loopback_mixin_cb));
  return controller;
}

void InputController::Record() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  SCOPED_UMA_HISTOGRAM_TIMER("Media.AudioInputController.RecordTime");

  if (!stream_ || audio_callback_)
    return;

  SendLogMessage("%s", __func__);

#if BUILDFLAG(CHROME_WIDE_ECHO_CANCELLATION)
  if (output_tapper_) {
    ReferenceOpenOutcome reference_open_outcome = output_tapper_->Start();
    if (reference_open_outcome != ReferenceOpenOutcome::SUCCESS) {
      // The AEC reference stream failed to start.
      DoReportError(
          MapReferenceOpenOutcomeToInputErrorCode(reference_open_outcome));
      return;
    }
  }

  if (processing_fifo_) {
    processing_fifo_->Start();
  }
#endif

  stream_create_time_ = base::TimeTicks::Now();

  // Unretained() is safe, since |this| and |loopback_mixin_| outlive
  // |audio_callback_|.
  AudioCallback::OnDataCallback on_data_callback =
      loopback_mixin_
          ? base::BindRepeating(&LoopbackMixin::OnData,
                                base::Unretained(loopback_mixin_.get()))
          : base::BindRepeating(&InputController::OnData,
                                base::Unretained(this));

  // |on_first_data_callback| and |on_error_callback| calls are posted on the
  // audio thread, since all AudioCallback callbacks run on the hw callback
  // thread.
  audio_callback_ = std::make_unique<AudioCallback>(
      std::move(on_data_callback),
      /*on_first_data_callback=*/
      base::BindPostTask(
          task_runner_,
          base::BindOnce(&InputController::ReportIsAlive, weak_this_)),
      /*on_error_callback=*/
      base::BindPostTask(task_runner_,
                         base::BindRepeating(&InputController::DoReportError,
                                             weak_this_, STREAM_ERROR)));

  if (loopback_mixin_) {
    // Start receiving chromium playout loopback.
    loopback_mixin_->Start();
  }
  stream_->Start(audio_callback_.get());
}

void InputController::Close() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  SCOPED_UMA_HISTOGRAM_TIMER("Media.AudioInputController.CloseTime");

  if (!stream_)
    return;

  check_muted_state_timer_.Stop();

  // Allow calling unconditionally and bail if we don't have a stream to close.
  if (audio_callback_) {
    // Calls to OnData() should stop beyond this point.
    stream_->Stop();

#if BUILDFLAG(CHROME_WIDE_ECHO_CANCELLATION)
    if (output_tapper_) {
      output_tapper_->Stop();
    }

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
    SendLogMessage("%s => (stream duration=%" PRId64 " seconds%s", __func__,
                   duration.InSeconds(),
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

    audio_callback_.reset();
    loopback_mixin_.reset();
  } else {
    SendLogMessage("%s => (WARNING: recording never started)", __func__);
  }

  stream_->Close();
  stream_ = nullptr;

  sync_writer_->Close();

#if defined(AUDIO_POWER_MONITORING)
  // Send stats if enabled.
  if (power_measurement_is_enabled_) {
    SendLogMessage("%s => (silence_state=%s)", __func__,
                   SilenceStateToString(silence_state_));
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

  SendLogMessage("SetVolume({volume=%.2f})", volume);

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

void InputController::DoCreate(
    media::AudioManager* audio_manager,
    const media::AudioParameters& params,
    const std::string& device_id,
    bool enable_agc,
    LoopbackMixin::MaybeCreateCallback maybe_create_loopback_mixin_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!stream_);
  SCOPED_UMA_HISTOGRAM_TIMER("Media.AudioInputController.CreateTime");
  SendLogMessage("%s({device_id=%s})", __func__, device_id.c_str());

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
  SendLogMessage("%s => (power_measurement_is_enabled=%d)", __func__,
                 power_measurement_is_enabled_);
#else
  stream->SetAutomaticGainControl(enable_agc);
#endif

  // Finally, keep the stream pointer around, update the state and notify.
  stream_ = stream;

  loopback_mixin_ = std::move(maybe_create_loopback_mixin_cb)
                        .Run(device_id, audio_input_stream_params,
                             base::BindRepeating(&InputController::OnData,
                                                 base::Unretained(this)));

  // Send initial muted state along with OnCreated, to avoid races.
  is_muted_ = stream_->IsMuted();
  event_handler_->OnCreated(is_muted_);
  check_muted_state_timer_.Start(FROM_HERE, kCheckMutedStateInterval, this,
                                 &InputController::CheckMutedState);
  DCHECK(check_muted_state_timer_.IsRunning());
}

void InputController::DoReportError(ErrorCode error_code) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  event_handler_->OnError(error_code);
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
    SendLogMessage("%s => (microphone is muted)", __func__);
  } else {
    LogMicrophoneMuteResult(MICROPHONE_IS_NOT_MUTED);
  }

  static const float kSilenceThresholdDBFS = -72.24719896f;
  SendLogMessage(
      "%s => (average audio level=%.2f dBFS%s)", __func__, level_dbfs,
      level_dbfs < kSilenceThresholdDBFS ? " <=> low audio input level" : "");

  if (!microphone_is_muted) {
    UpdateSilenceState(level_dbfs < kSilenceThresholdDBFS);
  }
  SendLogMessage("%s => (microphone volume=%d%%%s)", __func__,
                 microphone_volume_percent,
                 microphone_volume_percent < kLowLevelMicrophoneLevelPercent
                     ? " <=> low microphone level"
                     : "");
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

void InputController::SendLogMessage(const char* format, ...) {
  va_list args;
  va_start(args, format);
  event_handler_->OnLog(
      base::StrCat({"AIC::", base::StringPrintV(format, args)}));
  va_end(args);
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
    SendLogMessage("%s => (is_muted=%s)", __func__,
                   is_muted_ ? "true" : "false");
  }
}

void InputController::ReportIsAlive() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(stream_);
  // Don't store any state, just log the event for now.
  SendLogMessage("%s => (stream is alive)", __func__);
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
#if BUILDFLAG(CHROME_WIDE_ECHO_CANCELLATION)
  if (processing_fifo_) {
    DCHECK(audio_processor_handler_);
    processing_fifo_->PushData(source, capture_time, volume, glitch_info);
  } else if (audio_processor_handler_) {
    audio_processor_handler_->ProcessCapturedAudio(*source, capture_time,
                                                   volume, glitch_info);
  } else
#endif
  {
    delay_reporter_->ReportDelay(capture_time);
    sync_writer_->Write(source, volume, capture_time, glitch_info);
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
  delay_reporter_->ReportDelay(audio_capture_time);
  // When processing is performed in the audio service, the consumer is not
  // expected to use the input volume and keypress information.
  sync_writer_->Write(&audio_bus, /*volume=*/1.0, audio_capture_time,
                      glitch_info);
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
