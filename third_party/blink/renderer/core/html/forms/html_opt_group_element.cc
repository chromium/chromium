/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2010 Apple Inc. All rights
 * reserved.
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

#include "third_party/blink/renderer/core/html/forms/html_opt_group_element.h"

#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/dom/events/simulated_click_options.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/html/forms/html_legend_element.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"

namespace blink {

namespace {

bool CanAssignToOptGroupSlot(const Node& node) {
  return node.HasTagName(html_names::kOptionTag) ||
         node.HasTagName(html_names::kHrTag);
}

}  // namespace

HTMLOptGroupElement::HTMLOptGroupElement(Document& document)
    : HTMLElement(html_names::kOptgroupTag, document) {
  EnsureUserAgentShadowRoot(SlotAssignmentMode::kManual);
}

// An explicit empty destructor should be in html_opt_group_element.cc, because
// if an implicit destructor is used or an empty destructor is defined in
// html_opt_group_element.h, when including html_opt_group_element.h,
// msvc tries to expand the destructor and causes
// a compile error because of lack of ComputedStyle definition.
HTMLOptGroupElement::~HTMLOptGroupElement() = default;

bool HTMLOptGroupElement::IsDisabledFormControl() const {
  return FastHasAttribute(html_names::kDisabledAttr);
}

void HTMLOptGroupElement::ParseAttribute(
    const AttributeModificationParams& params) {
  HTMLElement::ParseAttribute(params);

  if (params.name == html_names::kDisabledAttr) {
    PseudoStateChanged(CSSSelector::kPseudoDisabled);
    PseudoStateChanged(CSSSelector::kPseudoEnabled);
  } else if (params.name == html_names::kLabelAttr) {
    UpdateGroupLabel();
  }
}

FocusableState HTMLOptGroupElement::SupportsFocus(
    UpdateBehavior update_behavior) const {
  HTMLSelectElement* select = OwnerSelectElement();
  if (select && select->UsesMenuList())
    return FocusableState::kNotFocusable;
  return HTMLElement::SupportsFocus(update_behavior);
}

bool HTMLOptGroupElement::MatchesEnabledPseudoClass() const {
  return !IsDisabledFormControl();
}

void HTMLOptGroupElement::ChildrenChanged(const ChildrenChange& change) {
  HTMLElement::ChildrenChanged(change);
  auto* select = OwnerSelectElement();
  if (!select)
    return;
  // Code path used for kFinishedBuildingDocumentFragmentTree should not be
  // hit for optgroups as fast-path parser does not handle optgroups.
  DCHECK_NE(change.type,
            ChildrenChangeType::kFinishedBuildingDocumentFragmentTree);
  if (change.type == ChildrenChangeType::kElementInserted) {
    if (auto* option = DynamicTo<HTMLOptionElement>(change.sibling_changed))
      select->OptionInserted(*option, option->Selected());
  } else if (change.type == ChildrenChangeType::kElementRemoved) {
    if (auto* option = DynamicTo<HTMLOptionElement>(change.sibling_changed))
      select->OptionRemoved(*option);
  } else if (change.type == ChildrenChangeType::kAllChildrenRemoved) {
    for (Node* node : change.removed_nodes) {
      if (auto* option = DynamicTo<HTMLOptionElement>(node))
        select->OptionRemoved(*option);
    }
  }
}

bool HTMLOptGroupElement::ChildrenChangedAllChildrenRemovedNeedsList() const {
  return true;
}

Node::InsertionNotificationRequest HTMLOptGroupElement::InsertedInto(
    ContainerNode& insertion_point) {
  customizable_select_rendering_ = false;
  HTMLElement::InsertedInto(insertion_point);
  if (HTMLSelectElement* select = OwnerSelectElement()) {
    if (&insertion_point == select)
      select->OptGroupInsertedOrRemoved(*this);
    // TODO(crbug.com/1511354): This UsesMenuList check doesn't account for
    // the case when the select's rendering is changed after insertion.
    customizable_select_rendering_ =
        RuntimeEnabledFeatures::CustomizableSelectEnabled() &&
        select->UsesMenuList();
  }
  if (RuntimeEnabledFeatures::CustomizableSelectEnabled()) {
    UpdateGroupLabel();
  }
  return kInsertionDone;
}

void HTMLOptGroupElement::RemovedFrom(ContainerNode& insertion_point) {
  if (auto* select = DynamicTo<HTMLSelectElement>(insertion_point)) {
    if (!parentNode())
      select->OptGroupInsertedOrRemoved(*this);
  }
  HTMLElement::RemovedFrom(insertion_point);
}

String HTMLOptGroupElement::GroupLabelText() const {
  String label_attribute_text = LabelAttributeText();
  if (RuntimeEnabledFeatures::CustomizableSelectEnabled() &&
      label_attribute_text.ContainsOnlyWhitespaceOrEmpty()) {
    for (auto& node : NodeTraversal::DescendantsOf(*this)) {
      if (auto* legend = DynamicTo<HTMLLegendElement>(node)) {
        return legend->textContent();
      }
    }
  }
  return label_attribute_text;
}

String HTMLOptGroupElement::LabelAttributeText() const {
  String item_text = FastGetAttribute(html_names::kLabelAttr);

  // In WinIE, leading and trailing whitespace is ignored in options and
  // optgroups. We match this behavior.
  item_text = item_text.StripWhiteSpace();
  // We want to collapse our whitespace too.  This will match other browsers.
  item_text = item_text.SimplifyWhiteSpace();

  return item_text;
}

HTMLSelectElement* HTMLOptGroupElement::OwnerSelectElement() const {
  if (RuntimeEnabledFeatures::SelectParserRelaxationEnabled()) {
    // TODO(crbug.com/351990825): Cache the owner select ancestor on insertion
    // rather than doing a tree traversal here every time OwnerSelectElement is
    // called, which may be a lot.
    for (Node& ancestor : NodeTraversal::AncestorsOf(*this)) {
      if (auto* select = DynamicTo<HTMLSelectElement>(ancestor)) {
        return select;
      }
    }
    return nullptr;
  } else {
    return DynamicTo<HTMLSelectElement>(parentNode());
  }
}

String HTMLOptGroupElement::DefaultToolTip() const {
  if (HTMLSelectElement* select = OwnerSelectElement())
    return select->DefaultToolTip();
  return String();
}

void HTMLOptGroupElement::AccessKeyAction(
    SimulatedClickCreationScope creation_scope) {
  HTMLSelectElement* select = OwnerSelectElement();
  // Send to the parent to bring focus to the list box.
  // TODO(crbug.com/1176745): investigate why we don't care
  // about creation scope.
  if (select && !select->IsFocused())
    select->AccessKeyAction(SimulatedClickCreationScope::kFromUserAgent);
}

void HTMLOptGroupElement::DidAddUserAgentShadowRoot(ShadowRoot& root) {
  DEFINE_STATIC_LOCAL(AtomicString, label_padding, ("0 2px 1px 2px"));
  DEFINE_STATIC_LOCAL(AtomicString, label_min_height, ("1.2em"));
  auto* label = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  label->setAttribute(html_names::kAriaHiddenAttr, AtomicString("true"));
  label->SetInlineStyleProperty(CSSPropertyID::kPadding, label_padding);
  label->SetInlineStyleProperty(CSSPropertyID::kMinHeight, label_min_height);
  label->SetIdAttribute(shadow_element_names::kIdOptGroupLabel);
  root.AppendChild(label);
  opt_group_slot_ = MakeGarbageCollected<HTMLSlotElement>(GetDocument());
  root.AppendChild(opt_group_slot_);
}

void HTMLOptGroupElement::ManuallyAssignSlots() {
  HeapVector<Member<Node>> opt_group_nodes;
  for (Node& child : NodeTraversal::ChildrenOf(*this)) {
    if (!child.IsSlotable())
      continue;
    if (customizable_select_rendering_ || CanAssignToOptGroupSlot(child)) {
      opt_group_nodes.push_back(child);
    }
  }
  opt_group_slot_->Assign(opt_group_nodes);
}

void HTMLOptGroupElement::UpdateGroupLabel() {
  const String& label_text = LabelAttributeText();
  HTMLDivElement& label = OptGroupLabelElement();
  label.setTextContent(label_text);
  label.setAttribute(html_names::kAriaLabelAttr, AtomicString(label_text));
  if (label_text.ContainsOnlyWhitespaceOrEmpty()) {
    if (customizable_select_rendering_) {
      // If the author uses <legend> to label the <optgroup> instead of the
      // label attribute, then we don't want extra space being taken up for the
      // unused label attribute.
      label.SetInlineStyleProperty(CSSPropertyID::kDisplay, "none");
    }
  } else {
    label.RemoveInlineStyleProperty(CSSPropertyID::kDisplay);
  }
}

HTMLDivElement& HTMLOptGroupElement::OptGroupLabelElement() const {
  auto* element = UserAgentShadowRoot()->getElementById(
      shadow_element_names::kIdOptGroupLabel);
  CHECK(!element || IsA<HTMLDivElement>(element));
  return *To<HTMLDivElement>(element);
}

void HTMLOptGroupElement::Trace(Visitor* visitor) const {
  visitor->Trace(opt_group_slot_);
  HTMLElement::Trace(visitor);
}

}  // namespace blink
