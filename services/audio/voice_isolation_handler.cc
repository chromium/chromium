// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/voice_isolation_handler.h"

#include <cinttypes>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "media/base/audio_bus.h"
#include "media/webrtc/ml_model_handle.h"
#include "media/webrtc/voice_isolation/voice_isolation.h"
#include "media/webrtc/voice_isolation/voice_isolation_component.h"
#include "services/audio/ml_model_manager.h"
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

std::unique_ptr<media::VoiceIsolationComponent> CreateVoiceIsolationComponent(
    scoped_refptr<media::MlModelHandle> model_handle) {
  TRACE_EVENT("audio", "VoiceIsolationHandler::CreateVoiceIsolationComponent");
  if (!model_handle) {
    return nullptr;
  }
  return media::VoiceIsolation::CreateComponent(&model_handle->Get());
}

}  // namespace

class VoiceIsolationHandler::StartupMetricsLogger {
 public:
  StartupMetricsLogger() : start_time_(base::TimeTicks::Now()) {}
  StartupMetricsLogger(const StartupMetricsLogger&) = delete;
  StartupMetricsLogger& operator=(const StartupMetricsLogger&) = delete;
  ~StartupMetricsLogger() {
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
  const base::TimeTicks start_time_;
  StartupResult result_{StartupResult::kAborted};
};

VoiceIsolationHandler::VoiceIsolationHandler(
    scoped_refptr<media::MlModelHandle> model_handle,
    const media::AudioParameters& output_params,
    DeliverProcessedAudioCallback deliver_processed_audio_callback,
    LogCallback log_callback)
    : model_handle_(std::move(model_handle)),
      output_params_(output_params),
      deliver_processed_audio_callback_(
          std::move(deliver_processed_audio_callback)),
      log_callback_(std::move(log_callback)),
      output_bus_(media::AudioBus::Create(output_params)),
      bypass_voice_isolation_(true),
      startup_metrics_logger_(std::make_unique<StartupMetricsLogger>()) {
  CHECK(!deliver_processed_audio_callback_.is_null());
  CHECK(!log_callback_.is_null());
  CHECK(output_bus_);
  CHECK(model_handle_);

  SendLogMessage(
      base::StringPrintf("%s({output_params_=[%s], async=true})", __func__,
                         output_params_.AsHumanReadableString().c_str()));

  TRACE_EVENT_BEGIN(
      "audio", "VoiceIsolationHandler::Initialize",
      perfetto::NamedTrack::FromPointer("audio::VoiceIsolationHandler", this));

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&CreateVoiceIsolationComponent, model_handle_),
      base::BindOnce(&VoiceIsolationHandler::OnComponentCreated,
                     weak_factory_.GetWeakPtr()));
}

VoiceIsolationHandler::VoiceIsolationHandler(
    std::unique_ptr<media::VoiceIsolation> voice_isolation,
    const media::AudioParameters& output_params,
    DeliverProcessedAudioCallback deliver_processed_audio_callback,
    LogCallback log_callback)
    : model_handle_(nullptr),
      output_params_(output_params),
      deliver_processed_audio_callback_(
          std::move(deliver_processed_audio_callback)),
      log_callback_(std::move(log_callback)),
      output_bus_(media::AudioBus::Create(output_params)),
      voice_isolation_(std::move(voice_isolation)),
      bypass_voice_isolation_(false) {
  CHECK(!deliver_processed_audio_callback_.is_null());
  CHECK(!log_callback_.is_null());
  CHECK(output_bus_);
  CHECK(voice_isolation_);

  SendLogMessage(
      base::StringPrintf("%s({output_params_=[%s], async=false})", __func__,
                         output_params_.AsHumanReadableString().c_str()));
}

VoiceIsolationHandler::~VoiceIsolationHandler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  SendLogMessage(
      base::StringPrintf("%s({initialized=%s})", __func__,
                         base::ToString(voice_isolation_ != nullptr)));
  if (!voice_isolation_) {
    TRACE_EVENT_END("audio", perfetto::NamedTrack::FromPointer(
                                 "audio::VoiceIsolationHandler", this));
  }
}

void VoiceIsolationHandler::OnComponentCreated(
    std::unique_ptr<media::VoiceIsolationComponent> component) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  TRACE_EVENT("audio", "VoiceIsolationHandler::OnComponentCreated");
  SendLogMessage(base::StringPrintf("%s({success=%s})", __func__,
                                    base::ToString(component != nullptr)));

  CHECK(startup_metrics_logger_);
  startup_metrics_logger_->SetResult(component ? StartupResult::kSuccess
                                               : StartupResult::kFailed);
  startup_metrics_logger_.reset();

  if (!component) {
    LOG(ERROR) << "Failed to create VoiceIsolationComponent.";
    return;
  }
  voice_isolation_ =
      media::VoiceIsolation::Create(std::move(component), output_params_);
  TRACE_EVENT_END("audio", perfetto::NamedTrack::FromPointer(
                               "audio::VoiceIsolationHandler", this));
  if (voice_isolation_enabled_) {
    bypass_voice_isolation_.store(false, std::memory_order_release);
  }

  SendLogMessage(base::StringPrintf(
      "%s => bypass=%s", __func__,
      base::ToString(bypass_voice_isolation_.load(std::memory_order_relaxed))));
}

void VoiceIsolationHandler::ProcessCapturedAudio(
    const media::AudioBus& audio_source,
    base::TimeTicks audio_capture_time,
    const media::AudioGlitchInfo& audio_glitch_info) {
  TRACE_EVENT("audio", "VoiceIsolationHandler::ProcessCapturedAudio");
  if (IsVoiceIsolationBypassed()) {
    deliver_processed_audio_callback_.Run(audio_source, audio_capture_time,
                                          audio_glitch_info);
    return;
  }
  DCHECK(voice_isolation_);
  DCHECK_EQ(output_bus_->channels(), audio_source.channels());
  DCHECK_EQ(output_bus_->frames(), audio_source.frames());
  voice_isolation_->ProcessAudio(audio_source, *output_bus_);
  deliver_processed_audio_callback_.Run(*output_bus_, audio_capture_time,
                                        audio_glitch_info);
}

void VoiceIsolationHandler::SetVoiceIsolation(bool enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  if (voice_isolation_enabled_ == enabled) {
    return;
  }
  voice_isolation_enabled_ = enabled;
  if (!enabled) {
    // TODO(crbug.com/544689562): Disabling/bypassing voice isolation leaves
    // stranded audio inside the internal lookahead buffers or FIFOs. When
    // re-enabled, this stale audio can be delivered belatedly alongside new
    // audio, yielding audible glitches or echoes. We must reset or flush the
    // internal state of media::VoiceIsolation when voice isolation is
    // re-enabled or bypassed.
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
  return false;
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
    LogCallback log_callback) {
  TRACE_EVENT("audio", "VoiceIsolationHandler::MaybeCreate");
  scoped_refptr<media::MlModelHandle> model_handle =
      ml_model_manager.GetModel(mojom::MlModelType::kVoiceIsolationDenoiser);

  if (!model_handle) {
    // Model not available or there is a problem with the model manager.
    return nullptr;
  }

  return base::WrapUnique(new VoiceIsolationHandler(
      std::move(model_handle), output_params,
      std::move(deliver_processed_audio_callback), std::move(log_callback)));
}

std::unique_ptr<VoiceIsolationHandler> VoiceIsolationHandler::CreateForTesting(
    std::unique_ptr<media::VoiceIsolation> voice_isolation,
    const media::AudioParameters& output_params,
    DeliverProcessedAudioCallback deliver_processed_audio_callback,
    LogCallback log_callback) {
  return base::WrapUnique(new VoiceIsolationHandler(
      std::move(voice_isolation), output_params,
      std::move(deliver_processed_audio_callback), std::move(log_callback)));
}
}  // namespace audio
