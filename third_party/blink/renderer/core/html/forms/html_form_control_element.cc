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

#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/css/selector_checker.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatch_forbidden_scope.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/forms/listed_element.h"
#include "third_party/blink/renderer/core/html/forms/validity_state.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

HTMLFormControlElement::HTMLFormControlElement(const QualifiedName& tag_name,
                                               Document& document)
    : HTMLElement(tag_name, document),
      autofill_state_(WebAutofillState::kNotFilled),
      blocks_form_submission_(false) {
  SetHasCustomStyleCallbacks();
  static uint64_t next_free_unique_id = 1;
  unique_renderer_form_control_id_ = next_free_unique_id++;
}

HTMLFormControlElement::~HTMLFormControlElement() = default;

void HTMLFormControlElement::Trace(Visitor* visitor) const {
  ListedElement::Trace(visitor);
  HTMLElement::Trace(visitor);
}

String HTMLFormControlElement::formAction() const {
  const AtomicString& action = FastGetAttribute(html_names::kFormactionAttr);
  if (action.empty()) {
    return GetDocument().Url();
  }
  return GetDocument().CompleteURL(StripLeadingAndTrailingHTMLSpaces(action));
}

void HTMLFormControlElement::setFormAction(const AtomicString& value) {
  setAttribute(html_names::kFormactionAttr, value);
}

String HTMLFormControlElement::formEnctype() const {
  const AtomicString& form_enctype_attr =
      FastGetAttribute(html_names::kFormenctypeAttr);
  if (form_enctype_attr.IsNull())
    return g_empty_string;
  return FormSubmission::Attributes::ParseEncodingType(form_enctype_attr);
}

void HTMLFormControlElement::setFormEnctype(const AtomicString& value) {
  setAttribute(html_names::kFormenctypeAttr, value);
}

String HTMLFormControlElement::formMethod() const {
  const AtomicString& form_method_attr =
      FastGetAttribute(html_names::kFormmethodAttr);
  if (form_method_attr.IsNull())
    return g_empty_string;
  return FormSubmission::Attributes::MethodString(
      FormSubmission::Attributes::ParseMethodType(form_method_attr));
}

void HTMLFormControlElement::setFormMethod(const AtomicString& value) {
  setAttribute(html_names::kFormmethodAttr, value);
}

bool HTMLFormControlElement::FormNoValidate() const {
  return FastHasAttribute(html_names::kFormnovalidateAttr);
}

void HTMLFormControlElement::Reset() {
  SetAutofillState(WebAutofillState::kNotFilled);
  ResetImpl();
}

void HTMLFormControlElement::AttributeChanged(
    const AttributeModificationParams& params) {
  HTMLElement::AttributeChanged(params);
  if (params.name == html_names::kDisabledAttr &&
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
  if (name == html_names::kFormAttr) {
    FormAttributeChanged();
    UseCounter::Count(GetDocument(), WebFeature::kFormAttribute);
  } else if (name == html_names::kReadonlyAttr) {
    if (params.old_value.IsNull() != params.new_value.IsNull()) {
      ReadonlyAttributeChanged();
      PseudoStateChanged(CSSSelector::kPseudoReadOnly);
      PseudoStateChanged(CSSSelector::kPseudoReadWrite);
      InvalidateIfHasEffectiveAppearance();
    }
  } else if (name == html_names::kRequiredAttr) {
    if (params.old_value.IsNull() != params.new_value.IsNull())
      RequiredAttributeChanged();
    UseCounter::Count(GetDocument(), WebFeature::kRequiredAttribute);
  } else if (name == html_names::kAutofocusAttr) {
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

  ListedElement::DisabledAttributeChanged();
  InvalidateIfHasEffectiveAppearance();

  // TODO(dmazzoni): http://crbug.com/699438.
  // Replace |CheckedStateChanged| with a generic tree changed event.
  if (AXObjectCache* cache = GetDocument().ExistingAXObjectCache())
    cache->CheckedStateChanged(this);

  CheckAndPossiblyClosePopoverStack();
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
  return FastHasAttribute(html_names::kReadonlyAttr);
}

bool HTMLFormControlElement::IsDisabledOrReadOnly() const {
  return IsDisabledFormControl() || IsReadOnly();
}

void HTMLFormControlElement::SetAutofillState(WebAutofillState autofill_state) {
  if (autofill_state == autofill_state_)
    return;

  autofill_state_ = autofill_state;
  PseudoStateChanged(CSSSelector::kPseudoAutofill);
  PseudoStateChanged(CSSSelector::kPseudoWebKitAutofill);
  PseudoStateChanged(CSSSelector::kPseudoAutofillSelected);
  PseudoStateChanged(CSSSelector::kPseudoAutofillPreviewed);
}

void HTMLFormControlElement::SetPreventHighlightingOfAutofilledFields(
    bool prevent_highlighting) {
  if (prevent_highlighting == prevent_highlighting_of_autofilled_fields_)
    return;

  prevent_highlighting_of_autofilled_fields_ = prevent_highlighting;
  PseudoStateChanged(CSSSelector::kPseudoAutofill);
  PseudoStateChanged(CSSSelector::kPseudoWebKitAutofill);
  PseudoStateChanged(CSSSelector::kPseudoAutofillSelected);
  PseudoStateChanged(CSSSelector::kPseudoAutofillPreviewed);
}

void HTMLFormControlElement::SetAutofillSection(const WebString& section) {
  autofill_section_ = section;
}

bool HTMLFormControlElement::IsAutocompleteEmailUrlOrPassword() const {
  DEFINE_STATIC_LOCAL(HashSet<AtomicString>, values,
                      ({"username", "new-password", "current-password", "url",
                        "email", "impp"}));
  const AtomicString& autocomplete =
      FastGetAttribute(html_names::kAutocompleteAttr);
  if (autocomplete.IsNull())
    return false;
  return values.Contains(autocomplete.LowerASCII());
}

const AtomicString& HTMLFormControlElement::autocapitalize() const {
  if (!FastGetAttribute(html_names::kAutocapitalizeAttr).empty())
    return HTMLElement::autocapitalize();

  // If the form control itself does not have the autocapitalize attribute set,
  // but the form owner is non-null and does have the autocapitalize attribute
  // set, we inherit from the form owner.
  if (HTMLFormElement* form = Form())
    return form->autocapitalize();

  return g_empty_atom;
}

void HTMLFormControlElement::DidMoveToNewDocument(Document& old_document) {
  ListedElement::DidMoveToNewDocument(old_document);
  HTMLElement::DidMoveToNewDocument(old_document);
}

Node::InsertionNotificationRequest HTMLFormControlElement::InsertedInto(
    ContainerNode& insertion_point) {
  HTMLElement::InsertedInto(insertion_point);
  ListedElement::InsertedInto(insertion_point);
  return kInsertionDone;
}

void HTMLFormControlElement::RemovedFrom(ContainerNode& insertion_point) {
  HTMLElement::RemovedFrom(insertion_point);
  ListedElement::RemovedFrom(insertion_point);
  CheckAndPossiblyClosePopoverStack();
}

void HTMLFormControlElement::WillChangeForm() {
  ListedElement::WillChangeForm();
  if (formOwner() && CanBeSuccessfulSubmitButton())
    formOwner()->InvalidateDefaultButtonStyle();
}

void HTMLFormControlElement::DidChangeForm() {
  ListedElement::DidChangeForm();
  if (formOwner() && isConnected() && CanBeSuccessfulSubmitButton())
    formOwner()->InvalidateDefaultButtonStyle();
  CheckAndPossiblyClosePopoverStack();
}

HTMLFormElement* HTMLFormControlElement::formOwner() const {
  return ListedElement::Form();
}

bool HTMLFormControlElement::IsDisabledFormControl() const {
  // Since the MHTML is loaded in sandboxing mode with form submission and
  // script execution disabled, we should gray out all form control elements
  // to indicate that the form cannot be worked on.
  if (GetDocument().Fetcher()->Archive())
    return true;

  return IsActuallyDisabled();
}

bool HTMLFormControlElement::MatchesEnabledPseudoClass() const {
  return !IsDisabledFormControl();
}

bool HTMLFormControlElement::IsRequired() const {
  return FastHasAttribute(html_names::kRequiredAttr);
}

String HTMLFormControlElement::ResultForDialogSubmit() {
  return FastGetAttribute(html_names::kValueAttr);
}

bool HTMLFormControlElement::SupportsFocus() const {
  return !IsDisabledFormControl();
}

bool HTMLFormControlElement::IsKeyboardFocusable() const {
  if (RuntimeEnabledFeatures::FocuslessSpatialNavigationEnabled())
    return HTMLElement::IsKeyboardFocusable();

  // Skip tabIndex check in a parent class.
  return IsBaseElementFocusable();
}

bool HTMLFormControlElement::MayTriggerVirtualKeyboard() const {
  return false;
}

bool HTMLFormControlElement::ShouldHaveFocusAppearance() const {
  return SelectorChecker::MatchesFocusVisiblePseudoClass(*this);
}

bool HTMLFormControlElement::willValidate() const {
  return ListedElement::WillValidate();
}

bool HTMLFormControlElement::MatchesValidityPseudoClasses() const {
  return willValidate();
}

bool HTMLFormControlElement::IsValidElement() {
  return ListedElement::IsValidElement();
}

bool HTMLFormControlElement::IsSuccessfulSubmitButton() const {
  return CanBeSuccessfulSubmitButton() && !IsDisabledFormControl();
}

// The element is returned if a) that element exists, b) it is a valid Popover
// element, and c) this form control supports popover triggering. If multiple
// toggle attributes are present:
//  1. Only one idref will ever be used, if multiple attributes are present.
//  2. If 'popovertoggletarget' is present, its IDREF will be used.
//  3. If 'popovershowtarget' is present and 'popovertoggletarget' isn't present
//     or its value doesn't match that of popovershowtarget, only
//     popovershowtarget will be used.
//  4. If both 'popovershowtarget' and 'popoverhidetarget' are present and their
//     values match, the behavior is to toggle.
HTMLFormControlElement::PopoverTargetElement
HTMLFormControlElement::popoverTargetElement() {
  const PopoverTargetElement no_element{.popover = nullptr,
                                        .action = PopoverTriggerAction::kNone,
                                        .attribute_name = g_null_name};
  if (!RuntimeEnabledFeatures::HTMLPopoverAttributeEnabled(
          GetDocument().GetExecutionContext()) ||
      !IsInTreeScope() ||
      SupportsPopoverTriggering() == PopoverTriggerSupport::kNone ||
      IsDisabledFormControl() || (Form() && IsSuccessfulSubmitButton())) {
    return no_element;
  }

  Element* target_element;
  QualifiedName attribute_name = html_names::kPopovertoggletargetAttr;
  PopoverTriggerAction action = PopoverTriggerAction::kToggle;
  target_element = GetElementAttribute(html_names::kPopovertoggletargetAttr);
  if (!target_element) {
    target_element = GetElementAttribute(html_names::kPopovershowtargetAttr);
    if (target_element) {
      action = PopoverTriggerAction::kShow;
      attribute_name = html_names::kPopovershowtargetAttr;
    }
  }
  if (Element* hide_target =
          GetElementAttribute(html_names::kPopoverhidetargetAttr)) {
    if (!target_element) {
      target_element = hide_target;
      action = PopoverTriggerAction::kHide;
      attribute_name = html_names::kPopoverhidetargetAttr;
    } else if (hide_target == target_element) {
      action = PopoverTriggerAction::kToggle;
      // Leave attribute_name as-is in this case.
    }
  }
  if (!target_element) {
    return no_element;
  }
  auto* target_popover = DynamicTo<HTMLElement>(target_element);
  if (!target_popover || !target_popover->HasPopoverAttribute()) {
    return no_element;
  }
  return PopoverTargetElement{.popover = target_popover,
                              .action = action,
                              .attribute_name = attribute_name};
}

void HTMLFormControlElement::DefaultEventHandler(Event& event) {
  if (!IsDisabledFormControl()) {
    auto popover = popoverTargetElement();
    if (popover.popover) {
      DCHECK(RuntimeEnabledFeatures::HTMLPopoverAttributeEnabled(
          GetDocument().GetExecutionContext()));
      auto trigger_support = SupportsPopoverTriggering();
      DCHECK_NE(popover.action, PopoverTriggerAction::kNone);
      DCHECK_NE(trigger_support, PopoverTriggerSupport::kNone);
      // Note that the order is: `mousedown` which runs popover light dismiss
      // code, then (for clicked elements) focus is set to the clicked
      // element, then |DOMActivate| runs here. Also note that the light
      // dismiss code will not hide popovers when an activating element is
      // clicked. Taking that together, if the clicked control is a triggering
      // element for a popover, light dismiss will do nothing, focus will be set
      // to the triggering element, then this code will run and will set focus
      // to the previously focused element. If instead the clicked control is
      // not a triggering element, then the light dismiss code will hide the
      // popover and set focus to the previously focused element, then the
      // normal focus management code will reset focus to the clicked control.
      bool can_show =
          popover.popover->IsPopoverReady(PopoverTriggerAction::kShow,
                                          /*exception_state=*/nullptr) &&
          (popover.action == PopoverTriggerAction::kToggle ||
           popover.action == PopoverTriggerAction::kShow);
      bool can_hide =
          popover.popover->IsPopoverReady(PopoverTriggerAction::kHide,
                                          /*exception_state=*/nullptr) &&
          (popover.action == PopoverTriggerAction::kToggle ||
           popover.action == PopoverTriggerAction::kHide);
      if (event.type() == event_type_names::kDOMActivate &&
          (!Form() || !IsSuccessfulSubmitButton())) {
        if (can_hide) {
          popover.popover->HidePopoverInternal(
              HidePopoverFocusBehavior::kFocusPreviousElement,
              HidePopoverTransitionBehavior::kFireEventsAndWaitForTransitions,
              /*exception_state=*/nullptr);
        } else if (can_show) {
          popover.popover->InvokePopover(this);
        }
      }
    }
  }
  HTMLElement::DefaultEventHandler(event);
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
  if (!trimmed_name.empty())
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
}

int32_t HTMLFormControlElement::GetAxId() const {
  Document& document = GetDocument();
  if (!document.IsActive() || !document.View())
    return 0;
  if (AXObjectCache* cache = document.ExistingAXObjectCache()) {
    LocalFrameView* local_frame_view = document.View();
    if (local_frame_view->IsUpdatingLifecycle()) {
      // Autofill (the caller of this code) can end up making calls to get AXIDs
      // of form elements during, e.g. resize observer callbacks, which are
      // in the middle up updating the document lifecycle. In these cases, just
      // return the existing AXID of the element.
      return cache->GetExistingAXID(const_cast<HTMLFormControlElement*>(this));
    }

    if (document.NeedsLayoutTreeUpdate() || document.View()->NeedsLayout() ||
        document.Lifecycle().GetState() < DocumentLifecycle::kPrePaintClean) {
      document.View()->UpdateAllLifecyclePhasesExceptPaint(
          DocumentUpdateReason::kAccessibility);
    }
    return cache->GetAXID(const_cast<HTMLFormControlElement*>(this));
  }

  return 0;
}

}  // namespace blink
