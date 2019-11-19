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

#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/html/html_details_element.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/html/shadow/details_marker_control.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_object_factory.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

HTMLSummaryElement::HTMLSummaryElement(Document& document)
    : HTMLElement(html_names::kSummaryTag, document) {
  SetHasCustomStyleCallbacks();
  EnsureUserAgentShadowRoot();
}

LayoutObject* HTMLSummaryElement::CreateLayoutObject(const ComputedStyle& style,
                                                     LegacyLayout legacy) {
  // See: crbug.com/603928 - We manually check for other dislay types, then
  // fallback to a regular LayoutBlockFlow as "display: inline;" should behave
  // as an "inline-block".
  EDisplay display = style.Display();
  if (display == EDisplay::kFlex || display == EDisplay::kInlineFlex ||
      display == EDisplay::kGrid || display == EDisplay::kInlineGrid ||
      display == EDisplay::kLayoutCustom ||
      display == EDisplay::kInlineLayoutCustom)
    return LayoutObject::CreateObject(this, style, legacy);
  return LayoutObjectFactory::CreateBlockFlow(*this, style, legacy);
}

void HTMLSummaryElement::DidAddUserAgentShadowRoot(ShadowRoot& root) {
  auto* marker_control =
      MakeGarbageCollected<DetailsMarkerControl>(GetDocument());
  marker_control->SetIdAttribute(shadow_element_names::DetailsMarker());
  root.AppendChild(marker_control);
  root.AppendChild(HTMLSlotElement::CreateUserAgentDefaultSlot(GetDocument()));
}

HTMLDetailsElement* HTMLSummaryElement::DetailsElement() const {
  if (auto* details = DynamicTo<HTMLDetailsElement>(parentNode()))
    return details;
  if (auto* details = DynamicTo<HTMLDetailsElement>(OwnerShadowHost()))
    return details;
  return nullptr;
}

Element* HTMLSummaryElement::MarkerControl() {
  return EnsureUserAgentShadowRoot().getElementById(
      shadow_element_names::DetailsMarker());
}

bool HTMLSummaryElement::IsMainSummary() const {
  if (HTMLDetailsElement* details = DetailsElement())
    return details->FindMainSummary() == this;

  return false;
}

static bool IsClickableControl(Node* node) {
  auto* element = DynamicTo<Element>(node);
  if (!element)
    return false;
  if (element->IsFormControlElement())
    return true;
  Element* host = element->OwnerShadowHost();
  return host && host->IsFormControlElement();
}

bool HTMLSummaryElement::SupportsFocus() const {
  return IsMainSummary() || HTMLElement::SupportsFocus();
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

    if (event.IsKeyboardEvent()) {
      if (event.type() == event_type_names::kKeydown &&
          ToKeyboardEvent(event).key() == " ") {
        SetActive(true);
        // No setDefaultHandled() - IE dispatches a keypress in this case.
        return;
      }
      if (event.type() == event_type_names::kKeypress) {
        switch (ToKeyboardEvent(event).charCode()) {
          case '\r':
            DispatchSimulatedClick(&event);
            event.SetDefaultHandled();
            return;
          case ' ':
            // Prevent scrolling down the page.
            event.SetDefaultHandled();
            return;
        }
      }
      if (event.type() == event_type_names::kKeyup &&
          ToKeyboardEvent(event).key() == " ") {
        if (IsActive())
          DispatchSimulatedClick(&event);
        event.SetDefaultHandled();
        return;
      }
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

void HTMLSummaryElement::WillRecalcStyle(const StyleRecalcChange) {
  if (GetForceReattachLayoutTree() && IsMainSummary()) {
    if (Element* marker = MarkerControl()) {
      marker->SetNeedsStyleRecalc(
          kLocalStyleChange,
          StyleChangeReasonForTracing::Create(style_change_reason::kControl));
    }
  }
}

}  // namespace blink
