/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights
 * reserved.
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

#include "third_party/blink/renderer/core/html/forms/html_form_element.h"

#include <limits>
#include "base/auto_reset.h"
#include "third_party/blink/public/platform/web_insecure_request_policy.h"
#include "third_party/blink/renderer/bindings/core/v8/radio_node_list_or_element.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/script_event_listener.h"
#include "third_party/blink/renderer/core/dom/attribute.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/scoped_event_queue.h"
#include "third_party/blink/renderer/core/dom/node_lists_node_data.h"
#include "third_party/blink/renderer/core/dom/user_gesture_indicator.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/remote_frame.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/html/forms/form_controller.h"
#include "third_party/blink/renderer/core/html/forms/form_data.h"
#include "third_party/blink/renderer/core/html/forms/form_data_event.h"
#include "third_party/blink/renderer/core/html/forms/html_form_controls_collection.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/radio_node_list.h"
#include "third_party/blink/renderer/core/html/html_collection.h"
#include "third_party/blink/renderer/core/html/html_dialog_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/html_object_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/loader/form_submission.h"
#include "third_party/blink/renderer/core/loader/mixed_content_checker.h"
#include "third_party/blink/renderer/core/loader/navigation_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

using namespace HTMLNames;

HTMLFormElement::HTMLFormElement(Document& document)
    : HTMLElement(formTag, document),
      listed_elements_are_dirty_(false),
      image_elements_are_dirty_(false),
      has_elements_associated_by_parser_(false),
      has_elements_associated_by_form_attribute_(false),
      did_finish_parsing_children_(false),
      is_in_reset_function_(false) {
  static unsigned next_nique_renderer_form_id = 0;
  unique_renderer_form_id_ = next_nique_renderer_form_id++;
}

HTMLFormElement* HTMLFormElement::Create(Document& document) {
  UseCounter::Count(document, WebFeature::kFormElement);
  return new HTMLFormElement(document);
}

HTMLFormElement::~HTMLFormElement() = default;

void HTMLFormElement::Trace(blink::Visitor* visitor) {
  visitor->Trace(past_names_map_);
  visitor->Trace(radio_button_group_scope_);
  visitor->Trace(listed_elements_);
  visitor->Trace(image_elements_);
  visitor->Trace(planned_navigation_);
  HTMLElement::Trace(visitor);
}

bool HTMLFormElement::MatchesValidityPseudoClasses() const {
  return true;
}

bool HTMLFormElement::IsValidElement() {
  return !CheckInvalidControlsAndCollectUnhandled(
      nullptr, kCheckValidityDispatchNoEvent);
}

Node::InsertionNotificationRequest HTMLFormElement::InsertedInto(
    ContainerNode& insertion_point) {
  HTMLElement::InsertedInto(insertion_point);
  LogAddElementIfIsolatedWorldAndInDocument("form", methodAttr, actionAttr);
  if (insertion_point.isConnected())
    GetDocument().DidAssociateFormControl(this);
  return kInsertionDone;
}

template <class T>
void NotifyFormRemovedFromTree(const T& elements, Node& root) {
  for (const auto& element : elements)
    element->FormRemovedFromTree(root);
}

void HTMLFormElement::RemovedFrom(ContainerNode& insertion_point) {
  // We don't need to take care of form association by 'form' content
  // attribute becuse IdTargetObserver handles it.
  if (has_elements_associated_by_parser_) {
    Node& root = NodeTraversal::HighestAncestorOrSelf(*this);
    if (!listed_elements_are_dirty_) {
      ListedElement::List elements(ListedElements());
      NotifyFormRemovedFromTree(elements, root);
    } else {
      ListedElement::List elements;
      CollectListedElements(
          NodeTraversal::HighestAncestorOrSelf(insertion_point), elements);
      NotifyFormRemovedFromTree(elements, root);
      CollectListedElements(root, elements);
      NotifyFormRemovedFromTree(elements, root);
    }

    if (!image_elements_are_dirty_) {
      HeapVector<Member<HTMLImageElement>> images(ImageElements());
      NotifyFormRemovedFromTree(images, root);
    } else {
      HeapVector<Member<HTMLImageElement>> images;
      CollectImageElements(
          NodeTraversal::HighestAncestorOrSelf(insertion_point), images);
      NotifyFormRemovedFromTree(images, root);
      CollectImageElements(root, images);
      NotifyFormRemovedFromTree(images, root);
    }
  }
  GetDocument().GetFormController().WillDeleteForm(this);
  HTMLElement::RemovedFrom(insertion_point);
}

void HTMLFormElement::HandleLocalEvents(Event& event) {
  Node* target_node = event.target()->ToNode();
  if (event.eventPhase() != Event::kCapturingPhase && target_node &&
      target_node != this &&
      (event.type() == EventTypeNames::submit ||
       event.type() == EventTypeNames::reset)) {
    event.stopPropagation();
    return;
  }
  HTMLElement::HandleLocalEvents(event);
}

unsigned HTMLFormElement::length() const {
  unsigned len = 0;
  for (const auto& element : ListedElements()) {
    if (element->IsEnumeratable())
      ++len;
  }
  return len;
}

HTMLElement* HTMLFormElement::item(unsigned index) {
  return elements()->item(index);
}

void HTMLFormElement::SubmitImplicitly(Event& event,
                                       bool from_implicit_submission_trigger) {
  int submission_trigger_count = 0;
  bool seen_default_button = false;
  for (const auto& element : ListedElements()) {
    if (!element->IsFormControlElement())
      continue;
    HTMLFormControlElement* control = ToHTMLFormControlElement(element);
    if (!seen_default_button && control->CanBeSuccessfulSubmitButton()) {
      if (from_implicit_submission_trigger)
        seen_default_button = true;
      if (control->IsSuccessfulSubmitButton()) {
        control->DispatchSimulatedClick(&event);
        return;
      }
      if (from_implicit_submission_trigger) {
        // Default (submit) button is not activated; no implicit submission.
        return;
      }
    } else if (control->CanTriggerImplicitSubmission()) {
      ++submission_trigger_count;
    }
  }
  if (from_implicit_submission_trigger && submission_trigger_count == 1)
    PrepareForSubmission(event, nullptr);
}

bool HTMLFormElement::ValidateInteractively() {
  UseCounter::Count(GetDocument(), WebFeature::kFormValidationStarted);
  for (const auto& element : ListedElements()) {
    if (element->IsFormControlElement())
      ToHTMLFormControlElement(element)->HideVisibleValidationMessage();
  }

  HeapVector<Member<HTMLFormControlElement>> unhandled_invalid_controls;
  if (!CheckInvalidControlsAndCollectUnhandled(
          &unhandled_invalid_controls, kCheckValidityDispatchInvalidEvent))
    return true;
  UseCounter::Count(GetDocument(),
                    WebFeature::kFormValidationAbortedSubmission);
  // Because the form has invalid controls, we abort the form submission and
  // show a validation message on a focusable form control.

  // Needs to update layout now because we'd like to call isFocusable(), which
  // has !layoutObject()->needsLayout() assertion.
  GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();

  // Focus on the first focusable control and show a validation message.
  for (const auto& unhandled : unhandled_invalid_controls) {
    if (unhandled->IsFocusable()) {
      unhandled->ShowValidationMessage();
      UseCounter::Count(GetDocument(),
                        WebFeature::kFormValidationShowedMessage);
      break;
    }
  }
  // Warn about all of unfocusable controls.
  if (GetDocument().GetFrame()) {
    for (const auto& unhandled : unhandled_invalid_controls) {
      if (unhandled->IsFocusable())
        continue;
      String message(
          "An invalid form control with name='%name' is not focusable.");
      message.Replace("%name", unhandled->GetName());
      GetDocument().AddConsoleMessage(ConsoleMessage::Create(
          kRenderingMessageSource, kErrorMessageLevel, message));
    }
  }
  return false;
}

void HTMLFormElement::PrepareForSubmission(
    Event& event,
    HTMLFormControlElement* submit_button) {
  LocalFrame* frame = GetDocument().GetFrame();
  if (!frame || is_submitting_ || in_user_js_submit_event_)
    return;

  if (!isConnected()) {
    GetDocument().AddConsoleMessage(ConsoleMessage::Create(
        kJSMessageSource, kWarningMessageLevel,
        "Form submission canceled because the form is not connected"));
    return;
  }

  if (GetDocument().IsSandboxed(kSandboxForms)) {
    GetDocument().AddConsoleMessage(ConsoleMessage::Create(
        kSecurityMessageSource, kErrorMessageLevel,
        "Blocked form submission to '" + attributes_.Action() +
            "' because the form's frame is sandboxed and the 'allow-forms' "
            "permission is not set."));
    return;
  }

  // https://github.com/whatwg/html/issues/2253
  for (const auto& element : ListedElements()) {
    if (element->IsFormControlElement() &&
        ToHTMLFormControlElement(element)->BlocksFormSubmission()) {
      UseCounter::Count(GetDocument(),
                        WebFeature::kFormSubmittedWithUnclosedFormControl);
      if (RuntimeEnabledFeatures::UnclosedFormControlIsInvalidEnabled()) {
        String tag_name = ToHTMLFormControlElement(element)->tagName();
        GetDocument().AddConsoleMessage(ConsoleMessage::Create(
            kSecurityMessageSource, kErrorMessageLevel,
            "Form submission failed, as the <" + tag_name +
                "> element named "
                "'" +
                element->GetName() +
                "' was implicitly closed by reaching "
                "the end of the file. Please add an explicit end tag "
                "('</" +
                tag_name + ">')"));
        DispatchEvent(*Event::Create(EventTypeNames::error));
        return;
      }
    }
  }

  bool skip_validation = !GetDocument().GetPage() || NoValidate();
  if (submit_button && submit_button->FormNoValidate())
    skip_validation = true;

  UseCounter::Count(GetDocument(), WebFeature::kFormSubmissionStarted);
  // Interactive validation must be done before dispatching the submit event.
  if (!skip_validation && !ValidateInteractively())
    return;

  bool should_submit;
  {
    base::AutoReset<bool> submit_event_handler_scope(&in_user_js_submit_event_,
                                                     true);
    frame->Client()->DispatchWillSendSubmitEvent(this);
    should_submit =
        DispatchEvent(*Event::CreateCancelableBubble(EventTypeNames::submit)) ==
        DispatchEventResult::kNotCanceled;
  }
  if (should_submit) {
    planned_navigation_ = nullptr;
    Submit(&event, submit_button);
  }
  if (!planned_navigation_)
    return;
  base::AutoReset<bool> submit_scope(&is_submitting_, true);
  ScheduleFormSubmission(planned_navigation_);
  planned_navigation_ = nullptr;
}

void HTMLFormElement::submitFromJavaScript() {
  Submit(nullptr, nullptr);
}

void HTMLFormElement::SubmitDialog(FormSubmission* form_submission) {
  for (Node* node = this; node; node = node->ParentOrShadowHostNode()) {
    if (auto* dialog = ToHTMLDialogElementOrNull(*node)) {
      dialog->close(form_submission->Result());
      return;
    }
  }
}

void HTMLFormElement::Submit(Event* event,
                             HTMLFormControlElement* submit_button) {
  LocalFrameView* view = GetDocument().View();
  LocalFrame* frame = GetDocument().GetFrame();
  if (!view || !frame || !frame->GetPage())
    return;

  // https://html.spec.whatwg.org/multipage/forms.html#form-submission-algorithm
  // 2. If form document is not connected, has no associated browsing context,
  // or its active sandboxing flag set has its sandboxed forms browsing
  // context flag set, then abort these steps without doing anything.
  if (!isConnected()) {
    GetDocument().AddConsoleMessage(ConsoleMessage::Create(
        kJSMessageSource, kWarningMessageLevel,
        "Form submission canceled because the form is not connected"));
    return;
  }

  if (is_submitting_)
    return;

  // Delay dispatching 'close' to dialog until done submitting.
  EventQueueScope scope_for_dialog_close;
  base::AutoReset<bool> submit_scope(&is_submitting_, true);

  if (event && !submit_button) {
    // In a case of implicit submission without a submit button, 'submit'
    // event handler might add a submit button. We search for a submit
    // button again.
    // TODO(tkent): Do we really need to activate such submit button?
    for (const auto& listed_element : ListedElements()) {
      if (!listed_element->IsFormControlElement())
        continue;
      HTMLFormControlElement* control =
          ToHTMLFormControlElement(listed_element);
      DCHECK(!control->IsActivatedSubmit());
      if (control->IsSuccessfulSubmitButton()) {
        submit_button = control;
        break;
      }
    }
  }

  FormSubmission* form_submission =
      FormSubmission::Create(this, attributes_, event, submit_button);
  // 'formdata' event handlers might disconnect the form.
  if (RuntimeEnabledFeatures::FormDataEventEnabled() && !isConnected()) {
    GetDocument().AddConsoleMessage(ConsoleMessage::Create(
        kJSMessageSource, kWarningMessageLevel,
        "Form submission canceled because the form is not connected"));
    return;
  }
  if (form_submission->Method() == FormSubmission::kDialogMethod) {
    SubmitDialog(form_submission);
  } else if (in_user_js_submit_event_) {
    // Need to postpone the submission in order to make this cancelable by
    // another submission request.
    planned_navigation_ = form_submission;
  } else {
    // This runs JavaScript code if action attribute value is javascript:
    // protocol.
    ScheduleFormSubmission(form_submission);
  }
}

void HTMLFormElement::ConstructFormDataSet(
    HTMLFormControlElement* submit_button,
    FormData& form_data) {
  // TODO(tkent): We might move the event dispatching later than the
  // ListedElements iteration.
  if (RuntimeEnabledFeatures::FormDataEventEnabled())
    DispatchEvent(*FormDataEvent::Create(form_data));

  if (submit_button)
    submit_button->SetActivatedSubmit(true);
  for (ListedElement* control : ListedElements()) {
    DCHECK(control);
    HTMLElement& element = ToHTMLElement(*control);
    if (!element.IsDisabledFormControl())
      control->AppendToFormData(form_data);
    if (auto* input = ToHTMLInputElementOrNull(element)) {
      if (input->type() == InputTypeNames::password &&
          !input->value().IsEmpty())
        form_data.SetContainsPasswordData(true);
    }
  }
  if (submit_button)
    submit_button->SetActivatedSubmit(false);
}

void HTMLFormElement::ScheduleFormSubmission(FormSubmission* submission) {
  DCHECK(submission->Method() == FormSubmission::kPostMethod ||
         submission->Method() == FormSubmission::kGetMethod);
  DCHECK(submission->Data());
  DCHECK(submission->Form());
  if (submission->Action().IsEmpty())
    return;
  if (GetDocument().IsSandboxed(kSandboxForms)) {
    // FIXME: This message should be moved off the console once a solution to
    // https://bugs.webkit.org/show_bug.cgi?id=103274 exists.
    GetDocument().AddConsoleMessage(ConsoleMessage::Create(
        kSecurityMessageSource, kErrorMessageLevel,
        "Blocked form submission to '" + submission->Action().ElidedString() +
            "' because the form's frame is sandboxed and the 'allow-forms' "
            "permission is not set."));
    return;
  }

  if (!GetDocument().GetContentSecurityPolicy()->AllowFormAction(
          submission->Action())) {
    return;
  }

  if (submission->Action().ProtocolIsJavaScript()) {
    if (FastHasAttribute(disabledAttr)) {
      UseCounter::Count(GetDocument(),
                        WebFeature::kFormDisabledAttributePresentAndSubmit);
    }
    GetDocument()
        .GetFrame()
        ->GetScriptController()
        .ExecuteScriptIfJavaScriptURL(submission->Action(), this);
    return;
  }

  Frame* target_frame = GetDocument().GetFrame()->FindFrameForNavigation(
      submission->Target(), *GetDocument().GetFrame(),
      submission->RequestURL());
  if (!target_frame) {
    target_frame = GetDocument().GetFrame();
  } else {
    submission->ClearTarget();
  }
  if (!target_frame->GetPage())
    return;

  UseCounter::Count(GetDocument(), WebFeature::kFormsSubmitted);
  if (MixedContentChecker::IsMixedFormAction(GetDocument().GetFrame(),
                                             submission->Action())) {
    UseCounter::Count(GetDocument().GetFrame(),
                      WebFeature::kMixedContentFormsSubmitted);
  }
  if (FastHasAttribute(disabledAttr)) {
    UseCounter::Count(GetDocument(),
                      WebFeature::kFormDisabledAttributePresentAndSubmit);
  }

  // TODO(lukasza): Investigate if the code below can uniformly handle remote
  // and local frames (i.e. by calling virtual Frame::navigate from a timer).
  // See also https://goo.gl/95d2KA.
  if (target_frame->IsLocalFrame()) {
    ToLocalFrame(target_frame)
        ->GetNavigationScheduler()
        .ScheduleFormSubmission(&GetDocument(), submission);
  } else {
    FrameLoadRequest frame_load_request =
        submission->CreateFrameLoadRequest(&GetDocument());
    frame_load_request.GetResourceRequest().SetHasUserGesture(
        LocalFrame::HasTransientUserActivation(GetDocument().GetFrame()));
    // TODO(dgozman): we lose information about triggering event and desired
    // navigation policy here.
    ToRemoteFrame(target_frame)
        ->Navigate(frame_load_request, WebFrameLoadType::kStandard);
  }
}

void HTMLFormElement::reset() {
  LocalFrame* frame = GetDocument().GetFrame();
  if (is_in_reset_function_ || !frame)
    return;

  is_in_reset_function_ = true;

  if (DispatchEvent(*Event::CreateCancelableBubble(EventTypeNames::reset)) !=
      DispatchEventResult::kNotCanceled) {
    is_in_reset_function_ = false;
    return;
  }

  // Copy the element list because |reset()| implementation can update DOM
  // structure.
  ListedElement::List elements(ListedElements());
  for (const auto& element : elements) {
    if (element->IsFormControlElement())
      ToHTMLFormControlElement(element)->Reset();
  }

  is_in_reset_function_ = false;
}

void HTMLFormElement::ParseAttribute(
    const AttributeModificationParams& params) {
  const QualifiedName& name = params.name;
  if (name == actionAttr) {
    attributes_.ParseAction(params.new_value);
    LogUpdateAttributeIfIsolatedWorldAndInDocument("form", params);

    // If we're not upgrading insecure requests, and the new action attribute is
    // pointing to an insecure "action" location from a secure page it is marked
    // as "passive" mixed content.
    if (GetDocument().GetInsecureRequestPolicy() & kUpgradeInsecureRequests)
      return;
    KURL action_url = GetDocument().CompleteURL(
        attributes_.Action().IsEmpty() ? GetDocument().Url().GetString()
                                       : attributes_.Action());
    if (MixedContentChecker::IsMixedFormAction(GetDocument().GetFrame(),
                                               action_url)) {
      UseCounter::Count(GetDocument().GetFrame(),
                        WebFeature::kMixedContentFormPresent);
    }
  } else if (name == targetAttr) {
    attributes_.SetTarget(params.new_value);
  } else if (name == methodAttr) {
    attributes_.UpdateMethodType(params.new_value);
  } else if (name == enctypeAttr) {
    attributes_.UpdateEncodingType(params.new_value);
  } else if (name == accept_charsetAttr) {
    attributes_.SetAcceptCharset(params.new_value);
  } else if (name == disabledAttr) {
    UseCounter::Count(GetDocument(), WebFeature::kFormDisabledAttributePresent);
  } else {
    HTMLElement::ParseAttribute(params);
  }
}

void HTMLFormElement::Associate(ListedElement& e) {
  listed_elements_are_dirty_ = true;
  listed_elements_.clear();
  if (ToHTMLElement(e).FastHasAttribute(formAttr))
    has_elements_associated_by_form_attribute_ = true;
}

void HTMLFormElement::Disassociate(ListedElement& e) {
  listed_elements_are_dirty_ = true;
  listed_elements_.clear();
  RemoveFromPastNamesMap(ToHTMLElement(e));
}

bool HTMLFormElement::IsURLAttribute(const Attribute& attribute) const {
  return attribute.GetName() == actionAttr ||
         HTMLElement::IsURLAttribute(attribute);
}

bool HTMLFormElement::HasLegalLinkAttribute(const QualifiedName& name) const {
  return name == actionAttr || HTMLElement::HasLegalLinkAttribute(name);
}

void HTMLFormElement::Associate(HTMLImageElement& e) {
  image_elements_are_dirty_ = true;
  image_elements_.clear();
}

void HTMLFormElement::Disassociate(HTMLImageElement& e) {
  image_elements_are_dirty_ = true;
  image_elements_.clear();
  RemoveFromPastNamesMap(e);
}

void HTMLFormElement::DidAssociateByParser() {
  if (!did_finish_parsing_children_)
    return;
  has_elements_associated_by_parser_ = true;
  UseCounter::Count(GetDocument(), WebFeature::kFormAssociationByParser);
}

HTMLFormControlsCollection* HTMLFormElement::elements() {
  return EnsureCachedCollection<HTMLFormControlsCollection>(kFormControls);
}

void HTMLFormElement::CollectListedElements(
    Node& root,
    ListedElement::List& elements) const {
  elements.clear();
  for (HTMLElement& element : Traversal<HTMLElement>::StartsAfter(root)) {
    ListedElement* listed_element = nullptr;
    if (element.IsFormControlElement())
      listed_element = ToHTMLFormControlElement(&element);
    else if (auto* object = ToHTMLObjectElementOrNull(element))
      listed_element = object;
    else
      continue;
    if (listed_element->Form() == this)
      elements.push_back(listed_element);
  }
}

// This function should be const conceptually. However we update some fields
// because of lazy evaluation.
const ListedElement::List& HTMLFormElement::ListedElements() const {
  if (!listed_elements_are_dirty_)
    return listed_elements_;
  HTMLFormElement* mutable_this = const_cast<HTMLFormElement*>(this);
  Node* scope = mutable_this;
  if (has_elements_associated_by_parser_)
    scope = &NodeTraversal::HighestAncestorOrSelf(*mutable_this);
  if (isConnected() && has_elements_associated_by_form_attribute_)
    scope = &GetTreeScope().RootNode();
  DCHECK(scope);
  CollectListedElements(*scope, mutable_this->listed_elements_);
  mutable_this->listed_elements_are_dirty_ = false;
  return listed_elements_;
}

void HTMLFormElement::CollectImageElements(
    Node& root,
    HeapVector<Member<HTMLImageElement>>& elements) {
  elements.clear();
  for (HTMLImageElement& image :
       Traversal<HTMLImageElement>::StartsAfter(root)) {
    if (image.formOwner() == this)
      elements.push_back(&image);
  }
}

const HeapVector<Member<HTMLImageElement>>& HTMLFormElement::ImageElements() {
  if (!image_elements_are_dirty_)
    return image_elements_;
  CollectImageElements(has_elements_associated_by_parser_
                           ? NodeTraversal::HighestAncestorOrSelf(*this)
                           : *this,
                       image_elements_);
  image_elements_are_dirty_ = false;
  return image_elements_;
}

String HTMLFormElement::GetName() const {
  return GetNameAttribute();
}

bool HTMLFormElement::NoValidate() const {
  return FastHasAttribute(novalidateAttr);
}

String HTMLFormElement::action() const {
  Document& document = GetDocument();
  KURL action_url = document.CompleteURL(attributes_.Action().IsEmpty()
                                             ? document.Url().GetString()
                                             : attributes_.Action());
  return action_url.GetString();
}

void HTMLFormElement::setAction(const AtomicString& value) {
  setAttribute(actionAttr, value);
}

void HTMLFormElement::setEnctype(const AtomicString& value) {
  setAttribute(enctypeAttr, value);
}

String HTMLFormElement::method() const {
  return FormSubmission::Attributes::MethodString(attributes_.Method());
}

void HTMLFormElement::setMethod(const AtomicString& value) {
  setAttribute(methodAttr, value);
}

HTMLFormControlElement* HTMLFormElement::FindDefaultButton() const {
  for (const auto& element : ListedElements()) {
    if (!element->IsFormControlElement())
      continue;
    HTMLFormControlElement* control = ToHTMLFormControlElement(element);
    if (control->CanBeSuccessfulSubmitButton())
      return control;
  }
  return nullptr;
}

bool HTMLFormElement::checkValidity() {
  return !CheckInvalidControlsAndCollectUnhandled(
      nullptr, kCheckValidityDispatchInvalidEvent);
}

bool HTMLFormElement::CheckInvalidControlsAndCollectUnhandled(
    HeapVector<Member<HTMLFormControlElement>>* unhandled_invalid_controls,
    CheckValidityEventBehavior event_behavior) {
  // Copy listedElements because event handlers called from
  // HTMLFormControlElement::checkValidity() might change listedElements.
  const ListedElement::List& listed_elements = ListedElements();
  HeapVector<Member<ListedElement>> elements;
  elements.ReserveCapacity(listed_elements.size());
  for (const auto& element : listed_elements)
    elements.push_back(element);
  int invalid_controls_count = 0;
  for (const auto& element : elements) {
    if (element->Form() == this && element->IsFormControlElement()) {
      HTMLFormControlElement* control = ToHTMLFormControlElement(element);
      if (control->IsSubmittableElement() &&
          !control->checkValidity(unhandled_invalid_controls, event_behavior) &&
          control->formOwner() == this) {
        ++invalid_controls_count;
        if (!unhandled_invalid_controls &&
            event_behavior == kCheckValidityDispatchNoEvent)
          return true;
      }
    }
  }
  return invalid_controls_count;
}

bool HTMLFormElement::reportValidity() {
  return ValidateInteractively();
}

Element* HTMLFormElement::ElementFromPastNamesMap(
    const AtomicString& past_name) {
  if (past_name.IsEmpty() || !past_names_map_)
    return nullptr;
  Element* element = past_names_map_->at(past_name);
#if DCHECK_IS_ON()
  if (!element)
    return nullptr;
  SECURITY_DCHECK(ToHTMLElement(element)->formOwner() == this);
  if (IsHTMLImageElement(*element)) {
    SECURITY_DCHECK(ImageElements().Find(element) != kNotFound);
  } else if (IsHTMLObjectElement(*element)) {
    SECURITY_DCHECK(ListedElements().Find(ToHTMLObjectElement(element)) !=
                    kNotFound);
  } else {
    SECURITY_DCHECK(ListedElements().Find(ToHTMLFormControlElement(element)) !=
                    kNotFound);
  }
#endif
  return element;
}

void HTMLFormElement::AddToPastNamesMap(Element* element,
                                        const AtomicString& past_name) {
  if (past_name.IsEmpty())
    return;
  if (!past_names_map_)
    past_names_map_ = new PastNamesMap;
  past_names_map_->Set(past_name, element);
}

void HTMLFormElement::RemoveFromPastNamesMap(HTMLElement& element) {
  if (!past_names_map_)
    return;
  for (auto& it : *past_names_map_) {
    if (it.value == &element) {
      it.value = nullptr;
      // Keep looping. Single element can have multiple names.
    }
  }
}

void HTMLFormElement::GetNamedElements(
    const AtomicString& name,
    HeapVector<Member<Element>>& named_items) {
  // http://www.whatwg.org/specs/web-apps/current-work/multipage/forms.html#dom-form-nameditem
  elements()->NamedItems(name, named_items);

  Element* element_from_past = ElementFromPastNamesMap(name);
  if (named_items.size() && named_items.front() != element_from_past) {
    AddToPastNamesMap(named_items.front().Get(), name);
  } else if (element_from_past && named_items.IsEmpty()) {
    named_items.push_back(element_from_past);
    UseCounter::Count(GetDocument(),
                      WebFeature::kFormNameAccessForPastNamesMap);
  }
}

bool HTMLFormElement::ShouldAutocomplete() const {
  return !DeprecatedEqualIgnoringCase(FastGetAttribute(autocompleteAttr),
                                      "off");
}

void HTMLFormElement::FinishParsingChildren() {
  HTMLElement::FinishParsingChildren();
  GetDocument().GetFormController().RestoreControlStateIn(*this);
  did_finish_parsing_children_ = true;
}

void HTMLFormElement::AnonymousNamedGetter(
    const AtomicString& name,
    RadioNodeListOrElement& return_value) {
  // Call getNamedElements twice, first time check if it has a value
  // and let HTMLFormElement update its cache.
  // See issue: 867404
  {
    HeapVector<Member<Element>> elements;
    GetNamedElements(name, elements);
    if (elements.IsEmpty())
      return;
  }

  // Second call may return different results from the first call,
  // but if the first the size cannot be zero.
  HeapVector<Member<Element>> elements;
  GetNamedElements(name, elements);
  DCHECK(!elements.IsEmpty());

  bool only_match_img =
      !elements.IsEmpty() && IsHTMLImageElement(*elements.front());
  if (only_match_img) {
    UseCounter::Count(GetDocument(),
                      WebFeature::kFormNameAccessForImageElement);
    // The following code has performance impact, but it should be small
    // because <img> access via <form> name getter is rarely used.
    for (auto& element : elements) {
      if (IsHTMLImageElement(*element) && !element->IsDescendantOf(this)) {
        UseCounter::Count(
            GetDocument(),
            WebFeature::kFormNameAccessForNonDescendantImageElement);
        break;
      }
    }
  }
  if (elements.size() == 1) {
    return_value.SetElement(elements.at(0));
    return;
  }

  return_value.SetRadioNodeList(GetRadioNodeList(name, only_match_img));
}

void HTMLFormElement::InvalidateDefaultButtonStyle() const {
  for (const auto& control : ListedElements()) {
    if (!control->IsFormControlElement())
      continue;
    if (ToHTMLFormControlElement(control)->CanBeSuccessfulSubmitButton()) {
      ToHTMLFormControlElement(control)->PseudoStateChanged(
          CSSSelector::kPseudoDefault);
    }
  }
}

}  // namespace blink
