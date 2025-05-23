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
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_io.h"
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
// the loopback stream, as well as delivering data to the listeners. If the core
// experiences an error, it will be destroyed (not implemented yet because error
// handling is not imlpemented).
//
// * LoopbackReferenceProvider implements ReferenceSignalProvider. Each
// LoopbackReferenceProvider contains a weak pointer to a
// LoopbackReferenceManagerCore, which it forwards StartListening() and
// StopListening() to. If the core has been destroyed due to an error, it does
// nothing (this cannot happen yet because error handling is not implemented).

namespace audio {

// Tracks ReferenceOutput::Listeners. When there are at least one listener, it
// creates a system loopback stream and forwards the audio to the listeners.
class LoopbackReferenceManagerCore
    : public media::AudioInputStream::AudioInputCallback {
 public:
  explicit LoopbackReferenceManagerCore(media::AudioManager* audio_manager)
      : audio_manager_(audio_manager) {}

  LoopbackReferenceManagerCore(const LoopbackReferenceManagerCore&) = delete;
  LoopbackReferenceManagerCore& operator=(const LoopbackReferenceManagerCore&) =
      delete;

  ~LoopbackReferenceManagerCore() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
    EnsureLoopbackStreamClosed();
  }

  void StartListening(ReferenceOutput::Listener* listener,
                      const std::string& device_id) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
    EnsureLoopbackStreamStarted();
    base::AutoLock scoped_lock(lock_);
    listeners_.insert(listener);
  }

  void StopListening(ReferenceOutput::Listener* listener) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
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
    DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
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
  }

  void OnError() override {
    // TODO(crbug.com/412581642): Handle errors.
    LOG(ERROR) << "System loopback AEC reference stream failed.";
  }

 private:
  void EnsureLoopbackStreamStarted() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
    if (loopback_stream_) {
      return;
    }
    // Capture audio from all audio devices, or equivalently, audio from all
    // PIDs playing out audio.
    const std::string loopback_device_id =
        media::AudioDeviceDescription::kLoopbackAllDevicesId;

    // TODO(crbug.com/412581642): Determine optimal parameters.
    const media::AudioParameters params =
        AudioManagerPowerUser(audio_manager_)
            .GetInputStreamParameters(loopback_device_id);
    sample_rate_ = params.sample_rate();

    // TODO(crbug.com/412581642): Add a different AudioComponent for the
    // reference loopback streams and show them in chrome://media-internals
    audio_log_ = audio_manager_->CreateAudioLog(
        media::AudioLogFactory::AudioComponent::kAudioInputController,
        next_loopback_stream_id_++);

    loopback_stream_ = audio_manager_->MakeAudioInputStream(
        params, loopback_device_id,
        base::BindRepeating(&media::AudioLog::OnLogMessage,
                            base::Unretained(audio_log_.get())));
    loopback_stream_->Open();
    audio_log_->OnCreated(params, loopback_device_id);
    loopback_stream_->Start(this);
    audio_log_->OnStarted();
  }

  void EnsureLoopbackStreamClosed() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
    if (!loopback_stream_) {
      return;
    }
    loopback_stream_->Stop();
    audio_log_->OnStopped();
    // The the stream will destroy itself upon Close(), so we use
    // ExtractAsDangling() to clear the raw_ptr first.
    loopback_stream_.ExtractAsDangling()->Close();
    audio_log_->OnClosed();
    audio_log_.reset();
  }

  SEQUENCE_CHECKER(owning_sequence_);
  const raw_ptr<media::AudioManager> audio_manager_;
  raw_ptr<media::AudioInputStream> loopback_stream_ = nullptr;
  std::unique_ptr<media::AudioLog> audio_log_;
  // To differentiate the streams that LoopbackReferenceManagerCore creates from
  // the InputControllers, we start their ids at 1000000.
  // TODO(crbug.com/412581642): Remove this hack once the reference streams get
  // their own category.
  int next_loopback_stream_id_ = 1000000;
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

  void StartListening(ReferenceOutput::Listener* listener,
                      const std::string& device_id) final {
    DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
    DCHECK(core_);
    if (core_) {
      core_->StartListening(listener, device_id);
    }
  }

  void StopListening(ReferenceOutput::Listener* listener) final {
    DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
    DCHECK(core_);
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
    : audio_manager_(audio_manager) {}

std::unique_ptr<ReferenceSignalProvider>
LoopbackReferenceManager::GetReferenceSignalProvider() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  if (!core_) {
    core_ = std::make_unique<LoopbackReferenceManagerCore>(audio_manager_);
  }
  return std::make_unique<LoopbackReferenceProvider>(core_->GetWeakPtr());
}

LoopbackReferenceManager::~LoopbackReferenceManager() = default;

}  // namespace audio
