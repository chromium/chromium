// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/html_select_list_element.h"

#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/core/css/properties/longhand.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
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
#include "third_party/blink/renderer/core/html/forms/html_opt_group_element.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/select_list_part_traversal.h"
#include "third_party/blink/renderer/core/html/html_collection.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/html/html_style_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/keywords.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"

namespace blink {
namespace {
class PreviewPopoverInnerElement : public HTMLDivElement {
 public:
  explicit PreviewPopoverInnerElement(Document& document)
      : HTMLDivElement(document) {
    SetHasCustomStyleCallbacks();
  }

 private:
  const ComputedStyle* CustomStyleForLayoutObject(
      const StyleRecalcContext& style_recalc_context) override {
    HTMLSelectListElement* selectlist =
        DynamicTo<HTMLSelectListElement>(OwnerShadowHost());
    if (!selectlist || !selectlist->ButtonPart() ||
        !selectlist->ButtonPart()->GetComputedStyle()) {
      return HTMLDivElement::CustomStyleForLayoutObject(style_recalc_context);
    }

    const ComputedStyle& button_style =
        selectlist->ButtonPart()->ComputedStyleRef();
    const ComputedStyle* original_style =
        OriginalStyleForLayoutObject(style_recalc_context);
    ComputedStyleBuilder style_builder(*original_style);
    if (button_style.HasAuthorBorderRadius()) {
      style_builder.SetBorderBottomLeftRadius(
          button_style.BorderBottomLeftRadius());
      style_builder.SetBorderBottomRightRadius(
          button_style.BorderBottomRightRadius());
      style_builder.SetBorderTopLeftRadius(button_style.BorderTopLeftRadius());
      style_builder.SetBorderTopRightRadius(
          button_style.BorderTopRightRadius());
    }
    if (button_style.HasAuthorBorder()) {
      style_builder.SetBorderBottomColor(
          button_style.BorderBottom().GetColor());
      style_builder.SetBorderLeftColor(button_style.BorderLeft().GetColor());
      style_builder.SetBorderRightColor(button_style.BorderRight().GetColor());
      style_builder.SetBorderTopColor(button_style.BorderTop().GetColor());

      style_builder.SetBorderBottomWidth(button_style.BorderBottomWidth());
      style_builder.SetBorderLeftWidth(button_style.BorderLeftWidth());
      style_builder.SetBorderRightWidth(button_style.BorderRightWidth());
      style_builder.SetBorderTopWidth(button_style.BorderTopWidth());

      style_builder.SetBorderBottomStyle(button_style.BorderBottomStyle());
      style_builder.SetBorderLeftStyle(button_style.BorderLeftStyle());
      style_builder.SetBorderRightStyle(button_style.BorderRightStyle());
      style_builder.SetBorderTopStyle(button_style.BorderTopStyle());
    }

    style_builder.SetPaddingBottom(button_style.PaddingBottom());
    style_builder.SetPaddingLeft(button_style.PaddingLeft());
    style_builder.SetPaddingRight(button_style.PaddingRight());
    style_builder.SetPaddingTop(button_style.PaddingTop());

    return style_builder.TakeStyle();
  }
};

}  // anonymous namespace

class HTMLSelectListElement::SelectMutationCallback
    : public GarbageCollected<HTMLSelectListElement::SelectMutationCallback>,
      public SynchronousMutationObserver {
 public:
  explicit SelectMutationCallback(HTMLSelectListElement& select);

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

  Member<HTMLSelectListElement> select_;
};

HTMLSelectListElement::SelectMutationCallback::SelectMutationCallback(
    HTMLSelectListElement& select)
    : select_(select) {
  SetDocument(&select_->GetDocument());
}

void HTMLSelectListElement::SelectMutationCallback::Trace(
    Visitor* visitor) const {
  visitor->Trace(select_);
  SynchronousMutationObserver::Trace(visitor);
}

void HTMLSelectListElement::SelectMutationCallback::DidChangeChildren(
    const ContainerNode& container,
    const ContainerNode::ChildrenChange& change) {
  if (!select_->IsShadowIncludingInclusiveAncestorOf(container))
    return;

  if (change.type == ChildrenChangeType::kElementInserted) {
    auto* root_node = change.sibling_changed;
    for (auto* node = root_node; node != nullptr;
         node = SelectListPartTraversal::Next(*node, root_node)) {
      if (auto* element = DynamicTo<HTMLElement>(node)) {
        const AtomicString& part =
            element->getAttribute(html_names::kBehaviorAttr);
        PartInserted(part, element);
        SlotChanged(element->SlotName());

        if (element->HasTagName(html_names::kSelectedoptionTag)) {
          select_->UpdateSelectedValuePart();
        } else if (IsA<HTMLListboxElement>(element)) {
          select_->UpdateListboxPart();
        }
      }
    }
  } else if (change.type == ChildrenChangeType::kElementRemoved) {
    auto* root_node = change.sibling_changed;
    for (auto* node = root_node; node != nullptr;
         node = SelectListPartTraversal::Next(*node, root_node)) {
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

void HTMLSelectListElement::SelectMutationCallback::AttributeChanged(
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
      // an earlier child of the <selectlist> which makes
      // FirstValidListboxPart() return a different element.
      select_->UpdateListboxPart();
    }
  }
}

template <typename StringType>
void HTMLSelectListElement::SelectMutationCallback::PartInserted(
    const StringType& part_name,
    HTMLElement* element) {
  if (part_name == kButtonPartName) {
    select_->ButtonPartInserted(element);
  } else if (part_name == kSelectedValuePartName) {
    select_->SelectedValuePartInserted(element);
  } else if (part_name == kListboxPartName) {
    select_->ListboxPartInserted(element);
  } else if (auto* options_element = DynamicTo<HTMLOptionElement>(element)) {
    select_->OptionPartInserted(options_element);
  }
}

template <typename StringType>
void HTMLSelectListElement::SelectMutationCallback::PartRemoved(
    const StringType& part_name,
    HTMLElement* element) {
  if (part_name == kButtonPartName) {
    select_->ButtonPartRemoved(element);
  } else if (part_name == kSelectedValuePartName) {
    select_->SelectedValuePartRemoved(element);
  } else if (part_name == kListboxPartName) {
    select_->ListboxPartRemoved(element);
  } else if (auto* options_element = DynamicTo<HTMLOptionElement>(element)) {
    select_->OptionPartRemoved(options_element);
  }
}

template <typename StringType>
void HTMLSelectListElement::SelectMutationCallback::SlotChanged(
    const StringType& slot_name) {
  if (slot_name == kListboxPartName) {
    select_->UpdateListboxPart();
  } else if (slot_name == kButtonPartName) {
    select_->UpdateButtonPart();
    select_->UpdateSelectedValuePart();
  }
}

HTMLSelectListElement::HTMLSelectListElement(Document& document)
    : HTMLFormControlElementWithState(html_names::kSelectlistTag, document),
      type_ahead_(this) {
  DCHECK(RuntimeEnabledFeatures::HTMLSelectListElementEnabled());
  UseCounter::Count(document, WebFeature::kSelectListElement);

  EnsureUserAgentShadowRoot().SetSlotAssignmentMode(
      SlotAssignmentMode::kManual);
  select_mutation_callback_ =
      MakeGarbageCollected<HTMLSelectListElement::SelectMutationCallback>(
          *this);

  // A selectlist is the implicit anchor of its listbox and of the autofill
  // preview.
  IncrementImplicitlyAnchoredElementCount();
  IncrementImplicitlyAnchoredElementCount();
}

namespace {
bool HasOptionElementDescendant(Element* element) {
  for (auto& descendant : ElementTraversal::DescendantsOf(*element)) {
    if (DynamicTo<HTMLOptionElement>(descendant)) {
      return true;
    }
  }
  return false;
}
}  // namespace

void HTMLSelectListElement::ManuallyAssignSlots() {
  Element* explicit_button = nullptr;
  VectorOf<Node> button_nodes;
  Element* listbox = nullptr;
  Element* selected_value = nullptr;
  Element* marker = nullptr;
  VectorOf<Node> options;
  for (Node& node : NodeTraversal::ChildrenOf(*this)) {
    if (auto* element = DynamicTo<Element>(node)) {
      if (!explicit_button && element->SlotName() == kButtonPartName) {
        explicit_button = element;
      } else if (!listbox && (element->SlotName() == kListboxPartName ||
                              IsA<HTMLListboxElement>(element))) {
        listbox = element;
      } else if (!selected_value &&
                 element->SlotName() == kSelectedValuePartName) {
        selected_value = element;
      } else if (!marker && element->SlotName() == kMarkerPartName) {
        marker = element;
      } else if (auto* option = DynamicTo<HTMLOptionElement>(element)) {
        options.push_back(option);
      } else if (auto* optgroup = DynamicTo<HTMLOptGroupElement>(element)) {
        options.push_back(optgroup);
      } else if (HasOptionElementDescendant(element)) {
        options.push_back(element);
      } else {
        button_nodes.push_back(element);
      }
    } else if (auto* text = DynamicTo<Text>(node)) {
      if (!text->ContainsOnlyWhitespaceOrEmpty()) {
        button_nodes.push_back(node);
      }
    }
  }

  if (explicit_button) {
    button_slot_->Assign(explicit_button);
  } else {
    button_slot_->Assign(button_nodes);
  }
  listbox_slot_->Assign(listbox);
  selected_value_slot_->Assign(selected_value);
  marker_slot_->Assign(marker);
  options_slot_->Assign(options);
}

// static
HTMLSelectListElement* HTMLSelectListElement::OwnerSelectList(Node* node) {
  // Do some quick checks in order to avoid, in most cases, walking up the
  // entire tree if `node` does not have a selectlist ancestor.
  if (!IsA<HTMLOptionElement>(node)) {
    HTMLElement* html_element = DynamicTo<HTMLElement>(node);
    if (!html_element ||
        !html_element->FastHasAttribute(html_names::kBehaviorAttr)) {
      return nullptr;
    }
  }

  HTMLSelectListElement* nearest_select_list_ancestor =
      SelectListPartTraversal::NearestSelectListAncestor(*node);

  if (nearest_select_list_ancestor &&
      nearest_select_list_ancestor->AssignedPartType(node) != PartType::kNone) {
    return nearest_select_list_ancestor;
  }

  return nullptr;
}

HTMLSelectListElement::PartType HTMLSelectListElement::AssignedPartType(
    Node* node) const {
  if (node == button_part_) {
    return PartType::kButton;
  } else if (node == listbox_part_) {
    return PartType::kListBox;
  } else if (auto* option_element = DynamicTo<HTMLOptionElement>(node);
             option_element && option_parts_.Contains(option_element)) {
    return PartType::kOption;
  }

  return PartType::kNone;
}

const HTMLSelectListElement::ListItems& HTMLSelectListElement::GetListItems()
    const {
  if (should_recalc_list_items_) {
    RecalcListItems();
  }
  return list_items_;
}

void HTMLSelectListElement::DidAddUserAgentShadowRoot(ShadowRoot& root) {
  DCHECK(IsShadowHost(this));

  root.SetDelegatesFocus(true);

  Document& document = GetDocument();

  AtomicString button_part(kButtonPartName);
  button_slot_ = MakeGarbageCollected<HTMLSlotElement>(document);
  button_slot_->setAttribute(html_names::kNameAttr, button_part);

  button_part_ = MakeGarbageCollected<HTMLButtonElement>(document);
  button_part_->setAttribute(html_names::kPartAttr, button_part);
  button_part_->setAttribute(html_names::kBehaviorAttr, button_part);
  button_part_->SetShadowPseudoId(AtomicString("-internal-selectlist-button"));
  button_part_listener_ =
      MakeGarbageCollected<HTMLSelectListElement::ButtonPartEventListener>(
          this);
  button_part_listener_->AddEventListeners(button_part_);

  AtomicString selected_value_part(kSelectedValuePartName);
  selected_value_slot_ = MakeGarbageCollected<HTMLSlotElement>(document);
  selected_value_slot_->setAttribute(html_names::kNameAttr,
                                     selected_value_part);

  selected_value_part_ = MakeGarbageCollected<HTMLDivElement>(document);
  selected_value_part_->setAttribute(html_names::kPartAttr,
                                     selected_value_part);
  selected_value_part_->setAttribute(html_names::kBehaviorAttr,
                                     selected_value_part);
  selected_value_part_->SetShadowPseudoId(
      AtomicString("-internal-selectlist-selected-value"));

  AtomicString marker_part(kMarkerPartName);
  marker_slot_ = MakeGarbageCollected<HTMLSlotElement>(document);
  marker_slot_->setAttribute(html_names::kNameAttr, marker_part);

  auto* marker_icon = MakeGarbageCollected<HTMLDivElement>(document);
  marker_icon->SetShadowPseudoId(
      AtomicString("-internal-selectlist-button-icon"));
  marker_icon->setAttribute(html_names::kPartAttr, marker_part);

  AtomicString listbox_part(kListboxPartName);
  listbox_slot_ = MakeGarbageCollected<HTMLSlotElement>(document);
  listbox_slot_->setAttribute(html_names::kNameAttr, listbox_part);

  HTMLElement* new_popover;
  new_popover = MakeGarbageCollected<HTMLDivElement>(document);
  new_popover->setAttribute(html_names::kPopoverAttr, keywords::kAuto);
  new_popover->setAttribute(html_names::kPartAttr, listbox_part);
  new_popover->setAttribute(html_names::kBehaviorAttr, listbox_part);
  new_popover->SetShadowPseudoId(AtomicString("-internal-selectlist-listbox"));
  SetListboxPart(new_popover);

  options_slot_ = MakeGarbageCollected<HTMLSlotElement>(document);

  button_part_->AppendChild(selected_value_slot_);
  button_part_->AppendChild(marker_slot_);

  selected_value_slot_->AppendChild(selected_value_part_);

  marker_slot_->AppendChild(marker_icon);

  button_slot_->AppendChild(button_part_);

  listbox_part_->appendChild(options_slot_);
  listbox_slot_->appendChild(listbox_part_);

  root.AppendChild(button_slot_);
  root.AppendChild(listbox_slot_);

  option_part_listener_ =
      MakeGarbageCollected<HTMLSelectListElement::OptionPartEventListener>(
          this);

  suggested_option_popover_ =
      MakeGarbageCollected<PreviewPopoverInnerElement>(document);
  suggested_option_popover_->setAttribute(html_names::kPopoverAttr,
                                          keywords::kManual);
  suggested_option_popover_->SetPopoverOwnerSelectListElement(this);
  suggested_option_popover_->SetShadowPseudoId(
      AtomicString("-internal-selectlist-preview"));
  root.AppendChild(suggested_option_popover_);

  auto* style =
      MakeGarbageCollected<HTMLStyleElement>(document, CreateElementFlags());
  // For small touch screens, expand the touch targets and enlarge the overall
  // picker, akin to the native <select> picker.
  // TODO(crbug.com/1121840) Add back (pointer: coarse) once testing is
  // complete:
  //   @media (pointer: coarse) and (max-width: 500px) {
  style->setInnerHTML(R"CSS(
    @media (max-width: 500px) {
      ::backdrop,
      slot[name=listbox]::slotted([popover])::backdrop {
        background-color: rgba(0, 0, 0, .7);
      }
      slot[name=listbox]>div[popover=auto],
      slot[name=listbox]::slotted([popover]) {
        box-shadow: none;
        border-radius: 0.5em;
        padding: 0.25em 0;
        border: none;
        max-height: 50%;
        inset: 0;
        margin: auto;
      }
      ::slotted(option) {
        padding: 0.5em;
        font-size: 1em;
      }
    }
  )CSS");
  root.AppendChild(style);
}

void HTMLSelectListElement::DidMoveToNewDocument(Document& old_document) {
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

void HTMLSelectListElement::DisabledAttributeChanged() {
  HTMLFormControlElementWithState::DisabledAttributeChanged();
  if (GetShadowRoot()) {
    // Clear "delegates focus" property to make selectlist unfocusable.
    GetShadowRoot()->SetDelegatesFocus(!IsDisabledFormControl());
  }
}

String HTMLSelectListElement::value() const {
  if (HTMLOptionElement* option = selectedOption()) {
    return option->value();
  }
  return "";
}

void HTMLSelectListElement::setValueForBinding(const String& value) {
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

void HTMLSelectListElement::setValue(const String& value,
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
  SetSuggestedOption(nullptr);
  SetSelectedOption(selected_option, send_events, autofill_state);
}

bool HTMLSelectListElement::open() const {
  // TODO(crbug.com/1121840) listbox_part_ can be null if
  // the author has filled the listbox slot without including
  // a replacement listbox part. Instead of null checks like this,
  // we should consider refusing to render the control at all if
  // either of the key parts (button or listbox) are missing.
  if (!listbox_part_)
    return false;
  return listbox_part_->HasPopoverAttribute() && listbox_part_->popoverOpen();
}

void HTMLSelectListElement::SetAutofillValue(const String& value,
                                             WebAutofillState autofill_state) {
  bool user_has_edited_the_field = user_has_edited_the_field_;
  setValue(value, /*send_events=*/true, autofill_state);
  SetUserHasEditedTheField(user_has_edited_the_field);
}

String HTMLSelectListElement::SuggestedValue() const {
  return suggested_option_ ? suggested_option_->value() : "";
}

void HTMLSelectListElement::SetSuggestedValue(const String& value) {
  if (value.IsNull()) {
    SetSuggestedOption(nullptr);
    return;
  }

  for (auto& option : option_parts_) {
    if (option->value() == value) {
      SetSuggestedOption(option);
      return;
    }
  }

  SetSuggestedOption(nullptr);
}

void HTMLSelectListElement::OpenListbox() {
  if (listbox_part_ && !open()) {
    listbox_part_->showPopover(ASSERT_NO_EXCEPTION);
    PseudoStateChanged(CSSSelector::kPseudoClosed);
    PseudoStateChanged(CSSSelector::kPseudoOpen);
    if (selectedOption()) {
      selectedOption()->Focus(FocusParams(FocusTrigger::kUserGesture));
    }
    selected_option_when_listbox_opened_ = selectedOption();
  }
}

void HTMLSelectListElement::CloseListbox() {
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

bool HTMLSelectListElement::TypeAheadFind(const KeyboardEvent& event,
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
    selectedOption()->Focus(FocusParams(FocusTrigger::kUserGesture));
  }

  selected_option_->SetDirty(true);
  return true;
}

void HTMLSelectListElement::ListboxWasClosed() {
  PseudoStateChanged(CSSSelector::kPseudoClosed);
  PseudoStateChanged(CSSSelector::kPseudoOpen);
  if (button_part_) {
    button_part_->Focus(FocusParams(FocusTrigger::kUserGesture));
  }
  if (selectedOption() != selected_option_when_listbox_opened_) {
    DispatchChangeEvent();
  }
}

void HTMLSelectListElement::ResetTypeAheadSessionForTesting() {
  type_ahead_.ResetSession();
}

bool HTMLSelectListElement::SetListboxPart(HTMLElement* new_listbox_part) {
  if (listbox_part_ == new_listbox_part)
    return false;

  if (listbox_part_) {
    listbox_part_->SetPopoverOwnerSelectListElement(nullptr);
  }

  if (new_listbox_part) {
    new_listbox_part->SetPopoverOwnerSelectListElement(this);
  } else {
    QueueCheckForMissingParts();
  }

  listbox_part_ = new_listbox_part;
  return true;
}

bool HTMLSelectListElement::IsValidButtonPart(const Node* node,
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
        "button must not be a descendant of the <selectlist>'s listbox "
        "part. This <selectlist> will not be fully functional."));
  }

  return is_valid_tree_position;
}

bool HTMLSelectListElement::IsValidListboxPart(const Node* node,
                                               bool show_warning) const {
  auto* element = DynamicTo<HTMLElement>(node);
  if (!element) {
    return false;
  }

  if (IsA<HTMLListboxElement>(element) && element->parentNode() == this) {
    return true;
  }

  if (element->getAttribute(html_names::kBehaviorAttr) != kListboxPartName) {
    return false;
  }

  if (!element->HasPopoverAttribute()) {
    if (show_warning) {
      GetDocument().AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
          mojom::blink::ConsoleMessageSource::kRendering,
          mojom::blink::ConsoleMessageLevel::kWarning,
          "Found non-popover element labeled as listbox under "
          "<selectlist>, which is not allowed. The <selectlist>'s "
          "listbox element must have a valid value set for the 'popover' "
          "attribute. This <selectlist> will not be fully functional."));
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
          "listbox must not be a descendant of the <selectlist>'s button "
          "part. This <selectlist> will not be fully functional."));
    }
    return false;
  }

  // We only get here if behavior=listbox.
  return true;
}

bool HTMLSelectListElement::IsValidOptionPart(const Node* node,
                                              bool show_warning) const {
  auto* element = DynamicTo<HTMLElement>(node);
  if (!element || !IsA<HTMLOptionElement>(element)) {
    return false;
  }

  bool is_valid_tree_position =
      listbox_part_ &&
      SelectListPartTraversal::IsDescendantOf(*element, *listbox_part_);
  if (!is_valid_tree_position && show_warning) {
    GetDocument().AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kRendering,
        mojom::blink::ConsoleMessageLevel::kWarning,
        "To receive option part controller code, an element labeled as an "
        "option must be a descendant of the <selectlist>'s listbox "
        "part. This <selectlist> will not be fully functional."));
  }
  return is_valid_tree_position;
}

HTMLElement* HTMLSelectListElement::FirstValidButtonPart() const {
  for (Node* node = SelectListPartTraversal::FirstChild(*this); node;
       node = SelectListPartTraversal::Next(*node, this)) {
    if (IsValidButtonPart(node, /*show_warning=*/false)) {
      return DynamicTo<HTMLElement>(node);
    }
  }

  return nullptr;
}

void HTMLSelectListElement::SetButtonPart(HTMLElement* new_button_part) {
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

void HTMLSelectListElement::ButtonPartInserted(HTMLElement* new_button_part) {
  if (!IsValidButtonPart(new_button_part, /*show_warning=*/true)) {
    return;
  }

  UpdateButtonPart();
}

void HTMLSelectListElement::ButtonPartRemoved(HTMLElement* button_part) {
  if (button_part != button_part_) {
    return;
  }

  UpdateButtonPart();
}

void HTMLSelectListElement::UpdateButtonPart() {
  SetButtonPart(FirstValidButtonPart());
}

void HTMLSelectListElement::EnsureButtonPartIsValid() {
  if (!button_part_ ||
      !SelectListPartTraversal::IsDescendantOf(*button_part_, *this) ||
      !IsValidButtonPart(button_part_, /*show_warning*/ false)) {
    UpdateButtonPart();
  }
}

HTMLElement* HTMLSelectListElement::FirstValidSelectedValuePart() const {
  for (Node* node = SelectListPartTraversal::FirstChild(*this); node;
       node = SelectListPartTraversal::Next(*node, this)) {
    auto* element = DynamicTo<HTMLElement>(node);
    if (!element) {
      continue;
    }

    if (element->getAttribute(html_names::kBehaviorAttr) ==
            kSelectedValuePartName ||
        element->HasTagName(html_names::kSelectedoptionTag)) {
      return element;
    }
  }
  return nullptr;
}

void HTMLSelectListElement::SelectedValuePartInserted(
    HTMLElement* new_selected_value_part) {
  UpdateSelectedValuePart();
}

void HTMLSelectListElement::SelectedValuePartRemoved(
    HTMLElement* selected_value_part) {
  if (selected_value_part != selected_value_part_) {
    return;
  }
  UpdateSelectedValuePart();
}

void HTMLSelectListElement::UpdateSelectedValuePart() {
  selected_value_part_ = FirstValidSelectedValuePart();
}

void HTMLSelectListElement::EnsureSelectedValuePartIsValid() {
  if (!selected_value_part_ ||
      selected_value_part_->getAttribute(html_names::kBehaviorAttr) !=
          kSelectedValuePartName ||
      !selected_value_part_->HasTagName(html_names::kSelectedoptionTag) ||
      !SelectListPartTraversal::IsDescendantOf(*selected_value_part_, *this)) {
    UpdateSelectedValuePart();
  }
}

HTMLElement* HTMLSelectListElement::FirstValidListboxPart() const {
  for (Node* node = SelectListPartTraversal::FirstChild(*this); node;
       node = SelectListPartTraversal::Next(*node, this)) {
    if (IsValidListboxPart(node, /*show_warning=*/false)) {
      return DynamicTo<HTMLElement>(node);
    }
  }
  return nullptr;
}

void HTMLSelectListElement::ListboxPartInserted(HTMLElement* new_listbox_part) {
  if (!IsValidListboxPart(new_listbox_part, /*show_warning=*/true)) {
    return;
  }
  UpdateListboxPart();
}

void HTMLSelectListElement::ListboxPartRemoved(HTMLElement* listbox_part) {
  if (listbox_part_ != listbox_part) {
    return;
  }
  UpdateListboxPart();
}

void HTMLSelectListElement::UpdateListboxPart() {
  if (!SetListboxPart(FirstValidListboxPart()))
    return;
  ResetOptionParts();
}

void HTMLSelectListElement::EnsureListboxPartIsValid() {
  if (!listbox_part_ ||
      !SelectListPartTraversal::IsDescendantOf(*listbox_part_, *this) ||
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

void HTMLSelectListElement::QueueCheckForMissingParts() {
  if (!queued_check_for_missing_parts_ && GetDocument().View()) {
    queued_check_for_missing_parts_ = true;
    GetDocument().View()->RegisterForLifecycleNotifications(this);
  }
}

void HTMLSelectListElement::ResetOptionParts() {
  // Remove part status from all current option parts
  while (!option_parts_.empty()) {
    OptionPartRemoved(option_parts_.back());
  }

  // Find new option parts under the new listbox
  for (Node* node = SelectListPartTraversal::FirstChild(*this); node;
       node = SelectListPartTraversal::Next(*node, this)) {
    if (IsValidOptionPart(node, /*show_warning=*/false)) {
      OptionPartInserted(DynamicTo<HTMLOptionElement>(node));
    }
  }
}

void HTMLSelectListElement::DispatchInputAndChangeEventsIfNeeded() {
  DispatchInputEvent();
  if (!open()) {
    // Only fire change if the listbox is already closed, because if it's open
    // we'll  fire change later when the listbox closes.
    DispatchChangeEvent();
  }
}

void HTMLSelectListElement::DispatchInputEvent() {
  Event* input_event = Event::CreateBubble(event_type_names::kInput);
  input_event->SetComposed(true);
  DispatchScopedEvent(*input_event);
}

void HTMLSelectListElement::DispatchChangeEvent() {
  DispatchScopedEvent(*Event::CreateBubble(event_type_names::kChange));
}

void HTMLSelectListElement::OptionPartInserted(
    HTMLOptionElement* new_option_part) {
  if (!IsValidOptionPart(new_option_part, /*show_warning=*/true)) {
    return;
  }

  if (option_parts_.Contains(new_option_part)) {
    return;
  }

  new_option_part->OptionInsertedIntoSelectListElement();
  option_part_listener_->AddEventListeners(new_option_part);

  // TODO(crbug.com/1191131) The option part list should match the flat tree
  // order.
  option_parts_.insert(new_option_part);

  if (!selected_option_ || new_option_part->Selected()) {
    SetSelectedOption(new_option_part);
  }
  SetNeedsValidityCheck();
  should_recalc_list_items_ = true;

  if (GetDocument().IsActive()) {
    GetDocument()
        .GetFrame()
        ->GetPage()
        ->GetChromeClient()
        .SelectOrSelectListFieldOptionsChanged(*this);
  }
}

void HTMLSelectListElement::OptionPartRemoved(HTMLOptionElement* option_part) {
  if (!option_parts_.Contains(option_part)) {
    return;
  }

  option_part->OptionRemovedFromSelectListElement();
  option_part_listener_->RemoveEventListeners(option_part);
  option_parts_.erase(option_part);

  if (selected_option_ == option_part) {
    ResetToDefaultSelection();
  }
  if (suggested_option_ == option_part) {
    SetSuggestedOption(nullptr);
  }
  SetNeedsValidityCheck();
  should_recalc_list_items_ = true;

  if (GetDocument().IsActive()) {
    GetDocument()
        .GetFrame()
        ->GetPage()
        ->GetChromeClient()
        .SelectOrSelectListFieldOptionsChanged(*this);
  }
}

HTMLOptionElement* HTMLSelectListElement::FirstOptionPart() const {
  // TODO(crbug.com/1121840) This is going to be replaced by an option part
  // list iterator, or we could reuse OptionListIterator if we decide that just
  // <option>s are supported as option parts.
  for (Node* node = SelectListPartTraversal::FirstChild(*this); node;
       node = SelectListPartTraversal::Next(*node, this)) {
    if (IsValidOptionPart(node, /*show_warning=*/false)) {
      return DynamicTo<HTMLOptionElement>(node);
    }
  }

  return nullptr;
}

void HTMLSelectListElement::OptionSelectionStateChanged(
    HTMLOptionElement* option,
    bool option_is_selected) {
  DCHECK(option_parts_.Contains(option));
  if (option_is_selected) {
    SetSelectedOption(option);
  } else if (selectedOption() == option) {
    ResetToDefaultSelection();
  }
}

void HTMLSelectListElement::DidFinishLifecycleUpdate(
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
        "A <selectlist>'s default button was removed and a new one was not "
        "provided. This <selectlist> will not be fully functional."));
  }

  if (!listbox_part_) {
    GetDocument().AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kRendering,
        mojom::blink::ConsoleMessageLevel::kWarning,
        "A <selectlist>'s default listbox was removed and a new one was not "
        "provided. This <selectlist> will not be fully functional."));
  }
}

int HTMLSelectListElement::IndexOfSelectedOption() const {
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

int HTMLSelectListElement::OptionCount() const {
  return GetListItems().size();
}

String HTMLSelectListElement::OptionAtIndex(int index) const {
  HTMLOptionElement* option = OptionAtListIndex(index);
  if (!option || option->IsDisabledFormControl()) {
    return String();
  }
  return option->DisplayLabel();
}

HTMLOptionElement* HTMLSelectListElement::selectedOption() const {
  DCHECK(!selected_option_ ||
         IsValidOptionPart(selected_option_, /*show_warning=*/false));
  return selected_option_;
}

void HTMLSelectListElement::SetSelectedOption(
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

void HTMLSelectListElement::OptionElementChildrenChanged(
    const HTMLOptionElement& option) {
  if (selected_option_ == &option) {
    SetNeedsValidityCheck();
    NotifyFormStateChanged();
    UpdateSelectedValuePartContents();
  }
}

void HTMLSelectListElement::OptionElementValueChanged(
    const HTMLOptionElement& option) {
  if (selected_option_ == &option) {
    SetNeedsValidityCheck();
    NotifyFormStateChanged();
  }
}

void HTMLSelectListElement::SelectNextOption() {
  for (Node* node = SelectListPartTraversal::Next(*selectedOption(), this);
       node; node = SelectListPartTraversal::Next(*node, this)) {
    if (IsValidOptionPart(node, /*show_warning=*/false)) {
      auto* element = DynamicTo<HTMLOptionElement>(node);
      if (element->IsDisabledFormControl())
        continue;
      SetSelectedOption(element);
      element->Focus(FocusParams(FocusTrigger::kUserGesture));
      DispatchInputAndChangeEventsIfNeeded();
      return;
    }
  }
}

void HTMLSelectListElement::SelectPreviousOption() {
  for (Node* node = SelectListPartTraversal::Previous(*selectedOption(), this);
       node; node = SelectListPartTraversal::Previous(*node, this)) {
    if (IsValidOptionPart(node, /*show_warning=*/false)) {
      auto* element = DynamicTo<HTMLOptionElement>(node);
      if (element->IsDisabledFormControl())
        continue;
      SetSelectedOption(element);
      element->Focus(FocusParams(FocusTrigger::kUserGesture));
      DispatchInputAndChangeEventsIfNeeded();
      return;
    }
  }
}

void HTMLSelectListElement::SetSuggestedOption(HTMLOptionElement* option) {
  if (suggested_option_ == option) {
    return;
  }

  SetAutofillState(option ? WebAutofillState::kPreviewed
                          : WebAutofillState::kNotFilled);
  suggested_option_ = option;

  if (suggested_option_) {
    suggested_option_popover_->showPopover(ASSERT_NO_EXCEPTION);
    suggested_option_popover_->setInnerText(option->label());
  } else if (suggested_option_popover_->popoverOpen()) {
    suggested_option_popover_->HidePopoverInternal(
        HidePopoverFocusBehavior::kNone,
        HidePopoverTransitionBehavior::kNoEventsNoWaiting,
        /*exception_state=*/nullptr);
  }
}

void HTMLSelectListElement::UpdateSelectedValuePartContents() {
  // Null-check here because the selected-value part is optional; the author
  // might replace the button contents and not provide a selected-value part if
  // they want to show something in the button other than the current value of
  // the <selectlist>.
  if (selected_value_part_) {
    // TODO(crbug.com/1121840): when we remove the old architecture, this
    // should be a CHECK that selected_value_part_ is a <selectedoption>.
    if (selected_value_part_->HasTagName(html_names::kSelectedoptionTag) &&
        selected_option_) {
      // TODO(crbug.com/1121840): should the label attribute be used instead if
      // it is specified?
      auto* clone = selected_option_->cloneNode(/*deep=*/true);
      VectorOf<Node> nodes;
      for (Node& child : NodeTraversal::ChildrenOf(*clone)) {
        nodes.push_back(child);
      }
      // TODO(crbug.com/1121840): Instead of using RemoveChildren and
      // AppendChild, we should be using replaceChildren. replaceChildren is
      // currently only called from V8 code and uses V8 unions which makes it
      // hard to call from here but we should make a new ReplaceChildren method
      // that takes normal nodes.
      selected_value_part_->RemoveChildren();
      for (Member<Node> child : nodes) {
        selected_value_part_->AppendChild(child);
      }
    } else {
      selected_value_part_->setTextContent(
          selected_option_ ? selected_option_->innerText() : "");
    }
  }
}

void HTMLSelectListElement::RecalcListItems() const {
  list_items_.clear();
  for (Node* node = SelectListPartTraversal::FirstChild(*this); node;
       node = SelectListPartTraversal::Next(*node, this)) {
    if (IsValidOptionPart(node, /*show_warning=*/false)) {
      list_items_.push_back(To<HTMLOptionElement>(node));
    }
  }
}

HTMLOptionElement* HTMLSelectListElement::OptionAtListIndex(
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

void HTMLSelectListElement::ButtonPartEventListener::Invoke(ExecutionContext*,
                                                            Event* event) {
  select_list_element_->HandleButtonEvent(*event);
}

void HTMLSelectListElement::HandleButtonEvent(Event& event) {
  if (event.defaultPrevented()) {
    return;
  }

  if (event.type() == event_type_names::kClick && !IsDisabledFormControl()) {
    if (!open()) {
      OpenListbox();
    }
    // TODO(crbug.com/1408838) Close list box if dialog is open.
  } else if (event.type() == event_type_names::kBlur) {
    type_ahead_.ResetSession();
  } else if (event.IsKeyboardEvent()) {
    auto* keyboard_event = DynamicTo<KeyboardEvent>(event);
    if (!keyboard_event) {
      return;
    }

    if (!open() && !IsDisabledFormControl() &&
        HandleButtonKeyboardEvent(*keyboard_event)) {
      event.SetDefaultHandled();
    }
  }
}

void HTMLSelectListElement::ButtonPartEventListener::AddEventListeners(
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

void HTMLSelectListElement::ButtonPartEventListener::RemoveEventListeners(
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

bool HTMLSelectListElement::HandleButtonKeyboardEvent(KeyboardEvent& event) {
  if (event.keyCode() == VKEY_SPACE) {
    if (event.type() == event_type_names::kKeydown) {
      if (type_ahead_.HasActiveSession(event)) {
        TypeAheadFind(event, ' ');
      } else {
        OpenListbox();
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
    OpenListbox();
    return true;
  }
  // Handled in event_type_names::kKeypress event handler because
  // KeyboardEvent::charCode() == 0 for event_type_names::kKeydown.
  return event.type() == event_type_names::kKeypress &&
         TypeAheadFind(event, event.charCode());
}

void HTMLSelectListElement::OptionPartEventListener::Invoke(ExecutionContext*,
                                                            Event* event) {
  if (event->defaultPrevented())
    return;

  if (event->type() == event_type_names::kClick) {
    auto* target_element =
        DynamicTo<HTMLOptionElement>(event->currentTarget()->ToNode());
    DCHECK(target_element);
    DCHECK(select_list_element_->option_parts_.Contains(target_element));
    if (target_element != select_list_element_->selectedOption()) {
      select_list_element_->SetSelectedOption(target_element);
      select_list_element_->DispatchInputEvent();
    }
    select_list_element_->CloseListbox();
  } else if (event->IsKeyboardEvent()) {
    auto* keyboard_event = DynamicTo<KeyboardEvent>(event);
    if (keyboard_event && HandleKeyboardEvent(*keyboard_event)) {
      event->stopPropagation();
      event->SetDefaultHandled();
    }
  }
}

void HTMLSelectListElement::OptionPartEventListener::AddEventListeners(
    HTMLOptionElement* option_part) {
  option_part->addEventListener(event_type_names::kClick, this,
                                /*use_capture=*/false);
  option_part->addEventListener(event_type_names::kKeydown, this,
                                /*use_capture=*/false);
  option_part->addEventListener(event_type_names::kKeypress, this,
                                /*use_capture=*/false);
}

void HTMLSelectListElement::OptionPartEventListener::RemoveEventListeners(
    HTMLOptionElement* option_part) {
  option_part->removeEventListener(event_type_names::kClick, this,
                                   /*use_capture=*/false);
  option_part->removeEventListener(event_type_names::kKeydown, this,
                                   /*use_capture=*/false);
  option_part->removeEventListener(event_type_names::kKeypress, this,
                                   /*use_capture=*/false);
}

bool HTMLSelectListElement::OptionPartEventListener::HandleKeyboardEvent(
    const KeyboardEvent& event) {
  if (event.type() == event_type_names::kKeydown) {
    switch (event.keyCode()) {
      case VKEY_RETURN: {
        auto* target_element =
            DynamicTo<HTMLOptionElement>(event.currentTarget()->ToNode());
        DCHECK(target_element);
        DCHECK(select_list_element_->option_parts_.Contains(target_element));
        if (target_element != select_list_element_->selectedOption()) {
          select_list_element_->SetSelectedOption(target_element);
          select_list_element_->DispatchInputEvent();
        }
        select_list_element_->CloseListbox();
        return true;
      }
      case VKEY_SPACE: {
        select_list_element_->TypeAheadFind(event, ' ');
        // Prevent the default behavior of scrolling the page on spacebar
        // that would cause the listbox to close.
        return true;
      }
      case VKEY_UP: {
        select_list_element_->SelectPreviousOption();
        return true;
      }
      case VKEY_DOWN: {
        select_list_element_->SelectNextOption();
        return true;
      }
    }
  } else if (event.type() == event_type_names::kKeypress) {
    // Handled in event_type_names::kKeypress event handler because
    // KeyboardEvent::charCode() == 0 for event_type_names::kKeydown.
    return select_list_element_->TypeAheadFind(event, event.charCode());
  }

  return false;
}

const AtomicString& HTMLSelectListElement::FormControlType() const {
  DEFINE_STATIC_LOCAL(const AtomicString, selectlist, ("selectlist"));
  return selectlist;
}

void HTMLSelectListElement::DefaultEventHandler(Event& event) {
  if (!GetLayoutObject()) {
    return;
  }

  if (event.type() == event_type_names::kChange) {
    user_has_edited_the_field_ = true;
  }
}

bool HTMLSelectListElement::MayTriggerVirtualKeyboard() const {
  return true;
}

void HTMLSelectListElement::AppendToFormData(FormData& form_data) {
  if (!GetName().empty())
    form_data.AppendFromElement(GetName(), value());
}

FormControlState HTMLSelectListElement::SaveFormControlState() const {
  return FormControlState(value());
}

void HTMLSelectListElement::RestoreFormControlState(
    const FormControlState& state) {
  setValue(state[0]);
}

bool HTMLSelectListElement::IsRequiredFormControl() const {
  return IsRequired();
}

bool HTMLSelectListElement::IsOptionalFormControl() const {
  return !IsRequiredFormControl();
}

bool HTMLSelectListElement::IsLabelable() const {
  return true;
}

bool HTMLSelectListElement::ValueMissing() const {
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

void HTMLSelectListElement::CloneNonAttributePropertiesFrom(
    const Element& source,
    NodeCloningData& data) {
  const auto& source_element =
      static_cast<const HTMLSelectListElement&>(source);
  user_has_edited_the_field_ = source_element.user_has_edited_the_field_;
  HTMLFormControlElement::CloneNonAttributePropertiesFrom(source, data);
}

// https://html.spec.whatwg.org/C/#ask-for-a-reset
void HTMLSelectListElement::ResetImpl() {
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
void HTMLSelectListElement::ResetToDefaultSelection() {
  HTMLOptionElement* first_enabled_option = nullptr;
  HTMLOptionElement* last_selected_option = nullptr;

  for (Node* node = SelectListPartTraversal::FirstChild(*this); node;
       node = SelectListPartTraversal::Next(*node, this)) {
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

String HTMLSelectListElement::validationMessage() const {
  if (!willValidate())
    return String();
  if (CustomError())
    return CustomValidationMessage();
  if (ValueMissing()) {
    return GetLocale().QueryString(IDS_FORM_VALIDATION_VALUE_MISSING_SELECT);
  }
  return String();
}

void HTMLSelectListElement::Trace(Visitor* visitor) const {
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
  visitor->Trace(options_slot_);
  visitor->Trace(selected_option_);
  visitor->Trace(selected_option_when_listbox_opened_);
  visitor->Trace(suggested_option_);
  visitor->Trace(suggested_option_popover_);
  visitor->Trace(list_items_);
  HTMLFormControlElementWithState::Trace(visitor);
}

constexpr char HTMLSelectListElement::kButtonPartName[];
constexpr char HTMLSelectListElement::kSelectedValuePartName[];
constexpr char HTMLSelectListElement::kListboxPartName[];

}  // namespace blink
