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

#include "third_party/blink/renderer/core/html/forms/time_input_type.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/forms/date_time_fields_state.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/platform/text/date_components.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "third_party/blink/renderer/platform/wtf/date_math.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

static const int kTimeDefaultStep = 60;
static const int kTimeDefaultStepBase = 0;
static const int kTimeStepScaleFactor = 1000;

TimeInputType::TimeInputType(HTMLInputElement& element)
    : BaseTemporalInputType(element) {}

void TimeInputType::CountUsage() {
  CountUsageIfVisible(WebFeature::kInputTypeTime);
}

const AtomicString& TimeInputType::FormControlType() const {
  return input_type_names::kTime;
}

Decimal TimeInputType::DefaultValueForStepUp() const {
  DateComponents date;
  date.SetMillisecondsSinceMidnight(
      ConvertToLocalTime(base::Time::Now()).InMillisecondsF());
  double milliseconds = date.MillisecondsSinceEpoch();
  DCHECK(std::isfinite(milliseconds));
  return Decimal::FromDouble(milliseconds);
}

StepRange TimeInputType::CreateStepRange(
    AnyStepHandling any_step_handling) const {
  DEFINE_STATIC_LOCAL(
      const StepRange::StepDescription, step_description,
      (kTimeDefaultStep, kTimeDefaultStepBase, kTimeStepScaleFactor,
       StepRange::kScaledStepValueShouldBeInteger));

  return InputType::CreateStepRange(
      any_step_handling, kTimeDefaultStepBase,
      Decimal::FromDouble(DateComponents::MinimumTime()),
      Decimal::FromDouble(DateComponents::MaximumTime()), step_description);
}

bool TimeInputType::ParseToDateComponentsInternal(const String& string,
                                                  DateComponents* out) const {
  DCHECK(out);
  unsigned end;
  return out->ParseTime(string, 0, end) && end == string.length();
}

bool TimeInputType::SetMillisecondToDateComponents(double value,
                                                   DateComponents* date) const {
  DCHECK(date);
  return date->SetMillisecondsSinceMidnight(value);
}

void TimeInputType::WarnIfValueIsInvalid(const String& value) const {
  if (value != GetElement().SanitizeValue(value)) {
    AddWarningToConsole(
        "The specified value %s does not conform to the required format.  The "
        "format is \"HH:mm\", \"HH:mm:ss\" or \"HH:mm:ss.SSS\" where HH is "
        "00-23, mm is 00-59, ss is 00-59, and SSS is 000-999.",
        value);
  }
}

String TimeInputType::LocalizeValue(const String& proposed_value) const {
  DateComponents date;
  if (!ParseToDateComponents(proposed_value, &date))
    return proposed_value;

  Locale::FormatType format_type = ShouldHaveSecondField(date)
                                       ? Locale::kFormatTypeMedium
                                       : Locale::kFormatTypeShort;

  String localized = GetElement().GetLocale().FormatDateTime(date, format_type);
  return localized.IsEmpty() ? proposed_value : localized;
}

String TimeInputType::FormatDateTimeFieldsState(
    const DateTimeFieldsState& date_time_fields_state) const {
  if (!date_time_fields_state.HasHour() ||
      !date_time_fields_state.HasMinute() || !date_time_fields_state.HasAMPM())
    return g_empty_string;
  if (date_time_fields_state.HasMillisecond() &&
      date_time_fields_state.Millisecond()) {
    return String::Format(
        "%02u:%02u:%02u.%03u", date_time_fields_state.Hour23(),
        date_time_fields_state.Minute(),
        date_time_fields_state.HasSecond() ? date_time_fields_state.Second()
                                           : 0,
        date_time_fields_state.Millisecond());
  }
  if (date_time_fields_state.HasSecond() && date_time_fields_state.Second()) {
    return String::Format("%02u:%02u:%02u", date_time_fields_state.Hour23(),
                          date_time_fields_state.Minute(),
                          date_time_fields_state.Second());
  }
  return String::Format("%02u:%02u", date_time_fields_state.Hour23(),
                        date_time_fields_state.Minute());
}

void TimeInputType::SetupLayoutParameters(
    DateTimeEditElement::LayoutParameters& layout_parameters,
    const DateComponents& date) const {
  if (ShouldHaveSecondField(date)) {
    layout_parameters.date_time_format = layout_parameters.locale.TimeFormat();
    layout_parameters.fallback_date_time_format = "HH:mm:ss";
  } else {
    layout_parameters.date_time_format =
        layout_parameters.locale.ShortTimeFormat();
    layout_parameters.fallback_date_time_format = "HH:mm";
  }
  if (!ParseToDateComponents(
          GetElement().FastGetAttribute(html_names::kMinAttr),
          &layout_parameters.minimum))
    layout_parameters.minimum = DateComponents();
  if (!ParseToDateComponents(
          GetElement().FastGetAttribute(html_names::kMaxAttr),
          &layout_parameters.maximum))
    layout_parameters.maximum = DateComponents();
}

bool TimeInputType::IsValidFormat(bool has_year,
                                  bool has_month,
                                  bool has_week,
                                  bool has_day,
                                  bool has_ampm,
                                  bool has_hour,
                                  bool has_minute,
                                  bool has_second) const {
  return has_hour && has_minute && has_ampm;
}

}  // namespace blink
