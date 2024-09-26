/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All
 * rights reserved.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 * Copyright (C) 2007 Samuel Weinig (sam@webkit.org)
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (C) 2012 Samsung Electronics. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "third_party/blink/renderer/core/html/forms/html_input_element.h"

#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/choosers/date_time_chooser.mojom-blink.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom-blink.h"
#include "third_party/blink/public/mojom/scroll/scroll_into_view_params.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/bindings/core/v8/js_event_handler_for_content_attribute.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_focus_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_selection_mode.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatch_forbidden_scope.h"
#include "third_party/blink/renderer/core/dom/events/scoped_event_queue.h"
#include "third_party/blink/renderer/core/dom/events/simulated_click_options.h"
#include "third_party/blink/renderer/core/dom/id_target_observer.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/spellcheck/spell_checker.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/events/before_text_inserted_event.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/fileapi/file_list.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/forms/color_chooser.h"
#include "third_party/blink/renderer/core/html/forms/date_time_chooser.h"
#include "third_party/blink/renderer/core/html/forms/email_input_type.h"
#include "third_party/blink/renderer/core/html/forms/file_input_type.h"
#include "third_party/blink/renderer/core/html/forms/form_controller.h"
#include "third_party/blink/renderer/core/html/forms/html_data_list_element.h"
#include "third_party/blink/renderer/core/html/forms/html_data_list_options_collection.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/input_type.h"
#include "third_party/blink/renderer/core/html/forms/radio_button_group_scope.h"
#include "third_party/blink/renderer/core/html/forms/search_input_type.h"
#include "third_party/blink/renderer/core/html/forms/text_input_type.h"
#include "third_party/blink/renderer/core/html/html_collection.h"
#include "third_party/blink/renderer/core/html/html_image_loader.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/keywords.h"
#include "third_party/blink/renderer/core/layout/adjust_for_absolute_zoom.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_theme_font_provider.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/scroll/scroll_into_view_util.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/language.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "third_party/blink/renderer/platform/text/text_break_iterator.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "ui/base/ui_base_features.h"

namespace blink {

using mojom::blink::FormControlType;

namespace {

const unsigned kMaxEmailFieldLength = 254;

static bool is_default_font_prewarmed_ = false;

}  // namespace

using ValueMode = InputType::ValueMode;

class ListAttributeTargetObserver : public IdTargetObserver {
 public:
  ListAttributeTargetObserver(const AtomicString& id, HTMLInputElement*);

  void Trace(Visitor*) const override;
  void IdTargetChanged() override;

 private:
  Member<HTMLInputElement> element_;
};

const int kDefaultSize = 20;

HTMLInputElement::HTMLInputElement(Document& document,
                                   const CreateElementFlags flags)
    : TextControlElement(html_names::kInputTag, document),
      LazyActiveScriptWrappable<HTMLInputElement>({}),
      size_(kDefaultSize),
      has_dirty_value_(false),
      is_checked_(false),
      dirty_checkedness_(false),
      is_indeterminate_(false),
      is_activated_submit_(false),
      autocomplete_(kUninitialized),
      has_non_empty_list_(false),
      state_restored_(false),
      parsing_in_progress_(flags.IsCreatedByParser()),
      can_receive_dropped_files_(false),
      should_reveal_password_(false),
      needs_to_update_view_value_(true),
      is_placeholder_visible_(false),
      has_been_password_field_(false),
      scheduled_create_shadow_tree_(false),
      // |input_type_| is lazily created when constructed by the parser to avoid
      // constructing unnecessarily a text InputType and its shadow subtree,
      // just to destroy them when the |type| attribute gets set by the parser
      // to something else than 'text'.
      input_type_(flags.IsCreatedByParser()
                      ? nullptr
                      : MakeGarbageCollected<TextInputType>(*this)),
      input_type_view_(input_type_ ? input_type_->CreateView() : nullptr) {
  SetHasCustomStyleCallbacks();
}

void HTMLInputElement::Trace(Visitor* visitor) const {
  visitor->Trace(input_type_);
  visitor->Trace(input_type_view_);
  visitor->Trace(list_attribute_target_observer_);
  visitor->Trace(image_loader_);
  TextControlElement::Trace(visitor);
}

bool HTMLInputElement::HasPendingActivity() const {
  return ImageLoader() && ImageLoader()->HasPendingActivity();
}

HTMLImageLoader& HTMLInputElement::EnsureImageLoader() {
  if (!image_loader_) {
    image_loader_ = MakeGarbageCollected<HTMLImageLoader>(this);
    RegisterActiveScriptWrappable(GetExecutionContext()->GetIsolate());
  }
  return *image_loader_;
}

HTMLInputElement::~HTMLInputElement() = default;

const AtomicString& HTMLInputElement::GetName() const {
  return name_.IsNull() ? g_empty_atom : name_;
}

Vector<String> HTMLInputElement::FilesFromFileInputFormControlState(
    const FormControlState& state) {
  return FileInputType::FilesFromFormControlState(state);
}

bool HTMLInputElement::ShouldAutocomplete() const {
  if (autocomplete_ != kUninitialized)
    return autocomplete_ == kOn;
  return TextControlElement::ShouldAutocomplete();
}

bool HTMLInputElement::IsValidValue(const String& value) const {
  if (!input_type_->IsValidValue(value)) {
    return false;
  }
  return !TooLong(value, kIgnoreDirtyFlag) &&
         !TooShort(value, kIgnoreDirtyFlag);
}

bool HTMLInputElement::TooLong() const {
  return TooLong(Value(), kCheckDirtyFlag);
}

bool HTMLInputElement::TooShort() const {
  return TooShort(Value(), kCheckDirtyFlag);
}

bool HTMLInputElement::TypeMismatch() const {
  return input_type_->TypeMismatch();
}

bool HTMLInputElement::ValueMissing() const {
  return input_type_->ValueMissing(Value());
}

bool HTMLInputElement::HasBadInput() const {
  return input_type_view_->HasBadInput();
}

bool HTMLInputElement::PatternMismatch() const {
  return input_type_->PatternMismatch(Value());
}

bool HTMLInputElement::TooLong(const String& value,
                               NeedsToCheckDirtyFlag check) const {
  return input_type_->TooLong(value, check);
}

bool HTMLInputElement::TooShort(const String& value,
                                NeedsToCheckDirtyFlag check) const {
  return input_type_->TooShort(value, check);
}

bool HTMLInputElement::RangeUnderflow() const {
  return input_type_->RangeUnderflow(Value());
}

bool HTMLInputElement::RangeOverflow() const {
  return input_type_->RangeOverflow(Value());
}

String HTMLInputElement::validationMessage() const {
  if (!willValidate())
    return String();
  if (CustomError())
    return CustomValidationMessage();

  return input_type_->ValidationMessage(*input_type_view_).first;
}

String HTMLInputElement::ValidationSubMessage() const {
  if (CustomError())
    return String();
  return input_type_->ValidationMessage(*input_type_view_).second;
}

double HTMLInputElement::Minimum() const {
  return input_type_->Minimum();
}

double HTMLInputElement::Maximum() const {
  return input_type_->Maximum();
}

bool HTMLInputElement::StepMismatch() const {
  return input_type_->StepMismatch(Value());
}

bool HTMLInputElement::GetAllowedValueStep(Decimal* step) const {
  return input_type_->GetAllowedValueStep(step);
}

StepRange HTMLInputElement::CreateStepRange(
    AnyStepHandling any_step_handling) const {
  return input_type_->CreateStepRange(any_step_handling);
}

Decimal HTMLInputElement::FindClosestTickMarkValue(const Decimal& value) {
  return input_type_->FindClosestTickMarkValue(value);
}

void HTMLInputElement::stepUp(int n, ExceptionState& exception_state) {
  input_type_->StepUp(n, exception_state);
}

void HTMLInputElement::stepDown(int n, ExceptionState& exception_state) {
  input_type_->StepUp(-1.0 * n, exception_state);
}

void HTMLInputElement::blur() {
  input_type_view_->Blur();
}

void HTMLInputElement::DefaultBlur() {
  TextControlElement::blur();
}

bool HTMLInputElement::HasCustomFocusLogic() const {
  return input_type_view_->HasCustomFocusLogic();
}

bool HTMLInputElement::IsKeyboardFocusable(
    UpdateBehavior update_behavior) const {
  return input_type_->IsKeyboardFocusable(update_behavior);
}

bool HTMLInputElement::MayTriggerVirtualKeyboard() const {
  return input_type_->MayTriggerVirtualKeyboard();
}

bool HTMLInputElement::ShouldHaveFocusAppearance() const {
  // Don't draw focus ring for an input that has its popup open.
  if (input_type_view_->HasOpenedPopup())
    return false;

  return TextControlElement::ShouldHaveFocusAppearance();
}

void HTMLInputElement::UpdateSelectionOnFocus(
    SelectionBehaviorOnFocus selection_behavior,
    const FocusOptions* options) {
  if (IsTextField()) {
    switch (selection_behavior) {
      case SelectionBehaviorOnFocus::kReset:
        select();
        break;
      case SelectionBehaviorOnFocus::kRestore:
        RestoreCachedSelection();
        break;
      case SelectionBehaviorOnFocus::kNone:
        return;
    }
    // TODO(tkent): scrollRectToVisible is a workaround of a bug of
    // FrameSelection::revealSelection().  It doesn't scroll correctly in a
    // case of RangeSelection. crbug.com/443061.
    if (!options->preventScroll()) {
      if (GetLayoutObject()) {
        scroll_into_view_util::ScrollRectToVisible(
            *GetLayoutObject(), BoundingBoxForScrollIntoView(),
            scroll_into_view_util::CreateScrollIntoViewParams());
      }
      if (GetDocument().GetFrame())
        GetDocument().GetFrame()->Selection().RevealSelection();
    }
  } else {
    TextControlElement::UpdateSelectionOnFocus(selection_behavior, options);
  }
}

void HTMLInputElement::EndEditing() {
  DCHECK(GetDocument().IsActive());
  if (!GetDocument().IsActive())
    return;

  if (!IsTextField())
    return;

  LocalFrame* frame = GetDocument().GetFrame();
  frame->GetSpellChecker().DidEndEditingOnTextField(this);
  frame->GetPage()->GetChromeClient().DidEndEditingOnTextField(*this);

  MaybeReportPiiMetrics();
}

void HTMLInputElement::DispatchFocusInEvent(
    const AtomicString& event_type,
    Element* old_focused_element,
    mojom::blink::FocusType type,
    InputDeviceCapabilities* source_capabilities) {
  if (event_type == event_type_names::kDOMFocusIn)
    input_type_view_->HandleFocusInEvent(old_focused_element, type);
  HTMLFormControlElementWithState::DispatchFocusInEvent(
      event_type, old_focused_element, type, source_capabilities);
}

void HTMLInputElement::HandleBlurEvent() {
  input_type_view_->HandleBlurEvent();
}

void HTMLInputElement::setType(const AtomicString& type) {
  if (!RuntimeEnabledFeatures::CreateInputShadowTreeDuringLayoutEnabled()) {
    EnsureShadowSubtree();
  }
  setAttribute(html_names::kTypeAttr, type);
}

void HTMLInputElement::InitializeTypeInParsing() {
  DCHECK(parsing_in_progress_);
  DCHECK(!input_type_);
  DCHECK(!input_type_view_);

  const AtomicString& new_type_name =
      InputType::NormalizeTypeName(FastGetAttribute(html_names::kTypeAttr));
  input_type_ = InputType::Create(*this, new_type_name);
  input_type_view_ = input_type_->CreateView();
  String default_value = FastGetAttribute(html_names::kValueAttr);
  if (input_type_->GetValueMode() == ValueMode::kValue)
    non_attribute_value_ = SanitizeValue(default_value);
  has_been_password_field_ |= new_type_name == input_type_names::kPassword;

  UpdateWillValidateCache();

  if (!default_value.IsNull())
    input_type_->WarnIfValueIsInvalid(default_value);

  if (!RuntimeEnabledFeatures::CreateInputShadowTreeDuringLayoutEnabled() ||
      input_type_view_->HasCreatedShadowSubtree()) {
    input_type_view_->UpdateView();
  } else {
    input_type_view_->set_needs_update_view_in_create_shadow_subtree(true);
    UpdatePlaceholderVisibility();
  }

  // Prewarm the default font family. Do this while parsing because the style
  // recalc calls |TextControlInnerEditorElement::CreateInnerEditorStyle| which
  // needs the primary font.
  if (!is_default_font_prewarmed_ && new_type_name == input_type_names::kText) {
    FontCache::PrewarmFamily(LayoutThemeFontProvider::SystemFontFamily(
        CSSValueID::kWebkitSmallControl));
    is_default_font_prewarmed_ = true;
  }
}

void HTMLInputElement::UpdateType(const AtomicString& type_attribute_value) {
  DCHECK(input_type_);
  DCHECK(input_type_view_);

  const AtomicString& new_type_name =
      InputType::NormalizeTypeName(type_attribute_value);
  if (input_type_->FormControlTypeAsString() == new_type_name) {
    return;
  }

  InputType* new_type = InputType::Create(*this, new_type_name);
  RemoveFromRadioButtonGroup();

  ValueMode old_value_mode = input_type_->GetValueMode();
  bool did_respect_height_and_width =
      input_type_->ShouldRespectHeightAndWidthAttributes();
  bool could_be_successful_submit_button = CanBeSuccessfulSubmitButton();

  input_type_view_->ClosePopupView();
  input_type_view_->DestroyShadowSubtree();
  DropInnerEditorElement();
  SetForceReattachLayoutTree();

  if (input_type_->SupportsRequired() != new_type->SupportsRequired() &&
      IsRequired()) {
    PseudoStateChanged(CSSSelector::kPseudoRequired);
    PseudoStateChanged(CSSSelector::kPseudoOptional);
  }
  if (input_type_->SupportsReadOnly() != new_type->SupportsReadOnly()) {
    PseudoStateChanged(CSSSelector::kPseudoReadOnly);
    PseudoStateChanged(CSSSelector::kPseudoReadWrite);
  }
  if (input_type_->IsCheckable() != new_type->IsCheckable()) {
    PseudoStateChanged(CSSSelector::kPseudoChecked);
  }
  PseudoStateChanged(CSSSelector::kPseudoIndeterminate);
  if (input_type_->IsSteppable() || new_type->IsSteppable()) {
    PseudoStateChanged(CSSSelector::kPseudoInRange);
    PseudoStateChanged(CSSSelector::kPseudoOutOfRange);
  }
  if (input_type_->ShouldRespectListAttribute() !=
      new_type->ShouldRespectListAttribute())
    PseudoStateChanged(CSSSelector::kPseudoHasDatalist);

  bool placeholder_changed =
      input_type_->SupportsPlaceholder() != new_type->SupportsPlaceholder();

  has_been_password_field_ |= new_type_name == input_type_names::kPassword;

  // 7. Let previouslySelectable be true if setRangeText() previously applied
  // to the element, and false otherwise.
  const bool previously_selectable = input_type_->SupportsSelectionAPI();

  input_type_view_->WillBeDestroyed();
  InputType* old_type = input_type_;
  input_type_ = new_type;
  input_type_view_ = input_type_->CreateView();

  const AtomicString& dir = FastGetAttribute(html_names::kDirAttr);
  if ((!dir && (old_type->IsTelephoneInputType() || IsTelephone())) ||
      (EqualIgnoringASCIICase(dir, "auto") &&
       (old_type->IsAutoDirectionalityFormAssociated() ||
        IsAutoDirectionalityFormAssociated()))) {
    const AtomicString& value_dir = AtomicString(DirectionForFormData());
    UpdateDirectionalityAfterInputTypeChange(dir, value_dir);
  }

  // No need for CreateShadowSubtreeIfNeeded() to call UpdateView() as we'll
  // do that later on in this function (and calling UpdateView() here is
  // problematic as state hasn't fully been updated).
  if (RuntimeEnabledFeatures::CreateInputShadowTreeDuringLayoutEnabled()) {
    input_type_view_->set_needs_update_view_in_create_shadow_subtree(false);
  }
  input_type_view_->CreateShadowSubtreeIfNeeded(true);

  UpdateWillValidateCache();

  if (placeholder_changed) {
    // We need to update the UA shadow and then the placeholder visibility flag
    // here. Otherwise it would happen as part of attaching the layout tree
    // which would be too late in order to make style invalidation work for
    // the upcoming frame.
    UpdatePlaceholderText();
    UpdatePlaceholderVisibility();
    PseudoStateChanged(CSSSelector::kPseudoPlaceholderShown);
  }

  ValueMode new_value_mode = input_type_->GetValueMode();

  // https://html.spec.whatwg.org/C/#input-type-change
  //
  // 1. If the previous state of the element's type attribute put the value IDL
  // attribute in the value mode, and the element's value is not the empty
  // string, and the new state of the element's type attribute puts the value
  // IDL attribute in either the default mode or the default/on mode, then set
  // the element's value content attribute to the element's value.
  if (old_value_mode == ValueMode::kValue &&
      (new_value_mode == ValueMode::kDefault ||
       new_value_mode == ValueMode::kDefaultOn)) {
    if (HasDirtyValue() && !non_attribute_value_.empty())
      setAttribute(html_names::kValueAttr, AtomicString(non_attribute_value_));
    non_attribute_value_ = String();
    has_dirty_value_ = false;
  }
  // 2. Otherwise, if the previous state of the element's type attribute put the
  // value IDL attribute in any mode other than the value mode, and the new
  // state of the element's type attribute puts the value IDL attribute in the
  // value mode, then set the value of the element to the value of the value
  // content attribute, if there is one, or the empty string otherwise, and then
  // set the control's dirty value flag to false.
  else if (old_value_mode != ValueMode::kValue &&
           new_value_mode == ValueMode::kValue) {
    AtomicString value_string = FastGetAttribute(html_names::kValueAttr);
    input_type_->WarnIfValueIsInvalid(value_string);
    non_attribute_value_ = SanitizeValue(value_string);
    has_dirty_value_ = false;
  }
  // 3. Otherwise, if the previous state of the element's type attribute put the
  // value IDL attribute in any mode other than the filename mode, and the new
  // state of the element's type attribute puts the value IDL attribute in the
  // filename mode, then set the value of the element to the empty string.
  else if (old_value_mode != ValueMode::kFilename &&
           new_value_mode == ValueMode::kFilename) {
    non_attribute_value_ = String();
    has_dirty_value_ = false;

  } else {
    // ValueMode wasn't changed, or kDefault <-> kDefaultOn.
    if (!HasDirtyValue()) {
      String default_value = FastGetAttribute(html_names::kValueAttr);
      if (!default_value.IsNull())
        input_type_->WarnIfValueIsInvalid(default_value);
    }

    if (new_value_mode == ValueMode::kValue) {
      String new_value = SanitizeValue(non_attribute_value_);
      if (!EqualIgnoringNullity(new_value, non_attribute_value_)) {
        if (HasDirtyValue())
          SetValue(new_value);
        else
          SetNonDirtyValue(new_value);
      }
    }
  }

  needs_to_update_view_value_ = true;
  input_type_view_->UpdateView();

  if (did_respect_height_and_width !=
      input_type_->ShouldRespectHeightAndWidthAttributes()) {
    DCHECK(HasElementData());
    AttributeCollection attributes = AttributesWithoutUpdate();
    if (const Attribute* height = attributes.Find(html_names::kHeightAttr)) {
      TextControlElement::AttributeChanged(AttributeModificationParams(
          html_names::kHeightAttr, height->Value(), height->Value(),
          AttributeModificationReason::kDirectly));
    }
    if (const Attribute* width = attributes.Find(html_names::kWidthAttr)) {
      TextControlElement::AttributeChanged(AttributeModificationParams(
          html_names::kWidthAttr, width->Value(), width->Value(),
          AttributeModificationReason::kDirectly));
    }
    if (const Attribute* align = attributes.Find(html_names::kAlignAttr)) {
      TextControlElement::AttributeChanged(AttributeModificationParams(
          html_names::kAlignAttr, align->Value(), align->Value(),
          AttributeModificationReason::kDirectly));
    }
  }

  // UA Shadow tree was recreated. We need to set selection again. We do it
  // later in order to avoid force layout.
  if (GetDocument().FocusedElement() == this)
    GetDocument().SetShouldUpdateSelectionAfterLayout(true);

  // TODO(tkent): Should we dispatch a change event?
  ClearValueBeforeFirstUserEdit();

  // 5. Signal a type change for the element. (The Radio Button state uses
  // this, in particular.)
  AddToRadioButtonGroup();

  // 8. Let nowSelectable be true if setRangeText() now applies to the element,
  // and false otherwise.
  const bool now_selectable = input_type_->SupportsSelectionAPI();

  // 9. If previouslySelectable is false and nowSelectable is true, set the
  // element's text entry cursor position to the beginning of the text control,
  // and set its selection direction to "none".
  if (!previously_selectable && now_selectable)
    SetSelectionRange(0, 0, kSelectionHasNoDirection);

  SetNeedsValidityCheck();
  if ((could_be_successful_submit_button || CanBeSuccessfulSubmitButton()) &&
      formOwner() && isConnected())
    formOwner()->InvalidateDefaultButtonStyle();
  NotifyFormStateChanged();
}

void HTMLInputElement::SubtreeHasChanged() {
  input_type_view_->SubtreeHasChanged();

  if (HasDirectionAuto() ||
      !RuntimeEnabledFeatures::TextInputNotAlwaysDirAutoEnabled()) {
    // When typing in an input field, childrenChanged is not called, so we
    // need to force the directionality check.
    CalculateAndAdjustAutoDirectionality();
  }
}

FormControlType HTMLInputElement::FormControlType() const {
  return input_type_->FormControlType();
}

const AtomicString& HTMLInputElement::FormControlTypeAsString() const {
  return input_type_->FormControlTypeAsString();
}

bool HTMLInputElement::ShouldSaveAndRestoreFormControlState() const {
  if (!input_type_->ShouldSaveAndRestoreFormControlState())
    return false;
  return TextControlElement::ShouldSaveAndRestoreFormControlState();
}

FormControlState HTMLInputElement::SaveFormControlState() const {
  return input_type_view_->SaveFormControlState();
}

void HTMLInputElement::RestoreFormControlState(const FormControlState& state) {
  input_type_view_->RestoreFormControlState(state);
  state_restored_ = true;
}

bool HTMLInputElement::CanStartSelection() const {
  if (!IsTextField())
    return false;
  return TextControlElement::CanStartSelection();
}

std::optional<uint32_t> HTMLInputElement::selectionStartForBinding(
    ExceptionState& exception_state) const {
  if (!input_type_->SupportsSelectionAPI())
    return std::nullopt;
  return TextControlElement::selectionStart();
}

std::optional<uint32_t> HTMLInputElement::selectionEndForBinding(
    ExceptionState& exception_state) const {
  if (!input_type_->SupportsSelectionAPI())
    return std::nullopt;
  return TextControlElement::selectionEnd();
}

String HTMLInputElement::selectionDirectionForBinding(
    ExceptionState& exception_state) const {
  if (!input_type_->SupportsSelectionAPI()) {
    return String();
  }
  return TextControlElement::selectionDirection();
}

void HTMLInputElement::setSelectionStartForBinding(
    std::optional<uint32_t> start,
    ExceptionState& exception_state) {
  if (!input_type_->SupportsSelectionAPI()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The input element's type ('" + input_type_->FormControlTypeAsString() +
            "') does not support selection.");
    return;
  }
  TextControlElement::setSelectionStart(start.value_or(0));
}

void HTMLInputElement::setSelectionEndForBinding(
    std::optional<uint32_t> end,
    ExceptionState& exception_state) {
  if (!input_type_->SupportsSelectionAPI()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The input element's type ('" + input_type_->FormControlTypeAsString() +
            "') does not support selection.");
    return;
  }
  TextControlElement::setSelectionEnd(end.value_or(0));
}

void HTMLInputElement::setSelectionDirectionForBinding(
    const String& direction,
    ExceptionState& exception_state) {
  if (!input_type_->SupportsSelectionAPI()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The input element's type ('" + input_type_->FormControlTypeAsString() +
            "') does not support selection.");
    return;
  }
  TextControlElement::setSelectionDirection(direction);
}

void HTMLInputElement::setSelectionRangeForBinding(
    unsigned start,
    unsigned end,
    ExceptionState& exception_state) {
  if (!input_type_->SupportsSelectionAPI()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The input element's type ('" + input_type_->FormControlTypeAsString() +
            "') does not support selection.");
    return;
  }
  TextControlElement::setSelectionRangeForBinding(start, end);
}

void HTMLInputElement::setSelectionRangeForBinding(
    unsigned start,
    unsigned end,
    const String& direction,
    ExceptionState& exception_state) {
  if (!input_type_->SupportsSelectionAPI()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The input element's type ('" + input_type_->FormControlTypeAsString() +
            "') does not support selection.");
    return;
  }
  TextControlElement::setSelectionRangeForBinding(start, end, direction);
}

// This function can be used to allow tests to set the selection
// range for Number inputs, which do not support the ordinary
// selection API.
void HTMLInputElement::SetSelectionRangeForTesting(
    unsigned start,
    unsigned end,
    ExceptionState& exception_state) {
  if (FormControlType() != FormControlType::kInputNumber) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The input element's type ('" + input_type_->FormControlTypeAsString() +
            "') is not a number input.");
  }
  TextControlElement::setSelectionRangeForBinding(start, end);
}

void HTMLInputElement::AccessKeyAction(
    SimulatedClickCreationScope creation_scope) {
  input_type_view_->AccessKeyAction(creation_scope);
}

bool HTMLInputElement::IsPresentationAttribute(
    const QualifiedName& name) const {
  // FIXME: Remove type check.
  if (name == html_names::kVspaceAttr || name == html_names::kHspaceAttr ||
      name == html_names::kAlignAttr || name == html_names::kWidthAttr ||
      name == html_names::kHeightAttr ||
      (name == html_names::kBorderAttr &&
       FormControlType() == FormControlType::kInputImage)) {
    return true;
  }
  return TextControlElement::IsPresentationAttribute(name);
}

void HTMLInputElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableCSSPropertyValueSet* style) {
  if (name == html_names::kVspaceAttr) {
    AddHTMLLengthToStyle(style, CSSPropertyID::kMarginTop, value);
    AddHTMLLengthToStyle(style, CSSPropertyID::kMarginBottom, value);
  } else if (name == html_names::kHspaceAttr) {
    AddHTMLLengthToStyle(style, CSSPropertyID::kMarginLeft, value);
    AddHTMLLengthToStyle(style, CSSPropertyID::kMarginRight, value);
  } else if (name == html_names::kAlignAttr) {
    if (input_type_->ShouldRespectAlignAttribute())
      ApplyAlignmentAttributeToStyle(value, style);
  } else if (name == html_names::kWidthAttr) {
    if (input_type_->ShouldRespectHeightAndWidthAttributes()) {
      AddHTMLLengthToStyle(style, CSSPropertyID::kWidth, value);
      const AtomicString& height = FastGetAttribute(html_names::kHeightAttr);
      if (height)
        ApplyAspectRatioToStyle(value, height, style);
    }
  } else if (name == html_names::kHeightAttr) {
    if (input_type_->ShouldRespectHeightAndWidthAttributes()) {
      AddHTMLLengthToStyle(style, CSSPropertyID::kHeight, value);
      const AtomicString& width = FastGetAttribute(html_names::kWidthAttr);
      if (width)
        ApplyAspectRatioToStyle(width, value, style);
    }
  } else if (name == html_names::kBorderAttr &&
             FormControlType() ==
                 FormControlType::kInputImage) {  // FIXME: Remove type check.
    ApplyBorderAttributeToStyle(value, style);
  } else {
    TextControlElement::CollectStyleForPresentationAttribute(name, value,
                                                             style);
  }
}

void HTMLInputElement::DidRecalcStyle(const StyleRecalcChange change) {
  HTMLElement::DidRecalcStyle(change);
  input_type_->DidRecalcStyle(change);
}

void HTMLInputElement::ParseAttribute(
    const AttributeModificationParams& params) {
  DCHECK(input_type_);
  DCHECK(input_type_view_);
  const QualifiedName& name = params.name;
  const AtomicString& value = params.new_value;

  if (name == html_names::kNameAttr) {
    RemoveFromRadioButtonGroup();
    name_ = value;
    AddToRadioButtonGroup();
    TextControlElement::ParseAttribute(params);
  } else if (name == html_names::kAutocompleteAttr) {
    if (EqualIgnoringASCIICase(value, keywords::kOff)) {
      autocomplete_ = kOff;
    } else {
      if (value.empty())
        autocomplete_ = kUninitialized;
      else
        autocomplete_ = kOn;
    }
  } else if (name == html_names::kTypeAttr) {
    if (params.reason != AttributeModificationReason::kByParser &&
        params.old_value != value) {
      UpdateType(value);
    }
  } else if (name == html_names::kValueAttr) {
    // We only need to setChanged if the form is looking at the default value
    // right now.
    if (!HasDirtyValue()) {
      if (input_type_->GetValueMode() == ValueMode::kValue)
        non_attribute_value_ = SanitizeValue(value);
      UpdatePlaceholderVisibility();
      SetNeedsStyleRecalc(
          kSubtreeStyleChange,
          StyleChangeReasonForTracing::FromAttribute(html_names::kValueAttr));
      needs_to_update_view_value_ = true;
    }
    SetNeedsValidityCheck();
    input_type_->WarnIfValueIsInvalidAndElementIsVisible(value);
    input_type_->InRangeChanged();
    if (input_type_view_->HasCreatedShadowSubtree() ||
        !RuntimeEnabledFeatures::CreateInputShadowTreeDuringLayoutEnabled() ||
        !input_type_view_->NeedsShadowSubtree()) {
      input_type_view_->ValueAttributeChanged();
    } else {
      input_type_view_->set_needs_update_view_in_create_shadow_subtree(true);
    }
  } else if (name == html_names::kCheckedAttr) {
    // Another radio button in the same group might be checked by state
    // restore. We shouldn't call SetChecked() even if this has the checked
    // attribute. So, delay the SetChecked() call until
    // finishParsingChildren() is called if parsing is in progress.
    if ((!parsing_in_progress_ ||
         !GetDocument().GetFormController().HasControlStates()) &&
        !dirty_checkedness_) {
      SetChecked(!value.IsNull());
      dirty_checkedness_ = false;
    }
    PseudoStateChanged(CSSSelector::kPseudoDefault);
  } else if (name == html_names::kMaxlengthAttr) {
    SetNeedsValidityCheck();
  } else if (name == html_names::kMinlengthAttr) {
    SetNeedsValidityCheck();
  } else if (name == html_names::kSizeAttr) {
    unsigned size = 0;
    if (value.empty() || !ParseHTMLNonNegativeInteger(value, size) ||
        size == 0 || size > 0x7fffffffu)
      size = kDefaultSize;
    if (size_ != size) {
      size_ = size;
      if (GetLayoutObject()) {
        GetLayoutObject()
            ->SetNeedsLayoutAndIntrinsicWidthsRecalcAndFullPaintInvalidation(
                layout_invalidation_reason::kAttributeChanged);
      }
    }
  } else if (name == html_names::kAltAttr) {
    input_type_view_->AltAttributeChanged();
  } else if (name == html_names::kSrcAttr) {
    input_type_view_->SrcAttributeChanged();
  } else if (name == html_names::kUsemapAttr ||
             name == html_names::kAccesskeyAttr) {
    // FIXME: ignore for the moment
  } else if (name == html_names::kOnsearchAttr) {
    // Search field and slider attributes all just cause updateFromElement to be
    // called through style recalcing.
    SetAttributeEventListener(event_type_names::kSearch,
                              JSEventHandlerForContentAttribute::Create(
                                  GetExecutionContext(), name, value));
  } else if (name == html_names::kIncrementalAttr) {
    UseCounter::Count(GetDocument(), WebFeature::kIncrementalAttribute);
  } else if (name == html_names::kMinAttr) {
    input_type_view_->MinOrMaxAttributeChanged();
    input_type_->SanitizeValueInResponseToMinOrMaxAttributeChange();
    input_type_->InRangeChanged();
    SetNeedsValidityCheck();
    UseCounter::Count(GetDocument(), WebFeature::kMinAttribute);
  } else if (name == html_names::kMaxAttr) {
    input_type_view_->MinOrMaxAttributeChanged();
    input_type_->SanitizeValueInResponseToMinOrMaxAttributeChange();
    input_type_->InRangeChanged();
    SetNeedsValidityCheck();
    UseCounter::Count(GetDocument(), WebFeature::kMaxAttribute);
  } else if (name == html_names::kMultipleAttr) {
    input_type_view_->MultipleAttributeChanged();
    SetNeedsValidityCheck();
  } else if (name == html_names::kStepAttr) {
    input_type_view_->StepAttributeChanged();
    SetNeedsValidityCheck();
    UseCounter::Count(GetDocument(), WebFeature::kStepAttribute);
  } else if (name == html_names::kPatternAttr) {
    SetNeedsValidityCheck();
    UseCounter::Count(GetDocument(), WebFeature::kPatternAttribute);
  } else if (name == html_names::kReadonlyAttr) {
    TextControlElement::ParseAttribute(params);
    input_type_view_->ReadonlyAttributeChanged();
  } else if (name == html_names::kListAttr) {
    has_non_empty_list_ = !value.empty();
    ResetListAttributeTargetObserver();
    ListAttributeTargetChanged();
    PseudoStateChanged(CSSSelector::kPseudoHasDatalist);
    UseCounter::Count(GetDocument(), WebFeature::kListAttribute);
  } else if (name == html_names::kWebkitdirectoryAttr) {
    TextControlElement::ParseAttribute(params);
    UseCounter::Count(GetDocument(), WebFeature::kPrefixedDirectoryAttribute);
  } else {
    if (name == html_names::kFormactionAttr)
      LogUpdateAttributeIfIsolatedWorldAndInDocument("input", params);
    TextControlElement::ParseAttribute(params);
  }
}

void HTMLInputElement::ParserDidSetAttributes() {
  DCHECK(parsing_in_progress_);
  InitializeTypeInParsing();
  TextControlElement::ParserDidSetAttributes();
}

void HTMLInputElement::FinishParsingChildren() {
  parsing_in_progress_ = false;
  DCHECK(input_type_);
  DCHECK(input_type_view_);
  TextControlElement::FinishParsingChildren();
  if (!state_restored_) {
    bool checked = FastHasAttribute(html_names::kCheckedAttr);
    if (checked)
      SetChecked(checked);
    dirty_checkedness_ = false;
  }
}

bool HTMLInputElement::LayoutObjectIsNeeded(const DisplayStyle& style) const {
  return input_type_->LayoutObjectIsNeeded() &&
         TextControlElement::LayoutObjectIsNeeded(style);
}

LayoutObject* HTMLInputElement::CreateLayoutObject(const ComputedStyle& style) {
  return input_type_view_->CreateLayoutObject(style);
}

void HTMLInputElement::AttachLayoutTree(AttachContext& context) {
  TextControlElement::AttachLayoutTree(context);
  if (GetLayoutObject())
    input_type_->OnAttachWithLayoutObject();
  input_type_->CountUsage();
}

void HTMLInputElement::DetachLayoutTree(bool performing_reattach) {
  TextControlElement::DetachLayoutTree(performing_reattach);
  needs_to_update_view_value_ = true;
  input_type_view_->ClosePopupView();
}

String HTMLInputElement::AltText() const {
  // http://www.w3.org/TR/1998/REC-html40-19980424/appendix/notes.html#altgen
  // also heavily discussed by Hixie on bugzilla
  // note this is intentionally different to HTMLImageElement::altText()
  String alt = FastGetAttribute(html_names::kAltAttr);
  // fall back to title attribute
  if (alt.IsNull())
    alt = FastGetAttribute(html_names::kTitleAttr);
  if (alt.IsNull())
    alt = FastGetAttribute(html_names::kValueAttr);
  if (alt.IsNull())
    alt = GetLocale().QueryString(IDS_FORM_INPUT_ALT);
  return alt;
}

bool HTMLInputElement::CanBeSuccessfulSubmitButton() const {
  return input_type_->CanBeSuccessfulSubmitButton();
}

bool HTMLInputElement::IsActivatedSubmit() const {
  return is_activated_submit_;
}

void HTMLInputElement::SetActivatedSubmit(bool flag) {
  is_activated_submit_ = flag;
}

void HTMLInputElement::AppendToFormData(FormData& form_data) {
  if (input_type_->IsFormDataAppendable())
    input_type_->AppendToFormData(form_data);
}

String HTMLInputElement::ResultForDialogSubmit() {
  return input_type_->ResultForDialogSubmit();
}

void HTMLInputElement::ResetImpl() {
  if (input_type_->GetValueMode() == ValueMode::kValue) {
    SetNonDirtyValue(DefaultValue());
    SetNeedsValidityCheck();
  } else if (input_type_->GetValueMode() == ValueMode::kFilename) {
    SetNonDirtyValue(String());
    SetNeedsValidityCheck();
  }
  SetChecked(FastHasAttribute(html_names::kCheckedAttr));
  dirty_checkedness_ = false;
  HTMLFormControlElementWithState::ResetImpl();
}

bool HTMLInputElement::IsTextField() const {
  return input_type_->IsTextFieldInputType();
}

bool HTMLInputElement::IsTelephone() const {
  return input_type_->IsTelephoneInputType();
}

bool HTMLInputElement::IsAutoDirectionalityFormAssociated() const {
  return input_type_->IsAutoDirectionalityFormAssociated();
}

bool HTMLInputElement::HasBeenPasswordField() const {
  return has_been_password_field_;
}

void HTMLInputElement::DispatchChangeEventIfNeeded() {
  if (isConnected() && input_type_->ShouldSendChangeEventAfterCheckedChanged())
    DispatchChangeEvent();
}

void HTMLInputElement::DispatchInputAndChangeEventIfNeeded() {
  if (isConnected() &&
      input_type_->ShouldSendChangeEventAfterCheckedChanged()) {
    DispatchInputEvent();
    DispatchChangeEvent();
  }
}

bool HTMLInputElement::IsCheckable() const {
  return input_type_->IsCheckable();
}

bool HTMLInputElement::Checked() const {
  input_type_->ReadingChecked();
  return is_checked_;
}

void HTMLInputElement::setCheckedForBinding(bool now_checked) {
  SetChecked(now_checked, TextFieldEventBehavior::kDispatchNoEvent,
             IsAutofilled() && this->Checked() == now_checked
                 ? WebAutofillState::kAutofilled
                 : WebAutofillState::kNotFilled);
}

void HTMLInputElement::SetChecked(bool now_checked,
                                  TextFieldEventBehavior event_behavior,
                                  WebAutofillState autofill_state) {
  SetAutofillState(autofill_state);

  dirty_checkedness_ = true;
  if (Checked() == now_checked)
    return;

  input_type_->WillUpdateCheckedness(now_checked);
  is_checked_ = now_checked;

  if (RadioButtonGroupScope* scope = GetRadioButtonGroupScope())
    scope->UpdateCheckedState(this);
  InvalidateIfHasEffectiveAppearance();
  SetNeedsValidityCheck();

  // Ideally we'd do this from the layout tree (matching
  // LayoutTextView), but it's not possible to do it at the moment
  // because of the way the code is structured.
  if (GetLayoutObject()) {
    if (AXObjectCache* cache =
            GetLayoutObject()->GetDocument().ExistingAXObjectCache())
      cache->CheckedStateChanged(this);
  }

  if (!RuntimeEnabledFeatures::AllowJavaScriptToResetAutofillStateEnabled()) {
    // Only send a change event for items in the document (avoid firing during
    // parsing) and don't send a change event for a radio button that's getting
    // unchecked to match other browsers. DOM is not a useful standard for this
    // because it says only to fire change events at "lose focus" time, which is
    // definitely wrong in practice for these types of elements.
    if (event_behavior ==
            TextFieldEventBehavior::kDispatchInputAndChangeEvent &&
        isConnected() &&
        input_type_->ShouldSendChangeEventAfterCheckedChanged()) {
      DispatchInputEvent();
    }
  }

  // We set the Autofilled state again because setting the autofill value
  // triggers JavaScript events and the site may override the autofilled value,
  // which resets the autofill state. Even if the website modifies the from
  // control element's content during the autofill operation, we want the state
  // to show as as autofilled.
  SetAutofillState(autofill_state);

  PseudoStateChanged(CSSSelector::kPseudoChecked);
}

void HTMLInputElement::setIndeterminate(bool new_value) {
  if (indeterminate() == new_value)
    return;

  is_indeterminate_ = new_value;

  PseudoStateChanged(CSSSelector::kPseudoIndeterminate);

  InvalidateIfHasEffectiveAppearance();

  if (AXObjectCache* cache = GetDocument().ExistingAXObjectCache())
    cache->CheckedStateChanged(this);
}

unsigned HTMLInputElement::size() const {
  return size_;
}

bool HTMLInputElement::SizeShouldIncludeDecoration(int& preferred_size) const {
  return input_type_view_->SizeShouldIncludeDecoration(kDefaultSize,
                                                       preferred_size);
}

void HTMLInputElement::CloneNonAttributePropertiesFrom(const Element& source,
                                                       NodeCloningData& data) {
  const auto& source_element = To<HTMLInputElement>(source);

  non_attribute_value_ = source_element.non_attribute_value_;
  has_dirty_value_ = source_element.has_dirty_value_;
  SetChecked(source_element.is_checked_);
  dirty_checkedness_ = source_element.dirty_checkedness_;
  is_indeterminate_ = source_element.is_indeterminate_;
  input_type_->CopyNonAttributeProperties(source_element);

  TextControlElement::CloneNonAttributePropertiesFrom(source, data);

  needs_to_update_view_value_ = true;
  input_type_view_->UpdateView();
}

String HTMLInputElement::Value() const {
  switch (input_type_->GetValueMode()) {
    case ValueMode::kFilename:
      return input_type_->ValueInFilenameValueMode();
    case ValueMode::kDefault:
      return FastGetAttribute(html_names::kValueAttr);
    case ValueMode::kDefaultOn: {
      AtomicString value_string = FastGetAttribute(html_names::kValueAttr);
      return value_string.IsNull() ? AtomicString("on") : value_string;
    }
    case ValueMode::kValue:
      return non_attribute_value_;
  }
  NOTREACHED_IN_MIGRATION();
  return g_empty_string;
}

String HTMLInputElement::ValueOrDefaultLabel() const {
  String value = this->Value();
  if (!value.IsNull())
    return value;
  return input_type_->DefaultLabel();
}

void HTMLInputElement::SetValueForUser(const String& value) {
  // Call setValue and make it send a change event.
  SetValue(value, TextFieldEventBehavior::kDispatchChangeEvent);
}

void HTMLInputElement::SetSuggestedValue(const String& value) {
  if (!input_type_->CanSetSuggestedValue()) {
    // Clear the suggested value because it may have been set when
    // `input_type_->CanSetSuggestedValue()` was true.
    SetAutofillState(WebAutofillState::kNotFilled);
    TextControlElement::SetSuggestedValue(String());
    return;
  }
  needs_to_update_view_value_ = true;
  String sanitized_value = SanitizeValue(value);
  SetAutofillState(sanitized_value.empty() ? WebAutofillState::kNotFilled
                                           : WebAutofillState::kPreviewed);
  TextControlElement::SetSuggestedValue(sanitized_value);

  // Update the suggested value revelation.
  if (auto* placeholder = PlaceholderElement()) {
    const AtomicString reveal("reveal-password");
    if (should_reveal_password_) {
      placeholder->classList().Add(reveal);
    } else {
      placeholder->classList().Remove(reveal);
    }
  }

  SetNeedsStyleRecalc(
      kSubtreeStyleChange,
      StyleChangeReasonForTracing::Create(style_change_reason::kControlValue));
  input_type_view_->UpdateView();
}

void HTMLInputElement::SetInnerEditorValue(const String& value) {
  TextControlElement::SetInnerEditorValue(value);
  if (InnerEditorElement()) {
    needs_to_update_view_value_ = false;
  }
}

void HTMLInputElement::setValueForBinding(const String& value,
                                          ExceptionState& exception_state) {
  // FIXME: Remove type check.
  if (FormControlType() == FormControlType::kInputFile && !value.empty()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "This input element accepts a filename, "
                                      "which may only be programmatically set "
                                      "to the empty string.");
    return;
  }
  String old_value = this->Value();
  bool was_autofilled = IsAutofilled();
  bool value_changed = old_value != value;
  SetValue(value, TextFieldEventBehavior::kDispatchNoEvent,
           TextControlSetValueSelection::kSetSelectionToEnd,
           was_autofilled && !value_changed && !value.empty()
               ? WebAutofillState::kAutofilled
               : WebAutofillState::kNotFilled);
  if (Page* page = GetDocument().GetPage(); page && value_changed) {
    page->GetChromeClient().JavaScriptChangedValue(*this, old_value,
                                                   was_autofilled);
  }
}

void HTMLInputElement::SetValue(const String& value,
                                TextFieldEventBehavior event_behavior,
                                TextControlSetValueSelection selection,
                                WebAutofillState autofill_state) {
  input_type_->WarnIfValueIsInvalidAndElementIsVisible(value);
  if (!input_type_->CanSetValue(value))
    return;

  // Clear the suggested value. Use the base class version to not trigger a view
  // update.
  TextControlElement::SetSuggestedValue(String());

  SetAutofillState(autofill_state);

  // Scope for EventQueueScope so that change events are dispatched and handled
  // before the second SetAutofillState is executed.
  {
    EventQueueScope scope;
    String sanitized_value = SanitizeValue(value);
    bool value_changed = sanitized_value != this->Value();

    SetLastChangeWasNotUserEdit();
    needs_to_update_view_value_ = true;

    input_type_->SetValue(sanitized_value, value_changed, event_behavior,
                          selection);
    input_type_view_->DidSetValue(sanitized_value, value_changed);

    if (value_changed) {
      NotifyFormStateChanged();
      if (sanitized_value.empty() && HasBeenPasswordField() &&
          GetDocument().GetPage()) {
        GetDocument().GetPage()->GetChromeClient().PasswordFieldReset(*this);
      }
    }
  }

  if (!RuntimeEnabledFeatures::AllowJavaScriptToResetAutofillStateEnabled()) {
    // We set the Autofilled state again because setting the autofill value
    // triggers JavaScript events and the site may override the autofilled
    // value, which resets the autofill state. Even if the website modifies the
    // form control element's content during the autofill operation, we want the
    // state to show as autofilled.
    // If AllowJavaScriptToResetAutofillState is enabled, the WebAutofillClient
    // will monitor JavaScript induced changes and take care of resetting the
    // autofill state when appropriate.
    SetAutofillState(autofill_state);
  }
}

void HTMLInputElement::SetNonAttributeValue(const String& sanitized_value) {
  // This is a common code for ValueMode::kValue.
  DCHECK_EQ(input_type_->GetValueMode(), ValueMode::kValue);
  non_attribute_value_ = sanitized_value;
  has_dirty_value_ = true;
  SetNeedsValidityCheck();
  input_type_->InRangeChanged();
}

void HTMLInputElement::SetNonAttributeValueByUserEdit(
    const String& sanitized_value) {
  SetValueBeforeFirstUserEditIfNotSet();
  SetNonAttributeValue(sanitized_value);
  CheckIfValueWasReverted(sanitized_value);
}

void HTMLInputElement::SetNonDirtyValue(const String& new_value) {
  SetValue(new_value);
  has_dirty_value_ = false;
}

bool HTMLInputElement::HasDirtyValue() const {
  return has_dirty_value_;
}

void HTMLInputElement::UpdateView() {
  input_type_view_->UpdateView();
}

ScriptValue HTMLInputElement::valueAsDate(ScriptState* script_state) const {
  UseCounter::Count(GetDocument(), WebFeature::kInputElementValueAsDateGetter);
  // TODO(crbug.com/988343): InputType::ValueAsDate() should return
  // std::optional<base::Time>.
  double date = input_type_->ValueAsDate();
  v8::Isolate* isolate = script_state->GetIsolate();
  if (!std::isfinite(date))
    return ScriptValue::CreateNull(isolate);
  return ScriptValue(
      isolate,
      ToV8Traits<IDLNullable<IDLDate>>::ToV8(
          script_state, base::Time::FromMillisecondsSinceUnixEpoch(date)));
}

void HTMLInputElement::setValueAsDate(ScriptState* script_state,
                                      const ScriptValue& value,
                                      ExceptionState& exception_state) {
  UseCounter::Count(GetDocument(), WebFeature::kInputElementValueAsDateSetter);
  std::optional<base::Time> date =
      NativeValueTraits<IDLNullable<IDLDate>>::NativeValue(
          script_state->GetIsolate(), value.V8Value(), exception_state);
  if (exception_state.HadException())
    return;
  input_type_->SetValueAsDate(date, exception_state);
}

double HTMLInputElement::valueAsNumber() const {
  return input_type_->ValueAsDouble();
}

void HTMLInputElement::setValueAsNumber(double new_value,
                                        ExceptionState& exception_state,
                                        TextFieldEventBehavior event_behavior) {
  // http://www.whatwg.org/specs/web-apps/current-work/multipage/common-input-element-attributes.html#dom-input-valueasnumber
  // On setting, if the new value is infinite, then throw a TypeError exception.
  if (std::isinf(new_value)) {
    exception_state.ThrowTypeError(
        ExceptionMessages::NotAFiniteNumber(new_value));
    return;
  }
  input_type_->SetValueAsDouble(new_value, event_behavior, exception_state);
}

Decimal HTMLInputElement::RatioValue() const {
  DCHECK_EQ(FormControlType(), FormControlType::kInputRange);
  const StepRange step_range(CreateStepRange(kRejectAny));
  const Decimal old_value =
      ParseToDecimalForNumberType(Value(), step_range.DefaultValue());
  return step_range.ProportionFromValue(step_range.ClampValue(old_value));
}

void HTMLInputElement::SetValueFromRenderer(const String& value) {
  // File upload controls will never use this.
  DCHECK_NE(FormControlType(), FormControlType::kInputFile);

  // Clear the suggested value. Use the base class version to not trigger a view
  // update.
  TextControlElement::SetSuggestedValue(String());

  // Renderer and our event handler are responsible for sanitizing values.
  DCHECK(value == input_type_->SanitizeUserInputValue(value) ||
         input_type_->SanitizeUserInputValue(value).empty());

  DCHECK(!value.IsNull());
  SetValueBeforeFirstUserEditIfNotSet();
  non_attribute_value_ = value;
  has_dirty_value_ = true;
  if (InnerEditorElement()) {
    needs_to_update_view_value_ = false;
  }
  CheckIfValueWasReverted(value);

  // Input event is fired by the Node::defaultEventHandler for editable
  // controls.
  if (!IsTextField())
    DispatchInputEvent();
  NotifyFormStateChanged();

  SetNeedsValidityCheck();

  // Clear autofill flag (and yellow background) on user edit.
  SetAutofillState(WebAutofillState::kNotFilled);
}

EventDispatchHandlingState* HTMLInputElement::PreDispatchEventHandler(
    Event& event) {
  if (event.type() == event_type_names::kTextInput &&
      input_type_view_->ShouldSubmitImplicitly(event)) {
    event.stopPropagation();
    return nullptr;
  }
  if (event.type() != event_type_names::kClick)
    return nullptr;

  auto* mouse_event = DynamicTo<MouseEvent>(event);
  if (!mouse_event ||
      mouse_event->button() !=
          static_cast<int16_t>(WebPointerProperties::Button::kLeft))
    return nullptr;
  return input_type_view_->WillDispatchClick();
}

void HTMLInputElement::PostDispatchEventHandler(
    Event& event,
    EventDispatchHandlingState* state) {
  if (!state)
    return;
  input_type_view_->DidDispatchClick(event,
                                     *static_cast<ClickHandlingState*>(state));
}

void HTMLInputElement::DefaultEventHandler(Event& evt) {
  auto* mouse_event = DynamicTo<MouseEvent>(evt);
  if (mouse_event && evt.type() == event_type_names::kClick &&
      mouse_event->button() ==
          static_cast<int16_t>(WebPointerProperties::Button::kLeft)) {
    input_type_view_->HandleClickEvent(To<MouseEvent>(evt));
    if (evt.DefaultHandled())
      return;
  }

  auto* keyboad_event = DynamicTo<KeyboardEvent>(evt);
  if (keyboad_event && evt.type() == event_type_names::kKeydown) {
    input_type_view_->HandleKeydownEvent(*keyboad_event);
    if (evt.DefaultHandled())
      return;
  }

  // Call the base event handler before any of our own event handling for almost
  // all events in text fields.  Makes editing keyboard handling take precedence
  // over the keydown and keypress handling in this function.
  bool call_base_class_early =
      IsTextField() && (evt.type() == event_type_names::kKeydown ||
                        evt.type() == event_type_names::kKeypress);
  if (call_base_class_early) {
    TextControlElement::DefaultEventHandler(evt);
    if (evt.DefaultHandled())
      return;
  }

  // DOMActivate events cause the input to be "activated" - in the case of image
  // and submit inputs, this means actually submitting the form. For reset
  // inputs, the form is reset. These events are sent when the user clicks on
  // the element, or presses enter while it is the active element. JavaScript
  // code wishing to activate the element must dispatch a DOMActivate event - a
  // click event will not do the job.
  if (evt.type() == event_type_names::kDOMActivate) {
    input_type_view_->HandleDOMActivateEvent(evt);
    if (evt.DefaultHandled())
      return;
  }

  // Use key press event here since sending simulated mouse events
  // on key down blocks the proper sending of the key press event.
  if (keyboad_event && evt.type() == event_type_names::kKeypress) {
    input_type_view_->HandleKeypressEvent(*keyboad_event);
    if (evt.DefaultHandled())
      return;
  }

  if (keyboad_event && evt.type() == event_type_names::kKeyup) {
    input_type_view_->HandleKeyupEvent(*keyboad_event);
    if (evt.DefaultHandled())
      return;
  }

  if (input_type_view_->ShouldSubmitImplicitly(evt)) {
    // FIXME: Remove type check.
    if (FormControlType() == FormControlType::kInputSearch) {
      GetDocument()
          .GetTaskRunner(TaskType::kUserInteraction)
          ->PostTask(FROM_HERE, WTF::BindOnce(&HTMLInputElement::OnSearch,
                                              WrapPersistent(this)));
    }
    // Form submission finishes editing, just as loss of focus does.
    // If there was a change, send the event now.
    DispatchFormControlChangeEvent();

    HTMLFormElement* form_for_submission =
        input_type_view_->FormForSubmission();
    // Form may never have been present, or may have been destroyed by code
    // responding to the change event.
    if (form_for_submission) {
      form_for_submission->SubmitImplicitly(evt,
                                            CanTriggerImplicitSubmission());
    }
    evt.SetDefaultHandled();
    return;
  }

  if (evt.IsBeforeTextInsertedEvent()) {
    input_type_view_->HandleBeforeTextInsertedEvent(
        static_cast<BeforeTextInsertedEvent&>(evt));
  }

  if (mouse_event && evt.type() == event_type_names::kMousedown) {
    input_type_view_->HandleMouseDownEvent(*mouse_event);
    if (evt.DefaultHandled())
      return;
  }

  input_type_view_->ForwardEvent(evt);

  if (!call_base_class_early && !evt.DefaultHandled())
    TextControlElement::DefaultEventHandler(evt);
}

ShadowRoot* HTMLInputElement::EnsureShadowSubtree() {
  scheduled_create_shadow_tree_ = false;
  input_type_view_->CreateShadowSubtreeIfNeeded();
  return UserAgentShadowRoot();
}

bool HTMLInputElement::HasActivationBehavior() const {
  return true;
}

bool HTMLInputElement::WillRespondToMouseClickEvents() {
  // FIXME: Consider implementing willRespondToMouseClickEvents() in InputType
  // if more accurate results are necessary.
  if (!IsDisabledFormControl())
    return true;

  return TextControlElement::WillRespondToMouseClickEvents();
}

bool HTMLInputElement::IsURLAttribute(const Attribute& attribute) const {
  return attribute.GetName() == html_names::kSrcAttr ||
         attribute.GetName() == html_names::kFormactionAttr ||
         TextControlElement::IsURLAttribute(attribute);
}

bool HTMLInputElement::HasLegalLinkAttribute(const QualifiedName& name) const {
  return input_type_->HasLegalLinkAttribute(name) ||
         TextControlElement::HasLegalLinkAttribute(name);
}

const AtomicString& HTMLInputElement::DefaultValue() const {
  return FastGetAttribute(html_names::kValueAttr);
}

static inline bool IsRFC2616TokenCharacter(UChar ch) {
  return IsASCII(ch) && ch > ' ' && ch != '"' && ch != '(' && ch != ')' &&
         ch != ',' && ch != '/' && (ch < ':' || ch > '@') &&
         (ch < '[' || ch > ']') && ch != '{' && ch != '}' && ch != 0x7f;
}

static bool IsValidMIMEType(const String& type) {
  size_t slash_position = type.find('/');
  if (slash_position == kNotFound || !slash_position ||
      slash_position == type.length() - 1)
    return false;
  for (wtf_size_t i = 0; i < type.length(); ++i) {
    if (!IsRFC2616TokenCharacter(type[i]) && i != slash_position)
      return false;
  }
  return true;
}

static bool IsValidFileExtension(const String& type) {
  if (type.length() < 2)
    return false;
  return type[0] == '.';
}

static Vector<String> ParseAcceptAttribute(const String& accept_string,
                                           bool (*predicate)(const String&)) {
  Vector<String> types;
  if (accept_string.empty())
    return types;

  Vector<String> split_types;
  accept_string.Split(',', false, split_types);
  for (const String& split_type : split_types) {
    String trimmed_type = StripLeadingAndTrailingHTMLSpaces(split_type);
    if (trimmed_type.empty())
      continue;
    if (!predicate(trimmed_type))
      continue;
    types.push_back(trimmed_type.DeprecatedLower());
  }

  return types;
}

Vector<String> HTMLInputElement::AcceptMIMETypes() const {
  return ParseAcceptAttribute(FastGetAttribute(html_names::kAcceptAttr),
                              IsValidMIMEType);
}

Vector<String> HTMLInputElement::AcceptFileExtensions() const {
  return ParseAcceptAttribute(FastGetAttribute(html_names::kAcceptAttr),
                              IsValidFileExtension);
}

const AtomicString& HTMLInputElement::Alt() const {
  return FastGetAttribute(html_names::kAltAttr);
}

bool HTMLInputElement::Multiple() const {
  return FastHasAttribute(html_names::kMultipleAttr);
}

void HTMLInputElement::setSize(unsigned size, ExceptionState& exception_state) {
  if (size == 0) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        "The value provided is 0, which is an invalid size.");
  } else {
    SetUnsignedIntegralAttribute(html_names::kSizeAttr,
                                 size ? size : kDefaultSize, kDefaultSize);
  }
}

KURL HTMLInputElement::Src() const {
  return GetDocument().CompleteURL(FastGetAttribute(html_names::kSrcAttr));
}

FileList* HTMLInputElement::files() const {
  return input_type_->Files();
}

void HTMLInputElement::setFiles(FileList* files) {
  input_type_->SetFiles(files);
}

bool HTMLInputElement::ReceiveDroppedFiles(const DragData* drag_data) {
  return input_type_->ReceiveDroppedFiles(drag_data);
}

String HTMLInputElement::DroppedFileSystemId() {
  return input_type_->DroppedFileSystemId();
}

bool HTMLInputElement::CanReceiveDroppedFiles() const {
  return can_receive_dropped_files_;
}

void HTMLInputElement::SetCanReceiveDroppedFiles(
    bool can_receive_dropped_files) {
  if (!!can_receive_dropped_files_ == can_receive_dropped_files)
    return;
  can_receive_dropped_files_ = can_receive_dropped_files;
  if (HTMLInputElement* button = UploadButton())
    button->SetActive(can_receive_dropped_files);
}

HTMLInputElement* HTMLInputElement::UploadButton() const {
  return input_type_view_->UploadButton();
}

String HTMLInputElement::SanitizeValue(const String& proposed_value) const {
  return input_type_->SanitizeValue(proposed_value);
}

String HTMLInputElement::LocalizeValue(const String& proposed_value) const {
  if (proposed_value.IsNull())
    return proposed_value;
  return input_type_->LocalizeValue(proposed_value);
}

bool HTMLInputElement::IsInRange() const {
  return willValidate() && input_type_->IsInRange(Value());
}

bool HTMLInputElement::IsOutOfRange() const {
  return willValidate() && input_type_->IsOutOfRange(Value());
}

bool HTMLInputElement::IsRequiredFormControl() const {
  return input_type_->SupportsRequired() && IsRequired();
}

bool HTMLInputElement::MatchesReadOnlyPseudoClass() const {
  return !input_type_->SupportsReadOnly() || IsDisabledOrReadOnly();
}

bool HTMLInputElement::MatchesReadWritePseudoClass() const {
  return input_type_->SupportsReadOnly() && !IsDisabledOrReadOnly();
}

ControlPart HTMLInputElement::AutoAppearance() const {
  return input_type_view_->AutoAppearance();
}

void HTMLInputElement::OnSearch() {
  input_type_->DispatchSearchEvent();
}

void HTMLInputElement::UpdateClearButtonVisibility() {
  input_type_view_->UpdateClearButtonVisibility();
}

bool HTMLInputElement::IsInnerEditorValueEmpty() const {
  return input_type_view_->IsInnerEditorValueEmpty();
}

void HTMLInputElement::WillChangeForm() {
  if (input_type_)
    RemoveFromRadioButtonGroup();
  TextControlElement::WillChangeForm();
}

void HTMLInputElement::DidChangeForm() {
  TextControlElement::DidChangeForm();
  if (input_type_)
    AddToRadioButtonGroup();
}

Node::InsertionNotificationRequest HTMLInputElement::InsertedInto(
    ContainerNode& insertion_point) {
  TextControlElement::InsertedInto(insertion_point);
  if (insertion_point.isConnected()) {
    if (!Form()) {
      AddToRadioButtonGroup();
    }
    if (RuntimeEnabledFeatures::CreateInputShadowTreeDuringLayoutEnabled() &&
        !input_type_view_->HasCreatedShadowSubtree() &&
        input_type_view_->NeedsShadowSubtree()) {
      scheduled_create_shadow_tree_ = true;
      GetDocument().ScheduleShadowTreeCreation(*this);
    }
  }
  ResetListAttributeTargetObserver();
  LogAddElementIfIsolatedWorldAndInDocument("input", html_names::kTypeAttr,
                                            html_names::kFormactionAttr);
  if (!RuntimeEnabledFeatures::CreateInputShadowTreeDuringLayoutEnabled()) {
    EventDispatchForbiddenScope::AllowUserAgentEvents allow_events;
    input_type_view_->CreateShadowSubtreeIfNeeded();
  }
  return kInsertionShouldCallDidNotifySubtreeInsertions;
}

void HTMLInputElement::RemovedFrom(ContainerNode& insertion_point) {
  input_type_view_->ClosePopupView();
  if (insertion_point.isConnected()) {
    if (!Form()) {
      RemoveFromRadioButtonGroup();
    }
    if (scheduled_create_shadow_tree_) {
      scheduled_create_shadow_tree_ = false;
      GetDocument().UnscheduleShadowTreeCreation(*this);
    }
  }
  TextControlElement::RemovedFrom(insertion_point);
  DCHECK(!isConnected());
  ResetListAttributeTargetObserver();
}

void HTMLInputElement::DidMoveToNewDocument(Document& old_document) {
  if (ImageLoader())
    ImageLoader()->ElementDidMoveToNewDocument();

  // FIXME: Remove type check.
  if (FormControlType() == FormControlType::kInputRadio) {
    GetTreeScope().GetRadioButtonGroupScope().RemoveButton(this);
  }

  TextControlElement::DidMoveToNewDocument(old_document);
}

bool HTMLInputElement::RecalcWillValidate() const {
  return input_type_->SupportsValidation() &&
         TextControlElement::RecalcWillValidate();
}

void HTMLInputElement::RequiredAttributeChanged() {
  TextControlElement::RequiredAttributeChanged();
  if (RadioButtonGroupScope* scope = GetRadioButtonGroupScope())
    scope->RequiredAttributeChanged(this);
  input_type_view_->RequiredAttributeChanged();
}

void HTMLInputElement::DisabledAttributeChanged() {
  TextControlElement::DisabledAttributeChanged();
  input_type_view_->DisabledAttributeChanged();
}

void HTMLInputElement::SelectColorInColorChooser(const Color& color) {
  if (ColorChooserClient* client = input_type_->GetColorChooserClient())
    client->DidChooseColor(color);
}

void HTMLInputElement::EndColorChooserForTesting() {
  input_type_view_->ClosePopupView();
}

HTMLDataListElement* HTMLInputElement::DataList() const {
  if (!has_non_empty_list_) {
    return nullptr;
  }

  if (!input_type_->ShouldRespectListAttribute()) {
    return nullptr;
  }

  return DynamicTo<HTMLDataListElement>(
      GetElementAttributeResolvingReferenceTarget(html_names::kListAttr));
}

HTMLElement* HTMLInputElement::listForBinding() const {
  if (!has_non_empty_list_) {
    return nullptr;
  }

  if (!input_type_->ShouldRespectListAttribute()) {
    return nullptr;
  }

  return DynamicTo<HTMLDataListElement>(
      GetElementAttribute(html_names::kListAttr));
}

bool HTMLInputElement::HasValidDataListOptions() const {
  HTMLDataListElement* data_list = DataList();
  if (!data_list)
    return false;
  HTMLDataListOptionsCollection* options = data_list->options();
  for (unsigned i = 0; HTMLOptionElement* option = options->Item(i); ++i) {
    if (!option->value().empty() && !option->IsDisabledFormControl())
      return true;
  }
  return false;
}

HeapVector<Member<HTMLOptionElement>>
HTMLInputElement::FilteredDataListOptions() const {
  HeapVector<Member<HTMLOptionElement>> filtered;
  HTMLDataListElement* data_list = DataList();
  if (!data_list)
    return filtered;

  // Ensure the editor has been created as InnerEditorValue() returns an empty
  // string if the editor wasn't created.
  EnsureInnerEditorElement();

  String editor_value = InnerEditorValue();
  if (Multiple() && FormControlType() == FormControlType::kInputEmail) {
    Vector<String> emails;
    editor_value.Split(',', true, emails);
    if (!emails.empty())
      editor_value = emails.back().StripWhiteSpace();
  }

  HTMLDataListOptionsCollection* options = data_list->options();
  filtered.reserve(options->length());
  editor_value = editor_value.FoldCase();

  TextBreakIterator* iter =
      WordBreakIterator(editor_value, 0, editor_value.length());

  Vector<bool> filtering_flag(options->length(), true);
  if (iter) {
    for (int word_start = iter->current(), word_end = iter->next();
         word_end != kTextBreakDone; word_end = iter->next()) {
      String value = editor_value.Substring(word_start, word_end - word_start);
      word_start = word_end;

      if (!IsWordBreak(value[0]))
        continue;

      for (unsigned i = 0; i < options->length(); ++i) {
        if (!filtering_flag[i])
          continue;
        HTMLOptionElement* option = options->Item(i);
        DCHECK(option);
        if (!value.empty()) {
          // Firefox shows OPTIONs with matched labels, Edge shows OPTIONs
          // with matches values. We show both.
          if (!(option->value()
                        .FoldCase()
                        .RemoveCharacters(IsWhitespace)
                        .Find(value) == kNotFound &&
                option->label()
                        .FoldCase()
                        .RemoveCharacters(IsWhitespace)
                        .Find(value) == kNotFound))
            continue;
        }
        filtering_flag[i] = false;
      }
    }
  }

  for (unsigned i = 0; i < options->length(); ++i) {
    HTMLOptionElement* option = options->Item(i);
    DCHECK(option);
    if (option->value().empty() || option->IsDisabledFormControl())
      continue;
    if (filtering_flag[i])
      filtered.push_back(option);
  }
  return filtered;
}

void HTMLInputElement::SetListAttributeTargetObserver(
    ListAttributeTargetObserver* new_observer) {
  if (list_attribute_target_observer_)
    list_attribute_target_observer_->Unregister();
  list_attribute_target_observer_ = new_observer;
}

void HTMLInputElement::ResetListAttributeTargetObserver() {
  const AtomicString& value = FastGetAttribute(html_names::kListAttr);
  if (!value.IsNull() && isConnected()) {
    SetListAttributeTargetObserver(
        MakeGarbageCollected<ListAttributeTargetObserver>(value, this));
  } else {
    SetListAttributeTargetObserver(nullptr);
  }
}

void HTMLInputElement::ListAttributeTargetChanged() {
  input_type_view_->ListAttributeTargetChanged();
  PseudoStateChanged(CSSSelector::kPseudoHasDatalist);
}

bool HTMLInputElement::IsSteppable() const {
  return input_type_->IsSteppable();
}

bool HTMLInputElement::IsButton() const {
  return input_type_->IsButton();
}

bool HTMLInputElement::IsTextButton() const {
  return input_type_->IsTextButton();
}

bool HTMLInputElement::IsEnumeratable() const {
  return input_type_->IsEnumeratable();
}

bool HTMLInputElement::IsLabelable() const {
  return input_type_->IsInteractiveContent();
}

bool HTMLInputElement::MatchesDefaultPseudoClass() const {
  return input_type_->MatchesDefaultPseudoClass();
}

int HTMLInputElement::scrollWidth() {
  if (!IsTextField())
    return TextControlElement::scrollWidth();
  // If in preview state, fake the scroll width to prevent that any information
  // about the suggested content can be derived from the size.
  if (!SuggestedValue().empty())
    return clientWidth();

  GetDocument().UpdateStyleAndLayoutForNode(this,
                                            DocumentUpdateReason::kJavaScript);
  const auto* editor = InnerEditorElement();
  const auto* editor_box = editor ? editor->GetLayoutBox() : nullptr;
  const auto* box = GetLayoutBox();
  if (!editor_box || !box)
    return TextControlElement::scrollWidth();
  // Adjust scrollWidth to include input element horizontal paddings and
  // decoration width.
  LayoutUnit adjustment = box->ClientWidth() - editor_box->ClientWidth();
  int snapped_scroll_width =
      SnapSizeToPixel(editor_box->ScrollWidth() + adjustment,
                      box->PhysicalLocation().left + box->ClientLeft());
  return AdjustForAbsoluteZoom::AdjustLayoutUnit(
             LayoutUnit(snapped_scroll_width), box->StyleRef())
      .Round();
}

int HTMLInputElement::scrollHeight() {
  if (!IsTextField())
    return TextControlElement::scrollHeight();

  // If in preview state, fake the scroll height to prevent that any information
  // about the suggested content can be derived from the size.
  if (!SuggestedValue().empty())
    return clientHeight();

  GetDocument().UpdateStyleAndLayoutForNode(this,
                                            DocumentUpdateReason::kJavaScript);
  const auto* editor = InnerEditorElement();
  const auto* editor_box = editor ? editor->GetLayoutBox() : nullptr;
  const auto* box = GetLayoutBox();
  if (!editor_box || !box)
    return TextControlElement::scrollHeight();
  // Adjust scrollHeight to include input element vertical paddings and
  // decoration height.
  LayoutUnit adjustment = box->ClientHeight() - editor_box->ClientHeight();
  return AdjustForAbsoluteZoom::AdjustLayoutUnit(
             editor_box->ScrollHeight() + adjustment, box->StyleRef())
      .Round();
}

bool HTMLInputElement::ShouldAppearChecked() const {
  return Checked() && IsCheckable();
}

void HTMLInputElement::SetPlaceholderVisibility(bool visible) {
  is_placeholder_visible_ = visible;
}

bool HTMLInputElement::SupportsPlaceholder() const {
  return input_type_->SupportsPlaceholder();
}

void HTMLInputElement::CreateInnerEditorElementIfNecessary() const {
  input_type_view_->CreateShadowSubtreeIfNeeded();
}

HTMLElement* HTMLInputElement::UpdatePlaceholderText() {
  return input_type_view_->UpdatePlaceholderText(!SuggestedValue().empty());
}

String HTMLInputElement::GetPlaceholderValue() const {
  return !SuggestedValue().empty() ? SuggestedValue() : StrippedPlaceholder();
}

String HTMLInputElement::DefaultToolTip() const {
  return input_type_->DefaultToolTip(*input_type_view_);
}

String HTMLInputElement::FileStatusText() const {
  return input_type_view_->FileStatusText();
}

bool HTMLInputElement::ShouldApplyMiddleEllipsis() const {
  return files() && files()->length() <= 1;
}

bool HTMLInputElement::ShouldAppearIndeterminate() const {
  return input_type_->ShouldAppearIndeterminate();
}

HTMLFormControlElement::PopoverTriggerSupport
HTMLInputElement::SupportsPopoverTriggering() const {
  return input_type_->SupportsPopoverTriggering();
}

RadioButtonGroupScope* HTMLInputElement::GetRadioButtonGroupScope() const {
  // FIXME: Remove type check.
  if (FormControlType() != FormControlType::kInputRadio) {
    return nullptr;
  }
  if (HTMLFormElement* form_element = Form())
    return &form_element->GetRadioButtonGroupScope();
  if (isConnected())
    return &GetTreeScope().GetRadioButtonGroupScope();
  return nullptr;
}

unsigned HTMLInputElement::SizeOfRadioGroup() const {
  RadioButtonGroupScope* scope = GetRadioButtonGroupScope();
  if (!scope)
    return 0;
  return scope->GroupSizeFor(this);
}

inline void HTMLInputElement::AddToRadioButtonGroup() {
  if (RadioButtonGroupScope* scope = GetRadioButtonGroupScope())
    scope->AddButton(this);
}

inline void HTMLInputElement::RemoveFromRadioButtonGroup() {
  if (RadioButtonGroupScope* scope = GetRadioButtonGroupScope())
    scope->RemoveButton(this);
}

unsigned HTMLInputElement::height() const {
  return input_type_->Height();
}

unsigned HTMLInputElement::width() const {
  return input_type_->Width();
}

void HTMLInputElement::setHeight(unsigned height) {
  SetUnsignedIntegralAttribute(html_names::kHeightAttr, height);
}

void HTMLInputElement::setWidth(unsigned width) {
  SetUnsignedIntegralAttribute(html_names::kWidthAttr, width);
}

ListAttributeTargetObserver::ListAttributeTargetObserver(
    const AtomicString& id,
    HTMLInputElement* element)
    : IdTargetObserver(element->GetTreeScope().EnsureIdTargetObserverRegistry(),
                       id),
      element_(element) {}

void ListAttributeTargetObserver::Trace(Visitor* visitor) const {
  visitor->Trace(element_);
  IdTargetObserver::Trace(visitor);
}

void ListAttributeTargetObserver::IdTargetChanged() {
  element_->ListAttributeTargetChanged();
}

void HTMLInputElement::setRangeText(const String& replacement,
                                    ExceptionState& exception_state) {
  if (!input_type_->SupportsSelectionAPI()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The input element's type ('" + input_type_->FormControlTypeAsString() +
            "') does not support selection.");
    return;
  }

  TextControlElement::setRangeText(replacement, exception_state);
}

void HTMLInputElement::setRangeText(const String& replacement,
                                    unsigned start,
                                    unsigned end,
                                    const V8SelectionMode& selection_mode,
                                    ExceptionState& exception_state) {
  if (!input_type_->SupportsSelectionAPI()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The input element's type ('" + input_type_->FormControlTypeAsString() +
            "') does not support selection.");
    return;
  }

  TextControlElement::setRangeText(replacement, start, end, selection_mode,
                                   exception_state);
}

bool HTMLInputElement::SetupDateTimeChooserParameters(
    DateTimeChooserParameters& parameters) {
  if (!GetDocument().View())
    return false;

  parameters.type = input_type_->type();
  parameters.minimum = Minimum();
  parameters.maximum = Maximum();
  parameters.required = IsRequired();
  if (!RuntimeEnabledFeatures::LangAttributeAwareFormControlUIEnabled()) {
    parameters.locale = DefaultLanguage();
  } else {
    AtomicString computed_locale = ComputeInheritedLanguage();
    parameters.locale =
        computed_locale.empty() ? DefaultLanguage() : computed_locale;
  }

  StepRange step_range = CreateStepRange(kRejectAny);
  if (step_range.HasStep()) {
    parameters.step = step_range.Step().ToDouble();
    parameters.step_base = step_range.StepBase().ToDouble();
  } else {
    parameters.step = 1.0;
    parameters.step_base = 0;
  }

  parameters.anchor_rect_in_screen =
      GetDocument().View()->FrameToScreen(PixelSnappedBoundingBox());
  parameters.double_value = input_type_->ValueAsDouble();
  parameters.focused_field_index = input_type_view_->FocusedFieldIndex();
  parameters.is_anchor_element_rtl =
      GetLayoutObject()
          ? input_type_view_->ComputedTextDirection() == TextDirection::kRtl
          : false;
  if (HTMLDataListElement* data_list = DataList()) {
    HTMLDataListOptionsCollection* options = data_list->options();
    for (unsigned i = 0; HTMLOptionElement* option = options->Item(i); ++i) {
      if (option->value().empty() || option->IsDisabledFormControl() ||
          !IsValidValue(option->value()))
        continue;
      auto suggestion = mojom::blink::DateTimeSuggestion::New();
      suggestion->value =
          input_type_->ParseToNumber(option->value(), Decimal::Nan())
              .ToDouble();
      if (std::isnan(suggestion->value))
        continue;
      suggestion->localized_value = LocalizeValue(option->value());
      suggestion->label =
          option->value() == option->label() ? String("") : option->label();
      parameters.suggestions.push_back(std::move(suggestion));
    }
  }
  return true;
}

bool HTMLInputElement::SupportsInputModeAttribute() const {
  return input_type_->SupportsInputModeAttribute();
}

void HTMLInputElement::CapsLockStateMayHaveChanged() {
  input_type_view_->CapsLockStateMayHaveChanged();
}

bool HTMLInputElement::ShouldDrawCapsLockIndicator() const {
  return input_type_view_->ShouldDrawCapsLockIndicator();
}

void HTMLInputElement::SetShouldRevealPassword(bool value) {
  if (!!should_reveal_password_ == value)
    return;
  should_reveal_password_ = value;
  if (HTMLElement* inner_editor = InnerEditorElement()) {
    // Update -webkit-text-security style.
    inner_editor->SetNeedsStyleRecalc(
        kLocalStyleChange,
        StyleChangeReasonForTracing::Create(style_change_reason::kControl));
  }
}

#if BUILDFLAG(IS_ANDROID)
bool HTMLInputElement::IsLastInputElementInForm() {
  DCHECK(GetDocument().GetPage());
  return !GetDocument()
              .GetPage()
              ->GetFocusController()
              .NextFocusableElementForImeAndAutofill(
                  this, mojom::blink::FocusType::kForward);
}

void HTMLInputElement::DispatchSimulatedEnter() {
  DCHECK(GetDocument().GetPage());
  GetDocument().GetPage()->GetFocusController().SetFocusedElement(
      this, GetDocument().GetFrame());

  EventDispatcher::DispatchSimulatedEnterEvent(*this);
}
#endif

bool HTMLInputElement::IsInteractiveContent() const {
  return input_type_->IsInteractiveContent();
}

void HTMLInputElement::AdjustStyle(ComputedStyleBuilder& builder) {
  return input_type_view_->AdjustStyle(builder);
}

void HTMLInputElement::DidNotifySubtreeInsertionsToDocument() {
  ListAttributeTargetChanged();
}

AXObject* HTMLInputElement::PopupRootAXObject() {
  return input_type_view_->PopupRootAXObject();
}

void HTMLInputElement::EnsureFallbackContent() {
  input_type_view_->EnsureFallbackContent();
}

void HTMLInputElement::EnsurePrimaryContent() {
  input_type_view_->EnsurePrimaryContent();
}

bool HTMLInputElement::HasFallbackContent() const {
  return input_type_view_->HasFallbackContent();
}

void HTMLInputElement::SetFilesFromPaths(const Vector<String>& paths) {
  return input_type_->SetFilesFromPaths(paths);
}

void HTMLInputElement::ChildrenChanged(const ChildrenChange& change) {
  // Some input types only need shadow roots to hide any children that may
  // have been appended by script. For such types, shadow roots are lazily
  // created when children are added for the first time. For the case of
  // `kFinishedBuildingDocumentFragmentTree` this function may be called
  // when the HTMLInputElement has no children.
  if (change.type !=
          ChildrenChangeType::kFinishedBuildingDocumentFragmentTree ||
      HasChildren()) {
    EnsureUserAgentShadowRoot();
  }
  ContainerNode::ChildrenChanged(change);
}

LayoutBox* HTMLInputElement::GetLayoutBoxForScrolling() const {
  // If it's LayoutTextControlSingleLine, return InnerEditorElement's LayoutBox.
  if (IsTextField() && InnerEditorElement())
    return InnerEditorElement()->GetLayoutBox();
  return Element::GetLayoutBoxForScrolling();
}

bool HTMLInputElement::IsDraggedSlider() const {
  return input_type_view_->IsDraggedSlider();
}

void HTMLInputElement::MaybeReportPiiMetrics() {
  // Don't report metrics if the field is empty.
  if (Value().empty())
    return;

  // Report the PII types derived from autofill field semantic type prediction.
  if (GetFormElementPiiType() != FormElementPiiType::kUnknown) {
    UseCounter::Count(GetDocument(),
                      WebFeature::kUserDataFieldFilled_PredictedTypeMatch);

    if (GetFormElementPiiType() == FormElementPiiType::kEmail) {
      UseCounter::Count(GetDocument(),
                        WebFeature::kEmailFieldFilled_PredictedTypeMatch);
    } else if (GetFormElementPiiType() == FormElementPiiType::kPhone) {
      UseCounter::Count(GetDocument(),
                        WebFeature::kPhoneFieldFilled_PredictedTypeMatch);
    }
  }

  // Report the PII types derived by matching the field value with patterns.

  // For Email, we add a length limitation (based on
  // https://www.rfc-editor.org/errata_search.php?rfc=3696) in addition to
  // matching with the pattern given by the HTML standard.
  if (Value().length() <= kMaxEmailFieldLength &&
      EmailInputType::IsValidEmailAddress(GetDocument().EnsureEmailRegexp(),
                                          Value())) {
    UseCounter::Count(GetDocument(),
                      WebFeature::kEmailFieldFilled_PatternMatch);
  }
}

// https://html.spec.whatwg.org/multipage/form-control-infrastructure.html#concept-fe-mutable
bool HTMLInputElement::isMutable() {
  return !IsDisabledFormControl() &&
         !(input_type_->SupportsReadOnly() && IsReadOnly());
}

// Show a browser picker for this input element.
// https://html.spec.whatwg.org/multipage/input.html#dom-input-showpicker
void HTMLInputElement::showPicker(ExceptionState& exception_state) {
  LocalFrame* frame = GetDocument().GetFrame();
  // In cross-origin iframes it should throw a "SecurityError" DOMException
  // except on file and color. In same-origin iframes it should work fine.
  // https://github.com/whatwg/html/issues/6909#issuecomment-917138991
  if (FormControlType() != FormControlType::kInputFile &&
      FormControlType() != FormControlType::kInputColor && frame) {
    if (!frame->IsSameOrigin()) {
      exception_state.ThrowSecurityError(
          "showPicker() called from cross-origin iframe.");
      return;
    }
  }

  if (!isMutable()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "HTMLInputElement::showPicker() cannot be used on immutable controls.");
    return;
  }

  if (!LocalFrame::HasTransientUserActivation(frame)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "HTMLInputElement::showPicker() requires a user gesture.");
    return;
  }
  LocalFrame::ConsumeTransientUserActivation(frame);

  input_type_view_->OpenPopupView();
}

bool HTMLInputElement::IsValidCommand(HTMLElement& invoker,
                                      CommandEventType command) {
  bool parent_is_valid = HTMLElement::IsValidCommand(invoker, command);
  if (!RuntimeEnabledFeatures::HTMLInvokeActionsV2Enabled() ||
      parent_is_valid) {
    return parent_is_valid;
  }

  if (input_type_->IsNumberInputType()) {
    if (command == CommandEventType::kStepUp ||
        command == CommandEventType::kStepDown) {
      return true;
    }
  }

  return command == CommandEventType::kShowPicker;
}

bool HTMLInputElement::HandleCommandInternal(HTMLElement& invoker,
                                             CommandEventType command) {
  CHECK(IsValidCommand(invoker, command));

  if (HTMLElement::HandleCommandInternal(invoker, command)) {
    return true;
  }

  // Step 1. If this is not mutable, then return.
  if (!isMutable()) {
    return false;
  }

  if (command == CommandEventType::kShowPicker) {
    // Step 2. If this's relevant settings object's origin is not same origin
    // with this's relevant settings object's top-level origin, [...], then
    // return.
    Document& document = GetDocument();
    LocalFrame* frame = document.GetFrame();
    if (frame && !frame->IsSameOrigin()) {
      String message = "Input cannot be invoked from cross-origin iframe.";
      document.AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
          mojom::ConsoleMessageSource::kJavaScript,
          mojom::ConsoleMessageLevel::kWarning, message));
      return false;
    }

    // If this's relevant global object does not have transient
    // activation, then return.
    if (!LocalFrame::HasTransientUserActivation(frame)) {
      String message = "Input cannot be invoked without a user gesture.";
      document.AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
          mojom::ConsoleMessageSource::kJavaScript,
          mojom::ConsoleMessageLevel::kWarning, message));
      return false;
    }

    // Step 3. ... show the picker, if applicable, for this.
    input_type_view_->OpenPopupView();
    return true;
  }

  if (input_type_->IsNumberInputType()) {
    if (command == CommandEventType::kStepUp) {
      input_type_->StepUp(1.0, ASSERT_NO_EXCEPTION);
      return true;
    }

    if (command == CommandEventType::kStepDown) {
      input_type_->StepUp(-1.0, ASSERT_NO_EXCEPTION);
      return true;
    }
  }

  return false;
}

void HTMLInputElement::SetFocused(bool is_focused,
                                  mojom::blink::FocusType focus_type) {
  TextControlElement::SetFocused(is_focused, focus_type);
  // Multifield inputs will call SetFocused when switching between the
  // individual parts, but we don't want to start matching
  // :user-valid/:user-invalid at that time. However, for other inputs, we want
  // to start matching :user-valid/:user-invalid as soon as possible, especially
  // to support the case where the user types something, then deletes it, then
  // blurs the input.
  if (!is_focused && !input_type_view_->IsMultipleFieldsTemporal() &&
      UserHasEditedTheField()) {
    SetUserHasEditedTheFieldAndBlurred();
  }
}

}  // namespace blink
