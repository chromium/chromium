// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_overflow_menu_button_element.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/user_metrics_action.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "ui/strings/grit/ax_strings.h"

namespace blink {

MediaControlOverflowMenuButtonElement::MediaControlOverflowMenuButtonElement(
    MediaControlsImpl& media_controls)
    : MediaControlInputElement(media_controls) {
  setType(input_type_names::kButton);
  setAttribute(
      html_names::kAriaLabelAttr,
      WTF::AtomicString(GetLocale().QueryString(IDS_AX_MEDIA_OVERFLOW_BUTTON)));
  setAttribute(html_names::kTitleAttr,
               WTF::AtomicString(
                   GetLocale().QueryString(IDS_AX_MEDIA_OVERFLOW_BUTTON_HELP)));
  setAttribute(html_names::kAriaHaspopupAttr, AtomicString("menu"));
  SetShadowPseudoId(AtomicString("-internal-media-controls-overflow-button"));
  SetIsWanted(false);
}

bool MediaControlOverflowMenuButtonElement::WillRespondToMouseClickEvents() {
  return true;
}

bool MediaControlOverflowMenuButtonElement::IsControlPanelButton() const {
  return true;
}

const char* MediaControlOverflowMenuButtonElement::GetNameForHistograms()
    const {
  return "OverflowButton";
}

void MediaControlOverflowMenuButtonElement::DefaultEventHandler(Event& event) {
  // Only respond to a click event if we are not disabled.
  if (!IsDisabled() && (event.type() == event_type_names::kClick ||
                        event.type() == event_type_names::kGesturetap)) {
    if (GetMediaControls().OverflowMenuVisible()) {
      Platform::Current()->RecordAction(
          UserMetricsAction("Media.Controls.OverflowClose"));
    } else {
      Platform::Current()->RecordAction(
          UserMetricsAction("Media.Controls.OverflowOpen"));
    }

    GetMediaControls().ToggleOverflowMenu();
    event.SetDefaultHandled();
  }

  MediaControlInputElement::DefaultEventHandler(event);
}

}  // namespace blink
