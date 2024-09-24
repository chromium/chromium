/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2010 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/html/forms/html_label_element.h"

#include "third_party/blink/public/mojom/input/focus_type.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/events/simulated_click_options.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/selection_controller.h"
#include "third_party/blink/renderer/core/editing/visible_selection.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/custom/element_internals.h"
#include "third_party/blink/renderer/core/html/forms/html_button_element.h"
#include "third_party/blink/renderer/core/html/forms/html_form_control_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/forms/listed_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

HTMLLabelElement::HTMLLabelElement(Document& document)
    : HTMLElement(html_names::kLabelTag, document), processing_click_(false) {}

// For JavaScript binding, return the control element without resolving the
// reference target, to avoid exposing shadow root content to JS.
HTMLElement* HTMLLabelElement::controlForBinding() const {
  // https://html.spec.whatwg.org/C/#labeled-control
  const AtomicString& control_id = FastGetAttribute(html_names::kForAttr);
  if (control_id.IsNull()) {
    // "If the for attribute is not specified, but the label element has a
    // labelable element descendant, then the first such descendant in tree
    // order is the label element's labeled control."
    for (HTMLElement& element : Traversal<HTMLElement>::DescendantsOf(*this)) {
      if (element.IsLabelable()) {
        if (!element.IsFormControlElement()) {
          UseCounter::Count(
              GetDocument(),
              WebFeature::kHTMLLabelElementControlForNonFormAssociatedElement);
        }
        return &element;
      }
    }
    return nullptr;
  }

  if (!IsInTreeScope())
    return nullptr;

  if (Element* element = GetTreeScope().getElementById(control_id)) {
    if (auto* html_element = DynamicTo<HTMLElement>(*element)) {
      if (html_element->IsLabelable()) {
        if (!html_element->IsFormControlElement()) {
          UseCounter::Count(
              GetDocument(),
              WebFeature::kHTMLLabelElementControlForNonFormAssociatedElement);
        }
        return html_element;
      }
    }
  }

  return nullptr;
}

HTMLElement* HTMLLabelElement::Control() const {
  HTMLElement* control = controlForBinding();
  if (!control) {
    return nullptr;
  }

  if (auto* reference_target =
          control->GetShadowReferenceTarget(html_names::kForAttr)) {
    return DynamicTo<HTMLElement>(reference_target);
  }

  return control;
}

HTMLFormElement* HTMLLabelElement::form() const {
  if (HTMLElement* control = Control()) {
    if (auto* form_control_element = DynamicTo<HTMLFormControlElement>(control))
      return form_control_element->Form();
    if (control->IsFormAssociatedCustomElement())
      return control->EnsureElementInternals().Form();
  }
  return nullptr;
}

void HTMLLabelElement::SetActive(bool active) {
  if (active != IsActive()) {
    // Update our status first.
    HTMLElement::SetActive(active);
  }

  // Also update our corresponding control.
  HTMLElement* control_element = Control();
  if (control_element && control_element->IsActive() != IsActive())
    control_element->SetActive(IsActive());
}

void HTMLLabelElement::SetHovered(bool hovered) {
  if (hovered != IsHovered()) {
    // Update our status first.
    HTMLElement::SetHovered(hovered);
  }

  // Also update our corresponding control.
  HTMLElement* element = Control();
  if (element && element->IsHovered() != IsHovered())
    element->SetHovered(IsHovered());
}

bool HTMLLabelElement::IsInteractiveContent() const {
  return true;
}

bool HTMLLabelElement::IsInInteractiveContent(Node* node) const {
  if (!node || !IsShadowIncludingInclusiveAncestorOf(*node))
    return false;
  while (node && this != node) {
    auto* html_element = DynamicTo<HTMLElement>(node);
    if (html_element && html_element->IsInteractiveContent())
      return true;
    node = node->ParentOrShadowHostNode();
  }
  return false;
}

void HTMLLabelElement::DefaultEventHandler(Event& evt) {
  if (DefaultEventHandlerInternal(evt) ||
      RuntimeEnabledFeatures::LabelEventHandlerCallSuperEnabled()) {
    HTMLElement::DefaultEventHandler(evt);
  }
}

// If this returns false, then it means that we should not run
// HTMLElement::DefaultEventHandler when LabelEventHandlerCallSuper is disabled
// to emulate old behavior.
// TODO(crbug.com/1523168): Remove this method when the flag is removed.
bool HTMLLabelElement::DefaultEventHandlerInternal(Event& evt) {
  if (evt.type() == event_type_names::kClick && !processing_click_) {
    HTMLElement* element = Control();

    // If we can't find a control or if the control received the click
    // event, then there's no need for us to do anything.
    if (!element)
      return false;
    Node* target_node = evt.target() ? evt.target()->ToNode() : nullptr;
    if (target_node) {
      if (element->IsShadowIncludingInclusiveAncestorOf(*target_node))
        return false;

      if (IsInInteractiveContent(target_node))
        return false;
    }

    //   Behaviour of label element is as follows:
    //     - If there is double click, two clicks will be passed to control
    //       element. Control element will *not* be focused.
    //     - If there is selection of label element by dragging, no click
    //       event is passed. Also, no focus on control element.
    //     - If there is already a selection on label element and then label
    //       is clicked, then click event is passed to control element and
    //       control element is focused.

    bool is_label_text_selected = false;

    GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kInput);

    // If the click is not simulated and the text of the label element
    // is selected by dragging over it, then return without passing the
    // click event to control element.
    // Note: check if it is a MouseEvent because a click event may
    // not be an instance of a MouseEvent if created by document.createEvent().
    auto* mouse_event = DynamicTo<MouseEvent>(evt);
    if (mouse_event && mouse_event->HasPosition()) {
      if (LocalFrame* frame = GetDocument().GetFrame()) {
        // Check if there is a selection and click is not on the
        // selection.
        if (GetLayoutObject() && GetLayoutObject()->IsSelectable() &&
            frame->Selection().ComputeVisibleSelectionInDOMTree().IsRange() &&
            !frame->GetEventHandler()
                 .GetSelectionController()
                 .MouseDownWasSingleClickInSelection() &&
            target_node->CanStartSelection()) {
          is_label_text_selected = true;

          // If selection is there and is single click i.e. text is
          // selected by dragging over label text, then return.
          // Click count >=2, meaning double click or triple click,
          // should pass click event to control element.
          // Only in case of drag, *neither* we pass the click event,
          // *nor* we focus the control element.
          if (mouse_event->ClickCount() == 1)
            return false;
        }
      }
    }

    processing_click_ = true;
    if (element->IsMouseFocusable() ||
        (element->IsShadowHostWithDelegatesFocus() &&
         RuntimeEnabledFeatures::LabelAndDelegatesFocusNewHandlingEnabled())) {
      // If the label is *not* selected, or if the click happened on
      // selection of label, only then focus the control element.
      // In case of double click or triple click, selection will be there,
      // so do not focus the control element.
      if (!is_label_text_selected) {
        auto* select = DynamicTo<HTMLSelectElement>(element);
        if (RuntimeEnabledFeatures::CustomizableSelectEnabled() && select &&
            select->IsAppearanceBaseButton() && select->SlottedButton()) {
          // TODO(crbug.com/1511354): This is a workaround due to
          // GetFocusableArea/GetFocusDelegate not supporting slotted elements.
          // Once it is fixed, this can be removed.
          // https://github.com/whatwg/html/issues/9245#issuecomment-2098998865
          select->SlottedButton()->Focus(
              FocusParams(SelectionBehaviorOnFocus::kRestore,
                          mojom::blink::FocusType::kMouse, nullptr,
                          FocusOptions::Create()));
        } else {
          element->Focus(FocusParams(SelectionBehaviorOnFocus::kRestore,
                                     mojom::blink::FocusType::kMouse, nullptr,
                                     FocusOptions::Create()));
        }
      }
    }

    // Click the corresponding control.
    element->DispatchSimulatedClick(&evt);

    processing_click_ = false;

    evt.SetDefaultHandled();
  }

  return true;
}

bool HTMLLabelElement::HasActivationBehavior() const {
  return true;
}

bool HTMLLabelElement::WillRespondToMouseClickEvents() {
  if (Control() && Control()->WillRespondToMouseClickEvents()) {
    return true;
  }

  return HTMLElement::WillRespondToMouseClickEvents();
}

void HTMLLabelElement::Focus(const FocusParams& params) {
  GetDocument().UpdateStyleAndLayoutTreeForElement(
      this, DocumentUpdateReason::kFocus);
  if (IsFocusable()) {
    HTMLElement::Focus(params);
    return;
  }

  if (params.type == blink::mojom::blink::FocusType::kAccessKey)
    return;

  // To match other browsers, always restore previous selection.
  if (HTMLElement* element = Control()) {
    element->Focus(FocusParams(SelectionBehaviorOnFocus::kRestore, params.type,
                               params.source_capabilities, params.options,
                               params.focus_trigger));
  }
}

void HTMLLabelElement::AccessKeyAction(
    SimulatedClickCreationScope creation_scope) {
  if (HTMLElement* element = Control()) {
    element->AccessKeyAction(creation_scope);
  } else
    HTMLElement::AccessKeyAction(creation_scope);
}

}  // namespace blink
