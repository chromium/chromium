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

#include "third_party/blink/renderer/core/html/forms/number_input_type.h"

#include <limits>
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/core/dom/events/scoped_event_queue.h"
#include "third_party/blink/renderer/core/events/before_text_inserted_event.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

static const int kNumberDefaultStep = 1;
static const int kNumberDefaultStepBase = 0;
static const int kNumberStepScaleFactor = 1;

struct RealNumberRenderSize {
  unsigned size_before_decimal_point;
  unsigned size_afte_decimal_point;

  RealNumberRenderSize(unsigned before, unsigned after)
      : size_before_decimal_point(before), size_afte_decimal_point(after) {}

  RealNumberRenderSize Max(const RealNumberRenderSize& other) const {
    return RealNumberRenderSize(
        std::max(size_before_decimal_point, other.size_before_decimal_point),
        std::max(size_afte_decimal_point, other.size_afte_decimal_point));
  }
};

static RealNumberRenderSize CalculateRenderSize(const Decimal& value) {
  DCHECK(value.IsFinite());
  const unsigned size_of_digits =
      String::Number(value.Value().Coefficient()).length();
  const unsigned size_of_sign = value.IsNegative() ? 1 : 0;
  const int exponent = value.Exponent();
  if (exponent >= 0)
    return RealNumberRenderSize(size_of_sign + size_of_digits, 0);

  const int size_before_decimal_point = exponent + size_of_digits;
  if (size_before_decimal_point > 0) {
    // In case of "123.456"
    return RealNumberRenderSize(size_of_sign + size_before_decimal_point,
                                size_of_digits - size_before_decimal_point);
  }

  // In case of "0.00012345"
  const unsigned kSizeOfZero = 1;
  const unsigned number_of_zero_after_decimal_point =
      -size_before_decimal_point;
  return RealNumberRenderSize(
      size_of_sign + kSizeOfZero,
      number_of_zero_after_decimal_point + size_of_digits);
}

void NumberInputType::CountUsage() {
  CountUsageIfVisible(WebFeature::kInputTypeNumber);
}

void NumberInputType::SetValue(const String& sanitized_value,
                               bool value_changed,
                               TextFieldEventBehavior event_behavior,
                               TextControlSetValueSelection selection) {
  if (!value_changed && sanitized_value.empty() &&
      !GetElement().InnerEditorValue().empty())
    GetElement().UpdateView();
  TextFieldInputType::SetValue(sanitized_value, value_changed, event_behavior,
                               selection);
}

double NumberInputType::ValueAsDouble() const {
  return ParseToDoubleForNumberType(GetElement().Value());
}

void NumberInputType::SetValueAsDouble(double new_value,
                                       TextFieldEventBehavior event_behavior,
                                       ExceptionState& exception_state) const {
  GetElement().SetValue(SerializeForNumberType(new_value), event_behavior);
}

void NumberInputType::SetValueAsDecimal(const Decimal& new_value,
                                        TextFieldEventBehavior event_behavior,
                                        ExceptionState& exception_state) const {
  GetElement().SetValue(SerializeForNumberType(new_value), event_behavior);
}

bool NumberInputType::TypeMismatchFor(const String& value) const {
  return !value.empty() && !std::isfinite(ParseToDoubleForNumberType(value));
}

bool NumberInputType::TypeMismatch() const {
  DCHECK(!TypeMismatchFor(GetElement().Value()));
  return false;
}

StepRange NumberInputType::CreateStepRange(
    AnyStepHandling any_step_handling) const {
  DEFINE_STATIC_LOCAL(
      const StepRange::StepDescription, step_description,
      (kNumberDefaultStep, kNumberDefaultStepBase, kNumberStepScaleFactor));
  const Decimal double_max =
      Decimal::FromDouble(std::numeric_limits<double>::max());
  return InputType::CreateStepRange(any_step_handling, kNumberDefaultStepBase,
                                    -double_max, double_max, step_description);
}

bool NumberInputType::SizeShouldIncludeDecoration(int default_size,
                                                  int& preferred_size) const {
  preferred_size = default_size;

  const String step_string =
      GetElement().FastGetAttribute(html_names::kStepAttr);
  if (EqualIgnoringASCIICase(step_string, "any"))
    return false;

  const Decimal minimum = ParseToDecimalForNumberType(
      GetElement().FastGetAttribute(html_names::kMinAttr));
  if (!minimum.IsFinite())
    return false;

  const Decimal maximum = ParseToDecimalForNumberType(
      GetElement().FastGetAttribute(html_names::kMaxAttr));
  if (!maximum.IsFinite())
    return false;

  const Decimal step = ParseToDecimalForNumberType(step_string, 1);
  DCHECK(step.IsFinite());

  RealNumberRenderSize size = CalculateRenderSize(minimum).Max(
      CalculateRenderSize(maximum).Max(CalculateRenderSize(step)));

  preferred_size = size.size_before_decimal_point +
                   size.size_afte_decimal_point +
                   (size.size_afte_decimal_point ? 1 : 0);

  return true;
}

static bool IsE(UChar ch) {
  return ch == 'e' || ch == 'E';
}

void NumberInputType::HandleKeydownEvent(KeyboardEvent& event) {
  EventQueueScope scope;
  HandleKeydownEventForSpinButton(event);
  if (!event.DefaultHandled())
    TextFieldInputType::HandleKeydownEvent(event);
}

void NumberInputType::HandleBeforeTextInsertedEvent(
    BeforeTextInsertedEvent& event) {
  Locale& locale = GetLocale();

  // If the cleaned up text doesn't match input text, don't insert partial input
  // since it could be an incorrect paste.
  String updated_event_text =
      locale.StripInvalidNumberCharacters(event.GetText(), "0123456789.Ee-+");

  // Check if locale supports more cleanup rules
  if (!locale.UsesSingleCharNumberFiltering()) {
    event.SetText(updated_event_text);
    return;
  }

  // Get left and right of cursor
  String original_value = GetElement().InnerEditorValue();
  String left_half = original_value.Substring(0, GetElement().selectionStart());
  String right_half = original_value.Substring(GetElement().selectionEnd());

  // Process 1 char at a time
  unsigned len = updated_event_text.length();
  StringBuilder final_event_text;
  for (unsigned i = 0; i < len; ++i) {
    UChar c = updated_event_text[i];

    // For a decimal point input:
    // - Reject if the editing value already contains another decimal point
    // - Reject if the editing value contains 'e' and the caret is placed
    // after the 'e'.
    // - Reject if the editing value contains '+' or '-' and the caret is
    // placed before it unless it's after an e
    if (locale.IsDecimalSeparator(c)) {
      if (locale.HasDecimalSeparator(left_half) ||
          locale.HasDecimalSeparator(right_half) ||
          left_half.Find(IsE) != kNotFound ||
          locale.HasSignNotAfterE(right_half))
        continue;
    }
    // For 'e' input:
    // - Reject if the editing value already contains another 'e'
    // - Reject if the editing value contains a decimal point, and the caret
    // is placed before it
    else if (IsE(c)) {
      if (left_half.Find(IsE) != kNotFound ||
          right_half.Find(IsE) != kNotFound ||
          locale.HasDecimalSeparator(right_half))
        continue;
    }
    // For '-' or '+' input:
    // - Reject if the editing value already contains two signs
    // - Reject if the editing value contains 'e' and the caret is placed
    // neither at the beginning of the value nor just after 'e'
    else if (locale.IsSignPrefix(c)) {
      String both_halves = left_half + right_half;
      if (locale.HasTwoSignChars(both_halves) ||
          (both_halves.Find(IsE) != kNotFound &&
           !(left_half == "" || IsE(left_half[left_half.length() - 1]))))
        continue;
    }
    // For a digit input:
    // - Reject if the first letter of the editing value is a sign and the
    // caret is placed just before it
    // - Reject if the editing value contains 'e' + a sign, and the caret is
    // placed between them.
    else if (locale.IsDigit(c)) {
      if ((left_half.empty() && !right_half.empty() &&
           locale.IsSignPrefix(right_half[0])) ||
          (!left_half.empty() && IsE(left_half[left_half.length() - 1]) &&
           !right_half.empty() && locale.IsSignPrefix(right_half[0])))
        continue;
    }

    // Add character
    left_half = left_half + c;
    final_event_text.Append(c);
  }
  event.SetText(final_event_text.ToString());
}

Decimal NumberInputType::ParseToNumber(const String& src,
                                       const Decimal& default_value) const {
  return ParseToDecimalForNumberType(src, default_value);
}

String NumberInputType::Serialize(const Decimal& value) const {
  if (!value.IsFinite())
    return String();
  return SerializeForNumberType(value);
}

String NumberInputType::LocalizeValue(const String& proposed_value) const {
  if (proposed_value.empty())
    return proposed_value;
  // We don't localize scientific notations.
  if (proposed_value.Find(IsE) != kNotFound)
    return proposed_value;
  return GetElement().GetLocale().ConvertToLocalizedNumber(proposed_value);
}

String NumberInputType::VisibleValue() const {
  return LocalizeValue(GetElement().Value());
}

String NumberInputType::ConvertFromVisibleValue(
    const String& visible_value) const {
  if (visible_value.empty())
    return visible_value;
  // We don't localize scientific notations.
  if (visible_value.Find(IsE) != kNotFound)
    return visible_value;
  return GetElement().GetLocale().ConvertFromLocalizedNumber(visible_value);
}

String NumberInputType::SanitizeValue(const String& proposed_value) const {
  if (proposed_value.empty())
    return proposed_value;
  return std::isfinite(ParseToDoubleForNumberType(proposed_value))
             ? proposed_value
             : g_empty_string;
}

void NumberInputType::WarnIfValueIsInvalid(const String& value) const {
  if (value.empty() || !GetElement().SanitizeValue(value).empty())
    return;
  AddWarningToConsole(
      "The specified value %s cannot be parsed, or is out of range.", value);
}

bool NumberInputType::HasBadInput() const {
  String standard_value =
      ConvertFromVisibleValue(GetElement().InnerEditorValue());
  return !standard_value.empty() &&
         !std::isfinite(ParseToDoubleForNumberType(standard_value));
}

String NumberInputType::BadInputText() const {
  return GetLocale().QueryString(IDS_FORM_VALIDATION_BAD_INPUT_NUMBER);
}

String NumberInputType::ValueNotEqualText(const Decimal& value) const {
  return GetLocale().QueryString(IDS_FORM_VALIDATION_VALUE_NOT_EQUAL,
                                 LocalizeValue(Serialize(value)));
}

String NumberInputType::RangeOverflowText(const Decimal& maximum) const {
  return GetLocale().QueryString(IDS_FORM_VALIDATION_RANGE_OVERFLOW,
                                 LocalizeValue(Serialize(maximum)));
}

String NumberInputType::RangeUnderflowText(const Decimal& minimum) const {
  return GetLocale().QueryString(IDS_FORM_VALIDATION_RANGE_UNDERFLOW,
                                 LocalizeValue(Serialize(minimum)));
}

String NumberInputType::RangeInvalidText(const Decimal& minimum,
                                         const Decimal& maximum) const {
  return GetLocale().QueryString(IDS_FORM_VALIDATION_RANGE_REVERSED,
                                 LocalizeValue(Serialize(minimum)),
                                 LocalizeValue(Serialize(maximum)));
}

bool NumberInputType::SupportsPlaceholder() const {
  return true;
}

void NumberInputType::MinOrMaxAttributeChanged() {
  TextFieldInputType::MinOrMaxAttributeChanged();

  if (GetElement().GetLayoutObject()) {
    GetElement()
        .GetLayoutObject()
        ->SetNeedsLayoutAndIntrinsicWidthsRecalcAndFullPaintInvalidation(
            layout_invalidation_reason::kAttributeChanged);
  }
}

void NumberInputType::StepAttributeChanged() {
  TextFieldInputType::StepAttributeChanged();

  if (GetElement().GetLayoutObject()) {
    GetElement()
        .GetLayoutObject()
        ->SetNeedsLayoutAndIntrinsicWidthsRecalcAndFullPaintInvalidation(
            layout_invalidation_reason::kAttributeChanged);
  }
}

bool NumberInputType::SupportsSelectionAPI() const {
  return false;
}

}  // namespace blink
