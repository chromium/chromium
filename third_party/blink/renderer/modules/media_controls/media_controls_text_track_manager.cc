// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/media_controls_text_track_manager.h"

#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/track/text_track.h"
#include "third_party/blink/renderer/core/html/track/text_track_list.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"

namespace blink {

MediaControlsTextTrackManager::MediaControlsTextTrackManager(
    HTMLMediaElement& media_element)
    : media_element_(&media_element) {}

String MediaControlsTextTrackManager::GetTextTrackLabel(
    TextTrack* track) const {
  if (!track) {
    return media_element_->GetLocale().QueryString(IDS_MEDIA_TRACKS_OFF);
  }

  String track_label = track->label();

  if (track_label.IsEmpty())
    track_label = track->language();

  if (track_label.IsEmpty()) {
    track_label = String(media_element_->GetLocale().QueryString(
        IDS_MEDIA_TRACKS_NO_LABEL, String::Number(track->TrackIndex() + 1)));
  }

  return track_label;
}

void MediaControlsTextTrackManager::ShowTextTrackAtIndex(
    unsigned index_to_enable) {
  TextTrackList* track_list = media_element_->textTracks();
  if (index_to_enable >= track_list->length())
    return;
  TextTrack* track = track_list->AnonymousIndexedGetter(index_to_enable);
  if (track && track->CanBeRendered())
    track->setMode(TextTrack::ShowingKeyword());
}

void MediaControlsTextTrackManager::DisableShowingTextTracks() {
  TextTrackList* track_list = media_element_->textTracks();
  for (unsigned i = 0; i < track_list->length(); ++i) {
    TextTrack* track = track_list->AnonymousIndexedGetter(i);
    if (track->mode() == TextTrack::ShowingKeyword())
      track->setMode(TextTrack::DisabledKeyword());
  }
}

void MediaControlsTextTrackManager::Trace(Visitor* visitor) {
  visitor->Trace(media_element_);
}

}  // namespace blink
