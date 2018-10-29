/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
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

#include "third_party/blink/renderer/core/html/forms/html_form_control_element.h"

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatch_forbidden_scope.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/html/forms/html_data_list_element.h"
#include "third_party/blink/renderer/core/html/forms/html_field_set_element.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_legend_element.h"
#include "third_party/blink/renderer/core/html/forms/validity_state.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/validation_message_client.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/text/bidi_text_run.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

using namespace HTMLNames;

HTMLFormControlElement::HTMLFormControlElement(const QualifiedName& tag_name,
                                               Document& document)
    : LabelableElement(tag_name, document),
      autofill_state_(WebAutofillState::kNotFilled),
      ancestor_disabled_state_(kAncestorDisabledStateUnknown),
      data_list_ancestor_state_(kUnknown),
      may_have_field_set_ancestor_(true),
      has_validation_message_(false),
      will_validate_initialized_(false),
      will_validate_(true),
      is_valid_(true),
      validity_is_dirty_(false),
      blocks_form_submission_(false) {
  SetHasCustomStyleCallbacks();
  static unsigned next_free_unique_id = 0;
  unique_renderer_form_control_id_ = next_free_unique_id++;
}

HTMLFormControlElement::~HTMLFormControlElement() = default;

void HTMLFormControlElement::Trace(blink::Visitor* visitor) {
  ListedElement::Trace(visitor);
  LabelableElement::Trace(visitor);
}

String HTMLFormControlElement::formAction() const {
  const AtomicString& action = FastGetAttribute(formactionAttr);
  if (action.IsEmpty())
    return GetDocument().Url();
  return GetDocument().CompleteURL(StripLeadingAndTrailingHTMLSpaces(action));
}

void HTMLFormControlElement::setFormAction(const AtomicString& value) {
  setAttribute(formactionAttr, value);
}

String HTMLFormControlElement::formEnctype() const {
  const AtomicString& form_enctype_attr = FastGetAttribute(formenctypeAttr);
  if (form_enctype_attr.IsNull())
    return g_empty_string;
  return FormSubmission::Attributes::ParseEncodingType(form_enctype_attr);
}

void HTMLFormControlElement::setFormEnctype(const AtomicString& value) {
  setAttribute(formenctypeAttr, value);
}

String HTMLFormControlElement::formMethod() const {
  const AtomicString& form_method_attr = FastGetAttribute(formmethodAttr);
  if (form_method_attr.IsNull())
    return g_empty_string;
  return FormSubmission::Attributes::MethodString(
      FormSubmission::Attributes::ParseMethodType(form_method_attr));
}

void HTMLFormControlElement::setFormMethod(const AtomicString& value) {
  setAttribute(formmethodAttr, value);
}

bool HTMLFormControlElement::FormNoValidate() const {
  return FastHasAttribute(formnovalidateAttr);
}

void HTMLFormControlElement::UpdateAncestorDisabledState() const {
  if (!may_have_field_set_ancestor_) {
    ancestor_disabled_state_ = kAncestorDisabledStateEnabled;
    return;
  }
  may_have_field_set_ancestor_ = false;
  // <fieldset> element of which |disabled| attribute affects |this| element.
  HTMLFieldSetElement* disabled_fieldset_ancestor = nullptr;
  ContainerNode* last_legend_ancestor = nullptr;
  for (HTMLElement* ancestor = Traversal<HTMLElement>::FirstAncestor(*this);
       ancestor; ancestor = Traversal<HTMLElement>::FirstAncestor(*ancestor)) {
    if (IsHTMLLegendElement(*ancestor))
      last_legend_ancestor = ancestor;
    if (IsHTMLFieldSetElement(*ancestor)) {
      may_have_field_set_ancestor_ = true;
      if (ancestor->IsDisabledFormControl()) {
        auto* fieldset = ToHTMLFieldSetElement(ancestor);
        if (last_legend_ancestor && last_legend_ancestor == fieldset->Legend())
          continue;
        disabled_fieldset_ancestor = fieldset;
        break;
      }
    }
  }
  ancestor_disabled_state_ = disabled_fieldset_ancestor
                                 ? kAncestorDisabledStateDisabled
                                 : kAncestorDisabledStateEnabled;
}

void HTMLFormControlElement::AncestorDisabledStateWasChanged() {
  ancestor_disabled_state_ = kAncestorDisabledStateUnknown;
  DisabledAttributeChanged();
}

void HTMLFormControlElement::Reset() {
  SetAutofillState(WebAutofillState::kNotFilled);
  ResetImpl();
}

void HTMLFormControlElement::AttributeChanged(
    const AttributeModificationParams& params) {
  HTMLElement::AttributeChanged(params);
  if (params.name == disabledAttr &&
      params.old_value.IsNull() != params.new_value.IsNull()) {
    DisabledAttributeChanged();
    if (params.reason == AttributeModificationReason::kDirectly &&
        IsDisabledFormControl() && AdjustedFocusedElementInTreeScope() == this)
      blur();
  }
}

void HTMLFormControlElement::ParseAttribute(
    const AttributeModificationParams& params) {
  const QualifiedName& name = params.name;
  if (name == formAttr) {
    FormAttributeChanged();
    UseCounter::Count(GetDocument(), WebFeature::kFormAttribute);
  } else if (name == readonlyAttr) {
    if (params.old_value.IsNull() != params.new_value.IsNull()) {
      SetNeedsWillValidateCheck();
      PseudoStateChanged(CSSSelector::kPseudoReadOnly);
      PseudoStateChanged(CSSSelector::kPseudoReadWrite);
      if (LayoutObject* o = GetLayoutObject())
        o->InvalidateIfControlStateChanged(kReadOnlyControlState);
    }
  } else if (name == requiredAttr) {
    if (params.old_value.IsNull() != params.new_value.IsNull())
      RequiredAttributeChanged();
    UseCounter::Count(GetDocument(), WebFeature::kRequiredAttribute);
  } else if (name == autofocusAttr) {
    HTMLElement::ParseAttribute(params);
    UseCounter::Count(GetDocument(), WebFeature::kAutoFocusAttribute);
  } else {
    HTMLElement::ParseAttribute(params);
  }
}

void HTMLFormControlElement::DisabledAttributeChanged() {
  // Don't blur in this function because this is called for descendants of
  // <fieldset> while tree traversal.
  EventDispatchForbiddenScope event_forbidden;

  SetNeedsWillValidateCheck();
  PseudoStateChanged(CSSSelector::kPseudoDisabled);
  PseudoStateChanged(CSSSelector::kPseudoEnabled);
  if (LayoutObject* o = GetLayoutObject())
    o->InvalidateIfControlStateChanged(kEnabledControlState);

  // TODO(dmazzoni): http://crbug.com/699438.
  // Replace |CheckedStateChanged| with a generic tree changed event.
  if (AXObjectCache* cache = GetDocument().ExistingAXObjectCache())
    cache->CheckedStateChanged(this);
}

void HTMLFormControlElement::RequiredAttributeChanged() {
  SetNeedsValidityCheck();
  PseudoStateChanged(CSSSelector::kPseudoRequired);
  PseudoStateChanged(CSSSelector::kPseudoOptional);
  // TODO(dmazzoni): http://crbug.com/699438.
  // Replace |CheckedStateChanged| with a generic tree changed event.
  if (AXObjectCache* cache = GetDocument().ExistingAXObjectCache())
    cache->CheckedStateChanged(this);
}

bool HTMLFormControlElement::IsReadOnly() const {
  return FastHasAttribute(HTMLNames::readonlyAttr);
}

bool HTMLFormControlElement::IsDisabledOrReadOnly() const {
  return IsDisabledFormControl() || IsReadOnly();
}

bool HTMLFormControlElement::SupportsAutofocus() const {
  return false;
}

bool HTMLFormControlElement::IsAutofocusable() const {
  return FastHasAttribute(autofocusAttr) && SupportsAutofocus();
}

void HTMLFormControlElement::SetAutofillState(WebAutofillState autofill_state) {
  if (autofill_state == autofill_state_)
    return;

  autofill_state_ = autofill_state;
  PseudoStateChanged(CSSSelector::kPseudoAutofill);
  PseudoStateChanged(CSSSelector::kPseudoAutofillSelected);
  PseudoStateChanged(CSSSelector::kPseudoAutofillPreviewed);
}

void HTMLFormControlElement::SetAutofillSection(const WebString& section) {
  autofill_section_ = section;
}

const AtomicString& HTMLFormControlElement::autocapitalize() const {
  if (!FastGetAttribute(autocapitalizeAttr).IsEmpty())
    return HTMLElement::autocapitalize();

  // If the form control itself does not have the autocapitalize attribute set,
  // but the form owner is non-null and does have the autocapitalize attribute
  // set, we inherit from the form owner.
  if (HTMLFormElement* form = Form())
    return form->autocapitalize();

  return g_empty_atom;
}

static bool ShouldAutofocusOnAttach(const HTMLFormControlElement* element) {
  if (!element->IsAutofocusable())
    return false;
  if (element->GetDocument().IsSandboxed(kSandboxAutomaticFeatures)) {
    // FIXME: This message should be moved off the console once a solution to
    // https://bugs.webkit.org/show_bug.cgi?id=103274 exists.
    element->GetDocument().AddConsoleMessage(ConsoleMessage::Create(
        kSecurityMessageSource, kErrorMessageLevel,
        "Blocked autofocusing on a form control because the form's frame is "
        "sandboxed and the 'allow-scripts' permission is not set."));
    return false;
  }

  return true;
}

void HTMLFormControlElement::AttachLayoutTree(AttachContext& context) {
  HTMLElement::AttachLayoutTree(context);

  if (!GetLayoutObject())
    return;

  // The call to updateFromElement() needs to go after the call through
  // to the base class's attachLayoutTree() because that can sometimes do a
  // close on the layoutObject.
  GetLayoutObject()->UpdateFromElement();

  // FIXME: Autofocus handling should be moved to insertedInto according to
  // the standard.
  if (ShouldAutofocusOnAttach(this))
    GetDocument().SetAutofocusElement(this);
}

void HTMLFormControlElement::DidMoveToNewDocument(Document& old_document) {
  ListedElement::DidMoveToNewDocument(old_document);
  HTMLElement::DidMoveToNewDocument(old_document);
}

Node::InsertionNotificationRequest HTMLFormControlElement::InsertedInto(
    ContainerNode& insertion_point) {
  ancestor_disabled_state_ = kAncestorDisabledStateUnknown;
  // Force traversal to find ancestor
  may_have_field_set_ancestor_ = true;
  data_list_ancestor_state_ = kUnknown;
  SetNeedsWillValidateCheck();
  HTMLElement::InsertedInto(insertion_point);
  ListedElement::InsertedInto(insertion_point);
  FieldSetAncestorsSetNeedsValidityCheck(&insertion_point);

  // Trigger for elements outside of forms.
  if (!formOwner() && insertion_point.isConnected())
    GetDocument().DidAssociateFormControl(this);

  return kInsertionDone;
}

void HTMLFormControlElement::RemovedFrom(ContainerNode& insertion_point) {
  FieldSetAncestorsSetNeedsValidityCheck(&insertion_point);
  HideVisibleValidationMessage();
  has_validation_message_ = false;
  ancestor_disabled_state_ = kAncestorDisabledStateUnknown;
  data_list_ancestor_state_ = kUnknown;
  SetNeedsWillValidateCheck();
  HTMLElement::RemovedFrom(insertion_point);
  ListedElement::RemovedFrom(insertion_point);
}

void HTMLFormControlElement::WillChangeForm() {
  ListedElement::WillChangeForm();
  FormOwnerSetNeedsValidityCheck();
  if (formOwner() && CanBeSuccessfulSubmitButton())
    formOwner()->InvalidateDefaultButtonStyle();
}

void HTMLFormControlElement::DidChangeForm() {
  ListedElement::DidChangeForm();
  FormOwnerSetNeedsValidityCheck();
  if (formOwner() && isConnected() && CanBeSuccessfulSubmitButton())
    formOwner()->InvalidateDefaultButtonStyle();
}

void HTMLFormControlElement::FormOwnerSetNeedsValidityCheck() {
  if (HTMLFormElement* form = formOwner()) {
    form->PseudoStateChanged(CSSSelector::kPseudoValid);
    form->PseudoStateChanged(CSSSelector::kPseudoInvalid);
  }
}

void HTMLFormControlElement::FieldSetAncestorsSetNeedsValidityCheck(
    Node* node) {
  if (!node)
    return;
  if (!may_have_field_set_ancestor_)
    return;
  for (HTMLFieldSetElement* field_set =
           Traversal<HTMLFieldSetElement>::FirstAncestorOrSelf(*node);
       field_set;
       field_set = Traversal<HTMLFieldSetElement>::FirstAncestor(*field_set)) {
    field_set->PseudoStateChanged(CSSSelector::kPseudoValid);
    field_set->PseudoStateChanged(CSSSelector::kPseudoInvalid);
  }
}

void HTMLFormControlElement::DispatchChangeEvent() {
  DispatchScopedEvent(*Event::CreateBubble(EventTypeNames::change));
}

HTMLFormElement* HTMLFormControlElement::formOwner() const {
  return ListedElement::Form();
}

bool HTMLFormControlElement::IsDisabledFormControl() const {
  if (FastHasAttribute(disabledAttr))
    return true;

  // Since the MHTML is loaded in sandboxing mode with form submission and
  // script execution disabled, we should gray out all form control elements
  // to indicate that the form cannot be worked on.
  if (GetDocument().Fetcher()->Archive())
    return true;

  if (ancestor_disabled_state_ == kAncestorDisabledStateUnknown)
    UpdateAncestorDisabledState();
  return ancestor_disabled_state_ == kAncestorDisabledStateDisabled;
}

bool HTMLFormControlElement::MatchesEnabledPseudoClass() const {
  return !IsDisabledFormControl();
}

bool HTMLFormControlElement::IsRequired() const {
  return FastHasAttribute(requiredAttr);
}

String HTMLFormControlElement::ResultForDialogSubmit() {
  return FastGetAttribute(valueAttr);
}

void HTMLFormControlElement::DidRecalcStyle(StyleRecalcChange) {
  if (LayoutObject* layout_object = GetLayoutObject())
    layout_object->UpdateFromElement();
}

bool HTMLFormControlElement::SupportsFocus() const {
  return !IsDisabledFormControl();
}

bool HTMLFormControlElement::IsKeyboardFocusable() const {
  // Skip tabIndex check in a parent class.
  return IsFocusable();
}

bool HTMLFormControlElement::MayTriggerVirtualKeyboard() const {
  return false;
}

bool HTMLFormControlElement::ShouldHaveFocusAppearance() const {
  return (GetDocument().LastFocusType() != kWebFocusTypeMouse) ||
         GetDocument().HadKeyboardEvent() || MayTriggerVirtualKeyboard();
}

int HTMLFormControlElement::tabIndex() const {
  // Skip the supportsFocus check in HTMLElement.
  return Element::tabIndex();
}

bool HTMLFormControlElement::RecalcWillValidate() const {
  if (data_list_ancestor_state_ == kUnknown) {
    if (Traversal<HTMLDataListElement>::FirstAncestor(*this))
      data_list_ancestor_state_ = kInsideDataList;
    else
      data_list_ancestor_state_ = kNotInsideDataList;
  }
  return data_list_ancestor_state_ == kNotInsideDataList &&
         !IsDisabledOrReadOnly();
}

bool HTMLFormControlElement::willValidate() const {
  if (!will_validate_initialized_ || data_list_ancestor_state_ == kUnknown) {
    const_cast<HTMLFormControlElement*>(this)->SetNeedsWillValidateCheck();
  } else {
    // If the following assertion fails, setNeedsWillValidateCheck() is not
    // called correctly when something which changes recalcWillValidate() result
    // is updated.
    DCHECK_EQ(will_validate_, RecalcWillValidate());
  }
  return will_validate_;
}

void HTMLFormControlElement::SetNeedsWillValidateCheck() {
  // We need to recalculate willValidate immediately because willValidate change
  // can causes style change.
  bool new_will_validate = RecalcWillValidate();
  if (will_validate_initialized_ && will_validate_ == new_will_validate)
    return;
  will_validate_initialized_ = true;
  will_validate_ = new_will_validate;
  // Needs to force SetNeedsValidityCheck() to invalidate validity state of
  // FORM/FIELDSET. If this element updates willValidate twice and
  // IsValidElement() is not called between them, the second call of this
  // function still has validity_is_dirty_==true, which means
  // SetNeedsValidityCheck() doesn't invalidate validity state of
  // FORM/FIELDSET.
  validity_is_dirty_ = false;
  SetNeedsValidityCheck();
  // No need to trigger style recalculation here because
  // SetNeedsValidityCheck() does it in the right away. This relies on
  // the assumption that Valid() is always true if willValidate() is false.

  if (!will_validate_)
    HideVisibleValidationMessage();
}

void HTMLFormControlElement::FindCustomValidationMessageTextDirection(
    const String& message,
    TextDirection& message_dir,
    String& sub_message,
    TextDirection& sub_message_dir) {
  message_dir = DetermineDirectionality(message);
  if (!sub_message.IsEmpty())
    sub_message_dir = GetLayoutObject()->Style()->Direction();
}

void HTMLFormControlElement::UpdateVisibleValidationMessage() {
  Page* page = GetDocument().GetPage();
  if (!page || !page->IsPageVisible() || GetDocument().UnloadStarted())
    return;
  if (page->Paused())
    return;
  String message;
  if (GetLayoutObject() && willValidate())
    message = validationMessage().StripWhiteSpace();

  has_validation_message_ = true;
  ValidationMessageClient* client = &page->GetValidationMessageClient();
  TextDirection message_dir = TextDirection::kLtr;
  TextDirection sub_message_dir = TextDirection::kLtr;
  String sub_message = ValidationSubMessage().StripWhiteSpace();
  if (message.IsEmpty()) {
    client->HideValidationMessage(*this);
  } else {
    FindCustomValidationMessageTextDirection(message, message_dir, sub_message,
                                             sub_message_dir);
  }
  client->ShowValidationMessage(*this, message, message_dir, sub_message,
                                sub_message_dir);
}

void HTMLFormControlElement::HideVisibleValidationMessage() {
  if (!has_validation_message_)
    return;

  if (ValidationMessageClient* client = GetValidationMessageClient())
    client->HideValidationMessage(*this);
}

bool HTMLFormControlElement::IsValidationMessageVisible() const {
  if (!has_validation_message_)
    return false;

  ValidationMessageClient* client = GetValidationMessageClient();
  if (!client)
    return false;

  return client->IsValidationMessageVisible(*this);
}

ValidationMessageClient* HTMLFormControlElement::GetValidationMessageClient()
    const {
  Page* page = GetDocument().GetPage();
  if (!page)
    return nullptr;

  return &page->GetValidationMessageClient();
}

bool HTMLFormControlElement::checkValidity(
    HeapVector<Member<HTMLFormControlElement>>* unhandled_invalid_controls,
    CheckValidityEventBehavior event_behavior) {
  if (!willValidate())
    return true;
  if (IsValidElement())
    return true;
  if (event_behavior != kCheckValidityDispatchInvalidEvent)
    return false;
  Document* original_document = &GetDocument();
  DispatchEventResult dispatch_result =
      DispatchEvent(*Event::CreateCancelable(EventTypeNames::invalid));
  if (dispatch_result == DispatchEventResult::kNotCanceled &&
      unhandled_invalid_controls && isConnected() &&
      original_document == GetDocument())
    unhandled_invalid_controls->push_back(this);
  return false;
}

void HTMLFormControlElement::ShowValidationMessage() {
  scrollIntoViewIfNeeded(false);
  focus();
  UpdateVisibleValidationMessage();
}

bool HTMLFormControlElement::reportValidity() {
  HeapVector<Member<HTMLFormControlElement>> unhandled_invalid_controls;
  bool is_valid = checkValidity(&unhandled_invalid_controls,
                                kCheckValidityDispatchInvalidEvent);
  if (is_valid || unhandled_invalid_controls.IsEmpty())
    return is_valid;
  DCHECK_EQ(unhandled_invalid_controls.size(), 1u);
  DCHECK_EQ(unhandled_invalid_controls[0].Get(), this);
  // Update layout now before calling isFocusable(), which has
  // !layoutObject()->needsLayout() assertion.
  GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();
  if (IsFocusable()) {
    ShowValidationMessage();
    return false;
  }
  if (GetDocument().GetFrame()) {
    String message(
        "An invalid form control with name='%name' is not focusable.");
    message.Replace("%name", GetName());
    GetDocument().AddConsoleMessage(ConsoleMessage::Create(
        kRenderingMessageSource, kErrorMessageLevel, message));
  }
  return false;
}

bool HTMLFormControlElement::MatchesValidityPseudoClasses() const {
  return willValidate();
}

bool HTMLFormControlElement::IsValidElement() {
  if (validity_is_dirty_) {
    is_valid_ = !willValidate() || Valid();
    validity_is_dirty_ = false;
  } else {
    // If the following assertion fails, setNeedsValidityCheck() is not
    // called correctly when something which changes validity is updated.
    DCHECK_EQ(is_valid_, (!willValidate() || Valid()));
  }
  return is_valid_;
}

void HTMLFormControlElement::SetNeedsValidityCheck() {
  if (!validity_is_dirty_) {
    validity_is_dirty_ = true;
    FormOwnerSetNeedsValidityCheck();
    FieldSetAncestorsSetNeedsValidityCheck(parentNode());
    PseudoStateChanged(CSSSelector::kPseudoValid);
    PseudoStateChanged(CSSSelector::kPseudoInvalid);
  }

  // Updates only if this control already has a validation message.
  if (IsValidationMessageVisible()) {
    // Calls UpdateVisibleValidationMessage() even if is_valid_ is not
    // changed because a validation message can be changed.
    GetDocument()
        .GetTaskRunner(TaskType::kDOMManipulation)
        ->PostTask(
            FROM_HERE,
            WTF::Bind(&HTMLFormControlElement::UpdateVisibleValidationMessage,
                      WrapPersistent(this)));
  }
}

void HTMLFormControlElement::setCustomValidity(const String& error) {
  ListedElement::setCustomValidity(error);
  SetNeedsValidityCheck();
}

void HTMLFormControlElement::DispatchBlurEvent(
    Element* new_focused_element,
    WebFocusType type,
    InputDeviceCapabilities* source_capabilities) {
  HTMLElement::DispatchBlurEvent(new_focused_element, type,
                                 source_capabilities);
  HideVisibleValidationMessage();
}

bool HTMLFormControlElement::IsSuccessfulSubmitButton() const {
  return CanBeSuccessfulSubmitButton() && !IsDisabledFormControl();
}

// static
const HTMLFormControlElement*
HTMLFormControlElement::EnclosingFormControlElement(const Node* node) {
  if (!node)
    return nullptr;
  return Traversal<HTMLFormControlElement>::FirstAncestorOrSelf(*node);
}

String HTMLFormControlElement::NameForAutofill() const {
  String full_name = GetName();
  String trimmed_name = full_name.StripWhiteSpace();
  if (!trimmed_name.IsEmpty())
    return trimmed_name;
  full_name = GetIdAttribute();
  trimmed_name = full_name.StripWhiteSpace();
  return trimmed_name;
}

void HTMLFormControlElement::CloneNonAttributePropertiesFrom(
    const Element& source,
    CloneChildrenFlag flag) {
  HTMLElement::CloneNonAttributePropertiesFrom(source, flag);
  SetNeedsValidityCheck();
}

void HTMLFormControlElement::AssociateWith(HTMLFormElement* form) {
  AssociateByParser(form);
};

}  // namespace blink
