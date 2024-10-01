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
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/simulated_click_options.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/dom/mutation_observer.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/events/gesture_event.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/html/forms/html_data_list_element.h"
#include "third_party/blink/renderer/core/html/forms/html_opt_group_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
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
    if (RuntimeEnabledFeatures::CustomizableSelectEnabled()) {
      init->setAttributes(true);
    }
    observer_->observe(option_, init, ASSERT_NO_EXCEPTION);
  }

  ExecutionContext* GetExecutionContext() const override {
    return option_->GetExecutionContext();
  }

  void Deliver(const MutationRecordVector& records,
               MutationObserver&) override {
    option_->DidChangeTextContent();
  }

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
  HTMLElement::Trace(visitor);
}

FocusableState HTMLOptionElement::SupportsFocus(
    UpdateBehavior update_behavior) const {
  HTMLSelectElement* select = OwnerSelectElement();
  if (select && select->UsesMenuList()) {
    if (select->IsAppearanceBasePicker()) {
      // If this option is being rendered as regular web content inside a
      // base-select <select> popover, then we need this element to be
      // focusable.
      return IsDisabledFormControl() ? FocusableState::kNotFocusable
                                     : FocusableState::kFocusable;
    }
    return FocusableState::kNotFocusable;
  }
  return HTMLElement::SupportsFocus(update_behavior);
}

bool HTMLOptionElement::MatchesDefaultPseudoClass() const {
  return FastHasAttribute(html_names::kSelectedAttr);
}

bool HTMLOptionElement::MatchesEnabledPseudoClass() const {
  return !IsDisabledFormControl();
}

String HTMLOptionElement::DisplayLabel() const {
  String label_attr = String(FastGetAttribute(html_names::kLabelAttr))
    .StripWhiteSpace(IsHTMLSpace<UChar>).SimplifyWhiteSpace(IsHTMLSpace<UChar>);
  String inner_text = CollectOptionInnerText()
    .StripWhiteSpace(IsHTMLSpace<UChar>).SimplifyWhiteSpace(IsHTMLSpace<UChar>);
  // FIXME: The following treats an element with the label attribute set to
  // the empty string the same as an element with no label attribute at all.
  // Is that correct? If it is, then should the label function work the same
  // way?
  return label_attr.empty() ? inner_text : label_attr;
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
  for (auto* const option : select_element->GetOptionList()) {
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

void HTMLOptionElement::SetSelectedState(bool selected) {
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
  if (change.type == ChildrenChangeType::kElementInserted && !text_observer_)
    text_observer_ = MakeGarbageCollected<OptionTextObserver>(*this);
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

HTMLSelectElement* HTMLOptionElement::OwnerSelectElement() const {
  if (RuntimeEnabledFeatures::SelectParserRelaxationEnabled()) {
    // TODO(crbug.com/1511354): Consider using a flat tree traversal here
    // instead of a node traversal. That would probably also require
    // changing HTMLOptionsCollection to support flat tree traversals as well.
    // TODO(crbug.com/351990825): Cache the owner select ancestor on insertion
    // rather than doing a tree traversal here every time OwnerSelectElement is
    // called, which may be a lot.
    for (Node& ancestor : NodeTraversal::AncestorsOf(*this)) {
      if (auto* select = DynamicTo<HTMLSelectElement>(ancestor)) {
        return select;
      }
    }
  } else {
    if (!parentNode()) {
      return nullptr;
    }
    if (auto* select = DynamicTo<HTMLSelectElement>(*parentNode())) {
      return select;
    }
    if (IsA<HTMLOptGroupElement>(*parentNode())) {
      return DynamicTo<HTMLSelectElement>(parentNode()->parentNode());
    }
  }
  return nullptr;
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
  ContainerNode* parent = parentNode();
  if (parent && IsA<HTMLOptGroupElement>(*parent))
    return "    " + DisplayLabel();
  return DisplayLabel();
}

bool HTMLOptionElement::OwnElementDisabled() const {
  return FastHasAttribute(html_names::kDisabledAttr);
}

bool HTMLOptionElement::IsDisabledFormControl() const {
  if (OwnElementDisabled())
    return true;
  if (Element* parent = parentElement())
    return IsA<HTMLOptGroupElement>(*parent) && parent->IsDisabledFormControl();
  return false;
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

HTMLFormElement* HTMLOptionElement::form() const {
  if (HTMLSelectElement* select_element = OwnerSelectElement())
    return select_element->formOwner();

  return nullptr;
}

void HTMLOptionElement::DidAddUserAgentShadowRoot(ShadowRoot& root) {
  UpdateLabel();
}

void HTMLOptionElement::UpdateLabel() {
  // For appearance:base-select <select> we also need to render all children. We
  // only check UsesMenuList and not computed style because we don't want to
  // change DOM content based on computed style and because appearance:auto/none
  // don't render the UA shadowroot when UsesMenuList is true.
  if (RuntimeEnabledFeatures::CustomizableSelectEnabled()) {
    if (auto* select = OwnerSelectElement()) {
      if (select->UsesMenuList()) {
        return;
      }
    }
  }

  if (ShadowRoot* root = UserAgentShadowRoot())
    root->setTextContent(DisplayLabel());
}

Node::InsertionNotificationRequest HTMLOptionElement::InsertedInto(
    ContainerNode& insertion_point) {
  auto return_value = HTMLElement::InsertedInto(insertion_point);
  if (!RuntimeEnabledFeatures::SelectParserRelaxationEnabled()) {
    CHECK(!RuntimeEnabledFeatures::CustomizableSelectEnabled());
    return return_value;
  }

  auto* parent_select = DynamicTo<HTMLSelectElement>(parentNode());
  if (!parent_select) {
    if (auto* optgroup = DynamicTo<HTMLOptGroupElement>(parentNode())) {
      parent_select = DynamicTo<HTMLSelectElement>(optgroup->parentNode());
    }
  }
  if (parent_select) {
    // Don't call OptionInserted because HTMLSelectElement::ChildrenChanged or
    // HTMLOptGroupElement::ChildrenChanged will call it for us in this case. If
    // insertion_point is an ancestor of parent_select, then we shouldn't really
    // be doing anything here and OptionInserted was already called in a
    // previous insertion.
    // TODO(crbug.com/1511354): When the CustomizableSelect flag is removed, we
    // can remove the code in HTMLSelectElement::ChildrenChanged and
    // HTMLOptGroupElement::ChildrenChanged which handles this case as well as
    // the code here which avoids handling it.
    // TODO(crbug.com/1511354): This UsesMenuList check doesn't account for
    // the case when the select's rendering is changed after insertion.
    SetTextOnlyRendering(!parent_select->UsesMenuList());
    return return_value;
  }

  // If there is a <select> in between this and insertion_point, then don't call
  // OptionInserted. Otherwise, if this option is being inserted into a <select>
  // ancestor, then we must call OptionInserted on it.
  bool passed_insertion_point = false;
  for (Node* ancestor = parentNode(); ancestor;
       ancestor = ancestor->parentNode()) {
    if (ancestor == insertion_point) {
      passed_insertion_point = true;
    }
    if (auto* select = DynamicTo<HTMLSelectElement>(ancestor)) {
      if (passed_insertion_point) {
        // TODO(crbug.com/1511354): This UsesMenuList check doesn't account for
        // the case when the select's rendering is changed after insertion.
        SetTextOnlyRendering(!select->UsesMenuList());
        select->OptionInserted(*this, Selected());
      }
      break;
    }
  }

  return return_value;
}

void HTMLOptionElement::RemovedFrom(ContainerNode& insertion_point) {
  HTMLElement::RemovedFrom(insertion_point);
  if (!RuntimeEnabledFeatures::SelectParserRelaxationEnabled()) {
    CHECK(!RuntimeEnabledFeatures::CustomizableSelectEnabled());
    return;
  }

  // This code determines the value of was_removed_from_select_parent, which
  // should be true in the case that this <option> was a child of a <select> and
  // got removed, or there was an <optgroup> directly in between this <option>
  // and a <select> and either the optgroup-option or select-optgroup child
  // relationship was disconnected.
  bool insertion_point_passed = false;
  bool is_parent_select_or_optgroup = false;
  ContainerNode* parent = parentNode();
  if (!parent) {
    parent = &insertion_point;
    insertion_point_passed = true;
  }
  if (IsA<HTMLSelectElement>(parent)) {
    is_parent_select_or_optgroup = true;
  } else if (IsA<HTMLOptGroupElement>(parent)) {
    parent = parent->parentNode();
    if (!parent) {
      parent = &insertion_point;
      insertion_point_passed = true;
    }
    is_parent_select_or_optgroup = IsA<HTMLSelectElement>(parent);
  }
  bool was_removed_from_select_parent =
      insertion_point_passed && is_parent_select_or_optgroup;

  if (was_removed_from_select_parent) {
    // Don't call select->OptionRemoved() in this case because
    // HTMLSelectElement::ChildrenChanged or
    // HTMLOptGroupElement::ChildrenChanged will call it for us.
    SetTextOnlyRendering(true);
    return;
  }

  for (Node* ancestor = parentNode(); ancestor;
       ancestor = ancestor->parentNode()) {
    // If this option is still associated with a <select> inside the detached
    // subtree, then we should not call OptionRemoved() because we don't call
    // OptionInserted() in the corresponding attachment case. Also, APIs like
    // select.options should still work when the <select> is detached.
    if (IsA<HTMLSelectElement>(ancestor)) {
      return;
    }
  }

  for (Node* ancestor = &insertion_point; ancestor;
       ancestor = ancestor->parentNode()) {
    if (auto* select = DynamicTo<HTMLSelectElement>(ancestor)) {
      SetTextOnlyRendering(true);
      select->OptionRemoved(*this);
      break;
    }
  }
}

void HTMLOptionElement::SetTextOnlyRendering(bool text_only) {
  if (!RuntimeEnabledFeatures::CustomizableSelectEnabled()) {
    return;
  }

#if DCHECK_IS_ON()
  {
    // Double-check to make sure that we are setting the correct state according
    // to the DOM tree. If there is a nearest ancestor <select> and it
    // UsesMenuList, then we should be rendering all content rather than
    // text-only.
    auto* select = OwnerSelectElement();
    DCHECK_EQ(select && select->UsesMenuList(), !text_only);
  }
#endif

  if (auto* first_child = GetShadowRoot()->firstChild()) {
    bool currently_text_only = first_child->getNodeType() == kTextNode;
    CHECK_NE(currently_text_only, IsA<HTMLSlotElement>(first_child))
        << " <option>'s UA ShadowRoot should either be text or a <slot>.";
    if (currently_text_only == text_only) {
      return;
    }
  }

  GetShadowRoot()->RemoveChildren();
  if (!text_only) {
    // Render all child content by just having an unnamed <slot>.
    GetShadowRoot()->AppendChild(
        MakeGarbageCollected<HTMLSlotElement>(GetDocument()));
  } else {
    // Render only text content by only having a text node inside the
    // shadowroot.
    UpdateLabel();
  }
}

bool HTMLOptionElement::SpatialNavigationFocused() const {
  HTMLSelectElement* select = OwnerSelectElement();
  if (!select || !select->IsFocused())
    return false;
  return select->SpatialNavigationFocusedOption() == this;
}

bool HTMLOptionElement::IsDisplayNone() const {
  const ComputedStyle* style = GetComputedStyle();
  return !style || style->Display() == EDisplay::kNone;
}

void HTMLOptionElement::DefaultEventHandler(Event& event) {
  auto* select = OwnerSelectElement();
  if (select && !select->IsAppearanceBasePicker()) {
    // We only want to apply mouse/keyboard behavior for appearance:base-select
    // select pickers.
    select = nullptr;
  }

  if (select) {
    // This logic to determine if we should select the option is copied from
    // ListBoxSelectType::DefaultEventHandler. It will likely change when we try
    // to spec it.
    const auto* mouse_event = DynamicTo<MouseEvent>(event);
    const auto* gesture_event = DynamicTo<GestureEvent>(event);
    if ((event.type() == event_type_names::kGesturetap && gesture_event) ||
        (event.type() == event_type_names::kMousedown && mouse_event &&
         mouse_event->button() ==
             static_cast<int16_t>(WebPointerProperties::Button::kLeft))) {
      SetSelected(true);
      select->HidePopup();
      event.SetDefaultHandled();
      return;
    }
  }

  auto* keyboard_event = DynamicTo<KeyboardEvent>(event);
  int tab_ignore_modifiers = WebInputEvent::kControlKey |
                             WebInputEvent::kAltKey | WebInputEvent::kMetaKey;
  int ignore_modifiers = WebInputEvent::kShiftKey | tab_ignore_modifiers;

  if (keyboard_event && event.type() == event_type_names::kKeydown) {
    const AtomicString key(keyboard_event->key());

    if (!(keyboard_event->GetModifiers() & ignore_modifiers)) {
      if (key == keywords::kArrowUp && select) {
        HTMLOptionElement* previous_option = nullptr;
        OptionListIterator option_list = select->GetOptionList().begin();
        while (*option_list && *option_list != this) {
          previous_option = *option_list;
          ++option_list;
        }
        if (previous_option) {
          previous_option->Focus(FocusParams(FocusTrigger::kUserGesture));
          event.SetDefaultHandled();
          return;
        }
      } else if (key == keywords::kArrowDown && select) {
        OptionListIterator option_list = select->GetOptionList().begin();
        while (*option_list && *option_list != this) {
          ++option_list;
        }
        if (*option_list) {
          CHECK_EQ(*option_list, this);
          ++option_list;
          auto* next_option = *option_list;
          if (next_option) {
            next_option->Focus(FocusParams(FocusTrigger::kUserGesture));
            event.SetDefaultHandled();
            return;
          }
        }
      } else if ((key == " " || key == keywords::kCapitalEnter) && select) {
        SetSelected(true);
        select->HidePopup();
        event.SetDefaultHandled();
        return;
      }
    }

    if (key == keywords::kTab &&
        !(keyboard_event->GetModifiers() & tab_ignore_modifiers)) {
      if (select) {
        // TODO(http://crbug.com/1511354): Consider focusing something in this
        // case. https://github.com/openui/open-ui/issues/1016
        select->HidePopup();
        event.SetDefaultHandled();
        return;
      }
    }
  }

  HTMLElement::DefaultEventHandler(event);
}

}  // namespace blink
