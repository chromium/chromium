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

#include <utility>

#include "third_party/blink/renderer/core/dom/attribute.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/simulated_click_options.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/core/events/command_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/forms/form_data.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
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
  if (style.IsVerticalWritingMode()) {
    UseCounter::Count(GetDocument(), WebFeature::kVerticalFormControls);
  }
  // https://html.spec.whatwg.org/C/#button-layout
  EDisplay display = style.Display();
  if (display == EDisplay::kInlineGrid || display == EDisplay::kGrid ||
      display == EDisplay::kInlineGridLanes ||
      display == EDisplay::kGridLanes || display == EDisplay::kInlineFlex ||
      display == EDisplay::kFlex || display == EDisplay::kInlineLayoutCustom ||
      display == EDisplay::kLayoutCustom) {
    return HTMLFormControlElement::CreateLayoutObject(style);
  }
  return MakeGarbageCollected<LayoutBlockFlow>(this);
}

void HTMLButtonElement::AdjustStyle(ComputedStyleBuilder& builder) {
  builder.SetShouldIgnoreOverflowPropertyForInlineBlockBaseline();
  builder.SetInlineBlockBaselineEdge(EInlineBlockBaselineEdge::kContentBox);
  HTMLFormControlElement::AdjustStyle(builder);
}

FormControlType HTMLButtonElement::FormControlType() const {
  return static_cast<mojom::blink::FormControlType>(std::to_underlying(type_));
}

const AtomicString& HTMLButtonElement::FormControlTypeAsString() const {
  switch (type_) {
    case Type::kButton: {
      DEFINE_STATIC_LOCAL(const AtomicString, button, (keywords::kButton));
      return button;
    }
    case Type::kSubmit: {
      DEFINE_STATIC_LOCAL(const AtomicString, submit, (keywords::kSubmit));
      return submit;
    }
    case Type::kReset: {
      DEFINE_STATIC_LOCAL(const AtomicString, reset, (keywords::kReset));
      return reset;
    }
  }
  NOTREACHED();
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

// static
std::optional<HTMLButtonElement::Type> HTMLButtonElement::TypeFromString(
    const AtomicString& string) {
  if (EqualIgnoringASCIICase(string, keywords::kReset)) {
    return kReset;
  }
  if (EqualIgnoringASCIICase(string, keywords::kButton)) {
    return kButton;
  }
  if (EqualIgnoringASCIICase(string, keywords::kSubmit)) {
    return kSubmit;
  }
  return std::nullopt;
}

void HTMLButtonElement::ParseAttribute(
    const AttributeModificationParams& params) {
  if (params.name == html_names::kTypeAttr) {
    if (std::optional<HTMLButtonElement::Type> type =
            TypeFromString(params.new_value)) {
      SetTypeInternal(*type);
    } else {
      if (!params.new_value.IsNull()) {
        if (params.new_value.empty()) {
          UseCounter::Count(GetDocument(),
                            WebFeature::kButtonTypeAttrEmptyString);
        } else {
          UseCounter::Count(GetDocument(), WebFeature::kButtonTypeAttrInvalid);
        }
      }
      if (FastHasAttribute(html_names::kCommandAttr) ||
          FastHasAttribute(html_names::kCommandforAttr)) {
        UseCounter::Count(
            GetDocument(),
            WebFeature::kButtonTypeAttrInvalidWithCommandOrCommandfor);
        SetTypeInternal(kButton);
      } else {
        SetTypeInternal(kSubmit);
      }
    }
    if (formOwner() && isConnected()) {
      formOwner()->InvalidateDefaultButtonStyle();
    }
  } else if (params.name == html_names::kCommandAttr ||
             params.name == html_names::kCommandforAttr) {
    bool has_type = FastHasAttribute(html_names::kTypeAttr);
    auto type = TypeFromString(FastGetAttribute(html_names::kTypeAttr));
    bool type_is_button = type && *type == kButton;
    if ((!has_type || !type_is_button)) {
      UseCounter::Count(
          GetDocument(),
          WebFeature::kButtonTypeAttrInvalidWithCommandOrCommandfor);
    }

    if (!params.new_value.IsNull() && !type) {
      // https://html.spec.whatwg.org/multipage/form-elements.html#dom-button-type
      // Type, as reflected in the IDL, must be "button" if there are command
      // attributes without an explicit valid type attribute set.
      SetTypeInternal(kButton);
    }
  } else {
    if (params.name == html_names::kFormactionAttr) {
      LogUpdateAttributeIfIsolatedWorldAndInDocument("button", params);
    }
    HTMLFormControlElement::ParseAttribute(params);
  }
}

void HTMLButtonElement::SetTypeInternal(Type type) {
  type_ = type;
  UpdateWillValidateCache();
  if (formOwner() && isConnected()) {
    formOwner()->InvalidateDefaultButtonStyle();
  }
}

bool HTMLButtonElement::CanBeCommandInvoker() const {
  return !IsFormAssociatedSubmitButton();
}

bool HTMLButtonElement::IsFormAssociatedSubmitButton() const {
  return Form() && FastHasAttribute(html_names::kTypeAttr) && type_ == kSubmit;
}

void HTMLButtonElement::DefaultEventHandler(Event& event) {
  if (event.type() == event_type_names::kDOMActivate) {
    if (auto* form = Form();
          form && !IsDisabledFormControl()) {
      bool has_command_attr = FastHasAttribute(html_names::kCommandforAttr) ||
                              FastHasAttribute(html_names::kCommandAttr);
      if (has_command_attr && type_ == kButton &&
          !EqualIgnoringASCIICase(FastGetAttribute(html_names::kTypeAttr),
                                  keywords::kButton)) {
        AddConsoleMessage(mojom::blink::ConsoleMessageSource::kOther,
                          mojom::blink::ConsoleMessageLevel::kWarning,
                          "Buttons associated with forms that include "
                          "command or commandfor attributes are "
                          "ambiguous, and require a type=button attribute. "
                          "No action will be taken.");
        return;
      }
      if (type_ == kSubmit) {
        if (has_command_attr &&
            EqualIgnoringASCIICase(FastGetAttribute(html_names::kTypeAttr),
                                   keywords::kSubmit)) {
          AddConsoleMessage(
              mojom::blink::ConsoleMessageSource::kOther,
              mojom::blink::ConsoleMessageLevel::kWarning,
              "Buttons with an explicit type=submit will always submit a "
              "form, so command or commandfor attributes will be ignored.");
        }
        form->PrepareForSubmission(&event, this);
        event.SetDefaultHandled();
        return;
      }
      if (type_ == kReset) {
        form->reset();
        event.SetDefaultHandled();
        if (has_command_attr) {
          AddConsoleMessage(
              mojom::blink::ConsoleMessageSource::kOther,
              mojom::blink::ConsoleMessageLevel::kWarning,
              "Buttons with an explicit type=reset will always reset a form, "
              "so command or commandfor attributes will be ignored.");
        }
        return;
      }
    }
  }

  if (event.type() == event_type_names::kDOMActivate) {
    if (HandleCommandForActivation()) {
      return;
    }
  }

  if (HandleKeyboardActivation(event)) {
    return;
  }
  HTMLFormControlElement::DefaultEventHandler(event);
}

bool HTMLButtonElement::HasActivationBehavior() const {
  return true;
}

bool HTMLButtonElement::WillRespondToMouseClickEvents() {
  if (!IsDisabledFormControl() && Form() &&
      (type_ == kSubmit || type_ == kReset)) {
    return true;
  }
  return HTMLFormControlElement::WillRespondToMouseClickEvents();
}

bool HTMLButtonElement::CanBeSuccessfulSubmitButton() const {
  return type_ == kSubmit && !OwnerSelect();
}

bool HTMLButtonElement::IsActivatedSubmit() const {
  return is_activated_submit_;
}

void HTMLButtonElement::SetActivatedSubmit(bool flag) {
  is_activated_submit_ = flag;
}

void HTMLButtonElement::AppendToFormData(FormData& form_data) {
  if (type_ == kSubmit && !GetName().empty() && is_activated_submit_) {
    form_data.AppendFromElement(GetName(), Value());
  }
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
  // The button might be the control element of a label
  // that is in :active state. In that case the control should
  // remain :active to avoid crbug.com/40934455.
  if (!HasActiveLabel()) {
    SetActive(false);
  }
  HTMLFormControlElement::DispatchBlurEvent(new_focused_element, type,
                                            source_capabilities);
}

HTMLSelectElement* HTMLButtonElement::OwnerSelect() const {
  if (auto* select = DynamicTo<HTMLSelectElement>(parentNode())) {
    if (select->SlottedButton() == this) {
      return select;
    }
  }
  return nullptr;
}

}  // namespace blink
