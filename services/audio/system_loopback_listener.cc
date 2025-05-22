// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/system_loopback_listener.h"

#include <memory>

#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_io.h"
#include "media/base/audio_bus.h"
#include "services/audio/audio_manager_power_user.h"
#include "services/audio/device_output_listener.h"

namespace audio {

class SystemLoopbackListener::AudioCallback
    : public media::AudioInputStream::AudioInputCallback {
 public:
  explicit AudioCallback(int sample_rate) : sample_rate_(sample_rate) {}

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

  base::Lock lock_;
  base::flat_set<ReferenceOutput::Listener*> listeners_ GUARDED_BY(lock_);
  const int sample_rate_;

  void AddListener(ReferenceOutput::Listener* listener) {
    base::AutoLock scoped_lock(lock_);
    listeners_.insert(listener);
  }

  bool RemoveListener(ReferenceOutput::Listener* listener) {
    base::AutoLock scoped_lock(lock_);
    listeners_.erase(listener);
    return listeners_.empty();
  }
};

SystemLoopbackListener::SystemLoopbackListener(
    media::AudioManager* audio_manager)
    : audio_manager_(audio_manager) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
}

SystemLoopbackListener::~SystemLoopbackListener() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  EnsureLoopbackStreamClosed();
}

void SystemLoopbackListener::StartListening(ReferenceOutput::Listener* listener,
                                            const std::string& device_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  EnsureLoopbackStreamStarted();
  audio_callback_->AddListener(listener);
}

void SystemLoopbackListener::StopListening(
    ReferenceOutput::Listener* listener) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  if (audio_callback_->RemoveListener(listener)) {
    // TODO(crbug.com/412581642): Close after a delay instead, in case we are
    // calling StartListening() again soon.
    EnsureLoopbackStreamClosed();
  }
}

void SystemLoopbackListener::EnsureLoopbackStreamStarted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  if (loopback_stream_) {
    return;
  }
  // Capture audio from all audio devices, or equivalently, audio from all PIDs
  // playing out audio.
  const std::string loopback_device_id =
      media::AudioDeviceDescription::kLoopbackAllDevicesId;

  // TODO(crbug.com/412581642): Determine optimal parameters.
  const media::AudioParameters params =
      AudioManagerPowerUser(audio_manager_)
          .GetInputStreamParameters(loopback_device_id);
  audio_callback_ = std::make_unique<AudioCallback>(params.sample_rate());

  // TODO(crbug.com/412581642): Add a different AudioComponent for the reference
  // loopback streams and show them in chrome://media-internals
  audio_log_ = audio_manager_->CreateAudioLog(
      media::AudioLogFactory::AudioComponent::kAudioInputController,
      next_loopback_stream_id_++);

  loopback_stream_ = audio_manager_->MakeAudioInputStream(
      params, loopback_device_id,
      base::BindRepeating(&media::AudioLog::OnLogMessage,
                          base::Unretained(audio_log_.get())));
  loopback_stream_->Open();
  audio_log_->OnCreated(params, loopback_device_id);
  loopback_stream_->Start(audio_callback_.get());
  audio_log_->OnStarted();
}

void SystemLoopbackListener::EnsureLoopbackStreamClosed() {
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
  audio_callback_.reset();
  audio_log_.reset();
}

}  // namespace audio
