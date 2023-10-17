/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2010 Apple Inc. All rights reserved.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 * Copyright (C) 2007 Samuel Weinig (sam@webkit.org)
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

#include "third_party/blink/renderer/core/html/forms/html_button_element.h"

#include "third_party/blink/renderer/core/dom/attribute.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/simulated_click_options.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/forms/form_data.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_list_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/forms/layout_button.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

using mojom::blink::FormControlType;

HTMLButtonElement::HTMLButtonElement(Document& document)
    : HTMLFormControlElement(html_names::kButtonTag, document) {}

void HTMLButtonElement::setType(const AtomicString& type) {
  setAttribute(html_names::kTypeAttr, type);
}

LayoutObject* HTMLButtonElement::CreateLayoutObject(
    const ComputedStyle& style) {
  // https://html.spec.whatwg.org/C/#button-layout
  EDisplay display = style.Display();
  if (display == EDisplay::kInlineGrid || display == EDisplay::kGrid ||
      display == EDisplay::kInlineFlex || display == EDisplay::kFlex ||
      display == EDisplay::kInlineLayoutCustom ||
      display == EDisplay::kLayoutCustom)
    return HTMLFormControlElement::CreateLayoutObject(style);
  return MakeGarbageCollected<LayoutButton>(this);
}

FormControlType HTMLButtonElement::FormControlType() const {
  return static_cast<mojom::blink::FormControlType>(base::to_underlying(type_));
}

const AtomicString& HTMLButtonElement::FormControlTypeAsString() const {
  switch (type_) {
    case Type::kButton: {
      DEFINE_STATIC_LOCAL(const AtomicString, button, ("button"));
      return button;
    }
    case Type::kSubmit: {
      DEFINE_STATIC_LOCAL(const AtomicString, submit, ("submit"));
      return submit;
    }
    case Type::kReset: {
      DEFINE_STATIC_LOCAL(const AtomicString, reset, ("reset"));
      return reset;
    }
    case Type::kSelectlist: {
      if (RuntimeEnabledFeatures::HTMLSelectListElementEnabled()) {
        DEFINE_STATIC_LOCAL(const AtomicString, selectlist, ("selectlist"));
        return selectlist;
      }
    }
  }
  NOTREACHED_NORETURN();
}

bool HTMLButtonElement::IsPresentationAttribute(
    const QualifiedName& name) const {
  if (name == html_names::kAlignAttr) {
    // Don't map 'align' attribute.  This matches what Firefox and IE do, but
    // not Opera.  See http://bugs.webkit.org/show_bug.cgi?id=12071
    return false;
  }

  return HTMLFormControlElement::IsPresentationAttribute(name);
}

void HTMLButtonElement::ParseAttribute(
    const AttributeModificationParams& params) {
  if (params.name == html_names::kTypeAttr) {
    if (EqualIgnoringASCIICase(params.new_value, "reset")) {
      type_ = kReset;
    } else if (EqualIgnoringASCIICase(params.new_value, "button")) {
      type_ = kButton;
    } else if (RuntimeEnabledFeatures::HTMLSelectListElementEnabled() &&
               EqualIgnoringASCIICase(params.new_value, "selectlist")) {
      type_ = kSelectlist;
    } else {
      type_ = kSubmit;
    }
    UpdateWillValidateCache();
    if (formOwner() && isConnected())
      formOwner()->InvalidateDefaultButtonStyle();
  } else {
    if (params.name == html_names::kFormactionAttr)
      LogUpdateAttributeIfIsolatedWorldAndInDocument("button", params);
    HTMLFormControlElement::ParseAttribute(params);
  }
}

void HTMLButtonElement::DefaultEventHandler(Event& event) {
  if (event.type() == event_type_names::kDOMActivate) {
    if (!IsDisabledFormControl()) {
      if (Form() && type_ == kSubmit) {
        Form()->PrepareForSubmission(&event, this);
        event.SetDefaultHandled();
      }
      if (Form() && type_ == kReset) {
        Form()->reset();
        event.SetDefaultHandled();
      }
    }
  }

  if (type_ == kSelectlist) {
    CHECK(RuntimeEnabledFeatures::HTMLSelectListElementEnabled());
    if (auto* selectlist = OwnerSelectList()) {
      selectlist->HandleButtonEvent(event);
    }
  }

  // type=selectlist should not open the listbox when enter is pressed, which
  // HandleKeyboardActivation would do via simulated click.
  if (type_ != kSelectlist && HandleKeyboardActivation(event)) {
    return;
  }

  HTMLFormControlElement::DefaultEventHandler(event);
}

bool HTMLButtonElement::HasActivationBehavior() const {
  return true;
}

bool HTMLButtonElement::WillRespondToMouseClickEvents() {
  if (!IsDisabledFormControl() && Form() &&
      (type_ == kSubmit || type_ == kReset))
    return true;
  return HTMLFormControlElement::WillRespondToMouseClickEvents();
}

bool HTMLButtonElement::CanBeSuccessfulSubmitButton() const {
  return type_ == kSubmit;
}

bool HTMLButtonElement::IsActivatedSubmit() const {
  return is_activated_submit_;
}

void HTMLButtonElement::SetActivatedSubmit(bool flag) {
  is_activated_submit_ = flag;
}

void HTMLButtonElement::AppendToFormData(FormData& form_data) {
  if (type_ == kSubmit && !GetName().empty() && is_activated_submit_)
    form_data.AppendFromElement(GetName(), Value());
}

void HTMLButtonElement::AccessKeyAction(
    SimulatedClickCreationScope creation_scope) {
  Focus(FocusParams(FocusTrigger::kUserGesture));
  DispatchSimulatedClick(nullptr, creation_scope);
}

bool HTMLButtonElement::IsURLAttribute(const Attribute& attribute) const {
  return attribute.GetName() == html_names::kFormactionAttr ||
         HTMLFormControlElement::IsURLAttribute(attribute);
}

const AtomicString& HTMLButtonElement::Value() const {
  return FastGetAttribute(html_names::kValueAttr);
}

bool HTMLButtonElement::RecalcWillValidate() const {
  return type_ == kSubmit && HTMLFormControlElement::RecalcWillValidate();
}

int HTMLButtonElement::DefaultTabIndex() const {
  return 0;
}

bool HTMLButtonElement::IsInteractiveContent() const {
  return true;
}

bool HTMLButtonElement::MatchesDefaultPseudoClass() const {
  // HTMLFormElement::findDefaultButton() traverses the tree. So we check
  // canBeSuccessfulSubmitButton() first for early return.
  return CanBeSuccessfulSubmitButton() && Form() &&
         Form()->FindDefaultButton() == this;
}

Node::InsertionNotificationRequest HTMLButtonElement::InsertedInto(
    ContainerNode& insertion_point) {
  InsertionNotificationRequest request =
      HTMLFormControlElement::InsertedInto(insertion_point);
  LogAddElementIfIsolatedWorldAndInDocument("button", html_names::kTypeAttr,
                                            html_names::kFormmethodAttr,
                                            html_names::kFormactionAttr);
  return request;
}

void HTMLButtonElement::DispatchBlurEvent(
    Element* new_focused_element,
    mojom::blink::FocusType type,
    InputDeviceCapabilities* source_capabilities) {
  SetActive(false);
  HTMLFormControlElement::DispatchBlurEvent(new_focused_element, type,
                                            source_capabilities);
}

HTMLSelectListElement* HTMLButtonElement::OwnerSelectList() const {
  if (type_ != kSelectlist) {
    return nullptr;
  }
  for (auto& ancestor : FlatTreeTraversal::AncestorsOf(*this)) {
    if (IsA<HTMLListboxElement>(ancestor)) {
      // Buttons inside listboxes are excluded from triggering the listbox.
      return nullptr;
    }
    if (auto* selectlist = DynamicTo<HTMLSelectListElement>(ancestor)) {
      return selectlist;
    }
  }
  return nullptr;
}

}  // namespace blink
