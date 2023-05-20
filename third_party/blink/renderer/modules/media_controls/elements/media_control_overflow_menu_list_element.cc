// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_overflow_menu_list_element.h"

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_consts.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_overflow_menu_button_element.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

MediaControlOverflowMenuListElement::MediaControlOverflowMenuListElement(
    MediaControlsImpl& media_controls)
    : MediaControlPopupMenuElement(media_controls) {
  SetShadowPseudoId(
      AtomicString("-internal-media-controls-overflow-menu-list"));
  setAttribute(html_names::kRoleAttr, AtomicString("menu"));
  CloseOverflowMenu();
}

void MediaControlOverflowMenuListElement::OpenOverflowMenu() {
  classList().Remove(AtomicString(kClosedCSSClass));
}

void MediaControlOverflowMenuListElement::CloseOverflowMenu() {
  classList().Add(AtomicString(kClosedCSSClass));
}

void MediaControlOverflowMenuListElement::DefaultEventHandler(Event& event) {
  if (event.type() == event_type_names::kClick)
    event.SetDefaultHandled();

  MediaControlPopupMenuElement::DefaultEventHandler(event);
}

void MediaControlOverflowMenuListElement::SetIsWanted(bool wanted) {
  MediaControlPopupMenuElement::SetIsWanted(wanted);

  if (wanted) {
    OpenOverflowMenu();
  } else if (!GetMediaControls().TextTrackListIsWanted() &&
             !GetMediaControls().PlaybackSpeedListIsWanted()) {
    CloseOverflowMenu();
  }
}

}  // namespace blink
