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

#include "base/feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/web/web_form_related_change_type.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/css/selector_checker.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatch_forbidden_scope.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/popover_data.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/command_event.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/forms/html_button_element.h"
#include "third_party/blink/renderer/core/html/forms/html_data_list_element.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/forms/listed_element.h"
#include "third_party/blink/renderer/core/html/forms/validity_state.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/keywords.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

HTMLFormControlElement::HTMLFormControlElement(const QualifiedName& tag_name,
                                               Document& document)
    : HTMLElement(tag_name, document),
      autofill_state_(WebAutofillState::kNotFilled),
      blocks_form_submission_(false) {
  SetHasCustomStyleCallbacks();
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

void HTMLFormControlElement::AttachLayoutTree(AttachContext& context) {
  HTMLElement::AttachLayoutTree(context);
  if (!GetLayoutObject()) {
    FocusabilityLost();
  }
}

void HTMLFormControlElement::DetachLayoutTree(bool performing_reattach) {
  HTMLElement::DetachLayoutTree(performing_reattach);
  if (!performing_reattach) {
    FocusabilityLost();
  }
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

bool HTMLFormControlElement::IsAutocompleteEmailUrlOrPassword() const {
  DEFINE_STATIC_LOCAL(HashSet<AtomicString>, values,
                      ({AtomicString("username"), AtomicString("new-password"),
                        AtomicString("current-password"), AtomicString("url"),
                        AtomicString("email"), AtomicString("impp")}));
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
}

HTMLFormElement* HTMLFormControlElement::formOwner() const {
  return ListedElement::Form();
}

bool HTMLFormControlElement::IsDisabledFormControl() const {
  // When an MHTML page is loaded through a HTTPS URL, it's considered a trusted
  // offline page. This can only happen on Android, and happens automatically
  // sometimes to show cached pages rather than an error page.
  // For this circumstance, it's beneficial to disable form controls so that
  // users do not waste time trying to edit them.
  //
  // For MHTML pages loaded through other means, we do not disable forms. This
  // avoids modification of the original page, and more closely matches other
  // saved page formats.
  if (GetDocument().Fetcher()->Archive()) {
    if (base::FeatureList::IsEnabled(blink::features::kMHTML_Improvements)) {
      if (GetDocument().Url().ProtocolIsInHTTPFamily()) {
        return true;
      }
    } else {
      // Without `kMHTML_Improvements`, MHTML forms are always disabled.
      return true;
    }
  }

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

FocusableState HTMLFormControlElement::SupportsFocus(UpdateBehavior) const {
  return IsDisabledFormControl() ? FocusableState::kNotFocusable
                                 : FocusableState::kFocusable;
}

bool HTMLFormControlElement::IsKeyboardFocusable(
    UpdateBehavior update_behavior) const {
  // Form control elements are always keyboard focusable if they are focusable
  // at all, and don't have a negative tabindex set.
  return IsFocusable(update_behavior) && tabIndex() >= 0;
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

// The element referenced by the `popovertarget` attribute is returned if a)
// that element exists, b) it is a valid Popover element, and c) this form
// control supports popover triggering. The return value will include the
// behavior, which is taken from the `popovertargetaction` attribute, and will
// be kNone unless there is a valid popover target.
HTMLFormControlElement::PopoverTargetElement
HTMLFormControlElement::popoverTargetElement() {
  const PopoverTargetElement no_element{.popover = nullptr,
                                        .action = PopoverTriggerAction::kNone};
  if (!IsInTreeScope() ||
      SupportsPopoverTriggering() == PopoverTriggerSupport::kNone ||
      IsDisabledFormControl() || (Form() && IsSuccessfulSubmitButton())) {
    return no_element;
  }

  Element* target_element;
  target_element = GetElementAttributeResolvingReferenceTarget(
      html_names::kPopovertargetAttr);


  if (!target_element) {
    return no_element;
  }
  auto* target_popover = DynamicTo<HTMLElement>(target_element);
  if (!target_popover || !target_popover->HasPopoverAttribute()) {
    return no_element;
  }
  // The default action is "toggle".
  PopoverTriggerAction action = PopoverTriggerAction::kToggle;
  auto action_value =
      getAttribute(html_names::kPopovertargetactionAttr).LowerASCII();
  if (action_value == "show") {
    action = PopoverTriggerAction::kShow;
  } else if (action_value == "hide") {
    action = PopoverTriggerAction::kHide;
  } else if (RuntimeEnabledFeatures::HTMLPopoverActionHoverEnabled() &&
             action_value == "hover") {
    action = PopoverTriggerAction::kHover;
  }
  return PopoverTargetElement{.popover = target_popover, .action = action};
}

Element* HTMLFormControlElement::interestTargetElement() {
  CHECK(RuntimeEnabledFeatures::HTMLInvokeTargetAttributeEnabled());

  if (!IsInTreeScope() || IsDisabledFormControl()) {
    return nullptr;
  }

  return GetElementAttribute(html_names::kInteresttargetAttr);
}

AtomicString HTMLFormControlElement::popoverTargetAction() const {
  auto attribute_value =
      FastGetAttribute(html_names::kPopovertargetactionAttr).LowerASCII();
  // ReflectEmpty="toggle", ReflectMissing="toggle"
  if (attribute_value.IsNull() || attribute_value.empty()) {
    return keywords::kToggle;
  } else if (attribute_value == keywords::kToggle ||
             attribute_value == keywords::kShow ||
             attribute_value == keywords::kHide) {
    return attribute_value;  // ReflectOnly
  } else if (RuntimeEnabledFeatures::HTMLPopoverActionHoverEnabled() &&
             attribute_value == keywords::kHover) {
    return attribute_value;  // ReflectOnly (with HTMLPopoverHint enabled)
  } else {
    return keywords::kToggle;  // ReflectInvalid = "toggle"
  }
}
void HTMLFormControlElement::setPopoverTargetAction(const AtomicString& value) {
  setAttribute(html_names::kPopovertargetactionAttr, value);
}

AtomicString HTMLFormControlElement::interestAction() const {
  CHECK(RuntimeEnabledFeatures::HTMLInterestTargetAttributeEnabled());
  const AtomicString& attribute_value =
      FastGetAttribute(html_names::kInterestactionAttr);
  if (attribute_value && !attribute_value.IsNull() &&
      !attribute_value.empty()) {
    return attribute_value;
  }
  return g_empty_atom;
}

void HTMLFormControlElement::DefaultEventHandler(Event& event) {
  // Buttons that aren't form participants might be Invoker buttons or Popover
  // buttons.
  if (event.type() == event_type_names::kDOMActivate && IsInTreeScope() &&
      !IsDisabledFormControl() && (!Form() || !IsSuccessfulSubmitButton())) {
    auto popover = popoverTargetElement();

    // CommandFor should have been handled in
    // HTMLButtonElement::DefaultEventHandler
    DCHECK(!IsA<HTMLButtonElement>(this) ||
           !DynamicTo<HTMLButtonElement>(this)->commandForElement());

    if (popover.popover) {
      // Buttons with a popovertarget will invoke popovers, which is the same
      // logic as an invoketarget with an appropriate command (e.g.
      // togglePopover), sans the `CommandEvent` dispatch. Calling
      // `HandleCommandInternal()` does not dispatch the event but can handle
      // the popover triggering logic. `popovertargetaction` must also be mapped
      // to the equivalent `command` string:
      //  popovertargetaction=toggle -> command=togglePopover
      //  popovertargetaction=show -> command=showPopover
      //  popovertargetaction=hide -> command=hidePopover
      // We must check to ensure the action is one of the available popover
      // invoker actions so that popovertargetaction cannot be set to something
      // like showModal.
      auto trigger_support = SupportsPopoverTriggering();
      CHECK_NE(trigger_support, PopoverTriggerSupport::kNone);
      CHECK_NE(popover.action, PopoverTriggerAction::kNone);
      CommandEventType action;

      switch (popover.action) {
        case PopoverTriggerAction::kToggle:
          action = CommandEventType::kTogglePopover;
          break;
        case PopoverTriggerAction::kShow:
          action = CommandEventType::kShowPopover;
          break;
        case PopoverTriggerAction::kHide:
          action = CommandEventType::kHidePopover;
          break;
        case PopoverTriggerAction::kHover:
          CHECK(RuntimeEnabledFeatures::HTMLPopoverActionHoverEnabled());
          action = CommandEventType::kShowPopover;
          break;
        case PopoverTriggerAction::kNone:
          NOTREACHED();
      }

      CHECK(popover.popover->IsValidBuiltinCommand(*this, action));
      popover.popover->HandleCommandInternal(*this, action);
    }
  }
  HTMLElement::DefaultEventHandler(event);
}

void HTMLFormControlElement::SetHovered(bool hovered) {
  HandlePopoverInvokerHovered(hovered);
  HTMLElement::SetHovered(hovered);
}

void HTMLFormControlElement::HandlePopoverInvokerHovered(bool hovered) {
  if (!IsInTreeScope()) {
    return;
  }
  if (auto* button = DynamicTo<HTMLButtonElement>(this)) {
    if (button->commandForElement()) {
      return;
    }
  }
  if (RuntimeEnabledFeatures::HTMLInvokeTargetAttributeEnabled() &&
      interestTargetElement()) {
    return;
  }
  auto target_info = popoverTargetElement();
  auto target_popover = target_info.popover;
  if (!target_popover || target_info.action != PopoverTriggerAction::kHover) {
    return;
  }
  CHECK(RuntimeEnabledFeatures::HTMLPopoverActionHoverEnabled());

  if (hovered) {
    // If we've just hovered an element (or the descendant of an element), see
    // if it has a popovertarget element set for hover triggering. If so, queue
    // a task to show the popover after a timeout.
    auto& hover_tasks = target_popover->GetPopoverData()->hoverShowTasks();
    CHECK(!hover_tasks.Contains(this));
    const ComputedStyle* computed_style = GetComputedStyle();
    if (!computed_style) {
      return;
    }
    float hover_delay_seconds = computed_style->PopoverShowDelay();
    // If the value is infinite or NaN, don't queue a task at all.
    CHECK_GE(hover_delay_seconds, 0);
    if (!std::isfinite(hover_delay_seconds)) {
      return;
    }
    // It's possible that multiple nested elements have popoverhovertarget
    // attributes pointing to the same popover, and in that case, we want to
    // trigger on the first of them that reaches its timeout threshold.
    hover_tasks.insert(
        this,
        PostDelayedCancellableTask(
            *GetExecutionContext()->GetTaskRunner(TaskType::kInternalDefault),
            FROM_HERE,
            WTF::BindOnce(
                [](HTMLFormControlElement* trigger_element,
                   HTMLElement* popover_element) {
                  if (!popover_element ||
                      !popover_element->HasPopoverAttribute()) {
                    return;
                  }
                  // Remove this element from hoverShowTasks always.
                  popover_element->GetPopoverData()->hoverShowTasks().erase(
                      trigger_element);
                  // Only trigger the popover if the popovertarget attribute
                  // still points to the same popover, and the popover is in the
                  // tree and still not showing.
                  auto current_target =
                      trigger_element->popoverTargetElement().popover;
                  if (popover_element->IsInTreeScope() &&
                      !popover_element->popoverOpen() &&
                      popover_element == current_target) {
                    popover_element->InvokePopover(*trigger_element);
                  }
                },
                WrapWeakPersistent(this),
                WrapWeakPersistent(target_popover.Get())),
            base::Seconds(hover_delay_seconds)));
  } else {
    // If we have a hover show task still waiting, cancel it. Based on this
    // logic, if you hover a popovertargetaction=hover element, then remove the
    // popovertarget attribute, there will be no way to stop the popover from
    // being shown after the delay, even if you subsequently de-hover the
    // element.
    if (auto& hover_tasks = target_popover->GetPopoverData()->hoverShowTasks();
        hover_tasks.Contains(this)) {
      hover_tasks.Take(this).Cancel();
    }
  }
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
    NodeCloningData& data) {
  HTMLElement::CloneNonAttributePropertiesFrom(source, data);
  SetNeedsValidityCheck();
}

void HTMLFormControlElement::AssociateWith(HTMLFormElement* form) {
  AssociateByParser(form);
}

int32_t HTMLFormControlElement::GetAxId() const {
  Document& document = GetDocument();
  if (!document.IsActive() || !document.View()) {
    return 0;
  }
  // The AXId is the same as the DOM node id.
  int32_t result = DOMNodeIds::ExistingIdForNode(this);
  CHECK(result) << "May need to call GetDomNodeId() from a non-const function";
  return result;
}

}  // namespace blink
