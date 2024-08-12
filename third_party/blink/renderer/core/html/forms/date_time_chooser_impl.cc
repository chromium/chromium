/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/html/forms/date_time_chooser_impl.h"

#include "build/build_config.h"
#include "third_party/blink/public/mojom/choosers/date_time_chooser.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/forms/chooser_resource_loader.h"
#include "third_party/blink/renderer/core/html/forms/date_time_chooser_client.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page_popup.h"
#include "third_party/blink/renderer/platform/language.h"
#include "third_party/blink/renderer/platform/text/date_components.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "ui/base/ui_base_features.h"
#include "ui/strings/grit/ax_strings.h"

namespace blink {

DateTimeChooserImpl::DateTimeChooserImpl(
    LocalFrame* frame,
    DateTimeChooserClient* client,
    const DateTimeChooserParameters& parameters)
    : frame_(frame),
      client_(client),
      popup_(nullptr),
      parameters_(&parameters),
      locale_(Locale::Create(parameters.locale)) {
  DCHECK(RuntimeEnabledFeatures::InputMultipleFieldsUIEnabled());
  DCHECK(frame_);
  DCHECK(client_);
  popup_ = frame_->View()->GetChromeClient()->OpenPagePopup(this);
  parameters_ = nullptr;
}

DateTimeChooserImpl::~DateTimeChooserImpl() = default;

void DateTimeChooserImpl::Trace(Visitor* visitor) const {
  visitor->Trace(frame_);
  visitor->Trace(client_);
  DateTimeChooser::Trace(visitor);
}

void DateTimeChooserImpl::EndChooser() {
  if (!popup_)
    return;
  frame_->View()->GetChromeClient()->ClosePagePopup(popup_);
}

AXObject* DateTimeChooserImpl::RootAXObject(Element* popup_owner) {
  return popup_ ? popup_->RootAXObject(popup_owner) : nullptr;
}

static String ValueToDateTimeString(double value, InputType::Type type) {
  DateComponents components;
  switch (type) {
    case InputType::Type::kDate:
      components.SetMillisecondsSinceEpochForDate(value);
      break;
    case InputType::Type::kDateTimeLocal:
      components.SetMillisecondsSinceEpochForDateTimeLocal(value);
      break;
    case InputType::Type::kMonth:
      components.SetMonthsSinceEpoch(value);
      break;
    case InputType::Type::kTime:
      components.SetMillisecondsSinceMidnight(value);
      break;
    case InputType::Type::kWeek:
      components.SetMillisecondsSinceEpochForWeek(value);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return components.GetType() == DateComponents::kInvalid
             ? String()
             : components.ToString();
}

void DateTimeChooserImpl::WriteDocument(SegmentedBuffer& data) {
  String step_string = String::Number(parameters_->step);
  String step_base_string = String::Number(parameters_->step_base, 11);
  String today_label_string;
  String other_date_label_string;
  switch (parameters_->type) {
    case InputType::Type::kMonth:
      today_label_string = GetLocale().QueryString(IDS_FORM_THIS_MONTH_LABEL);
      other_date_label_string =
          GetLocale().QueryString(IDS_FORM_OTHER_MONTH_LABEL);
      break;
    case InputType::Type::kWeek:
      today_label_string = GetLocale().QueryString(IDS_FORM_THIS_WEEK_LABEL);
      other_date_label_string =
          GetLocale().QueryString(IDS_FORM_OTHER_WEEK_LABEL);
      break;
    default:
      today_label_string = GetLocale().QueryString(IDS_FORM_CALENDAR_TODAY);
      other_date_label_string =
          GetLocale().QueryString(IDS_FORM_OTHER_DATE_LABEL);
  }

  AddString(
      "<!DOCTYPE html><head><meta charset='UTF-8'><meta name='color-scheme' "
      "content='light dark'><style>\n",
      data);

  data.Append(ChooserResourceLoader::GetPickerCommonStyleSheet());
  data.Append(ChooserResourceLoader::GetSuggestionPickerStyleSheet());
  data.Append(ChooserResourceLoader::GetCalendarPickerStyleSheet());
  if (parameters_->type == InputType::Type::kTime ||
      parameters_->type == InputType::Type::kDateTimeLocal) {
    data.Append(ChooserResourceLoader::GetTimePickerStyleSheet());
  }
  AddString(
      "</style></head><body><div id=main>Loading...</div><script>\n"
      "window.dialogArguments = {\n",
      data);
  AddProperty("anchorRectInScreen", parameters_->anchor_rect_in_screen, data);
  AddProperty("zoomFactor", ScaledZoomFactor(), data);
  AddProperty("min",
              ValueToDateTimeString(parameters_->minimum, parameters_->type),
              data);
  AddProperty("max",
              ValueToDateTimeString(parameters_->maximum, parameters_->type),
              data);
  AddProperty("step", step_string, data);
  AddProperty("stepBase", step_base_string, data);
  AddProperty("required", parameters_->required, data);
  AddProperty(
      "currentValue",
      ValueToDateTimeString(parameters_->double_value, parameters_->type),
      data);
  AddProperty("focusedFieldIndex", parameters_->focused_field_index, data);
  AddProperty("locale", parameters_->locale.GetString(), data);
  AddProperty("todayLabel", today_label_string, data);
  AddLocalizedProperty("clearLabel", IDS_FORM_CALENDAR_CLEAR, data);
  AddLocalizedProperty("weekLabel", IDS_FORM_WEEK_NUMBER_LABEL, data);
  AddLocalizedProperty("axShowMonthSelector",
                       IDS_AX_CALENDAR_SHOW_MONTH_SELECTOR, data);
  AddLocalizedProperty("axShowNextMonth", IDS_AX_CALENDAR_SHOW_NEXT_MONTH,
                       data);
  AddLocalizedProperty("axShowPreviousMonth",
                       IDS_AX_CALENDAR_SHOW_PREVIOUS_MONTH, data);
  AddLocalizedProperty("axHourLabel", IDS_AX_HOUR_FIELD_TEXT, data);
  AddLocalizedProperty("axMinuteLabel", IDS_AX_MINUTE_FIELD_TEXT, data);
  AddLocalizedProperty("axSecondLabel", IDS_AX_SECOND_FIELD_TEXT, data);
  AddLocalizedProperty("axMillisecondLabel", IDS_AX_MILLISECOND_FIELD_TEXT,
                       data);
  AddLocalizedProperty("axAmPmLabel", IDS_AX_AM_PM_FIELD_TEXT, data);
  AddProperty("weekStartDay", locale_->FirstDayOfWeek(), data);
  AddProperty("shortMonthLabels", locale_->ShortMonthLabels(), data);
  AddProperty("dayLabels", locale_->WeekDayShortLabels(), data);
  AddProperty("ampmLabels", locale_->TimeAMPMLabels(), data);
  AddProperty("isLocaleRTL", locale_->IsRTL(), data);
  AddProperty("isRTL", parameters_->is_anchor_element_rtl, data);
#if BUILDFLAG(IS_MAC)
  AddProperty("isBorderTransparent", true, data);
#endif
  AddProperty("mode", InputType::TypeToString(parameters_->type).GetString(),
              data);
  AddProperty("isAMPMFirst", parameters_->is_ampm_first, data);
  AddProperty("hasAMPM", parameters_->has_ampm, data);
  AddProperty("hasSecond", parameters_->has_second, data);
  AddProperty("hasMillisecond", parameters_->has_millisecond, data);
  if (parameters_->suggestions.size()) {
    Vector<String> suggestion_values;
    Vector<String> localized_suggestion_values;
    Vector<String> suggestion_labels;
    for (unsigned i = 0; i < parameters_->suggestions.size(); i++) {
      suggestion_values.push_back(ValueToDateTimeString(
          parameters_->suggestions[i]->value, parameters_->type));
      localized_suggestion_values.push_back(
          parameters_->suggestions[i]->localized_value);
      suggestion_labels.push_back(parameters_->suggestions[i]->label);
    }
    AddProperty("suggestionValues", suggestion_values, data);
    AddProperty("localizedSuggestionValues", localized_suggestion_values, data);
    AddProperty("suggestionLabels", suggestion_labels, data);
    AddProperty(
        "inputWidth",
        static_cast<unsigned>(parameters_->anchor_rect_in_screen.width()),
        data);
    AddProperty(
        "showOtherDateEntry",
        LayoutTheme::GetTheme().SupportsCalendarPicker(parameters_->type),
        data);
    AddProperty("otherDateLabel", other_date_label_string, data);

    const ComputedStyle* style = OwnerElement().GetComputedStyle();
    mojom::blink::ColorScheme color_scheme =
        style ? style->UsedColorScheme() : mojom::blink::ColorScheme::kLight;

    AddProperty("suggestionHighlightColor",
                LayoutTheme::GetTheme()
                    .ActiveListBoxSelectionBackgroundColor(color_scheme)
                    .SerializeAsCSSColor(),
                data);
    AddProperty("suggestionHighlightTextColor",
                LayoutTheme::GetTheme()
                    .ActiveListBoxSelectionForegroundColor(color_scheme)
                    .SerializeAsCSSColor(),
                data);
  }
  AddString("}\n", data);

  data.Append(ChooserResourceLoader::GetPickerCommonJS());
  data.Append(ChooserResourceLoader::GetSuggestionPickerJS());
  data.Append(ChooserResourceLoader::GetMonthPickerJS());
  if (parameters_->type == InputType::Type::kTime) {
    data.Append(ChooserResourceLoader::GetTimePickerJS());
  } else if (parameters_->type == InputType::Type::kDateTimeLocal) {
    data.Append(ChooserResourceLoader::GetTimePickerJS());
    data.Append(ChooserResourceLoader::GetDateTimeLocalPickerJS());
  }
  data.Append(ChooserResourceLoader::GetCalendarPickerJS());
  AddString("</script></body>\n", data);
}

Element& DateTimeChooserImpl::OwnerElement() {
  return client_->OwnerElement();
}

ChromeClient& DateTimeChooserImpl::GetChromeClient() {
  return *frame_->View()->GetChromeClient();
}

Locale& DateTimeChooserImpl::GetLocale() {
  return *locale_;
}

void DateTimeChooserImpl::SetValueAndClosePopup(int num_value,
                                                const String& string_value) {
  if (num_value >= 0)
    SetValue(string_value);
  EndChooser();
}

void DateTimeChooserImpl::SetValue(const String& value) {
  client_->DidChooseValue(value);
}

void DateTimeChooserImpl::CancelPopup() {
  EndChooser();
}

void DateTimeChooserImpl::DidClosePopup() {
  DCHECK(client_);
  popup_ = nullptr;
  client_->DidEndChooser();
}

void DateTimeChooserImpl::AdjustSettings(Settings& popup_settings) {
  AdjustSettingsFromOwnerColorScheme(popup_settings);
}

}  // namespace blink
