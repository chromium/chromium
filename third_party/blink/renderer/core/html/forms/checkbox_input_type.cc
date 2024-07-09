/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/html/forms/checkbox_input_type.h"

#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/keywords.h"
#include "third_party/blink/renderer/core/page/spatial_navigation.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"

namespace blink {

void CheckboxInputType::CountUsage() {
  CountUsageIfVisible(WebFeature::kInputTypeCheckbox);
}

ControlPart CheckboxInputType::AutoAppearance() const {
  return kCheckboxPart;
}

bool CheckboxInputType::ValueMissing(const String&) const {
  return GetElement().IsRequired() && !GetElement().Checked();
}

String CheckboxInputType::ValueMissingText() const {
  return GetLocale().QueryString(IDS_FORM_VALIDATION_VALUE_MISSING_CHECKBOX);
}

void CheckboxInputType::HandleKeyupEvent(KeyboardEvent& event) {
  // Use Space key simulated click by default.
  // Use Enter key simulated click when Spatial Navigation enabled.
  if (event.key() == " " ||
      (IsSpatialNavigationEnabled(GetElement().GetDocument().GetFrame()) &&
       event.key() == keywords::kCapitalEnter)) {
    DispatchSimulatedClickIfActive(event);
  }
}

ClickHandlingState* CheckboxInputType::WillDispatchClick() {
  // An event handler can use preventDefault or "return false" to reverse the
  // checking we do here.  The ClickHandlingState object contains what we need
  // to undo what we did here in didDispatchClick.

  ClickHandlingState* state = MakeGarbageCollected<ClickHandlingState>();

  state->checked = GetElement().Checked();
  state->indeterminate = GetElement().indeterminate();

  if (state->indeterminate)
    GetElement().setIndeterminate(false);

  GetElement().SetChecked(!state->checked,
                          TextFieldEventBehavior::kDispatchChangeEvent);
  is_in_click_handler_ = true;
  return state;
}

void CheckboxInputType::DidDispatchClick(Event& event,
                                         const ClickHandlingState& state) {
  if (event.defaultPrevented() || event.DefaultHandled()) {
    GetElement().setIndeterminate(state.indeterminate);
    GetElement().SetChecked(state.checked);
  } else {
    if (event.isTrusted()) {
      // This is needed in order to match :user-valid/:user-invalid
      GetElement().SetUserHasEditedTheField();
    }
    GetElement().DispatchInputAndChangeEventIfNeeded();
  }
  is_in_click_handler_ = false;
  // The work we did in willDispatchClick was default handling.
  event.SetDefaultHandled();
}

bool CheckboxInputType::ShouldAppearIndeterminate() const {
  return GetElement().indeterminate();
}

}  // namespace blink
