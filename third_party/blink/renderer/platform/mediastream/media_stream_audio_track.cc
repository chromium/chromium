// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_track.h"

#include <atomic>
#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_glitch_info.h"
#include "media/base/audio_timestamp_helper.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_audio_sink.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_source.h"
#include "third_party/blink/public/platform/modules/webrtc/webrtc_logging.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_source.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"

namespace blink {

namespace {

constexpr char kTag[] = "MSAT::";

}  // namespace

MediaStreamAudioTrack::MediaStreamAudioTrack(bool is_local_track)
    : MediaStreamTrackPlatform(is_local_track), is_enabled_(1) {
  WebRtcLog(kTag, this, "%s({is_local_track=%s})", __func__,
            (is_local_track ? "true" : "false"));
}

MediaStreamAudioTrack::~MediaStreamAudioTrack() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  WebRtcLog(kTag, this, "%s()", __func__);
  Stop();
}

std::unique_ptr<MediaStreamTrackPlatform>
MediaStreamAudioTrack::CreateFromComponent(
    const MediaStreamComponent* component,
    const String& id) {
  MediaStreamSource* source = component->Source();
  CHECK_EQ(source->GetType(), MediaStreamSource::kTypeAudio);
  return MediaStreamAudioSource::From(source)->CreateMediaStreamAudioTrack(
      id.Utf8());
}

// static
MediaStreamAudioTrack* MediaStreamAudioTrack::From(
    const MediaStreamComponent* component) {
  if (!component ||
      component->GetSourceType() != MediaStreamSource::kTypeAudio) {
    return nullptr;
  }
  return static_cast<MediaStreamAudioTrack*>(component->GetPlatformTrack());
}

void MediaStreamAudioTrack::AddSink(WebMediaStreamAudioSink* sink) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  WebRtcLog(kTag, this, "%s()", __func__);

  // If the track has already stopped, just notify the sink of this fact without
  // adding it.
  if (stop_callback_.is_null()) {
    sink->OnReadyStateChanged(WebMediaStreamSource::kReadyStateEnded);
    return;
  }

  deliverer_.AddConsumer(sink);
  sink->OnEnabledChanged(is_enabled_.load(std::memory_order_relaxed));
}

void MediaStreamAudioTrack::RemoveSink(WebMediaStreamAudioSink* sink) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  WebRtcLog(kTag, this, "%s()", __func__);
  deliverer_.RemoveConsumer(sink);
}

media::AudioParameters MediaStreamAudioTrack::GetOutputFormat() const {
  return deliverer_.GetAudioParameters();
}

void MediaStreamAudioTrack::SetEnabled(bool enabled) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  WebRtcLog(kTag, this, "%s({enabled=%s})", __func__,
            (enabled ? "true" : "false"));

  const bool previously_enabled =
      is_enabled_.exchange(enabled, std::memory_order_relaxed);
  if (enabled == previously_enabled)
    return;

  Vector<WebMediaStreamAudioSink*> sinks_to_notify;
  deliverer_.GetConsumerList(&sinks_to_notify);
  for (WebMediaStreamAudioSink* sink : sinks_to_notify)
    sink->OnEnabledChanged(enabled);
}

bool MediaStreamAudioTrack::IsEnabled() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return is_enabled_.load(std::memory_order_relaxed);
}

void MediaStreamAudioTrack::SetContentHint(
    WebMediaStreamTrack::ContentHintType content_hint) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  Vector<WebMediaStreamAudioSink*> sinks_to_notify;
  deliverer_.GetConsumerList(&sinks_to_notify);
  for (WebMediaStreamAudioSink* sink : sinks_to_notify)
    sink->OnContentHintChanged(content_hint);
}

int MediaStreamAudioTrack::NumPreferredChannels() const {
  return deliverer_.NumPreferredChannels();
}

void* MediaStreamAudioTrack::GetClassIdentifier() const {
  return nullptr;
}

void MediaStreamAudioTrack::Start(base::OnceClosure stop_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!stop_callback.is_null());
  DCHECK(stop_callback_.is_null());
  WebRtcLog(kTag, this, "%s()", __func__);
  stop_callback_ = std::move(stop_callback);
}

void MediaStreamAudioTrack::StopAndNotify(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  WebRtcLog(kTag, this, "%s()", __func__);

  if (!stop_callback_.is_null())
    std::move(stop_callback_).Run();

  Vector<WebMediaStreamAudioSink*> sinks_to_end;
  deliverer_.GetConsumerList(&sinks_to_end);
  for (WebMediaStreamAudioSink* sink : sinks_to_end) {
    deliverer_.RemoveConsumer(sink);
    sink->OnReadyStateChanged(WebMediaStreamSource::kReadyStateEnded);
  }

  if (callback)
    std::move(callback).Run();
  weak_factory_.InvalidateWeakPtrs();
}

void MediaStreamAudioTrack::OnSetFormat(const media::AudioParameters& params) {
  WebRtcLog(kTag, this, "%s({params: [%s]})", __func__,
            params.AsHumanReadableString().c_str());
  deliverer_.OnSetFormat(params);
}

void MediaStreamAudioTrack::OnData(const media::AudioBus& audio_bus,
                                   base::TimeTicks reference_time,
                                   const media::AudioGlitchInfo& glitch_info) {
  TRACE_EVENT2(TRACE_DISABLED_BY_DEFAULT("mediastream"),
               "MediaStreamAudioTrack::OnData", "this",
               static_cast<void*>(this), "frame", audio_bus.frames());

  if (!received_audio_callback_) {
    // Add log message with unique this pointer id to mark the audio track as
    // alive at the first data callback.

    WebRtcLog(kTag, this, "%s() => (audio track is alive))", __func__);
    received_audio_callback_ = true;
  }

  // Note: Using relaxed ordering because the timing of when the audio thread
  // sees a changed |is_enabled_| value can be relaxed.
  const bool deliver_data = is_enabled_.load(std::memory_order_relaxed);

  if (deliver_data) {
    UpdateFrameStats(audio_bus, reference_time, glitch_info);
    deliverer_.OnData(audio_bus, reference_time, glitch_info);
  } else {
    // The W3C spec requires silent audio to flow while a track is disabled.
    if (!silent_bus_ || silent_bus_->channels() != audio_bus.channels() ||
        silent_bus_->frames() != audio_bus.frames()) {
      silent_bus_ =
          media::AudioBus::Create(audio_bus.channels(), audio_bus.frames());
      silent_bus_->Zero();
    }
    deliverer_.OnData(*silent_bus_, reference_time, {});
  }
}

void MediaStreamAudioTrack::TransferAudioFrameStatsTo(
    MediaStreamTrackPlatform::AudioFrameStats& destination) {
  base::AutoLock auto_lock(mainthread_frame_stats_lock_);
  destination.Absorb(mainthread_frame_stats_);
}

void MediaStreamAudioTrack::UpdateFrameStats(
    const media::AudioBus& audio_bus,
    base::TimeTicks reference_time,
    const media::AudioGlitchInfo& glitch_info) {
  pending_frame_stats_.Update(GetOutputFormat(), reference_time, glitch_info);

  // If the main thread does not already hold the lock, take it and transfer
  // the latest stats to the main thread.
  if (mainthread_frame_stats_lock_.Try()) {
    mainthread_frame_stats_.Absorb(pending_frame_stats_);
    mainthread_frame_stats_lock_.Release();
  }
}

}  // namespace blink
