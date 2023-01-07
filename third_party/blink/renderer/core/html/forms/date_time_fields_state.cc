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

#include "third_party/blink/renderer/core/html/forms/date_time_fields_state.h"

#include "third_party/blink/renderer/core/html/forms/form_controller.h"

namespace blink {

const unsigned DateTimeFieldsState::kEmptyValue = static_cast<unsigned>(-1);

static unsigned GetNumberFromFormControlState(const FormControlState& state,
                                              wtf_size_t index) {
  if (index >= state.ValueSize())
    return DateTimeFieldsState::kEmptyValue;
  bool parsed;
  unsigned const value = state[index].ToUInt(&parsed);
  return parsed ? value : DateTimeFieldsState::kEmptyValue;
}

static DateTimeFieldsState::AMPMValue GetAMPMFromFormControlState(
    const FormControlState& state,
    wtf_size_t index) {
  if (index >= state.ValueSize())
    return DateTimeFieldsState::kAMPMValueEmpty;
  const String value = state[index];
  if (value == "A")
    return DateTimeFieldsState::kAMPMValueAM;
  if (value == "P")
    return DateTimeFieldsState::kAMPMValuePM;
  return DateTimeFieldsState::kAMPMValueEmpty;
}

DateTimeFieldsState::DateTimeFieldsState()
    : year_(kEmptyValue),
      month_(kEmptyValue),
      day_of_month_(kEmptyValue),
      hour_(kEmptyValue),
      minute_(kEmptyValue),
      second_(kEmptyValue),
      millisecond_(kEmptyValue),
      week_of_year_(kEmptyValue),
      ampm_(kAMPMValueEmpty) {}

unsigned DateTimeFieldsState::Hour24() const {
  if (!HasHour() || !HasAMPM())
    return kEmptyValue;
  return (hour_ % 12) + (ampm_ == kAMPMValuePM ? 12 : 0);
}

void DateTimeFieldsState::SetHour24(unsigned hour24) {
  DCHECK_LT(hour24, 24u);
  if (hour24 >= 12) {
    ampm_ = kAMPMValuePM;
    hour_ = hour24 - 12;
  } else {
    ampm_ = kAMPMValueAM;
    hour_ = hour24;
  }
}

DateTimeFieldsState DateTimeFieldsState::RestoreFormControlState(
    const FormControlState& state) {
  DateTimeFieldsState date_time_fields_state;
  date_time_fields_state.SetYear(GetNumberFromFormControlState(state, 0));
  date_time_fields_state.SetMonth(GetNumberFromFormControlState(state, 1));
  date_time_fields_state.SetDayOfMonth(GetNumberFromFormControlState(state, 2));
  date_time_fields_state.SetHour(GetNumberFromFormControlState(state, 3));
  date_time_fields_state.SetMinute(GetNumberFromFormControlState(state, 4));
  date_time_fields_state.SetSecond(GetNumberFromFormControlState(state, 5));
  date_time_fields_state.SetMillisecond(
      GetNumberFromFormControlState(state, 6));
  date_time_fields_state.SetWeekOfYear(GetNumberFromFormControlState(state, 7));
  date_time_fields_state.SetAMPM(GetAMPMFromFormControlState(state, 8));
  return date_time_fields_state;
}

FormControlState DateTimeFieldsState::SaveFormControlState() const {
  FormControlState state;
  state.Append(HasYear() ? String::Number(year_) : g_empty_string);
  state.Append(HasMonth() ? String::Number(month_) : g_empty_string);
  state.Append(HasDayOfMonth() ? String::Number(day_of_month_)
                               : g_empty_string);
  state.Append(HasHour() ? String::Number(hour_) : g_empty_string);
  state.Append(HasMinute() ? String::Number(minute_) : g_empty_string);
  state.Append(HasSecond() ? String::Number(second_) : g_empty_string);
  state.Append(HasMillisecond() ? String::Number(millisecond_)
                                : g_empty_string);
  state.Append(HasWeekOfYear() ? String::Number(week_of_year_)
                               : g_empty_string);
  if (HasAMPM())
    state.Append(ampm_ == kAMPMValueAM ? "A" : "P");
  else
    state.Append(g_empty_string);
  return state;
}

}  // namespace blink
