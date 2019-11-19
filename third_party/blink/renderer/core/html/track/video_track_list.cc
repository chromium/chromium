// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/track/video_track_list.h"

#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/track/video_track.h"

namespace blink {

VideoTrackList::~VideoTrackList() = default;

VideoTrackList::VideoTrackList(HTMLMediaElement& media_element)
    : TrackListBase<VideoTrack>(&media_element) {}

const AtomicString& VideoTrackList::InterfaceName() const {
  return event_target_names::kVideoTrackList;
}

int VideoTrackList::selectedIndex() const {
  for (unsigned i = 0; i < length(); ++i) {
    VideoTrack* track = AnonymousIndexedGetter(i);

    if (track->selected())
      return i;
  }

  return -1;
}

void VideoTrackList::TrackSelected(WebMediaPlayer::TrackId selected_track_id) {
  // Clear the selected flag on the previously selected track, if any.
  for (unsigned i = 0; i < length(); ++i) {
    VideoTrack* track = AnonymousIndexedGetter(i);

    if (track->id() != selected_track_id)
      track->ClearSelected();
    else
      DCHECK(track->selected());
  }
}

}  // namespace blink
