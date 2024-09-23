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

#include "third_party/blink/renderer/core/html/forms/month_input_type.h"

#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/forms/date_time_fields_state.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/platform/text/date_components.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "third_party/blink/renderer/platform/wtf/date_math.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/strings/grit/ax_strings.h"

namespace blink {

static const int kMonthDefaultStep = 1;
static const int kMonthDefaultStepBase = 0;
static const int kMonthStepScaleFactor = 1;

void MonthInputType::CountUsage() {
  CountUsageIfVisible(WebFeature::kInputTypeMonth);
}

double MonthInputType::ValueAsDate() const {
  DateComponents date;
  if (!ParseToDateComponents(GetElement().Value(), &date))
    return DateComponents::InvalidMilliseconds();
  double msec = date.MillisecondsSinceEpoch();
  DCHECK(std::isfinite(msec));
  return msec;
}

String MonthInputType::SerializeWithDate(
    const std::optional<base::Time>& value) const {
  DateComponents date;
  if (!value || !date.SetMillisecondsSinceEpochForMonth(
                    value->InMillisecondsFSinceUnixEpochIgnoringNull())) {
    return String();
  }
  return SerializeWithComponents(date);
}

Decimal MonthInputType::DefaultValueForStepUp() const {
  DateComponents date;
  date.SetMillisecondsSinceEpochForMonth(
      ConvertToLocalTime(base::Time::Now()).InMillisecondsF());
  double months = date.MonthsSinceEpoch();
  DCHECK(std::isfinite(months));
  return Decimal::FromDouble(months);
}

StepRange MonthInputType::CreateStepRange(
    AnyStepHandling any_step_handling) const {
  DEFINE_STATIC_LOCAL(
      const StepRange::StepDescription, step_description,
      (kMonthDefaultStep, kMonthDefaultStepBase, kMonthStepScaleFactor,
       StepRange::kParsedStepValueShouldBeInteger));

  return InputType::CreateStepRange(
      any_step_handling, Decimal::FromDouble(kMonthDefaultStepBase),
      Decimal::FromDouble(DateComponents::MinimumMonth()),
      Decimal::FromDouble(DateComponents::MaximumMonth()), step_description);
}

Decimal MonthInputType::ParseToNumber(const String& src,
                                      const Decimal& default_value) const {
  DateComponents date;
  if (!ParseToDateComponents(src, &date))
    return default_value;
  double months = date.MonthsSinceEpoch();
  DCHECK(std::isfinite(months));
  return Decimal::FromDouble(months);
}

bool MonthInputType::ParseToDateComponentsInternal(const String& string,
                                                   DateComponents* out) const {
  DCHECK(out);
  unsigned end;
  return out->ParseMonth(string, 0, end) && end == string.length();
}

bool MonthInputType::SetMillisecondToDateComponents(
    double value,
    DateComponents* date) const {
  DCHECK(date);
  return date->SetMonthsSinceEpoch(value);
}

bool MonthInputType::CanSetSuggestedValue() {
  return true;
}

void MonthInputType::WarnIfValueIsInvalid(const String& value) const {
  if (value != GetElement().SanitizeValue(value))
    AddWarningToConsole(
        "The specified value %s does not conform to the required format.  The "
        "format is \"yyyy-MM\" where yyyy is year in four or more digits, and "
        "MM is 01-12.",
        value);
}

String MonthInputType::FormatDateTimeFieldsState(
    const DateTimeFieldsState& date_time_fields_state) const {
  if (!date_time_fields_state.HasMonth() || !date_time_fields_state.HasYear())
    return g_empty_string;
  return String::Format("%04u-%02u", date_time_fields_state.Year(),
                        date_time_fields_state.Month());
}

void MonthInputType::SetupLayoutParameters(
    DateTimeEditElement::LayoutParameters& layout_parameters,
    const DateComponents& date) const {
  layout_parameters.date_time_format = layout_parameters.locale.MonthFormat();
  layout_parameters.fallback_date_time_format = "yyyy-MM";
  if (!ParseToDateComponents(
          GetElement().FastGetAttribute(html_names::kMinAttr),
          &layout_parameters.minimum))
    layout_parameters.minimum = DateComponents();
  if (!ParseToDateComponents(
          GetElement().FastGetAttribute(html_names::kMaxAttr),
          &layout_parameters.maximum))
    layout_parameters.maximum = DateComponents();
  layout_parameters.placeholder_for_month = "--";
  layout_parameters.placeholder_for_year = "----";
}

bool MonthInputType::IsValidFormat(bool has_year,
                                   bool has_month,
                                   bool has_week,
                                   bool has_day,
                                   bool has_ampm,
                                   bool has_hour,
                                   bool has_minute,
                                   bool has_second) const {
  return has_year && has_month;
}

String MonthInputType::AriaLabelForPickerIndicator() const {
  return GetLocale().QueryString(IDS_AX_CALENDAR_SHOW_MONTH_PICKER);
}

}  // namespace blink
