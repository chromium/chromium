/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2010 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/html/forms/html_field_set_element.h"

#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatch_forbidden_scope.h"
#include "third_party/blink/renderer/core/dom/layout_tree_builder_traversal.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/node_lists_node_data.h"
#include "third_party/blink/renderer/core/html/custom/element_internals.h"
#include "third_party/blink/renderer/core/html/forms/html_legend_element.h"
#include "third_party/blink/renderer/core/html/html_collection.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/forms/layout_fieldset.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

using mojom::blink::FormControlType;

namespace {

bool WillReattachChildLayoutObject(const Element& parent) {
  for (const Node* child = LayoutTreeBuilderTraversal::FirstChild(parent);
       child; child = LayoutTreeBuilderTraversal::NextSibling(*child)) {
    if (child->NeedsReattachLayoutTree()) {
      return true;
    }
    const auto* element = DynamicTo<Element>(child);
    if (!element || !element->ChildNeedsReattachLayoutTree()) {
      continue;
    }
    if (const ComputedStyle* style = element->GetComputedStyle()) {
      if (style->Display() == EDisplay::kContents &&
          WillReattachChildLayoutObject(*element)) {
        return true;
      }
    }
  }
  return false;
}

}  // namespace

HTMLFieldSetElement::HTMLFieldSetElement(Document& document)
    : HTMLFormControlElement(html_names::kFieldsetTag, document) {
  // This class has DidRecalcStyle().
  SetHasCustomStyleCallbacks();
}

bool HTMLFieldSetElement::MatchesValidityPseudoClasses() const {
  return true;
}

bool HTMLFieldSetElement::IsValidElement() {
  for (Element* element : *elements()) {
    if (auto* html_form_element = DynamicTo<HTMLFormControlElement>(element)) {
      if (!html_form_element->IsNotCandidateOrValid())
        return false;
    } else if (auto* html_element = DynamicTo<HTMLElement>(element)) {
      if (html_element->IsFormAssociatedCustomElement() &&
          !element->EnsureElementInternals().IsNotCandidateOrValid())
        return false;
    }
  }
  return true;
}

bool HTMLFieldSetElement::IsSubmittableElement() {
  return false;
}

// Returns a disabled focused element if it's in descendants of |base|.
Element*
HTMLFieldSetElement::InvalidateDescendantDisabledStateAndFindFocusedOne(
    Element& base) {
  Element* focused_element = AdjustedFocusedElementInTreeScope();
  bool should_blur = false;
  {
    EventDispatchForbiddenScope event_forbidden;
    for (HTMLElement& element : Traversal<HTMLElement>::DescendantsOf(base)) {
      if (auto* control = DynamicTo<HTMLFormControlElement>(element))
        control->AncestorDisabledStateWasChanged();
      else if (element.IsFormAssociatedCustomElement())
        element.EnsureElementInternals().AncestorDisabledStateWasChanged();
      else
        continue;
      if (focused_element == &element && element.IsDisabledFormControl())
        should_blur = true;
    }
  }
  return should_blur ? focused_element : nullptr;
}

void HTMLFieldSetElement::DisabledAttributeChanged() {
  // This element must be updated before the style of nodes in its subtree gets
  // recalculated.
  HTMLFormControlElement::DisabledAttributeChanged();
  if (Element* focused_element =
          InvalidateDescendantDisabledStateAndFindFocusedOne(*this))
    focused_element->blur();
}

void HTMLFieldSetElement::AncestorDisabledStateWasChanged() {
  ancestor_disabled_state_ = AncestorDisabledState::kUnknown;
  // Do not re-enter HTMLFieldSetElement::DisabledAttributeChanged(), so that
  // we only invalidate this element's own disabled state and do not traverse
  // the descendants.
  HTMLFormControlElement::DisabledAttributeChanged();
}

void HTMLFieldSetElement::ChildrenChanged(const ChildrenChange& change) {
  HTMLFormControlElement::ChildrenChanged(change);
  Element* focused_element = nullptr;
  {
    EventDispatchForbiddenScope event_forbidden;
    for (HTMLLegendElement& legend :
         Traversal<HTMLLegendElement>::ChildrenOf(*this)) {
      if (Element* element =
              InvalidateDescendantDisabledStateAndFindFocusedOne(legend))
        focused_element = element;
    }
  }
  if (focused_element)
    focused_element->blur();
}

FocusableState HTMLFieldSetElement::SupportsFocus(
    UpdateBehavior update_behavior) const {
  if (IsDisabledFormControl()) {
    return FocusableState::kNotFocusable;
  }
  return HTMLElement::SupportsFocus(update_behavior);
}

FormControlType HTMLFieldSetElement::FormControlType() const {
  return FormControlType::kFieldset;
}

const AtomicString& HTMLFieldSetElement::FormControlTypeAsString() const {
  DEFINE_STATIC_LOCAL(const AtomicString, fieldset, ("fieldset"));
  return fieldset;
}

LayoutObject* HTMLFieldSetElement::CreateLayoutObject(const ComputedStyle&) {
  return MakeGarbageCollected<LayoutFieldset>(this);
}

LayoutBox* HTMLFieldSetElement::GetLayoutBoxForScrolling() const {
  if (const auto* ng_fieldset = DynamicTo<LayoutFieldset>(GetLayoutBox())) {
    if (auto* content = ng_fieldset->FindAnonymousFieldsetContentBox()) {
      return content;
    }
  }
  return HTMLFormControlElement::GetLayoutBoxForScrolling();
}

void HTMLFieldSetElement::DidRecalcStyle(const StyleRecalcChange change) {
  if (ChildNeedsReattachLayoutTree() && WillReattachChildLayoutObject(*this))
    SetNeedsReattachLayoutTree();
}

HTMLLegendElement* HTMLFieldSetElement::Legend() const {
  return Traversal<HTMLLegendElement>::FirstChild(*this);
}

HTMLCollection* HTMLFieldSetElement::elements() {
  return EnsureCachedCollection<HTMLCollection>(kFormControls);
}

bool HTMLFieldSetElement::IsDisabledFormControl() const {
  // The fieldset element itself should never be considered disabled, it is
  // only supposed to affect its descendants:
  // https://html.spec.whatwg.org/multipage/form-control-infrastructure.html#concept-fe-disabled
  return false;
}

// <fieldset> should never be considered disabled, but should still match the
// :enabled or :disabled pseudo-classes according to whether the attribute is
// set or not. See here for context:
// https://github.com/whatwg/html/issues/5886#issuecomment-1582410112
bool HTMLFieldSetElement::MatchesEnabledPseudoClass() const {
  return !IsActuallyDisabled();
}

}  // namespace blink
