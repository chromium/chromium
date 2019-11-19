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

#include "third_party/blink/renderer/core/html/forms/password_input_type.h"

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/html/forms/form_controller.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/layout/layout_text_control_single_line.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

void PasswordInputType::CountUsage() {
  CountUsageIfVisible(WebFeature::kInputTypePassword);
  if (GetElement().FastHasAttribute(html_names::kMaxlengthAttr))
    CountUsageIfVisible(WebFeature::kInputTypePasswordMaxLength);
}

const AtomicString& PasswordInputType::FormControlType() const {
  return input_type_names::kPassword;
}

bool PasswordInputType::ShouldSaveAndRestoreFormControlState() const {
  return false;
}

FormControlState PasswordInputType::SaveFormControlState() const {
  // Should never save/restore password fields.
  NOTREACHED();
  return FormControlState();
}

void PasswordInputType::RestoreFormControlState(const FormControlState&) {
  // Should never save/restore password fields.
  NOTREACHED();
}

bool PasswordInputType::ShouldRespectListAttribute() {
  return false;
}

bool PasswordInputType::NeedsContainer() const {
  return RuntimeEnabledFeatures::PasswordRevealEnabled();
}

void PasswordInputType::CreateShadowSubtree() {
  BaseTextInputType::CreateShadowSubtree();

  if (RuntimeEnabledFeatures::PasswordRevealEnabled()) {
    Element* container = ContainerElement();
    Element* view_port = GetElement().UserAgentShadowRoot()->getElementById(
        shadow_element_names::EditingViewPort());
    DCHECK(container);
    DCHECK(view_port);
    container->InsertBefore(MakeGarbageCollected<PasswordRevealButtonElement>(
                                GetElement().GetDocument()),
                            view_port->nextSibling());
  }
}

void PasswordInputType::DidSetValueByUserEdit() {
  if (RuntimeEnabledFeatures::PasswordRevealEnabled()) {
    // If the last character is deleted, we hide the reveal button.
    if (GetElement().value().IsEmpty()) {
      should_show_reveal_button_ = false;
    }
    UpdatePasswordRevealButton();
  }
  BaseTextInputType::DidSetValueByUserEdit();
}

void PasswordInputType::DidSetValue(const String& string, bool value_changed) {
  if (RuntimeEnabledFeatures::PasswordRevealEnabled()) {
    if (value_changed) {
      // Hide the password if the value is changed by script.
      should_show_reveal_button_ = false;
      UpdatePasswordRevealButton();
    }
  }
  BaseTextInputType::DidSetValue(string, value_changed);
}

void PasswordInputType::UpdateView() {
  BaseTextInputType::UpdateView();

  if (RuntimeEnabledFeatures::PasswordRevealEnabled())
    UpdatePasswordRevealButton();
}

void PasswordInputType::UpdatePasswordRevealButton() {
  Element* button = GetElement().UserAgentShadowRoot()->getElementById(
      shadow_element_names::PasswordRevealButton());

  // Update the glyph.
  const AtomicString reveal("reveal");
  if (GetElement().ShouldRevealPassword())
    button->classList().Add(reveal);
  else
    button->classList().Remove(reveal);

  // Update the visibility.
  if (should_show_reveal_button_) {
    // Show the reveal button only when the width is enough for the reveal
    // button plus a few characters. (The number of characters slightly varies
    // based on the font size/family).
    const float kRevealButtonWidthEm = 1.3;  // 1.3em
    const float kPasswordMinWidthEm =
        0.7;                       // 0.7em which is enough for ~2 chars.
    const int kLeftMarginPx = 3;   // 3px
    const int kRightMarginPx = 3;  // 3px
    float current_width = GetElement().getBoundingClientRect()->width();
    float width_needed = GetElement().ComputedStyleRef().FontSize() *
                             (kRevealButtonWidthEm + kPasswordMinWidthEm) +
                         kLeftMarginPx + kRightMarginPx;

    if (current_width >= width_needed) {
      button->RemoveInlineStyleProperty(CSSPropertyID::kDisplay);
    }
  } else {
    button->SetInlineStyleProperty(CSSPropertyID::kDisplay, CSSValueID::kNone);
    // Always obscure password when the reveal button is hidden.
    // (ex. out of focus)
    GetElement().SetShouldRevealPassword(false);
  }
}

void PasswordInputType::HandleBlurEvent() {
  if (RuntimeEnabledFeatures::PasswordRevealEnabled()) {
    should_show_reveal_button_ = false;
    UpdatePasswordRevealButton();
  }

  BaseTextInputType::HandleBlurEvent();
}

void PasswordInputType::HandleBeforeTextInsertedEvent(
    BeforeTextInsertedEvent& event) {
  if (RuntimeEnabledFeatures::PasswordRevealEnabled()) {
    // This is the only scenario we go from no reveal button to showing the
    // reveal button: the password is empty and we have some user input.
    if (GetElement().value().IsEmpty())
      should_show_reveal_button_ = true;
  }

  TextFieldInputType::HandleBeforeTextInsertedEvent(event);
}

void PasswordInputType::HandleKeydownEvent(KeyboardEvent& event) {
  if (RuntimeEnabledFeatures::PasswordRevealEnabled()) {
    if (should_show_reveal_button_) {
      // Alt-F8 to reveal/obscure password
      if (event.getModifierState("Alt") && event.key() == "F8") {
        GetElement().SetShouldRevealPassword(
            !GetElement().ShouldRevealPassword());
        UpdatePasswordRevealButton();
        event.SetDefaultHandled();
      }
    }
  }

  if (!event.DefaultHandled())
    BaseTextInputType::HandleKeydownEvent(event);
}
}  // namespace blink
