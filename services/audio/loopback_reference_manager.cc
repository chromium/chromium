// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/loopback_reference_manager.h"

#include <memory>

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/task/bind_post_task.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_io.h"
#include "media/audio/system_glitch_reporter.h"
#include "media/base/audio_bus.h"
#include "services/audio/audio_manager_power_user.h"
#include "services/audio/reference_signal_provider.h"

// The design of LoopbackReferenceManager is divided into three classes:
//
// * LoopbackReferenceManager is a singleton in the AudioService, and is
// responsible for creating LoopbackReferenceProviders for the caller. It owns
// up to one lazily created LoopbackReferenceManagerCore, which the created
// LoopbackReferenceProviders get a weak reference to.
//
// * LoopbackReferenceManagerCore contains the logic for starting and stopping
// the loopback stream, as well as delivering data to the listeners. If the
// loopback stream experiences an error while it's running, the core will be
// destroyed.
//
// * LoopbackReferenceProvider implements ReferenceSignalProvider. Each
// LoopbackReferenceProvider contains a weak pointer to a
// LoopbackReferenceManagerCore, which it forwards StartListening() and
// StopListening() to. If the core has been destroyed due to an error, these
// calls are safe no-ops.

namespace audio {
namespace {
using ReferenceOpenOutcome = ReferenceSignalProvider::ReferenceOpenOutcome;
using OpenOutcome = media::AudioInputStream::OpenOutcome;

ReferenceOpenOutcome MapStreamOpenOutcomeToReferenceOpenOutcome(
    OpenOutcome outcome) {
  switch (outcome) {
    case OpenOutcome::kSuccess:
      return ReferenceOpenOutcome::SUCCESS;
    case OpenOutcome::kFailedSystemPermissions:
      return ReferenceOpenOutcome::STREAM_OPEN_SYSTEM_PERMISSIONS_ERROR;
    case OpenOutcome::kFailedInUse:
      return ReferenceOpenOutcome::STREAM_OPEN_DEVICE_IN_USE_ERROR;
    default:
      return ReferenceOpenOutcome::STREAM_OPEN_ERROR;
  }
}

ReferenceOpenOutcome ReportOpenResult(ReferenceOpenOutcome outcome) {
  base::UmaHistogramEnumeration("Media.Audio.LoopbackReference.OpenResult",
                                outcome);
  return outcome;
}

}  // namespace

class LoopbackReferenceStreamIdProvider {
 public:
  LoopbackReferenceStreamIdProvider() = default;
  ~LoopbackReferenceStreamIdProvider() = default;

  LoopbackReferenceStreamIdProvider(const LoopbackReferenceStreamIdProvider&) =
      delete;
  LoopbackReferenceStreamIdProvider& operator=(
      const LoopbackReferenceStreamIdProvider&) = delete;

  int GetId() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
    return next_stream_id_;
  }

  int GetNextId() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
    return next_stream_id_++;
  }

 private:
  SEQUENCE_CHECKER(owning_sequence_);
  // To differentiate the streams that LoopbackReferenceManagerCore creates from
  // the InputControllers, we start their ids at 1000000.
  // TODO(crbug.com/412581642): Remove this hack once the reference streams get
  // their own category.
  int next_stream_id_ = 1000000;
};

// Tracks ReferenceOutput::Listeners. When there are at least one listener, it
// creates a system loopback stream and forwards the audio to the listeners.
class LoopbackReferenceManagerCore
    : public media::AudioInputStream::AudioInputCallback {
 public:
  using ErrorCallback = base::OnceCallback<void()>;

  explicit LoopbackReferenceManagerCore(
      media::AudioManager* audio_manager,
      LoopbackReferenceStreamIdProvider* stream_id_provider,
      ErrorCallback on_error_callback)
      : audio_manager_(audio_manager),
        glitch_reporter_(
            media::SystemGlitchReporter::StreamType::kLoopbackReference),
        task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
        stream_id_provider_(stream_id_provider),
        on_error_callback_(std::move(on_error_callback)) {}

  LoopbackReferenceManagerCore(const LoopbackReferenceManagerCore&) = delete;
  LoopbackReferenceManagerCore& operator=(const LoopbackReferenceManagerCore&) =
      delete;

  ~LoopbackReferenceManagerCore() override {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    EnsureLoopbackStreamClosed();
    // If the error callback has been used, there has been an error.
    bool had_runtime_error = !on_error_callback_;
    base::UmaHistogramBoolean("Media.Audio.LoopbackReference.HadRuntimeError",
                              had_runtime_error);
    if (had_runtime_error) {
      for (ReferenceOutput::Listener* listener : listeners_) {
        listener->OnReferenceStreamError();
      }
    }
  }

  void SendLogMessage(const std::string& message) {
    if (!audio_log_) {
      return;
    }

    audio_log_->OnLogMessage(base::StringPrintf(
        "LRMC::%s [id=%u] [this=0x%" PRIXPTR "]", message.c_str(),
        stream_id_provider_->GetId(), reinterpret_cast<uintptr_t>(this)));
  }

  void ReportAndResetGlitchStats() {
    media::SystemGlitchReporter::Stats stats =
        glitch_reporter_.GetLongTermStatsAndReset();
    std::string formatted_message = base::StringPrintf(
        "%s => (num_glitches_detected=[%d], cumulative_audio_lost=[%llu ms], "
        "largest_glitch=[%llu ms])",
        __func__, stats.glitches_detected,
        stats.total_glitch_duration.InMilliseconds(),
        stats.largest_glitch_duration.InMilliseconds());
    SendLogMessage(formatted_message);
  }

  ReferenceOpenOutcome StartListening(ReferenceOutput::Listener* listener,
                                      const std::string& device_id) {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    if (loopback_stream_) {
      // The core was already successfully started.
      base::AutoLock scoped_lock(lock_);
      listeners_.insert(listener);
      return ReferenceOpenOutcome::SUCCESS;
    }

    // Capture audio from all audio devices, or equivalently, audio from all
    // PIDs playing out audio.
    const std::string loopback_device_id =
        media::AudioDeviceDescription::kLoopbackAllDevicesId;
    // TODO(crbug.com/412581642): Determine optimal parameters.
    const media::AudioParameters params =
        AudioManagerPowerUser(audio_manager_)
            .GetInputStreamParameters(loopback_device_id);
    // Does not require the lock because the audio stream is not started.
    sample_rate_ = params.sample_rate();

    // Create an AudioLog for the new stream.
    // TODO(crbug.com/412581642): Add a different AudioComponent for the
    // reference loopback streams and show them in chrome://media-internals
    audio_log_ = audio_manager_->CreateAudioLog(
        media::AudioLogFactory::AudioComponent::kAudioInputController,
        stream_id_provider_->GetNextId());
    SendLogMessage(
        base::StrCat({"StartListening(device_id=", loopback_device_id, ")"}));

    // Create the stream, and return an error if we fail.
    loopback_stream_ = audio_manager_->MakeAudioInputStream(
        params, loopback_device_id,
        base::BindRepeating(&media::AudioLog::OnLogMessage,
                            base::Unretained(audio_log_.get())));
    if (!loopback_stream_) {
      return ReferenceOpenOutcome::STREAM_CREATE_ERROR;
    }

    // Open the stream, and return an error if we fail.
    media::AudioInputStream::OpenOutcome stream_open_outcome =
        loopback_stream_->Open();
    if (stream_open_outcome != OpenOutcome::kSuccess) {
      loopback_stream_.ExtractAsDangling()->Close();
      return MapStreamOpenOutcomeToReferenceOpenOutcome(stream_open_outcome);
    }
    audio_log_->OnCreated(params, loopback_device_id);

    // Add the listener and start the stream.
    {
      base::AutoLock scoped_lock(lock_);
      listeners_.insert(listener);
    }
    loopback_stream_->Start(this);
    audio_log_->OnStarted();

    return ReferenceOpenOutcome::SUCCESS;
  }

  void StopListening(ReferenceOutput::Listener* listener) {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    SendLogMessage("StopListening()");
    bool is_empty;
    {
      base::AutoLock scoped_lock(lock_);
      listeners_.erase(listener);
      is_empty = listeners_.empty();
    }
    if (is_empty) {
      // TODO(crbug.com/412581642): Close after a delay instead, in case we are
      // calling StartListening() again soon.
      EnsureLoopbackStreamClosed();
    }
  }

  base::WeakPtr<LoopbackReferenceManagerCore> GetWeakPtr() {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    return weak_ptr_factory_.GetWeakPtr();
  }

  // AudioInputCallback implementation
  void OnData(const media::AudioBus* source,
              base::TimeTicks capture_time,
              double volume,
              const media::AudioGlitchInfo& audio_glitch_info) override {
    base::AutoLock scoped_lock(lock_);
    for (ReferenceOutput::Listener* listener : listeners_) {
      // Since we are using a loopback signal, the audio has likely already been
      // played out at this point, so the delay would be negative. To avoid
      // confusion in the AudioProcessor, we set it to 0 for now.
      // TODO(crbug.com/412581642): Figure out the correct value for this delay.
      listener->OnPlayoutData(*source, sample_rate_,
                              /*audio_delay=*/base::TimeDelta());
    }
    glitch_reporter_.UpdateStats(audio_glitch_info.duration);
  }

  void OnError() override {
    // We post a new task even when we run on the same sequence, to avoid odd
    // call stacks where the input stream is closed while it's being started.
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&LoopbackReferenceManagerCore::OnErrorMainSequence,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void OnErrorMainSequence() {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    SendLogMessage("OnError()");
    if (on_error_callback_) {
      std::move(on_error_callback_).Run();
    }
  }

 private:
  void EnsureLoopbackStreamClosed() {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    if (!loopback_stream_) {
      return;
    }
    loopback_stream_->Stop();
    ReportAndResetGlitchStats();
    audio_log_->OnStopped();
    // The the stream will destroy itself upon Close(), so we use
    // ExtractAsDangling() to clear the raw_ptr first.
    loopback_stream_.ExtractAsDangling()->Close();
    audio_log_->OnClosed();
    audio_log_.reset();
  }

  const raw_ptr<media::AudioManager> audio_manager_;
  media::SystemGlitchReporter glitch_reporter_;
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  const raw_ptr<LoopbackReferenceStreamIdProvider> stream_id_provider_;
  ErrorCallback on_error_callback_;
  raw_ptr<media::AudioInputStream> loopback_stream_ = nullptr;
  std::unique_ptr<media::AudioLog> audio_log_;
  int sample_rate_;

  base::Lock lock_;
  base::flat_set<ReferenceOutput::Listener*> listeners_ GUARDED_BY(lock_);

  base::WeakPtrFactory<LoopbackReferenceManagerCore> weak_ptr_factory_{this};
};

// Contains a weak pointer to a LoopbackReferenceManagerCore, and forwards its
// calls to it. If the core has been destroyed due to an error, all its
// operations become safe no-ops. (not implemented yet).
class LoopbackReferenceProvider : public ReferenceSignalProvider {
 public:
  LoopbackReferenceProvider(base::WeakPtr<LoopbackReferenceManagerCore> core)
      : core_(core) {}

  ReferenceSignalProvider::Type GetType() const final {
    return ReferenceSignalProvider::Type::kLoopbackReference;
  }

  ReferenceOpenOutcome StartListening(ReferenceOutput::Listener* listener,
                                      const std::string& device_id) final {
    DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
    if (core_) {
      return ReportOpenResult(core_->StartListening(listener, device_id));
    }
    // If the core no longer exists, it must have been destroyed due to an
    // error.
    return ReportOpenResult(ReferenceOpenOutcome::STREAM_PREVIOUS_ERROR);
  }

  void StopListening(ReferenceOutput::Listener* listener) final {
    DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
    if (core_) {
      core_->StopListening(listener);
    }
  }

 private:
  SEQUENCE_CHECKER(owning_sequence_);
  base::WeakPtr<LoopbackReferenceManagerCore> core_;
};

LoopbackReferenceManager::LoopbackReferenceManager(
    media::AudioManager* audio_manager)
    : audio_manager_(audio_manager),
      stream_id_provider_(
          std::make_unique<LoopbackReferenceStreamIdProvider>()) {}

std::unique_ptr<ReferenceSignalProvider>
LoopbackReferenceManager::GetReferenceSignalProvider() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  if (!core_) {
    core_ = std::make_unique<LoopbackReferenceManagerCore>(
        audio_manager_, stream_id_provider_.get(),
        base::BindOnce(&LoopbackReferenceManager::OnCoreError,
                       weak_ptr_factory_.GetWeakPtr()));
  }
  return std::make_unique<LoopbackReferenceProvider>(core_->GetWeakPtr());
}

void LoopbackReferenceManager::OnCoreError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  CHECK(core_);
  core_.reset();
}

LoopbackReferenceManager::~LoopbackReferenceManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
}

}  // namespace audio
