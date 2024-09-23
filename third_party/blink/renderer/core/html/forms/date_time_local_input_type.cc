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

#include "third_party/blink/renderer/core/html/forms/date_time_local_input_type.h"

#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/forms/date_time_fields_state.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/text/date_components.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/strings/grit/ax_strings.h"

namespace blink {

static const int kDateTimeLocalDefaultStep = 60;
static const int kDateTimeLocalDefaultStepBase = 0;
static const int kDateTimeLocalStepScaleFactor = 1000;

void DateTimeLocalInputType::CountUsage() {
  CountUsageIfVisible(WebFeature::kInputTypeDateTimeLocal);
}

double DateTimeLocalInputType::ValueAsDate() const {
  // valueAsDate doesn't work for the datetime-local type according to the
  // standard.
  return DateComponents::InvalidMilliseconds();
}

void DateTimeLocalInputType::SetValueAsDate(
    const std::optional<base::Time>& value,
    ExceptionState& exception_state) const {
  // valueAsDate doesn't work for the datetime-local type according to the
  // standard.
  InputType::SetValueAsDate(value, exception_state);
}

StepRange DateTimeLocalInputType::CreateStepRange(
    AnyStepHandling any_step_handling) const {
  DEFINE_STATIC_LOCAL(const StepRange::StepDescription, step_description,
                      (kDateTimeLocalDefaultStep, kDateTimeLocalDefaultStepBase,
                       kDateTimeLocalStepScaleFactor,
                       StepRange::kScaledStepValueShouldBeInteger));

  return InputType::CreateStepRange(
      any_step_handling, kDateTimeLocalDefaultStepBase,
      Decimal::FromDouble(DateComponents::MinimumDateTime()),
      Decimal::FromDouble(DateComponents::MaximumDateTime()), step_description);
}

bool DateTimeLocalInputType::ParseToDateComponentsInternal(
    const String& string,
    DateComponents* out) const {
  DCHECK(out);
  unsigned end;
  return out->ParseDateTimeLocal(string, 0, end) && end == string.length();
}

bool DateTimeLocalInputType::SetMillisecondToDateComponents(
    double value,
    DateComponents* date) const {
  DCHECK(date);
  return date->SetMillisecondsSinceEpochForDateTimeLocal(value);
}

String DateTimeLocalInputType::LocalizeValue(
    const String& proposed_value) const {
  DateComponents date;
  if (!ParseToDateComponents(proposed_value, &date))
    return proposed_value;

  Locale::FormatType format_type = ShouldHaveSecondField(date)
                                       ? Locale::kFormatTypeMedium
                                       : Locale::kFormatTypeShort;
  String localized = GetElement().GetLocale().FormatDateTime(date, format_type);
  return localized.empty() ? proposed_value : localized;
}

void DateTimeLocalInputType::WarnIfValueIsInvalid(const String& value) const {
  if (!value.empty() && GetElement().SanitizeValue(value).empty())
    AddWarningToConsole(
        "The specified value %s does not conform to the required format.  The "
        "format is \"yyyy-MM-ddThh:mm\" followed by optional \":ss\" or "
        "\":ss.SSS\".",
        value);
}

String DateTimeLocalInputType::FormatDateTimeFieldsState(
    const DateTimeFieldsState& date_time_fields_state) const {
  if (!date_time_fields_state.HasDayOfMonth() ||
      !date_time_fields_state.HasMonth() || !date_time_fields_state.HasYear() ||
      !date_time_fields_state.HasHour() ||
      !date_time_fields_state.HasMinute() || !date_time_fields_state.HasAMPM())
    return g_empty_string;

  if (date_time_fields_state.HasMillisecond() &&
      date_time_fields_state.Millisecond()) {
    // According to WPTs and other browsers, we should remove trailing zeros
    // from the milliseconds field.
    auto milliseconds =
        String::Format("%03u", date_time_fields_state.Millisecond());
    while (milliseconds.length() &&
           milliseconds[milliseconds.length() - 1] == '0') {
      milliseconds.Truncate(milliseconds.length() - 1);
    }
    return String::Format(
        "%04u-%02u-%02uT%02u:%02u:%02u.%s", date_time_fields_state.Year(),
        date_time_fields_state.Month(), date_time_fields_state.DayOfMonth(),
        date_time_fields_state.Hour24(), date_time_fields_state.Minute(),
        date_time_fields_state.HasSecond() ? date_time_fields_state.Second()
                                           : 0,
        milliseconds.Ascii().c_str());
  }

  if (date_time_fields_state.HasSecond() && date_time_fields_state.Second()) {
    return String::Format(
        "%04u-%02u-%02uT%02u:%02u:%02u", date_time_fields_state.Year(),
        date_time_fields_state.Month(), date_time_fields_state.DayOfMonth(),
        date_time_fields_state.Hour24(), date_time_fields_state.Minute(),
        date_time_fields_state.Second());
  }

  return String::Format(
      "%04u-%02u-%02uT%02u:%02u", date_time_fields_state.Year(),
      date_time_fields_state.Month(), date_time_fields_state.DayOfMonth(),
      date_time_fields_state.Hour24(), date_time_fields_state.Minute());
}

void DateTimeLocalInputType::SetupLayoutParameters(
    DateTimeEditElement::LayoutParameters& layout_parameters,
    const DateComponents& date) const {
  if (ShouldHaveSecondField(date)) {
    layout_parameters.date_time_format =
        layout_parameters.locale.DateTimeFormatWithSeconds();
    layout_parameters.fallback_date_time_format = "yyyy-MM-dd'T'HH:mm:ss";
  } else {
    layout_parameters.date_time_format =
        layout_parameters.locale.DateTimeFormatWithoutSeconds();
    layout_parameters.fallback_date_time_format = "yyyy-MM-dd'T'HH:mm";
  }
  if (!ParseToDateComponents(
          GetElement().FastGetAttribute(html_names::kMinAttr),
          &layout_parameters.minimum))
    layout_parameters.minimum = DateComponents();
  if (!ParseToDateComponents(
          GetElement().FastGetAttribute(html_names::kMaxAttr),
          &layout_parameters.maximum))
    layout_parameters.maximum = DateComponents();
  layout_parameters.placeholder_for_day =
      GetLocale().QueryString(IDS_FORM_PLACEHOLDER_FOR_DAY_OF_MONTH_FIELD);
  layout_parameters.placeholder_for_month =
      GetLocale().QueryString(IDS_FORM_PLACEHOLDER_FOR_MONTH_FIELD);
  layout_parameters.placeholder_for_year =
      GetLocale().QueryString(IDS_FORM_PLACEHOLDER_FOR_YEAR_FIELD);
}

bool DateTimeLocalInputType::IsValidFormat(bool has_year,
                                           bool has_month,
                                           bool has_week,
                                           bool has_day,
                                           bool has_ampm,
                                           bool has_hour,
                                           bool has_minute,
                                           bool has_second) const {
  return has_year && has_month && has_day && has_ampm && has_hour && has_minute;
}

String DateTimeLocalInputType::AriaLabelForPickerIndicator() const {
  return GetLocale().QueryString(IDS_AX_CALENDAR_SHOW_DATE_TIME_LOCAL_PICKER);
}

String DateTimeLocalInputType::SanitizeValue(
    const String& proposed_string) const {
  if (BaseTemporalInputType::SanitizeValue(proposed_string) == g_empty_string)
    return g_empty_string;

  DateComponents components;
  if (!ParseToDateComponents(proposed_string, &components))
    return g_empty_string;

  DateTimeFieldsState fields;
  fields.SetMillisecond(components.Millisecond());
  fields.SetSecond(components.Second());
  fields.SetMinute(components.Minute());
  fields.SetHour24(components.Hour());
  fields.SetDayOfMonth(components.MonthDay());
  fields.SetMonth(components.Month() + 1);
  fields.SetYear(components.FullYear());
  return FormatDateTimeFieldsState(fields);
}

}  // namespace blink
