// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_LOOPBACK_SIGNAL_PROVIDER_H_
#define SERVICES_AUDIO_LOOPBACK_SIGNAL_PROVIDER_H_

#include <map>
#include <memory>

#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "media/base/audio_parameters.h"
#include "services/audio/loopback_coordinator.h"
#include "services/audio/snooper_node.h"

namespace media {
class AudioBus;
}  // namespace media

namespace audio {

class LoopbackSource;

class LoopbackSignalProviderInterface {
 public:
  LoopbackSignalProviderInterface(const LoopbackSignalProviderInterface&) =
      delete;
  LoopbackSignalProviderInterface& operator=(
      const LoopbackSignalProviderInterface&) = delete;
  virtual ~LoopbackSignalProviderInterface() = default;

  virtual void Start() = 0;
  virtual base::TimeTicks PullLoopbackData(media::AudioBus* destination,
                                           base::TimeTicks capture_time,
                                           double volume) = 0;

 protected:
  LoopbackSignalProviderInterface() = default;
};

class LoopbackSignalProvider final : public LoopbackSignalProviderInterface,
                                     public LoopbackGroupObserver::Listener {
 public:
  LoopbackSignalProvider(
      const media::AudioParameters& output_params,
      std::unique_ptr<LoopbackGroupObserver> loopback_group_observer);

  ~LoopbackSignalProvider() final;

  // Starts observing the sources.
  void Start() final;

  // Pulls audio from the observed sources that was/will be played at
  // `capture_time`. The LoopbackSignalProvider may add additional delay to
  // avoid glitches. Returns the (possibly delayed) capture time. Can be called
  // on any thread.
  base::TimeTicks PullLoopbackData(media::AudioBus* destination,
                                   base::TimeTicks capture_time,
                                   double volume) final;

  // LoopbackGroupObserver::Listener.
  void OnSourceAdded(LoopbackSource* source) final;
  void OnSourceRemoved(LoopbackSource* source) final;

 private:
  // The audio parameters of the output.
  const media::AudioParameters output_params_;

  // Observer for the group whose combined loopback signal should be captured.
  const std::unique_ptr<LoopbackGroupObserver> loopback_group_observer_;

  // Lock preventing simultaneous access to `snoopers_`, which is accessed both
  // on the main thread and the audio thread.
  base::Lock lock_;

  // The snoopers associated with each group member.
  std::map<LoopbackSource*, std::unique_ptr<SnooperNode>> snoopers_
      GUARDED_BY(lock_);

  // The amount of time in the past from which to capture the audio. The audio
  // recorded from each SnooperNode input is being generated with a target
  // playout time in the near future (usually 1 to 20 ms). To avoid underflow,
  // audio is always fetched from a safe position in the recent past.
  //
  // This is updated to match the SnooperNode whose recording is most delayed.
  //
  // Only used on the audio thread.
  base::TimeDelta capture_delay_;

  // Used to transfer the audio from each SnooperNode before mixing them into a
  // combined signal, if there are several SnooperNodes.
  //
  // Only used on the audio thread.
  std::unique_ptr<media::AudioBus> transfer_bus_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace audio

#endif  // SERVICES_AUDIO_LOOPBACK_SIGNAL_PROVIDER_H_
