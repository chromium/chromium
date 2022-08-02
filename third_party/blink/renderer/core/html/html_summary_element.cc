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
#include "third_party/blink/renderer/core/layout/layout_object_factory.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

HTMLSummaryElement::HTMLSummaryElement(Document& document)
    : HTMLElement(html_names::kSummaryTag, document) {
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

    auto* keyboard_event = DynamicTo<KeyboardEvent>(event);
    if (keyboard_event) {
      if (event.type() == event_type_names::kKeydown &&
          keyboard_event->key() == " ") {
        SetActive(true);
        // No setDefaultHandled() - IE dispatches a keypress in this case.
        return;
      }
      if (event.type() == event_type_names::kKeypress) {
        switch (keyboard_event->charCode()) {
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
          keyboard_event->key() == " ") {
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

}  // namespace blink
