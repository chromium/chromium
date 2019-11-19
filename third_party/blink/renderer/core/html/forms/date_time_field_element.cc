/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/html/forms/date_time_field_element.h"

#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/text_run_constructor.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

DateTimeFieldElement::FieldOwner::~FieldOwner() = default;

DateTimeFieldElement::DateTimeFieldElement(Document& document,
                                           FieldOwner& field_owner,
                                           DateTimeField type)
    : HTMLSpanElement(document), field_owner_(&field_owner), type_(type) {}

void DateTimeFieldElement::Trace(Visitor* visitor) {
  visitor->Trace(field_owner_);
  HTMLSpanElement::Trace(visitor);
}

float DateTimeFieldElement::ComputeTextWidth(const ComputedStyle& style,
                                             const String& text) {
  return style.GetFont().Width(ConstructTextRun(style.GetFont(), text, style));
}

void DateTimeFieldElement::DefaultEventHandler(Event& event) {
  if (event.IsKeyboardEvent()) {
    auto& keyboard_event = ToKeyboardEvent(event);
    if (!IsDisabled() && !IsFieldOwnerDisabled() && !IsFieldOwnerReadOnly()) {
      HandleKeyboardEvent(keyboard_event);
      if (keyboard_event.DefaultHandled()) {
        if (field_owner_)
          field_owner_->FieldDidChangeValueByKeyboard();
        return;
      }
    }
    DefaultKeyboardEventHandler(keyboard_event);
    if (field_owner_)
      field_owner_->FieldDidChangeValueByKeyboard();
    if (keyboard_event.DefaultHandled())
      return;
  }

  HTMLElement::DefaultEventHandler(event);
}

void DateTimeFieldElement::DefaultKeyboardEventHandler(
    KeyboardEvent& keyboard_event) {
  if (keyboard_event.type() != event_type_names::kKeydown)
    return;

  if (IsDisabled() || IsFieldOwnerDisabled())
    return;

  const String& key = keyboard_event.key();

  if (key == "ArrowLeft") {
    if (!field_owner_)
      return;
    // FIXME: We'd like to use FocusController::advanceFocus(FocusDirectionLeft,
    // ...) but it doesn't work for shadow nodes. webkit.org/b/104650
    if (!LocaleForOwner().IsRTL() && field_owner_->FocusOnPreviousField(*this))
      keyboard_event.SetDefaultHandled();
    return;
  }

  if (key == "ArrowRight") {
    if (!field_owner_)
      return;
    // FIXME: We'd like to use
    // FocusController::advanceFocus(FocusDirectionRight, ...)
    // but it doesn't work for shadow nodes. webkit.org/b/104650
    if (!LocaleForOwner().IsRTL() && field_owner_->FocusOnNextField(*this))
      keyboard_event.SetDefaultHandled();
    return;
  }

  if (IsFieldOwnerReadOnly())
    return;

  if (key == "ArrowDown") {
    if (keyboard_event.getModifierState("Alt"))
      return;
    keyboard_event.SetDefaultHandled();
    StepDown();
    return;
  }

  if (key == "ArrowUp") {
    keyboard_event.SetDefaultHandled();
    StepUp();
    return;
  }

  if (key == "Backspace" || key == "Delete") {
    keyboard_event.SetDefaultHandled();
    SetEmptyValue(kDispatchEvent);
    return;
  }
}

void DateTimeFieldElement::SetFocused(bool value, WebFocusType focus_type) {
  if (field_owner_) {
    if (value) {
      field_owner_->DidFocusOnField(focus_type);
    } else {
      field_owner_->DidBlurFromField(focus_type);
    }
  }

  ContainerNode::SetFocused(value, focus_type);
}

void DateTimeFieldElement::FocusOnNextField() {
  if (!field_owner_)
    return;
  field_owner_->FocusOnNextField(*this);
}

void DateTimeFieldElement::Initialize(const AtomicString& pseudo,
                                      const String& ax_help_text,
                                      int ax_minimum,
                                      int ax_maximum) {
  // On accessibility, DateTimeFieldElement acts like spin button.
  setAttribute(html_names::kRoleAttr, AtomicString("spinbutton"));
  setAttribute(html_names::kAriaPlaceholderAttr, AtomicString(Placeholder()));
  setAttribute(html_names::kAriaValueminAttr, AtomicString::Number(ax_minimum));
  setAttribute(html_names::kAriaValuemaxAttr, AtomicString::Number(ax_maximum));

  setAttribute(html_names::kAriaLabelAttr, AtomicString(ax_help_text));
  SetShadowPseudoId(pseudo);
  AppendChild(Text::Create(GetDocument(), VisibleValue()));
}

bool DateTimeFieldElement::IsDateTimeFieldElement() const {
  return true;
}

bool DateTimeFieldElement::IsFieldOwnerDisabled() const {
  return field_owner_ && field_owner_->IsFieldOwnerDisabled();
}

bool DateTimeFieldElement::IsFieldOwnerReadOnly() const {
  return field_owner_ && field_owner_->IsFieldOwnerReadOnly();
}

bool DateTimeFieldElement::IsDisabled() const {
  return FastHasAttribute(html_names::kDisabledAttr);
}

Locale& DateTimeFieldElement::LocaleForOwner() const {
  return GetDocument().GetCachedLocale(LocaleIdentifier());
}

AtomicString DateTimeFieldElement::LocaleIdentifier() const {
  return field_owner_ ? field_owner_->LocaleIdentifier() : g_null_atom;
}

float DateTimeFieldElement::MaximumWidth(const ComputedStyle&) {
  const float kPaddingLeftAndRight = 2;  // This should match to html.css.
  return kPaddingLeftAndRight;
}

void DateTimeFieldElement::SetDisabled() {
  // Set HTML attribute disabled to change apperance.
  SetBooleanAttribute(html_names::kDisabledAttr, true);
  SetNeedsStyleRecalc(kSubtreeStyleChange,
                      StyleChangeReasonForTracing::CreateWithExtraData(
                          style_change_reason::kPseudoClass,
                          style_change_extra_data::g_disabled));
}

bool DateTimeFieldElement::SupportsFocus() const {
  return !IsDisabled() && !IsFieldOwnerDisabled();
}

void DateTimeFieldElement::UpdateVisibleValue(EventBehavior event_behavior) {
  auto* const text_node = To<Text>(firstChild());
  const String new_visible_value = VisibleValue();
  DCHECK_GT(new_visible_value.length(), 0u);

  if (text_node->wholeText() == new_visible_value)
    return;

  text_node->ReplaceWholeText(new_visible_value);
  if (HasValue()) {
    setAttribute(html_names::kAriaValuenowAttr,
                 AtomicString::Number(ValueForARIAValueNow()));
    setAttribute(html_names::kAriaValuetextAttr,
                 AtomicString(new_visible_value));
  } else {
    removeAttribute(html_names::kAriaValuenowAttr);
    removeAttribute(html_names::kAriaValuetextAttr);
  }

  if (event_behavior == kDispatchEvent && field_owner_)
    field_owner_->FieldValueChanged();
}

int DateTimeFieldElement::ValueForARIAValueNow() const {
  return ValueAsInteger();
}

DateTimeField DateTimeFieldElement::Type() const {
  return type_;
}

}  // namespace blink
