// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_fullscreen_button_element.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/user_metrics_action.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "ui/strings/grit/ax_strings.h"

namespace blink {

MediaControlFullscreenButtonElement::MediaControlFullscreenButtonElement(
    MediaControlsImpl& media_controls)
    : MediaControlInputElement(media_controls) {
  setType(input_type_names::kButton);
  SetShadowPseudoId(AtomicString("-webkit-media-controls-fullscreen-button"));
  SetIsFullscreen(MediaElement().IsFullscreen());
  SetIsWanted(false);
}

void MediaControlFullscreenButtonElement::SetIsFullscreen(bool is_fullscreen) {
  if (is_fullscreen) {
    setAttribute(html_names::kAriaLabelAttr,
                 WTF::AtomicString(GetLocale().QueryString(
                     IDS_AX_MEDIA_EXIT_FULL_SCREEN_BUTTON)));
  } else {
    setAttribute(html_names::kAriaLabelAttr,
                 WTF::AtomicString(GetLocale().QueryString(
                     IDS_AX_MEDIA_ENTER_FULL_SCREEN_BUTTON)));
  }
  SetClass("fullscreen", is_fullscreen);
}

bool MediaControlFullscreenButtonElement::WillRespondToMouseClickEvents() {
  return true;
}

int MediaControlFullscreenButtonElement::GetOverflowStringId() const {
  if (MediaElement().IsFullscreen())
    return IDS_MEDIA_OVERFLOW_MENU_EXIT_FULLSCREEN;
  return IDS_MEDIA_OVERFLOW_MENU_ENTER_FULLSCREEN;
}

bool MediaControlFullscreenButtonElement::HasOverflowButton() const {
  return true;
}

bool MediaControlFullscreenButtonElement::IsControlPanelButton() const {
  return true;
}

const char* MediaControlFullscreenButtonElement::GetNameForHistograms() const {
  return IsOverflowElement() ? "FullscreenOverflowButton" : "FullscreenButton";
}

void MediaControlFullscreenButtonElement::DefaultEventHandler(Event& event) {
  if (!IsDisabled() && (event.type() == event_type_names::kClick ||
                        event.type() == event_type_names::kGesturetap)) {
    RecordClickMetrics();
    if (MediaElement().IsFullscreen())
      GetMediaControls().ExitFullscreen();
    else
      GetMediaControls().EnterFullscreen();

    if (!IsOverflowElement())
      event.SetDefaultHandled();
  }
  MediaControlInputElement::DefaultEventHandler(event);
}

void MediaControlFullscreenButtonElement::RecordClickMetrics() {
  bool is_embedded_experience_enabled =
      GetDocument().GetSettings() &&
      GetDocument().GetSettings()->GetEmbeddedMediaExperienceEnabled();

  if (MediaElement().IsFullscreen()) {
    Platform::Current()->RecordAction(
        UserMetricsAction("Media.Controls.ExitFullscreen"));
    if (is_embedded_experience_enabled) {
      Platform::Current()->RecordAction(UserMetricsAction(
          "Media.Controls.ExitFullscreen.EmbeddedExperience"));
    }
  } else {
    Platform::Current()->RecordAction(
        UserMetricsAction("Media.Controls.EnterFullscreen"));
    if (is_embedded_experience_enabled) {
      Platform::Current()->RecordAction(UserMetricsAction(
          "Media.Controls.EnterFullscreen.EmbeddedExperience"));
    }
  }
}

}  // namespace blink
