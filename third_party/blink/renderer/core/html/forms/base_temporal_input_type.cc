/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/html/forms/base_temporal_input_type.h"

#include <limits>
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/core/html/forms/chooser_only_temporal_input_type_view.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/multiple_fields_temporal_input_type_view.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "third_party/blink/renderer/platform/wtf/date_math.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

static constexpr int kMsecPerMinute = base::Minutes(1).InMilliseconds();
static constexpr int kMsecPerSecond = base::Seconds(1).InMilliseconds();

String BaseTemporalInputType::BadInputText() const {
  return GetLocale().QueryString(IDS_FORM_VALIDATION_BAD_INPUT_DATETIME);
}

InputTypeView* BaseTemporalInputType::CreateView() {
  if (RuntimeEnabledFeatures::InputMultipleFieldsUIEnabled()) {
    return MakeGarbageCollected<MultipleFieldsTemporalInputTypeView>(
        GetElement(), *this);
  }
  return MakeGarbageCollected<ChooserOnlyTemporalInputTypeView>(GetElement(),
                                                                *this);
}

InputType::ValueMode BaseTemporalInputType::GetValueMode() const {
  return ValueMode::kValue;
}

double BaseTemporalInputType::ValueAsDate() const {
  return ValueAsDouble();
}

void BaseTemporalInputType::SetValueAsDate(
    const std::optional<base::Time>& value,
    ExceptionState&) const {
  GetElement().SetValue(SerializeWithDate(value));
}

double BaseTemporalInputType::ValueAsDouble() const {
  const Decimal value = ParseToNumber(GetElement().Value(), Decimal::Nan());
  return value.IsFinite() ? value.ToDouble()
                          : DateComponents::InvalidMilliseconds();
}

void BaseTemporalInputType::SetValueAsDouble(
    double new_value,
    TextFieldEventBehavior event_behavior,
    ExceptionState& exception_state) const {
  SetValueAsDecimal(Decimal::FromDouble(new_value), event_behavior,
                    exception_state);
}

bool BaseTemporalInputType::TypeMismatchFor(const String& value) const {
  return !value.empty() && !ParseToDateComponents(value, nullptr);
}

bool BaseTemporalInputType::TypeMismatch() const {
  return TypeMismatchFor(GetElement().Value());
}

String BaseTemporalInputType::ValueNotEqualText(const Decimal& value) const {
  return GetLocale().QueryString(IDS_FORM_VALIDATION_VALUE_NOT_EQUAL_DATETIME,
                                 LocalizeValue(Serialize(value)));
}

String BaseTemporalInputType::RangeOverflowText(const Decimal& maximum) const {
  return GetLocale().QueryString(IDS_FORM_VALIDATION_RANGE_OVERFLOW_DATETIME,
                                 LocalizeValue(Serialize(maximum)));
}

String BaseTemporalInputType::RangeUnderflowText(const Decimal& minimum) const {
  return GetLocale().QueryString(IDS_FORM_VALIDATION_RANGE_UNDERFLOW_DATETIME,
                                 LocalizeValue(Serialize(minimum)));
}

String BaseTemporalInputType::RangeInvalidText(const Decimal& minimum,
                                               const Decimal& maximum) const {
  DCHECK(minimum > maximum)
      << "RangeInvalidText should only be called with minimum>maximum";

  return GetLocale().QueryString(IDS_FORM_VALIDATION_RANGE_INVALID_DATETIME,
                                 LocalizeValue(Serialize(minimum)),
                                 LocalizeValue(Serialize(maximum)));
}

Decimal BaseTemporalInputType::DefaultValueForStepUp() const {
  return Decimal::FromDouble(
      ConvertToLocalTime(base::Time::Now()).InMillisecondsF());
}

Decimal BaseTemporalInputType::ParseToNumber(
    const String& source,
    const Decimal& default_value) const {
  DateComponents date;
  if (!ParseToDateComponents(source, &date))
    return default_value;
  double msec = date.MillisecondsSinceEpoch();
  DCHECK(std::isfinite(msec));
  return Decimal::FromDouble(msec);
}

bool BaseTemporalInputType::ParseToDateComponents(const String& source,
                                                  DateComponents* out) const {
  if (source.empty())
    return false;
  DateComponents ignored_result;
  if (!out)
    out = &ignored_result;
  return ParseToDateComponentsInternal(source, out);
}

String BaseTemporalInputType::Serialize(const Decimal& value) const {
  if (!value.IsFinite())
    return String();
  DateComponents date;
  if (!SetMillisecondToDateComponents(value.ToDouble(), &date))
    return String();
  return SerializeWithComponents(date);
}

String BaseTemporalInputType::SerializeWithComponents(
    const DateComponents& date) const {
  Decimal step;
  if (!GetElement().GetAllowedValueStep(&step))
    return date.ToString();
  if (step.Remainder(kMsecPerMinute).IsZero())
    return date.ToString(DateComponents::SecondFormat::kNone);
  if (step.Remainder(kMsecPerSecond).IsZero())
    return date.ToString(DateComponents::SecondFormat::kSecond);
  return date.ToString(DateComponents::SecondFormat::kMillisecond);
}

String BaseTemporalInputType::SerializeWithDate(
    const std::optional<base::Time>& value) const {
  if (!value)
    return g_empty_string;
  return Serialize(
      Decimal::FromDouble(value->InMillisecondsFSinceUnixEpochIgnoringNull()));
}

String BaseTemporalInputType::LocalizeValue(
    const String& proposed_value) const {
  DateComponents date;
  if (!ParseToDateComponents(proposed_value, &date))
    return proposed_value;

  String localized = GetElement().GetLocale().FormatDateTime(date);
  return localized.empty() ? proposed_value : localized;
}

String BaseTemporalInputType::VisibleValue() const {
  return LocalizeValue(GetElement().Value());
}

String BaseTemporalInputType::SanitizeValue(
    const String& proposed_value) const {
  return TypeMismatchFor(proposed_value) ? g_empty_string : proposed_value;
}

bool BaseTemporalInputType::SupportsReadOnly() const {
  return true;
}

bool BaseTemporalInputType::ShouldRespectListAttribute() {
  return true;
}

bool BaseTemporalInputType::ValueMissing(const String& value) const {
  // For text-mode input elements (including dates), the value is missing only
  // if it is mutable.
  // https://html.spec.whatwg.org/multipage/input.html#the-required-attribute
  return GetElement().IsRequired() && value.empty() &&
         !GetElement().IsDisabledOrReadOnly();
}

bool BaseTemporalInputType::MayTriggerVirtualKeyboard() const {
  return true;
}

bool BaseTemporalInputType::ShouldHaveSecondField(
    const DateComponents& date) const {
  StepRange step_range = CreateStepRange(kAnyIsDefaultStep);
  return date.Second() || date.Millisecond() ||
         !step_range.Minimum()
              .Remainder(static_cast<int>(kMsPerMinute))
              .IsZero() ||
         !step_range.Step().Remainder(static_cast<int>(kMsPerMinute)).IsZero();
}

}  // namespace blink
