// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_OUTPUT_DEVICE_MIXER_IMPL_H_
#define SERVICES_AUDIO_OUTPUT_DEVICE_MIXER_IMPL_H_

#include <memory>
#include <set>
#include <string>

#include "base/check.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/dcheck_is_on.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "base/timer/timer.h"
#include "media/audio/audio_io.h"
#include "media/base/audio_parameters.h"
#include "media/base/reentrancy_checker.h"
#include "services/audio/mixing_graph.h"
#include "services/audio/output_device_mixer.h"

namespace audio {
class MixingGraph;

// OutputDeviceMixerImpl manages the rendering of all output streams created by
// MakeMixableStream(). It has two rendering modes: if there
// are not listeners attached, each active output stream is rendered
// independently. If the mixer has listeners and at least one of the managed
// streams there is playing, all output streams are mixed by
// |mixing_graph_| and the result is played by |mixing_graph_output_stream_|.
// After mixing started, the mixed playback will stop only when the last
// listener is gone; this means the mixed playback will continue even when there
// are no managed streams playing. That is done because the listeners do not
// like when the reference signal stops and then starts again: it can cause
// reference signal delay jumps. TODO(olka): add metrics for how often/how long
// we may have listeners forsing such silent mixed playback.
class OutputDeviceMixerImpl final : public OutputDeviceMixer {
 public:
  using ErrorType = media::AudioOutputStream::AudioSourceCallback::ErrorType;

  static constexpr base::TimeDelta kSwitchToUnmixedPlaybackDelay =
      base::Seconds(1);

  static constexpr double kDefaultVolume = 1.0;

  // |device_id| is the id of the device to manage the playback for (expected to
  // be a valid physical output device id); |output_params| are the parameters
  // of the audio mix the mixer will be playing in case there are listeners
  // attached (otherwise, each output stream is played independently using its
  // individual parameters); |create_mixing_graph_callback| is used to create
  // a mixing graph object which will combine individual audio streams into a
  // mixed signal; |create_stream_callback| is used to create physical output
  // streams to be used for audio rendering.
  OutputDeviceMixerImpl(
      const std::string& device_id,
      const media::AudioParameters& output_params,
      MixingGraph::CreateCallback create_mixing_graph_callback,
      CreateStreamCallback create_stream_callback);
  OutputDeviceMixerImpl(const OutputDeviceMixerImpl&) = delete;
  OutputDeviceMixerImpl& operator=(const OutputDeviceMixerImpl&) = delete;
  ~OutputDeviceMixerImpl() final;

  // Creates an output stream managed by |this|.
  media::AudioOutputStream* MakeMixableStream(
      const media::AudioParameters& params,
      base::OnceCallback<void()> on_device_change_callback) final;

  void ProcessDeviceChange() final;

  // ReferenceOutput implementation.
  // Starts listening to the mixed audio stream.
  void StartListening(Listener* listener) final;
  // Stops listening to the mixed audio stream.
  void StopListening(Listener* listener) final;

 private:
  class MixTrack;
  class MixableOutputStream;

  struct StreamAutoClose {
    void operator()(media::AudioOutputStream* stream) {
      if (!stream)
        return;
      stream->Close();
      stream = nullptr;
    }
  };

  // Do not change: used for UMA reporting, matches
  // AudioOutputDeviceMixerMixedPlaybackStatus from enums.xml.
  enum class MixingError {
    kNone = 0,
    kOpenFailed,
    kPlaybackFailed,
    kMaxValue = kPlaybackFailed
  };

  using MixTracks =
      std::set<std::unique_ptr<MixTrack>, base::UniquePtrComparator>;
  using ActiveTracks = std::set<raw_ptr<MixTrack, SetExperimental>>;
  using Listeners = std::set<raw_ptr<Listener, SetExperimental>>;

  // Operations delegated by MixableOutputStream.
  bool OpenStream(MixTrack* mix_track);
  void StartStream(MixTrack* mix_track,
                   media::AudioOutputStream::AudioSourceCallback* callback);
  void StopStream(MixTrack* mix_track);
  void CloseStream(MixTrack* mix_track);

  // Helper to create physical audio streams.
  media::AudioOutputStream* CreateAndOpenDeviceStream(
      const media::AudioParameters& params);

  // Delivers audio to listeners; provided as a callback to MixingGraph.
  void BroadcastToListeners(const media::AudioBus& audio_bus,
                            base::TimeDelta delay);
  // Processes |mixing_output_stream_| rendering errors; provided as a callback
  // to MixingGraph.
  void OnMixingGraphError(ErrorType error);

  // Helpers to manage audio playback.
  bool HasListeners() const;
  bool MixingInProgress() const { return mixing_in_progress_; }
  void EnsureMixingGraphOutputStreamOpen();
  void StartMixingGraphPlayback();
  void StopMixingGraphPlayback(MixingError mixing_error);
  void SwitchToUnmixedPlaybackTimerHelper();

  static const char* ErrorToString(MixingError error);

  SEQUENCE_CHECKER(owning_sequence_);

  // Callback to create physical audio streams.
  const CreateStreamCallback create_stream_callback_;

  // Streams managed by the mixer.
  MixTracks mix_tracks_ GUARDED_BY_CONTEXT(owning_sequence_);

  // Active tracks rendering audio.
  ActiveTracks active_tracks_ GUARDED_BY_CONTEXT(owning_sequence_);

  // Listeners receiving the reference output (mixed playback). Playback is
  // mixed if and only if there is at least one listener.
  mutable base::Lock listener_lock_;
  Listeners listeners_ GUARDED_BY(listener_lock_);

  // Audio parameters for mixed playback.
  const media::AudioParameters mixing_graph_output_params_;

  // Provides the audio mix combined of |active_members_| audio to
  // |mixing_output_stream_| for playback.
  const std::unique_ptr<MixingGraph> mixing_graph_
      GUARDED_BY_CONTEXT(owning_sequence_);

  // Stream to render the audio mix. Non-null if and only if the playback is
  // being mixed at the moment.
  std::unique_ptr<media::AudioOutputStream, StreamAutoClose>
      mixing_graph_output_stream_ GUARDED_BY_CONTEXT(owning_sequence_);

  // Delays switching to unmixed playback in case a new listener is coming
  // soon (within kSwitchToIndependentPlaybackDelay).
  base::OneShotTimer switch_to_unmixed_playback_delay_timer_
      GUARDED_BY_CONTEXT(owning_sequence_);

#if DCHECK_IS_ON()
  bool device_changed_ = false;
#endif

  // A mixable stream operation cannot be invoked within a context of another
  // such operation. In practice it means that AudioOutputStream created by the
  // mixer cannot be stopped/closed synchronously from AudioSourceCallback
  // provided to it on AudioOutputStream::Start().
  REENTRANCY_CHECKER(reentrancy_checker_);

  bool mixing_in_progress_ = false;

  // Supplies weak pointers to |this| for MixableOutputStream instances.
  base::WeakPtrFactory<OutputDeviceMixerImpl> weak_factory_{this};
};

}  // namespace audio

#endif  // SERVICES_AUDIO_OUTPUT_DEVICE_MIXER_IMPL_H_
