// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/modules/mediastream/media_stream_audio_track.h"

#include <utility>
#include <vector>

#include "base/logging.h"
#include "media/base/audio_bus.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_audio_sink.h"
#include "third_party/blink/public/platform/web_media_stream_source.h"

namespace blink {

MediaStreamAudioTrack::MediaStreamAudioTrack(bool is_local_track)
    : WebPlatformMediaStreamTrack(is_local_track), is_enabled_(1) {
  DVLOG(1) << "MediaStreamAudioTrack@" << this << "::MediaStreamAudioTrack("
           << (is_local_track ? "local" : "remote") << " track)";
}

MediaStreamAudioTrack::~MediaStreamAudioTrack() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(1) << "MediaStreamAudioTrack@" << this << " is being destroyed.";
  Stop();
}

// static
MediaStreamAudioTrack* MediaStreamAudioTrack::From(
    const WebMediaStreamTrack& track) {
  if (track.IsNull() ||
      track.Source().GetType() != WebMediaStreamSource::kTypeAudio) {
    return nullptr;
  }
  return static_cast<MediaStreamAudioTrack*>(track.GetPlatformTrack());
}

void MediaStreamAudioTrack::AddSink(WebMediaStreamAudioSink* sink) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  DVLOG(1) << "Adding WebMediaStreamAudioSink@" << sink
           << " to MediaStreamAudioTrack@" << this << '.';

  // If the track has already stopped, just notify the sink of this fact without
  // adding it.
  if (stop_callback_.is_null()) {
    sink->OnReadyStateChanged(WebMediaStreamSource::kReadyStateEnded);
    return;
  }

  deliverer_.AddConsumer(sink);
  sink->OnEnabledChanged(!!base::subtle::NoBarrier_Load(&is_enabled_));
}

void MediaStreamAudioTrack::RemoveSink(WebMediaStreamAudioSink* sink) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  deliverer_.RemoveConsumer(sink);
  DVLOG(1) << "Removed WebMediaStreamAudioSink@" << sink
           << " from MediaStreamAudioTrack@" << this << '.';
}

media::AudioParameters MediaStreamAudioTrack::GetOutputFormat() const {
  return deliverer_.GetAudioParameters();
}

void MediaStreamAudioTrack::SetEnabled(bool enabled) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(1) << "MediaStreamAudioTrack@" << this << "::SetEnabled("
           << (enabled ? 'Y' : 'N') << ')';

  const bool previously_enabled =
      !!base::subtle::NoBarrier_AtomicExchange(&is_enabled_, enabled ? 1 : 0);
  if (enabled == previously_enabled)
    return;

  std::vector<WebMediaStreamAudioSink*> sinks_to_notify;
  deliverer_.GetConsumerList(&sinks_to_notify);
  for (WebMediaStreamAudioSink* sink : sinks_to_notify)
    sink->OnEnabledChanged(enabled);
}

void MediaStreamAudioTrack::SetContentHint(
    WebMediaStreamTrack::ContentHintType content_hint) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  std::vector<WebMediaStreamAudioSink*> sinks_to_notify;
  deliverer_.GetConsumerList(&sinks_to_notify);
  for (WebMediaStreamAudioSink* sink : sinks_to_notify)
    sink->OnContentHintChanged(content_hint);
}

void* MediaStreamAudioTrack::GetClassIdentifier() const {
  return nullptr;
}

void MediaStreamAudioTrack::Start(base::OnceClosure stop_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!stop_callback.is_null());
  DCHECK(stop_callback_.is_null());
  DVLOG(1) << "Starting MediaStreamAudioTrack@" << this << '.';
  stop_callback_ = std::move(stop_callback);
}

void MediaStreamAudioTrack::StopAndNotify(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(1) << "Stopping MediaStreamAudioTrack@" << this << '.';

  if (!stop_callback_.is_null())
    std::move(stop_callback_).Run();

  std::vector<WebMediaStreamAudioSink*> sinks_to_end;
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
  deliverer_.OnSetFormat(params);
}

void MediaStreamAudioTrack::OnData(const media::AudioBus& audio_bus,
                                   base::TimeTicks reference_time) {
  // Note: Using NoBarrier_Load because the timing of when the audio thread sees
  // a changed |is_enabled_| value can be relaxed.
  const bool deliver_data = !!base::subtle::NoBarrier_Load(&is_enabled_);

  if (deliver_data) {
    deliverer_.OnData(audio_bus, reference_time);
  } else {
    // The W3C spec requires silent audio to flow while a track is disabled.
    if (!silent_bus_ || silent_bus_->channels() != audio_bus.channels() ||
        silent_bus_->frames() != audio_bus.frames()) {
      silent_bus_ =
          media::AudioBus::Create(audio_bus.channels(), audio_bus.frames());
      silent_bus_->Zero();
    }
    deliverer_.OnData(*silent_bus_, reference_time);
  }
}

}  // namespace blink
