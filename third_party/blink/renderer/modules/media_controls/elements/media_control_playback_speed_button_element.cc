// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_playback_speed_button_element.h"

#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"
#include "third_party/blink/renderer/platform/language.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "ui/strings/grit/ax_strings.h"

namespace blink {

MediaControlPlaybackSpeedButtonElement::MediaControlPlaybackSpeedButtonElement(
    MediaControlsImpl& media_controls)
    : MediaControlInputElement(media_controls) {
  setAttribute(html_names::kAriaLabelAttr,
               WTF::AtomicString(GetLocale().QueryString(
                   IDS_AX_MEDIA_SHOW_PLAYBACK_SPEED_MENU_BUTTON)));
  setType(input_type_names::kButton);
  SetShadowPseudoId(
      AtomicString("-internal-media-controls-playback-speed-button"));
}

bool MediaControlPlaybackSpeedButtonElement::WillRespondToMouseClickEvents() {
  return true;
}

int MediaControlPlaybackSpeedButtonElement::GetOverflowStringId() const {
  return IDS_MEDIA_OVERFLOW_MENU_PLAYBACK_SPEED;
}

bool MediaControlPlaybackSpeedButtonElement::HasOverflowButton() const {
  return true;
}

const char* MediaControlPlaybackSpeedButtonElement::GetNameForHistograms()
    const {
  return IsOverflowElement() ? "PlaybackSpeedOverflowButton"
                             : "PlaybackSpeedButton";
}

void MediaControlPlaybackSpeedButtonElement::DefaultEventHandler(Event& event) {
  if (event.type() == event_type_names::kClick ||
      event.type() == event_type_names::kGesturetap) {
    GetMediaControls().TogglePlaybackSpeedList();
  }
  MediaControlInputElement::DefaultEventHandler(event);
}

}  // namespace blink
