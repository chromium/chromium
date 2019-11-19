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

#include "third_party/blink/renderer/core/html/forms/week_input_type.h"

#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/forms/date_time_fields_state.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/platform/text/date_components.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

static const int kWeekDefaultStepBase =
    -259200000;  // The first day of 1970-W01.
static const int kWeekDefaultStep = 1;
static const int kWeekStepScaleFactor = 604800000;

void WeekInputType::CountUsage() {
  CountUsageIfVisible(WebFeature::kInputTypeWeek);
}

const AtomicString& WeekInputType::FormControlType() const {
  return input_type_names::kWeek;
}

StepRange WeekInputType::CreateStepRange(
    AnyStepHandling any_step_handling) const {
  DEFINE_STATIC_LOCAL(
      const StepRange::StepDescription, step_description,
      (kWeekDefaultStep, kWeekDefaultStepBase, kWeekStepScaleFactor,
       StepRange::kParsedStepValueShouldBeInteger));

  return InputType::CreateStepRange(
      any_step_handling, kWeekDefaultStepBase,
      Decimal::FromDouble(DateComponents::MinimumWeek()),
      Decimal::FromDouble(DateComponents::MaximumWeek()), step_description);
}

bool WeekInputType::ParseToDateComponentsInternal(const String& string,
                                                  DateComponents* out) const {
  DCHECK(out);
  unsigned end;
  return out->ParseWeek(string, 0, end) && end == string.length();
}

bool WeekInputType::SetMillisecondToDateComponents(double value,
                                                   DateComponents* date) const {
  DCHECK(date);
  return date->SetMillisecondsSinceEpochForWeek(value);
}

void WeekInputType::WarnIfValueIsInvalid(const String& value) const {
  if (value != GetElement().SanitizeValue(value))
    AddWarningToConsole(
        "The specified value %s does not conform to the required format.  The "
        "format is \"yyyy-Www\" where yyyy is year in four or more digits, and "
        "ww is 01-53.",
        value);
}

String WeekInputType::FormatDateTimeFieldsState(
    const DateTimeFieldsState& date_time_fields_state) const {
  if (!date_time_fields_state.HasYear() ||
      !date_time_fields_state.HasWeekOfYear())
    return g_empty_string;
  return String::Format("%04u-W%02u", date_time_fields_state.Year(),
                        date_time_fields_state.WeekOfYear());
}

void WeekInputType::SetupLayoutParameters(
    DateTimeEditElement::LayoutParameters& layout_parameters,
    const DateComponents&) const {
  layout_parameters.date_time_format = GetLocale().WeekFormatInLDML();
  layout_parameters.fallback_date_time_format = "yyyy-'W'ww";
  if (!ParseToDateComponents(
          GetElement().FastGetAttribute(html_names::kMinAttr),
          &layout_parameters.minimum))
    layout_parameters.minimum = DateComponents();
  if (!ParseToDateComponents(
          GetElement().FastGetAttribute(html_names::kMaxAttr),
          &layout_parameters.maximum))
    layout_parameters.maximum = DateComponents();
  layout_parameters.placeholder_for_year = "----";
}

bool WeekInputType::IsValidFormat(bool has_year,
                                  bool has_month,
                                  bool has_week,
                                  bool has_day,
                                  bool has_ampm,
                                  bool has_hour,
                                  bool has_minute,
                                  bool has_second) const {
  return has_year && has_week;
}

}  // namespace blink
