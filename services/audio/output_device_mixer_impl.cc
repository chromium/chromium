// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/output_device_mixer_impl.h"

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/dcheck_is_on.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/not_fatal_until.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_io.h"

namespace audio {

constexpr base::TimeDelta OutputDeviceMixerImpl::kSwitchToUnmixedPlaybackDelay;
constexpr double OutputDeviceMixerImpl::kDefaultVolume;

namespace {

// Do not change: used for UMA reporting, matches
// AudioOutputDeviceMixerStreamStatus from enums.xml.
enum class TrackError {
  kNone = 0,
  kIndependentOpenFailed,
  kIndependentPlaybackFailed,
  kMixedOpenFailed,
  kMixedPlaybackFailed,
  kMaxValue = kMixedPlaybackFailed
};

const char* TrackErrorToString(TrackError error) {
  switch (error) {
    case TrackError::kIndependentOpenFailed:
      return "Failed to open independent rendering stream";
    case TrackError::kIndependentPlaybackFailed:
      return "Error during independent playback";
    case TrackError::kMixedOpenFailed:
      return "Failed to open mixing rendering stream";
    case TrackError::kMixedPlaybackFailed:
      return "Error during mixed playback";
    default:
      NOTREACHED_IN_MIGRATION();
      return "No error";
  }
}

}  // namespace

// Audio data flow though the mixer:
//
// * Independent (ummixed) audio stream playback:
//    MixTrack::|audio_source_callback_|
//    -> MixTrack::|rendering_stream_|.
//
// * Mixed playback:
//    MixTrack::|audio_source_callback_|
//    -> MixTrack::|graph_input_|
//    --> OutputDeviceMixerImpl::|mixing_graph_|
//    ---> OutputDeviceMixerImpl::|mixing_graph_rendering_stream_|

// Helper class which stores all the data associated with a specific audio
// output managed by the mixer. To the clients such an audio output is
// represented as MixableOutputStream (below).
class OutputDeviceMixerImpl::MixTrack final
    : public media::AudioOutputStream::AudioSourceCallback {
 public:
  MixTrack(OutputDeviceMixerImpl* mixer,
           std::unique_ptr<MixingGraph::Input> graph_input,
           base::OnceClosure on_device_change_callback)
      : mixer_(mixer),
        on_device_change_callback_(std::move(on_device_change_callback)),
        graph_input_(std::move(graph_input)) {}

  ~MixTrack() final {
    DCHECK(!audio_source_callback_);
    base::UmaHistogramEnumeration(
        "Media.Audio.OutputDeviceMixer.StreamStatus", error_);
  }

  void SetSource(
      media::AudioOutputStream::AudioSourceCallback* audio_source_callback) {
    DCHECK(!audio_source_callback_ || !audio_source_callback);
    audio_source_callback_ = audio_source_callback;
  }

  void SetVolume(double volume) {
    volume_ = volume;
    graph_input_->SetVolume(volume);
    if (rendering_stream_)
      rendering_stream_->SetVolume(volume);
  }

  double GetVolume() const { return volume_; }

  void StartProvidingAudioToMixingGraph() {
    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("audio"),
                 "MixTrack::StartProvidingAudioToMixingGraph", "this",
                 static_cast<void*>(this));
    DCHECK(audio_source_callback_);
    graph_input_->Start(audio_source_callback_);
  }

  void StopProvidingAudioToMixingGraph() {
    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("audio"),
                 "MixTrack::StopProvidingAudioToMixingGraph", "this",
                 static_cast<void*>(this));
    DCHECK(audio_source_callback_);
    graph_input_->Stop();
  }

  bool OpenIndependentRenderingStream() {
    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("audio"),
                 "MixTrack::OpenIndependentRenderingStream", "this",
                 static_cast<void*>(this));
    DCHECK(!rendering_stream_);
    rendering_stream_.reset(
        mixer_->CreateAndOpenDeviceStream(graph_input_->GetParams()));

    if (rendering_stream_) {
      rendering_stream_->SetVolume(volume_);
    } else {
      LOG(ERROR) << "Failed to open individual rendering stream";
    }
    return !!rendering_stream_;
  }

  void StartIndependentRenderingStream() {
    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("audio"),
                 "MixTrack::StartIndependentRenderingStream", "this",
                 static_cast<void*>(this));
    DCHECK(audio_source_callback_);
    if (!rendering_stream_) {
      // Open the rendering stream if it's not open yet. It will be closed in
      // CloseIndependentRenderingStream() or during destruction.
      if (!OpenIndependentRenderingStream()) {
        ReportError(TrackError::kIndependentOpenFailed);
        return;
      }
    }

    rendering_stream_->Start(this);
  }

  void StopIndependentRenderingStream() {
    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("audio"),
                 "MixTrack::StopIndependentRenderingStream", "this",
                 static_cast<void*>(this));
    DCHECK(audio_source_callback_);
    if (rendering_stream_) {
      rendering_stream_->Stop();
    }
  }

  void CloseIndependentRenderingStream() {
    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("audio"),
                 "MixTrack::CloseIndependentRenderingStream", "this",
                 static_cast<void*>(this));
    DCHECK(!audio_source_callback_);
    // Closes the stream.
    rendering_stream_.reset();
  }

  void ReportError(TrackError error) {
    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("audio"), "MixTrack::ReportError",
                 "this", static_cast<void*>(this));
    DCHECK(audio_source_callback_);
    DCHECK_NE(error, TrackError::kNone);
    LOG(ERROR) << "MixableOutputStream: " << TrackErrorToString(error);
    error_ = error;
    audio_source_callback_->OnError(ErrorType::kUnknown);
  }

  void OnDeviceChange() {
    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("audio"), "MixTrack::OnDeviceChange",
                 "this", static_cast<void*>(this));
    DCHECK(!on_device_change_callback_.is_null());
    std::move(on_device_change_callback_).Run();
  }

 private:
  // media::AudioOutputStream::AudioSourceCallback implementation to intercept
  // error reporting during independent playback.
  int OnMoreData(base::TimeDelta delay,
                 base::TimeTicks delay_timestamp,
                 const media::AudioGlitchInfo& glitch_info,
                 media::AudioBus* dest) final {
    DCHECK(audio_source_callback_);
    return audio_source_callback_->OnMoreData(delay, delay_timestamp,
                                              glitch_info, dest);
  }

  void OnError(ErrorType type) final {
    // Device change events are intercepted by the underlying physical streams.
    DCHECK_EQ(type, ErrorType::kUnknown);
    ReportError(TrackError::kIndependentPlaybackFailed);
  }

  double volume_ = kDefaultVolume;

  const raw_ptr<OutputDeviceMixerImpl> mixer_;

  // Callback to notify the audio output client of the device change. Note that
  // all the device change events are initially routed to MixerManager which
  // does the centralized processing and notifies each mixer via
  // OutputDeviceMixer::ProcessDeviceChange(). There OutputDeviceMixerImpl does
  // the cleanup and then dispatches the device change event to all its clients
  // via |on_device_change_callback_| of its MixTracks.
  base::OnceClosure on_device_change_callback_;

  // Callback to request the audio output data from the client.
  raw_ptr<media::AudioOutputStream::AudioSourceCallback>
      audio_source_callback_ = nullptr;

  // Delivers the audio output into MixingGraph, to be mixed for the reference
  // signal playback.
  const std::unique_ptr<MixingGraph::Input> graph_input_;

  // When non-nullptr, points to an open physical output stream used to render
  // the audio output independently when mixing is not required.
  std::unique_ptr<media::AudioOutputStream, StreamAutoClose> rendering_stream_ =
      nullptr;

  TrackError error_ = TrackError::kNone;
};

// A proxy which represents MixTrack as media::AudioOutputStream.
class OutputDeviceMixerImpl::MixableOutputStream final
    : public media::AudioOutputStream {
 public:
  MixableOutputStream(base::WeakPtr<OutputDeviceMixerImpl> mixer,
                      MixTrack* mix_track)
      : mixer_(std::move(mixer)), mix_track_(mix_track) {}

  MixableOutputStream(const MixableOutputStream&) = delete;
  MixableOutputStream& operator=(const MixableOutputStream&) = delete;

  ~MixableOutputStream() final {
    DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  }

  // AudioOutputStream interface.
  bool Open() final {
    DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
    if (!mixer_) {
      LOG(ERROR) << "Stream start failed: device changed";
      return false;
    }
    return mixer_->OpenStream(mix_track_);
  }

  void Start(AudioSourceCallback* callback) final {
    DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
    DCHECK(callback);
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
        TRACE_DISABLED_BY_DEFAULT("audio"), "MixableOutputStream::IsPlaying",
        this, "device_id", mixer_ ? mixer_->device_id() : "device changed");
    if (!mixer_) {
      LOG(ERROR) << "Stream start failed: device changed";
      callback->OnError(ErrorType::kDeviceChange);
      return;
    }
    mixer_->StartStream(mix_track_, callback);
  }

  void Stop() final {
    DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
    TRACE_EVENT_NESTABLE_ASYNC_END0(TRACE_DISABLED_BY_DEFAULT("audio"),
                                    "MixableOutputStream::IsPlaying", this);
    if (!mixer_)
      return;
    mixer_->StopStream(mix_track_);
  }

  void SetVolume(double volume) final {
    DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
    if (!mixer_)
      return;
    mix_track_->SetVolume(volume);
  }

  void GetVolume(double* volume) final {
    DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
    DCHECK(volume);
    if (!mixer_)
      return;
    *volume = mix_track_->GetVolume();
  }

  void Close() final {
    DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
    if (mixer_) {
      // `CloseStream` destroys `mix_track_`. Use `ExtractAsDangling` to clear
      // the underlying pointer and return another raw_ptr instance that is
      // allowed to dangle.
      mixer_->CloseStream(mix_track_.ExtractAsDangling());
    }

    // To match the typical usage pattern of AudioOutputStream.
    delete this;
  }

  void Flush() final {}

 private:
  SEQUENCE_CHECKER(owning_sequence_);
  // OutputDeviceMixerImpl will release all the resources and invalidate its
  // weak pointers in OutputDeviceMixer::ProcessDeviceChange(). After that
  // MixableOutputStream becomes a no-op.
  base::WeakPtr<OutputDeviceMixerImpl> const mixer_
      GUARDED_BY_CONTEXT(owning_sequence_);
  raw_ptr<MixTrack> mix_track_;  // Valid only when |mixer_| is valid.
};

OutputDeviceMixerImpl::OutputDeviceMixerImpl(
    const std::string& device_id,
    const media::AudioParameters& output_params,
    MixingGraph::CreateCallback create_mixing_graph_callback,
    CreateStreamCallback create_stream_callback)
    : OutputDeviceMixer(device_id),
      create_stream_callback_(std::move(create_stream_callback)),
      mixing_graph_output_params_(output_params),
      mixing_graph_(std::move(create_mixing_graph_callback)
                        .Run(output_params,
                             base::BindRepeating(
                                 &OutputDeviceMixerImpl::BroadcastToListeners,
                                 base::Unretained(this)),
                             base::BindRepeating(
                                 &OutputDeviceMixerImpl::OnMixingGraphError,
                                 base::Unretained(this)))) {
  DCHECK(mixing_graph_output_params_.IsValid());
  DCHECK_EQ(mixing_graph_output_params_.format(),
            media::AudioParameters::AUDIO_PCM_LOW_LATENCY);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(TRACE_DISABLED_BY_DEFAULT("audio"),
                                    "OutputDeviceMixerImpl", this, "device_id",
                                    device_id);
}

OutputDeviceMixerImpl::~OutputDeviceMixerImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  DCHECK(active_tracks_.empty());
  DCHECK(!HasListeners());
  DCHECK(!MixingInProgress());
  DCHECK(!mixing_graph_output_stream_);

  TRACE_EVENT_NESTABLE_ASYNC_END0(TRACE_DISABLED_BY_DEFAULT("audio"),
                                  "OutputDeviceMixerImpl", this);
}

media::AudioOutputStream* OutputDeviceMixerImpl::MakeMixableStream(
    const media::AudioParameters& params,
    base::OnceClosure on_device_change_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  NON_REENTRANT_SCOPE(reentrancy_checker_);
  if (!(params.IsValid() &&
        params.format() == media::AudioParameters::AUDIO_PCM_LOW_LATENCY)) {
    LOG(ERROR) << "Invalid output stream patameters for device [" << device_id()
               << "], parameters: " << params.AsHumanReadableString();
    return nullptr;
  }

  auto mix_track =
      std::make_unique<MixTrack>(this, mixing_graph_->CreateInput(params),
                                 std::move(on_device_change_callback));

  media::AudioOutputStream* mixable_stream =
      new MixableOutputStream(weak_factory_.GetWeakPtr(), mix_track.get());
  mix_tracks_.insert(std::move(mix_track));
  return mixable_stream;
}

void OutputDeviceMixerImpl::ProcessDeviceChange() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  NON_REENTRANT_SCOPE(reentrancy_checker_);

#if DCHECK_IS_ON()
  DCHECK(!device_changed_);
  device_changed_ = true;
#endif

  // Make all MixableOutputStream instances no-op.
  weak_factory_.InvalidateWeakPtrs();

  // Stop and close all audio playback.
  if (MixingInProgress()) {
    StopMixingGraphPlayback(MixingError::kNone);
  } else {
    for (MixTrack* mix_track : active_tracks_) {
      mix_track->StopIndependentRenderingStream();
    }
    // In case it was opened when listeners came:
    mixing_graph_output_stream_.reset();
  }

  // For consistency.
  for (MixTrack* mix_track : active_tracks_)
    mix_track->SetSource(nullptr);

  active_tracks_.clear();

  // Close independent rendering streams: the clients will want to restore
  // active playback in mix_track->OnDeviceChange() calls below; we don't want
  // to exceed the limit of simultaneously open output streams when they do so.
  for (auto&& mix_track : mix_tracks_)
    mix_track->CloseIndependentRenderingStream();

  {
    base::AutoLock scoped_lock(listener_lock_);
    listeners_.clear();
  }

  // Notify MixableOutputStream users of the device change. Normally they should
  // close the current stream they are holding to, create/open a new one and
  // resume the playback. We already released all the resources; closing a
  // MixableOutputStream will be a no-op since weak pointers to |this| have just
  // been invalidated.
  for (auto&& mix_track : mix_tracks_)
    mix_track->OnDeviceChange();
}

void OutputDeviceMixerImpl::StartListening(Listener* listener) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  NON_REENTRANT_SCOPE(reentrancy_checker_);
#if DCHECK_IS_ON()
  DCHECK(!device_changed_);
#endif
  DVLOG(1) << "Reference output listener added for device [" << device_id()
           << "]";

  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("audio"),
               "OutputDeviceMixerImpl::StartListening", "device_id",
               device_id());

  // A new listener came: cancel scheduled switch to independent playback.
  switch_to_unmixed_playback_delay_timer_.Stop();
  {
    base::AutoLock scoped_lock(listener_lock_);
    DCHECK(listeners_.find(listener) == listeners_.end());
    listeners_.insert(listener);
    if (MixingInProgress()) {
      DCHECK(mixing_graph_output_stream_);  // We are mixing.
      return;
    }
  }
  // Since we now have listeners, warm-up |mixing_graph_output_stream_|.
  EnsureMixingGraphOutputStreamOpen();

  // Start reference playback only if at least one audio stream is playing.
  if (active_tracks_.empty())
    return;

  for (MixTrack* mix_track : active_tracks_)
    mix_track->StopIndependentRenderingStream();
  StartMixingGraphPlayback();
  // Note that if StartMixingGraphPlayback() failed, no audio will be playing
  // and each client of a playing MixableOutputStream will receive OnError()
  // callback call.
}

void OutputDeviceMixerImpl::StopListening(Listener* listener) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
#if DCHECK_IS_ON()
  DCHECK(!device_changed_);
#endif
  DVLOG(1) << "Reference output listener removed for device [" << device_id()
           << "]";

  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("audio"),
               "OutputDeviceMixerImpl::StopListening", "device_id",
               device_id());

  {
    base::AutoLock scoped_lock(listener_lock_);
    auto iter = listeners_.find(listener);
    CHECK(iter != listeners_.end(), base::NotFatalUntil::M130);
    listeners_.erase(iter);
  }

  if (HasListeners()) {
    // We still have some listeners left, so no need to do anything.
    return;
  }

  if (!MixingInProgress()) {
    // There is no mixing in progress and no more listeners left: closing
    // the warmed up stream.
    mixing_graph_output_stream_.reset();
    return;
  }

  DCHECK(mixing_graph_output_stream_);  // We are mixing.

  if (active_tracks_.empty()) {
    // There is no actual playback: we were just sending silence to the
    // listener as a reference.
    StopMixingGraphPlayback(MixingError::kNone);
  } else {
    // No listeners left, and we are playing via the mixing graph. Schedule
    // switching to independent playback.
    switch_to_unmixed_playback_delay_timer_.Start(
        FROM_HERE, kSwitchToUnmixedPlaybackDelay, this,
        &OutputDeviceMixerImpl::SwitchToUnmixedPlaybackTimerHelper);
  }
}

bool OutputDeviceMixerImpl::OpenStream(MixTrack* mix_track) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
#if DCHECK_IS_ON()
  DCHECK(!device_changed_);
#endif
  DCHECK(mix_track);

  // Defer opening the physical output stream, as the audio mix is requested.
  // The physical stream will be open on start if needed.
  if (HasListeners())
    return true;

  return mix_track->OpenIndependentRenderingStream();
}

void OutputDeviceMixerImpl::StartStream(
    MixTrack* mix_track,
    media::AudioOutputStream::AudioSourceCallback* callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  NON_REENTRANT_SCOPE(reentrancy_checker_);
#if DCHECK_IS_ON()
  DCHECK(!device_changed_);
#endif
  DCHECK(mix_track);
  DCHECK(callback);
  DCHECK(!base::Contains(active_tracks_, mix_track));

  TRACE_EVENT2(TRACE_DISABLED_BY_DEFAULT("audio"),
               "OutputDeviceMixerImpl::StartStream", "device_id", device_id(),
               "mix_track", static_cast<void*>(mix_track));

  mix_track->SetSource(callback);
  active_tracks_.emplace(mix_track);

  if (MixingInProgress()) {
    // We are playing all audio as a |mixing_graph_| output.
    mix_track->StartProvidingAudioToMixingGraph();
  } else if (HasListeners()) {
    // Either we are starting the first active stream, or the previous switch to
    // playing via the mixing graph failed because the its output stream failed
    // to open. In any case, none of the active streams are playing individually
    // at this point.
    EnsureMixingGraphOutputStreamOpen();
    StartMixingGraphPlayback();
  } else {
    // No reference signal is requested.
    mix_track->StartIndependentRenderingStream();
  }
}

void OutputDeviceMixerImpl::StopStream(MixTrack* mix_track) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  NON_REENTRANT_SCOPE(reentrancy_checker_);
#if DCHECK_IS_ON()
  DCHECK(!device_changed_);
#endif
  DCHECK(mix_track);
  if (!base::Contains(active_tracks_, mix_track)) {
    // MixableOutputStream::Stop() can be called multiple times, even if the
    // stream has not been started. See media::AudioOutputStream documentation.
    return;
  }

  TRACE_EVENT2(TRACE_DISABLED_BY_DEFAULT("audio"),
               "OutputDeviceMixerImpl::StopStream", "device_id", device_id(),
               "mix_track", static_cast<void*>(mix_track));

  active_tracks_.erase(mix_track);

  if (MixingInProgress()) {
    // We are playing all audio as a |mixing_graph_| output.
    mix_track->StopProvidingAudioToMixingGraph();

    if (!HasListeners() && active_tracks_.empty()) {
      // All listeners are gone, which means a switch to an independent playback
      // is scheduled. But since we have no active tracks, we can stop mixing
      // immediately.
      DCHECK(switch_to_unmixed_playback_delay_timer_.IsRunning());
      StopMixingGraphPlayback(MixingError::kNone);
    }

    // Note: if there are listeners, we do not stop the reference playback even
    // if there are no active mix members. This way the echo canceller will be
    // in a consistent state when the playback is activated again. Drawback: we
    // keep playing silent audio. An example would be some occasional
    // notification sounds and no other audio output: we start the mixing
    // playback at the first notification, and stop it only when the echo
    // cancellation session is finished (i.e. all the listeners are gone).
  } else {
    mix_track->StopIndependentRenderingStream();
  }
  mix_track->SetSource(nullptr);
}

void OutputDeviceMixerImpl::CloseStream(MixTrack* mix_track) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  NON_REENTRANT_SCOPE(reentrancy_checker_);
#if DCHECK_IS_ON()
  DCHECK(!device_changed_);
#endif
  DCHECK(mix_track);
  DCHECK(!base::Contains(active_tracks_, mix_track));

  auto iter = mix_tracks_.find(mix_track);
  CHECK(iter != mix_tracks_.end(), base::NotFatalUntil::M130);

  mix_tracks_.erase(iter);
}

media::AudioOutputStream* OutputDeviceMixerImpl::CreateAndOpenDeviceStream(
    const media::AudioParameters& params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  DCHECK(params.IsValid());
  DCHECK_EQ(params.format(), media::AudioParameters::AUDIO_PCM_LOW_LATENCY);

  media::AudioOutputStream* stream =
      create_stream_callback_.Run(device_id(), params);
  if (!stream)
    return nullptr;

  if (!stream->Open()) {
    LOG(ERROR) << "Failed to open stream";
    stream->Close();
    return nullptr;
  }

  return stream;
}

void OutputDeviceMixerImpl::BroadcastToListeners(
    const media::AudioBus& audio_bus,
    base::TimeDelta delay) {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("audio"),
               "OutputDeviceMixerImpl::BroadcastToListeners", "delay ms",
               delay.InMilliseconds());

  base::AutoLock scoped_lock(listener_lock_);
  for (Listener* listener : listeners_) {
    listener->OnPlayoutData(audio_bus,
                            mixing_graph_output_params_.sample_rate(), delay);
  }
}

// Processes errors coming from |mixing_graph_| during mixed playback.
void OutputDeviceMixerImpl::OnMixingGraphError(ErrorType error) {
  // |mixing_graph_output_stream_| guarantees this.
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  // Device change events are intercepted by the underlying physical stream.
  DCHECK_EQ(error, ErrorType::kUnknown);
  // This call is expected only during mixed playback.
  DCHECK(mixing_graph_output_stream_);

  StopMixingGraphPlayback(MixingError::kPlaybackFailed);
}

bool OutputDeviceMixerImpl::HasListeners() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  return TS_UNCHECKED_READ(listeners_).size();
}

void OutputDeviceMixerImpl::EnsureMixingGraphOutputStreamOpen() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  DCHECK(HasListeners());
  if (mixing_graph_output_stream_)
    return;
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("audio"),
               "OutputDeviceMixerImpl::EnsureMixingGraphOutputStreamOpen",
               "device_id", device_id());
  mixing_graph_output_stream_.reset(
      CreateAndOpenDeviceStream(mixing_graph_output_params_));
}

// Expects |mixing_graph_output_stream_| to be already open; if not - this is
// interpreted as a failure.
void OutputDeviceMixerImpl::StartMixingGraphPlayback() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(TRACE_DISABLED_BY_DEFAULT("audio"),
                                    "OutputDeviceMixerImpl mixing", this,
                                    "device_id", device_id());
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("audio"),
               "OutputDeviceMixerImpl::StartMixingGraphPlayback", "device_id",
               device_id());

  if (!mixing_graph_output_stream_) {
    StopMixingGraphPlayback(MixingError::kOpenFailed);
    return;
  }

  DCHECK(!mixing_in_progress_);
  mixing_in_progress_ = true;

  for (MixTrack* mix_track : active_tracks_)
    mix_track->StartProvidingAudioToMixingGraph();

  mixing_graph_output_stream_->Start(mixing_graph_.get());

  DVLOG(1) << " Mixing started for device [" << device_id() << "]";
}

void OutputDeviceMixerImpl::StopMixingGraphPlayback(MixingError error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);

  if (MixingInProgress()) {
    // Mixing was in progress, we should stop it.
    DCHECK(mixing_graph_output_stream_);
    DCHECK_NE(error, MixingError::kOpenFailed);

    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("audio"),
                 "OutputDeviceMixerImpl::StopMixingGraphPlayback", "device_id",
                 device_id());

    switch_to_unmixed_playback_delay_timer_.Stop();

    mixing_graph_output_stream_->Stop();
    mixing_graph_output_stream_.reset();  // Auto-close the stream.

    mixing_in_progress_ = false;

    DVLOG(1) << " Mixing stopped for device [" << device_id() << "]";

    for (MixTrack* mix_track : active_tracks_)
      mix_track->StopProvidingAudioToMixingGraph();

    TRACE_EVENT_NESTABLE_ASYNC_END0(TRACE_DISABLED_BY_DEFAULT("audio"),
                                    "OutputDeviceMixerImpl mixing", this);
  }

  DCHECK(!mixing_graph_output_stream_);

  if (error != MixingError::kNone) {
    LOG(ERROR) << ErrorToString(error) << " for device [" << device_id() << "]";
    TrackError track_error = (error == MixingError::kOpenFailed)
                                 ? TrackError::kMixedOpenFailed
                                 : TrackError::kMixedPlaybackFailed;
    for (MixTrack* mix_track : active_tracks_)
      mix_track->ReportError(track_error);
  }

  base::UmaHistogramEnumeration(
      "Media.Audio.OutputDeviceMixer.MixedPlaybackStatus", error);
}

void OutputDeviceMixerImpl::SwitchToUnmixedPlaybackTimerHelper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  NON_REENTRANT_SCOPE(reentrancy_checker_);
  DCHECK(MixingInProgress());
#if DCHECK_IS_ON()
  DCHECK(!device_changed_);
#endif
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("audio"),
               "OutputDeviceMixerImpl::SwitchToUnmixedPlaybackTimerHelper",
               "device_id", device_id());
  StopMixingGraphPlayback(MixingError::kNone);
  for (MixTrack* mix_track : active_tracks_)
    mix_track->StartIndependentRenderingStream();
}

// static
const char* OutputDeviceMixerImpl::ErrorToString(MixingError error) {
  switch (error) {
    case MixingError::kOpenFailed:
      return "Failed to open mixing rendering stream";
    case MixingError::kPlaybackFailed:
      return "Error during mixed playback";
    default:
      NOTREACHED_IN_MIGRATION();
      return "No error";
  }
}

}  // namespace audio
