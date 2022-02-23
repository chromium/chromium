// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/focusgroup_controller_utils.h"

#include "third_party/blink/renderer/core/events/keyboard_event.h"

namespace blink {

FocusgroupDirection FocusgroupControllerUtils::FocusgroupDirectionForEvent(
    KeyboardEvent* event) {
  DCHECK(event);
  if (event->ctrlKey() || event->metaKey() || event->shiftKey())
    return FocusgroupDirection::kNone;

  // TODO(bebeaudr): Support RTL. Will it be as simple as inverting the
  // direction associated with the left and right arrows when in a RTL element?
  if (event->key() == "ArrowDown")
    return FocusgroupDirection::kForwardVertical;
  else if (event->key() == "ArrowRight")
    return FocusgroupDirection::kForwardHorizontal;
  else if (event->key() == "ArrowUp")
    return FocusgroupDirection::kBackwardVertical;
  else if (event->key() == "ArrowLeft")
    return FocusgroupDirection::kBackwardHorizontal;

  return FocusgroupDirection::kNone;
}

}  // namespace blink