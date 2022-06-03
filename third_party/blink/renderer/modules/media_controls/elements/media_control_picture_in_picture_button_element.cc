// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_picture_in_picture_button_element.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"
#include "third_party/blink/renderer/modules/picture_in_picture/picture_in_picture_controller_impl.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"

namespace blink {

MediaControlPictureInPictureButtonElement::
    MediaControlPictureInPictureButtonElement(MediaControlsImpl& media_controls)
    : MediaControlInputElement(media_controls) {
  setType(input_type_names::kButton);
  setAttribute(html_names::kRoleAttr, "button");

  bool isInPictureInPicture =
      PictureInPictureController::IsElementInPictureInPicture(
          &To<HTMLVideoElement>(MediaElement()));

  UpdateAriaString(isInPictureInPicture);

  SetShadowPseudoId(
      AtomicString("-internal-media-controls-picture-in-picture-button"));
  SetIsWanted(false);
}

bool MediaControlPictureInPictureButtonElement::
    WillRespondToMouseClickEvents() {
  return true;
}

void MediaControlPictureInPictureButtonElement::UpdateDisplayType() {
  DCHECK(IsA<HTMLVideoElement>(MediaElement()));
  bool isInPictureInPicture =
      PictureInPictureController::IsElementInPictureInPicture(
          &To<HTMLVideoElement>(MediaElement()));
  SetClass("on", isInPictureInPicture);
  UpdateOverflowString();

  UpdateAriaString(isInPictureInPicture);

  MediaControlInputElement::UpdateDisplayType();
}

int MediaControlPictureInPictureButtonElement::GetOverflowStringId() const {
  DCHECK(IsA<HTMLVideoElement>(MediaElement()));
  bool isInPictureInPicture =
      PictureInPictureController::IsElementInPictureInPicture(
          &To<HTMLVideoElement>(MediaElement()));

  return isInPictureInPicture
             ? IDS_MEDIA_OVERFLOW_MENU_EXIT_PICTURE_IN_PICTURE
             : IDS_MEDIA_OVERFLOW_MENU_ENTER_PICTURE_IN_PICTURE;
}

bool MediaControlPictureInPictureButtonElement::HasOverflowButton() const {
  return true;
}

bool MediaControlPictureInPictureButtonElement::IsControlPanelButton() const {
  return true;
}

const char* MediaControlPictureInPictureButtonElement::GetNameForHistograms()
    const {
  return IsOverflowElement() ? "PictureInPictureOverflowButton"
                             : "PictureInPictureButton";
}

void MediaControlPictureInPictureButtonElement::DefaultEventHandler(
    Event& event) {
  if (event.type() == event_type_names::kClick ||
      event.type() == event_type_names::kGesturetap) {
    PictureInPictureControllerImpl& controller =
        PictureInPictureControllerImpl::From(MediaElement().GetDocument());

    auto* video_element = &To<HTMLVideoElement>(MediaElement());
    if (PictureInPictureController::IsElementInPictureInPicture(
            video_element)) {
      controller.ExitPictureInPicture(video_element, nullptr);
    } else {
      controller.EnterPictureInPicture(video_element, nullptr /* options */,
                                       nullptr /* promise */);
    }
  }

  MediaControlInputElement::DefaultEventHandler(event);
}

void MediaControlPictureInPictureButtonElement::UpdateAriaString(
    bool isInPictureInPicture) {
  String aria_string =
      isInPictureInPicture
          ? GetLocale().QueryString(IDS_AX_MEDIA_EXIT_PICTURE_IN_PICTURE_BUTTON)
          : GetLocale().QueryString(
                IDS_AX_MEDIA_ENTER_PICTURE_IN_PICTURE_BUTTON);

  setAttribute(html_names::kAriaLabelAttr, WTF::AtomicString(aria_string));
}

}  // namespace blink
