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

#include "third_party/blink/renderer/core/html/forms/listed_element.h"

#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/dom/id_target_observer.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/html/custom/element_internals.h"
#include "third_party/blink/renderer/core/html/forms/form_controller.h"
#include "third_party/blink/renderer/core/html/forms/html_button_element.h"
#include "third_party/blink/renderer/core/html/forms/html_data_list_element.h"
#include "third_party/blink/renderer/core/html/forms/html_field_set_element.h"
#include "third_party/blink/renderer/core/html/forms/html_form_control_element.h"
#include "third_party/blink/renderer/core/html/forms/html_form_control_element_with_state.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/forms/html_legend_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/forms/validity_state.h"
#include "third_party/blink/renderer/core/html/html_object_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/validation_message_client.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/text/bidi_paragraph.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

void InvalidateAncestorFormsForAutofill(ContainerNode& insertion_point) {
  // Let any forms in the shadow including ancestors know that this
  // ListedElement has changed.
  ContainerNode* starting_node = &insertion_point;
  for (ContainerNode* parent = starting_node; parent;
       parent = parent->ParentOrShadowHostNode()) {
    if (HTMLFormElement* form = DynamicTo<HTMLFormElement>(parent)) {
      form->InvalidateListedElementsForAutofill();
    }
  }
}

}  // namespace

class FormAttributeTargetObserver : public IdTargetObserver {
 public:
  FormAttributeTargetObserver(const AtomicString& id, ListedElement*);

  void Trace(Visitor*) const override;
  void IdTargetChanged() override;

 private:
  Member<ListedElement> element_;
};

ListedElement::ListedElement()
    : has_validation_message_(false),
      form_was_set_by_parser_(false),
      will_validate_initialized_(false),
      will_validate_(true),
      is_valid_(true),
      validity_is_dirty_(false),
      is_element_disabled_(false),
      is_readonly_(false) {}

ListedElement::~ListedElement() {
  // We can't call setForm here because it contains virtual calls.
}

void ListedElement::Trace(Visitor* visitor) const {
  visitor->Trace(form_attribute_target_observer_);
  visitor->Trace(form_);
  visitor->Trace(validity_state_);
}

ValidityState* ListedElement::validity() {
  if (!validity_state_)
    validity_state_ = MakeGarbageCollected<ValidityState>(this);

  return validity_state_.Get();
}

void ListedElement::DidMoveToNewDocument(Document& old_document) {
  if (ToHTMLElement().FastHasAttribute(html_names::kFormAttr))
    SetFormAttributeTargetObserver(nullptr);
}

void ListedElement::InsertedInto(ContainerNode& insertion_point) {
  ancestor_disabled_state_ = AncestorDisabledState::kUnknown;
  // Force traversal to find ancestor
  may_have_fieldset_ancestor_ = true;
  data_list_ancestor_state_ = DataListAncestorState::kUnknown;
  UpdateWillValidateCache(WillValidateReason::kForInsertionOrRemoval);

  if (!form_was_set_by_parser_ || !form_ ||
      NodeTraversal::HighestAncestorOrSelf(insertion_point) !=
          NodeTraversal::HighestAncestorOrSelf(*form_.Get()))
    ResetFormOwner();

  HTMLElement& element = ToHTMLElement();
  if (insertion_point.isConnected()) {
    if (element.FastHasAttribute(html_names::kFormAttr))
      ResetFormAttributeTargetObserver();
  }

  FieldSetAncestorsSetNeedsValidityCheck(&insertion_point,
                                         StartingNodeType::IS_INSERTION_POINT);
  DisabledStateMightBeChanged();

  if (ClassSupportsStateRestore() && insertion_point.isConnected() &&
      !element.ContainingShadowRoot()) {
    element.GetDocument()
        .GetFormController()
        .InvalidateStatefulFormControlList();
  }

  // Trigger for elements outside of forms.
  if (!form_ && insertion_point.isConnected()) {
    element.GetDocument().DidChangeFormRelatedElementDynamically(
        &element, WebFormRelatedChangeType::kAdd);
  }

  InvalidateAncestorFormsForAutofill(insertion_point);
}

void ListedElement::RemovedFrom(ContainerNode& insertion_point) {
  FieldSetAncestorsSetNeedsValidityCheck(&insertion_point,
                                         StartingNodeType::IS_INSERTION_POINT);
  HideVisibleValidationMessage();
  has_validation_message_ = false;
  // Two values that might change as a result of being removed are
  // `ancestor_disabled_state_` and `data_list_ancestor_state_`. Both of
  // these values feed into the WillValidate cache. If this ListedElement is
  // not in a fieldset and not in a data-list, then it won't be in a fieldset
  // or fieldset after the removal, so that the cache does not need to be
  // updated.
  if (ancestor_disabled_state_ == AncestorDisabledState::kEnabled &&
      data_list_ancestor_state_ == DataListAncestorState::kNotInsideDataList) {
    DCHECK_EQ(will_validate_, RecalcWillValidate());
  } else {
    ancestor_disabled_state_ = AncestorDisabledState::kUnknown;
    data_list_ancestor_state_ = DataListAncestorState::kUnknown;
    UpdateWillValidateCache(WillValidateReason::kForInsertionOrRemoval);
  }

  HTMLElement& element = ToHTMLElement();
  if (insertion_point.isConnected() &&
      element.FastHasAttribute(html_names::kFormAttr)) {
    SetFormAttributeTargetObserver(nullptr);
    ResetFormOwner();
  } else if (!form_ && insertion_point.isConnected()) {
    // If there is no associated form, then there won't be one after removing,
    // so don't need to call ResetFormOwner(). While this doesn't need to call
    // ResetFormOwner(), it needs to call SetForm() to ensure Document level
    // state is updated.
    form_was_set_by_parser_ = false;
    SetForm(nullptr);
  } else if (form_ && NodeTraversal::HighestAncestorOrSelf(element) !=
                          NodeTraversal::HighestAncestorOrSelf(*form_.Get())) {
    // If the form and element are both in the same tree, preserve the
    // connection to the form.  Otherwise, null out our form and remove
    // ourselves from the form's list of elements.
    ResetFormOwner();
  }

  DisabledStateMightBeChanged();

  if (ClassSupportsStateRestore() && insertion_point.isConnected() &&
      !element.ContainingShadowRoot() &&
      !insertion_point.ContainingShadowRoot()) {
    element.GetDocument()
        .GetFormController()
        .InvalidateStatefulFormControlList();
  }

  InvalidateAncestorFormsForAutofill(insertion_point);

  if (insertion_point.isConnected()) {
    // We don't insist on form_ being non-null as the form does not take care of
    // reporting the removal.
    element.GetDocument().DidChangeFormRelatedElementDynamically(
        &element, WebFormRelatedChangeType::kRemove);
  }
}

void ListedElement::FormRemovedFromTree(const Node& form_root) {
  DCHECK(form_);
  if (NodeTraversal::HighestAncestorOrSelf(ToHTMLElement()) == form_root)
    return;
  ResetFormOwner();
}

void ListedElement::AssociateByParser(HTMLFormElement* form) {
  if (form && form->isConnected()) {
    form_was_set_by_parser_ = true;
    SetForm(form);
    form->DidAssociateByParser();
  }
}

void ListedElement::SetForm(HTMLFormElement* new_form) {
  if (!form_ || !new_form) {
    // Element was unassociated, or is becoming unassociated.
    ToHTMLElement().GetDocument().MarkUnassociatedListedElementsDirty();
  }
  if (form_.Get() == new_form)
    return;
  WillChangeForm();
  if (form_)
    form_->Disassociate(*this);
  if (new_form) {
    form_ = new_form;
    form_->Associate(*this);
  } else {
    form_ = nullptr;
  }
  DidChangeForm();
}

void ListedElement::WillChangeForm() {
  FormOwnerSetNeedsValidityCheck();
}

void ListedElement::DidChangeForm() {
  if (!form_was_set_by_parser_ && form_ && form_->isConnected()) {
    auto& element = ToHTMLElement();
    element.GetDocument().DidChangeFormRelatedElementDynamically(
        &element, WebFormRelatedChangeType::kReassociate);
  }
  FormOwnerSetNeedsValidityCheck();
}

void ListedElement::FormOwnerSetNeedsValidityCheck() {
  if (HTMLFormElement* form = Form()) {
    form->PseudoStateChanged(CSSSelector::kPseudoValid);
    form->PseudoStateChanged(CSSSelector::kPseudoInvalid);
    form->PseudoStateChanged(CSSSelector::kPseudoUserValid);
    form->PseudoStateChanged(CSSSelector::kPseudoUserInvalid);
  }
}

void ListedElement::FieldSetAncestorsSetNeedsValidityCheck(
    Node* node,
    StartingNodeType starting_type) {
  if (!node)
    return;
  if (!may_have_fieldset_ancestor_)
    return;
  auto* field_set = Traversal<HTMLFieldSetElement>::FirstAncestorOrSelf(*node);
  if (!field_set) {
    if (starting_type == StartingNodeType::IS_PARENT) {
      may_have_fieldset_ancestor_ = false;
    }
    return;
  }
  do {
    field_set->PseudoStateChanged(CSSSelector::kPseudoValid);
    field_set->PseudoStateChanged(CSSSelector::kPseudoInvalid);
    field_set->PseudoStateChanged(CSSSelector::kPseudoUserValid);
    field_set->PseudoStateChanged(CSSSelector::kPseudoUserInvalid);
  } while (
      (field_set = Traversal<HTMLFieldSetElement>::FirstAncestor(*field_set)));
}

HTMLElement* ListedElement::RetargetedForm() const {
  auto* form = Form();
  if (!form) {
    return nullptr;
  }
  const HTMLElement& element = ToHTMLElement();
  if (RuntimeEnabledFeatures::ShadowRootReferenceTargetEnabled(
          element.GetDocument().GetExecutionContext())) {
    // Retarget to avoid exposing reference target elements.
    return DynamicTo<HTMLElement>(&element.GetTreeScope().Retarget(*form));
  }
  return form;
}

// https://html.spec.whatwg.org/multipage/C#reset-the-form-owner
void ListedElement::ResetFormOwner() {
  // 1. Unset element's parser inserted flag.
  form_was_set_by_parser_ = false;
  HTMLElement& element = ToHTMLElement();
  const AtomicString& form_id(element.FastGetAttribute(html_names::kFormAttr));
  HTMLFormElement* nearest_form = element.FindFormAncestor();
  // 2. If all of the following are true:
  //    - element's form owner is not null;
  //    - element is not listed or its form content attribute is not present;
  //      and
  //    - element's form owner is its nearest form element ancestor after the
  //      change to the ancestor chain,
  // then return.
  if (form_ && form_id.IsNull() && form_.Get() == nearest_form)
    return;

  // 3. Set element's form owner to null.
  // 4. If element is listed, has a form content attribute, and is connected,
  //    then:
  //    1. If the first element in element's tree, in tree order, to have an
  //       ID that is identical to element's form content attribute's value,
  //       is a form element, then associate the element with that form
  //       element.
  HTMLFormElement* new_form = nullptr;
  if (!form_id.IsNull() && element.isConnected()) {
    Element* new_form_candidate =
        element.GetTreeScope().getElementById(form_id);
    new_form = DynamicTo<HTMLFormElement>(new_form_candidate);

    if (RuntimeEnabledFeatures::ShadowRootReferenceTargetEnabled(
            element.GetDocument().GetExecutionContext()) &&
        new_form_candidate) {
      new_form = DynamicTo<HTMLFormElement>(
          new_form_candidate->GetShadowReferenceTargetOrSelf(
              html_names::kFormAttr));
    }
  } else {
    // 5. Otherwise, if element has an ancestor form element, then associate
    //    element with the nearest such ancestor form element.
    new_form = nearest_form;
  }

  SetForm(new_form);
}

void ListedElement::FormAttributeChanged() {
  ResetFormOwner();
  ResetFormAttributeTargetObserver();
}

bool ListedElement::RecalcWillValidate() const {
  const HTMLElement& element = ToHTMLElement();
  if (data_list_ancestor_state_ == DataListAncestorState::kUnknown) {
    if (element.GetDocument().HasAtLeastOneDataList() &&
        Traversal<HTMLDataListElement>::FirstAncestor(element)) {
      data_list_ancestor_state_ = DataListAncestorState::kInsideDataList;
    } else {
      data_list_ancestor_state_ = DataListAncestorState::kNotInsideDataList;
    }
  }
  return data_list_ancestor_state_ ==
             DataListAncestorState::kNotInsideDataList &&
         !element.IsDisabledFormControl() && !is_readonly_;
}

bool ListedElement::WillValidate() const {
  if (!will_validate_initialized_ ||
      data_list_ancestor_state_ == DataListAncestorState::kUnknown) {
    const_cast<ListedElement*>(this)->UpdateWillValidateCache();
  } else {
    // If the following assertion fails, UpdateWillValidateCache() is not
    // called correctly when something which changes RecalcWillValidate() result
    // is updated.
    DCHECK_EQ(will_validate_, RecalcWillValidate());
  }
  return will_validate_;
}

void ListedElement::UpdateWillValidateCache(WillValidateReason reason) {
  // We need to recalculate willValidate immediately because willValidate change
  // can causes style change.
  bool new_will_validate = RecalcWillValidate();
  if (will_validate_initialized_ && will_validate_ == new_will_validate)
    return;
  will_validate_initialized_ = true;
  will_validate_ = new_will_validate;

  if (reason != WillValidateReason::kForInsertionOrRemoval) {
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

    if (!will_validate_) {
      HideVisibleValidationMessage();
    }
  } else {
    // We don't need to do any of the work above for insertion or removal,
    // because:
    //
    // * We don't need to notify that pseudo-states on this element have
    //   changed because it wasn't previously in the tree (or won't be in the
    //   tree shortly).
    // * FormOwnerSetNeedsValidityCheck is also called when changing the form
    // * FieldSetAncestorsSetNeedsValidityCheck is also called on insertion
    //   and removal
    // * RemovedFrom already hides the validation message, so we don't need to
    //   update or hide it.
    validity_is_dirty_ = true;
  }
}

bool ListedElement::CustomError() const {
  return !custom_validation_message_.empty();
}

bool ListedElement::HasBadInput() const {
  return false;
}

bool ListedElement::PatternMismatch() const {
  return false;
}

bool ListedElement::RangeOverflow() const {
  return false;
}

bool ListedElement::RangeUnderflow() const {
  return false;
}

bool ListedElement::StepMismatch() const {
  return false;
}

bool ListedElement::TooLong() const {
  return false;
}

bool ListedElement::TooShort() const {
  return false;
}

bool ListedElement::TypeMismatch() const {
  return false;
}

bool ListedElement::Valid() const {
  bool some_error = TypeMismatch() || StepMismatch() || RangeUnderflow() ||
                    RangeOverflow() || TooLong() || TooShort() ||
                    PatternMismatch() || ValueMissing() || HasBadInput() ||
                    CustomError();
  return !some_error;
}

bool ListedElement::ValueMissing() const {
  return false;
}

String ListedElement::CustomValidationMessage() const {
  return custom_validation_message_;
}

void ListedElement::SetCustomValidationMessage(const String& message) {
  // \r\n and \r should be replaced with \n:
  // https://github.com/whatwg/html/pull/10350.
  String message_copy(message);
  custom_validation_message_ =
      message_copy.Replace("\r\n", "\n").Replace('\r', '\n');
}

String ListedElement::validationMessage() const {
  return ToHTMLElement().willValidate() && CustomError()
             ? custom_validation_message_
             : String();
}

String ListedElement::ValidationSubMessage() const {
  return String();
}

void ListedElement::setCustomValidity(const String& error) {
  SetCustomValidationMessage(error);
  SetNeedsValidityCheck();
}

void ListedElement::FindCustomValidationMessageTextDirection(
    const String& message,
    TextDirection& message_dir,
    String& sub_message,
    TextDirection& sub_message_dir) {
  message_dir = BidiParagraph::BaseDirectionForStringOrLtr(message);
  if (!sub_message.empty()) {
    sub_message_dir = ToHTMLElement().GetLayoutObject()->Style()->Direction();
  }
}

void ListedElement::UpdateVisibleValidationMessage() {
  Element& element = ValidationAnchor();
  Page* page = element.GetDocument().GetPage();
  if (!page || !page->IsPageVisible() || element.GetDocument().UnloadStarted())
    return;
  if (page->Paused())
    return;
  String message;
  if (element.GetLayoutObject() && WillValidate() &&
      ToHTMLElement().IsShadowIncludingInclusiveAncestorOf(element))
    message = validationMessage().StripWhiteSpace();

  has_validation_message_ = true;
  ValidationMessageClient* client = &page->GetValidationMessageClient();
  TextDirection message_dir = TextDirection::kLtr;
  TextDirection sub_message_dir = TextDirection::kLtr;
  String sub_message = ValidationSubMessage().StripWhiteSpace();
  if (message.empty()) {
    client->HideValidationMessage(element);
  } else {
    FindCustomValidationMessageTextDirection(message, message_dir, sub_message,
                                             sub_message_dir);
  }
  client->ShowValidationMessage(element, message, message_dir, sub_message,
                                sub_message_dir);
}

void ListedElement::HideVisibleValidationMessage() {
  if (!has_validation_message_)
    return;

  if (auto* client = GetValidationMessageClient())
    client->HideValidationMessage(ValidationAnchor());
}

bool ListedElement::IsValidationMessageVisible() const {
  if (!has_validation_message_)
    return false;

  if (auto* client = GetValidationMessageClient()) {
    return client->IsValidationMessageVisible(ValidationAnchor());
  }
  return false;
}

ValidationMessageClient* ListedElement::GetValidationMessageClient() const {
  if (Page* page = ToHTMLElement().GetDocument().GetPage())
    return &page->GetValidationMessageClient();
  return nullptr;
}

Element& ListedElement::ValidationAnchor() const {
  return const_cast<HTMLElement&>(ToHTMLElement());
}

Element& ListedElement::GetHostOrFocusDelegate() const {
  const HTMLElement& host = ToHTMLElement();
  // If host is a shadow host with delegatesFocus, then the element to get
  // focus should be its focusable area.
  if (host.IsShadowHostWithDelegatesFocus()) {
    if (Element* focusable_area =
            host.GetFocusableArea(/*in_descendant_traversal=*/true)) {
      return *focusable_area;
    }
  }
  return const_cast<HTMLElement&>(host);
}

bool ListedElement::ValidationAnchorOrHostIsFocusable() const {
  const Element& anchor = ValidationAnchor();
  if (anchor.IsFocusable())
    return true;
  const Element& host = GetHostOrFocusDelegate();
  if (&anchor == &host)
    return false;
  return host.IsFocusable();
}

bool ListedElement::checkValidity(List* unhandled_invalid_controls) {
  if (IsNotCandidateOrValid())
    return true;
  HTMLElement& element = ToHTMLElement();
  Document* original_document = &element.GetDocument();
  DispatchEventResult dispatch_result = element.DispatchEvent(
      *Event::CreateCancelable(event_type_names::kInvalid));
  if (dispatch_result == DispatchEventResult::kNotCanceled &&
      unhandled_invalid_controls && element.isConnected() &&
      original_document == element.GetDocument())
    unhandled_invalid_controls->push_back(this);
  return false;
}

void ListedElement::ShowValidationMessage() {
  Element& element = ValidationAnchor();
  element.scrollIntoViewIfNeeded(false);
  if (element.IsFocusable()) {
    element.Focus();
  } else {
    Element& host = GetHostOrFocusDelegate();
    host.Focus();
  }
  UpdateVisibleValidationMessage();
}

bool ListedElement::reportValidity() {
  List unhandled_invalid_controls;
  bool is_valid = checkValidity(&unhandled_invalid_controls);
  if (is_valid || unhandled_invalid_controls.empty())
    return is_valid;
  DCHECK_EQ(unhandled_invalid_controls.size(), 1u);
  DCHECK_EQ(unhandled_invalid_controls[0].Get(), this);
  ShowValidationMessage();
  return false;
}

bool ListedElement::IsValidElement() {
  if (validity_is_dirty_) {
    is_valid_ = !WillValidate() || Valid();
    validity_is_dirty_ = false;
  } else {
    // If the following assertion fails, SetNeedsValidityCheck() is not
    // called correctly when something which changes validity is updated.
    DCHECK_EQ(is_valid_, (!WillValidate() || Valid()));
  }
  return is_valid_;
}

bool ListedElement::IsNotCandidateOrValid() {
  // Apply Element::willValidate(), not ListedElement::WillValidate(), because
  // some elements override willValidate().
  return !ToHTMLElement().willValidate() || IsValidElement();
}

void ListedElement::SetNeedsValidityCheck() {
  HTMLElement& element = ToHTMLElement();
  if (!validity_is_dirty_) {
    validity_is_dirty_ = true;
    FormOwnerSetNeedsValidityCheck();
    FieldSetAncestorsSetNeedsValidityCheck(element.parentNode(),
                                           StartingNodeType::IS_PARENT);
    element.PseudoStateChanged(CSSSelector::kPseudoValid);
    element.PseudoStateChanged(CSSSelector::kPseudoInvalid);
    element.PseudoStateChanged(CSSSelector::kPseudoUserValid);
    element.PseudoStateChanged(CSSSelector::kPseudoUserInvalid);
  }

  // Updates only if this control already has a validation message.
  if (IsValidationMessageVisible()) {
    // Calls UpdateVisibleValidationMessage() even if is_valid_ is not
    // changed because a validation message can be changed.
    element.GetDocument()
        .GetTaskRunner(TaskType::kDOMManipulation)
        ->PostTask(FROM_HERE,
                   BindOnce(&ListedElement::UpdateVisibleValidationMessage,
                            WrapPersistent(this)));
  }
}

void ListedElement::DisabledAttributeChanged() {
  HTMLElement& element = ToHTMLElement();
  is_element_disabled_ = element.FastHasAttribute(html_names::kDisabledAttr);
  UpdateWillValidateCache();
  element.PseudoStateChanged(CSSSelector::kPseudoDisabled);
  element.PseudoStateChanged(CSSSelector::kPseudoEnabled);
  DisabledStateMightBeChanged();
}

void ListedElement::ReadonlyAttributeChanged() {
  is_readonly_ = ToHTMLElement().FastHasAttribute(html_names::kReadonlyAttr);
  UpdateWillValidateCache();
}

void ListedElement::UpdateAncestorDisabledState() const {
  ancestor_disabled_state_ = AncestorDisabledState::kEnabled;
  const HTMLElement& element = ToHTMLElement();
  if (may_have_fieldset_ancestor_ &&
      element.GetDocument().HasAtLeastOneDisabledFieldset()) {
    may_have_fieldset_ancestor_ = false;
    ContainerNode* last_legend_ancestor = nullptr;
    for (auto* ancestor = Traversal<HTMLElement>::FirstAncestor(element);
         ancestor;
         ancestor = Traversal<HTMLElement>::FirstAncestor(*ancestor)) {
      if (IsA<HTMLLegendElement>(*ancestor)) {
        last_legend_ancestor = ancestor;
        continue;
      }
      if (HTMLFieldSetElement* fieldset_ancestor =
              DynamicTo<HTMLFieldSetElement>(ancestor)) {
        may_have_fieldset_ancestor_ = true;
        if (fieldset_ancestor->is_element_disabled_) {
          if (last_legend_ancestor &&
              last_legend_ancestor == fieldset_ancestor->Legend()) {
            continue;
          }
          ancestor_disabled_state_ = AncestorDisabledState::kDisabled;
          break;
        }
      }
    }
  }
}

void ListedElement::AncestorDisabledStateWasChanged() {
  ancestor_disabled_state_ = AncestorDisabledState::kUnknown;
  DisabledAttributeChanged();
}

bool ListedElement::IsActuallyDisabled() const {
  if (is_element_disabled_)
    return true;
  if (ancestor_disabled_state_ == AncestorDisabledState::kUnknown)
    UpdateAncestorDisabledState();
  return ancestor_disabled_state_ == AncestorDisabledState::kDisabled;
}

bool ListedElement::ClassSupportsStateRestore() const {
  return false;
}

bool ListedElement::ShouldSaveAndRestoreFormControlState() const {
  return false;
}

FormControlState ListedElement::SaveFormControlState() const {
  return FormControlState();
}

void ListedElement::RestoreFormControlState(const FormControlState& state) {}

void ListedElement::NotifyFormStateChanged() {
  Document& doc = ToHTMLElement().GetDocument();
  // This can be called during fragment parsing as a result of option
  // selection before the document is active (or even in a frame).
  if (!doc.IsActive())
    return;
  doc.GetFrame()->Client()->DidUpdateCurrentHistoryItem();
}

void ListedElement::TakeStateAndRestore() {
  if (ClassSupportsStateRestore()) {
    ToHTMLElement().GetDocument().GetFormController().RestoreControlStateFor(
        *this);
  }
}

HTMLFormElement* ListedElement::GetOwningFormForAutofill() const {
  // The owning form is the furthest ancestor form element, if there is one.
  HTMLFormElement* owner = nullptr;
  // Look for ancestors of the associated form of this element inside the same
  // tree.
  for (Node* ancestor = Form(); ancestor; ancestor = ancestor->parentNode()) {
    if (auto* form = DynamicTo<HTMLFormElement>(ancestor)) {
      owner = form;
    }
  }

  // If this element is inside Shadow DOM, also consider ancestors of this
  // element.
  for (Node* ancestor = ToHTMLElement().OwnerShadowHost(); ancestor;
       ancestor = ancestor->ParentOrShadowHostNode()) {
    if (auto* form = DynamicTo<HTMLFormElement>(ancestor)) {
      owner = form;
    }
  }
  return owner;
}

void ListedElement::SetFormAttributeTargetObserver(
    FormAttributeTargetObserver* new_observer) {
  if (form_attribute_target_observer_)
    form_attribute_target_observer_->Unregister();
  form_attribute_target_observer_ = new_observer;
}

void ListedElement::ResetFormAttributeTargetObserver() {
  HTMLElement& element = ToHTMLElement();
  const AtomicString& form_id(element.FastGetAttribute(html_names::kFormAttr));
  if (!form_id.IsNull() && element.isConnected()) {
    SetFormAttributeTargetObserver(
        MakeGarbageCollected<FormAttributeTargetObserver>(form_id, this));
  } else {
    SetFormAttributeTargetObserver(nullptr);
  }
}

void ListedElement::FormAttributeTargetChanged() {
  ResetFormOwner();
}

const AtomicString& ListedElement::GetName() const {
  const AtomicString& name = ToHTMLElement().GetNameAttribute();
  return name.IsNull() ? g_empty_atom : name;
}

bool ListedElement::IsFormControlElement() const {
  return false;
}

bool ListedElement::IsFormControlElementWithState() const {
  return false;
}

bool ListedElement::IsElementInternals() const {
  return false;
}

bool ListedElement::IsObjectElement() const {
  return false;
}

ListedElement* ListedElement::From(Element& element) {
  auto* html_element = DynamicTo<HTMLElement>(element);
  if (!html_element)
    return nullptr;
  if (auto* form_control_element = DynamicTo<HTMLFormControlElement>(element))
    return form_control_element;
  if (html_element->IsFormAssociatedCustomElement())
    return &element.EnsureElementInternals();
  if (auto* object = DynamicTo<HTMLObjectElement>(html_element))
    return object;
  return nullptr;
}

const HTMLElement& ListedElement::ToHTMLElement() const {
  if (auto* form_control_element = DynamicTo<HTMLFormControlElement>(*this)) {
    return *form_control_element;
  }
  if (auto* element_internals = DynamicTo<ElementInternals>(*this)) {
    return element_internals->Target();
  }
  return To<HTMLObjectElement>(*this);
}

HTMLElement& ListedElement::ToHTMLElement() {
  return const_cast<HTMLElement&>(
      static_cast<const ListedElement&>(*this).ToHTMLElement());
}

FormAttributeTargetObserver::FormAttributeTargetObserver(const AtomicString& id,
                                                         ListedElement* element)
    : IdTargetObserver(element->ToHTMLElement()
                           .GetTreeScope()
                           .EnsureIdTargetObserverRegistry(),
                       id),
      element_(element) {}

void FormAttributeTargetObserver::Trace(Visitor* visitor) const {
  visitor->Trace(element_);
  IdTargetObserver::Trace(visitor);
}

void FormAttributeTargetObserver::IdTargetChanged() {
  element_->FormAttributeTargetChanged();
}

}  // namespace blink
