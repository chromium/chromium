// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediasource/source_buffer_track_base_supplement.h"

#include "third_party/blink/renderer/core/html/track/track_base.h"
#include "third_party/blink/renderer/modules/mediasource/source_buffer.h"

namespace blink {

// static
SourceBufferTrackBaseSupplement* SourceBufferTrackBaseSupplement::FromIfExists(
    TrackBase& track) {
  return track.GetSourceBufferTrackBaseSupplement();
}

// static
SourceBufferTrackBaseSupplement& SourceBufferTrackBaseSupplement::From(
    TrackBase& track) {
  SourceBufferTrackBaseSupplement* supplement = FromIfExists(track);
  if (!supplement) {
    supplement = MakeGarbageCollected<SourceBufferTrackBaseSupplement>();
    track.SetSourceBufferTrackBaseSupplement(supplement);
  }
  return *supplement;
}

// static
SourceBuffer* SourceBufferTrackBaseSupplement::sourceBuffer(TrackBase& track) {
  SourceBufferTrackBaseSupplement* supplement = FromIfExists(track);
  if (supplement)
    return supplement->source_buffer_.Get();
  return nullptr;
}

void SourceBufferTrackBaseSupplement::SetSourceBuffer(
    TrackBase& track,
    SourceBuffer* source_buffer) {
  From(track).source_buffer_ = source_buffer;
}

void SourceBufferTrackBaseSupplement::Trace(Visitor* visitor) const {
  visitor->Trace(source_buffer_);
}

}  // namespace blink
