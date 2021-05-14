/*
 * Copyright (C) 2011 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/html/track/track_event.h"

#include "third_party/blink/public/platform/web_media_player.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_track_event_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_audiotrack_texttrack_videotrack.h"
#include "third_party/blink/renderer/bindings/core/v8/video_track_or_audio_track_or_text_track.h"
#include "third_party/blink/renderer/core/event_interface_names.h"
#include "third_party/blink/renderer/core/html/track/audio_track.h"
#include "third_party/blink/renderer/core/html/track/text_track.h"
#include "third_party/blink/renderer/core/html/track/video_track.h"

namespace blink {

TrackEvent::TrackEvent() = default;

TrackEvent::TrackEvent(const AtomicString& type,
                       const TrackEventInit* initializer)
    : Event(type, initializer) {
  if (!initializer->hasTrack())
    return;

  const VideoTrackOrAudioTrackOrTextTrack& track = initializer->track();
  if (track.IsVideoTrack())
    track_ = track.GetAsVideoTrack();
  else if (track.IsAudioTrack())
    track_ = track.GetAsAudioTrack();
  else if (track.IsTextTrack())
    track_ = track.GetAsTextTrack();
  else
    NOTREACHED();
}

TrackEvent::~TrackEvent() = default;

const AtomicString& TrackEvent::InterfaceName() const {
  return event_interface_names::kTrackEvent;
}

#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
V8UnionAudioTrackOrTextTrackOrVideoTrack* TrackEvent::track() {
  if (!track_)
    return nullptr;

  switch (track_->GetType()) {
    case WebMediaPlayer::kTextTrack:
      return MakeGarbageCollected<V8UnionAudioTrackOrTextTrackOrVideoTrack>(
          To<TextTrack>(track_.Get()));
    case WebMediaPlayer::kAudioTrack:
      return MakeGarbageCollected<V8UnionAudioTrackOrTextTrackOrVideoTrack>(
          To<AudioTrack>(track_.Get()));
    case WebMediaPlayer::kVideoTrack:
      return MakeGarbageCollected<V8UnionAudioTrackOrTextTrackOrVideoTrack>(
          To<VideoTrack>(track_.Get()));
  }

  NOTREACHED();
  return nullptr;
}
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
void TrackEvent::track(VideoTrackOrAudioTrackOrTextTrack& return_value) {
  if (!track_)
    return;

  switch (track_->GetType()) {
    case WebMediaPlayer::kTextTrack:
      return_value.SetTextTrack(To<TextTrack>(track_.Get()));
      break;
    case WebMediaPlayer::kAudioTrack:
      return_value.SetAudioTrack(To<AudioTrack>(track_.Get()));
      break;
    case WebMediaPlayer::kVideoTrack:
      return_value.SetVideoTrack(To<VideoTrack>(track_.Get()));
      break;
    default:
      NOTREACHED();
  }
}
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)

void TrackEvent::Trace(Visitor* visitor) const {
  visitor->Trace(track_);
  Event::Trace(visitor);
}

}  // namespace blink
