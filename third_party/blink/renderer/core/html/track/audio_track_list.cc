// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/track/audio_track_list.h"
#include "third_party/blink/renderer/core/event_target_names.h"

namespace blink {

AudioTrackList::~AudioTrackList() = default;

AudioTrackList::AudioTrackList(HTMLMediaElement& media_element)
    : TrackListBase<AudioTrack>(&media_element) {}

bool AudioTrackList::HasEnabledTrack() const {
  for (size_t i = 0; i < length(); ++i) {
    if (AnonymousIndexedGetter(i)->enabled())
      return true;
  }

  return false;
}

const AtomicString& AudioTrackList::InterfaceName() const {
  return event_target_names::kAudioTrackList;
}

void AudioTrackList::TrackEnabled(const String& track_id, bool exclusive) {
  for (size_t i = 0; i < length(); ++i) {
    AudioTrack* track = AnonymousIndexedGetter(i);
    if (track->id() != track_id) {
      if (exclusive || track->IsExclusive()) {
        track->ClearEnabled();
      }
    } else {
      DCHECK(track->enabled());
    }
  }
}

}  // namespace blink
