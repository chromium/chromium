// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/interest_hint_pseudo_element.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_keyboard_event_init.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"

namespace blink {

InterestHintPseudoElement::InterestHintPseudoElement(
    Element* originating_element,
    PseudoId pseudo_id)
    : PseudoElement(originating_element, pseudo_id) {
  UseCounter::Count(GetDocument(), WebFeature::kInterestHintPseudoElement);
  CHECK(RuntimeEnabledFeatures::HTMLInterestForInterestHintPseudoEnabled(
      GetDocument().GetExecutionContext()));
}

void InterestHintPseudoElement::HandleButtonActivation() {
  CHECK(RuntimeEnabledFeatures::HTMLInterestForInterestHintPseudoEnabled(
      GetDocument().GetExecutionContext()));
  Element& invoker = UltimateOriginatingElement();
  invoker.ShowInterestNow();
}

void InterestHintPseudoElement::DefaultEventHandler(Event& event) {
  bool should_intercept =
      event.IsMouseEvent() && event.type() == event_type_names::kClick;
  if (auto* keyboard_event = DynamicTo<KeyboardEvent>(event)) {
    auto key_code = keyboard_event->keyCode();
    should_intercept |= event.type() == event_type_names::kKeydown &&
                        (key_code == VKEY_RETURN || key_code == VKEY_SPACE);
  }
  if (event.RawTarget() == this && should_intercept) {
    HandleButtonActivation();
    event.SetDefaultHandled();
  }
  PseudoElement::DefaultEventHandler(event);
}

FocusableState InterestHintPseudoElement::SupportsFocus(
    UpdateBehavior update_behavior) const {
  if (PseudoElement::SupportsFocus(update_behavior) ==
      FocusableState::kNotFocusable) {
    return FocusableState::kNotFocusable;
  }
  return FocusableState::kFocusable;
}

}  // namespace blink
