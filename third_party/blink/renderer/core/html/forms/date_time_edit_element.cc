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

#include "third_party/blink/renderer/core/html/forms/date_time_edit_element.h"

#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/frame/use_counter_helper.h"
#include "third_party/blink/renderer/core/html/forms/date_time_field_elements.h"
#include "third_party/blink/renderer/core/html/forms/date_time_fields_state.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/text/date_time_format.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "third_party/blink/renderer/platform/wtf/date_math.h"

namespace blink {

class DateTimeEditBuilder : private DateTimeFormat::TokenHandler {
 public:
  // The argument objects must be alive until this object dies.
  DateTimeEditBuilder(DateTimeEditElement&,
                      const DateTimeEditElement::LayoutParameters&,
                      const DateComponents&);

  bool Build(const String&);

 private:
  bool NeedMillisecondField() const;
  bool ShouldAMPMFieldDisabled() const;
  bool ShouldDayOfMonthFieldDisabled() const;
  bool ShouldHourFieldDisabled() const;
  bool ShouldMillisecondFieldDisabled() const;
  bool ShouldMinuteFieldDisabled() const;
  bool ShouldSecondFieldDisabled() const;
  bool ShouldYearFieldDisabled() const;
  inline const StepRange& GetStepRange() const {
    return parameters_.step_range;
  }
  DateTimeNumericFieldElement::Step CreateStep(double ms_per_field_unit,
                                               double ms_per_field_size) const;

  // DateTimeFormat::TokenHandler functions.
  void VisitField(DateTimeFormat::FieldType, int) final;
  void VisitLiteral(const String&) final;

  DateTimeEditElement& EditElement() const;

  Member<DateTimeEditElement> edit_element_;
  const DateComponents date_value_;
  const DateTimeEditElement::LayoutParameters& parameters_;
  DateTimeNumericFieldElement::Range day_range_;
  DateTimeNumericFieldElement::Range hour23_range_;
  DateTimeNumericFieldElement::Range minute_range_;
  DateTimeNumericFieldElement::Range second_range_;
  DateTimeNumericFieldElement::Range millisecond_range_;
};

DateTimeEditBuilder::DateTimeEditBuilder(
    DateTimeEditElement& element,
    const DateTimeEditElement::LayoutParameters& layout_parameters,
    const DateComponents& date_value)
    : edit_element_(&element),
      date_value_(date_value),
      parameters_(layout_parameters),
      day_range_(1, 31),
      hour23_range_(0, 23),
      minute_range_(0, 59),
      second_range_(0, 59),
      millisecond_range_(0, 999) {
  if (date_value_.GetType() == DateComponents::kDate ||
      date_value_.GetType() == DateComponents::kDateTimeLocal) {
    if (parameters_.minimum.GetType() != DateComponents::kInvalid &&
        parameters_.maximum.GetType() != DateComponents::kInvalid &&
        parameters_.minimum.FullYear() == parameters_.maximum.FullYear() &&
        parameters_.minimum.Month() == parameters_.maximum.Month() &&
        parameters_.minimum.MonthDay() <= parameters_.maximum.MonthDay()) {
      day_range_.minimum = parameters_.minimum.MonthDay();
      day_range_.maximum = parameters_.maximum.MonthDay();
    }
  }

  if (date_value_.GetType() == DateComponents::kTime ||
      day_range_.IsSingleton()) {
    if (parameters_.minimum.GetType() != DateComponents::kInvalid &&
        parameters_.maximum.GetType() != DateComponents::kInvalid &&
        parameters_.minimum.Hour() <= parameters_.maximum.Hour()) {
      hour23_range_.minimum = parameters_.minimum.Hour();
      hour23_range_.maximum = parameters_.maximum.Hour();
    }
  }

  if (hour23_range_.IsSingleton() &&
      parameters_.minimum.Minute() <= parameters_.maximum.Minute()) {
    minute_range_.minimum = parameters_.minimum.Minute();
    minute_range_.maximum = parameters_.maximum.Minute();
  }
  if (minute_range_.IsSingleton() &&
      parameters_.minimum.Second() <= parameters_.maximum.Second()) {
    second_range_.minimum = parameters_.minimum.Second();
    second_range_.maximum = parameters_.maximum.Second();
  }
  if (second_range_.IsSingleton() &&
      parameters_.minimum.Millisecond() <= parameters_.maximum.Millisecond()) {
    millisecond_range_.minimum = parameters_.minimum.Millisecond();
    millisecond_range_.maximum = parameters_.maximum.Millisecond();
  }
}

bool DateTimeEditBuilder::Build(const String& format_string) {
  EditElement().ResetFields();

  // Mute UseCounter when constructing the DateTime object, to avoid counting
  // attributes on elements inside the user-agent shadow DOM.
  UseCounterMuteScope scope(EditElement());
  return DateTimeFormat::Parse(format_string, *this);
}

bool DateTimeEditBuilder::NeedMillisecondField() const {
  return date_value_.Millisecond() ||
         !GetStepRange()
              .Minimum()
              .Remainder(static_cast<int>(kMsPerSecond))
              .IsZero() ||
         !GetStepRange()
              .Step()
              .Remainder(static_cast<int>(kMsPerSecond))
              .IsZero();
}

void DateTimeEditBuilder::VisitField(DateTimeFormat::FieldType field_type,
                                     int count) {
  const int kCountForAbbreviatedMonth = 3;
  const int kCountForFullMonth = 4;
  const int kCountForNarrowMonth = 5;
  Document& document = EditElement().GetDocument();

  switch (field_type) {
    case DateTimeFormat::kFieldTypeDayOfMonth: {
      DateTimeFieldElement* field =
          MakeGarbageCollected<DateTimeDayFieldElement>(
              document, EditElement(), parameters_.placeholder_for_day,
              day_range_);
      EditElement().AddField(field);
      if (ShouldDayOfMonthFieldDisabled()) {
        field->SetValueAsDate(date_value_);
        field->SetDisabled();
      }
      return;
    }

    case DateTimeFormat::kFieldTypeHour11: {
      DateTimeNumericFieldElement::Step step =
          CreateStep(kMsPerHour, kMsPerHour * 12);
      DateTimeFieldElement* field =
          MakeGarbageCollected<DateTimeHour11FieldElement>(
              document, EditElement(), hour23_range_, step);
      EditElement().AddField(field);
      if (ShouldHourFieldDisabled()) {
        field->SetValueAsDate(date_value_);
        field->SetDisabled();
      }
      return;
    }

    case DateTimeFormat::kFieldTypeHour12: {
      DateTimeNumericFieldElement::Step step =
          CreateStep(kMsPerHour, kMsPerHour * 12);
      DateTimeFieldElement* field =
          MakeGarbageCollected<DateTimeHour12FieldElement>(
              document, EditElement(), hour23_range_, step);
      EditElement().AddField(field);
      if (ShouldHourFieldDisabled()) {
        field->SetValueAsDate(date_value_);
        field->SetDisabled();
      }
      return;
    }

    case DateTimeFormat::kFieldTypeHour23: {
      DateTimeNumericFieldElement::Step step =
          CreateStep(kMsPerHour, kMsPerDay);
      DateTimeFieldElement* field =
          MakeGarbageCollected<DateTimeHour23FieldElement>(
              document, EditElement(), hour23_range_, step);
      EditElement().AddField(field);
      if (ShouldHourFieldDisabled()) {
        field->SetValueAsDate(date_value_);
        field->SetDisabled();
      }
      return;
    }

    case DateTimeFormat::kFieldTypeHour24: {
      DateTimeNumericFieldElement::Step step =
          CreateStep(kMsPerHour, kMsPerDay);
      DateTimeFieldElement* field =
          MakeGarbageCollected<DateTimeHour24FieldElement>(
              document, EditElement(), hour23_range_, step);
      EditElement().AddField(field);
      if (ShouldHourFieldDisabled()) {
        field->SetValueAsDate(date_value_);
        field->SetDisabled();
      }
      return;
    }

    case DateTimeFormat::kFieldTypeMinute: {
      DateTimeNumericFieldElement::Step step =
          CreateStep(kMsPerMinute, kMsPerHour);
      DateTimeNumericFieldElement* field =
          MakeGarbageCollected<DateTimeMinuteFieldElement>(
              document, EditElement(), minute_range_, step);
      EditElement().AddField(field);
      if (ShouldMinuteFieldDisabled()) {
        field->SetValueAsDate(date_value_);
        field->SetDisabled();
      }
      return;
    }

    case DateTimeFormat::kFieldTypeMonth:  // Fallthrough.
    case DateTimeFormat::kFieldTypeMonthStandAlone: {
      int min_month = 0, max_month = 11;
      if (parameters_.minimum.GetType() != DateComponents::kInvalid &&
          parameters_.maximum.GetType() != DateComponents::kInvalid &&
          parameters_.minimum.FullYear() == parameters_.maximum.FullYear() &&
          parameters_.minimum.Month() <= parameters_.maximum.Month()) {
        min_month = parameters_.minimum.Month();
        max_month = parameters_.maximum.Month();
      }
      DateTimeFieldElement* field;
      switch (count) {
        case kCountForNarrowMonth:  // Fallthrough.
        case kCountForAbbreviatedMonth:
          field = MakeGarbageCollected<DateTimeSymbolicMonthFieldElement>(
              document, EditElement(),
              field_type == DateTimeFormat::kFieldTypeMonth
                  ? parameters_.locale.ShortMonthLabels()
                  : parameters_.locale.ShortStandAloneMonthLabels(),
              min_month, max_month);
          break;
        case kCountForFullMonth:
          field = MakeGarbageCollected<DateTimeSymbolicMonthFieldElement>(
              document, EditElement(),
              field_type == DateTimeFormat::kFieldTypeMonth
                  ? parameters_.locale.MonthLabels()
                  : parameters_.locale.StandAloneMonthLabels(),
              min_month, max_month);
          break;
        default:
          field = MakeGarbageCollected<DateTimeMonthFieldElement>(
              document, EditElement(), parameters_.placeholder_for_month,
              DateTimeNumericFieldElement::Range(min_month + 1, max_month + 1));
          break;
      }
      EditElement().AddField(field);
      if (min_month == max_month && min_month == date_value_.Month() &&
          date_value_.GetType() != DateComponents::kMonth) {
        field->SetValueAsDate(date_value_);
        field->SetDisabled();
      }
      return;
    }

    case DateTimeFormat::kFieldTypePeriod: {
      DateTimeFieldElement* field =
          MakeGarbageCollected<DateTimeAMPMFieldElement>(
              document, EditElement(), parameters_.locale.TimeAMPMLabels());
      EditElement().AddField(field);
      if (ShouldAMPMFieldDisabled()) {
        field->SetValueAsDate(date_value_);
        field->SetDisabled();
      }
      return;
    }

    case DateTimeFormat::kFieldTypeSecond: {
      DateTimeNumericFieldElement::Step step =
          CreateStep(kMsPerSecond, kMsPerMinute);
      DateTimeNumericFieldElement* field =
          MakeGarbageCollected<DateTimeSecondFieldElement>(
              document, EditElement(), second_range_, step);
      EditElement().AddField(field);
      if (ShouldSecondFieldDisabled()) {
        field->SetValueAsDate(date_value_);
        field->SetDisabled();
      }

      if (NeedMillisecondField()) {
        VisitLiteral(parameters_.locale.LocalizedDecimalSeparator());
        VisitField(DateTimeFormat::kFieldTypeFractionalSecond, 3);
      }
      return;
    }

    case DateTimeFormat::kFieldTypeFractionalSecond: {
      DateTimeNumericFieldElement::Step step = CreateStep(1, kMsPerSecond);
      DateTimeNumericFieldElement* field =
          MakeGarbageCollected<DateTimeMillisecondFieldElement>(
              document, EditElement(), millisecond_range_, step);
      EditElement().AddField(field);
      if (ShouldMillisecondFieldDisabled()) {
        field->SetValueAsDate(date_value_);
        field->SetDisabled();
      }
      return;
    }

    case DateTimeFormat::kFieldTypeWeekOfYear: {
      DateTimeNumericFieldElement::Range range(
          DateComponents::kMinimumWeekNumber,
          DateComponents::kMaximumWeekNumber);
      if (parameters_.minimum.GetType() != DateComponents::kInvalid &&
          parameters_.maximum.GetType() != DateComponents::kInvalid &&
          parameters_.minimum.FullYear() == parameters_.maximum.FullYear() &&
          parameters_.minimum.Week() <= parameters_.maximum.Week()) {
        range.minimum = parameters_.minimum.Week();
        range.maximum = parameters_.maximum.Week();
      }
      EditElement().AddField(MakeGarbageCollected<DateTimeWeekFieldElement>(
          document, EditElement(), range));
      return;
    }

    case DateTimeFormat::kFieldTypeYear: {
      DateTimeYearFieldElement::Parameters year_params;
      if (parameters_.minimum.GetType() == DateComponents::kInvalid) {
        year_params.minimum_year = DateComponents::MinimumYear();
        year_params.min_is_specified = false;
      } else {
        year_params.minimum_year = parameters_.minimum.FullYear();
        year_params.min_is_specified = true;
      }
      if (parameters_.maximum.GetType() == DateComponents::kInvalid) {
        year_params.maximum_year = DateComponents::MaximumYear();
        year_params.max_is_specified = false;
      } else {
        year_params.maximum_year = parameters_.maximum.FullYear();
        year_params.max_is_specified = true;
      }
      if (year_params.minimum_year > year_params.maximum_year) {
        std::swap(year_params.minimum_year, year_params.maximum_year);
        std::swap(year_params.min_is_specified, year_params.max_is_specified);
      }
      year_params.placeholder = parameters_.placeholder_for_year;
      DateTimeFieldElement* field =
          MakeGarbageCollected<DateTimeYearFieldElement>(
              document, EditElement(), year_params);
      EditElement().AddField(field);
      if (ShouldYearFieldDisabled()) {
        field->SetValueAsDate(date_value_);
        field->SetDisabled();
      }
      return;
    }

    default:
      return;
  }
}

bool DateTimeEditBuilder::ShouldAMPMFieldDisabled() const {
  return ShouldHourFieldDisabled() ||
         (hour23_range_.minimum < 12 && hour23_range_.maximum < 12 &&
          date_value_.Hour() < 12) ||
         (hour23_range_.minimum >= 12 && hour23_range_.maximum >= 12 &&
          date_value_.Hour() >= 12);
}

bool DateTimeEditBuilder::ShouldDayOfMonthFieldDisabled() const {
  return day_range_.IsSingleton() &&
         day_range_.minimum == date_value_.MonthDay() &&
         date_value_.GetType() != DateComponents::kDate;
}

bool DateTimeEditBuilder::ShouldHourFieldDisabled() const {
  if (hour23_range_.IsSingleton() &&
      hour23_range_.minimum == date_value_.Hour() &&
      !(ShouldMinuteFieldDisabled() && ShouldSecondFieldDisabled() &&
        ShouldMillisecondFieldDisabled()))
    return true;

  if (date_value_.GetType() == DateComponents::kTime)
    return false;
  DCHECK_EQ(date_value_.GetType(), DateComponents::kDateTimeLocal);

  if (ShouldDayOfMonthFieldDisabled()) {
    DCHECK_EQ(parameters_.minimum.FullYear(), parameters_.maximum.FullYear());
    DCHECK_EQ(parameters_.minimum.Month(), parameters_.maximum.Month());
    return false;
  }

  const Decimal decimal_ms_per_day(static_cast<int>(kMsPerDay));
  Decimal hour_part_of_minimum =
      (GetStepRange().StepBase().Abs().Remainder(decimal_ms_per_day) /
       static_cast<int>(kMsPerHour))
          .Floor();
  return hour_part_of_minimum == date_value_.Hour() &&
         GetStepRange().Step().Remainder(decimal_ms_per_day).IsZero();
}

bool DateTimeEditBuilder::ShouldMillisecondFieldDisabled() const {
  if (millisecond_range_.IsSingleton() &&
      millisecond_range_.minimum == date_value_.Millisecond())
    return true;

  const Decimal decimal_ms_per_second(static_cast<int>(kMsPerSecond));
  return GetStepRange().StepBase().Abs().Remainder(decimal_ms_per_second) ==
             date_value_.Millisecond() &&
         GetStepRange().Step().Remainder(decimal_ms_per_second).IsZero();
}

bool DateTimeEditBuilder::ShouldMinuteFieldDisabled() const {
  if (minute_range_.IsSingleton() &&
      minute_range_.minimum == date_value_.Minute())
    return true;

  const Decimal decimal_ms_per_hour(static_cast<int>(kMsPerHour));
  Decimal minute_part_of_minimum =
      (GetStepRange().StepBase().Abs().Remainder(decimal_ms_per_hour) /
       static_cast<int>(kMsPerMinute))
          .Floor();
  return minute_part_of_minimum == date_value_.Minute() &&
         GetStepRange().Step().Remainder(decimal_ms_per_hour).IsZero();
}

bool DateTimeEditBuilder::ShouldSecondFieldDisabled() const {
  if (second_range_.IsSingleton() &&
      second_range_.minimum == date_value_.Second())
    return true;

  const Decimal decimal_ms_per_minute(static_cast<int>(kMsPerMinute));
  Decimal second_part_of_minimum =
      (GetStepRange().StepBase().Abs().Remainder(decimal_ms_per_minute) /
       static_cast<int>(kMsPerSecond))
          .Floor();
  return second_part_of_minimum == date_value_.Second() &&
         GetStepRange().Step().Remainder(decimal_ms_per_minute).IsZero();
}

bool DateTimeEditBuilder::ShouldYearFieldDisabled() const {
  return parameters_.minimum.GetType() != DateComponents::kInvalid &&
         parameters_.maximum.GetType() != DateComponents::kInvalid &&
         parameters_.minimum.FullYear() == parameters_.maximum.FullYear() &&
         parameters_.minimum.FullYear() == date_value_.FullYear();
}

void DateTimeEditBuilder::VisitLiteral(const String& text) {
  DEFINE_STATIC_LOCAL(AtomicString, text_pseudo_id,
                      ("-webkit-datetime-edit-text"));
  DCHECK_GT(text.length(), 0u);
  auto* element =
      MakeGarbageCollected<HTMLDivElement>(EditElement().GetDocument());
  element->SetShadowPseudoId(text_pseudo_id);
  if (parameters_.locale.IsRTL() && text.length()) {
    WTF::unicode::CharDirection dir = WTF::unicode::Direction(text[0]);
    if (dir == WTF::unicode::kSegmentSeparator ||
        dir == WTF::unicode::kWhiteSpaceNeutral ||
        dir == WTF::unicode::kOtherNeutral)
      element->AppendChild(Text::Create(EditElement().GetDocument(),
                                        String(&kRightToLeftMarkCharacter, 1)));
  }
  element->AppendChild(Text::Create(EditElement().GetDocument(), text));
  EditElement().FieldsWrapperElement()->AppendChild(element);
}

DateTimeEditElement& DateTimeEditBuilder::EditElement() const {
  return *edit_element_;
}

DateTimeNumericFieldElement::Step DateTimeEditBuilder::CreateStep(
    double ms_per_field_unit,
    double ms_per_field_size) const {
  const Decimal ms_per_field_unit_decimal(static_cast<int>(ms_per_field_unit));
  const Decimal ms_per_field_size_decimal(static_cast<int>(ms_per_field_size));
  Decimal step_milliseconds = GetStepRange().Step();
  DCHECK(!ms_per_field_unit_decimal.IsZero());
  DCHECK(!ms_per_field_size_decimal.IsZero());
  DCHECK(!step_milliseconds.IsZero());

  DateTimeNumericFieldElement::Step step(1, 0);

  if (step_milliseconds.Remainder(ms_per_field_size_decimal).IsZero())
    step_milliseconds = ms_per_field_size_decimal;

  if (ms_per_field_size_decimal.Remainder(step_milliseconds).IsZero() &&
      step_milliseconds.Remainder(ms_per_field_unit_decimal).IsZero()) {
    step.step = static_cast<int>(
        (step_milliseconds / ms_per_field_unit_decimal).ToDouble());
    step.step_base = static_cast<int>(
        (GetStepRange().StepBase() / ms_per_field_unit_decimal)
            .Floor()
            .Remainder(ms_per_field_size_decimal / ms_per_field_unit_decimal)
            .ToDouble());
  }
  return step;
}

// ----------------------------

DateTimeEditElement::EditControlOwner::~EditControlOwner() = default;

DateTimeEditElement::DateTimeEditElement(Document& document,
                                         EditControlOwner& edit_control_owner)
    : HTMLDivElement(document), edit_control_owner_(&edit_control_owner) {
  SetHasCustomStyleCallbacks();
  SetShadowPseudoId(AtomicString("-webkit-datetime-edit"));
  setAttribute(html_names::kIdAttr, shadow_element_names::DateTimeEdit());
}

DateTimeEditElement::~DateTimeEditElement() = default;

void DateTimeEditElement::Trace(Visitor* visitor) {
  visitor->Trace(fields_);
  visitor->Trace(edit_control_owner_);
  HTMLDivElement::Trace(visitor);
}

inline Element* DateTimeEditElement::FieldsWrapperElement() const {
  CHECK(!firstChild() || IsA<Element>(firstChild()));
  return To<Element>(firstChild());
}

void DateTimeEditElement::AddField(DateTimeFieldElement* field) {
  if (fields_.size() >= kMaximumNumberOfFields)
    return;
  fields_.push_back(field);
  FieldsWrapperElement()->AppendChild(field);
}

bool DateTimeEditElement::AnyEditableFieldsHaveValues() const {
  for (const auto& field : fields_) {
    if (!field->IsDisabled() && field->HasValue())
      return true;
  }
  return false;
}

void DateTimeEditElement::BlurByOwner() {
  if (DateTimeFieldElement* field = FocusedField())
    field->blur();
}

scoped_refptr<ComputedStyle> DateTimeEditElement::CustomStyleForLayoutObject() {
  // FIXME: This is a kind of layout. We might want to introduce new
  // layoutObject.
  scoped_refptr<ComputedStyle> original_style = OriginalStyleForLayoutObject();
  scoped_refptr<ComputedStyle> style = ComputedStyle::Clone(*original_style);
  float width = 0;
  for (Node* child = FieldsWrapperElement()->firstChild(); child;
       child = child->nextSibling()) {
    auto* child_element = DynamicTo<Element>(child);
    if (!child_element)
      continue;
    if (child_element->IsDateTimeFieldElement()) {
      // We need to pass the ComputedStyle of this element because child
      // elements can't resolve inherited style at this timing.
      width += static_cast<DateTimeFieldElement*>(child_element)
                   ->MaximumWidth(*style);
    } else {
      // ::-webkit-datetime-edit-text case. It has no
      // border/padding/margin in html.css.
      width += DateTimeFieldElement::ComputeTextWidth(
          *style, child_element->textContent());
    }
  }
  style->SetWidth(Length::Fixed(ceilf(width)));
  return style;
}

void DateTimeEditElement::DidBlurFromField(WebFocusType focus_type) {
  if (edit_control_owner_)
    edit_control_owner_->DidBlurFromControl(focus_type);
}

void DateTimeEditElement::DidFocusOnField(WebFocusType focus_type) {
  if (edit_control_owner_)
    edit_control_owner_->DidFocusOnControl(focus_type);
}

void DateTimeEditElement::DisabledStateChanged() {
  UpdateUIState();
}

DateTimeFieldElement* DateTimeEditElement::FieldAt(
    wtf_size_t field_index) const {
  return field_index < fields_.size() ? fields_[field_index].Get() : nullptr;
}

wtf_size_t DateTimeEditElement::FieldIndexOf(
    const DateTimeFieldElement& field) const {
  for (wtf_size_t field_index = 0; field_index < fields_.size();
       ++field_index) {
    if (fields_[field_index] == &field)
      return field_index;
  }
  return kInvalidFieldIndex;
}

void DateTimeEditElement::FocusIfNoFocus() {
  if (FocusedFieldIndex() != kInvalidFieldIndex)
    return;
  FocusOnNextFocusableField(0);
}

void DateTimeEditElement::FocusByOwner(Element* old_focused_element) {
  if (old_focused_element && old_focused_element->IsDateTimeFieldElement()) {
    DateTimeFieldElement* old_focused_field =
        static_cast<DateTimeFieldElement*>(old_focused_element);
    wtf_size_t index = FieldIndexOf(*old_focused_field);
    GetDocument().UpdateStyleAndLayoutTreeForNode(old_focused_field);
    if (index != kInvalidFieldIndex && old_focused_field->IsFocusable()) {
      old_focused_field->focus();
      return;
    }
  }
  FocusOnNextFocusableField(0);
}

DateTimeFieldElement* DateTimeEditElement::FocusedField() const {
  return FieldAt(FocusedFieldIndex());
}

wtf_size_t DateTimeEditElement::FocusedFieldIndex() const {
  Element* const focused_field_element = GetDocument().FocusedElement();
  for (wtf_size_t field_index = 0; field_index < fields_.size();
       ++field_index) {
    if (fields_[field_index] == focused_field_element)
      return field_index;
  }
  return kInvalidFieldIndex;
}

void DateTimeEditElement::FieldValueChanged() {
  if (edit_control_owner_)
    edit_control_owner_->EditControlValueChanged();
}

bool DateTimeEditElement::FocusOnNextFocusableField(wtf_size_t start_index) {
  GetDocument().UpdateStyleAndLayoutTree();
  for (wtf_size_t field_index = start_index; field_index < fields_.size();
       ++field_index) {
    if (fields_[field_index]->IsFocusable()) {
      fields_[field_index]->focus();
      return true;
    }
  }
  return false;
}

bool DateTimeEditElement::FocusOnNextField(const DateTimeFieldElement& field) {
  const wtf_size_t start_field_index = FieldIndexOf(field);
  if (start_field_index == kInvalidFieldIndex)
    return false;
  return FocusOnNextFocusableField(start_field_index + 1);
}

bool DateTimeEditElement::FocusOnPreviousField(
    const DateTimeFieldElement& field) {
  const wtf_size_t start_field_index = FieldIndexOf(field);
  if (start_field_index == kInvalidFieldIndex)
    return false;
  GetDocument().UpdateStyleAndLayoutTree();
  wtf_size_t field_index = start_field_index;
  while (field_index > 0) {
    --field_index;
    if (fields_[field_index]->IsFocusable()) {
      fields_[field_index]->focus();
      return true;
    }
  }
  return false;
}

bool DateTimeEditElement::IsDateTimeEditElement() const {
  return true;
}

bool DateTimeEditElement::IsDisabled() const {
  return edit_control_owner_ &&
         edit_control_owner_->IsEditControlOwnerDisabled();
}

bool DateTimeEditElement::IsFieldOwnerDisabled() const {
  return IsDisabled();
}

bool DateTimeEditElement::IsFieldOwnerReadOnly() const {
  return IsReadOnly();
}

bool DateTimeEditElement::IsReadOnly() const {
  return edit_control_owner_ &&
         edit_control_owner_->IsEditControlOwnerReadOnly();
}

void DateTimeEditElement::GetLayout(const LayoutParameters& layout_parameters,
                                    const DateComponents& date_value) {
  // TODO(tkent): We assume this function never dispatches events. However this
  // can dispatch 'blur' event in Node::removeChild().

  DEFINE_STATIC_LOCAL(AtomicString, fields_wrapper_pseudo_id,
                      ("-webkit-datetime-edit-fields-wrapper"));
  if (!HasChildren()) {
    auto* element = MakeGarbageCollected<HTMLDivElement>(GetDocument());
    element->SetShadowPseudoId(fields_wrapper_pseudo_id);
    AppendChild(element);
  }
  Element* fields_wrapper = FieldsWrapperElement();

  wtf_size_t focused_field_index = FocusedFieldIndex();
  DateTimeFieldElement* const focused_field = FieldAt(focused_field_index);
  const AtomicString focused_field_id =
      focused_field ? focused_field->ShadowPseudoId() : g_null_atom;

  DateTimeEditBuilder builder(*this, layout_parameters, date_value);
  Node* last_child_to_be_removed = fields_wrapper->lastChild();
  if (!builder.Build(layout_parameters.date_time_format) || fields_.IsEmpty()) {
    last_child_to_be_removed = fields_wrapper->lastChild();
    builder.Build(layout_parameters.fallback_date_time_format);
  }

  if (focused_field_index != kInvalidFieldIndex) {
    for (wtf_size_t field_index = 0; field_index < fields_.size();
         ++field_index) {
      if (fields_[field_index]->ShadowPseudoId() == focused_field_id) {
        focused_field_index = field_index;
        break;
      }
    }
    if (DateTimeFieldElement* field =
            FieldAt(std::min(focused_field_index, fields_.size() - 1)))
      field->focus();
  }

  if (last_child_to_be_removed) {
    for (Node* child_node = fields_wrapper->firstChild(); child_node;
         child_node = fields_wrapper->firstChild()) {
      fields_wrapper->RemoveChild(child_node);
      if (child_node == last_child_to_be_removed)
        break;
    }
    SetNeedsStyleRecalc(
        kSubtreeStyleChange,
        StyleChangeReasonForTracing::Create(style_change_reason::kControl));
  }
}

AtomicString DateTimeEditElement::LocaleIdentifier() const {
  return edit_control_owner_ ? edit_control_owner_->LocaleIdentifier()
                             : g_null_atom;
}

void DateTimeEditElement::FieldDidChangeValueByKeyboard() {
  if (edit_control_owner_)
    edit_control_owner_->EditControlDidChangeValueByKeyboard();
}

void DateTimeEditElement::ReadOnlyStateChanged() {
  UpdateUIState();
}

void DateTimeEditElement::ResetFields() {
  for (const auto& field : fields_)
    field->RemoveEventHandler();
  fields_.Shrink(0);
}

void DateTimeEditElement::DefaultEventHandler(Event& event) {
  // In case of control owner forward event to control, e.g. DOM
  // dispatchEvent method.
  if (DateTimeFieldElement* field = FocusedField()) {
    field->DefaultEventHandler(event);
    if (event.DefaultHandled())
      return;
  }

  HTMLDivElement::DefaultEventHandler(event);
}

void DateTimeEditElement::SetValueAsDate(
    const LayoutParameters& layout_parameters,
    const DateComponents& date) {
  GetLayout(layout_parameters, date);
  for (const auto& field : fields_)
    field->SetValueAsDate(date);
}

void DateTimeEditElement::SetValueAsDateTimeFieldsState(
    const DateTimeFieldsState& date_time_fields_state) {
  for (const auto& field : fields_)
    field->SetValueAsDateTimeFieldsState(date_time_fields_state);
}

void DateTimeEditElement::SetEmptyValue(
    const LayoutParameters& layout_parameters,
    const DateComponents& date_for_read_only_field) {
  GetLayout(layout_parameters, date_for_read_only_field);
  for (const auto& field : fields_)
    field->SetEmptyValue(DateTimeFieldElement::kDispatchNoEvent);
}

bool DateTimeEditElement::HasField(DateTimeField type) const {
  for (const auto& field : fields_) {
    if (field->Type() == type)
      return true;
  }

  return false;
}

bool DateTimeEditElement::IsFirstFieldAMPM() const {
  const auto* first_field = FieldAt(0);
  return first_field && first_field->Type() == DateTimeField::kAMPM;
}

bool DateTimeEditElement::HasFocusedField() {
  return FocusedFieldIndex() != kInvalidFieldIndex;
}

void DateTimeEditElement::SetOnlyYearMonthDay(const DateComponents& date) {
  DCHECK_EQ(date.GetType(), DateComponents::kDate);

  if (!edit_control_owner_)
    return;

  DateTimeFieldsState date_time_fields_state = ValueAsDateTimeFieldsState();
  date_time_fields_state.SetYear(date.FullYear());
  date_time_fields_state.SetMonth(date.Month() + 1);
  date_time_fields_state.SetDayOfMonth(date.MonthDay());
  SetValueAsDateTimeFieldsState(date_time_fields_state);
  edit_control_owner_->EditControlValueChanged();
}

void DateTimeEditElement::SetOnlyTime(const DateComponents& date) {
  DCHECK_EQ(date.GetType(), DateComponents::kTime);

  if (!edit_control_owner_)
    return;

  DateTimeFieldsState date_time_fields_state = ValueAsDateTimeFieldsState();
  date_time_fields_state.SetHour(date.Hour() % 12 ? date.Hour() % 12 : 12);
  date_time_fields_state.SetMinute(date.Minute());
  date_time_fields_state.SetSecond(date.Second());
  date_time_fields_state.SetMillisecond(date.Millisecond());
  date_time_fields_state.SetAMPM(date.Hour() >= 12
                                     ? DateTimeFieldsState::kAMPMValuePM
                                     : DateTimeFieldsState::kAMPMValueAM);
  SetValueAsDateTimeFieldsState(date_time_fields_state);
  edit_control_owner_->EditControlValueChanged();
}

void DateTimeEditElement::StepDown() {
  if (DateTimeFieldElement* const field = FocusedField())
    field->StepDown();
}

void DateTimeEditElement::StepUp() {
  if (DateTimeFieldElement* const field = FocusedField())
    field->StepUp();
}

void DateTimeEditElement::UpdateUIState() {
  if (IsDisabled()) {
    if (DateTimeFieldElement* field = FocusedField())
      field->blur();
  }
}

String DateTimeEditElement::Value() const {
  if (!edit_control_owner_)
    return g_empty_string;
  return edit_control_owner_->FormatDateTimeFieldsState(
      ValueAsDateTimeFieldsState());
}

DateTimeFieldsState DateTimeEditElement::ValueAsDateTimeFieldsState() const {
  DateTimeFieldsState date_time_fields_state;
  for (const auto& field : fields_)
    field->PopulateDateTimeFieldsState(date_time_fields_state);
  return date_time_fields_state;
}

}  // namespace blink
