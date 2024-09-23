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

#include "third_party/blink/renderer/core/html/forms/date_time_symbolic_field_element.h"

#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/layout/text_utils.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/text/text_break_iterator.h"
#include "third_party/blink/renderer/platform/text/text_run.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"

namespace blink {

static AtomicString MakeVisibleEmptyValue(const Vector<String>& symbols) {
  unsigned maximum_length = 0;
  for (unsigned index = 0; index < symbols.size(); ++index)
    maximum_length =
        std::max(maximum_length, NumGraphemeClusters(symbols[index]));
  StringBuilder builder;
  builder.ReserveCapacity(maximum_length);
  for (unsigned length = 0; length < maximum_length; ++length)
    builder.Append('-');
  return builder.ToAtomicString();
}

DateTimeSymbolicFieldElement::DateTimeSymbolicFieldElement(
    Document& document,
    FieldOwner& field_owner,
    DateTimeField type,
    const Vector<String>& symbols,
    int minimum,
    int maximum)
    : DateTimeFieldElement(document, field_owner, type),
      symbols_(symbols),
      visible_empty_value_(MakeVisibleEmptyValue(symbols)),
      selected_index_(-1),
      type_ahead_(this),
      minimum_index_(minimum),
      maximum_index_(maximum) {
  DCHECK(!symbols.empty());
  DCHECK_GE(minimum_index_, 0);
  SECURITY_DCHECK(maximum_index_ < static_cast<int>(symbols_.size()));
  DCHECK_LE(minimum_index_, maximum_index_);
}

float DateTimeSymbolicFieldElement::MaximumWidth(const ComputedStyle& style) {
  float maximum_width = ComputeTextWidth(VisibleEmptyValue(), style);
  for (unsigned index = 0; index < symbols_.size(); ++index) {
    maximum_width =
        std::max(maximum_width, ComputeTextWidth(symbols_[index], style));
  }
  return maximum_width + DateTimeFieldElement::MaximumWidth(style);
}

void DateTimeSymbolicFieldElement::HandleKeyboardEvent(
    KeyboardEvent& keyboard_event) {
  if (keyboard_event.type() != event_type_names::kKeypress)
    return;

  const UChar char_code = WTF::unicode::ToLower(keyboard_event.charCode());
  if (char_code < ' ')
    return;

  keyboard_event.SetDefaultHandled();

  if (Type() == DateTimeField::kAMPM) {
    // Since AM/PM field has only 2 options, the type_ahead session should be
    // reset to enable fast toggling between the options.
    type_ahead_.ResetSession();
  }

  int index = type_ahead_.HandleEvent(keyboard_event, keyboard_event.charCode(),
                                      TypeAhead::kMatchPrefix |
                                          TypeAhead::kCycleFirstChar |
                                          TypeAhead::kMatchIndex);
  if (index < 0)
    return;
  SetValueAsInteger(index, kDispatchEvent);
}

bool DateTimeSymbolicFieldElement::HasValue() const {
  return selected_index_ >= 0;
}

void DateTimeSymbolicFieldElement::Initialize(const AtomicString& pseudo,
                                              const String& ax_help_text) {
  // The minimum and maximum below are exposed to users, and 1-based numbers
  // are natural for symbolic fields. For example, the minimum value of a
  // month field should be 1, not 0.
  DateTimeFieldElement::Initialize(pseudo, ax_help_text, minimum_index_ + 1,
                                   maximum_index_ + 1);
}

void DateTimeSymbolicFieldElement::SetEmptyValue(EventBehavior event_behavior) {
  if (IsDisabled())
    return;
  selected_index_ = kInvalidIndex;
  UpdateVisibleValue(event_behavior);
}

void DateTimeSymbolicFieldElement::SetValueAsInteger(
    int new_selected_index,
    EventBehavior event_behavior) {
  selected_index_ = std::max(
      0, std::min(new_selected_index, static_cast<int>(symbols_.size() - 1)));
  UpdateVisibleValue(event_behavior);
}

String DateTimeSymbolicFieldElement::Placeholder() const {
  return VisibleEmptyValue();
}

void DateTimeSymbolicFieldElement::StepDown() {
  if (HasValue()) {
    if (!IndexIsInRange(--selected_index_))
      selected_index_ = maximum_index_;
  } else {
    selected_index_ = maximum_index_;
  }
  UpdateVisibleValue(kDispatchEvent);
}

void DateTimeSymbolicFieldElement::StepUp() {
  if (HasValue()) {
    if (!IndexIsInRange(++selected_index_))
      selected_index_ = minimum_index_;
  } else {
    selected_index_ = minimum_index_;
  }
  UpdateVisibleValue(kDispatchEvent);
}

String DateTimeSymbolicFieldElement::Value() const {
  return HasValue() ? symbols_[selected_index_] : g_empty_string;
}

int DateTimeSymbolicFieldElement::ValueAsInteger() const {
  return selected_index_;
}

int DateTimeSymbolicFieldElement::ValueForARIAValueNow() const {
  // Synchronize with minimum/maximum adjustment in initialize().
  return selected_index_ + 1;
}

String DateTimeSymbolicFieldElement::VisibleEmptyValue() const {
  return visible_empty_value_;
}

String DateTimeSymbolicFieldElement::VisibleValue() const {
  return HasValue() ? symbols_[selected_index_] : VisibleEmptyValue();
}

int DateTimeSymbolicFieldElement::IndexOfSelectedOption() const {
  return selected_index_;
}

int DateTimeSymbolicFieldElement::OptionCount() const {
  return symbols_.size();
}

String DateTimeSymbolicFieldElement::OptionAtIndex(int index) const {
  return symbols_[index];
}

}  // namespace blink
