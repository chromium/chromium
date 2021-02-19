// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/html_select_menu_element.h"

#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/forms/html_button_element.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/html_popup_element.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

HTMLSelectMenuElement::HTMLSelectMenuElement(Document& document)
    : HTMLElement(html_names::kSelectmenuTag, document) {
  DCHECK(RuntimeEnabledFeatures::HTMLSelectMenuElementEnabled());
  DCHECK(RuntimeEnabledFeatures::HTMLPopupElementEnabled());
  UseCounter::Count(document, WebFeature::kSelectMenuElement);

  // TODO(crbug.com/1121840) This should really be a user-agent shadow root.
  // But, these don't support name-based assignment (see
  // ShouldAssignToCustomSlot). Perhaps names-based slot assignment can be added
  // to user-agent shadows? See crbug.com/1179356.
  AttachShadowRootInternal(ShadowRootType::kClosed);

  CreateShadowSubtree();
}

void HTMLSelectMenuElement::CreateShadowSubtree() {
  DCHECK(IsShadowHost(this));

  Document& document = this->GetDocument();

  // TODO(crbug.com/1121840) Where to put the styles for the default elements in
  // the shadow tree? We'd like to have them in the UA styles (html.css), but
  // the -webkit pseudo-id selectors only work if this is a UA shadow DOM.  We
  // can't use a UA shadow DOMs because these don't currently support named
  // slots. For now, just set the style attributes with raw inline strings, but
  // we should be able to do something better than this. Probably the solution
  // is to get named slots working in UA shadow DOM (crbug.com/1179356), and
  // then we can switch to that and use the -webkit pseudo-id selectors.

  auto* button_slot = MakeGarbageCollected<HTMLSlotElement>(document);
  slotchange_listener_ =
      MakeGarbageCollected<HTMLSelectMenuElement::SlotChangeEventListener>(
          this);
  button_slot->addEventListener(event_type_names::kSlotchange,
                                slotchange_listener_, false);
  button_slot->setAttribute(html_names::kNameAttr, kButtonPartName);

  button_part_ = MakeGarbageCollected<HTMLButtonElement>(document);
  button_part_->setAttribute(html_names::kPartAttr, kButtonPartName);
  button_part_->setAttribute(html_names::kStyleAttr,
                             R"CSS(
      display: inline-flex;
      align-items: center;
      background-color: #ffffff;
      padding: 0 0 0 3px;
      border: 1px solid #767676;
      border-radius: 2px;
      cursor: default;
  )CSS");
  button_part_listener_ =
      MakeGarbageCollected<HTMLSelectMenuElement::ButtonPartEventListener>(
          this);
  button_part_->addEventListener(event_type_names::kClick,
                                 button_part_listener_, false);

  selected_value_part_ = MakeGarbageCollected<HTMLDivElement>(document);
  selected_value_part_->setAttribute(html_names::kPartAttr,
                                     kSelectedValuePartName);

  auto* button_icon = MakeGarbageCollected<HTMLDivElement>(document);
  button_icon->setAttribute(html_names::kStyleAttr,
                            R"CSS(
    background-image: url(
      'data:image/svg+xml,\
      <svg width="20" height="14" viewBox="0 0 20 16" fill="none" xmlns="http://www.w3.org/2000/svg">\
        <path d="M4 6 L10 12 L 16 6" stroke="WindowText" stroke-width="3" stroke-linejoin="round"/>\
      </svg>');
    background-origin: content-box;
    background-repeat: no-repeat;
    background-size: contain;
    height: 1.0em;
    margin-inline-start: 4px;
    opacity: 1;
    outline: none;
    padding-bottom: 2px;
    padding-inline-start: 3px;
    padding-inline-end: 3px;
    padding-top: 2px;
    width: 1.2em;
    )CSS");

  auto* listbox_slot = MakeGarbageCollected<HTMLSlotElement>(document);
  listbox_slot->addEventListener(event_type_names::kSlotchange,
                                 slotchange_listener_, false);
  listbox_slot->setAttribute(html_names::kNameAttr, kListboxPartName);

  listbox_part_ = MakeGarbageCollected<HTMLPopupElement>(document);
  listbox_part_->setAttribute(html_names::kPartAttr, kListboxPartName);

  auto* options_slot = MakeGarbageCollected<HTMLSlotElement>(document);
  options_slot->addEventListener(event_type_names::kSlotchange,
                                 slotchange_listener_, false);

  button_part_->AppendChild(selected_value_part_);
  button_part_->AppendChild(button_icon);

  button_slot->AppendChild(button_part_);

  listbox_part_->appendChild(options_slot);
  listbox_slot->appendChild(listbox_part_);

  this->GetShadowRoot()->AppendChild(button_slot);
  this->GetShadowRoot()->AppendChild(listbox_slot);

  option_part_listener_ =
      MakeGarbageCollected<HTMLSelectMenuElement::OptionPartEventListener>(
          this);
}

String HTMLSelectMenuElement::value() const {
  if (selected_option_) {
    return selected_option_->innerText();
  }
  return "";
}

void HTMLSelectMenuElement::setValue(const String& value, bool send_events) {
  // Find the option with innerText matching the given parameter and make it the
  // current selection.
  for (auto& option : option_parts_) {
    if (option->innerText() == value) {
      SetSelectedOption(option);
      break;
    }
  }
}

bool HTMLSelectMenuElement::IsOpen() const {
  // TODO(crbug.com/1121840) listbox_part_ can be null if
  // the author has filled the listbox slot without including
  // a replacement listbox part. Instead of null checks like this,
  // we should consider refusing to render the control at all if
  // either of the key parts (button or listbox) are missing.
  return listbox_part_ != nullptr && listbox_part_->open();
}

void HTMLSelectMenuElement::Open() {
  if (listbox_part_ != nullptr && !IsOpen()) {
    listbox_part_->show();
  }
}

void HTMLSelectMenuElement::Close() {
  if (listbox_part_ != nullptr && IsOpen()) {
    listbox_part_->hide();
  }
}

void HTMLSelectMenuElement::UpdatePartElements() {
  Element* new_button_part = nullptr;
  Element* new_selected_value_part = nullptr;
  HTMLPopupElement* new_listbox_part = nullptr;
  HeapLinkedHashSet<Member<Element>> new_option_parts;

  for (Node* node = FlatTreeTraversal::FirstChild(*this); node != nullptr;
       node = FlatTreeTraversal::Next(*node, this)) {
    // For all part types, if there are multiple candidates, choose the
    // one that comes first in the flat tree traversal.

    auto* element = DynamicTo<Element>(node);
    if (element == nullptr) {
      continue;
    }

    if (new_button_part == nullptr &&
        element->getAttribute(html_names::kPartAttr) == kButtonPartName) {
      new_button_part = element;
    }

    if (new_selected_value_part == nullptr &&
        element->getAttribute(html_names::kPartAttr) ==
            kSelectedValuePartName) {
      new_selected_value_part = element;
    }

    if (new_listbox_part == nullptr &&
        element->getAttribute(html_names::kPartAttr) == kListboxPartName) {
      // TODO(crbug.com/1121840) Should we allow non-<popup> elements to be
      // the listbox part?  If so, how to manage open/closed state?
      if (auto* popup_element = DynamicTo<HTMLPopupElement>(element)) {
        new_listbox_part = popup_element;
      } else {
        GetDocument().AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kRendering,
            mojom::blink::ConsoleMessageLevel::kWarning,
            "Found non-<popup> element labeled as listbox under <selectmenu>, "
            "but only a <popup> can be used for the <selectmenu>'s listbox "
            "part."));
      }
    }

    // The fact that this comes after the clauses for other parts
    // means that an <option> element labeled as another part will
    // be handled as the other part type.  E.g. <option part="button">
    // will be treated as a button.
    // TODO(crbug.com/1121840) Only include options that are inside the
    // listbox, or allow them to be anywhere in the <selectmenu>?
    if (element->getAttribute(html_names::kPartAttr) == kOptionPartName ||
        IsA<HTMLOptionElement>(element)) {
      new_option_parts.insert(element);
    }
  }

  if (button_part_ != new_button_part) {
    if (button_part_) {
      button_part_->removeEventListener(event_type_names::kClick,
                                        button_part_listener_, false);
    }
    if (new_button_part) {
      new_button_part->addEventListener(event_type_names::kClick,
                                        button_part_listener_, false);
    }
    button_part_ = new_button_part;
  }

  selected_value_part_ = new_selected_value_part;

  listbox_part_ = new_listbox_part;

  bool updateSelectedOption = false;
  for (auto& option : option_parts_) {
    if (!new_option_parts.Contains(option)) {
      option->removeEventListener(event_type_names::kClick,
                                  option_part_listener_, false);

      if (option == selected_option_) {
        updateSelectedOption = true;
      }

      // TODO(crbug.com/1121840) Whenever we figure out how to set
      // focusability properly (without using tabIndex), we should undo up
      // those changes here for elements that are no longer option parts.
    }
  }

  for (auto& option : new_option_parts) {
    if (!option_parts_.Contains(option)) {
      option->addEventListener(event_type_names::kClick, option_part_listener_,
                               false);

      // TODO(crbug.com/1121840) We don't want to actually change the attribute,
      // and if tabindex is already set we shouldn't override it.  So we need to
      // come up with something else here.
      option->setTabIndex(-1);
    }
  }

  option_parts_ = new_option_parts;
  if (updateSelectedOption || selected_option_ == nullptr) {
    // If the currently selected option was removed, or if
    // we didn't have a selected option previously, change the
    // selection to the first option part, if there is one.
    SetSelectedOption(option_parts_.size() > 0 ? option_parts_.front()
                                               : nullptr);
  }
}

void HTMLSelectMenuElement::SetSelectedOption(Element* selected_option) {
  if (selected_option_ == selected_option)
    return;

  selected_option_ = selected_option;
  UpdateSelectedValuePartContents();
}

void HTMLSelectMenuElement::UpdateSelectedValuePartContents() {
  // Null-check here because the selected-value part is optional; the author
  // might replace the button contents and not provide a selected-value part if
  // they want to show something in the button other than the current value of
  // the <selectmenu>.
  if (selected_value_part_) {
    selected_value_part_->setTextContent(
        selected_option_ ? selected_option_->innerText() : "");
  }
}

void HTMLSelectMenuElement::ButtonPartEventListener::Invoke(ExecutionContext*,
                                                            Event* event) {
  if (event->type() == event_type_names::kClick &&
      !select_menu_element_->IsOpen()) {
    select_menu_element_->Open();
  }
}

void HTMLSelectMenuElement::OptionPartEventListener::Invoke(ExecutionContext*,
                                                            Event* event) {
  if (event->type() == event_type_names::kClick) {
    Element* target_element =
        DynamicTo<Element>(event->currentTarget()->ToNode());
    DCHECK(target_element);
    DCHECK(select_menu_element_->option_parts_.Contains(target_element));
    select_menu_element_->SetSelectedOption(target_element);
    select_menu_element_->listbox_part_->hide();
  }
}

void HTMLSelectMenuElement::SlotChangeEventListener::Invoke(ExecutionContext*,
                                                            Event* event) {
  DCHECK_EQ(event->type(), event_type_names::kSlotchange);
  // TODO(crbug.com/1121840) Slotchange doesn't fire when
  // the children of slotted content change, so it isn't
  // enough to do this here.  We might need to set up mutation observers
  // or something to watch for changes in addition or instead of the
  // slotchange event.
  // Also, if we want to match the select behavior, then we should be
  // doing this update synchronously.  See failing tests in
  // external/wpt/html/semantics/forms/the-selectmenu-element/selectmenu-value.html
  select_menu_element_->UpdatePartElements();
}

void HTMLSelectMenuElement::Trace(Visitor* visitor) const {
  visitor->Trace(button_part_listener_);
  visitor->Trace(option_part_listener_);
  visitor->Trace(slotchange_listener_);
  visitor->Trace(button_part_);
  visitor->Trace(selected_value_part_);
  visitor->Trace(listbox_part_);
  visitor->Trace(option_parts_);
  visitor->Trace(selected_option_);
  HTMLElement::Trace(visitor);
}

constexpr char HTMLSelectMenuElement::kButtonPartName[];
constexpr char HTMLSelectMenuElement::kSelectedValuePartName[];
constexpr char HTMLSelectMenuElement::kListboxPartName[];
constexpr char HTMLSelectMenuElement::kOptionPartName[];

}  // namespace blink
