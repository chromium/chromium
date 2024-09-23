/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All
 * rights reserved.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 * Copyright (C) 2007 Samuel Weinig (sam@webkit.org)
 * Copyright (C) 2009, 2010, 2011, 2012 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/html/forms/input_type_view.h"

#include "third_party/blink/renderer/core/dom/events/simulated_click_options.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/html/forms/form_controller.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"

namespace blink {

void InputTypeView::WillBeDestroyed() {
  will_be_destroyed_ = true;
}

InputTypeView::~InputTypeView() = default;

void InputTypeView::Trace(Visitor* visitor) const {
  visitor->Trace(element_);
}

bool InputTypeView::SizeShouldIncludeDecoration(int,
                                                int& preferred_size) const {
  preferred_size = GetElement().size();
  return false;
}

void InputTypeView::HandleClickEvent(MouseEvent&) {}

void InputTypeView::HandleMouseDownEvent(MouseEvent&) {}

void InputTypeView::HandleKeydownEvent(KeyboardEvent&) {}

void InputTypeView::HandleKeypressEvent(KeyboardEvent&) {}

void InputTypeView::HandleKeyupEvent(KeyboardEvent&) {}

void InputTypeView::HandleBeforeTextInsertedEvent(BeforeTextInsertedEvent&) {}

void InputTypeView::HandleDOMActivateEvent(Event&) {}

void InputTypeView::ForwardEvent(Event&) {}

void InputTypeView::DispatchSimulatedClickIfActive(KeyboardEvent& event) const {
  if (GetElement().IsActive())
    GetElement().DispatchSimulatedClick(&event);
  event.SetDefaultHandled();
}

void InputTypeView::AccessKeyAction(SimulatedClickCreationScope) {
  GetElement().Focus(FocusParams(
      SelectionBehaviorOnFocus::kReset, mojom::blink::FocusType::kNone, nullptr,
      FocusOptions::Create(), FocusTrigger::kUserGesture));
}

bool InputTypeView::ShouldSubmitImplicitly(const Event& event) {
  auto* keyboard_event = DynamicTo<KeyboardEvent>(event);
  return keyboard_event && event.type() == event_type_names::kKeypress &&
         keyboard_event->charCode() == '\r';
}

HTMLFormElement* InputTypeView::FormForSubmission() const {
  return GetElement().Form();
}

LayoutObject* InputTypeView::CreateLayoutObject(
    const ComputedStyle& style) const {
  // Avoid LayoutInline, which can be split to multiple lines.
  if (style.IsDisplayInlineType() && !style.IsDisplayReplacedType()) {
    return MakeGarbageCollected<LayoutBlockFlow>(&GetElement());
  }
  return LayoutObject::CreateObject(&GetElement(), style);
}

ControlPart InputTypeView::AutoAppearance() const {
  return kNoControlPart;
}

TextDirection InputTypeView::ComputedTextDirection() {
  return GetElement().ComputedStyleRef().Direction();
}

void InputTypeView::Blur() {
  GetElement().DefaultBlur();
}

bool InputTypeView::HasCustomFocusLogic() const {
  return true;
}

void InputTypeView::HandleBlurEvent() {}

void InputTypeView::HandleFocusInEvent(Element*, mojom::blink::FocusType) {}

void InputTypeView::OpenPopupView() {}

void InputTypeView::ClosePopupView() {}

bool InputTypeView::HasOpenedPopup() const {
  return false;
}

bool InputTypeView::NeedsShadowSubtree() const {
  return true;
}

void InputTypeView::CreateShadowSubtree() {}

void InputTypeView::CreateShadowSubtreeIfNeeded(bool is_type_changing) {
  if (has_created_shadow_subtree_ || !NeedsShadowSubtree()) {
    return;
  }
  GetElement().EnsureUserAgentShadowRoot();
  has_created_shadow_subtree_ = true;
  CreateShadowSubtree();
  // When called and the type is changing, HTMLInputElement's internal state may
  // not fully be up to date, so that it's problematic to do the following.
  // Additionally the following is not necessary when the type is changing,
  // because HTMLInputElement effectively has similar logic.
  if (RuntimeEnabledFeatures::CreateInputShadowTreeDuringLayoutEnabled() &&
      !is_type_changing) {
    if (needs_update_view_in_create_shadow_subtree_) {
      UpdateView();
    }
    // When CreateInputShadowTreeDuringLayoutEnabled is true, placeholder
    // updates are ignored. Update now if needed.
    if (!GetElement().SuggestedValue().empty() ||
        GetElement().FastHasAttribute(html_names::kPlaceholderAttr)) {
      GetElement().UpdatePlaceholderVisibility();
      if (auto* placeholder = GetElement().PlaceholderElement()) {
        GetElement().UpdatePlaceholderShadowPseudoId(*placeholder);
      }
    }
  }
  needs_update_view_in_create_shadow_subtree_ = false;
}

void InputTypeView::DestroyShadowSubtree() {
  if (ShadowRoot* root = GetElement().UserAgentShadowRoot())
    root->RemoveChildren();
}

HTMLInputElement* InputTypeView::UploadButton() const {
  return nullptr;
}

String InputTypeView::FileStatusText() const {
  return String();
}

void InputTypeView::AltAttributeChanged() {}

void InputTypeView::SrcAttributeChanged() {}

void InputTypeView::MinOrMaxAttributeChanged() {}

void InputTypeView::StepAttributeChanged() {}

ClickHandlingState* InputTypeView::WillDispatchClick() {
  return nullptr;
}

void InputTypeView::DidDispatchClick(Event&, const ClickHandlingState&) {}

void InputTypeView::UpdateView() {}

void InputTypeView::MultipleAttributeChanged() {}

void InputTypeView::DisabledAttributeChanged() {}

void InputTypeView::ReadonlyAttributeChanged() {}

void InputTypeView::RequiredAttributeChanged() {}

void InputTypeView::ValueAttributeChanged() {}

void InputTypeView::DidSetValue(const String&, bool) {}

void InputTypeView::SubtreeHasChanged() {
  NOTREACHED_IN_MIGRATION();
}

void InputTypeView::ListAttributeTargetChanged() {}

void InputTypeView::CapsLockStateMayHaveChanged() {}

bool InputTypeView::ShouldDrawCapsLockIndicator() const {
  return false;
}

void InputTypeView::UpdateClearButtonVisibility() {}

HTMLElement* InputTypeView::UpdatePlaceholderText(bool) {
  return nullptr;
}

AXObject* InputTypeView::PopupRootAXObject() {
  return nullptr;
}

FormControlState InputTypeView::SaveFormControlState() const {
  String current_value = GetElement().Value();
  if (current_value == GetElement().DefaultValue())
    return FormControlState();
  return FormControlState(current_value);
}

void InputTypeView::RestoreFormControlState(const FormControlState& state) {
  GetElement().SetValue(state[0]);
}

bool InputTypeView::IsDraggedSlider() const {
  return false;
}

bool InputTypeView::HasBadInput() const {
  return false;
}

void ClickHandlingState::Trace(Visitor* visitor) const {
  visitor->Trace(checked_radio_button);
  EventDispatchHandlingState::Trace(visitor);
}

}  // namespace blink
