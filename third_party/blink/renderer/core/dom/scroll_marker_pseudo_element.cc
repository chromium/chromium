// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/scroll_marker_pseudo_element.h"

#include "cc/input/scroll_snap_data.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_scroll_into_view_options.h"
#include "third_party/blink/renderer/core/dom/scroll_marker_group_pseudo_element.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/scroll/scroll_alignment.h"
#include "third_party/blink/renderer/core/scroll/scroll_into_view_util.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"

namespace blink {

void ScrollMarkerPseudoElement::DefaultEventHandler(Event& event) {
  Element* originating_element = OriginatingElement();
  bool is_click =
      event.IsMouseEvent() && event.type() == event_type_names::kClick;
  bool is_enter = event.IsKeyboardEvent() &&
                  To<KeyboardEvent>(event).keyCode() == VKEY_RETURN;
  bool should_intercept = event.target() == this && originating_element &&
                          IsScrollMarkerPseudoElement() &&
                          (is_click || is_enter);
  if (should_intercept) {
    mojom::blink::ScrollIntoViewParamsPtr params =
        scroll_into_view_util::CreateScrollIntoViewParams(
            *originating_element->GetComputedStyle());
    originating_element->ScrollIntoViewNoVisualUpdate(std::move(params));
    event.SetDefaultHandled();
  }
  PseudoElement::DefaultEventHandler(event);
}

void ScrollMarkerPseudoElement::Dispose() {
  if (scroll_marker_group_) {
    scroll_marker_group_->RemoveFromFocusGroup(*this);
    scroll_marker_group_ = nullptr;
  }
  PseudoElement::Dispose();
}

void ScrollMarkerPseudoElement::Trace(Visitor* v) const {
  v->Trace(scroll_marker_group_);
  PseudoElement::Trace(v);
}

}  // namespace blink
