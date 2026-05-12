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
#include "third_party/blink/renderer/core/keywords.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"

namespace blink {

namespace {

HTMLLegendElement* FirstChildLegend(const HTMLOptGroupElement& optgroup) {
  return Traversal<HTMLLegendElement>::FirstChild(optgroup);
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

// The :enabled and :disabled selectors have special behavior for option
// elements which is separate from their normal disabledness state. The
// selectors depend on whether the ancestor select is disabled, but the internal
// state does not. See https://github.com/w3c/csswg-drafts/issues/13383
bool HTMLOptGroupElement::MatchesEnabledPseudoClass() const {
  if (!RuntimeEnabledFeatures::OptionDisablednessCheckAncestorsEnabled()) {
    return !IsDisabledFormControl();
  }
  return !MatchesDisabledPseudoClass();
}
bool HTMLOptGroupElement::MatchesDisabledPseudoClass() const {
  if (IsDisabledFormControl()) {
    return true;
  }
  if (!RuntimeEnabledFeatures::OptionDisablednessCheckAncestorsEnabled()) {
    return false;
  }
  if (owner_select_ && owner_select_->IsDisabledFormControl()) {
    return true;
  }
  return false;
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
    if (IsA<HTMLLegendElement>(change.sibling_changed)) {
      UpdateGroupLabel();
    }
  } else if (change.type == ChildrenChangeType::kElementRemoved) {
    if (IsA<HTMLLegendElement>(change.sibling_changed)) {
      UpdateGroupLabel();
    }
  } else if (change.type == ChildrenChangeType::kAllChildrenRemoved) {
    for (Node* node : change.removed_nodes) {
      if (IsA<HTMLLegendElement>(node)) {
        UpdateGroupLabel();
      }
    }
  }
}

bool HTMLOptGroupElement::ChildrenChangedAllChildrenRemovedNeedsList() const {
  return true;
}

Node::InsertionNotificationRequest HTMLOptGroupElement::InsertedInto(
    ContainerNode& insertion_point) {
  HTMLElement::InsertedInto(insertion_point);

  owner_select_ = HTMLSelectElement::WalkAncestorsForRelatedParts(*this).select;
  if (owner_select_) {
    owner_select_->OptGroupInsertedOrRemoved(*this);
  }
  UpdateGroupLabel();

  if (HTMLSelectElement* select = OwnerSelectElement()) {
    if (&insertion_point == select) {
      select->OptGroupInsertedOrRemoved(*this);
    }
  }
  return kInsertionDone;
}

void HTMLOptGroupElement::RemovedFrom(ContainerNode& insertion_point) {
  HTMLSelectElement* new_ancestor_select =
      HTMLSelectElement::WalkAncestorsForRelatedParts(*this).select;
  if (owner_select_ != new_ancestor_select) {
    // When removing, we can only lose an associated <select>
    CHECK(owner_select_);
    CHECK(!new_ancestor_select);
    owner_select_->OptGroupInsertedOrRemoved(*this);
    owner_select_ = new_ancestor_select;
  }

  HTMLElement::RemovedFrom(insertion_point);
}

String HTMLOptGroupElement::GroupLabelText() const {
  String label_attribute_text = LabelAttributeText();
  if (label_attribute_text.ContainsOnlyWhitespaceOrEmpty()) {
    if (auto* legend = FirstChildLegend(*this)) {
      return legend->textContent();
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

HTMLSelectElement* HTMLOptGroupElement::OwnerSelectElement(
    bool skip_check) const {
  if (!skip_check) {
    DCHECK_EQ(owner_select_,
              HTMLSelectElement::WalkAncestorsForRelatedParts(*this).select);
  }
  return owner_select_;
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
  label_ = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  label_->setAttribute(html_names::kAriaHiddenAttr, keywords::kTrue);
  label_->SetShadowPseudoId(shadow_element_names::kIdOptGroupLabel);
  root.AppendChild(label_);
  opt_group_slot_ = MakeGarbageCollected<HTMLSlotElement>(GetDocument());
  root.AppendChild(opt_group_slot_);
}

void HTMLOptGroupElement::ManuallyAssignSlots() {
  HeapVector<Member<Node>> opt_group_nodes;
  for (Node& child : NodeTraversal::ChildrenOf(*this)) {
    if (!child.IsSlotable())
      continue;
    opt_group_nodes.push_back(child);
  }
  opt_group_slot_->Assign(opt_group_nodes);
}

void HTMLOptGroupElement::UpdateGroupLabel() {
  const String& label_text = LabelAttributeText();
  HTMLDivElement& label = OptGroupLabelElement();
  label.setTextContent(label_text);
  label.setAttribute(html_names::kAriaLabelAttr, AtomicString(label_text));

  // If the author provides a <legend> element to replace the label attribute,
  // then don't render the label element because it would result in an unwanted
  // empty line. Otherwise, always render the label element, even if it results
  // in an empty line. See fast/forms/select/listbox-appearance-basic.html.
  if (FirstChildLegend(*this)) {
    // TODO(crbug.com/383841336): Consider replacing this with UA style rules
    // if we can make the label attribute become a part like pseudo-element,
    // and add more tests for the label attribute with base appearance
    // rendering.
    label.SetInlineStyleProperty(CSSPropertyID::kDisplay, "none");
  } else {
    label.RemoveInlineStyleProperty(CSSPropertyID::kDisplay);
  }
}

HTMLDivElement& HTMLOptGroupElement::OptGroupLabelElement() const {
  return *label_;
}

void HTMLOptGroupElement::Trace(Visitor* visitor) const {
  visitor->Trace(opt_group_slot_);
  visitor->Trace(label_);
  visitor->Trace(owner_select_);
  HTMLElement::Trace(visitor);
}

}  // namespace blink
