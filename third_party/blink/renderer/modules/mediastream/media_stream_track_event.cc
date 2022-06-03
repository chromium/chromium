/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/mediastream/media_stream_track_event.h"

#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"

namespace blink {

MediaStreamTrackEvent::MediaStreamTrackEvent(const AtomicString& type,
                                             MediaStreamTrack* track)
    : Event(type, Bubbles::kNo, Cancelable::kNo), track_(track) {
  DCHECK(track_);
}

MediaStreamTrackEvent* MediaStreamTrackEvent::Create(
    const AtomicString& type,
    const MediaStreamTrackEventInit* initializer) {
  return MakeGarbageCollected<MediaStreamTrackEvent>(type, initializer);
}

MediaStreamTrackEvent::MediaStreamTrackEvent(
    const AtomicString& type,
    const MediaStreamTrackEventInit* initializer)
    : Event(type, initializer), track_(initializer->track()) {
  DCHECK(track_);
}

MediaStreamTrackEvent::~MediaStreamTrackEvent() = default;

MediaStreamTrack* MediaStreamTrackEvent::track() const {
  return track_.Get();
}

const AtomicString& MediaStreamTrackEvent::InterfaceName() const {
  return event_interface_names::kMediaStreamTrackEvent;
}

void MediaStreamTrackEvent::Trace(Visitor* visitor) const {
  visitor->Trace(track_);
  Event::Trace(visitor);
}

}  // namespace blink
