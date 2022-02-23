// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/focusgroup_controller.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/dom/focusgroup_flags.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/focusgroup_controller_utils.h"

namespace blink {

using utils = FocusgroupControllerUtils;

bool FocusgroupController::HandleArrowKeyboardEvent(KeyboardEvent* event,
                                                    const LocalFrame* frame) {
  DCHECK(RuntimeEnabledFeatures::FocusgroupEnabled());
  DCHECK(frame);
  FocusgroupDirection direction = utils::FocusgroupDirectionForEvent(event);
  if (direction == FocusgroupDirection::kNone)
    return false;

  if (!frame->GetDocument())
    return false;

  const Element* focused = frame->GetDocument()->FocusedElement();
  if (!focused || focused != event->target()) {
    // The FocusgroupController shouldn't handle this arrow key event when the
    // focus already moved to a different element than where it came from. The
    // webpage likely had a key-handler that moved the focus.
    return false;
  }

  return Advance(focused, direction);
}

bool FocusgroupController::Advance(const Element* initial_element,
                                   FocusgroupDirection direction) {
  // TODO(bebeaudr): Implement.
  return false;
}

}  // namespace blink