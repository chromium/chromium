/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 * Copyright (C) 2004, 2005, 2006, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2011 Motorola Mobility, Inc.  All rights reserved.
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

#include "third_party/blink/renderer/core/html/forms/html_option_element.h"

#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-shared.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_mutation_observer_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_scroll_into_view_options.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/simulated_click_options.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/dom/mutation_observer.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/events/gesture_event.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/html/forms/html_data_list_element.h"
#include "third_party/blink/renderer/core/html/forms/html_opt_group_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/html_hr_element.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/html/html_span_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

class OptionTextObserver : public MutationObserver::Delegate {
 public:
  explicit OptionTextObserver(HTMLOptionElement& option)
      : option_(option), observer_(MutationObserver::Create(this)) {
    MutationObserverInit* init = MutationObserverInit::Create();
    init->setCharacterData(true);
    init->setChildList(true);
    init->setSubtree(true);
    observer_->observe(option_, init, ASSERT_NO_EXCEPTION);
  }

  ExecutionContext* GetExecutionContext() const override {
    return option_->GetExecutionContext();
  }

  void Deliver(const MutationRecordVector& records,
               MutationObserver&) override {
    option_->DidChangeTextContent();
  }

  void Disconnect() { observer_->disconnect(); }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(option_);
    visitor->Trace(observer_);
    MutationObserver::Delegate::Trace(visitor);
  }

 private:
  Member<HTMLOptionElement> option_;
  Member<MutationObserver> observer_;
};

HTMLOptionElement::HTMLOptionElement(Document& document)
    : HTMLElement(html_names::kOptionTag, document), is_selected_(false) {
  EnsureUserAgentShadowRoot();
}

// An explicit empty destructor should be in html_option_element.cc, because
// if an implicit destructor is used or an empty destructor is defined in
// html_option_element.h, when including html_option_element.h,
// msvc tries to expand the destructor and causes
// a compile error because of lack of ComputedStyle definition.
HTMLOptionElement::~HTMLOptionElement() = default;

HTMLOptionElement* HTMLOptionElement::CreateForJSConstructor(
    Document& document,
    const String& data,
    const AtomicString& value,
    bool default_selected,
    bool selected,
    ExceptionState& exception_state) {
  HTMLOptionElement* element =
      MakeGarbageCollected<HTMLOptionElement>(document);
  element->EnsureUserAgentShadowRoot();
  if (!data.empty()) {
    element->AppendChild(Text::Create(document, data), exception_state);
    if (exception_state.HadException())
      return nullptr;
  }

  if (!value.IsNull())
    element->setValue(value);
  if (default_selected)
    element->setAttribute(html_names::kSelectedAttr, g_empty_atom);
  element->SetSelected(selected);

  return element;
}

void HTMLOptionElement::Trace(Visitor* visitor) const {
  visitor->Trace(text_observer_);
  visitor->Trace(nearest_ancestor_select_);
  visitor->Trace(nearest_ancestor_optgroup_);
  visitor->Trace(label_container_);
  HTMLElement::Trace(visitor);
}

FocusableState HTMLOptionElement::SupportsFocus(
    UpdateBehavior update_behavior) const {
  // Run SupportsFocus from the parent class first so it can do a style update
  // if appropriate, which we will make use of here.
  FocusableState superclass_focusable =
      HTMLElement::SupportsFocus(update_behavior);
  if (auto* select = OwnerSelectElement()) {
    auto* popover = select->PopoverPickerElement();
    bool base_with_picker =
        select->UsesMenuList() && popover && popover->popoverOpen();
    bool base_in_page =
        RuntimeEnabledFeatures::CustomizableSelectListboxEnabled() &&
        !select->UsesMenuList() && select->IsAppearanceBase();
    if (base_with_picker || base_in_page) {
      // If this option is being rendered as regular web content inside a
      // base-select <select>, then we need this element to be focusable.
      return IsDisabledFormControl() || select->IsDisabledFormControl()
                 ? FocusableState::kNotFocusable
                 : FocusableState::kFocusable;
    } else if (select->UsesMenuList()) {
      // appearance:auto ListBox <select>s have focusable <option>s, and
      // MenuList ones don't have focusable <option>s.
      return FocusableState::kNotFocusable;
    }
  }
  return superclass_focusable;
}

bool HTMLOptionElement::IsKeyboardFocusableSlow(
    UpdateBehavior update_behavior) const {
  if (!HTMLElement::IsKeyboardFocusableSlow(update_behavior)) {
    return false;
  }
  if (!RuntimeEnabledFeatures::CustomizableSelectListboxEnabled() ||
      !OwnerSelectElement() || OwnerSelectElement()->UsesMenuList()) {
    return true;
  }

  // In an in-page customizable <select>, pressing tab should go to the next
  // focusable element in the page after the end of the <select> instead of the
  // next focusable <option>. In order to implement this, we make the option
  // elements which aren't currently focused not keyboard focusable so they are
  // skipped by FocusController. This same trick is used in
  // RadioInputType::IsKeyboardFocusableSlow.
  if (auto* focused_option =
          DynamicTo<HTMLOptionElement>(GetDocument().FocusedElement())) {
    if (focused_option == this) {
      // Keep the currently focused option focusable in order to prevent issues
      // with invalidation and other things.
      return true;
    }
    if (focused_option->OwnerSelectElement() == OwnerSelectElement()) {
      return false;
    }
  }

  // TODO(crbug.com/357649033): consider implementing "memory" to only make only
  // the last focused option in a select focusable, so that tabbing out and back
  // into an in-page select results in the same <option> being focused.
  return true;
}

bool HTMLOptionElement::MatchesDefaultPseudoClass() const {
  return FastHasAttribute(html_names::kSelectedAttr);
}

bool HTMLOptionElement::MatchesEnabledPseudoClass() const {
  return !IsDisabledFormControl();
}

// The logic in this method to choose rendering the label attribute or the text
// content should be kept in sync with the ::-internal-option-label-container
// rules in the UA stylesheet.
String HTMLOptionElement::DisplayLabel() const {
  // If the label attribute is set and is not an empty string, then use its
  // value. Otherwise, use inner text.
  String label_attr = String(FastGetAttribute(html_names::kLabelAttr));
  if (!label_attr.empty()) {
    return label_attr;
  }
  return CollectOptionInnerText()
      .StripWhiteSpace(IsHTMLSpace<UChar>)
      .SimplifyWhiteSpace(IsHTMLSpace<UChar>);
}

String HTMLOptionElement::text() const {
  return CollectOptionInnerText()
      .StripWhiteSpace(IsHTMLSpace<UChar>)
      .SimplifyWhiteSpace(IsHTMLSpace<UChar>);
}

void HTMLOptionElement::setText(const String& text) {
  // Changing the text causes a recalc of a select's items, which will reset the
  // selected index to the first item if the select is single selection with a
  // menu list.  We attempt to preserve the selected item.
  HTMLSelectElement* select = OwnerSelectElement();
  bool select_is_menu_list = select && select->UsesMenuList();
  int old_selected_index = select_is_menu_list ? select->selectedIndex() : -1;

  setTextContent(text);

  if (select_is_menu_list && select->selectedIndex() != old_selected_index)
    select->setSelectedIndex(old_selected_index);
}

void HTMLOptionElement::AccessKeyAction(SimulatedClickCreationScope) {
  // TODO(crbug.com/1176745): why creation_scope arg is not used at all?
  if (HTMLSelectElement* select = OwnerSelectElement())
    select->SelectOptionByAccessKey(this);
}

int HTMLOptionElement::index() const {
  // It would be faster to cache the index, but harder to get it right in all
  // cases.

  HTMLSelectElement* select_element = OwnerSelectElement();
  if (!select_element)
    return 0;

  int option_index = 0;
  for (const auto& option : select_element->GetOptionList()) {
    if (option == this)
      return option_index;
    ++option_index;
  }

  return 0;
}

int HTMLOptionElement::ListIndex() const {
  if (HTMLSelectElement* select_element = OwnerSelectElement())
    return select_element->ListIndexForOption(*this);
  return -1;
}

void HTMLOptionElement::ParseAttribute(
    const AttributeModificationParams& params) {
  const QualifiedName& name = params.name;
  if (name == html_names::kValueAttr) {
    if (HTMLDataListElement* data_list = OwnerDataListElement()) {
      data_list->OptionElementChildrenChanged();
    }
    if (HTMLSelectElement* select = OwnerSelectElement()) {
      select->SetNeedsValidityCheck();
    }
  } else if (name == html_names::kDisabledAttr) {
    if (params.old_value.IsNull() != params.new_value.IsNull()) {
      PseudoStateChanged(CSSSelector::kPseudoDisabled);
      PseudoStateChanged(CSSSelector::kPseudoEnabled);
      InvalidateIfHasEffectiveAppearance();
    }
  } else if (name == html_names::kSelectedAttr) {
    if (params.old_value.IsNull() != params.new_value.IsNull() && !is_dirty_)
      SetSelected(!params.new_value.IsNull());
    PseudoStateChanged(CSSSelector::kPseudoDefault);
  } else if (name == html_names::kLabelAttr) {
    if (HTMLSelectElement* select = OwnerSelectElement())
      select->OptionElementChildrenChanged(*this);
    UpdateLabel();
  } else {
    HTMLElement::ParseAttribute(params);
  }
}

String HTMLOptionElement::value() const {
  const AtomicString& value = FastGetAttribute(html_names::kValueAttr);
  if (!value.IsNull())
    return value;
  return CollectOptionInnerText()
      .StripWhiteSpace(IsHTMLSpace<UChar>)
      .SimplifyWhiteSpace(IsHTMLSpace<UChar>);
}

void HTMLOptionElement::setValue(const AtomicString& value) {
  setAttribute(html_names::kValueAttr, value);
}

bool HTMLOptionElement::Selected() const {
  return is_selected_;
}

void HTMLOptionElement::SetSelected(bool selected) {
  if (is_selected_ == selected)
    return;

  SetSelectedState(selected);

  if (HTMLSelectElement* select = OwnerSelectElement()) {
    select->OptionSelectionStateChanged(this, selected);
  }
}

bool HTMLOptionElement::selectedForBinding() const {
  return Selected();
}

void HTMLOptionElement::setSelectedForBinding(bool selected) {
  bool was_selected = is_selected_;
  SetSelected(selected);

  // As of December 2015, the HTML specification says the dirtiness becomes
  // true by |selected| setter unconditionally. However it caused a real bug,
  // crbug.com/570367, and is not compatible with other browsers.
  // Firefox seems not to set dirtiness if an option is owned by a select
  // element and selectedness is not changed.
  if (OwnerSelectElement() && was_selected == is_selected_)
    return;

  is_dirty_ = true;
}

void HTMLOptionElement::SetSelectedState(bool selected,
                                         bool skip_mutation_observer_update) {
  if (is_selected_ == selected)
    return;

  is_selected_ = selected;
  PseudoStateChanged(CSSSelector::kPseudoChecked);

  if (HTMLSelectElement* select = OwnerSelectElement()) {
    select->InvalidateSelectedItems();

    if (AXObjectCache* cache = GetDocument().ExistingAXObjectCache()) {
      // If there is a layoutObject (most common), fire accessibility
      // notifications only when it's a listbox (and not a menu list). If
      // there's no layoutObject, fire them anyway just to be safe (to make sure
      // the AX tree is in sync).
      if (!select->GetLayoutObject() || !select->UsesMenuList()) {
        cache->ListboxOptionStateChanged(this);
        cache->ListboxSelectedChildrenChanged(select);
      }
    }
  }

  if (RuntimeEnabledFeatures::OptionMutationObserverImprovementEnabled() &&
      !skip_mutation_observer_update) {
    UpdateMutationObserver(/*in_style_recalc=*/false);
  }
}

void HTMLOptionElement::SetMultiSelectFocusedState(bool focused) {
  if (is_multi_select_focused_ == focused)
    return;

  if (auto* select = OwnerSelectElement()) {
    DCHECK(select->IsMultiple());
    is_multi_select_focused_ = focused;
    PseudoStateChanged(CSSSelector::kPseudoMultiSelectFocus);
  }
}

bool HTMLOptionElement::IsMultiSelectFocused() const {
  return is_multi_select_focused_;
}

void HTMLOptionElement::SetDirty(bool value) {
  is_dirty_ = value;
}

void HTMLOptionElement::ChildrenChanged(const ChildrenChange& change) {
  HTMLElement::ChildrenChanged(change);
  DidChangeTextContent();

  // If an element is inserted, We need to use MutationObserver to detect
  // textContent changes.
  if (change.type == ChildrenChangeType::kElementInserted &&
      !was_element_inserted_) {
    was_element_inserted_ = true;
    UpdateMutationObserver(/*in_style_recalc=*/false);
  }
}

void HTMLOptionElement::UpdateMutationObserver(bool in_style_recalc) {
  if (NeedsMutationObserver()) {
    if (!text_observer_) {
      if (in_style_recalc) {
        update_label_task_ = PostCancellableTask(
            *GetDocument().GetTaskRunner(TaskType::kDOMManipulation), FROM_HERE,
            BindOnce(&HTMLOptionElement::DidChangeTextContent,
                     WrapWeakPersistent(this)));
      } else {
        DidChangeTextContent();
      }
      text_observer_ = MakeGarbageCollected<OptionTextObserver>(*this);
    }
  } else if (text_observer_) {
    text_observer_->Disconnect();
    text_observer_ = nullptr;
  }
}

bool HTMLOptionElement::NeedsMutationObserver() {
  if (!was_element_inserted_) {
    return false;
  }

  // This flag check runs after was_element_inserted_ in order to match the
  // behavior before the flag was added, which was that a MutationObserver is
  // always registered when an element is inserted.
  if (!RuntimeEnabledFeatures::OptionMutationObserverImprovementEnabled()) {
    return true;
  }

  HTMLSelectElement* select = OwnerSelectElement();
  if (!select) {
    return false;
  }

  if (select->UsesMenuList()) {
    if (select->IsAppearanceBase() && select->SlottedButton()) {
      // The author provided button is being rendered instead of the
      // MenuListInnerElement, so we don't need to keep its text up to date.
      return false;
    }
    // If this option is selected, then it is being rendered in the
    // MenuListInnerElement.
    return Selected();
  } else {
    if (!select->GetComputedStyle()) {
      // If style recalc hasn't been done yet, then don't eagerly create a
      // MutationObserver. Otherwise, in the base appearance case, we would
      // create a MutationObserver and then quickly remove it as soon as style
      // recalc is done.
      return false;
    }
    return !select->IsAppearanceBase();
  }
}

void HTMLOptionElement::DidChangeTextContent() {
  if (HTMLDataListElement* data_list = OwnerDataListElement()) {
    data_list->OptionElementChildrenChanged();
  }
  if (HTMLSelectElement* select = OwnerSelectElement()) {
    select->OptionElementChildrenChanged(*this);
  }
  UpdateLabel();
}

HTMLDataListElement* HTMLOptionElement::OwnerDataListElement() const {
  return Traversal<HTMLDataListElement>::FirstAncestor(*this);
}

String HTMLOptionElement::label() const {
  const AtomicString& label = FastGetAttribute(html_names::kLabelAttr);
  if (!label.IsNull())
    return label;
  return CollectOptionInnerText()
      .StripWhiteSpace(IsHTMLSpace<UChar>)
      .SimplifyWhiteSpace(IsHTMLSpace<UChar>);
}

void HTMLOptionElement::setLabel(const AtomicString& label) {
  setAttribute(html_names::kLabelAttr, label);
}

String HTMLOptionElement::TextIndentedToRespectGroupLabel() const {
  if (nearest_ancestor_optgroup_) {
    return StrCat({"    ", DisplayLabel()});
  }
  return DisplayLabel();
}

bool HTMLOptionElement::OwnElementDisabled() const {
  return FastHasAttribute(html_names::kDisabledAttr);
}

bool HTMLOptionElement::IsDisabledFormControl() const {
  if (OwnElementDisabled())
    return true;
  return nearest_ancestor_optgroup_ &&
         nearest_ancestor_optgroup_->IsDisabledFormControl();
}

String HTMLOptionElement::DefaultToolTip() const {
  if (HTMLSelectElement* select = OwnerSelectElement())
    return select->DefaultToolTip();
  return String();
}

String HTMLOptionElement::CollectOptionInnerText() const {
  StringBuilder text;
  for (Node* node = firstChild(); node;) {
    if (node->IsTextNode())
      text.Append(node->nodeValue());
    // Text nodes inside script elements are not part of the option text.
    auto* element = DynamicTo<Element>(node);
    if (element && element->IsScriptElement())
      node = NodeTraversal::NextSkippingChildren(*node, this);
    else
      node = NodeTraversal::Next(*node, this);
  }
  return text.ToString();
}

HTMLElement* HTMLOptionElement::formForBinding() const {
  if (HTMLSelectElement* select_element = OwnerSelectElement())
    return select_element->formForBinding();

  return nullptr;
}

void HTMLOptionElement::DidAddUserAgentShadowRoot(ShadowRoot& root) {
  label_container_ = MakeGarbageCollected<HTMLSpanElement>(GetDocument());
  label_container_->SetShadowPseudoId(
      shadow_element_names::kOptionLabelContainer);
  label_container_->setAttribute(html_names::kAriaHiddenAttr, keywords::kTrue);
  root.appendChild(label_container_);

  auto* slot = MakeGarbageCollected<HTMLSlotElement>(GetDocument());
  slot->SetShadowPseudoId(shadow_element_names::kOptionSlot);
  root.appendChild(slot);

  UpdateLabel();
}

void HTMLOptionElement::UpdateLabel() {
  if (label_container_) {
    label_container_->setTextContent(DisplayLabel());
  }
}

Node::InsertionNotificationRequest HTMLOptionElement::InsertedInto(
    ContainerNode& insertion_point) {
  auto return_value = HTMLElement::InsertedInto(insertion_point);

  HTMLSelectElement* old_ancestor_select = nearest_ancestor_select_;
  std::tie(nearest_ancestor_select_, nearest_ancestor_optgroup_) =
      HTMLSelectElement::AssociatedSelectAndOptgroup(*this);

  if (nearest_ancestor_select_ &&
      nearest_ancestor_select_ != old_ancestor_select) {
    CHECK(!old_ancestor_select);
    nearest_ancestor_select_->OptionInserted(*this, Selected());
  }

  return return_value;
}

void HTMLOptionElement::RemovedFrom(ContainerNode& insertion_point) {
  HTMLElement::RemovedFrom(insertion_point);

  HTMLSelectElement* old_ancestor_select = nearest_ancestor_select_;
  std::tie(nearest_ancestor_select_, nearest_ancestor_optgroup_) =
      HTMLSelectElement::AssociatedSelectAndOptgroup(*this);
  if (nearest_ancestor_select_ != old_ancestor_select) {
    // We should only get here if we are being removed from a <select>
    CHECK(!nearest_ancestor_select_);
    CHECK(old_ancestor_select);
    const bool should_skip_option_removed =
        !parentNode() && insertion_point == old_ancestor_select;
    if (!RuntimeEnabledFeatures::SelectChildrenRemovedFixEnabled() ||
        !should_skip_option_removed) {
      // If this option was removed from a select element as a direct child,
      // then let HTMLSelectElement::ChildrenChanged make the call to
      // OptionRemoved in order to avoid
      // https://issues.chromium.org/issues/444330901
      old_ancestor_select->OptionRemoved(*this);
    }
  }
}

bool HTMLOptionElement::SpatialNavigationFocused() const {
  HTMLSelectElement* select = OwnerSelectElement();
  if (!select || !select->IsFocused())
    return false;
  return select->SpatialNavigationFocusedOption() == this;
}

bool HTMLOptionElement::IsDisplayNone(bool ensure_style) {
  const ComputedStyle* style = GetComputedStyle();
  if (!style && ensure_style) {
    style = EnsureComputedStyle();
  }
  return !style || style->Display() == EDisplay::kNone;
}

void HTMLOptionElement::DefaultEventHandler(Event& event) {
  DefaultEventHandlerInternal(event);
  HTMLElement::DefaultEventHandler(event);
}

bool HTMLOptionElement::IsVisibleInViewport() {
  HTMLSelectElement* select = OwnerSelectElement();
  if (!select) {
    return false;
  }

  PhysicalRect listbox_rect =
      select->UsesMenuList() ? select->PopoverPickerElement()->BoundingBox()
                             : select->BoundingBox();
  PhysicalRect option_rect = BoundingBox();
  LayoutUnit listbox_top = listbox_rect.Y();
  LayoutUnit option_top = option_rect.Y();
  return option_top >= listbox_top && option_top + option_rect.Height() <=
                                          listbox_top + listbox_rect.Height();
}
void HTMLOptionElement::DefaultEventHandlerInternal(Event& event) {
  auto* select = OwnerSelectElement();
  if (!select) {
    return;
  }

  const bool appearance_base_in_page =
      RuntimeEnabledFeatures::CustomizableSelectListboxEnabled() &&
      !select->UsesMenuList() && select->IsAppearanceBase();

  if (!appearance_base_in_page && !select->PickerIsPopover()) {
    // Select elements use this code for event handling on their options in
    // these cases:
    // - <select> with appearance:base-select on itself and its ::picker(select)
    // - <select size={not 1}> with appearance:base-select
    // - <select size=1 multiple> on platforms which don't delegate MenuList
    //   rendering (only Android currently delegates MenuList rendering)
    return;
  }

  if (appearance_base_in_page) {
    // TODO(crbug.com/411598949): Consider using mouseup/mousedown instead of
    // click here to support click and drag to select multiple options.
    if (event.type() == event_type_names::kClick) {
      ChooseOption(event);
    }
  } else {
    const auto* mouse_event = DynamicTo<MouseEvent>(event);
    if (mouse_event && event.type() == event_type_names::kMouseup &&
        mouse_event->button() ==
            static_cast<int16_t>(WebPointerProperties::Button::kLeft)) {
      // We leave the picker open, and do not "pick" an option, only if:
      //  1. The mousedown was on the <select> button, so we have a mousedown
      //     location stored, and
      //  2. The mouseup on this <option> was within kEpsilon layout units
      //     (post zoom, page-relative) of the location of the mousedown. I.e.
      //     the mouse was not dragged between mousedown and mouseup.
      auto mouse_down_info = GetDocument().PopoverPickerPointerdown();
      constexpr float kEpsilon = 5;  // 5 pixels in any direction
      bool mouse_moved = !mouse_down_info.target ||
                         !mouse_down_info.location.IsWithinDistance(
                             mouse_event->AbsoluteLocation(), kEpsilon);
      if (mouse_moved) {
        ChooseOption(event);
      }
      GetDocument().SetPopoverPickerPointerdown({.target = nullptr});
      return;
    } else if (event.type() == event_type_names::kMousedown) {
      GetDocument().SetPopoverPickerPointerdown({.target = nullptr});
    }
  }

  auto* keyboard_event = DynamicTo<KeyboardEvent>(event);
  int tab_ignore_modifiers = WebInputEvent::kControlKey |
                             WebInputEvent::kAltKey | WebInputEvent::kMetaKey;
  int ignore_modifiers = WebInputEvent::kShiftKey | tab_ignore_modifiers;
  FocusParams focus_params(FocusTrigger::kUserGesture);

  if (keyboard_event && event.type() == event_type_names::kKeydown) {
    const AtomicString key(keyboard_event->key());
    if (!(keyboard_event->GetModifiers() & ignore_modifiers)) {
      if ((key == " " || key == keywords::kCapitalEnter)) {
        ChooseOption(event);
        return;
      }
      OptionList options = select->GetOptionList();
      if (options.Empty()) {
        // Nothing below can do anything, if the options list is empty.
        return;
      }
      if (key == keywords::kArrowUp) {
        if (auto* previous_option = options.PreviousFocusableOption(*this)) {
          previous_option->Focus(focus_params);
        }
        event.SetDefaultHandled();
        return;
      } else if (key == keywords::kArrowDown) {
        if (auto* next_option = options.NextFocusableOption(*this)) {
          next_option->Focus(focus_params);
        }
        event.SetDefaultHandled();
        return;
      } else if (key == keywords::kHome) {
        if (auto* first_option = options.NextFocusableOption(
                *options.begin(), /*inclusive*/ true)) {
          first_option->Focus(focus_params);
          event.SetDefaultHandled();
          return;
        }
      } else if (key == keywords::kEnd) {
        if (auto* last_option = options.PreviousFocusableOption(
                *options.last(), /*inclusive*/ true)) {
          last_option->Focus(focus_params);
          event.SetDefaultHandled();
          return;
        }
      } else if (key == keywords::kPageDown) {
        if (!IsVisibleInViewport()) {
          // If the option isn't visible at all right now, *only* scroll it into
          // view.
          scrollIntoViewIfNeeded(/*center_if_needed*/ false);
        } else {
          auto* next_option = options.NextFocusableOption(*this);
          if (next_option && !next_option->IsVisibleInViewport()) {
            // The next option isn't visible, which means we were at the very
            // bottom. Scroll the current option to the top, and then focus the
            // bottom one.
            ScrollIntoViewOptions* scroll_into_view_options =
                ScrollIntoViewOptions::Create();
            scroll_into_view_options->setBlock(
                V8ScrollLogicalPosition::Enum::kStart);
            scroll_into_view_options->setInlinePosition(
                V8ScrollLogicalPosition::Enum::kNearest);
            scrollIntoViewWithOptions(scroll_into_view_options);
          }
          // Then find the last option that is still in the view.
          HTMLOptionElement* next_focus = this;
          for (auto* current = this; current && current->IsVisibleInViewport();
               current = options.NextFocusableOption(*current)) {
            next_focus = current;
          }
          next_focus->Focus(focus_params);
        }
        event.SetDefaultHandled();
      } else if (key == keywords::kPageUp) {
        if (!IsVisibleInViewport()) {
          // If the option isn't visible at all right now, *only* scroll it into
          // view.
          scrollIntoViewIfNeeded(/*center_if_needed*/ false);
        } else {
          auto* previous_option = options.PreviousFocusableOption(*this);
          if (previous_option && !previous_option->IsVisibleInViewport()) {
            // The previous option isn't visible, which means we were at the
            // very top. Scroll the current option to the bottom, and then focus
            // the top one.
            ScrollIntoViewOptions* scroll_into_view_options =
                ScrollIntoViewOptions::Create();
            scroll_into_view_options->setBlock(
                V8ScrollLogicalPosition::Enum::kEnd);
            scroll_into_view_options->setInlinePosition(
                V8ScrollLogicalPosition::Enum::kNearest);
            scrollIntoViewWithOptions(scroll_into_view_options);
          }
          // Then find the first option that is in the view.
          HTMLOptionElement* next_focus = this;
          for (auto* current = this; current && current->IsVisibleInViewport();
               current = options.PreviousFocusableOption(*current)) {
            next_focus = current;
          }
          next_focus->Focus(focus_params);
        }
        event.SetDefaultHandled();
      }
    }

    if (key == keywords::kTab &&
        !(keyboard_event->GetModifiers() & tab_ignore_modifiers) &&
        !select->IsInDialogMode()) {
      if (!appearance_base_in_page) {
        // TODO(http://crbug.com/1511354): Consider focusing something in this
        // case. https://github.com/openui/open-ui/issues/1016
        select->HidePopup(SelectPopupHideBehavior::kNormal);
        event.SetDefaultHandled();
      }
      return;
    }
  }
}

void HTMLOptionElement::ChooseOption(Event& event) {
  HTMLSelectElement* select = OwnerSelectElement();
  CHECK(select);
  if (IsDisabledFormControl() || select->IsDisabledFormControl()) {
    return;
  }
  CHECK(select->IsAppearanceBase() || select->PickerIsPopover());
  select->SelectOptionFromPopoverPickerOrBaseListbox(this);
  event.SetDefaultHandled();
}

void HTMLOptionElement::FinishParsingChildren() {
  HTMLElement::FinishParsingChildren();
  if (Selected()) {
    auto* select = OwnerSelectElement();
    if (select && !select->IsMultiple()) {
      select->UpdateAllSelectedcontents(this);
    }
  }
}

// static
bool HTMLOptionElement::IsLabelContainerElement(const Element& element) {
  return IsA<HTMLOptionElement>(element.OwnerShadowHost()) &&
         element.ShadowPseudoId() ==
             shadow_element_names::kOptionLabelContainer;
}

}  // namespace blink
