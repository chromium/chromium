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

#include "third_party/blink/renderer/core/html/forms/multiple_fields_temporal_input_type_view.h"

#include "third_party/blink/public/mojom/input/focus_type.mojom-blink.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/scoped_event_queue.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/html/forms/base_temporal_input_type.h"
#include "third_party/blink/renderer/core/html/forms/date_time_chooser.h"
#include "third_party/blink/renderer/core/html/forms/date_time_field_element.h"
#include "third_party/blink/renderer/core/html/forms/date_time_fields_state.h"
#include "third_party/blink/renderer/core/html/forms/form_controller.h"
#include "third_party/blink/renderer/core/html/forms/html_data_list_element.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/text/date_components.h"
#include "third_party/blink/renderer/platform/text/date_time_format.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "third_party/blink/renderer/platform/wtf/date_math.h"
#include "ui/base/ui_base_features.h"

namespace blink {

using mojom::blink::FormControlType;

class DateTimeFormatValidator : public DateTimeFormat::TokenHandler {
 public:
  DateTimeFormatValidator()
      : has_year_(false),
        has_month_(false),
        has_week_(false),
        has_day_(false),
        has_ampm_(false),
        has_hour_(false),
        has_minute_(false),
        has_second_(false) {}

  void VisitField(DateTimeFormat::FieldType, int) final;
  void VisitLiteral(const String&) final {}

  bool ValidateFormat(const String& format, const BaseTemporalInputType&);

 private:
  bool has_year_;
  bool has_month_;
  bool has_week_;
  bool has_day_;
  bool has_ampm_;
  bool has_hour_;
  bool has_minute_;
  bool has_second_;
};

void DateTimeFormatValidator::VisitField(DateTimeFormat::FieldType field_type,
                                         int) {
  switch (field_type) {
    case DateTimeFormat::kFieldTypeYear:
      has_year_ = true;
      break;
    case DateTimeFormat::kFieldTypeMonth:  // Fallthrough.
    case DateTimeFormat::kFieldTypeMonthStandAlone:
      has_month_ = true;
      break;
    case DateTimeFormat::kFieldTypeWeekOfYear:
      has_week_ = true;
      break;
    case DateTimeFormat::kFieldTypeDayOfMonth:
      has_day_ = true;
      break;
    case DateTimeFormat::kFieldTypePeriod:
    case DateTimeFormat::kFieldTypePeriodAmPmNoonMidnight:
    case DateTimeFormat::kFieldTypePeriodFlexible:
      has_ampm_ = true;
      break;
    case DateTimeFormat::kFieldTypeHour11:  // Fallthrough.
    case DateTimeFormat::kFieldTypeHour12:
      has_hour_ = true;
      break;
    case DateTimeFormat::kFieldTypeHour23:  // Fallthrough.
    case DateTimeFormat::kFieldTypeHour24:
      has_hour_ = true;
      has_ampm_ = true;
      break;
    case DateTimeFormat::kFieldTypeMinute:
      has_minute_ = true;
      break;
    case DateTimeFormat::kFieldTypeSecond:
      has_second_ = true;
      break;
    default:
      break;
  }
}

bool DateTimeFormatValidator::ValidateFormat(
    const String& format,
    const BaseTemporalInputType& input_type) {
  if (!DateTimeFormat::Parse(format, *this))
    return false;
  return input_type.IsValidFormat(has_year_, has_month_, has_week_, has_day_,
                                  has_ampm_, has_hour_, has_minute_,
                                  has_second_);
}

DateTimeEditElement*
MultipleFieldsTemporalInputTypeView::GetDateTimeEditElement() const {
  auto* element = GetElement().EnsureShadowSubtree()->getElementById(
      shadow_element_names::kIdDateTimeEdit);
  CHECK(!element || IsA<DateTimeEditElement>(element));
  return To<DateTimeEditElement>(element);
}

DateTimeEditElement*
MultipleFieldsTemporalInputTypeView::GetDateTimeEditElementIfCreated() const {
  return HasCreatedShadowSubtree() ? GetDateTimeEditElement() : nullptr;
}

SpinButtonElement* MultipleFieldsTemporalInputTypeView::GetSpinButtonElement()
    const {
  auto* element = GetElement().EnsureShadowSubtree()->getElementById(
      shadow_element_names::kIdSpinButton);
  CHECK(!element || IsA<SpinButtonElement>(element));
  return To<SpinButtonElement>(element);
}

ClearButtonElement* MultipleFieldsTemporalInputTypeView::GetClearButtonElement()
    const {
  auto* element = GetElement().EnsureShadowSubtree()->getElementById(
      shadow_element_names::kIdClearButton);
  CHECK(!element || IsA<ClearButtonElement>(element));
  return To<ClearButtonElement>(element);
}

PickerIndicatorElement*
MultipleFieldsTemporalInputTypeView::GetPickerIndicatorElement() const {
  auto* element = GetElement().EnsureShadowSubtree()->getElementById(
      shadow_element_names::kIdPickerIndicator);
  CHECK(!element || IsA<PickerIndicatorElement>(element));
  return To<PickerIndicatorElement>(element);
}

inline bool MultipleFieldsTemporalInputTypeView::ContainsFocusedShadowElement()
    const {
  return GetElement().EnsureShadowSubtree()->contains(
      GetElement().GetDocument().FocusedElement());
}

void MultipleFieldsTemporalInputTypeView::DidBlurFromControl(
    mojom::blink::FocusType focus_type) {
  // We don't need to call blur(). This function is called when control
  // lost focus.

  if (ContainsFocusedShadowElement())
    return;
  EventQueueScope scope;
  // Remove focus ring by CSS "focus" pseudo class.
  GetElement().SetFocused(false, focus_type);
  if (SpinButtonElement* spin_button = GetSpinButtonElement())
    spin_button->ReleaseCapture();
}

void MultipleFieldsTemporalInputTypeView::DidFocusOnControl(
    mojom::blink::FocusType focus_type) {
  // We don't need to call focus(). This function is called when control
  // got focus.

  if (!ContainsFocusedShadowElement())
    return;
  // Add focus ring by CSS "focus" pseudo class.
  // FIXME: Setting the focus flag to non-focused element is too tricky.
  GetElement().SetFocused(true, focus_type);
}

void MultipleFieldsTemporalInputTypeView::EditControlValueChanged() {
  String old_value = GetElement().Value();
  String new_value =
      input_type_->SanitizeValue(GetDateTimeEditElement()->Value());
  // Even if oldValue is null and newValue is "", we should assume they are
  // same.
  if ((old_value.empty() && new_value.empty()) || old_value == new_value) {
    GetElement().SetNeedsValidityCheck();
  } else {
    GetElement().SetNonAttributeValueByUserEdit(new_value);
    GetElement().SetNeedsStyleRecalc(kSubtreeStyleChange,
                                     StyleChangeReasonForTracing::Create(
                                         style_change_reason::kControlValue));
    GetElement().DispatchInputEvent();
  }
  GetElement().NotifyFormStateChanged();
  GetElement().UpdateClearButtonVisibility();
}

String MultipleFieldsTemporalInputTypeView::FormatDateTimeFieldsState(
    const DateTimeFieldsState& state) const {
  return input_type_->FormatDateTimeFieldsState(state);
}

bool MultipleFieldsTemporalInputTypeView::HasCustomFocusLogic() const {
  return false;
}

bool MultipleFieldsTemporalInputTypeView::IsEditControlOwnerDisabled() const {
  return GetElement().IsDisabledFormControl();
}

bool MultipleFieldsTemporalInputTypeView::IsEditControlOwnerReadOnly() const {
  return GetElement().IsReadOnly();
}

void MultipleFieldsTemporalInputTypeView::FocusAndSelectSpinButtonOwner() {
  if (DateTimeEditElement* edit = GetDateTimeEditElement())
    edit->FocusIfNoFocus();
}

bool MultipleFieldsTemporalInputTypeView::
    ShouldSpinButtonRespondToMouseEvents() {
  return !GetElement().IsDisabledOrReadOnly();
}

bool MultipleFieldsTemporalInputTypeView::
    ShouldSpinButtonRespondToWheelEvents() {
  if (!ShouldSpinButtonRespondToMouseEvents())
    return false;
  if (DateTimeEditElement* edit = GetDateTimeEditElement())
    return edit->HasFocusedField();
  return false;
}

void MultipleFieldsTemporalInputTypeView::SpinButtonStepDown() {
  if (DateTimeEditElement* edit = GetDateTimeEditElement())
    edit->StepDown();
}

void MultipleFieldsTemporalInputTypeView::SpinButtonStepUp() {
  if (DateTimeEditElement* edit = GetDateTimeEditElement())
    edit->StepUp();
}

void MultipleFieldsTemporalInputTypeView::SpinButtonDidReleaseMouseCapture(
    SpinButtonElement::EventDispatch event_dispatch) {
  if (event_dispatch == SpinButtonElement::kEventDispatchAllowed)
    GetElement().DispatchFormControlChangeEvent();
}

bool MultipleFieldsTemporalInputTypeView::
    IsPickerIndicatorOwnerDisabledOrReadOnly() const {
  return GetElement().IsDisabledOrReadOnly();
}

void MultipleFieldsTemporalInputTypeView::PickerIndicatorChooseValue(
    const String& value) {
  if (will_be_destroyed_)
    return;

  // SetUserHasEditedTheFieldAndBlurred is required in order to match
  // :user-valid/:user-invalid
  GetElement().SetUserHasEditedTheFieldAndBlurred();

  if (value.empty() || GetElement().IsValidValue(value)) {
    GetElement().SetValue(value, TextFieldEventBehavior::kDispatchInputEvent);
    return;
  }

  DateTimeEditElement* edit = GetDateTimeEditElement();
  if (!edit)
    return;
  EventQueueScope scope;
  DateComponents date;
  unsigned end;
  if (input_type_->FormControlType() == FormControlType::kInputTime) {
    if (date.ParseTime(value, 0, end) && end == value.length())
      edit->SetOnlyTime(date);
  } else if (input_type_->FormControlType() ==
             FormControlType::kInputDatetimeLocal) {
    if (date.ParseDateTimeLocal(value, 0, end) && end == value.length())
      edit->SetDateTimeLocal(date);
  } else {
    if (date.ParseDate(value, 0, end) && end == value.length())
      edit->SetOnlyYearMonthDay(date);
  }
}

void MultipleFieldsTemporalInputTypeView::PickerIndicatorChooseValue(
    double value) {
  if (will_be_destroyed_)
    return;

  // SetUserHasEditedTheFieldAndBlurred is required in order to match
  // :user-valid/:user-invalid
  GetElement().SetUserHasEditedTheFieldAndBlurred();

  DCHECK(std::isfinite(value) || std::isnan(value));
  if (std::isnan(value)) {
    GetElement().SetValue(g_empty_string,
                          TextFieldEventBehavior::kDispatchInputEvent);
  } else {
    GetElement().setValueAsNumber(value, ASSERT_NO_EXCEPTION,
                                  TextFieldEventBehavior::kDispatchInputEvent);
  }
}

Element& MultipleFieldsTemporalInputTypeView::PickerOwnerElement() const {
  return GetElement();
}

bool MultipleFieldsTemporalInputTypeView::SetupDateTimeChooserParameters(
    DateTimeChooserParameters& parameters) {
  // TODO(iopopesc): Get the field information by parsing the datetime format.
  if (DateTimeEditElement* edit = GetDateTimeEditElement()) {
    parameters.is_ampm_first = edit->IsFirstFieldAMPM();
    parameters.has_ampm = edit->HasField(DateTimeField::kAMPM);
    parameters.has_second = edit->HasField(DateTimeField::kSecond);
    parameters.has_millisecond = edit->HasField(DateTimeField::kMillisecond);
  } else {
    parameters.is_ampm_first = false;
    parameters.has_ampm = false;
    parameters.has_second = false;
    parameters.has_millisecond = false;
  }

  return GetElement().SetupDateTimeChooserParameters(parameters);
}

void MultipleFieldsTemporalInputTypeView::DidEndChooser() {
  GetElement().EnqueueChangeEvent();
}

String MultipleFieldsTemporalInputTypeView::AriaLabelForPickerIndicator()
    const {
  return input_type_->AriaLabelForPickerIndicator();
}

MultipleFieldsTemporalInputTypeView::MultipleFieldsTemporalInputTypeView(
    HTMLInputElement& element,
    BaseTemporalInputType& input_type)
    : InputTypeView(element),
      input_type_(input_type),
      is_destroying_shadow_subtree_(false),
      picker_indicator_is_visible_(false),
      picker_indicator_is_always_visible_(false) {}

MultipleFieldsTemporalInputTypeView::~MultipleFieldsTemporalInputTypeView() =
    default;

void MultipleFieldsTemporalInputTypeView::Trace(Visitor* visitor) const {
  visitor->Trace(input_type_);
  InputTypeView::Trace(visitor);
}

void MultipleFieldsTemporalInputTypeView::Blur() {
  if (DateTimeEditElement* edit = GetDateTimeEditElement())
    edit->BlurByOwner();
  ClosePopupView();
}

void MultipleFieldsTemporalInputTypeView::AdjustStyle(
    ComputedStyleBuilder& builder) {
  builder.SetShouldIgnoreOverflowPropertyForInlineBlockBaseline();
  builder.SetDirection(ComputedTextDirection());
}

void MultipleFieldsTemporalInputTypeView::CreateShadowSubtree() {
  DCHECK(IsShadowHost(GetElement()));

  Document& document = GetElement().GetDocument();
  ContainerNode* container = GetElement().UserAgentShadowRoot();

  auto* container_div = MakeGarbageCollected<HTMLDivElement>(document);
  container_div->SetShadowPseudoId(
      shadow_element_names::kPseudoInternalDatetimeContainer);
  container_div->SetInlineStyleProperty(CSSPropertyID::kUnicodeBidi,
                                        CSSValueID::kNormal);
  GetElement().UserAgentShadowRoot()->AppendChild(container_div);
  container = container_div;

  container->AppendChild(
      MakeGarbageCollected<DateTimeEditElement, Document&,
                           DateTimeEditElement::EditControlOwner&>(document,
                                                                   *this));
  if (LayoutTheme::GetTheme().SupportsCalendarPicker(input_type_->type())) {
    picker_indicator_is_always_visible_ = true;
  }
  container->AppendChild(
      MakeGarbageCollected<PickerIndicatorElement, Document&,
                           PickerIndicatorElement::PickerIndicatorOwner&>(
          document, *this));
  picker_indicator_is_visible_ = true;
  UpdatePickerIndicatorVisibility();
}

void MultipleFieldsTemporalInputTypeView::DestroyShadowSubtree() {
  DCHECK(!is_destroying_shadow_subtree_);
  is_destroying_shadow_subtree_ = true;
  if (SpinButtonElement* element = GetSpinButtonElement())
    element->RemoveSpinButtonOwner();
  if (ClearButtonElement* element = GetClearButtonElement())
    element->RemoveClearButtonOwner();
  if (DateTimeEditElement* element = GetDateTimeEditElement())
    element->RemoveEditControlOwner();
  if (PickerIndicatorElement* element = GetPickerIndicatorElement())
    element->RemovePickerIndicatorOwner();

  // If a field element has focus, set focus back to the <input> itself before
  // deleting the field. This prevents unnecessary focusout/blur events.
  if (ContainsFocusedShadowElement())
    GetElement().Focus(FocusParams(FocusTrigger::kUserGesture));

  InputTypeView::DestroyShadowSubtree();
  is_destroying_shadow_subtree_ = false;
}

void MultipleFieldsTemporalInputTypeView::HandleClickEvent(MouseEvent& event) {
  if (!event.isTrusted()) {
    UseCounter::Count(GetElement().GetDocument(),
                      WebFeature::kTemporalInputTypeIgnoreUntrustedClick);
  }
}

void MultipleFieldsTemporalInputTypeView::HandleFocusInEvent(
    Element* old_focused_element,
    mojom::blink::FocusType type) {
  DateTimeEditElement* edit = GetDateTimeEditElement();
  if (!edit || is_destroying_shadow_subtree_)
    return;
  if (type == mojom::blink::FocusType::kBackward) {
    if (GetElement().GetDocument().GetPage())
      GetElement().GetDocument().GetPage()->GetFocusController().AdvanceFocus(
          type);
  } else if (type == mojom::blink::FocusType::kForward) {
    edit->FocusByOwner();
  } else {
    edit->FocusByOwner(old_focused_element);
  }
}

void MultipleFieldsTemporalInputTypeView::ForwardEvent(Event& event) {
  if (SpinButtonElement* element = GetSpinButtonElement()) {
    element->ForwardEvent(event);
    if (event.DefaultHandled())
      return;
  }

  if (DateTimeEditElement* edit = GetDateTimeEditElement())
    edit->DefaultEventHandler(event);
}

void MultipleFieldsTemporalInputTypeView::DisabledAttributeChanged() {
  EventQueueScope scope;
  if (SpinButtonElement* spin_button = GetSpinButtonElement())
    spin_button->ReleaseCapture();
  if (DateTimeEditElement* edit = GetDateTimeEditElement())
    edit->DisabledStateChanged();
}

void MultipleFieldsTemporalInputTypeView::RequiredAttributeChanged() {
  UpdateClearButtonVisibility();
}

void MultipleFieldsTemporalInputTypeView::HandleKeydownEvent(
    KeyboardEvent& event) {
  if (!GetElement().IsFocused())
    return;
  if (picker_indicator_is_visible_ &&
      ((event.key() == keywords::kArrowDown && event.getModifierState("Alt")) ||
       event.key() == "F4" || event.key() == " ")) {
    OpenPopupView();
    event.SetDefaultHandled();
  } else {
    ForwardEvent(event);
  }
}

bool MultipleFieldsTemporalInputTypeView::HasBadInput() const {
  DateTimeEditElement* edit = GetDateTimeEditElementIfCreated();
  return edit && GetElement().Value().empty() &&
         edit->AnyEditableFieldsHaveValues();
}

AtomicString MultipleFieldsTemporalInputTypeView::LocaleIdentifier() const {
  return GetElement().ComputeInheritedLanguage();
}

void MultipleFieldsTemporalInputTypeView::
    EditControlDidChangeValueByKeyboard() {
  GetElement().SetUserHasEditedTheField();
  GetElement().DispatchFormControlChangeEvent();
}

void MultipleFieldsTemporalInputTypeView::MinOrMaxAttributeChanged() {
  UpdateView();
}

void MultipleFieldsTemporalInputTypeView::ReadonlyAttributeChanged() {
  EventQueueScope scope;
  if (SpinButtonElement* spin_button = GetSpinButtonElement())
    spin_button->ReleaseCapture();
  if (DateTimeEditElement* edit = GetDateTimeEditElement())
    edit->ReadOnlyStateChanged();
}

void MultipleFieldsTemporalInputTypeView::RestoreFormControlState(
    const FormControlState& state) {
  DateTimeEditElement* edit = GetDateTimeEditElement();
  if (!edit)
    return;
  DateTimeFieldsState date_time_fields_state =
      DateTimeFieldsState::RestoreFormControlState(state);
  edit->SetValueAsDateTimeFieldsState(date_time_fields_state);
  GetElement().SetNonAttributeValue(input_type_->SanitizeValue(edit->Value()));
  UpdateClearButtonVisibility();
}

FormControlState MultipleFieldsTemporalInputTypeView::SaveFormControlState()
    const {
  if (DateTimeEditElement* edit = GetDateTimeEditElement())
    return edit->ValueAsDateTimeFieldsState().SaveFormControlState();
  return FormControlState();
}

void MultipleFieldsTemporalInputTypeView::DidSetValue(
    const String& sanitized_value,
    bool value_changed) {
  DateTimeEditElement* edit = GetDateTimeEditElement();
  if (value_changed || (sanitized_value.empty() && edit &&
                        edit->AnyEditableFieldsHaveValues())) {
    GetElement().UpdateView();
    GetElement().SetNeedsValidityCheck();
  }
}

void MultipleFieldsTemporalInputTypeView::StepAttributeChanged() {
  UpdateView();
}

void MultipleFieldsTemporalInputTypeView::UpdateView() {
  DateTimeEditElement* edit = GetDateTimeEditElement();
  if (!edit)
    return;

  DateTimeEditElement::LayoutParameters layout_parameters(
      GetElement().GetLocale(),
      input_type_->CreateStepRange(kAnyIsDefaultStep));

  DateComponents date;
  bool has_value = false;
  if (!GetElement().SuggestedValue().IsNull())
    has_value = input_type_->ParseToDateComponents(
        GetElement().SuggestedValue(), &date);
  else
    has_value = input_type_->ParseToDateComponents(GetElement().Value(), &date);
  if (!has_value)
    input_type_->SetMillisecondToDateComponents(
        layout_parameters.step_range.Minimum().ToDouble(), &date);

  input_type_->SetupLayoutParameters(layout_parameters, date);

  DEFINE_STATIC_LOCAL(AtomicString, datetimeformat_attr, ("datetimeformat"));
  edit->setAttribute(datetimeformat_attr,
                     AtomicString(layout_parameters.date_time_format),
                     ASSERT_NO_EXCEPTION);
  const AtomicString pattern = edit->FastGetAttribute(html_names::kPatternAttr);
  if (!pattern.empty())
    layout_parameters.date_time_format = pattern;

  if (!DateTimeFormatValidator().ValidateFormat(
          layout_parameters.date_time_format, *input_type_))
    layout_parameters.date_time_format =
        layout_parameters.fallback_date_time_format;

  if (has_value)
    edit->SetValueAsDate(layout_parameters, date);
  else
    edit->SetEmptyValue(layout_parameters, date);
  UpdateClearButtonVisibility();
}

ControlPart MultipleFieldsTemporalInputTypeView::AutoAppearance() const {
  return kTextFieldPart;
}

void MultipleFieldsTemporalInputTypeView::OpenPopupView() {
  if (PickerIndicatorElement* picker = GetPickerIndicatorElement())
    picker->OpenPopup();
}

void MultipleFieldsTemporalInputTypeView::ClosePopupView() {
  if (!HasCreatedShadowSubtree()) {
    return;
  }
  if (PickerIndicatorElement* picker = GetPickerIndicatorElement())
    picker->ClosePopup();
}

bool MultipleFieldsTemporalInputTypeView::HasOpenedPopup() const {
  if (PickerIndicatorElement* picker = GetPickerIndicatorElement())
    return picker->HasOpenedPopup();

  return false;
}

void MultipleFieldsTemporalInputTypeView::ValueAttributeChanged() {
  if (!GetElement().HasDirtyValue())
    UpdateView();
}

void MultipleFieldsTemporalInputTypeView::ListAttributeTargetChanged() {
  UpdatePickerIndicatorVisibility();
}

void MultipleFieldsTemporalInputTypeView::UpdatePickerIndicatorVisibility() {
  if (picker_indicator_is_always_visible_) {
    ShowPickerIndicator();
    return;
  }
  if (GetElement().HasValidDataListOptions())
    ShowPickerIndicator();
  else
    HidePickerIndicator();
}

void MultipleFieldsTemporalInputTypeView::HidePickerIndicator() {
  if (!picker_indicator_is_visible_)
    return;
  picker_indicator_is_visible_ = false;
  DCHECK(GetPickerIndicatorElement());
  GetPickerIndicatorElement()->SetInlineStyleProperty(CSSPropertyID::kDisplay,
                                                      CSSValueID::kNone);
}

void MultipleFieldsTemporalInputTypeView::ShowPickerIndicator() {
  if (picker_indicator_is_visible_)
    return;
  picker_indicator_is_visible_ = true;
  DCHECK(GetPickerIndicatorElement());
  GetPickerIndicatorElement()->RemoveInlineStyleProperty(
      CSSPropertyID::kDisplay);
}

void MultipleFieldsTemporalInputTypeView::FocusAndSelectClearButtonOwner() {
  GetElement().Focus(FocusParams(FocusTrigger::kUserGesture));
}

bool MultipleFieldsTemporalInputTypeView::
    ShouldClearButtonRespondToMouseEvents() {
  return !GetElement().IsDisabledOrReadOnly() && !GetElement().IsRequired();
}

void MultipleFieldsTemporalInputTypeView::ClearValue() {
  GetElement().SetValue("",
                        TextFieldEventBehavior::kDispatchInputAndChangeEvent);
  GetElement().UpdateClearButtonVisibility();
}

void MultipleFieldsTemporalInputTypeView::UpdateClearButtonVisibility() {
  ClearButtonElement* clear_button = GetClearButtonElement();
  if (!clear_button)
    return;

  if (GetElement().IsRequired() ||
      !GetDateTimeEditElement()->AnyEditableFieldsHaveValues()) {
    clear_button->SetInlineStyleProperty(CSSPropertyID::kOpacity, 0.0,
                                         CSSPrimitiveValue::UnitType::kNumber);
    clear_button->SetInlineStyleProperty(CSSPropertyID::kPointerEvents,
                                         CSSValueID::kNone);
  } else {
    clear_button->RemoveInlineStyleProperty(CSSPropertyID::kOpacity);
    clear_button->RemoveInlineStyleProperty(CSSPropertyID::kPointerEvents);
  }
}

TextDirection MultipleFieldsTemporalInputTypeView::ComputedTextDirection() {
  return GetElement().GetLocale().IsRTL() ? TextDirection::kRtl
                                          : TextDirection::kLtr;
}

AXObject* MultipleFieldsTemporalInputTypeView::PopupRootAXObject() {
  if (PickerIndicatorElement* picker = GetPickerIndicatorElement())
    return picker->PopupRootAXObject();
  return nullptr;
}

wtf_size_t MultipleFieldsTemporalInputTypeView::FocusedFieldIndex() const {
  return GetDateTimeEditElement()->FocusedFieldIndex();
}

}  // namespace blink
