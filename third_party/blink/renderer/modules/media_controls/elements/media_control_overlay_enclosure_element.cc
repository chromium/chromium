// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_overlay_enclosure_element.h"

#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"

namespace blink {

MediaControlOverlayEnclosureElement::MediaControlOverlayEnclosureElement(
    MediaControlsImpl& media_controls)
    : MediaControlDivElement(media_controls) {
  SetShadowPseudoId(AtomicString("-webkit-media-controls-overlay-enclosure"));
}

void MediaControlOverlayEnclosureElement::DefaultEventHandler(Event& event) {
  // When the user interacts with the media element, the Cast overlay button
  // needs to be shown.
  if (event.type() == event_type_names::kGesturetap ||
      event.type() == event_type_names::kClick ||
      event.type() == event_type_names::kPointerover ||
      event.type() == event_type_names::kPointermove) {
    GetMediaControls().ShowOverlayCastButtonIfNeeded();
  }

  MediaControlDivElement::DefaultEventHandler(event);
}

}  // namespace blink
