// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/html_select_menu_element.h"

#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/synchronous_mutation_observer.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/forms/form_controller.h"
#include "third_party/blink/renderer/core/html/forms/form_data.h"
#include "third_party/blink/renderer/core/html/forms/html_button_element.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/select_menu_part_traversal.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"

namespace blink {

class HTMLSelectMenuElement::SelectMutationCallback
    : public GarbageCollected<HTMLSelectMenuElement::SelectMutationCallback>,
      public SynchronousMutationObserver {
 public:
  explicit SelectMutationCallback(HTMLSelectMenuElement& select);

  // SynchronousMutationObserver:
  void DidChangeChildren(const ContainerNode& container,
                         const ContainerNode::ChildrenChange& change) final;
  void AttributeChanged(const Element& target,
                        const QualifiedName& name,
                        const AtomicString& old_value,
                        const AtomicString& new_value) final;

  void Trace(Visitor* visitor) const override;

 private:
  template <typename StringType>
  void PartInserted(const StringType& part_name, HTMLElement* element);

  template <typename StringType>
  void PartRemoved(const StringType& part_name, HTMLElement* element);

  template <typename StringType>
  void SlotChanged(const StringType& slot_name);

  Member<HTMLSelectMenuElement> select_;
};

HTMLSelectMenuElement::SelectMutationCallback::SelectMutationCallback(
    HTMLSelectMenuElement& select)
    : select_(select) {
  SetDocument(&select_->GetDocument());
}

void HTMLSelectMenuElement::SelectMutationCallback::Trace(
    Visitor* visitor) const {
  visitor->Trace(select_);
  SynchronousMutationObserver::Trace(visitor);
}

void HTMLSelectMenuElement::SelectMutationCallback::DidChangeChildren(
    const ContainerNode& container,
    const ContainerNode::ChildrenChange& change) {
  if (!select_->IsShadowIncludingInclusiveAncestorOf(container))
    return;

  if (change.type == ChildrenChangeType::kElementInserted) {
    auto* root_node = change.sibling_changed;
    for (auto* node = root_node; node != nullptr;
         node = SelectMenuPartTraversal::Next(*node, root_node)) {
      if (auto* element = DynamicTo<HTMLElement>(node)) {
        const AtomicString& part =
            element->getAttribute(html_names::kBehaviorAttr);
        PartInserted(part, element);
        SlotChanged(element->SlotName());
      }
    }
  } else if (change.type == ChildrenChangeType::kElementRemoved) {
    auto* root_node = change.sibling_changed;
    for (auto* node = root_node; node != nullptr;
         node = SelectMenuPartTraversal::Next(*node, root_node)) {
      if (auto* element = DynamicTo<HTMLElement>(node)) {
        const AtomicString& part =
            element->getAttribute(html_names::kBehaviorAttr);
        PartRemoved(part, element);
        SlotChanged(element->SlotName());
      }
    }
  } else if (change.type == ChildrenChangeType::kAllChildrenRemoved) {
    select_->EnsureButtonPartIsValid();
    select_->EnsureSelectedValuePartIsValid();
    select_->EnsureListboxPartIsValid();
  }
}

void HTMLSelectMenuElement::SelectMutationCallback::AttributeChanged(
    const Element& target,
    const QualifiedName& name,
    const AtomicString& old_value,
    const AtomicString& new_value) {
  if (old_value == new_value ||
      !select_->IsShadowIncludingInclusiveAncestorOf(target)) {
    return;
  }
  if (auto* element = DynamicTo<HTMLElement>(const_cast<Element*>(&target))) {
    if (name == html_names::kBehaviorAttr) {
      PartRemoved(old_value, element);
      PartInserted(new_value, element);
    } else if (name == html_names::kSlotAttr) {
      if (auto* option = DynamicTo<HTMLOptionElement>(element)) {
        if (!select_->IsValidOptionPart(element, /*show_warning=*/false)) {
          select_->OptionPartRemoved(option);
        } else {
          select_->OptionPartInserted(option);
        }
      } else {
        SlotChanged(old_value);
        SlotChanged(new_value);
      }
    } else if (name == html_names::kPopoverAttr) {
      // We unconditionally update the listbox part here, because this popover
      // attribute change could either be on the existing listbox part, or on
      // an earlier child of the <selectmenu> which makes
      // FirstValidListboxPart() return a different element.
      select_->UpdateListboxPart();
    }
  }
}

template <typename StringType>
void HTMLSelectMenuElement::SelectMutationCallback::PartInserted(
    const StringType& part_name,
    HTMLElement* element) {
  if (part_name == kButtonPartName) {
    select_->ButtonPartInserted(element);
  } else if (part_name == kSelectedValuePartName) {
    select_->SelectedValuePartInserted(element);
  } else if (part_name == kListboxPartName) {
    select_->ListboxPartInserted(element);
  } else if (IsA<HTMLOptionElement>(element)) {
    select_->OptionPartInserted(DynamicTo<HTMLOptionElement>(element));
  }
}

template <typename StringType>
void HTMLSelectMenuElement::SelectMutationCallback::PartRemoved(
    const StringType& part_name,
    HTMLElement* element) {
  if (part_name == kButtonPartName) {
    select_->ButtonPartRemoved(element);
  } else if (part_name == kSelectedValuePartName) {
    select_->SelectedValuePartRemoved(element);
  } else if (part_name == kListboxPartName) {
    select_->ListboxPartRemoved(element);
  } else if (IsA<HTMLOptionElement>(element)) {
    select_->OptionPartRemoved(DynamicTo<HTMLOptionElement>(element));
  }
}

template <typename StringType>
void HTMLSelectMenuElement::SelectMutationCallback::SlotChanged(
    const StringType& slot_name) {
  if (slot_name == kListboxPartName) {
    select_->UpdateListboxPart();
  } else if (slot_name == kButtonPartName) {
    select_->UpdateButtonPart();
    select_->UpdateSelectedValuePart();
  }
}

HTMLSelectMenuElement::HTMLSelectMenuElement(Document& document)
    : HTMLFormControlElementWithState(html_names::kSelectmenuTag, document),
      type_ahead_(this) {
  DCHECK(RuntimeEnabledFeatures::HTMLSelectMenuElementEnabled());
  DCHECK(RuntimeEnabledFeatures::RuntimeEnabledFeatures::
             HTMLPopoverAttributeEnabled(document.GetExecutionContext()));
  UseCounter::Count(document, WebFeature::kSelectMenuElement);

  EnsureUserAgentShadowRoot();
  select_mutation_callback_ =
      MakeGarbageCollected<HTMLSelectMenuElement::SelectMutationCallback>(
          *this);

  // A selectmenu is the implicit anchor of its listbox.
  IncrementImplicitlyAnchoredElementCount();
}

// static
HTMLSelectMenuElement* HTMLSelectMenuElement::OwnerSelectMenu(Node* node) {
  HTMLSelectMenuElement* nearest_select_menu_ancestor =
      SelectMenuPartTraversal::NearestSelectMenuAncestor(*node);

  if (nearest_select_menu_ancestor &&
      nearest_select_menu_ancestor->AssignedPartType(node) != PartType::kNone) {
    return nearest_select_menu_ancestor;
  }

  return nullptr;
}

HTMLSelectMenuElement::PartType HTMLSelectMenuElement::AssignedPartType(
    Node* node) const {
  if (node == button_part_) {
    return PartType::kButton;
  } else if (node == listbox_part_) {
    return PartType::kListBox;
  } else if (IsA<HTMLOptionElement>(node) &&
             option_parts_.Contains(DynamicTo<HTMLOptionElement>(node))) {
    return PartType::kOption;
  }

  return PartType::kNone;
}

const HTMLSelectMenuElement::ListItems& HTMLSelectMenuElement::GetListItems()
    const {
  if (should_recalc_list_items_) {
    RecalcListItems();
  }
  return list_items_;
}

void HTMLSelectMenuElement::DidAddUserAgentShadowRoot(ShadowRoot& root) {
  DCHECK(IsShadowHost(this));

  Document& document = GetDocument();

  button_slot_ = MakeGarbageCollected<HTMLSlotElement>(document);
  button_slot_->setAttribute(html_names::kNameAttr, kButtonPartName);

  button_part_ = MakeGarbageCollected<HTMLButtonElement>(document);
  button_part_->setAttribute(html_names::kPartAttr, kButtonPartName);
  button_part_->setAttribute(html_names::kBehaviorAttr, kButtonPartName);
  button_part_->SetShadowPseudoId(AtomicString("-internal-selectmenu-button"));
  button_part_listener_ =
      MakeGarbageCollected<HTMLSelectMenuElement::ButtonPartEventListener>(
          this);
  button_part_listener_->AddEventListeners(button_part_);

  selected_value_slot_ = MakeGarbageCollected<HTMLSlotElement>(document);
  selected_value_slot_->setAttribute(html_names::kNameAttr,
                                     kSelectedValuePartName);

  selected_value_part_ = MakeGarbageCollected<HTMLDivElement>(document);
  selected_value_part_->setAttribute(html_names::kPartAttr,
                                     kSelectedValuePartName);
  selected_value_part_->setAttribute(html_names::kBehaviorAttr,
                                     kSelectedValuePartName);

  marker_slot_ = MakeGarbageCollected<HTMLSlotElement>(document);
  marker_slot_->setAttribute(html_names::kNameAttr, kMarkerPartName);

  auto* marker_icon = MakeGarbageCollected<HTMLDivElement>(document);
  marker_icon->SetShadowPseudoId(
      AtomicString("-internal-selectmenu-button-icon"));
  marker_icon->setAttribute(html_names::kPartAttr, kMarkerPartName);

  listbox_slot_ = MakeGarbageCollected<HTMLSlotElement>(document);
  listbox_slot_->setAttribute(html_names::kNameAttr, kListboxPartName);

  HTMLElement* new_popover;
  new_popover = MakeGarbageCollected<HTMLDivElement>(document);
  new_popover->setAttribute(html_names::kPopoverAttr, kPopoverTypeValueAuto);
  new_popover->setAttribute(html_names::kPartAttr, kListboxPartName);
  new_popover->setAttribute(html_names::kBehaviorAttr, kListboxPartName);
  new_popover->SetShadowPseudoId(AtomicString("-internal-selectmenu-listbox"));
  SetListboxPart(new_popover);

  auto* options_slot = MakeGarbageCollected<HTMLSlotElement>(document);

  button_part_->AppendChild(selected_value_slot_);
  button_part_->AppendChild(marker_slot_);

  selected_value_slot_->AppendChild(selected_value_part_);

  marker_slot_->AppendChild(marker_icon);

  button_slot_->AppendChild(button_part_);

  listbox_part_->appendChild(options_slot);
  listbox_slot_->appendChild(listbox_part_);

  root.AppendChild(button_slot_);
  root.AppendChild(listbox_slot_);

  option_part_listener_ =
      MakeGarbageCollected<HTMLSelectMenuElement::OptionPartEventListener>(
          this);
}

void HTMLSelectMenuElement::DidMoveToNewDocument(Document& old_document) {
  HTMLFormControlElementWithState::DidMoveToNewDocument(old_document);
  select_mutation_callback_->SetDocument(&GetDocument());

  // Since we're observing the lifecycle updates, ensure that we listen to the
  // right document's view.
  if (queued_check_for_missing_parts_) {
    if (old_document.View())
      old_document.View()->UnregisterFromLifecycleNotifications(this);

    if (GetDocument().View())
      GetDocument().View()->RegisterForLifecycleNotifications(this);
    else
      queued_check_for_missing_parts_ = false;
  }
}

String HTMLSelectMenuElement::value() const {
  if (HTMLOptionElement* option = selectedOption()) {
    return option->value();
  }
  return "";
}

void HTMLSelectMenuElement::setValueForBinding(const String& value) {
  if (GetAutofillState() != WebAutofillState::kAutofilled) {
    setValue(value);
  } else {
    String old_value = this->value();
    setValue(value, /*send_events=*/false,
             value != old_value ? WebAutofillState::kNotFilled
                                : WebAutofillState::kAutofilled);
    if (Page* page = GetDocument().GetPage()) {
      page->GetChromeClient().JavaScriptChangedAutofilledValue(*this,
                                                               old_value);
    }
  }
}

void HTMLSelectMenuElement::setValue(const String& value,
                                     bool send_events,
                                     WebAutofillState autofill_state) {
  // Find the option with value matching the given parameter and make it the
  // current selection.
  HTMLOptionElement* selected_option = nullptr;
  for (auto& option : option_parts_) {
    if (option->value() == value) {
      selected_option = option;
      break;
    }
  }
  SetSelectedOption(selected_option, send_events, autofill_state);
}

bool HTMLSelectMenuElement::open() const {
  // TODO(crbug.com/1121840) listbox_part_ can be null if
  // the author has filled the listbox slot without including
  // a replacement listbox part. Instead of null checks like this,
  // we should consider refusing to render the control at all if
  // either of the key parts (button or listbox) are missing.
  if (!listbox_part_)
    return false;
  return listbox_part_->HasPopoverAttribute() && listbox_part_->popoverOpen();
}

void HTMLSelectMenuElement::SetAutofillValue(const String& value,
                                             WebAutofillState autofill_state) {
  setValue(value, /*send_events=*/true, autofill_state);
}

void HTMLSelectMenuElement::OpenListbox() {
  if (listbox_part_ && !open()) {
    listbox_part_->showPopover(ASSERT_NO_EXCEPTION);
    PseudoStateChanged(CSSSelector::kPseudoClosed);
    PseudoStateChanged(CSSSelector::kPseudoOpen);
    if (selectedOption()) {
      selectedOption()->Focus();
    }
    selected_option_when_listbox_opened_ = selectedOption();
  }
}

void HTMLSelectMenuElement::CloseListbox() {
  if (listbox_part_ && open()) {
    if (listbox_part_->HasPopoverAttribute()) {
      // We will handle focus directly.
      listbox_part_->HidePopoverInternal(
          HidePopoverFocusBehavior::kNone,
          HidePopoverTransitionBehavior::kFireEventsAndWaitForTransitions,
          /*exception_state=*/nullptr);
    }
  }
}

bool HTMLSelectMenuElement::TypeAheadFind(const KeyboardEvent& event,
                                          int charCode) {
  if (event.ctrlKey() || event.altKey() || event.metaKey() ||
      !WTF::unicode::IsPrintableChar(charCode)) {
    return false;
  }

  int index = type_ahead_.HandleEvent(
      event, charCode, TypeAhead::kMatchPrefix | TypeAhead::kCycleFirstChar);
  if (index < 0) {
    return false;
  }

  SetSelectedOption(OptionAtListIndex(index), /*send_events=*/true);
  if (open() && selectedOption()) {
    selectedOption()->Focus();
  }

  selected_option_->SetDirty(true);
  return true;
}

void HTMLSelectMenuElement::ListboxWasClosed() {
  PseudoStateChanged(CSSSelector::kPseudoClosed);
  PseudoStateChanged(CSSSelector::kPseudoOpen);
  if (button_part_) {
    button_part_->Focus();
  }
  if (selectedOption() != selected_option_when_listbox_opened_) {
    DispatchChangeEvent();
  }
}

void HTMLSelectMenuElement::ResetTypeAheadSessionForTesting() {
  type_ahead_.ResetSession();
}

bool HTMLSelectMenuElement::SetListboxPart(HTMLElement* new_listbox_part) {
  if (listbox_part_ == new_listbox_part)
    return false;

  if (listbox_part_) {
    listbox_part_->SetOwnerSelectMenuElement(nullptr);
  }

  if (new_listbox_part) {
    new_listbox_part->SetOwnerSelectMenuElement(this);
  } else {
    QueueCheckForMissingParts();
  }

  listbox_part_ = new_listbox_part;
  return true;
}

bool HTMLSelectMenuElement::IsValidButtonPart(const Node* node,
                                              bool show_warning) const {
  auto* element = DynamicTo<Element>(node);
  if (!element ||
      element->getAttribute(html_names::kBehaviorAttr) != kButtonPartName) {
    return false;
  }

  bool is_valid_tree_position =
      !listbox_part_ ||
      !FlatTreeTraversal::IsDescendantOf(*element, *listbox_part_);
  if (!is_valid_tree_position && show_warning) {
    GetDocument().AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kRendering,
        mojom::blink::ConsoleMessageLevel::kWarning,
        "To receive button part controller code, an element labeled as a "
        "button must not be a descendant of the <selectmenu>'s listbox "
        "part. This <selectmenu> will not be fully functional."));
  }

  return is_valid_tree_position;
}

bool HTMLSelectMenuElement::IsValidListboxPart(const Node* node,
                                               bool show_warning) const {
  auto* element = DynamicTo<HTMLElement>(node);
  if (!element ||
      element->getAttribute(html_names::kBehaviorAttr) != kListboxPartName) {
    return false;
  }

  if (!element->HasPopoverAttribute()) {
    if (show_warning) {
      GetDocument().AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
          mojom::blink::ConsoleMessageSource::kRendering,
          mojom::blink::ConsoleMessageLevel::kWarning,
          "Found non-popover element labeled as listbox under "
          "<selectmenu>, which is not allowed. The <selectmenu>'s "
          "listbox element must have a valid value set for the 'popover' "
          "attribute. This <selectmenu> will not be fully functional."));
    }
    return false;
  }

  if (button_part_ &&
      FlatTreeTraversal::IsDescendantOf(*element, *button_part_)) {
    if (show_warning) {
      GetDocument().AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
          mojom::blink::ConsoleMessageSource::kRendering,
          mojom::blink::ConsoleMessageLevel::kWarning,
          "To receive listbox part controller code, an element labeled as a "
          "listbox must not be a descendant of the <selectmenu>'s button "
          "part. This <selectmenu> will not be fully functional."));
    }
    return false;
  }

  return true;
}

bool HTMLSelectMenuElement::IsValidOptionPart(const Node* node,
                                              bool show_warning) const {
  auto* element = DynamicTo<HTMLElement>(node);
  if (!element || !IsA<HTMLOptionElement>(element)) {
    return false;
  }

  bool is_valid_tree_position =
      listbox_part_ &&
      SelectMenuPartTraversal::IsDescendantOf(*element, *listbox_part_);
  if (!is_valid_tree_position && show_warning) {
    GetDocument().AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kRendering,
        mojom::blink::ConsoleMessageLevel::kWarning,
        "To receive option part controller code, an element labeled as an "
        "option must be a descendant of the <selectmenu>'s listbox "
        "part. This <selectmenu> will not be fully functional."));
  }
  return is_valid_tree_position;
}

HTMLElement* HTMLSelectMenuElement::FirstValidButtonPart() const {
  for (Node* node = SelectMenuPartTraversal::FirstChild(*this); node;
       node = SelectMenuPartTraversal::Next(*node, this)) {
    if (IsValidButtonPart(node, /*show_warning=*/false)) {
      return DynamicTo<HTMLElement>(node);
    }
  }

  return nullptr;
}

void HTMLSelectMenuElement::SetButtonPart(HTMLElement* new_button_part) {
  if (button_part_ == new_button_part)
    return;

  if (button_part_) {
    button_part_listener_->RemoveEventListeners(button_part_);
  }

  if (new_button_part) {
    button_part_listener_->AddEventListeners(new_button_part);
  } else {
    QueueCheckForMissingParts();
  }

  button_part_ = new_button_part;
}

void HTMLSelectMenuElement::ButtonPartInserted(HTMLElement* new_button_part) {
  if (!IsValidButtonPart(new_button_part, /*show_warning=*/true)) {
    return;
  }

  UpdateButtonPart();
}

void HTMLSelectMenuElement::ButtonPartRemoved(HTMLElement* button_part) {
  if (button_part != button_part_) {
    return;
  }

  UpdateButtonPart();
}

void HTMLSelectMenuElement::UpdateButtonPart() {
  SetButtonPart(FirstValidButtonPart());
}

void HTMLSelectMenuElement::EnsureButtonPartIsValid() {
  if (!button_part_ ||
      !SelectMenuPartTraversal::IsDescendantOf(*button_part_, *this) ||
      !IsValidButtonPart(button_part_, /*show_warning*/ false)) {
    UpdateButtonPart();
  }
}

HTMLElement* HTMLSelectMenuElement::FirstValidSelectedValuePart() const {
  for (Node* node = SelectMenuPartTraversal::FirstChild(*this); node;
       node = SelectMenuPartTraversal::Next(*node, this)) {
    auto* element = DynamicTo<HTMLElement>(node);
    if (!element) {
      continue;
    }

    if (element->getAttribute(html_names::kBehaviorAttr) ==
        kSelectedValuePartName) {
      return element;
    }
  }
  return nullptr;
}

void HTMLSelectMenuElement::SelectedValuePartInserted(
    HTMLElement* new_selected_value_part) {
  UpdateSelectedValuePart();
}

void HTMLSelectMenuElement::SelectedValuePartRemoved(
    HTMLElement* selected_value_part) {
  if (selected_value_part != selected_value_part_) {
    return;
  }
  UpdateSelectedValuePart();
}

void HTMLSelectMenuElement::UpdateSelectedValuePart() {
  selected_value_part_ = FirstValidSelectedValuePart();
}

void HTMLSelectMenuElement::EnsureSelectedValuePartIsValid() {
  if (!selected_value_part_ ||
      selected_value_part_->getAttribute(html_names::kBehaviorAttr) !=
          kSelectedValuePartName ||
      !SelectMenuPartTraversal::IsDescendantOf(*selected_value_part_, *this)) {
    UpdateSelectedValuePart();
  }
}

HTMLElement* HTMLSelectMenuElement::FirstValidListboxPart() const {
  for (Node* node = SelectMenuPartTraversal::FirstChild(*this); node;
       node = SelectMenuPartTraversal::Next(*node, this)) {
    if (IsValidListboxPart(node, /*show_warning=*/false)) {
      return DynamicTo<HTMLElement>(node);
    }
  }
  return nullptr;
}

void HTMLSelectMenuElement::ListboxPartInserted(HTMLElement* new_listbox_part) {
  if (!IsValidListboxPart(new_listbox_part, /*show_warning=*/true)) {
    return;
  }
  UpdateListboxPart();
}

void HTMLSelectMenuElement::ListboxPartRemoved(HTMLElement* listbox_part) {
  if (listbox_part_ != listbox_part) {
    return;
  }
  UpdateListboxPart();
}

void HTMLSelectMenuElement::UpdateListboxPart() {
  if (!SetListboxPart(FirstValidListboxPart()))
    return;
  ResetOptionParts();
}

void HTMLSelectMenuElement::EnsureListboxPartIsValid() {
  if (!listbox_part_ ||
      !SelectMenuPartTraversal::IsDescendantOf(*listbox_part_, *this) ||
      !IsValidListboxPart(listbox_part_, /*show_warning*/ false)) {
    UpdateListboxPart();
  } else {
    HeapLinkedHashSet<Member<HTMLOptionElement>> invalid_option_parts;
    for (auto& option : option_parts_) {
      if (!IsValidOptionPart(option.Get(), /*show_warning=*/false)) {
        invalid_option_parts.insert(option.Get());
      }
    }
    for (auto& invalid_option : invalid_option_parts) {
      OptionPartRemoved(invalid_option.Get());
    }
  }
}

void HTMLSelectMenuElement::QueueCheckForMissingParts() {
  if (!queued_check_for_missing_parts_ && GetDocument().View()) {
    queued_check_for_missing_parts_ = true;
    GetDocument().View()->RegisterForLifecycleNotifications(this);
  }
}

void HTMLSelectMenuElement::ResetOptionParts() {
  // Remove part status from all current option parts
  while (!option_parts_.empty()) {
    OptionPartRemoved(option_parts_.back());
  }

  // Find new option parts under the new listbox
  for (Node* node = SelectMenuPartTraversal::FirstChild(*this); node;
       node = SelectMenuPartTraversal::Next(*node, this)) {
    if (IsValidOptionPart(node, /*show_warning=*/false)) {
      OptionPartInserted(DynamicTo<HTMLOptionElement>(node));
    }
  }
}

void HTMLSelectMenuElement::DispatchInputAndChangeEventsIfNeeded() {
  DispatchInputEvent();
  if (!open()) {
    // Only fire change if the listbox is already closed, because if it's open
    // we'll  fire change later when the listbox closes.
    DispatchChangeEvent();
  }
}

void HTMLSelectMenuElement::DispatchInputEvent() {
  Event* input_event = Event::CreateBubble(event_type_names::kInput);
  input_event->SetComposed(true);
  DispatchScopedEvent(*input_event);
}

void HTMLSelectMenuElement::DispatchChangeEvent() {
  DispatchScopedEvent(*Event::CreateBubble(event_type_names::kChange));
}

void HTMLSelectMenuElement::OptionPartInserted(
    HTMLOptionElement* new_option_part) {
  if (!IsValidOptionPart(new_option_part, /*show_warning=*/true)) {
    return;
  }

  if (option_parts_.Contains(new_option_part)) {
    return;
  }

  new_option_part->OptionInsertedIntoSelectMenuElement();
  option_part_listener_->AddEventListeners(new_option_part);

  // TODO(crbug.com/1191131) The option part list should match the flat tree
  // order.
  option_parts_.insert(new_option_part);

  if (!selected_option_ || new_option_part->Selected()) {
    SetSelectedOption(new_option_part);
  }
  SetNeedsValidityCheck();
  should_recalc_list_items_ = true;
}

void HTMLSelectMenuElement::OptionPartRemoved(HTMLOptionElement* option_part) {
  if (!option_parts_.Contains(option_part)) {
    return;
  }

  option_part->OptionRemovedFromSelectMenuElement();
  option_part_listener_->RemoveEventListeners(option_part);
  option_parts_.erase(option_part);

  if (selected_option_ == option_part) {
    ResetToDefaultSelection();
  }
  SetNeedsValidityCheck();
  should_recalc_list_items_ = true;
}

HTMLOptionElement* HTMLSelectMenuElement::FirstOptionPart() const {
  // TODO(crbug.com/1121840) This is going to be replaced by an option part
  // list iterator, or we could reuse OptionListIterator if we decide that just
  // <option>s are supported as option parts.
  for (Node* node = SelectMenuPartTraversal::FirstChild(*this); node;
       node = SelectMenuPartTraversal::Next(*node, this)) {
    if (IsValidOptionPart(node, /*show_warning=*/false)) {
      return DynamicTo<HTMLOptionElement>(node);
    }
  }

  return nullptr;
}

void HTMLSelectMenuElement::OptionSelectionStateChanged(
    HTMLOptionElement* option,
    bool option_is_selected) {
  DCHECK(option_parts_.Contains(option));
  if (option_is_selected) {
    SetSelectedOption(option);
  } else if (selectedOption() == option) {
    ResetToDefaultSelection();
  }
}

void HTMLSelectMenuElement::DidFinishLifecycleUpdate(
    const LocalFrameView& local_frame_view) {
  Document* document = local_frame_view.GetFrame().GetDocument();
  if (document->Lifecycle().GetState() <
      DocumentLifecycle::kAfterPerformLayout) {
    return;
  }

  DCHECK(queued_check_for_missing_parts_);
  queued_check_for_missing_parts_ = false;
  document->View()->UnregisterFromLifecycleNotifications(this);

  if (!button_part_) {
    GetDocument().AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kRendering,
        mojom::blink::ConsoleMessageLevel::kWarning,
        "A <selectmenu>'s default button was removed and a new one was not "
        "provided. This <selectmenu> will not be fully functional."));
  }

  if (!listbox_part_) {
    GetDocument().AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kRendering,
        mojom::blink::ConsoleMessageLevel::kWarning,
        "A <selectmenu>'s default listbox was removed and a new one was not "
        "provided. This <selectmenu> will not be fully functional."));
  }
}

int HTMLSelectMenuElement::IndexOfSelectedOption() const {
  int index = 0;
  for (const auto& item : GetListItems()) {
    auto* option_element = DynamicTo<HTMLOptionElement>(item.Get());
    if (option_element && option_element->Selected()) {
      return index;
    }
    ++index;
  }
  return -1;
}

int HTMLSelectMenuElement::OptionCount() const {
  return GetListItems().size();
}

String HTMLSelectMenuElement::OptionAtIndex(int index) const {
  HTMLOptionElement* option = OptionAtListIndex(index);
  if (!option || option->IsDisabledFormControl()) {
    return String();
  }
  return option->DisplayLabel();
}

HTMLOptionElement* HTMLSelectMenuElement::selectedOption() const {
  DCHECK(!selected_option_ ||
         IsValidOptionPart(selected_option_, /*show_warning=*/false));
  return selected_option_;
}

void HTMLSelectMenuElement::SetSelectedOption(
    HTMLOptionElement* selected_option,
    bool send_events,
    WebAutofillState autofill_state) {
  SetAutofillState(selected_option ? autofill_state
                                   : WebAutofillState::kNotFilled);

  if (selected_option_ == selected_option)
    return;

  if (selected_option_)
    selected_option_->SetSelectedState(false);

  selected_option_ = selected_option;

  if (selected_option_)
    selected_option_->SetSelectedState(true);

  UpdateSelectedValuePartContents();
  SetNeedsValidityCheck();
  if (send_events) {
    DispatchInputAndChangeEventsIfNeeded();
  }
  NotifyFormStateChanged();

  // We set the Autofill state again because setting the autofill value
  // triggers JavaScript events and the site may override the autofilled value,
  // which resets the Autofilled state. Even if the website modifies the from
  // control element's content during the autofill operation, we want the state
  // to show as autofilled.
  SetAutofillState(selected_option ? autofill_state
                                   : WebAutofillState::kNotFilled);
}

void HTMLSelectMenuElement::OptionElementChildrenChanged(
    const HTMLOptionElement& option) {
  if (selected_option_ == &option) {
    SetNeedsValidityCheck();
    NotifyFormStateChanged();
    UpdateSelectedValuePartContents();
  }
}

void HTMLSelectMenuElement::OptionElementValueChanged(
    const HTMLOptionElement& option) {
  if (selected_option_ == &option) {
    SetNeedsValidityCheck();
    NotifyFormStateChanged();
  }
}

void HTMLSelectMenuElement::SelectNextOption() {
  for (Node* node = SelectMenuPartTraversal::Next(*selectedOption(), this);
       node; node = SelectMenuPartTraversal::Next(*node, this)) {
    if (IsValidOptionPart(node, /*show_warning=*/false)) {
      auto* element = DynamicTo<HTMLOptionElement>(node);
      if (element->IsDisabledFormControl())
        continue;
      SetSelectedOption(element);
      element->Focus();
      DispatchInputAndChangeEventsIfNeeded();
      return;
    }
  }
}

void HTMLSelectMenuElement::SelectPreviousOption() {
  for (Node* node = SelectMenuPartTraversal::Previous(*selectedOption(), this);
       node; node = SelectMenuPartTraversal::Previous(*node, this)) {
    if (IsValidOptionPart(node, /*show_warning=*/false)) {
      auto* element = DynamicTo<HTMLOptionElement>(node);
      if (element->IsDisabledFormControl())
        continue;
      SetSelectedOption(element);
      element->Focus();
      DispatchInputAndChangeEventsIfNeeded();
      return;
    }
  }
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

void HTMLSelectMenuElement::RecalcListItems() const {
  list_items_.clear();
  for (Node* node = SelectMenuPartTraversal::FirstChild(*this); node;
       node = SelectMenuPartTraversal::Next(*node, this)) {
    if (IsValidOptionPart(node, /*show_warning=*/false)) {
      list_items_.push_back(To<HTMLOptionElement>(node));
    }
  }
}

HTMLOptionElement* HTMLSelectMenuElement::OptionAtListIndex(
    int list_index) const {
  if (list_index < 0) {
    return nullptr;
  }
  const ListItems& items = GetListItems();
  if (static_cast<wtf_size_t>(list_index) >= items.size()) {
    return nullptr;
  }

  return DynamicTo<HTMLOptionElement>(items[list_index].Get());
}

void HTMLSelectMenuElement::ButtonPartEventListener::Invoke(ExecutionContext*,
                                                            Event* event) {
  if (event->defaultPrevented())
    return;

  if (event->type() == event_type_names::kClick &&
      !select_menu_element_->IsDisabledFormControl()) {
    if (!select_menu_element_->open()) {
      select_menu_element_->OpenListbox();
    }
    // TODO(crbug.com/1408838) Close list box if dialog is open.
  } else if (event->type() == event_type_names::kBlur) {
    select_menu_element_->type_ahead_.ResetSession();
  } else if (event->IsKeyboardEvent()) {
    auto* keyboard_event = DynamicTo<KeyboardEvent>(event);
    if (!keyboard_event) {
      return;
    }

    if (!select_menu_element_->open() &&
        !select_menu_element_->IsDisabledFormControl() &&
        HandleKeyboardEvent(*keyboard_event)) {
      event->SetDefaultHandled();
    }
  }
}

void HTMLSelectMenuElement::ButtonPartEventListener::AddEventListeners(
    HTMLElement* button_part) {
  button_part->addEventListener(event_type_names::kClick, this,
                                /*use_capture=*/false);
  button_part->addEventListener(event_type_names::kBlur, this,
                                /*use_capture=*/false);

  // Observe keydown and keyup in order to override default HTMLButtonElement
  // handling in HTMLElement::HandleKeyboardActivation() for VKEY_SPACE.
  button_part->addEventListener(event_type_names::kKeydown, this,
                                /*use_capture=*/false);
  button_part->addEventListener(event_type_names::kKeyup, this,
                                /*use_capture=*/false);
  button_part->addEventListener(event_type_names::kKeypress, this,
                                /*use_capture=*/false);
}

void HTMLSelectMenuElement::ButtonPartEventListener::RemoveEventListeners(
    HTMLElement* button_part) {
  button_part->removeEventListener(event_type_names::kClick, this,
                                   /*use_capture=*/false);
  button_part->removeEventListener(event_type_names::kBlur, this,
                                   /*use_capture=*/false);
  button_part->removeEventListener(event_type_names::kKeydown, this,
                                   /*use_capture=*/false);
  button_part->removeEventListener(event_type_names::kKeyup, this,
                                   /*use_capture=*/false);
  button_part->removeEventListener(event_type_names::kKeypress, this,
                                   /*use_capture=*/false);
}

bool HTMLSelectMenuElement::ButtonPartEventListener::HandleKeyboardEvent(
    const KeyboardEvent& event) {
  if (event.keyCode() == VKEY_SPACE) {
    if (event.type() == event_type_names::kKeydown) {
      if (select_menu_element_->type_ahead_.HasActiveSession(event)) {
        select_menu_element_->TypeAheadFind(event, ' ');
      } else {
        select_menu_element_->OpenListbox();
      }
    }
    // Override default HTMLButtonElement handling in
    // HTMLElement::HandleKeyboardActivation().
    return true;
  }
  if (event.keyCode() == VKEY_RETURN &&
      event.type() == event_type_names::kKeydown) {
    // Handle <RETURN> because not all HTML elements synthesize a click when
    // <RETURN> is pressed.
    select_menu_element_->OpenListbox();
    return true;
  }
  // Handled in event_type_names::kKeypress event handler because
  // KeyboardEvent::charCode() == 0 for event_type_names::kKeydown.
  return event.type() == event_type_names::kKeypress &&
         select_menu_element_->TypeAheadFind(event, event.charCode());
}

void HTMLSelectMenuElement::OptionPartEventListener::Invoke(ExecutionContext*,
                                                            Event* event) {
  if (event->defaultPrevented())
    return;

  if (event->type() == event_type_names::kClick) {
    auto* target_element =
        DynamicTo<HTMLOptionElement>(event->currentTarget()->ToNode());
    DCHECK(target_element);
    DCHECK(select_menu_element_->option_parts_.Contains(target_element));
    if (target_element != select_menu_element_->selectedOption()) {
      select_menu_element_->SetSelectedOption(target_element);
      select_menu_element_->DispatchInputEvent();
    }
    select_menu_element_->CloseListbox();
  } else if (event->IsKeyboardEvent()) {
    auto* keyboard_event = DynamicTo<KeyboardEvent>(event);
    if (keyboard_event && HandleKeyboardEvent(*keyboard_event)) {
      event->stopPropagation();
      event->SetDefaultHandled();
    }
  }
}

void HTMLSelectMenuElement::OptionPartEventListener::AddEventListeners(
    HTMLOptionElement* option_part) {
  option_part->addEventListener(event_type_names::kClick, this,
                                /*use_capture=*/false);
  option_part->addEventListener(event_type_names::kKeydown, this,
                                /*use_capture=*/false);
  option_part->addEventListener(event_type_names::kKeypress, this,
                                /*use_capture=*/false);
}

void HTMLSelectMenuElement::OptionPartEventListener::RemoveEventListeners(
    HTMLOptionElement* option_part) {
  option_part->removeEventListener(event_type_names::kClick, this,
                                   /*use_capture=*/false);
  option_part->removeEventListener(event_type_names::kKeydown, this,
                                   /*use_capture=*/false);
  option_part->removeEventListener(event_type_names::kKeypress, this,
                                   /*use_capture=*/false);
}

bool HTMLSelectMenuElement::OptionPartEventListener::HandleKeyboardEvent(
    const KeyboardEvent& event) {
  if (event.type() == event_type_names::kKeydown) {
    switch (event.keyCode()) {
      case VKEY_RETURN: {
        auto* target_element =
            DynamicTo<HTMLOptionElement>(event.currentTarget()->ToNode());
        DCHECK(target_element);
        DCHECK(select_menu_element_->option_parts_.Contains(target_element));
        if (target_element != select_menu_element_->selectedOption()) {
          select_menu_element_->SetSelectedOption(target_element);
          select_menu_element_->DispatchInputEvent();
        }
        select_menu_element_->CloseListbox();
        return true;
      }
      case VKEY_SPACE: {
        select_menu_element_->TypeAheadFind(event, ' ');
        // Prevent the default behavior of scrolling the page on spacebar
        // that would cause the listbox to close.
        return true;
      }
      case VKEY_UP: {
        select_menu_element_->SelectPreviousOption();
        return true;
      }
      case VKEY_DOWN: {
        select_menu_element_->SelectNextOption();
        return true;
      }
    }
  } else if (event.type() == event_type_names::kKeypress) {
    // Handled in event_type_names::kKeypress event handler because
    // KeyboardEvent::charCode() == 0 for event_type_names::kKeydown.
    return select_menu_element_->TypeAheadFind(event, event.charCode());
  }

  return false;
}

const AtomicString& HTMLSelectMenuElement::FormControlType() const {
  DEFINE_STATIC_LOCAL(const AtomicString, selectmenu, ("selectmenu"));
  return selectmenu;
}

void HTMLSelectMenuElement::DefaultEventHandler(Event& event) {
  if (!GetLayoutObject()) {
    return;
  }

  if (event.type() == event_type_names::kChange) {
    user_has_edited_the_field_ = true;
  }
}

bool HTMLSelectMenuElement::MayTriggerVirtualKeyboard() const {
  return true;
}

void HTMLSelectMenuElement::AppendToFormData(FormData& form_data) {
  if (!GetName().empty())
    form_data.AppendFromElement(GetName(), value());
}

FormControlState HTMLSelectMenuElement::SaveFormControlState() const {
  return FormControlState(value());
}

void HTMLSelectMenuElement::RestoreFormControlState(
    const FormControlState& state) {
  setValue(state[0]);
}

bool HTMLSelectMenuElement::IsRequiredFormControl() const {
  return IsRequired();
}

bool HTMLSelectMenuElement::IsOptionalFormControl() const {
  return !IsRequiredFormControl();
}

bool HTMLSelectMenuElement::IsLabelable() const {
  return true;
}

bool HTMLSelectMenuElement::ValueMissing() const {
  if (!IsRequired())
    return false;

  if (auto* selected_option = selectedOption()) {
    // If a non-placeholder label option is selected, it's not value-missing.
    // https://html.spec.whatwg.org/multipage/form-elements.html#placeholder-label-option
    return selected_option == FirstOptionPart() &&
           selected_option->value().empty();
  }

  return true;
}

void HTMLSelectMenuElement::CloneNonAttributePropertiesFrom(
    const Element& source,
    CloneChildrenFlag flag) {
  const auto& source_element =
      static_cast<const HTMLSelectMenuElement&>(source);
  user_has_edited_the_field_ = source_element.user_has_edited_the_field_;
  HTMLFormControlElement::CloneNonAttributePropertiesFrom(source, flag);
}

// https://html.spec.whatwg.org/C/#ask-for-a-reset
void HTMLSelectMenuElement::ResetImpl() {
  for (auto& option : option_parts_) {
    option->SetSelectedState(
        option->FastHasAttribute(html_names::kSelectedAttr));
    option->SetDirty(false);
  }
  ResetToDefaultSelection();
  SetNeedsValidityCheck();
  HTMLFormControlElementWithState::ResetImpl();
}

// https://html.spec.whatwg.org/C#selectedness-setting-algorithm
void HTMLSelectMenuElement::ResetToDefaultSelection() {
  HTMLOptionElement* first_enabled_option = nullptr;
  HTMLOptionElement* last_selected_option = nullptr;

  for (Node* node = SelectMenuPartTraversal::FirstChild(*this); node;
       node = SelectMenuPartTraversal::Next(*node, this)) {
    if (IsValidOptionPart(node, /*show_warning=*/false)) {
      auto* option = DynamicTo<HTMLOptionElement>(node);
      if (option->Selected()) {
        if (last_selected_option) {
          last_selected_option->SetSelectedState(false);
        }
        last_selected_option = option;
      }
      if (!first_enabled_option && !option->IsDisabledFormControl()) {
        first_enabled_option = option;
      }
    }
  }

  // If no option is selected, set the selection to the first non-disabled
  // option if it exists, or null otherwise.
  //
  // If two or more options are selected, set the selection to the last
  // selected option. ResetImpl() can temporarily select multiple options.
  if (last_selected_option) {
    SetSelectedOption(last_selected_option);
  } else {
    SetSelectedOption(first_enabled_option);
  }
}

String HTMLSelectMenuElement::validationMessage() const {
  if (!willValidate())
    return String();
  if (CustomError())
    return CustomValidationMessage();
  if (ValueMissing()) {
    return GetLocale().QueryString(IDS_FORM_VALIDATION_VALUE_MISSING_SELECT);
  }
  return String();
}

void HTMLSelectMenuElement::Trace(Visitor* visitor) const {
  visitor->Trace(button_part_listener_);
  visitor->Trace(option_part_listener_);
  visitor->Trace(select_mutation_callback_);
  visitor->Trace(button_part_);
  visitor->Trace(selected_value_part_);
  visitor->Trace(listbox_part_);
  visitor->Trace(option_parts_);
  visitor->Trace(button_slot_);
  visitor->Trace(listbox_slot_);
  visitor->Trace(marker_slot_);
  visitor->Trace(selected_value_slot_);
  visitor->Trace(selected_option_);
  visitor->Trace(selected_option_when_listbox_opened_);
  visitor->Trace(list_items_);
  HTMLFormControlElementWithState::Trace(visitor);
}

constexpr char HTMLSelectMenuElement::kButtonPartName[];
constexpr char HTMLSelectMenuElement::kSelectedValuePartName[];
constexpr char HTMLSelectMenuElement::kListboxPartName[];

}  // namespace blink
