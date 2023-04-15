// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/css_toggle_key_handling.h"

#include "third_party/blink/renderer/core/dom/css_toggle.h"
#include "third_party/blink/renderer/core/dom/css_toggle_inference.h"
#include "third_party/blink/renderer/core/dom/css_toggle_map.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/toggle_trigger.h"
#include "third_party/blink/renderer/core/style/toggle_trigger_list.h"

namespace blink {

namespace {

const ToggleTrigger* FindToggleTrigger(Element* element,
                                       const AtomicString& toggle_name) {
  const ComputedStyle* style = element->GetComputedStyle();
  if (!style) {
    return nullptr;
  }
  const ToggleTriggerList* trigger_list = style->ToggleTrigger();
  if (!trigger_list) {
    return nullptr;
  }
  for (const ToggleTrigger& trigger : trigger_list->Triggers()) {
    // TODO(https://crbug.com/1250716): Should consider what to do if
    // the element has more than one trigger for the toggle we're
    // looking for.
    if (trigger.Name() == toggle_name) {
      return &trigger;
    }
  }

  return nullptr;
}

bool ActivateToggle(Element* element, const AtomicString& toggle_name) {
  // Check (somewhat defensively, but it can probably happen) that the
  // element has a toggle named toggle_name, so that we don't activate
  // some random toggle on an ancestor.
  const ToggleTrigger* element_trigger =
      FindToggleTrigger(element, toggle_name);
  if (!element_trigger) {
    return false;
  }

  // Make a copy of the trigger since FireToggleActivation might change
  // style.
  // TODO(https://crbug.com/1250716): Is always using the trigger
  // specified the right thing here?  Might we want something that
  // varies by role?
  ToggleTrigger trigger(*element_trigger);
  return CSSToggle::FireToggleActivation(*element, trigger);
}

// TODO(https://crbug.com/1250716): Use this when adding
// arrows-to-open/close, and for handling of focusing of a popup when
// opening it.
#if 0
bool IsToggleActive(Element* element, const AtomicString& toggle_name) {
  CSSToggle* toggle = CSSToggle::FindToggleInScope(*element, toggle_name);
  if (!toggle) {
    return false;
  }

  return toggle->ValueIsActive();
}
#endif

}  // namespace

namespace css_toggle_key_handling {

bool HandleKeydownEvent(Element* element, KeyboardEvent& event) {
  bool handled = false;
  DCHECK_EQ(event.type(), event_type_names::kKeydown);

  if (CSSToggleInference* toggle_inference =
          element->GetDocument().GetCSSToggleInference()) {
    AtomicString toggle_name = toggle_inference->ToggleNameForElement(element);
    CSSToggleRole toggle_role = toggle_inference->RoleForElement(element);

    // For many roles, space or enter should activate the toggle.
    // Handle that in a separate switch so we don't need to repeat it
    // below.
    switch (toggle_role) {
      case CSSToggleRole::kNone:
      case CSSToggleRole::kAccordion:
      case CSSToggleRole::kAccordionItem:
      case CSSToggleRole::kCheckboxGroup:
      case CSSToggleRole::kDisclosure:
      case CSSToggleRole::kListbox:
      case CSSToggleRole::kRadioGroup:
      case CSSToggleRole::kTabContainer:
      case CSSToggleRole::kTabPanel:
      case CSSToggleRole::kTree:
      case CSSToggleRole::kTreeGroup:
      case CSSToggleRole::kTreeItem:
        break;

      case CSSToggleRole::kCheckbox:
      case CSSToggleRole::kRadioItem:
        // Checkboxes and radios should be activated by space but not
        // enter.
        if (event.key() == " ") {
          if (ActivateToggle(element, toggle_name)) {
            return true;
          }
        }
        break;

      case CSSToggleRole::kAccordionItemButton:
      case CSSToggleRole::kButtonWithPopup:
      case CSSToggleRole::kDisclosureButton:
      case CSSToggleRole::kButton:
      case CSSToggleRole::kListboxItem:
      case CSSToggleRole::kTab: {
        if (event.key() == " " || event.key() == "Enter") {
          if (ActivateToggle(element, toggle_name)) {
            return true;
          }
        }
        break;
      }
    }

    switch (toggle_role) {
      case CSSToggleRole::kNone:
      case CSSToggleRole::kAccordion:
      case CSSToggleRole::kAccordionItem:
      case CSSToggleRole::kCheckboxGroup:
      case CSSToggleRole::kDisclosure:
      case CSSToggleRole::kListbox:
      case CSSToggleRole::kRadioGroup:
      case CSSToggleRole::kTabContainer:
      case CSSToggleRole::kTabPanel:
        // No special keyboard handling.

        // Accordion items could optionally have up/down and home/end handling.
        break;

      case CSSToggleRole::kTree:
      case CSSToggleRole::kTreeGroup:
      case CSSToggleRole::kTreeItem:
        // TODO(https://crbug.com/1250716): Figure out keyboard handling.
        break;

      case CSSToggleRole::kAccordionItemButton: {
        // Note that for these roles, the toggle root might not be
        // element.
        //
        // Activation is handled above.

        // TODO(https://crbug.com/1250716): handle arrows and home/end.
        break;
      }

      case CSSToggleRole::kButton:
      case CSSToggleRole::kButtonWithPopup:
      case CSSToggleRole::kDisclosureButton: {
        // Note that for button, we expect element to be the toggle
        // root, but that may not be true for button with popup or
        // disclosure button.
        //
        // Activation is handled above.
        //
        // TODO(https://crbug.com/1250716): Button with popup, when
        // opening the popup, should move focus into the popup.
        break;
      }

      case CSSToggleRole::kCheckbox:
      case CSSToggleRole::kListboxItem:
      case CSSToggleRole::kRadioItem:
      case CSSToggleRole::kTab: {
        // Activation is handled above.
        //
        // Arrow keys are handled by
        // FocusgroupController::HandleArrowKeyboardEvent .
        //
        // TODO(https://crbug.com/1250716): Handle home/end, at least
        // for some of these.
        break;
      }
    }
  }

  return handled;
}

}  // namespace css_toggle_key_handling

}  // namespace blink
