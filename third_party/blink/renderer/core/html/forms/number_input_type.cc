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

const AtomicString& NumberInputType::FormControlType() const {
  return input_type_names::kNumber;
}

void NumberInputType::SetValue(const String& sanitized_value,
                               bool value_changed,
                               TextFieldEventBehavior event_behavior,
                               TextControlSetValueSelection selection) {
  if (!value_changed && sanitized_value.IsEmpty() &&
      !GetElement().InnerEditorValue().IsEmpty())
    GetElement().UpdateView();
  TextFieldInputType::SetValue(sanitized_value, value_changed, event_behavior,
                               selection);
}

double NumberInputType::ValueAsDouble() const {
  return ParseToDoubleForNumberType(GetElement().value());
}

void NumberInputType::SetValueAsDouble(double new_value,
                                       TextFieldEventBehavior event_behavior,
                                       ExceptionState& exception_state) const {
  GetElement().setValue(SerializeForNumberType(new_value), event_behavior);
}

void NumberInputType::SetValueAsDecimal(const Decimal& new_value,
                                        TextFieldEventBehavior event_behavior,
                                        ExceptionState& exception_state) const {
  GetElement().setValue(SerializeForNumberType(new_value), event_behavior);
}

bool NumberInputType::TypeMismatchFor(const String& value) const {
  return !value.IsEmpty() && !std::isfinite(ParseToDoubleForNumberType(value));
}

bool NumberInputType::TypeMismatch() const {
  DCHECK(!TypeMismatchFor(GetElement().value()));
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
  if (DeprecatedEqualIgnoringCase(step_string, "any"))
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

bool NumberInputType::IsSteppable() const {
  return true;
}

void NumberInputType::HandleKeydownEvent(KeyboardEvent& event) {
  EventQueueScope scope;
  HandleKeydownEventForSpinButton(event);
  if (!event.DefaultHandled())
    TextFieldInputType::HandleKeydownEvent(event);
}

void NumberInputType::HandleBeforeTextInsertedEvent(
    BeforeTextInsertedEvent& event) {
  event.SetText(GetLocale().StripInvalidNumberCharacters(event.GetText(),
                                                         "0123456789.Ee-+"));
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

static bool IsE(UChar ch) {
  return ch == 'e' || ch == 'E';
}

String NumberInputType::LocalizeValue(const String& proposed_value) const {
  if (proposed_value.IsEmpty())
    return proposed_value;
  // We don't localize scientific notations.
  if (proposed_value.Find(IsE) != kNotFound)
    return proposed_value;
  return GetElement().GetLocale().ConvertToLocalizedNumber(proposed_value);
}

String NumberInputType::VisibleValue() const {
  return LocalizeValue(GetElement().value());
}

String NumberInputType::ConvertFromVisibleValue(
    const String& visible_value) const {
  if (visible_value.IsEmpty())
    return visible_value;
  // We don't localize scientific notations.
  if (visible_value.Find(IsE) != kNotFound)
    return visible_value;
  return GetElement().GetLocale().ConvertFromLocalizedNumber(visible_value);
}

String NumberInputType::SanitizeValue(const String& proposed_value) const {
  if (proposed_value.IsEmpty())
    return proposed_value;
  return std::isfinite(ParseToDoubleForNumberType(proposed_value))
             ? proposed_value
             : g_empty_string;
}

void NumberInputType::WarnIfValueIsInvalid(const String& value) const {
  if (value.IsEmpty() || !GetElement().SanitizeValue(value).IsEmpty())
    return;
  AddWarningToConsole(
      "The specified value %s is not a valid number. The value must match to "
      "the following regular expression: "
      "-?(\\d+|\\d+\\.\\d+|\\.\\d+)([eE][-+]?\\d+)?",
      value);
}

bool NumberInputType::HasBadInput() const {
  String standard_value =
      ConvertFromVisibleValue(GetElement().InnerEditorValue());
  return !standard_value.IsEmpty() &&
         !std::isfinite(ParseToDoubleForNumberType(standard_value));
}

String NumberInputType::BadInputText() const {
  return GetLocale().QueryString(IDS_FORM_VALIDATION_BAD_INPUT_NUMBER);
}

String NumberInputType::RangeOverflowText(const Decimal& maximum) const {
  return GetLocale().QueryString(IDS_FORM_VALIDATION_RANGE_OVERFLOW,
                                 LocalizeValue(Serialize(maximum)));
}

String NumberInputType::RangeUnderflowText(const Decimal& minimum) const {
  return GetLocale().QueryString(IDS_FORM_VALIDATION_RANGE_UNDERFLOW,
                                 LocalizeValue(Serialize(minimum)));
}

bool NumberInputType::SupportsPlaceholder() const {
  return true;
}

void NumberInputType::MinOrMaxAttributeChanged() {
  TextFieldInputType::MinOrMaxAttributeChanged();

  if (GetElement().GetLayoutObject()) {
    GetElement()
        .GetLayoutObject()
        ->SetNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation(
            layout_invalidation_reason::kAttributeChanged);
  }
}

void NumberInputType::StepAttributeChanged() {
  TextFieldInputType::StepAttributeChanged();

  if (GetElement().GetLayoutObject()) {
    GetElement()
        .GetLayoutObject()
        ->SetNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation(
            layout_invalidation_reason::kAttributeChanged);
  }
}

bool NumberInputType::SupportsSelectionAPI() const {
  return false;
}

}  // namespace blink
