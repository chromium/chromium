// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_div_element.h"

#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_elements_helper.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"

namespace blink {

void MediaControlDivElement::SetOverflowElementIsWanted(bool) {}

void MediaControlDivElement::MaybeRecordDisplayed() {
  // No-op. At the moment, usage is only recorded in the context of CTR. It
  // could be recorded for MediaControlDivElement but there is no need for it at
  // the moment.
}

MediaControlDivElement::MediaControlDivElement(
    MediaControlsImpl& media_controls)
    : HTMLDivElement(media_controls.GetDocument()),
      MediaControlElementBase(media_controls, this) {}

bool MediaControlDivElement::IsMediaControlElement() const {
  return true;
}

WebSize MediaControlDivElement::GetSizeOrDefault() const {
  return MediaControlElementsHelper::GetSizeOrDefault(*this, WebSize(0, 0));
}

bool MediaControlDivElement::IsDisabled() const {
  // Div elements cannot be disabled.
  return false;
}

void MediaControlDivElement::Trace(blink::Visitor* visitor) {
  HTMLDivElement::Trace(visitor);
  MediaControlElementBase::Trace(visitor);
}

}  // namespace blink
