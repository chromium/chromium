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

#include "third_party/blink/renderer/core/html/forms/date_time_numeric_field_element.h"

#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/layout/text_utils.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "third_party/blink/renderer/platform/text/text_run.h"

namespace blink {

int DateTimeNumericFieldElement::Range::ClampValue(int value) const {
  return std::min(std::max(value, minimum), maximum);
}

bool DateTimeNumericFieldElement::Range::IsInRange(int value) const {
  return value >= minimum && value <= maximum;
}

// ----------------------------

DateTimeNumericFieldElement::DateTimeNumericFieldElement(
    Document& document,
    FieldOwner& field_owner,
    DateTimeField type,
    const Range& range,
    const Range& hard_limits,
    const String& placeholder,
    const DateTimeNumericFieldElement::Step& step)
    : DateTimeFieldElement(document, field_owner, type),
      placeholder_(placeholder),
      range_(range),
      hard_limits_(hard_limits),
      step_(step),
      value_(0),
      has_value_(false) {
  DCHECK_NE(step_.step, 0);
  DCHECK_LE(range_.minimum, range_.maximum);
  DCHECK_LE(hard_limits_.minimum, hard_limits_.maximum);

  // We show a direction-neutral string such as "--" as a placeholder. It
  // should follow the direction of numeric values.
  if (LocaleForOwner().IsRTL()) {
    WTF::unicode::CharDirection dir =
        WTF::unicode::Direction(FormatValue(Maximum())[0]);
    if (dir == WTF::unicode::kLeftToRight ||
        dir == WTF::unicode::kEuropeanNumber ||
        dir == WTF::unicode::kArabicNumber) {
      SetInlineStyleProperty(CSSPropertyID::kUnicodeBidi,
                             CSSValueID::kBidiOverride);
      SetInlineStyleProperty(CSSPropertyID::kDirection, CSSValueID::kLtr);
    }
  }
}

float DateTimeNumericFieldElement::MaximumWidth(const ComputedStyle& style) {
  float maximum_width = ComputeTextWidth(placeholder_, style);
  maximum_width =
      std::max(maximum_width, ComputeTextWidth(FormatValue(Maximum()), style));
  maximum_width = std::max(maximum_width, ComputeTextWidth(Value(), style));
  return maximum_width + DateTimeFieldElement::MaximumWidth(style);
}

int DateTimeNumericFieldElement::DefaultValueForStepDown() const {
  return range_.maximum;
}

int DateTimeNumericFieldElement::DefaultValueForStepUp() const {
  return range_.minimum;
}

void DateTimeNumericFieldElement::SetFocused(
    bool value,
    mojom::blink::FocusType focus_type) {
  if (!value) {
    int type_ahead_value = TypeAheadValue();
    type_ahead_buffer_.Clear();
    if (type_ahead_value >= 0)
      SetValueAsInteger(type_ahead_value, kDispatchEvent);
  }
  DateTimeFieldElement::SetFocused(value, focus_type);
}

String DateTimeNumericFieldElement::FormatValue(int value) const {
  Locale& locale = LocaleForOwner();
  if (hard_limits_.maximum > 999)
    return locale.ConvertToLocalizedNumber(String::Format("%04d", value));
  if (hard_limits_.maximum > 99)
    return locale.ConvertToLocalizedNumber(String::Format("%03d", value));
  return locale.ConvertToLocalizedNumber(String::Format("%02d", value));
}

void DateTimeNumericFieldElement::HandleKeyboardEvent(
    KeyboardEvent& keyboard_event) {
  DCHECK(!IsDisabled());
  if (keyboard_event.type() != event_type_names::kKeypress)
    return;

  UChar char_code = static_cast<UChar>(keyboard_event.charCode());
  String number =
      LocaleForOwner().ConvertFromLocalizedNumber(String(&char_code, 1u));
  const int digit = number[0] - '0';
  if (digit < 0 || digit > 9)
    return;

  unsigned maximum_length =
      DateTimeNumericFieldElement::FormatValue(range_.maximum).length();
  if (type_ahead_buffer_.length() >= maximum_length) {
    String current = type_ahead_buffer_.ToString();
    type_ahead_buffer_.Clear();
    unsigned desired_length = maximum_length - 1;
    type_ahead_buffer_.Append(current, current.length() - desired_length,
                              desired_length);
  }
  type_ahead_buffer_.Append(number);
  int new_value = TypeAheadValue();
  if (new_value >= hard_limits_.minimum) {
    SetValueAsInteger(new_value, kDispatchEvent);
  } else {
    has_value_ = false;
    UpdateVisibleValue(kDispatchEvent);
  }

  if (type_ahead_buffer_.length() >= maximum_length ||
      new_value * 10 > range_.maximum)
    FocusOnNextField();

  keyboard_event.SetDefaultHandled();
}

bool DateTimeNumericFieldElement::HasValue() const {
  return has_value_;
}

void DateTimeNumericFieldElement::Initialize(const AtomicString& pseudo,
                                             const String& ax_help_text) {
  DateTimeFieldElement::Initialize(pseudo, ax_help_text, range_.minimum,
                                   range_.maximum);
}

int DateTimeNumericFieldElement::Maximum() const {
  return range_.maximum;
}

String DateTimeNumericFieldElement::Placeholder() const {
  return placeholder_;
}

void DateTimeNumericFieldElement::SetEmptyValue(EventBehavior event_behavior) {
  if (IsDisabled())
    return;

  has_value_ = false;
  value_ = 0;
  type_ahead_buffer_.Clear();
  UpdateVisibleValue(event_behavior);
}

void DateTimeNumericFieldElement::SetValueAsInteger(
    int value,
    EventBehavior event_behavior) {
  value_ = hard_limits_.ClampValue(value);
  has_value_ = true;
  UpdateVisibleValue(event_behavior);
}

void DateTimeNumericFieldElement::StepDown() {
  int new_value =
      RoundDown(has_value_ ? value_ - 1 : DefaultValueForStepDown());
  if (!range_.IsInRange(new_value))
    new_value = RoundDown(range_.maximum);
  NotifyOwnerIfStepDownRollOver(has_value_, step_, value_, new_value);
  type_ahead_buffer_.Clear();
  SetValueAsInteger(new_value, kDispatchEvent);
}

void DateTimeNumericFieldElement::StepUp() {
  int new_value = RoundUp(has_value_ ? value_ + 1 : DefaultValueForStepUp());
  if (!range_.IsInRange(new_value))
    new_value = RoundUp(range_.minimum);
  NotifyOwnerIfStepUpRollOver(has_value_, step_, value_, new_value);
  type_ahead_buffer_.Clear();
  SetValueAsInteger(new_value, kDispatchEvent);
}

String DateTimeNumericFieldElement::Value() const {
  return has_value_ ? FormatValue(value_) : g_empty_string;
}

int DateTimeNumericFieldElement::ValueAsInteger() const {
  return has_value_ ? value_ : -1;
}

int DateTimeNumericFieldElement::TypeAheadValue() const {
  if (type_ahead_buffer_.length())
    return type_ahead_buffer_.ToString().ToInt();
  return -1;
}

String DateTimeNumericFieldElement::VisibleValue() const {
  if (type_ahead_buffer_.length())
    return FormatValue(TypeAheadValue());
  return has_value_ ? Value() : placeholder_;
}

int DateTimeNumericFieldElement::RoundDown(int n) const {
  n -= step_.step_base;
  if (n >= 0)
    n = n / step_.step * step_.step;
  else
    n = -((-n + step_.step - 1) / step_.step * step_.step);
  return n + step_.step_base;
}

int DateTimeNumericFieldElement::RoundUp(int n) const {
  n -= step_.step_base;
  if (n >= 0)
    n = (n + step_.step - 1) / step_.step * step_.step;
  else
    n = -(-n / step_.step * step_.step);
  return n + step_.step_base;
}

}  // namespace blink
