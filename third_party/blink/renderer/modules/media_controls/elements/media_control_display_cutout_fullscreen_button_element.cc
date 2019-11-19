// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_display_cutout_fullscreen_button_element.h"

#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/viewport_data.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"

namespace blink {

MediaControlDisplayCutoutFullscreenButtonElement::
    MediaControlDisplayCutoutFullscreenButtonElement(
        MediaControlsImpl& media_controls)
    : MediaControlInputElement(media_controls) {
  setType(input_type_names::kButton);
  setAttribute(html_names::kAriaLabelAttr,
               WTF::AtomicString(GetLocale().QueryString(
                   IDS_AX_MEDIA_DISPLAY_CUT_OUT_FULL_SCREEN_BUTTON)));
  SetShadowPseudoId(AtomicString(
      "-internal-media-controls-display-cutout-fullscreen-button"));
  SetIsWanted(false);
}

bool MediaControlDisplayCutoutFullscreenButtonElement::
    WillRespondToMouseClickEvents() {
  return true;
}

void MediaControlDisplayCutoutFullscreenButtonElement::DefaultEventHandler(
    Event& event) {
  if (event.type() == event_type_names::kClick ||
      event.type() == event_type_names::kGesturetap) {
    // The button shouldn't be visible if not in fullscreen.
    DCHECK(MediaElement().IsFullscreen());

    GetDocument().GetViewportData().SetExpandIntoDisplayCutout(
        !GetDocument().GetViewportData().GetExpandIntoDisplayCutout());
    event.SetDefaultHandled();
  }
  HTMLInputElement::DefaultEventHandler(event);
}

const char*
MediaControlDisplayCutoutFullscreenButtonElement::GetNameForHistograms() const {
  return "DisplayCutoutFullscreenButton";
}

}  // namespace blink
