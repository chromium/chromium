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
  // https://html.spec.whatwg.org/C/#button-layout
  EDisplay display = style.Display();
  if (display == EDisplay::kInlineGrid || display == EDisplay::kGrid ||
      display == EDisplay::kInlineFlex || display == EDisplay::kFlex ||
      display == EDisplay::kInlineLayoutCustom ||
      display == EDisplay::kLayoutCustom)
    return HTMLFormControlElement::CreateLayoutObject(style);
  return MakeGarbageCollected<LayoutBlockFlow>(this);
}

void HTMLButtonElement::AdjustStyle(ComputedStyleBuilder& builder) {
  builder.SetShouldIgnoreOverflowPropertyForInlineBlockBaseline();
  builder.SetInlineBlockBaselineEdge(EInlineBlockBaselineEdge::kContentBox);
  HTMLFormControlElement::AdjustStyle(builder);
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

void HTMLButtonElement::ParseAttribute(
    const AttributeModificationParams& params) {
  if (params.name == html_names::kTypeAttr) {
    if (EqualIgnoringASCIICase(params.new_value, "reset")) {
      type_ = kReset;
    } else if (EqualIgnoringASCIICase(params.new_value, "button")) {
      type_ = kButton;
    } else {
      if (!params.new_value.IsNull()) {
        if (params.new_value.empty()) {
          UseCounter::Count(GetDocument(),
                            WebFeature::kButtonTypeAttrEmptyString);
        } else if (!EqualIgnoringASCIICase(params.new_value, "submit")) {
          UseCounter::Count(GetDocument(), WebFeature::kButtonTypeAttrInvalid);
        }
      }
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
      if (Form() && type_ == kSubmit && !OwnerSelect()) {
        Form()->PrepareForSubmission(&event, this);
        event.SetDefaultHandled();
        return;
      }
      if (Form() && type_ == kReset) {
        Form()->reset();
        event.SetDefaultHandled();
        return;
      }
    }
  }

  if (auto* select = OwnerSelect()) {
    CHECK(RuntimeEnabledFeatures::CustomizableSelectEnabled());
    // For native popups, use HTMLSelectElement's codepath. For <datalist>
    // popover popups, use the HTMLFormControlElement popover code path.
    if (select->IsAppearanceBaseButton()) {
      CHECK(!event.DefaultHandled())
          << " We shouldn't run HTMLSelectElement::DefaultEventHandler here if "
             "the default has already been handled. event.type(): "
          << event.type();
      select->DefaultEventHandler(event);
      if (event.DefaultHandled()) {
        return;
      }
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
      (type_ == kSubmit || type_ == kReset))
    return true;
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
  // The button might be the control element of a label
  // that is in :active state. In that case the control should
  // remain :active to avoid crbug.com/40934455.
  if (!RuntimeEnabledFeatures::KeepActiveIfLabelActiveEnabled() ||
      !HasActiveLabel()) {
    SetActive(false);
  }
  HTMLFormControlElement::DispatchBlurEvent(new_focused_element, type,
                                            source_capabilities);
}

HTMLSelectElement* HTMLButtonElement::OwnerSelect() const {
  if (!RuntimeEnabledFeatures::CustomizableSelectEnabled()) {
    return nullptr;
  }
  if (auto* select = DynamicTo<HTMLSelectElement>(parentNode())) {
    for (auto* previous_sibling = previousSibling(); previous_sibling;
         previous_sibling = previous_sibling->previousSibling()) {
      if (IsA<HTMLButtonElement>(previous_sibling)) {
        // Only the first child <button> of a <select>, which is the one that
        // gets slotted into the button slot, should get the <select> opening
        // behavior.
        return nullptr;
      }
    }
    return select;
  }
  if (auto* root = ContainingShadowRoot()) {
    if (auto* select = DynamicTo<HTMLSelectElement>(root->host())) {
      return select;
    }
  }
  return nullptr;
}

}  // namespace blink
