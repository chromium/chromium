/*
 * Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies)
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

#include "third_party/blink/renderer/core/html/html_summary_element.h"

#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/html/html_details_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

HTMLSummaryElement::HTMLSummaryElement(Document& document)
    : HTMLElement(html_names::kSummaryTag, document) {
}

LayoutObject* HTMLSummaryElement::CreateLayoutObject(
    const ComputedStyle& style) {
  if (RuntimeEnabledFeatures::DetailsStylingEnabled()) {
    return HTMLElement::CreateLayoutObject(style);
  }

  // See: crbug.com/603928 - We manually check for other display types, then
  // fallback to a regular LayoutBlockFlow as "display: inline;" should behave
  // as an "inline-block".
  EDisplay display = style.Display();
  if (display == EDisplay::kFlex || display == EDisplay::kInlineFlex ||
      display == EDisplay::kGrid || display == EDisplay::kInlineGrid ||
      display == EDisplay::kLayoutCustom ||
      display == EDisplay::kInlineLayoutCustom)
    return LayoutObject::CreateObject(this, style);
  return LayoutObject::CreateBlockFlowOrListItem(this, style);
}

HTMLDetailsElement* HTMLSummaryElement::DetailsElement() const {
  if (auto* details = DynamicTo<HTMLDetailsElement>(parentNode()))
    return details;
  if (auto* details = DynamicTo<HTMLDetailsElement>(OwnerShadowHost()))
    return details;
  return nullptr;
}

bool HTMLSummaryElement::IsMainSummary() const {
  if (HTMLDetailsElement* details = DetailsElement())
    return details->FindMainSummary() == this;

  return false;
}

FocusableState HTMLSummaryElement::SupportsFocus(
    UpdateBehavior update_behavior) const {
  if (IsMainSummary()) {
    return FocusableState::kFocusable;
  }
  return HTMLElement::SupportsFocus(update_behavior);
}

int HTMLSummaryElement::DefaultTabIndex() const {
  return IsMainSummary() ? 0 : -1;
}

void HTMLSummaryElement::DefaultEventHandler(Event& event) {
  if (IsMainSummary()) {
    if (event.type() == event_type_names::kDOMActivate &&
        !IsClickableControl(event.target()->ToNode())) {
      if (HTMLDetailsElement* details = DetailsElement())
        details->ToggleOpen();
      event.SetDefaultHandled();
      return;
    }

    if (HandleKeyboardActivation(event)) {
      return;
    }
  }

  HTMLElement::DefaultEventHandler(event);
}

bool HTMLSummaryElement::HasActivationBehavior() const {
  return true;
}

bool HTMLSummaryElement::WillRespondToMouseClickEvents() {
  return IsMainSummary() || HTMLElement::WillRespondToMouseClickEvents();
}

}  // namespace blink
