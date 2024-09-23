// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/track_audio_renderer.h"

#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/synchronization/lock.h"
#include "base/trace_event/trace_event.h"
#include "media/audio/audio_sink_parameters.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_latency.h"
#include "media/base/audio_shifter.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_track.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_track.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
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

// Translates |num_samples_rendered| into a TimeDelta duration and adds it to
// |prior_elapsed_render_time|.
base::TimeDelta ComputeTotalElapsedRenderTime(
    base::TimeDelta prior_elapsed_render_time,
    int64_t num_samples_rendered,
    int sample_rate) {
  return prior_elapsed_render_time +
         base::Microseconds(num_samples_rendered *
                            base::Time::kMicrosecondsPerSecond / sample_rate);
}

WebLocalFrame* ToWebLocalFrame(LocalFrame* frame) {
  if (!frame)
    return nullptr;

  return static_cast<WebLocalFrame*>(WebFrame::FromCoreFrame(frame));
}

bool RequiresSinkReconfig(const media::AudioParameters& old_format,
                          const media::AudioParameters& new_format) {
  // Always favor |new_format| if our current params are invalid. This avoids
  // the edge case where |current_params| is valid except for 0
  // frames_per_buffer(), and never gets replaced by an almost identical
  // |new_format| with a valid frames_per_buffer().
  if (!old_format.IsValid())
    return true;

  // Ignore frames_per_buffer(), since the AudioRendererSink and the
  // AudioShifter handle those variations adequately.
  media::AudioParameters new_format_copy = new_format;
  new_format_copy.set_frames_per_buffer(old_format.frames_per_buffer());

  return !old_format.Equals(new_format_copy);
}

}  // namespace

TrackAudioRenderer::PendingData::PendingData(const media::AudioBus& audio_bus,
                                             base::TimeTicks ref_time)
    : reference_time(ref_time),
      audio(media::AudioBus::Create(audio_bus.channels(), audio_bus.frames())) {
  audio_bus.CopyTo(audio.get());
}

TrackAudioRenderer::PendingReconfig::PendingReconfig(
    const media::AudioParameters& format,
    int reconfig_number)
    : reconfig_number(reconfig_number), format(format) {}

// media::AudioRendererSink::RenderCallback implementation
int TrackAudioRenderer::Render(base::TimeDelta delay,
                               base::TimeTicks delay_timestamp,
                               const media::AudioGlitchInfo& glitch_info,
                               media::AudioBus* audio_bus) {
  TRACE_EVENT("audio", "TrackAudioRenderer::Render", "playout_delay (ms)",
              delay.InMillisecondsF(), "delay_timestamp (ms)",
              (delay_timestamp - base::TimeTicks()).InMillisecondsF());
  base::AutoLock auto_lock(thread_lock_);

  if (!audio_shifter_) {
    audio_bus->Zero();
    return 0;
  }

  const base::TimeTicks playout_time = delay_timestamp + delay;
  DVLOG(2) << "Pulling audio out of shifter to be played "
           << delay.InMilliseconds() << " ms from now.";
  audio_shifter_->Pull(audio_bus, playout_time);
  num_samples_rendered_ += audio_bus->frames();
  return audio_bus->frames();
}

void TrackAudioRenderer::OnRenderErrorCrossThread() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  on_render_error_callback_.Run();
}

void TrackAudioRenderer::OnRenderError() {
  DCHECK(on_render_error_callback_);

  PostCrossThreadTask(
      *task_runner_, FROM_HERE,
      CrossThreadBindOnce(&TrackAudioRenderer::OnRenderErrorCrossThread,
                          WrapRefCounted(this)));
}

// WebMediaStreamAudioSink implementation
void TrackAudioRenderer::OnData(const media::AudioBus& audio_bus,
                                base::TimeTicks reference_time) {
  TRACE_EVENT("audio", "TrackAudioRenderer::OnData", "capture_time (ms)",
              (reference_time - base::TimeTicks()).InMillisecondsF(),
              "capture_delay (ms)",
              (base::TimeTicks::Now() - reference_time).InMillisecondsF());

  base::AutoLock auto_lock(thread_lock_);

  // There is a pending ReconfigureSink() call. Copy |audio_bus| so it can be
  // pushed in to the |audio_shifter_| (or dropped later).
  if (!pending_reconfigs_.empty()) {
    // Copies |audio_bus| internally.
    pending_reconfigs_.back().data.emplace_back(audio_bus, reference_time);
    return;
  }

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
  PushDataIntoShifter_Locked(std::move(audio_data), reference_time);
}

void TrackAudioRenderer::OnSetFormat(const media::AudioParameters& params) {
  DVLOG(1) << "TrackAudioRenderer::OnSetFormat: "
           << params.AsHumanReadableString();

  // Don't attempt call ReconfigureSink() if the |last_reconfig_format_|
  // is compatible (e.g. identical, or varies only by frames_per_buffer()).
  if (!RequiresSinkReconfig(last_reconfig_format_, params))
    return;

  int reconfig_number;
  {
    base::AutoLock lock(thread_lock_);
    // Keep track of how many ReconfigureSink() calls we have made. This allows
    // us to drop all but the latest ReconfigureSink() calls on the main thread.
    reconfig_number = ++sink_reconfig_count_;

    // As long as there is an entry in |pending_reconfigs_|, we save data
    // instead of dropping it, or pushing it into |audio_shifter_|. This queue
    // entry is popped in ReconfigureSink().
    pending_reconfigs_.push_back(PendingReconfig(params, reconfig_number));
  }

  // Post a task on the main render thread to reconfigure the |sink_| with the
  // new format.
  PostCrossThreadTask(
      *task_runner_, FROM_HERE,
      CrossThreadBindOnce(&TrackAudioRenderer::ReconfigureSink,
                          WrapRefCounted(this), params, reconfig_number));

  last_reconfig_format_ = params;
}

TrackAudioRenderer::TrackAudioRenderer(
    MediaStreamComponent* audio_component,
    LocalFrame& playout_frame,
    const String& device_id,
    base::RepeatingClosure on_render_error_callback)
    : audio_component_(audio_component),
      playout_frame_(playout_frame),
      task_runner_(
          playout_frame.GetTaskRunner(blink::TaskType::kInternalMedia)),
      on_render_error_callback_(std::move(on_render_error_callback)),
      output_device_id_(device_id) {
  DCHECK(MediaStreamAudioTrack::From(audio_component_.Get()));
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

  // We get audio data from |audio_component_|...
  WebMediaStreamAudioSink::AddToAudioTrack(
      this, WebMediaStreamTrack(audio_component_.Get()));
  // ...and |sink_| will get audio data from us.
  DCHECK(!sink_);
  sink_ = Platform::Current()->NewAudioRendererSink(
      WebAudioDeviceSourceType::kNonRtcAudioTrack,
      ToWebLocalFrame(playout_frame_),
      {base::UnguessableToken(), output_device_id_.Utf8()});

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

  sink_started_ = false;

  // Ensure that the capturer stops feeding us with captured audio.
  WebMediaStreamAudioSink::RemoveFromAudioTrack(
      this, WebMediaStreamTrack(audio_component_.Get()));
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
  HaltAudioFlow_Locked();
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

void TrackAudioRenderer::SwitchOutputDevice(
    const std::string& device_id,
    media::OutputDeviceStatusCB callback) {
  DVLOG(1) << "TrackAudioRenderer::SwitchOutputDevice()";
  DCHECK(task_runner_->BelongsToCurrentThread());

  {
    base::AutoLock auto_lock(thread_lock_);
    HaltAudioFlow_Locked();
  }

  scoped_refptr<media::AudioRendererSink> new_sink =
      Platform::Current()->NewAudioRendererSink(
          WebAudioDeviceSourceType::kNonRtcAudioTrack,
          ToWebLocalFrame(playout_frame_),
          {base::UnguessableToken(), device_id});

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

void TrackAudioRenderer::MaybeStartSink(bool reconfiguring) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DVLOG(1) << "TrackAudioRenderer::MaybeStartSink()";

  if (!sink_ || !source_params_.IsValid() || !playing_)
    return;

  // Re-create the AudioShifter to drop old audio data and reset to a starting
  // state.  MaybeStartSink() is always called in a situation where either the
  // source or sink has changed somehow and so all of AudioShifter's internal
  // time-sync state is invalid.
  CreateAudioShifter(reconfiguring);

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
      hardware_params.format(), source_params_.channel_layout_config(),
      source_params_.sample_rate(),
      media::AudioLatency::GetRtcBufferSize(
          source_params_.sample_rate(), hardware_params.frames_per_buffer()));
  if (sink_params.channel_layout() == media::CHANNEL_LAYOUT_DISCRETE) {
    DCHECK_LE(source_params_.channels(), 2);
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
}

void TrackAudioRenderer::ReconfigureSink(
    const media::AudioParameters new_format,
    int reconfig_number) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  {
    base::AutoLock lock(thread_lock_);
    DCHECK(!pending_reconfigs_.empty());
    DCHECK_EQ(pending_reconfigs_.front().reconfig_number, reconfig_number);

    // ReconfigureSink() is only posted by OnSetFormat() when an incoming format
    // is incompatible with |last_reconfig_format_|. A mismatch between
    // |reconfig_number| and |sink_reconfig_count_| means there is at least
    // one more pending ReconfigureSink() call, which is definitively
    // incompatible with |new_format|. If so, ignore this reconfiguration, to
    // avoid creating a sink which would be immediately destroyed by the next
    // ReconfigureSink() call.
    if (reconfig_number != sink_reconfig_count_) {
      // Drop any pending data for this |reconfig_number|, as we won't have
      // an |audio_shifter_| or a |sink_| configured to ingest this data.
      pending_reconfigs_.pop_front();
      return;
    }

    // The |new_format| is compatible with the existing one. Skip this
    // reconfiguration.
    if (!RequiresSinkReconfig(source_params_, new_format)) {
      // Push pending data into |audio_shifter_|, if we have one, or clear
      // the entry corresponding to this |reconfig_number|.
      if (audio_shifter_)
        ConsumePendingReconfigsFront_Locked();
      else
        pending_reconfigs_.pop_front();

      return;
    }

    // If we need to reconfigure, drop all existing |audio_shifter_| data, as it
    // won't be compatible with the new shifter and data in
    // |pending_reconfigs_.front()|.
    if (audio_shifter_)
      HaltAudioFlow_Locked();
  }

  source_params_ = new_format;

  if (!sink_)
    return;  // TrackAudioRenderer has not yet been started.

  // Stop |sink_| and re-create a new one to be initialized with different audio
  // parameters.  Then, invoke MaybeStartSink() to restart everything again.
  sink_->Stop();
  sink_started_ = false;
  sink_ = Platform::Current()->NewAudioRendererSink(
      WebAudioDeviceSourceType::kNonRtcAudioTrack,
      ToWebLocalFrame(playout_frame_),
      {base::UnguessableToken(), output_device_id_.Utf8()});
  MaybeStartSink(/*reconfiguring=*/true);

  {
    base::AutoLock lock(thread_lock_);
    // We may have never created |audio_shifter_| (e.g. if the sink isn't
    // playing). Clear the corresponding |pending_reconfigs_| entry, so
    // we start dropping incoming data in OnData().
    if (!audio_shifter_)
      pending_reconfigs_.pop_front();
  }
}

void TrackAudioRenderer::CreateAudioShifter(bool reconfiguring) {
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
      base::Seconds(5), base::Milliseconds(20), base::Seconds(20),
      source_params_.sample_rate(), source_params_.channels());

  base::AutoLock auto_lock(thread_lock_);
  audio_shifter_.reset(new_shifter);

  // There might be pending data that needs to be pushed into |audio_shifter_|.
  if (reconfiguring)
    ConsumePendingReconfigsFront_Locked();
}

void TrackAudioRenderer::HaltAudioFlow_Locked() {
  thread_lock_.AssertAcquired();

  audio_shifter_.reset();

  if (source_params_.IsValid()) {
    prior_elapsed_render_time_ = ComputeTotalElapsedRenderTime(
        prior_elapsed_render_time_, num_samples_rendered_,
        source_params_.sample_rate());
    num_samples_rendered_ = 0;
  }
}

void TrackAudioRenderer::ConsumePendingReconfigsFront_Locked() {
  thread_lock_.AssertAcquired();
  DCHECK(audio_shifter_);

  PendingReconfig& current_reconfig = pending_reconfigs_.front();
  DCHECK(!RequiresSinkReconfig(source_params_, current_reconfig.format));

  auto& pending_data = current_reconfig.data;
  for (auto& data : pending_data)
    PushDataIntoShifter_Locked(std::move(data.audio), data.reference_time);

  // Once |pending_reconfigs_| is empty, new data will be pushed directly
  // into |audio_shifter_|. If it isn't empty, there is another
  // ReconfigureSink() in flight.
  pending_reconfigs_.pop_front();
}

void TrackAudioRenderer::PushDataIntoShifter_Locked(
    std::unique_ptr<media::AudioBus> data,
    base::TimeTicks reference_time) {
  thread_lock_.AssertAcquired();
  DCHECK(audio_shifter_);
  total_frames_pushed_for_testing_ += data->frames();
  audio_shifter_->Push(std::move(data), reference_time);
}

int TrackAudioRenderer::TotalFramesPushedForTesting() const {
  base::AutoLock auto_lock(thread_lock_);
  return total_frames_pushed_for_testing_;
}

int TrackAudioRenderer::FramesInAudioShifterForTesting() const {
  base::AutoLock auto_lock(thread_lock_);
  return audio_shifter_ ? audio_shifter_->frames_pushed_for_testing() : 0;
}

}  // namespace blink
