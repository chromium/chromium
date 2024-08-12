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

#include "third_party/blink/renderer/core/html/forms/date_input_type.h"

#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/forms/date_time_fields_state.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/platform/text/date_components.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "ui/strings/grit/ax_strings.h"

namespace blink {

static const int kDateDefaultStep = 1;
static const int kDateDefaultStepBase = 0;
static const int kDateStepScaleFactor = 86400000;

DateInputType::DateInputType(HTMLInputElement& element)
    : BaseTemporalInputType(Type::kDate, element) {}

void DateInputType::CountUsage() {
  CountUsageIfVisible(WebFeature::kInputTypeDate);
}

StepRange DateInputType::CreateStepRange(
    AnyStepHandling any_step_handling) const {
  DEFINE_STATIC_LOCAL(
      const StepRange::StepDescription, step_description,
      (kDateDefaultStep, kDateDefaultStepBase, kDateStepScaleFactor,
       StepRange::kParsedStepValueShouldBeInteger));

  return InputType::CreateStepRange(
      any_step_handling, kDateDefaultStepBase,
      Decimal::FromDouble(DateComponents::MinimumDate()),
      Decimal::FromDouble(DateComponents::MaximumDate()), step_description);
}

bool DateInputType::ParseToDateComponentsInternal(const String& string,
                                                  DateComponents* out) const {
  DCHECK(out);
  unsigned end;
  return out->ParseDate(string, 0, end) && end == string.length();
}

bool DateInputType::SetMillisecondToDateComponents(double value,
                                                   DateComponents* date) const {
  DCHECK(date);
  return date->SetMillisecondsSinceEpochForDate(value);
}

void DateInputType::WarnIfValueIsInvalid(const String& value) const {
  if (value != GetElement().SanitizeValue(value))
    AddWarningToConsole(
        "The specified value %s does not conform to the required format, "
        "\"yyyy-MM-dd\".",
        value);
}

String DateInputType::FormatDateTimeFieldsState(
    const DateTimeFieldsState& date_time_fields_state) const {
  if (!date_time_fields_state.HasDayOfMonth() ||
      !date_time_fields_state.HasMonth() || !date_time_fields_state.HasYear())
    return g_empty_string;

  return String::Format("%04u-%02u-%02u", date_time_fields_state.Year(),
                        date_time_fields_state.Month(),
                        date_time_fields_state.DayOfMonth());
}

void DateInputType::SetupLayoutParameters(
    DateTimeEditElement::LayoutParameters& layout_parameters,
    const DateComponents& date) const {
  layout_parameters.date_time_format = layout_parameters.locale.DateFormat();
  layout_parameters.fallback_date_time_format = "yyyy-MM-dd";
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

bool DateInputType::IsValidFormat(bool has_year,
                                  bool has_month,
                                  bool has_week,
                                  bool has_day,
                                  bool has_ampm,
                                  bool has_hour,
                                  bool has_minute,
                                  bool has_second) const {
  return has_year && has_month && has_day;
}

String DateInputType::AriaLabelForPickerIndicator() const {
  return GetLocale().QueryString(IDS_AX_CALENDAR_SHOW_DATE_PICKER);
}

}  // namespace blink
