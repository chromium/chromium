// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/voice_isolation_handler.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "base/types/expected.h"
#include "media/audio/audio_debug_recording_helper.h"
#include "media/audio/audio_debug_recording_manager.h"
#include "media/base/audio_bus.h"
#include "media/base/media_switches.h"
#include "media/webrtc/ml_model_handle.h"
#include "media/webrtc/voice_isolation/voice_isolation.h"
#include "media/webrtc/voice_isolation/voice_isolation_component.h"
#include "services/audio/ml_model_manager.h"
#include "services/audio/processing_audio_fifo.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

namespace audio {

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(VoiceIsolationStartupResult)
enum class StartupResult {
  kSuccess = 0,
  kFailed = 1,
  kAborted = 2,
  kMaxValue = kAborted,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/media/enums.xml:VoiceIsolationStartupResult)

base::expected<std::unique_ptr<media::VoiceIsolationComponent>,
               media::VoiceIsolationCreationResult>
CreateVoiceIsolationComponent(
    scoped_refptr<media::MlModelHandle> model_handle) {
  TRACE_EVENT("audio", "VoiceIsolationHandler::CreateVoiceIsolationComponent");
  CHECK(model_handle);
  return media::VoiceIsolation::CreateComponent(&model_handle->Get());
}

const char* VoiceIsolationCreationResultToString(
    media::VoiceIsolationCreationResult result) {
  switch (result) {
    case media::VoiceIsolationCreationResult::kSuccess:
      return "kSuccess";
    case media::VoiceIsolationCreationResult::kInterpreterCreationFailed:
      return "kInterpreterCreationFailed";
    case media::VoiceIsolationCreationResult::kDelegateCreationFailed:
      return "kDelegateCreationFailed";
    case media::VoiceIsolationCreationResult::kTensorAllocationFailed:
      return "kTensorAllocationFailed";
    case media::VoiceIsolationCreationResult::kIncompatibleModel:
      return "kIncompatibleModel";
    case media::VoiceIsolationCreationResult::kWarmupFailed:
      return "kWarmupFailed";
  }
  NOTREACHED();
}

}  // namespace

class VoiceIsolationHandler::StartupMetricsLogger {
 public:
  explicit StartupMetricsLogger(const void* track_owner)
      : trace_track_(
            perfetto::NamedTrack::FromPointer("audio::VoiceIsolationHandler",
                                              track_owner)),
        start_time_(base::TimeTicks::Now()) {
    TRACE_EVENT_BEGIN("audio", "VoiceIsolationHandler::Initialize",
                      trace_track_);
  }
  StartupMetricsLogger(const StartupMetricsLogger&) = delete;
  StartupMetricsLogger& operator=(const StartupMetricsLogger&) = delete;
  ~StartupMetricsLogger() {
    TRACE_EVENT_END("audio", trace_track_);
    base::UmaHistogramEnumeration(
        "Media.Audio.Capture.VoiceIsolation.StartupResult", result_);
    const base::TimeDelta duration = base::TimeTicks::Now() - start_time_;
    switch (result_) {
      case StartupResult::kSuccess:
        base::UmaHistogramTimes(
            "Media.Audio.Capture.VoiceIsolation.StartupDuration.Success",
            duration);
        break;
      case StartupResult::kFailed:
        base::UmaHistogramTimes(
            "Media.Audio.Capture.VoiceIsolation.StartupDuration.Failure",
            duration);
        break;
      case StartupResult::kAborted:
        break;
    }
  }

  void SetResult(StartupResult result) { result_ = result; }

 private:
  const perfetto::NamedTrack trace_track_;
  const base::TimeTicks start_time_;
  StartupResult result_{StartupResult::kAborted};
};

VoiceIsolationHandler::VoiceIsolationHandler(
    scoped_refptr<media::MlModelHandle> model_handle,
    std::unique_ptr<media::AudioDebugRecorder> debug_recorder,
    const media::AudioParameters& output_params,
    DeliverProcessedAudioCallback deliver_processed_audio_callback,
    ErrorCallback error_callback,
    LogCallback log_callback)
    : model_handle_(std::move(model_handle)),
      output_params_(output_params),
      deliver_processed_audio_callback_(
          std::move(deliver_processed_audio_callback)),
      error_callback_(std::move(error_callback)),
      log_callback_(std::move(log_callback)),
      output_bus_(media::AudioBus::Create(output_params)),
      transition_crossfader_(output_params.frames_per_buffer()),
      debug_recorder_(std::move(debug_recorder)),
      bypass_voice_isolation_(true),
      startup_metrics_logger_(std::make_unique<StartupMetricsLogger>(this)) {
  CHECK(!deliver_processed_audio_callback_.is_null());
  CHECK(!error_callback_.is_null());
  CHECK(!log_callback_.is_null());
  CHECK(output_bus_);
  CHECK(model_handle_);

  SendLogMessage(
      base::StringPrintf("%s({output_params_=[%s], async=true})", __func__,
                         output_params_.AsHumanReadableString().c_str()));

  processing_fifo_ = MaybeCreateProcessingFifo();

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&CreateVoiceIsolationComponent, model_handle_),
      base::BindOnce(&VoiceIsolationHandler::OnComponentCreated,
                     weak_factory_.GetWeakPtr()));
}

VoiceIsolationHandler::VoiceIsolationHandler(
    std::unique_ptr<media::VoiceIsolation> voice_isolation,
    std::unique_ptr<media::AudioDebugRecorder> debug_recorder,
    const media::AudioParameters& output_params,
    DeliverProcessedAudioCallback deliver_processed_audio_callback,
    LogCallback log_callback)
    : model_handle_(nullptr),
      output_params_(output_params),
      deliver_processed_audio_callback_(
          std::move(deliver_processed_audio_callback)),
      log_callback_(std::move(log_callback)),
      output_bus_(media::AudioBus::Create(output_params)),
      transition_crossfader_(output_params.frames_per_buffer()),
      debug_recorder_(std::move(debug_recorder)),
      voice_isolation_(std::move(voice_isolation)),
      bypass_voice_isolation_(false) {
  CHECK(!deliver_processed_audio_callback_.is_null());
  CHECK(!log_callback_.is_null());
  CHECK(output_bus_);
  CHECK(voice_isolation_);

  SendLogMessage(
      base::StringPrintf("%s({output_params_=[%s], async=false})", __func__,
                         output_params_.AsHumanReadableString().c_str()));

  processing_fifo_ = MaybeCreateProcessingFifo();
}

std::unique_ptr<ProcessingAudioFifo>
VoiceIsolationHandler::MaybeCreateProcessingFifo() {
  if (!base::FeatureList::IsEnabled(
          media::kWebRtcVoiceIsolationProcessingFifo)) {
    return nullptr;
  }
  const int fifo_size =
      std::clamp(media::kWebRtcVoiceIsolationProcessingFifoSize.Get(), 1, 100);
  SendLogMessage(base::StringPrintf("%s({fifo_size=%d})", __func__, fifo_size));

  // `base::Unretained(this)` is safe because VoiceIsolationHandler owns the
  // FIFO.
  return std::make_unique<ProcessingAudioFifo>(
      output_params_, fifo_size,
      base::BindRepeating(&VoiceIsolationHandler::ProcessCapturedAudioInternal,
                          base::Unretained(this)),
      base::BindRepeating(&VoiceIsolationHandler::SendLogMessage,
                          base::Unretained(this)),
      ProcessingAudioFifo::FifoType::kVoiceIsolation);
}

VoiceIsolationHandler::~VoiceIsolationHandler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  StopProcessing();
  SendLogMessage(
      base::StringPrintf("%s({initialized=%s})", __func__,
                         base::ToString(voice_isolation_ != nullptr)));
}

void VoiceIsolationHandler::OnComponentCreated(
    base::expected<std::unique_ptr<media::VoiceIsolationComponent>,
                   media::VoiceIsolationCreationResult> component_or_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  TRACE_EVENT("audio", "VoiceIsolationHandler::OnComponentCreated");
  CHECK(startup_metrics_logger_);

  const bool success = component_or_error.has_value();
  startup_metrics_logger_->SetResult(success ? StartupResult::kSuccess
                                             : StartupResult::kFailed);
  startup_metrics_logger_.reset();

  if (!success) {
    const auto error = component_or_error.error();
    const char* error_name = VoiceIsolationCreationResultToString(error);
    SendLogMessage(base::StringPrintf("%s({success=false, error=%s})", __func__,
                                      error_name));
    LOG(ERROR) << "Failed to create VoiceIsolationComponent, error="
               << error_name;

    // Voice isolation was requested when this stream was created. Even if voice
    // isolation is currently disabled/bypassed, a subsequent
    // SetVoiceIsolation(true) call during the stream's lifetime could not be
    // honored. Therefore, report a fatal error regardless of the current toggle
    // state.
    std::move(error_callback_).Run();
    return;
  }

  SendLogMessage(base::StringPrintf("%s({success=true})", __func__));
  voice_isolation_ = media::VoiceIsolation::Create(
      std::move(*component_or_error), output_params_);
  if (voice_isolation_enabled_) {
    bypass_voice_isolation_.store(false, std::memory_order_release);
  }

  SendLogMessage(base::StringPrintf(
      "%s => bypass=%s", __func__,
      base::ToString(bypass_voice_isolation_.load(std::memory_order_relaxed))));
}

void VoiceIsolationHandler::StartProcessing() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  if (processing_fifo_) {
    processing_fifo_->Start();
  }
}

void VoiceIsolationHandler::StopProcessing() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  processing_fifo_.reset();
}

void VoiceIsolationHandler::ProcessCapturedAudio(
    const media::AudioBus& audio_source,
    base::TimeTicks audio_capture_time,
    const media::AudioGlitchInfo& audio_glitch_info) {
  TRACE_EVENT("audio", "VoiceIsolationHandler::ProcessCapturedAudio", "frames",
              audio_source.frames(), "channels", audio_source.channels());

  // When processing is performed in the audio service, the consumer is not
  // expected to use the input volume. Pass a placeholder of 1.0.
  if (processing_fifo_) {
    processing_fifo_->PushData(&audio_source, audio_capture_time,
                               /*volume=*/1.0, audio_glitch_info);
    return;
  }
  ProcessCapturedAudioInternal(audio_source, audio_capture_time, /*volume=*/1.0,
                               audio_glitch_info);
}

void VoiceIsolationHandler::ProcessCapturedAudioInternal(
    const media::AudioBus& audio_source,
    base::TimeTicks audio_capture_time,
    double /*volume*/,
    const media::AudioGlitchInfo& audio_glitch_info) {
  TRACE_EVENT("audio", "VoiceIsolationHandler::ProcessCapturedAudioInternal",
              "frames", audio_source.frames(), "channels",
              audio_source.channels());

  const media::AudioBus* delivered_bus = &audio_source;
  const bool is_bypassed = IsVoiceIsolationBypassed();

  // Run voice isolation while it is enabled, and one last time on the ON -> OFF
  // transition frame to crossfade back to the microphone signal. In steady
  // state bypass, deliver `audio_source` untouched.
  if (!is_bypassed || !was_previously_bypassed_) {
    CHECK(voice_isolation_);
    DCHECK_EQ(output_bus_->channels(), audio_source.channels());
    DCHECK_EQ(output_bus_->frames(), audio_source.frames());
    voice_isolation_->ProcessAudio(audio_source, *output_bus_);
    delivered_bus = output_bus_.get();

    if (is_bypassed) {
      // ON -> OFF transition: switch from the denoised signal to the original
      // signal. Crossfade to the microphone signal, then purge the lookahead
      // frames so that stale speech does not leak when voice isolation is
      // re-enabled.
      // Note: the voice isolation output lags the microphone signal by its
      // algorithmic delay, so this transition still skips the buffered
      // lookahead. The crossfade only removes the amplitude discontinuity.
      CHECK(!was_previously_bypassed_);
      transition_crossfader_.Crossfade(/*from=*/*output_bus_,
                                       /*to=*/audio_source,
                                       /*destination=*/*output_bus_);
      voice_isolation_->ClearBuffers();
    }

    if (was_previously_bypassed_) {
      // OFF -> ON transition: crossfade from the microphone signal to the voice
      // isolation output. The voice isolation internal state is silence (fresh
      // or cleared), so its output ramps up from zeros on its own and its input
      // needs no fade.
      CHECK(!is_bypassed);
      transition_crossfader_.Crossfade(/*from=*/audio_source,
                                       /*to=*/*output_bus_,
                                       /*destination=*/*output_bus_);
    }
    was_previously_bypassed_ = is_bypassed;
  }

  if (debug_recorder_) {
    debug_recorder_->OnData(delivered_bus);
  }

  deliver_processed_audio_callback_.Run(*delivered_bus, audio_capture_time,
                                        audio_glitch_info);
}

void VoiceIsolationHandler::SetVoiceIsolation(bool enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  if (voice_isolation_enabled_ == enabled) {
    return;
  }
  voice_isolation_enabled_ = enabled;
  if (!enabled) {
    bypass_voice_isolation_.store(true, std::memory_order_release);
  } else if (voice_isolation_) {
    // If initialization has already finished, activate processing immediately.
    // Otherwise, async initialization is still in flight; audio remains
    // bypassed until OnComponentCreated() finishes creating `voice_isolation_`.
    bypass_voice_isolation_.store(false, std::memory_order_release);
  }

  SendLogMessage(base::StringPrintf(
      "%s({enabled=%s}) => bypass=%s", __func__, base::ToString(enabled),
      base::ToString(bypass_voice_isolation_.load(std::memory_order_relaxed))));
}

bool VoiceIsolationHandler::IsVoiceIsolationBypassed() const {
  return bypass_voice_isolation_.load(std::memory_order_acquire);
}

bool VoiceIsolationHandler::HasProcessingThread() const {
  return processing_fifo_ != nullptr;
}

int VoiceIsolationHandler::GetFifoSizeForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  return processing_fifo_ ? processing_fifo_->fifo_size() : 0;
}

void VoiceIsolationHandler::SendLogMessage(std::string_view message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  log_callback_.Run(base::StringPrintf("VIH::%.*s [id=%s]",
                                       static_cast<int>(message.size()),
                                       message.data(), id_.ToString().c_str()));
}

std::unique_ptr<VoiceIsolationHandler> VoiceIsolationHandler::MaybeCreate(
    MlModelManager& ml_model_manager,
    const media::AudioParameters& output_params,
    DeliverProcessedAudioCallback deliver_processed_audio_callback,
    ErrorCallback error_callback,
    LogCallback log_callback,
    media::AudioDebugRecordingManager* debug_recording_manager) {
  TRACE_EVENT("audio", "VoiceIsolationHandler::MaybeCreate");
  scoped_refptr<media::MlModelHandle> model_handle =
      ml_model_manager.GetModel(mojom::MlModelType::kVoiceIsolationDenoiser);

  if (!model_handle) {
    // Model not available or there is a problem with the model manager.
    return nullptr;
  }

  std::unique_ptr<media::AudioDebugRecorder> debug_recorder;
  if (debug_recording_manager) {
    debug_recorder = debug_recording_manager->RegisterDebugRecordingSource(
        media::AudioDebugRecordingStreamType::kVoiceIsolation, output_params);
  }

  return base::WrapUnique(new VoiceIsolationHandler(
      std::move(model_handle), std::move(debug_recorder), output_params,
      std::move(deliver_processed_audio_callback), std::move(error_callback),
      std::move(log_callback)));
}

std::unique_ptr<VoiceIsolationHandler> VoiceIsolationHandler::CreateForTesting(
    std::unique_ptr<media::VoiceIsolation> voice_isolation,
    const media::AudioParameters& output_params,
    DeliverProcessedAudioCallback deliver_processed_audio_callback,
    LogCallback log_callback,
    std::unique_ptr<media::AudioDebugRecorder> debug_recorder) {
  return base::WrapUnique(new VoiceIsolationHandler(
      std::move(voice_isolation), std::move(debug_recorder), output_params,
      std::move(deliver_processed_audio_callback), std::move(log_callback)));
}
}  // namespace audio
