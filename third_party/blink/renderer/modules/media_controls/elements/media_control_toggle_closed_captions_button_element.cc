// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_toggle_closed_captions_button_element.h"

#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/track/text_track.h"
#include "third_party/blink/renderer/core/html/track/text_track_list.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_text_track_manager.h"
#include "third_party/blink/renderer/platform/language.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "ui/strings/grit/ax_strings.h"

namespace blink {

namespace {

// The CSS class to use if we should use the closed captions icon.
const char kClosedCaptionClass[] = "closed-captions";

const char* kClosedCaptionLocales[] = {
    // English (United States)
    "en", "en-US",

    // Spanish (Latin America and Caribbean)
    "es-419",

    // Portuguese (Brazil)
    "pt-BR",
};

// Returns true if the default language should use the closed captions icon.
bool UseClosedCaptionsIcon() {
  for (auto*& locale : kClosedCaptionLocales) {
    if (locale == DefaultLanguage())
      return true;
  }

  return false;
}

}  // namespace

MediaControlToggleClosedCaptionsButtonElement::
    MediaControlToggleClosedCaptionsButtonElement(
        MediaControlsImpl& media_controls)
    : MediaControlInputElement(media_controls) {
  setAttribute(html_names::kAriaLabelAttr,
               WTF::AtomicString(GetLocale().QueryString(
                   IDS_AX_MEDIA_SHOW_CLOSED_CAPTIONS_MENU_BUTTON)));
  setType(input_type_names::kButton);
  SetShadowPseudoId(
      AtomicString("-webkit-media-controls-toggle-closed-captions-button"));
  SetClass(kClosedCaptionClass, UseClosedCaptionsIcon());
}

bool MediaControlToggleClosedCaptionsButtonElement::
    WillRespondToMouseClickEvents() {
  return true;
}

void MediaControlToggleClosedCaptionsButtonElement::UpdateDisplayType() {
  bool captions_visible = MediaElement().TextTracksVisible();
  SetClass("visible", captions_visible);
  UpdateOverflowString();

  MediaControlInputElement::UpdateDisplayType();
}

int MediaControlToggleClosedCaptionsButtonElement::GetOverflowStringId() const {
  return IDS_MEDIA_OVERFLOW_MENU_CLOSED_CAPTIONS;
}

bool MediaControlToggleClosedCaptionsButtonElement::HasOverflowButton() const {
  return true;
}

String
MediaControlToggleClosedCaptionsButtonElement::GetOverflowMenuSubtitleString()
    const {
  if (!MediaElement().HasClosedCaptions() ||
      !MediaElement().TextTracksAreReady()) {
    // Don't show any subtitle if no text tracks are available.
    return String();
  }

  TextTrackList* track_list = MediaElement().textTracks();
  for (unsigned i = 0; i < track_list->length(); i++) {
    TextTrack* track = track_list->AnonymousIndexedGetter(i);
    if (track && track->mode() == TextTrackMode::kShowing)
      return GetMediaControls().GetTextTrackManager().GetTextTrackLabel(track);
  }

  // Return the label for no text track.
  return GetMediaControls().GetTextTrackManager().GetTextTrackLabel(nullptr);
}

const char*
MediaControlToggleClosedCaptionsButtonElement::GetNameForHistograms() const {
  return IsOverflowElement() ? "ClosedCaptionOverflowButton"
                             : "ClosedCaptionButton";
}

void MediaControlToggleClosedCaptionsButtonElement::DefaultEventHandler(
    Event& event) {
  if (event.type() == event_type_names::kClick ||
      event.type() == event_type_names::kGesturetap) {
    if (MediaElement().textTracks()->length() == 1) {
      // If only one track exists, toggle it on/off
      if (MediaElement().textTracks()->HasShowingTracks())
        GetMediaControls().GetTextTrackManager().DisableShowingTextTracks();
      else
        GetMediaControls().GetTextTrackManager().ShowTextTrackAtIndex(0);
    } else {
      GetMediaControls().ToggleTextTrackList();
    }

    UpdateDisplayType();
  }

  MediaControlInputElement::DefaultEventHandler(event);
}

}  // namespace blink
