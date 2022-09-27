// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_panel_enclosure_element.h"

#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"

namespace blink {

MediaControlPanelEnclosureElement::MediaControlPanelEnclosureElement(
    MediaControlsImpl& media_controls)
    : MediaControlDivElement(media_controls) {
  SetShadowPseudoId(AtomicString("-webkit-media-controls-enclosure"));
}

}  // namespace blink
