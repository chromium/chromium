// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_volume_control_container_element.h"

#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_consts.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_elements_helper.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"

namespace blink {

MediaControlVolumeControlContainerElement::
    MediaControlVolumeControlContainerElement(MediaControlsImpl& media_controls)
    : MediaControlDivElement(media_controls) {
  SetShadowPseudoId(
      AtomicString("-webkit-media-controls-volume-control-container"));
  MediaControlElementsHelper::CreateDiv(
      "-webkit-media-controls-volume-control-hover-background", this);

  CloseContainer();
}

void MediaControlVolumeControlContainerElement::OpenContainer() {
  classList().Remove(kClosedCSSClass);
}

void MediaControlVolumeControlContainerElement::CloseContainer() {
  classList().Add(kClosedCSSClass);
}

void MediaControlVolumeControlContainerElement::DefaultEventHandler(
    Event& event) {
  if (event.type() == event_type_names::kMouseover)
    GetMediaControls().OpenVolumeSliderIfNecessary();

  if (event.type() == event_type_names::kMouseout)
    GetMediaControls().CloseVolumeSliderIfNecessary();
}

}  // namespace blink
