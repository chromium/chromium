// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_track_selector_menu_button_element.h"

#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"
#include "third_party/blink/renderer/platform/language.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "ui/strings/grit/ax_strings.h"

namespace blink {

MediaControlTrackSelectorMenuButtonElement::
    MediaControlTrackSelectorMenuButtonElement(
        MediaControlsImpl& media_controls,
        WebMediaPlayer::TrackType type)
    : MediaControlInputElement(media_controls), type_(type) {
  setType(input_type_names::kButton);
  switch (type) {
    case WebMediaPlayer::TrackType::kVideoTrack:
      setAttribute(html_names::kAriaLabelAttr,
                   AtomicString(GetLocale().QueryString(
                       IDS_AX_MEDIA_SHOW_VIDEO_TRACK_SELECTION_MENU_BUTTON)));
      SetShadowPseudoId(AtomicString(
          "-internal-media-controls-video-track-selection-button"));
      break;
    case WebMediaPlayer::TrackType::kAudioTrack:
      setAttribute(html_names::kAriaLabelAttr,
                   AtomicString(GetLocale().QueryString(
                       IDS_AX_MEDIA_SHOW_AUDIO_TRACK_SELECTION_MENU_BUTTON)));
      SetShadowPseudoId(AtomicString(
          "-internal-media-controls-audio-track-selection-button"));
      break;
    case WebMediaPlayer::TrackType::kTextTrack:
      NOTREACHED();
  }
}

bool MediaControlTrackSelectorMenuButtonElement::
    WillRespondToMouseClickEvents() {
  return true;
}

int MediaControlTrackSelectorMenuButtonElement::GetOverflowStringId() const {
  switch (type_) {
    case WebMediaPlayer::TrackType::kVideoTrack:
      return IDS_MEDIA_OVERFLOW_MENU_SHOW_VIDEO_TRACK_MENU_BUTTON;
    case WebMediaPlayer::TrackType::kAudioTrack:
      return IDS_MEDIA_OVERFLOW_MENU_SHOW_AUDIO_TRACK_MENU_BUTTON;
    case WebMediaPlayer::TrackType::kTextTrack:
      NOTREACHED();
  }
}

bool MediaControlTrackSelectorMenuButtonElement::HasOverflowButton() const {
  return true;
}

const char* MediaControlTrackSelectorMenuButtonElement::GetNameForHistograms()
    const {
  switch (type_) {
    case WebMediaPlayer::TrackType::kVideoTrack:
      return IsOverflowElement() ? "VideoTrackSelectorOverflowButton"
                                 : "VideoTrackSelectorButton";
    case WebMediaPlayer::TrackType::kAudioTrack:
      return IsOverflowElement() ? "AudioTrackSelectorOverflowButton"
                                 : "AudioTrackSelectorButton";
    case WebMediaPlayer::TrackType::kTextTrack:
      NOTREACHED();
  }
}

void MediaControlTrackSelectorMenuButtonElement::DefaultEventHandler(
    Event& event) {
  if (event.type() == event_type_names::kClick ||
      event.type() == event_type_names::kGesturetap) {
    GetMediaControls().ToggleTrackSelectionList(type_);
  }
  MediaControlInputElement::DefaultEventHandler(event);
}

}  // namespace blink
