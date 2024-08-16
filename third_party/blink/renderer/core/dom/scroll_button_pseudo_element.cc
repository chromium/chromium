// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/scroll_button_pseudo_element.h"

#include "cc/input/scroll_snap_data.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_keyboard_event_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_scroll_into_view_options.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/dom/scroll_marker_group_pseudo_element.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/scroll/scroll_alignment.h"
#include "third_party/blink/renderer/core/scroll/scroll_into_view_util.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"

namespace blink {

void ScrollButtonPseudoElement::DefaultEventHandler(Event& event) {
  bool is_click =
      event.IsMouseEvent() && event.type() == event_type_names::kClick;
  bool is_key_down =
      event.IsKeyboardEvent() && event.type() == event_type_names::kKeydown;
  bool is_enter_or_space =
      is_key_down && (To<KeyboardEvent>(event).keyCode() == VKEY_RETURN ||
                      To<KeyboardEvent>(event).keyCode() == VKEY_SPACE);
  bool should_intercept =
      event.target() == this && (is_click || is_enter_or_space);
  if (should_intercept) {
    if (scroll_marker_group_) {
      if (GetPseudoId() == kPseudoIdScrollNextButton) {
        scroll_marker_group_->ActivateNextScrollMarker();
      } else {
        scroll_marker_group_->ActivatePrevScrollMarker();
      }
    }
    event.SetDefaultHandled();
  }
  PseudoElement::DefaultEventHandler(event);
}

void ScrollButtonPseudoElement::Trace(Visitor* v) const {
  v->Trace(scroll_marker_group_);
  PseudoElement::Trace(v);
}

}  // namespace blink
