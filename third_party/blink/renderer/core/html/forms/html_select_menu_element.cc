// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/html_select_menu_element.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_mutation_observer_init.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/mutation_observer.h"
#include "third_party/blink/renderer/core/dom/mutation_record.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/forms/html_button_element.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/html_popup_element.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

class HTMLSelectMenuElement::SelectMutationCallback
    : public MutationObserver::Delegate {
 public:
  explicit SelectMutationCallback(HTMLSelectMenuElement& select);

  ExecutionContext* GetExecutionContext() const override;
  void Deliver(const MutationRecordVector& records, MutationObserver&) override;
  void Trace(Visitor* visitor) const override;

 private:
  template <typename StringType>
  void PartInserted(const StringType& part_name, Element* element);

  template <typename StringType>
  void PartRemoved(const StringType& part_name, Element* element);

  Member<HTMLSelectMenuElement> select_;
  Member<MutationObserver> observer_;
};

HTMLSelectMenuElement::SelectMutationCallback::SelectMutationCallback(
    HTMLSelectMenuElement& select)
    : select_(select), observer_(MutationObserver::Create(this)) {
  MutationObserverInit* init = MutationObserverInit::Create();
  init->setAttributeOldValue(true);
  init->setAttributes(true);
  // TODO(crbug.com/1121840) There are more attributes that affect <selectmenu>.
  init->setAttributeFilter({"part"});
  init->setChildList(true);
  init->setSubtree(true);
  observer_->observe(select_, init, ASSERT_NO_EXCEPTION);
}

ExecutionContext*
HTMLSelectMenuElement::SelectMutationCallback::GetExecutionContext() const {
  return select_->GetExecutionContext();
}

void HTMLSelectMenuElement::SelectMutationCallback::Trace(
    Visitor* visitor) const {
  visitor->Trace(select_);
  visitor->Trace(observer_);
  MutationObserver::Delegate::Trace(visitor);
}

void HTMLSelectMenuElement::SelectMutationCallback::Deliver(
    const MutationRecordVector& records,
    MutationObserver&) {
  for (const auto& record : records) {
    if (record->type() == "attributes") {
      if (record->attributeName() == html_names::kPartAttr) {
        auto* target = DynamicTo<Element>(record->target());
        if (target &&
            record->oldValue() != target->getAttribute(html_names::kPartAttr)) {
          PartRemoved(record->oldValue(), target);
          PartInserted(target->getAttribute(html_names::kPartAttr), target);
        }
      }
    } else if (record->type() == "childList") {
      for (unsigned i = 0; i < record->addedNodes()->length(); ++i) {
        auto* element = DynamicTo<Element>(record->addedNodes()->item(i));
        if (!element) {
          continue;
        }

        const AtomicString& part = element->getAttribute(html_names::kPartAttr);
        PartInserted(part, element);
      }

      for (unsigned i = 0; i < record->removedNodes()->length(); ++i) {
        auto* element = DynamicTo<Element>(record->removedNodes()->item(i));
        if (!element) {
          continue;
        }

        const AtomicString& part = element->getAttribute(html_names::kPartAttr);
        PartRemoved(part, element);
      }
    }
  }
}

template <typename StringType>
void HTMLSelectMenuElement::SelectMutationCallback::PartInserted(
    const StringType& part_name,
    Element* element) {
  if (part_name == kButtonPartName) {
    select_->ButtonPartInserted(element);
  } else if (part_name == kSelectedValuePartName) {
    select_->SelectedValuePartInserted(element);
  } else if (part_name == kListboxPartName) {
    select_->ListboxPartInserted(element);
  } else if (part_name == kOptionPartName || IsA<HTMLOptionElement>(element)) {
    select_->OptionPartInserted(element);
  }
}

template <typename StringType>
void HTMLSelectMenuElement::SelectMutationCallback::PartRemoved(
    const StringType& part_name,
    Element* element) {
  if (part_name == kButtonPartName) {
    select_->ButtonPartRemoved(element);
  } else if (part_name == kSelectedValuePartName) {
    select_->SelectedValuePartRemoved(element);
  } else if (part_name == kListboxPartName) {
    select_->ListboxPartRemoved(element);
  } else if (part_name == kOptionPartName || IsA<HTMLOptionElement>(element)) {
    select_->OptionPartRemoved(element);
  }
}

HTMLSelectMenuElement::HTMLSelectMenuElement(Document& document)
    : HTMLElement(html_names::kSelectmenuTag, document) {
  DCHECK(RuntimeEnabledFeatures::HTMLSelectMenuElementEnabled());
  DCHECK(RuntimeEnabledFeatures::HTMLPopupElementEnabled());
  UseCounter::Count(document, WebFeature::kSelectMenuElement);

  EnsureUserAgentShadowRoot();
  select_mutation_callback_ =
      MakeGarbageCollected<HTMLSelectMenuElement::SelectMutationCallback>(
          *this);
}

void HTMLSelectMenuElement::DidAddUserAgentShadowRoot(ShadowRoot& root) {
  DCHECK(IsShadowHost(this));

  root.EnableNameBasedSlotAssignment();
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
  listbox_slot->setAttribute(html_names::kNameAttr, kListboxPartName);

  listbox_part_ = MakeGarbageCollected<HTMLPopupElement>(document);
  listbox_part_->setAttribute(html_names::kPartAttr, kListboxPartName);

  auto* options_slot = MakeGarbageCollected<HTMLSlotElement>(document);

  button_part_->AppendChild(selected_value_part_);
  button_part_->AppendChild(button_icon);

  button_slot->AppendChild(button_part_);

  listbox_part_->appendChild(options_slot);
  listbox_slot->appendChild(listbox_part_);

  root.AppendChild(button_slot);
  root.AppendChild(listbox_slot);

  option_part_listener_ =
      MakeGarbageCollected<HTMLSelectMenuElement::OptionPartEventListener>(
          this);
}

String HTMLSelectMenuElement::value() {
  if (Element* option = SelectedOption()) {
    return option->innerText();
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

bool HTMLSelectMenuElement::IsValidButtonPart(const Element* part,
                                              bool show_warning) const {
  bool is_valid = !FlatTreeTraversal::IsDescendantOf(*part, *listbox_part_);
  if (!is_valid && show_warning) {
    GetDocument().AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kRendering,
        mojom::blink::ConsoleMessageLevel::kWarning,
        "A <selectmenu> must not contain an element labeled with "
        "part=\"button\" that is also a descendant of the \"listbox\" part. "
        "This <selectmenu> will not be fully functional."));
  }

  return is_valid;
}

bool HTMLSelectMenuElement::IsValidListboxPart(const Element* part,
                                               bool show_warning) const {
  if (!IsA<HTMLPopupElement>(part)) {
    if (show_warning) {
      GetDocument().AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
          mojom::blink::ConsoleMessageSource::kRendering,
          mojom::blink::ConsoleMessageLevel::kWarning,
          "Found non-<popup> element labeled as listbox under <selectmenu>, "
          "but only a <popup> can be used for the <selectmenu>'s listbox "
          "part. This <selectmenu> will not be fully functional."));
    }
    return false;
  }

  if (FlatTreeTraversal::IsDescendantOf(*part, *button_part_)) {
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

bool HTMLSelectMenuElement::IsValidOptionPart(const Element* part,
                                              bool show_warning) const {
  bool is_valid = FlatTreeTraversal::IsDescendantOf(*part, *listbox_part_);
  if (!is_valid && show_warning) {
    GetDocument().AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kRendering,
        mojom::blink::ConsoleMessageLevel::kWarning,
        "To receive option part controller code, an element labeled as an "
        "option must be a descendant of the <selectmenu>'s listbox "
        "part. This <selectmenu> will not be fully functional."));
  }
  return is_valid;
}

void HTMLSelectMenuElement::ButtonPartInserted(Element* new_button_part) {
  if (!IsValidButtonPart(new_button_part, /*show_warning=*/true)) {
    return;
  }

  // TODO(crbug.com/1191121) Decide which part gets controller code when there
  // are multiple parts available.
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
}

void HTMLSelectMenuElement::ButtonPartRemoved(Element* button_part) {
  if (button_part != button_part_) {
    return;
  }

  button_part_->removeEventListener(event_type_names::kClick,
                                    button_part_listener_, false);

  // Try to find a new button part by choosing the one that comes first in the
  // flat tree traversal.
  Element* new_button_part = nullptr;
  for (Node* node = FlatTreeTraversal::FirstChild(*this); node != nullptr;
       node = FlatTreeTraversal::Next(*node, this)) {
    auto* element = DynamicTo<Element>(node);
    if (!element) {
      continue;
    }

    if (element->getAttribute(html_names::kPartAttr) == kButtonPartName &&
        IsValidButtonPart(element, /*show_warning=*/false)) {
      new_button_part = element;
      break;
    }
  }

  if (new_button_part) {
    new_button_part->addEventListener(event_type_names::kClick,
                                      button_part_listener_, false);
  }
  button_part_ = new_button_part;
}

void HTMLSelectMenuElement::SelectedValuePartInserted(
    Element* new_selected_value_part) {
  // TODO(crbug.com/1191121) Decide which part gets controller code when there
  // are multiple parts available.
  selected_value_part_ = new_selected_value_part;
}

void HTMLSelectMenuElement::SelectedValuePartRemoved(
    Element* selected_value_part) {
  if (selected_value_part_ != selected_value_part) {
    return;
  }

  // Try to find a new selected value part by choosing the one that comes first
  // in the flat tree traversal.
  Element* new_selected_value_part = nullptr;
  for (Node* node = FlatTreeTraversal::FirstChild(*this); node != nullptr;
       node = FlatTreeTraversal::Next(*node, this)) {
    auto* element = DynamicTo<Element>(node);
    if (!element) {
      continue;
    }

    if (element->getAttribute(html_names::kPartAttr) ==
        kSelectedValuePartName) {
      new_selected_value_part = element;
      break;
    }
  }

  selected_value_part_ = new_selected_value_part;
}

void HTMLSelectMenuElement::ListboxPartInserted(Element* new_listbox_part) {
  if (!IsValidListboxPart(new_listbox_part, /*show_warning=*/true)) {
    return;
  }

  // TODO(crbug.com/1191121) Decide which part gets controller code when there
  // are multiple parts available.
  listbox_part_ = DynamicTo<HTMLPopupElement>(new_listbox_part);
  // TODO(crbug.com/1121840) Should the current option parts be revalidated?
}

void HTMLSelectMenuElement::ListboxPartRemoved(Element* listbox_part) {
  if (listbox_part_ != listbox_part) {
    return;
  }

  // Try to find a new listbox part by choosing the one that comes first in the
  // flat tree traversal.
  Element* new_listbox_part = nullptr;
  for (Node* node = FlatTreeTraversal::FirstChild(*this); node != nullptr;
       node = FlatTreeTraversal::Next(*node, this)) {
    auto* element = DynamicTo<Element>(node);
    if (!element) {
      continue;
    }

    if (element->getAttribute(html_names::kPartAttr) == kListboxPartName &&
        IsValidListboxPart(element, /*show_warning=*/false)) {
      new_listbox_part = element;
      break;
    }
  }

  listbox_part_ = DynamicTo<HTMLPopupElement>(new_listbox_part);
  // TODO(crbug.com/1121840) Should the current option parts be revalidated?
}

void HTMLSelectMenuElement::OptionPartInserted(Element* new_option_part) {
  if (!IsValidOptionPart(new_option_part, /*show_warning=*/true)) {
    return;
  }

  if (option_parts_.Contains(new_option_part)) {
    return;
  }

  if (auto* new_option_element =
          DynamicTo<HTMLOptionElement>(new_option_part)) {
    new_option_element->OptionInsertedIntoSelectMenuElement();
  }

  new_option_part->addEventListener(event_type_names::kClick,
                                    option_part_listener_, false);
  // TODO(crbug.com/1121840) We don't want to actually change the attribute,
  // and if tabindex is already set we shouldn't override it.  So we need to
  // come up with something else here.
  new_option_part->setTabIndex(-1);

  // TODO(crbug.com/1191131) The option part list should match the flat tree
  // order.
  option_parts_.insert(new_option_part);

  if (!selected_option_) {
    // If we didn't have a selected option previously, change the
    // selection to the first option part.
    SetSelectedOption(new_option_part);
  }
}

void HTMLSelectMenuElement::OptionPartRemoved(Element* option_part) {
  if (!option_parts_.Contains(option_part)) {
    return;
  }

  if (auto* option_element = DynamicTo<HTMLOptionElement>(option_part)) {
    option_element->OptionRemovedFromSelectMenuElement();
  }

  option_part->removeEventListener(event_type_names::kClick,
                                   option_part_listener_, false);
  // TODO(crbug.com/1121840) Whenever we figure out how to set
  // focusability properly (without using tabIndex), we should undo up
  // those changes here for elements that are no longer option parts.
  option_parts_.erase(option_part);

  if (selected_option_ == option_part) {
    // TODO(crbug.com/1121840) We should match the behavior from
    // https://html.spec.whatwg.org/C/#ask-for-a-reset
    // If the currently selected option was removed change the
    // selection to the first option part, if there is one.
    auto* first_option_part = FirstOptionPart();
    SetSelectedOption(first_option_part);
  }
}

Element* HTMLSelectMenuElement::FirstOptionPart() const {
  // TODO(crbug.com/1121840) This is going to be replaced by an option part
  // list iterator, or we could reuse OptionListIterator if we decide that just
  // <option>s are supported as option parts.
  for (Node* node = FlatTreeTraversal::FirstChild(*this); node != nullptr;
       node = FlatTreeTraversal::Next(*node, this)) {
    auto* element = DynamicTo<Element>(node);
    if (!element) {
      continue;
    }

    if ((element->getAttribute(html_names::kPartAttr) == kOptionPartName ||
         IsA<HTMLOptionElement>(element)) &&
        IsValidOptionPart(element, /*show_warning=*/false)) {
      return element;
    }
  }

  return nullptr;
}

void HTMLSelectMenuElement::EnsureSelectedOptionIsValid() {
  // TODO(crbug.com/1121840) Since we observe DOM tree mutation asynchronously
  // the selected option can become invalid. For now ensure that the selected
  // option is still valid before using it. In future, we may move to observe
  // DOM tree mutation synchronously.
  if (selected_option_ &&
      ((selected_option_->getAttribute(html_names::kPartAttr) !=
            kOptionPartName &&
        !IsA<HTMLOptionElement>(selected_option_.Get())) ||
       !IsValidOptionPart(selected_option_, /*show_warning=*/false))) {
    OptionPartRemoved(selected_option_);
  }
}

Element* HTMLSelectMenuElement::SelectedOption() {
  EnsureSelectedOptionIsValid();
  return selected_option_;
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

void HTMLSelectMenuElement::Trace(Visitor* visitor) const {
  visitor->Trace(button_part_listener_);
  visitor->Trace(option_part_listener_);
  visitor->Trace(select_mutation_callback_);
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
