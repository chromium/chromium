// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/html_media_track_element_media_track.h"

#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"

namespace blink {

// static
const char HTMLMediaTrackElementMediaTrack::kSupplementName[] =
    "HTMLMediaTrackElementMediaTrack";

// static
HTMLMediaTrackElementMediaTrack& HTMLMediaTrackElementMediaTrack::From(
    HTMLMediaTrackElementBase& element) {
  HTMLMediaTrackElementMediaTrack* supplement =
      Supplement<HTMLMediaTrackElementBase>::From<
          HTMLMediaTrackElementMediaTrack>(element);
  if (!supplement) {
    supplement = MakeGarbageCollected<HTMLMediaTrackElementMediaTrack>(element);
    ProvideTo(element, supplement);
  }
  return *supplement;
}

// static
MediaStreamTrack* HTMLMediaTrackElementMediaTrack::track(
    HTMLMediaTrackElementBase& element) {
  return From(element).MediaTrack();
}

HTMLMediaTrackElementMediaTrack::HTMLMediaTrackElementMediaTrack(
    HTMLMediaTrackElementBase& element)
    : Supplement<HTMLMediaTrackElementBase>(element) {}

void HTMLMediaTrackElementMediaTrack::Trace(Visitor* visitor) const {
  visitor->Trace(media_track_);
  Supplement<HTMLMediaTrackElementBase>::Trace(visitor);
}

}  // namespace blink

