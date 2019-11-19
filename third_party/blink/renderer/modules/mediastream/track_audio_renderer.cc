// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/track_audio_renderer.h"

#include <utility>

#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "media/audio/audio_sink_parameters.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_latency.h"
#include "media/base/audio_shifter.h"
#include "third_party/blink/public/platform/modules/mediastream/media_stream_audio_track.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_local_frame_wrapper.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace WTF {

template <>
struct CrossThreadCopier<media::AudioParameters> {
  STATIC_ONLY(CrossThreadCopier);
  using Type = media::AudioParameters;
  static Type Copy(Type pointer) { return pointer; }
};

}  // namespace WTF

namespace blink {

namespace {

enum LocalRendererSinkStates {
  kSinkStarted = 0,
  kSinkNeverStarted,
  kSinkStatesMax  // Must always be last!
};

// Translates |num_samples_rendered| into a TimeDelta duration and adds it to
// |prior_elapsed_render_time|.
base::TimeDelta ComputeTotalElapsedRenderTime(
    base::TimeDelta prior_elapsed_render_time,
    int64_t num_samples_rendered,
    int sample_rate) {
  return prior_elapsed_render_time +
         base::TimeDelta::FromMicroseconds(num_samples_rendered *
                                           base::Time::kMicrosecondsPerSecond /
                                           sample_rate);
}

}  // namespace

// media::AudioRendererSink::RenderCallback implementation
int TrackAudioRenderer::Render(base::TimeDelta delay,
                               base::TimeTicks delay_timestamp,
                               int prior_frames_skipped,
                               media::AudioBus* audio_bus) {
  TRACE_EVENT2("audio", "TrackAudioRenderer::Render", "delay (ms)",
               delay.InMillisecondsF(), "delay_timestamp (ms)",
               (delay_timestamp - base::TimeTicks()).InMillisecondsF());
  base::AutoLock auto_lock(thread_lock_);

  if (!audio_shifter_) {
    audio_bus->Zero();
    return 0;
  }

  // TODO(miu): Plumbing is needed to determine the actual playout timestamp
  // of the audio, instead of just snapshotting TimeTicks::Now(), for proper
  // audio/video sync. https://crbug.com/335335
  const base::TimeTicks playout_time = base::TimeTicks::Now() + delay;
  DVLOG(2) << "Pulling audio out of shifter to be played "
           << delay.InMilliseconds() << " ms from now.";
  audio_shifter_->Pull(audio_bus, playout_time);
  num_samples_rendered_ += audio_bus->frames();
  return audio_bus->frames();
}

void TrackAudioRenderer::OnRenderError() {
  NOTIMPLEMENTED();
}

// WebMediaStreamAudioSink implementation
void TrackAudioRenderer::OnData(const media::AudioBus& audio_bus,
                                base::TimeTicks reference_time) {
  DCHECK(!reference_time.is_null());

  TRACE_EVENT1("audio", "TrackAudioRenderer::OnData", "reference time (ms)",
               (reference_time - base::TimeTicks()).InMillisecondsF());

  base::AutoLock auto_lock(thread_lock_);
  if (!audio_shifter_)
    return;

  std::unique_ptr<media::AudioBus> audio_data(
      media::AudioBus::Create(audio_bus.channels(), audio_bus.frames()));
  audio_bus.CopyTo(audio_data.get());
  // Note: For remote audio sources, |reference_time| is the local playout time,
  // the ideal point-in-time at which the first audio sample should be played
  // out in the future.  For local sources, |reference_time| is the
  // point-in-time at which the first audio sample was captured in the past.  In
  // either case, AudioShifter will auto-detect and do the right thing when
  // audio is pulled from it.
  audio_shifter_->Push(std::move(audio_data), reference_time);
}

void TrackAudioRenderer::OnSetFormat(const media::AudioParameters& params) {
  DVLOG(1) << "TrackAudioRenderer::OnSetFormat: "
           << params.AsHumanReadableString();
  // If the parameters changed, the audio in the AudioShifter is invalid and
  // should be dropped.
  {
    base::AutoLock auto_lock(thread_lock_);
    if (audio_shifter_ &&
        (audio_shifter_->sample_rate() != params.sample_rate() ||
         audio_shifter_->channels() != params.channels())) {
      HaltAudioFlowWhileLockHeld();
    }
  }

  // Post a task on the main render thread to reconfigure the |sink_| with the
  // new format.
  PostCrossThreadTask(*task_runner_, FROM_HERE,
                      CrossThreadBindOnce(&TrackAudioRenderer::ReconfigureSink,
                                          WrapRefCounted(this), params));
}

TrackAudioRenderer::TrackAudioRenderer(const WebMediaStreamTrack& audio_track,
                                       WebLocalFrame* playout_web_frame,
                                       const base::UnguessableToken& session_id,
                                       const String& device_id)
    : audio_track_(audio_track),
      internal_playout_frame_(
          std::make_unique<MediaStreamInternalFrameWrapper>(playout_web_frame)),
      session_id_(session_id),
      task_runner_(
          playout_web_frame->GetTaskRunner(blink::TaskType::kInternalMedia)),
      num_samples_rendered_(0),
      playing_(false),
      output_device_id_(device_id),
      volume_(0.0),
      sink_started_(false) {
  DCHECK(MediaStreamAudioTrack::From(audio_track_));
  DCHECK(task_runner_->BelongsToCurrentThread());
  DVLOG(1) << "TrackAudioRenderer::TrackAudioRenderer()";
}

TrackAudioRenderer::~TrackAudioRenderer() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!sink_);
  DVLOG(1) << "TrackAudioRenderer::~TrackAudioRenderer()";
}

void TrackAudioRenderer::Start() {
  DVLOG(1) << "TrackAudioRenderer::Start()";
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(playing_, false);

  // We get audio data from |audio_track_|...
  WebMediaStreamAudioSink::AddToAudioTrack(this, audio_track_);
  // ...and |sink_| will get audio data from us.
  DCHECK(!sink_);
  sink_ = Platform::Current()->NewAudioRendererSink(
      WebAudioDeviceSourceType::kNonRtcAudioTrack,
      internal_playout_frame_->web_frame(),
      {session_id_, output_device_id_.Utf8()});

  base::AutoLock auto_lock(thread_lock_);
  prior_elapsed_render_time_ = base::TimeDelta();
  num_samples_rendered_ = 0;
}

void TrackAudioRenderer::Stop() {
  DVLOG(1) << "TrackAudioRenderer::Stop()";
  DCHECK(task_runner_->BelongsToCurrentThread());

  Pause();

  // Stop the output audio stream, i.e, stop asking for data to render.
  // It is safer to call Stop() on the |sink_| to clean up the resources even
  // when the |sink_| is never started.
  if (sink_) {
    sink_->Stop();
    sink_ = nullptr;
  }

  if (!sink_started_ && IsLocalRenderer()) {
    UMA_HISTOGRAM_ENUMERATION("Media.LocalRendererSinkStates",
                              kSinkNeverStarted, kSinkStatesMax);
  }
  sink_started_ = false;

  // Ensure that the capturer stops feeding us with captured audio.
  WebMediaStreamAudioSink::RemoveFromAudioTrack(this, audio_track_);
}

void TrackAudioRenderer::Play() {
  DVLOG(1) << "TrackAudioRenderer::Play()";
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (!sink_)
    return;

  playing_ = true;

  MaybeStartSink();
}

void TrackAudioRenderer::Pause() {
  DVLOG(1) << "TrackAudioRenderer::Pause()";
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (!sink_)
    return;

  playing_ = false;

  base::AutoLock auto_lock(thread_lock_);
  HaltAudioFlowWhileLockHeld();
}

void TrackAudioRenderer::SetVolume(float volume) {
  DVLOG(1) << "TrackAudioRenderer::SetVolume(" << volume << ")";
  DCHECK(task_runner_->BelongsToCurrentThread());

  // Cache the volume.  Whenever |sink_| is re-created, call SetVolume() with
  // this cached volume.
  volume_ = volume;
  if (sink_)
    sink_->SetVolume(volume);
}

base::TimeDelta TrackAudioRenderer::GetCurrentRenderTime() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  base::AutoLock auto_lock(thread_lock_);
  if (source_params_.IsValid()) {
    return ComputeTotalElapsedRenderTime(prior_elapsed_render_time_,
                                         num_samples_rendered_,
                                         source_params_.sample_rate());
  }
  return prior_elapsed_render_time_;
}

bool TrackAudioRenderer::IsLocalRenderer() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  return MediaStreamAudioTrack::From(audio_track_)->is_local_track();
}

void TrackAudioRenderer::SwitchOutputDevice(
    const std::string& device_id,
    media::OutputDeviceStatusCB callback) {
  DVLOG(1) << "TrackAudioRenderer::SwitchOutputDevice()";
  DCHECK(task_runner_->BelongsToCurrentThread());

  {
    base::AutoLock auto_lock(thread_lock_);
    HaltAudioFlowWhileLockHeld();
  }

  scoped_refptr<media::AudioRendererSink> new_sink =
      Platform::Current()->NewAudioRendererSink(
          WebAudioDeviceSourceType::kNonRtcAudioTrack,
          internal_playout_frame_->web_frame(), {session_id_, device_id});

  media::OutputDeviceStatus new_sink_status =
      new_sink->GetOutputDeviceInfo().device_status();
  UMA_HISTOGRAM_ENUMERATION("Media.Audio.TrackAudioRenderer.SwitchDeviceStatus",
                            new_sink_status,
                            media::OUTPUT_DEVICE_STATUS_MAX + 1);
  if (new_sink_status != media::OUTPUT_DEVICE_STATUS_OK) {
    new_sink->Stop();
    std::move(callback).Run(new_sink_status);
    return;
  }

  output_device_id_ = String(device_id.data(), device_id.size());
  bool was_sink_started = sink_started_;

  if (sink_)
    sink_->Stop();

  sink_started_ = false;
  sink_ = new_sink;
  if (was_sink_started)
    MaybeStartSink();

  std::move(callback).Run(media::OUTPUT_DEVICE_STATUS_OK);
}

void TrackAudioRenderer::MaybeStartSink() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DVLOG(1) << "TrackAudioRenderer::MaybeStartSink()";

  if (!sink_ || !source_params_.IsValid() || !playing_) {
    return;
  }

  // Re-create the AudioShifter to drop old audio data and reset to a starting
  // state.  MaybeStartSink() is always called in a situation where either the
  // source or sink has changed somehow and so all of AudioShifter's internal
  // time-sync state is invalid.
  CreateAudioShifter();

  if (sink_started_)
    return;

  const media::OutputDeviceInfo& device_info = sink_->GetOutputDeviceInfo();
  UMA_HISTOGRAM_ENUMERATION("Media.Audio.TrackAudioRenderer.DeviceStatus",
                            device_info.device_status(),
                            media::OUTPUT_DEVICE_STATUS_MAX + 1);
  if (device_info.device_status() != media::OUTPUT_DEVICE_STATUS_OK)
    return;

  // Output parameters consist of the same channel layout and sample rate as the
  // source, but having the buffer duration preferred by the hardware.
  const media::AudioParameters& hardware_params = device_info.output_params();
  media::AudioParameters sink_params(
      hardware_params.format(), source_params_.channel_layout(),
      source_params_.sample_rate(),
      media::AudioLatency::GetRtcBufferSize(
          source_params_.sample_rate(), hardware_params.frames_per_buffer()));
  if (sink_params.channel_layout() == media::CHANNEL_LAYOUT_DISCRETE) {
    DCHECK_LE(source_params_.channels(), 2);
    sink_params.set_channels_for_discrete(source_params_.channels());
  }
  DVLOG(1) << ("TrackAudioRenderer::MaybeStartSink() -- Starting sink.  "
               "source_params={")
           << source_params_.AsHumanReadableString() << "}, hardware_params={"
           << hardware_params.AsHumanReadableString() << "}, sink parameters={"
           << sink_params.AsHumanReadableString() << '}';

  // Specify the latency info to be passed to the browser side.
  sink_params.set_latency_tag(Platform::Current()->GetAudioSourceLatencyType(
      WebAudioDeviceSourceType::kNonRtcAudioTrack));

  sink_->Initialize(sink_params, this);
  sink_->Start();
  sink_->SetVolume(volume_);
  sink_->Play();  // Not all the sinks play on start.
  sink_started_ = true;
  if (IsLocalRenderer()) {
    UMA_HISTOGRAM_ENUMERATION("Media.LocalRendererSinkStates", kSinkStarted,
                              kSinkStatesMax);
  }
}

void TrackAudioRenderer::ReconfigureSink(const media::AudioParameters& params) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (source_params_.Equals(params))
    return;
  source_params_ = params;

  if (!sink_)
    return;  // TrackAudioRenderer has not yet been started.

  // Stop |sink_| and re-create a new one to be initialized with different audio
  // parameters.  Then, invoke MaybeStartSink() to restart everything again.
  sink_->Stop();
  sink_started_ = false;
  sink_ = Platform::Current()->NewAudioRendererSink(
      WebAudioDeviceSourceType::kNonRtcAudioTrack,
      internal_playout_frame_->web_frame(),
      {session_id_, output_device_id_.Utf8()});
  MaybeStartSink();
}

void TrackAudioRenderer::CreateAudioShifter() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // Note 1: The max buffer is fairly large to cover the case where
  // remotely-sourced audio is delivered well ahead of its scheduled playout
  // time (e.g., content streaming with a very large end-to-end
  // latency). However, there is no penalty for making it large in the
  // low-latency use cases since AudioShifter will discard data as soon as it is
  // no longer needed.
  //
  // Note 2: The clock accuracy is set to 20ms because clock accuracy is
  // ~15ms on Windows machines without a working high-resolution clock.  See
  // comments in base/time/time.h for details.
  media::AudioShifter* const new_shifter = new media::AudioShifter(
      base::TimeDelta::FromSeconds(5), base::TimeDelta::FromMilliseconds(20),
      base::TimeDelta::FromSeconds(20), source_params_.sample_rate(),
      source_params_.channels());

  base::AutoLock auto_lock(thread_lock_);
  audio_shifter_.reset(new_shifter);
}

void TrackAudioRenderer::HaltAudioFlowWhileLockHeld() {
  thread_lock_.AssertAcquired();

  audio_shifter_.reset();

  if (source_params_.IsValid()) {
    prior_elapsed_render_time_ = ComputeTotalElapsedRenderTime(
        prior_elapsed_render_time_, num_samples_rendered_,
        source_params_.sample_rate());
    num_samples_rendered_ = 0;
  }
}

}  // namespace blink
