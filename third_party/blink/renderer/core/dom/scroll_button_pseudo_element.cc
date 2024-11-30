// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/scroll_button_pseudo_element.h"

#include "cc/input/scroll_snap_data.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_keyboard_event_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_scroll_into_view_options.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
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
    Element* scroller = UltimateOriginatingElement();
    double dx =
        PageSizePercent * scroller->GetLayoutBox()->Size().width.ToDouble();
    double dy =
        PageSizePercent * scroller->GetLayoutBox()->Size().height.ToDouble();
    if (GetPseudoId() == kPseudoIdScrollUpButton) {
      scroller->scrollBy(0, -dy);
    } else if (GetPseudoId() == kPseudoIdScrollDownButton) {
      scroller->scrollBy(0, dy);
    } else if (GetPseudoId() == kPseudoIdScrollLeftButton) {
      scroller->scrollBy(-dx, 0);
    } else if (GetPseudoId() == kPseudoIdScrollRightButton) {
      scroller->scrollBy(dx, 0);
    }
    GetDocument().SetFocusedElement(this,
                                    FocusParams(SelectionBehaviorOnFocus::kNone,
                                                mojom::blink::FocusType::kNone,
                                                /*capabilities=*/nullptr));
    event.SetDefaultHandled();
  }
  PseudoElement::DefaultEventHandler(event);
}

}  // namespace blink
