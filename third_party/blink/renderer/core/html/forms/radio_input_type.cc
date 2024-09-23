/*
 * Copyright (C) 2005, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/html/forms/radio_input_type.h"

#include "third_party/blink/public/mojom/input/focus_type.mojom-blink.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/keywords.h"
#include "third_party/blink/renderer/core/page/spatial_navigation.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"

namespace blink {

using mojom::blink::FormControlType;

namespace {

HTMLInputElement* NextInputElement(const HTMLInputElement& element,
                                   const HTMLFormElement* stay_within,
                                   bool forward) {
  return forward ? Traversal<HTMLInputElement>::Next(element, stay_within)
                 : Traversal<HTMLInputElement>::Previous(element, stay_within);
}

}  // namespace

void RadioInputType::CountUsage() {
  CountUsageIfVisible(WebFeature::kInputTypeRadio);
}

ControlPart RadioInputType::AutoAppearance() const {
  return kRadioPart;
}

bool RadioInputType::ValueMissing(const String&) const {
  HTMLInputElement& input = GetElement();
  if (auto* scope = input.GetRadioButtonGroupScope())
    return scope->IsInRequiredGroup(&input) && !CheckedRadioButtonForGroup();

  // This element is not managed by a RadioButtonGroupScope. We need to traverse
  // the tree from TreeRoot.
  DCHECK(!input.isConnected());
  DCHECK(!input.formOwner());
  const AtomicString& name = input.GetName();
  if (name.empty())
    return false;
  bool is_required = false;
  bool is_checked = false;
  Node& root = input.TreeRoot();
  for (auto* another = Traversal<HTMLInputElement>::InclusiveFirstWithin(root);
       another; another = Traversal<HTMLInputElement>::Next(*another, &root)) {
    if (another->FormControlType() != FormControlType::kInputRadio ||
        another->GetName() != name || another->formOwner()) {
      continue;
    }
    if (another->Checked())
      is_checked = true;
    if (another->FastHasAttribute(html_names::kRequiredAttr))
      is_required = true;
    if (is_checked && is_required)
      return false;
  }
  return is_required && !is_checked;
}

String RadioInputType::ValueMissingText() const {
  return GetLocale().QueryString(IDS_FORM_VALIDATION_VALUE_MISSING_RADIO);
}

void RadioInputType::HandleClickEvent(MouseEvent& event) {
  event.SetDefaultHandled();
}

HTMLInputElement* RadioInputType::FindNextFocusableRadioButtonInGroup(
    HTMLInputElement* current_element,
    bool forward) {
  for (HTMLInputElement* input_element =
           NextRadioButtonInGroup(current_element, forward);
       input_element;
       input_element = NextRadioButtonInGroup(input_element, forward)) {
    if (input_element->IsFocusable())
      return input_element;
  }
  return nullptr;
}

void RadioInputType::HandleKeydownEvent(KeyboardEvent& event) {
  // TODO(tkent): We should return more earlier.
  if (!GetElement().GetLayoutObject())
    return;
  BaseCheckableInputType::HandleKeydownEvent(event);
  if (event.DefaultHandled())
    return;
  const AtomicString key(event.key());
  if (key != keywords::kArrowUp && key != keywords::kArrowDown &&
      key != keywords::kArrowLeft && key != keywords::kArrowRight) {
    return;
  }

  if (event.ctrlKey() || event.metaKey() || event.altKey())
    return;

  // Left and up mean "previous radio button".
  // Right and down mean "next radio button".
  // Tested in WinIE, and even for RTL, left still means previous radio button
  // (and so moves to the right). Seems strange, but we'll match it. However,
  // when using Spatial Navigation, we need to be able to navigate without
  // changing the selection.
  Document& document = GetElement().GetDocument();
  if (IsSpatialNavigationEnabled(document.GetFrame()))
    return;
  bool forward =
      ComputedTextDirection() == TextDirection::kRtl
          ? (key == keywords::kArrowDown || key == keywords::kArrowLeft)
          : (key == keywords::kArrowDown || key == keywords::kArrowRight);

  // Force layout for isFocusable() in findNextFocusableRadioButtonInGroup().
  document.UpdateStyleAndLayout(DocumentUpdateReason::kInput);

  // We can only stay within the form's children if the form hasn't been demoted
  // to a leaf because of malformed HTML.
  HTMLInputElement* input_element =
      FindNextFocusableRadioButtonInGroup(&GetElement(), forward);
  if (!input_element) {
    // Traverse in reverse direction till last or first radio button
    forward = !(forward);
    HTMLInputElement* next_input_element =
        FindNextFocusableRadioButtonInGroup(&GetElement(), forward);
    while (next_input_element) {
      input_element = next_input_element;
      next_input_element =
          FindNextFocusableRadioButtonInGroup(next_input_element, forward);
    }
  }
  if (input_element) {
    document.SetFocusedElement(
        input_element, FocusParams(SelectionBehaviorOnFocus::kRestore,
                                   mojom::blink::FocusType::kNone, nullptr));
    input_element->DispatchSimulatedClick(&event);
    event.SetDefaultHandled();
    return;
  }
}

void RadioInputType::HandleKeyupEvent(KeyboardEvent& event) {
  // Use Space key simulated click by default.
  // Use Enter key simulated click when Spatial Navigation enabled.
  if (event.key() == " " ||
      (IsSpatialNavigationEnabled(GetElement().GetDocument().GetFrame()) &&
       event.key() == keywords::kCapitalEnter)) {
    // If an unselected radio is tabbed into (because the entire group has
    // nothing checked, or because of some explicit .focus() call), then allow
    // space to check it.
    if (GetElement().Checked()) {
      // If we are going to skip DispatchSimulatedClick, then at least call
      // SetActive(false) to prevent the radio from being stuck in the active
      // state.
      GetElement().SetActive(false);
    } else {
      DispatchSimulatedClickIfActive(event);
    }
  }
}

bool RadioInputType::IsKeyboardFocusable(
    Element::UpdateBehavior update_behavior) const {
  if (!InputType::IsKeyboardFocusable(update_behavior)) {
    return false;
  }

  // When using Spatial Navigation, every radio button should be focusable.
  if (IsSpatialNavigationEnabled(GetElement().GetDocument().GetFrame()))
    return true;

  // Never allow keyboard tabbing to leave you in the same radio group. Always
  // skip any other elements in the group.
  Element* current_focused_element =
      GetElement().GetDocument().FocusedElement();
  if (auto* focused_input =
          DynamicTo<HTMLInputElement>(current_focused_element)) {
    if (focused_input->FormControlType() == FormControlType::kInputRadio &&
        focused_input->GetTreeScope() == GetElement().GetTreeScope() &&
        focused_input->Form() == GetElement().Form() &&
        focused_input->GetName() == GetElement().GetName()) {
      return false;
    }
  }

  // Allow keyboard focus if we're checked or if nothing in the group is
  // checked.
  return GetElement().Checked() || !CheckedRadioButtonForGroup();
}

bool RadioInputType::ShouldSendChangeEventAfterCheckedChanged() {
  // Don't send a change event for a radio button that's getting unchecked.
  // This was done to match the behavior of other browsers.
  return GetElement().Checked();
}

ClickHandlingState* RadioInputType::WillDispatchClick() {
  // An event handler can use preventDefault or "return false" to reverse the
  // selection we do here.  The ClickHandlingState object contains what we need
  // to undo what we did here in didDispatchClick.

  // We want radio groups to end up in sane states, i.e., to have something
  // checked.  Therefore if nothing is currently selected, we won't allow the
  // upcoming action to be "undone", since we want some object in the radio
  // group to actually get selected.

  ClickHandlingState* state = MakeGarbageCollected<ClickHandlingState>();

  state->checked = GetElement().Checked();
  state->checked_radio_button = CheckedRadioButtonForGroup();
  GetElement().SetChecked(true, TextFieldEventBehavior::kDispatchChangeEvent);
  is_in_click_handler_ = true;
  return state;
}

void RadioInputType::DidDispatchClick(Event& event,
                                      const ClickHandlingState& state) {
  if (event.defaultPrevented() || event.DefaultHandled()) {
    // Restore the original selected radio button if possible.
    // Make sure it is still a radio button and only do the restoration if it
    // still belongs to our group.
    HTMLInputElement* checked_radio_button = state.checked_radio_button.Get();
    if (!checked_radio_button) {
      GetElement().SetChecked(false);
    } else if (checked_radio_button->FormControlType() ==
                   FormControlType::kInputRadio &&
               checked_radio_button->Form() == GetElement().Form() &&
               checked_radio_button->GetName() == GetElement().GetName()) {
      checked_radio_button->SetChecked(true);
    }
  } else if (state.checked != GetElement().Checked()) {
    GetElement().DispatchInputAndChangeEventIfNeeded();
  }
  is_in_click_handler_ = false;
  // The work we did in willDispatchClick was default handling.
  event.SetDefaultHandled();
}

bool RadioInputType::ShouldAppearIndeterminate() const {
  return !CheckedRadioButtonForGroup();
}

HTMLInputElement* RadioInputType::NextRadioButtonInGroup(
    HTMLInputElement* current,
    bool forward) {
  // TODO(https://crbug.com/323953913): Staying within form() is
  // incorrect.  This code ignore input elements associated by |form|
  // content attribute.
  // TODO(tkent): Comparing name() with == is incorrect.  It should be
  // case-insensitive.
  for (HTMLInputElement* input_element =
           NextInputElement(*current, current->Form(), forward);
       input_element; input_element = NextInputElement(
                          *input_element, current->Form(), forward)) {
    if (current->Form() == input_element->Form() &&
        input_element->GetTreeScope() == current->GetTreeScope() &&
        input_element->FormControlType() == FormControlType::kInputRadio &&
        input_element->GetName() == current->GetName()) {
      return input_element;
    }
  }
  return nullptr;
}

HTMLInputElement* RadioInputType::CheckedRadioButtonForGroup() const {
  HTMLInputElement& input = GetElement();
  if (input.Checked())
    return &input;
  if (auto* scope = input.GetRadioButtonGroupScope())
    return scope->CheckedButtonForGroup(input.GetName());

  // This element is not managed by a RadioButtonGroupScope. We need to traverse
  // the tree from TreeRoot.
  DCHECK(!input.isConnected());
  DCHECK(!input.formOwner());
  const AtomicString& name = input.GetName();
  if (name.empty())
    return nullptr;
  Node& root = input.TreeRoot();
  for (auto* another = Traversal<HTMLInputElement>::InclusiveFirstWithin(root);
       another; another = Traversal<HTMLInputElement>::Next(*another, &root)) {
    if (another->FormControlType() != FormControlType::kInputRadio ||
        another->GetName() != name || another->formOwner()) {
      continue;
    }
    if (another->Checked())
      return another;
  }
  return nullptr;
}

void RadioInputType::WillUpdateCheckedness(bool new_checked) {
  if (!new_checked)
    return;
  if (GetElement().GetRadioButtonGroupScope()) {
    // Buttons in RadioButtonGroupScope are handled in
    // HTMLInputElement::SetChecked().
    return;
  }
  if (auto* input = CheckedRadioButtonForGroup())
    input->SetChecked(false);
}

}  // namespace blink
