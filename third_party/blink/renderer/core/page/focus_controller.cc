/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nuanti Ltd.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/page/focus_controller.h"

#include <limits>
#include <ranges>

#include "base/containers/adapters.h"
#include "base/memory/stack_allocated.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom-blink.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/dom/column_pseudo_element.h"
#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/dom/popover_data.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/dom/scroll_marker_group_pseudo_element.h"
#include "third_party/blink/renderer/core/dom/scroll_marker_pseudo_element.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"  // For firstPositionInOrBeforeNode
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/ime/input_method_controller.h"
#include "third_party/blink/renderer/core/frame/frame_client.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/remote_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/fenced_frame/html_fenced_frame_element.h"
#include "third_party/blink/renderer/core/html/forms/html_form_control_element.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/forms/listed_element.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/html/html_plugin_element.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/focus_changed_observer.h"
#include "third_party/blink/renderer/core/page/focusgroup_controller_utils.h"
#include "third_party/blink/renderer/core/page/frame_tree.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/spatial_navigation.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

namespace {

// Start of carousel helpers for focus navigation.
bool ElementHasScrollButton(const Element& element) {
  return element.GetPseudoElement(kPseudoIdScrollButtonBlockStart) ||
         element.GetPseudoElement(kPseudoIdScrollButtonInlineStart) ||
         element.GetPseudoElement(kPseudoIdScrollButtonInlineEnd) ||
         element.GetPseudoElement(kPseudoIdScrollButtonBlockEnd);
}

bool ElementHasCarouselPseudoElement(const Element& element) {
  return element.GetPseudoElement(kPseudoIdScrollMarkerGroupBefore) ||
         element.GetPseudoElement(kPseudoIdScrollMarkerGroupAfter) ||
         ElementHasScrollButton(element);
}

// As per https://drafts.csswg.org/css-overflow-5/#focus-order,
// focus order for carousel scroller and pseudo-elements is different
// from usual DOM order, these functions here help to achieve the specced
// order.
// is_scroller_in_links_mode determines if scroll-marker-group property of the
// scroller is set to `links` (vs `tabs`), as in links mode every
// ::scroll-marker is a tab stop, so we should return the first/last (based on
// `forward`) ::scroll-marker here.
template <bool forward = true>
Element* GetSelectedScrollMarkerFromScrollMarkerGroup(
    const Element& current,
    const bool scroller_in_links_mode) {
  if (auto* scroll_marker_group =
          DynamicTo<ScrollMarkerGroupPseudoElement>(current)) {
    if (scroller_in_links_mode) {
      return forward ? scroll_marker_group->First()
                     : scroll_marker_group->Last();
    }
    return scroll_marker_group->Selected();
  }
  return nullptr;
}

bool IsScrollerInMode(const Element& scroller,
                      ScrollMarkerGroup::ScrollMarkerMode mode) {
  std::optional<ScrollMarkerGroup::ScrollMarkerMode> scroller_mode =
      scroller.ComputedStyleRef().ScrollMarkerGroupMode();
  return scroller_mode.has_value() && scroller_mode.value() == mode &&
         RuntimeEnabledFeatures::CSSScrollMarkerGroupModesEnabled();
}

bool IsScrollerInLinksMode(const Element& scroller) {
  return IsScrollerInMode(scroller,
                          ScrollMarkerGroup::ScrollMarkerMode::kLinks);
}

bool IsScrollMarkerFromScrollerInTabsMode(const Element& maybe_scroll_marker) {
  auto* scroll_marker =
      DynamicTo<ScrollMarkerPseudoElement>(maybe_scroll_marker);
  if (!scroll_marker) {
    return false;
  }
  Element* scroller = scroll_marker->ScrollMarkerGroup()->parentElement();
  DCHECK(scroller);
  return IsScrollerInMode(*scroller,
                          ScrollMarkerGroup::ScrollMarkerMode::kTabs);
}

// Carousel pseudo-elements order.
static constexpr std::array<PseudoId, 6> carousel_focus_order = {
    kPseudoIdScrollMarkerGroupBefore, kPseudoIdScrollMarkerGroupAfter,
    kPseudoIdScrollButtonBlockStart,  kPseudoIdScrollButtonInlineStart,
    kPseudoIdScrollButtonInlineEnd,   kPseudoIdScrollButtonBlockEnd,
};

// Overwrites the DOM source order if it is currently inside a carousel or might
// move into the carousel element. If not, it will call NextIncludingPseudo.
// Carousel here means scroller with some special pseudo-elements and the focus
// order changes as described below.
// DOM order for carousel is:
// scroller, ::scroll-marker-group(before), ::scroll-button(),
// scroller's children (with ::scroll-markers), ::scroll-marker-group(after).
// Carousel focus order is defined as following
// (https://drafts.csswg.org/css-overflow-5/#focus-order):
// active ::scroll-marker from ::scroll-marker-group(both before and after),
// ::scroll-button(), scroller, scroller's children.
template <bool forward = true, class FocusOrderContainer>
Element* GetInCarouselOrder(const Element& scroller,
                            PseudoId current_pseudo_id,
                            const FocusOrderContainer& focus_order) {
  DCHECK(ElementHasCarouselPseudoElement(scroller));
  // Find in carousel focus order.
  bool current_pseudo_id_visited = current_pseudo_id == kPseudoIdNone;
  for (PseudoId pseudo_id : focus_order) {
    if (!current_pseudo_id_visited) {
      current_pseudo_id_visited = current_pseudo_id == pseudo_id;
      continue;
    }
    if (PseudoElement* pseudo = scroller.GetPseudoElement(pseudo_id)) {
      // If the scroll-marker-group mode of the scroller is `links`, every
      // scroll marker is a tab stop.
      if (Element* scroll_marker =
              GetSelectedScrollMarkerFromScrollMarkerGroup<forward>(
                  *pseudo, IsScrollerInLinksMode(scroller))) {
        return scroll_marker;
      }
      return pseudo;
    }
  }
  DCHECK_NE(current_pseudo_id, kPseudoIdNone);
  return forward ? const_cast<Element*>(&scroller) : nullptr;
}

Element* GetNextInCarouselOrder(const Element& scroller,
                                PseudoId current_pseudo_id) {
  return GetInCarouselOrder</*forward=*/true>(scroller, current_pseudo_id,
                                              carousel_focus_order);
}

Element* GetPrevInCarouselOrder(const Element& scroller,
                                PseudoId current_pseudo_id) {
  return GetInCarouselOrder</*forward=*/false>(
      scroller, current_pseudo_id, base::Reversed(carousel_focus_order));
}

// Tries to do carousel pseudos -> scroller step,
// also handles going "inside" ::column.
Element* GetNextForCarouselPseudoInFocusOrder(
    const Element& current,
    const ContainerNode* stay_within) {
  // Special case for ::column.
  if (auto* column_pseudo = DynamicTo<ColumnPseudoElement>(current)) {
    if (Element* first_in_column = column_pseudo->FirstChildInDOMOrder()) {
      return first_in_column;
    }
    // No elements in this column, nor in any of the columns that follow.
    const Element& multicol = column_pseudo->UltimateOriginatingElement();
    return ElementTraversal::NextSkippingChildren(multicol, stay_within);
  }
  // Try to find next per carousel focus order.
  if (current.IsCarouselPseudoElement()) {
    Element* scroller = current.parentElement();
    PseudoId pseudo_id = current.GetPseudoId();
    // Adjust for ::scroll-marker.
    if (auto* scroll_marker = DynamicTo<ScrollMarkerPseudoElement>(current)) {
      scroller = scroll_marker->ScrollMarkerGroup()->parentElement();
      pseudo_id = scroll_marker->ScrollMarkerGroup()->GetPseudoId();
      // If the scroll-marker-group mode of the scroller is `links`, every
      // scroll marker is a tab stop.
      if (IsScrollerInLinksMode(*scroller)) {
        if (auto* next_scroll_marker =
                scroll_marker->GetLayoutObject()->NextSibling()) {
          CHECK(next_scroll_marker->IsScrollMarker());
          return To<Element>(next_scroll_marker->GetNode());
        }
      }
    }
    return GetNextInCarouselOrder(*scroller, pseudo_id);
  }
  return nullptr;
}

// If on a scroller, goes inside its children.
Element* PreAdjustNextForCarouselFocusOrder(const Element& current,
                                            const ContainerNode* stay_within) {
  return ElementHasCarouselPseudoElement(current)
             ? ElementTraversal::Next(current, stay_within)
             : ElementTraversal::NextIncludingPseudo(current, stay_within);
}

// Goes from a scroller to the first of its carousel pseudos, if it has any,
// as we should first reach them in focus order.
Element* PostAdjustNextForCarouselFocusOrder(const Element& current,
                                             Element* next) {
  if (next && ElementHasCarouselPseudoElement(*next)) {
    return GetNextInCarouselOrder(*next, kPseudoIdNone);
  }
  return next;
}

// Tries to do scroller -> last of carousel pseudos
// or current carousel pseudo -> prev carousel pseudo step.
Element* GetPreviousForCarouselPseudoInFocusOrder(
    const Element& current,
    const ContainerNode* stay_within) {
  // Try to find previous per carousel focus order.
  if (current.IsCarouselPseudoElement()) {
    Element* scroller = current.parentElement();
    PseudoId pseudo_id = current.GetPseudoId();
    // Adjust for ::scroll-marker.
    if (auto* scroll_marker = DynamicTo<ScrollMarkerPseudoElement>(current)) {
      scroller = scroll_marker->ScrollMarkerGroup()->parentElement();
      pseudo_id = scroll_marker->ScrollMarkerGroup()->GetPseudoId();
      // If the scroll-marker-group mode of the scroller is `links`, every
      // scroll marker is a tab stop.
      if (IsScrollerInLinksMode(*scroller)) {
        if (auto* prev_scroll_marker =
                scroll_marker->GetLayoutObject()->PreviousSibling()) {
          CHECK(prev_scroll_marker->IsScrollMarker());
          return To<Element>(prev_scroll_marker->GetNode());
        }
      }
    }
    return GetPrevInCarouselOrder(*scroller, pseudo_id);
  }
  // In the `tabs` mode, the ::scroll-marker pseudo-element is a focus
  // navigation scope owner for its associated originating element. This means
  // that the backwards tab focus moves from the content to the scroll marker.
  if (RuntimeEnabledFeatures::CSSScrollMarkerGroupModesEnabled()) {
    if (auto* scroll_marker = DynamicTo<ScrollMarkerPseudoElement>(
            current.GetPseudoElement(kPseudoIdScrollMarker))) {
      if (scroll_marker->IsSelected() &&
          IsScrollMarkerFromScrollerInTabsMode(*scroll_marker)) {
        return scroll_marker;
      }
    }
  }
  if (ElementHasCarouselPseudoElement(current)) {
    return GetPrevInCarouselOrder(current, kPseudoIdNone);
  }
  return nullptr;
}

// From carousel pseudos we need to start our search from scroller, as
// the order is carousel pseudos -> scroller -> scroller's children.
Element* PreAdjustPreviousForCarouselFocusOrder(
    const Element& current,
    const ContainerNode* stay_within) {
  if (!current.IsCarouselPseudoElement()) {
    return ElementTraversal::PreviousIncludingPseudo(current, stay_within);
  }
  Element* scroller = current.parentElement();
  // Adjust for ::scroll-marker.
  if (auto* scroll_marker = DynamicTo<ScrollMarkerPseudoElement>(current)) {
    scroller = scroll_marker->ScrollMarkerGroup()->parentElement();
  }
  return ElementTraversal::Previous(*scroller, stay_within);
}

// Goes from carousel pseudo to its scroller, as we should first reach it
// in backward order.
Element* PostAdjustPreviousForCarouselFocusOrder(const Element& current,
                                                 Element* previous) {
  if (previous && previous->IsCarouselPseudoElement()) {
    return previous->parentElement();
  }
  return previous;
}
// End of carousel helpers.

Element* InvokerForOpenPopover(const Node* node) {
  auto* popover = DynamicTo<HTMLElement>(node);
  if (!popover || !popover->popoverOpen()) {
    return nullptr;
  }
  return popover->GetPopoverData()->invoker();
}

const Element* InclusiveAncestorOpenPopoverWithInvoker(const Element* element) {
  for (const Element* current = element; current;
       current = FlatTreeTraversal::ParentElement(*current)) {
    if (RuntimeEnabledFeatures::
            OpenPopoverInvokerRestrictToSameTreeScopeEnabled() &&
        element->GetTreeScope() != current->GetTreeScope()) {
      break;
    }
    if (InvokerForOpenPopover(current)) {
      return current;  // Return the popover
    }
  }
  return nullptr;
}


// A reading-flow item scope owner is a reading-flow item that is not a scope
// owner by other definitions.
bool IsReadingFlowItemScopeOwner(const ContainerNode* node) {
  // An iframe scope behaves the same way as a reading-flow item scope. We add
  // this condition to avoid overlapping definition, which will mess up finding
  // focusable elements across scopes.
  if (IsA<HTMLIFrameElement>(node)) {
    return false;
  }
  if (const Element* element = DynamicTo<Element>(node)) {
    if (ContainerNode* closest_layout_parent =
            LayoutTreeBuilderTraversal::LayoutParent(*element)) {
      return closest_layout_parent->IsReadingFlowContainer();
    }
  }
  return false;
}

// Returns true if node is a reading-flow container, a display: contents
// with a reading-flow container as its layout parent, or a reading-flow
// item scope owner.
bool IsReadingFlowScopeOwner(const ContainerNode* node) {
  return FocusController::ReadingFlowContainerOrDisplayContents(node) ||
         IsReadingFlowItemScopeOwner(node);
}

// This class defines the navigation order.
class FocusNavigation final {
  STACK_ALLOCATED();

 public:
  static FocusNavigation Create(ContainerNode& scoping_root_node,
                                FocusController::OwnerMap& owner_map) {
    if (auto* slot = DynamicTo<HTMLSlotElement>(scoping_root_node)) {
      if (slot->AssignedNodes().empty()) {
        return FocusNavigation(scoping_root_node, *slot, owner_map);
      }
      // Here, slot->AssignedNodes() are non null, so the slot must be inside
      // the shadow tree.
      DCHECK(scoping_root_node.ContainingShadowRoot());
      return FocusNavigation(scoping_root_node.ContainingShadowRoot()->host(),
                             *slot, owner_map);
    }
    return FocusNavigation(scoping_root_node, owner_map);
  }

  void SetScrollMarkerInfo(ScrollMarkerPseudoElement& scroll_marker) {
    scroll_marker_ = &scroll_marker;
    root_ = &scroll_marker_->UltimateOriginatingElement();
  }

  void SetReadingFlowInfo(const ContainerNode& reading_flow_container) {
    DCHECK(reading_flow_container.GetLayoutBox());
    DCHECK(!reading_flow_container_);
    reading_flow_container_ = &reading_flow_container;
    HeapVector<Member<Element>> children;
    for (Node* reading_flow_node : Owner()->ReadingFlowChildren()) {
      Element* reading_flow_item = DynamicTo<Element>(reading_flow_node);
      if (!reading_flow_item || !IsOwnedByRoot(*reading_flow_item)) {
        continue;
      }
      children.push_back(reading_flow_item);
    }
    reading_flow_next_elements_.ReserveCapacityForSize(children.size());
    reading_flow_previous_elements_.ReserveCapacityForSize(children.size());
    Element* prev_element = nullptr;
    for (Element* child : children) {
      // Pseudo-elements in reading-flow are not focusable and should not be
      // included in the elements to traverse. Keep in sync with the behavior in
      // FocusgroupVisualOrderTraversalContext::BuildReadingFlowElementMappings.
      if (child->IsPseudoElement()) {
        continue;
      }
      if (!prev_element) {
        reading_flow_first_element_ = child;
      } else {
        reading_flow_next_elements_.insert(prev_element, child);
      }
      reading_flow_previous_elements_.insert(child, prev_element);
      prev_element = child;
    }
    if (prev_element) {
      reading_flow_next_elements_.insert(prev_element, nullptr);
      reading_flow_last_element_ = prev_element;
    }
#if DCHECK_IS_ON()
    // At this point, the number of reading flow elements added should equal the
    // number of children.
    size_t num_children = 0;
    for (Element& child : ElementTraversal::ChildrenOf(*root_)) {
      DCHECK(reading_flow_next_elements_.Contains(&child));
      ++num_children;
    }
    DCHECK_EQ(reading_flow_next_elements_.size(), num_children);
#endif
  }

  const Element* NextInDomOrder(const Element& current) {
    Element* next;
    if (RuntimeEnabledFeatures::PseudoElementsFocusableEnabled()) {
      if (Element* maybe_next =
              GetNextForCarouselPseudoInFocusOrder(current, root_)) {
        return maybe_next;
      }
      next = PreAdjustNextForCarouselFocusOrder(current, root_);
      // We skip every ::scroll-marker we find inside scroller,
      // since we only want to get to it from ::scroll-marker-group.
      // Also, we skip ::scroll-marker-group(after), as its location in
      // DOM order is different from carousel focus order.
      while (next &&
             (!IsOwnedByRoot(*next) || next->IsScrollMarkerPseudoElement() ||
              next->IsScrollMarkerGroupAfterPseudoElement())) {
        next = ElementTraversal::NextIncludingPseudo(*next, root_);
      }
      next = PostAdjustNextForCarouselFocusOrder(current, next);
    } else {
      next = ElementTraversal::Next(current, root_);
      while (next && !IsOwnedByRoot(*next)) {
        next = ElementTraversal::Next(*next, root_);
      }
    }
    return next;
  }

  // Given current element, find next element to traverse:
  // 1. If current scope is in a reading-flow container and the current element
  //    is a reading flow element, use the reading flow.
  // 2. Else, use the DOM tree order.
  const Element* Next(const Element& current) {
    return reading_flow_container_ &&
                   reading_flow_next_elements_.Contains(&current)
               ? reading_flow_next_elements_.at(&current)
               : NextInDomOrder(current);
  }

  const Element* PreviousInDomOrder(const Element& current) {
    Element* previous;
    if (RuntimeEnabledFeatures::PseudoElementsFocusableEnabled()) {
      if (Element* maybe_previous =
              GetPreviousForCarouselPseudoInFocusOrder(current, root_)) {
        return maybe_previous;
      }
      previous = PreAdjustPreviousForCarouselFocusOrder(current, root_);
      if (previous == root_) {
        return nullptr;
      }
      // We skip every ::scroll-marker we find inside scroller,
      // since we only want to get to it from ::scroll-marker-group.
      // Also, we skip ::scroll-marker-group(after), as its location in
      // DOM order is different from carousel focus order.
      while (previous && (!IsOwnedByRoot(*previous) ||
                          previous->IsScrollMarkerPseudoElement() ||
                          previous->IsScrollMarkerGroupAfterPseudoElement())) {
        previous = ElementTraversal::PreviousIncludingPseudo(*previous, root_);
      }
      previous = PostAdjustPreviousForCarouselFocusOrder(current, previous);
    } else {
      previous = ElementTraversal::Previous(current, root_);
      if (previous == root_) {
        return nullptr;
      }
      while (previous && !IsOwnedByRoot(*previous)) {
        previous = ElementTraversal::Previous(*previous, root_);
      }
    }
    return previous;
  }

  // Given current element, find previous element to traverse:
  // 1. If current scope is in a reading-flow container and the current element
  //    is a reading flow element, use the reading flow.
  // 2. Else, use the DOM tree order.
  const Element* Previous(const Element& current) {
    return reading_flow_container_ &&
                   reading_flow_previous_elements_.Contains(&current)
               ? reading_flow_previous_elements_.at(&current)
               : PreviousInDomOrder(current);
  }

  const Element* First() {
    // For scroller in tabs mode, we should start from the ultimate originating
    // element of ::scroll-marker.
    if (scroll_marker_) {
      DCHECK(IsScrollMarkerFromScrollerInTabsMode(*scroll_marker_));
      return &scroll_marker_->UltimateOriginatingElement();
    }
    if (reading_flow_first_element_) {
      return reading_flow_first_element_;
    }
    Element* first = ElementTraversal::FirstChild(*root_);
    while (first && !IsOwnedByRoot(*first))
      first = ElementTraversal::Next(*first, root_);
    return first;
  }

  const Element* Last() {
    if (reading_flow_last_element_) {
      return reading_flow_last_element_;
    }
    const Element* last = ElementTraversal::LastWithin(*root_);
    while (last && !IsOwnedByRoot(const_cast<Element&>(*last))) {
      last = ElementTraversal::Previous(*last, root_);
    }
    return last;
  }

  Element* Owner() {
    if (slot_) {
      return slot_;
    }
    if (IsReadingFlowScopeOwner(root_)) {
      return DynamicTo<Element>(*root_);
    }
    if (scroll_marker_) {
      return scroll_marker_;
    }
    return FindOwner(*root_);
  }

  bool HasReadingFlowContainer() const { return reading_flow_container_; }

 private:
  FocusNavigation(ContainerNode& root, FocusController::OwnerMap& owner_map)
      : root_(&root), owner_map_(&owner_map) {
    Element* element = DynamicTo<Element>(root);
    if (ShadowRoot* shadow_root = DynamicTo<ShadowRoot>(root)) {
      // We need to check the shadow host when the root is a shadow root.
      element = &shadow_root->host();
    }
    if (auto* container =
            FocusController::ReadingFlowContainerOrDisplayContents(element)) {
      SetReadingFlowInfo(*container);
    }
    if (auto* scroll_marker = DynamicTo<ScrollMarkerPseudoElement>(element)) {
      SetScrollMarkerInfo(*scroll_marker);
    }
  }
  FocusNavigation(ContainerNode& root,
                  HTMLSlotElement& slot,
                  FocusController::OwnerMap& owner_map)
      : root_(&root), slot_(&slot), owner_map_(&owner_map) {
    // Slot scope might have to follow reading flow if its closest layout
    // parent is a reading flow container.
    // TODO(crbug.com/336358906): Re-evaluate for content-visibility case.
    if (auto* container =
            FocusController::ReadingFlowContainerOrDisplayContents(&slot)) {
      SetReadingFlowInfo(*container);
    }
  }

  Element* TreeOwner(ContainerNode* node) {
    if (ShadowRoot* shadow_root = DynamicTo<ShadowRoot>(node))
      return &shadow_root->host();
    // FIXME: Figure out the right thing for OOPI here.
    if (Frame* frame = node->GetDocument().GetFrame())
      return frame->DeprecatedLocalOwner();
    return nullptr;
  }

  // Owner of a FocusNavigation:
  // - If node is in slot scope, owner is the assigned slot (found by traversing
  //   ancestors).
  // - If node is in focus navigation scope of a scroll marker, owner is that
  //   scroll marker (found by traversing ancestors).
  // - If node is in a reading-flow container, owner is that container (found
  //   by traversing ancestors).
  // - If node is in a reading-flow item, owner is that reading flow item (found
  //   by traversing ancestors).
  // - If node is in slot fallback content scope, owner is the parent or
  //   shadowHost element.
  // - If node is in shadow tree scope, owner is the parent or shadowHost
  //   element.
  // - If node is in frame scope, owner is the iframe node.
  // - If node is inside an open popover with an invoker, owner is the invoker.
  Element* FindOwner(ContainerNode& node) {
    auto result = owner_map_->find(&node);
    if (result != owner_map_->end()) {
      return result->value.Get();
    }

    // Fallback contents owner is set to the nearest ancestor slot node even if
    // the slot node have assigned nodes.

    Element* owner = nullptr;
    Element* owner_slot_or_reading_flow_container = nullptr;
    if (Element* element = DynamicTo<Element>(node)) {
      owner_slot_or_reading_flow_container = FocusController::
          FindScopeOwnerSlotOrScrollMarkerOrReadingFlowContainer(*element);
    }
    if (owner_slot_or_reading_flow_container) {
      owner = owner_slot_or_reading_flow_container;
    } else if (IsA<HTMLSlotElement>(node.parentNode())) {
      owner = node.ParentOrShadowHostElement();
    } else if (&node == node.GetTreeScope().RootNode()) {
      owner = TreeOwner(&node);
    } else if (auto* invoker = InvokerForOpenPopover(&node)) {
      owner = invoker;
    } else if (node.parentNode()) {
      owner = FindOwner(*node.parentNode());
    }

    owner_map_->insert(&node, owner);
    return owner;
  }

  bool IsOwnedByRoot(ContainerNode& node) { return FindOwner(node) == Owner(); }

  ContainerNode* root_;
  HTMLSlotElement* slot_ = nullptr;
  FocusController::OwnerMap* owner_map_;
  // This member is the focus navigation scope owner ::scroll-marker, if it
  // exists.
  ScrollMarkerPseudoElement* scroll_marker_ = nullptr;
  // This member is the reading-flow container if it is exists.
  const ContainerNode* reading_flow_container_ = nullptr;
  // These members are the first and last reading flow elements in
  // the reading flow container if it has children.
  Element* reading_flow_first_element_ = nullptr;
  Element* reading_flow_last_element_ = nullptr;
  // Maps each element in reading_flow_container_ with its next and previous
  // reading ordered elements.
  HeapHashMap<Member<const Element>, Member<const Element>>
      reading_flow_next_elements_;
  HeapHashMap<Member<const Element>, Member<const Element>>
      reading_flow_previous_elements_;
};

class ScopedFocusNavigation {
  STACK_ALLOCATED();

 public:
  // Searches through the given tree scope, starting from start element, for
  // the next/previous selectable element that comes after/before start element.
  // The order followed is as specified in the HTML spec[1], which is elements
  // with tab indexes first (from lowest to highest), and then elements without
  // tab indexes (in document order).  The search algorithm also conforms the
  // Shadow DOM spec[2], which inserts sequence in a shadow tree into its host.
  //
  // @param start The element from which to start searching. The element after
  //              this will be focused. May be null.
  // @return The focus element that comes after/before start element.
  //
  // [1]
  // https://html.spec.whatwg.org/C/#sequential-focus-navigation
  // [2] https://w3c.github.io/webcomponents/spec/shadow/#focus-navigation
  Element* FindFocusableElement(mojom::blink::FocusType type) {
    return (type == mojom::blink::FocusType::kForward)
               ? NextFocusableElement()
               : PreviousFocusableElement();
  }

  Element* CurrentElement() const { return const_cast<Element*>(current_); }
  Element* Owner();

  static ScopedFocusNavigation CreateFor(const Element&,
                                         FocusController::OwnerMap&);
  static ScopedFocusNavigation CreateForDocument(Document&,
                                                 FocusController::OwnerMap&);
  static ScopedFocusNavigation OwnedByNonFocusableFocusScopeOwner(
      Element&,
      FocusController::OwnerMap&);
  static ScopedFocusNavigation OwnedByShadowHost(const Element&,
                                                 FocusController::OwnerMap&);
  static ScopedFocusNavigation OwnedByHTMLSlotElement(
      const HTMLSlotElement&,
      FocusController::OwnerMap&);
  static ScopedFocusNavigation OwnedByIFrame(const HTMLFrameOwnerElement&,
                                             FocusController::OwnerMap&);
  static ScopedFocusNavigation OwnedByPopoverInvoker(
      const Element&,
      FocusController::OwnerMap&);
  static ScopedFocusNavigation OwnedByScrollMarker(Element&,
                                                   FocusController::OwnerMap&);
  static ScopedFocusNavigation OwnedByReadingFlow(const Element&,
                                                  FocusController::OwnerMap&);
  static HTMLSlotElement* FindFallbackScopeOwnerSlot(const Element&);

 private:
  ScopedFocusNavigation(ContainerNode& scoping_root_node,
                        const Element* current,
                        FocusController::OwnerMap&);

  Element* FindElementWithExactTabIndex(int tab_index, mojom::blink::FocusType);
  Element* NextElementWithGreaterTabIndex(int tab_index);
  Element* PreviousElementWithLowerTabIndex(int tab_index);
  int ReadingFlowAdjustedTabIndex(const Element& element);
  Element* NextFocusableElement();
  Element* PreviousFocusableElement();

  // Returns true if the element is in a focusgroup segment but is not the
  // entry element for that segment. Such elements should be skipped during
  // sequential focus navigation.
  bool IsNonEntryFocusgroupItem(const Element& element);

  void SetCurrentElement(const Element* element) { current_ = element; }
  void MoveToNext();
  void MoveToPrevious();
  void MoveToFirst();
  void MoveToLast();

  const Element* current_;
  FocusNavigation navigation_;

  // Only populated when focusgroup feature is enabled.
  // Cache mapping the first focusgroup item in each segment to that segment's
  // entry element, avoiding redundant calls to
  // GetEntryElementForFocusgroupSegment. This cache does not persist across
  // focus navigation calls.
  // Key: First item in segment.
  // Value: Entry element for that segment.
  HeapHashMap<Member<const Element>, Member<const Element>>
      focusgroup_segment_entry_cache_;
};

ScopedFocusNavigation::ScopedFocusNavigation(
    ContainerNode& scoping_root_node,
    const Element* current,
    FocusController::OwnerMap& owner_map)
    : current_(current),
      navigation_(FocusNavigation::Create(scoping_root_node, owner_map)) {}

bool ScopedFocusNavigation::IsNonEntryFocusgroupItem(const Element& element) {
  if (!RuntimeEnabledFeatures::FocusgroupEnabled(
          element.GetExecutionContext())) {
    return false;
  }

  // Calling this on every element is expensive. TODO(janewman): We should keep
  // track of when we enter/exit focusgroups during navigation, and only call
  // this when we are inside a focusgroup.
  const Element* focusgroup_owner = focusgroup::FindFocusgroupOwner(&element);

  // GetFocusgroupOwnerOfItem additionally checks if the element is keyboard
  // focusable, avoid this expensive check as IsNonEntryFocusgroupItem assumes
  // the element is already keyboard focusable.
  DCHECK_EQ(focusgroup_owner,
            FocusgroupControllerUtils::GetFocusgroupOwnerOfItem(&element));
  if (!focusgroup_owner) {
    // Not in a focusgroup.
    return false;
  }

  // Find the first item in this element's segment to use as the cache key.
  const Element* segment_first_item =
      FocusgroupControllerUtils::FirstFocusgroupItemInSegment(element);
  // An element in a focusgroup defines a segment, so this should never be null.
  CHECK(segment_first_item);

  // Check if we've already computed the entry element for this segment.
  auto it = focusgroup_segment_entry_cache_.find(segment_first_item);
  const Element* segment_entry = nullptr;

  if (it != focusgroup_segment_entry_cache_.end()) {
    // Cache hit - use the cached entry element.
    segment_entry = it->value;
  } else {
    // Cache miss - compute and cache the entry element for this segment.
    // Use the optimized version since segment_first_item is already the first
    // item in the segment.
    segment_entry =
        FocusgroupControllerUtils::GetEntryElementForFocusgroupSegmentFromFirst(
            *segment_first_item, *focusgroup_owner);
    // By definition, a segment must have an entry element.
    CHECK(segment_entry) << "Focusgroup with owner "
                         << focusgroup_owner->ToString()
                         << " has segment with first item "
                         << segment_first_item->ToString()
                         << " but no entry element.";
    focusgroup_segment_entry_cache_.insert(segment_first_item, segment_entry);
  }

  // Return whether the current element is NOT the entry element.
  return segment_entry != &element;
}

void ScopedFocusNavigation::MoveToNext() {
  DCHECK(CurrentElement());
  SetCurrentElement(navigation_.Next(*CurrentElement()));
}

void ScopedFocusNavigation::MoveToPrevious() {
  DCHECK(CurrentElement());
  SetCurrentElement(navigation_.Previous(*CurrentElement()));
}

void ScopedFocusNavigation::MoveToFirst() {
  SetCurrentElement(navigation_.First());
}

void ScopedFocusNavigation::MoveToLast() {
  SetCurrentElement(navigation_.Last());
}

Element* ScopedFocusNavigation::Owner() {
  Element* owner = navigation_.Owner();
  // TODO(crbug.com/335909581): If the returned owner is a reading-flow
  // scope owner and a popover, we want the scope owner to be the invoker.
  if (auto* invoker = InvokerForOpenPopover(owner);
      invoker && IsReadingFlowScopeOwner(owner)) {
    return invoker;
  }
  return owner;
}

ScopedFocusNavigation ScopedFocusNavigation::CreateFor(
    const Element& current,
    FocusController::OwnerMap& owner_map) {
  if (Element* owner = FocusController::
          FindScopeOwnerSlotOrScrollMarkerOrReadingFlowContainer(current)) {
    return ScopedFocusNavigation(*owner, &current, owner_map);
  }
  if (HTMLSlotElement* slot =
          ScopedFocusNavigation::FindFallbackScopeOwnerSlot(current)) {
    return ScopedFocusNavigation(*slot, &current, owner_map);
  }
  if (auto* popover = InclusiveAncestorOpenPopoverWithInvoker(&current)) {
    return ScopedFocusNavigation(const_cast<Element&>(*popover), &current,
                                 owner_map);
  }
  DCHECK(current.IsInTreeScope());
  return ScopedFocusNavigation(current.GetTreeScope().RootNode(), &current,
                               owner_map);
}

ScopedFocusNavigation ScopedFocusNavigation::CreateForDocument(
    Document& document,
    FocusController::OwnerMap& owner_map) {
  return ScopedFocusNavigation(document, nullptr, owner_map);
}

ScopedFocusNavigation ScopedFocusNavigation::OwnedByNonFocusableFocusScopeOwner(
    Element& element,
    FocusController::OwnerMap& owner_map) {
  if (IsShadowHost(element)) {
    return ScopedFocusNavigation::OwnedByShadowHost(element, owner_map);
  }
  if (auto* scroll_marker = DynamicTo<ScrollMarkerPseudoElement>(element)) {
    if (IsScrollMarkerFromScrollerInTabsMode(*scroll_marker)) {
      return ScopedFocusNavigation::OwnedByScrollMarker(element, owner_map);
    }
  }
  if (IsReadingFlowScopeOwner(&element)) {
    return ScopedFocusNavigation::OwnedByReadingFlow(element, owner_map);
  }
  return ScopedFocusNavigation::OwnedByHTMLSlotElement(
      To<HTMLSlotElement>(element), owner_map);
}

ScopedFocusNavigation ScopedFocusNavigation::OwnedByShadowHost(
    const Element& element,
    FocusController::OwnerMap& owner_map) {
  DCHECK(IsShadowHost(element));
  return ScopedFocusNavigation(*element.GetShadowRoot(), nullptr, owner_map);
}

ScopedFocusNavigation ScopedFocusNavigation::OwnedByIFrame(
    const HTMLFrameOwnerElement& frame,
    FocusController::OwnerMap& owner_map) {
  DCHECK(frame.ContentFrame());
  return ScopedFocusNavigation(
      *To<LocalFrame>(frame.ContentFrame())->GetDocument(), nullptr, owner_map);
}

ScopedFocusNavigation ScopedFocusNavigation::OwnedByPopoverInvoker(
    const Element& invoker,
    FocusController::OwnerMap& owner_map) {
  HTMLElement* popover = invoker.GetOpenPopoverTarget();
  DCHECK(InvokerForOpenPopover(popover));
  return ScopedFocusNavigation(*popover, nullptr, owner_map);
}

ScopedFocusNavigation ScopedFocusNavigation::OwnedByScrollMarker(
    Element& scroll_marker,
    FocusController::OwnerMap& owner_map) {
  return ScopedFocusNavigation(scroll_marker, nullptr, owner_map);
}

ScopedFocusNavigation ScopedFocusNavigation::OwnedByReadingFlow(
    const Element& owner,
    FocusController::OwnerMap& owner_map) {
  DCHECK(IsReadingFlowScopeOwner(&owner));
  Element& element = const_cast<Element&>(owner);
  return ScopedFocusNavigation(element, nullptr, owner_map);
}

ScopedFocusNavigation ScopedFocusNavigation::OwnedByHTMLSlotElement(
    const HTMLSlotElement& element,
    FocusController::OwnerMap& owner_map) {
  HTMLSlotElement& slot = const_cast<HTMLSlotElement&>(element);
  return ScopedFocusNavigation(slot, nullptr, owner_map);
}

HTMLSlotElement* ScopedFocusNavigation::FindFallbackScopeOwnerSlot(
    const Element& element) {
  Element* parent = const_cast<Element*>(element.parentElement());
  while (parent) {
    if (auto* slot = DynamicTo<HTMLSlotElement>(parent))
      return slot->AssignedNodes().empty() ? slot : nullptr;
    parent = parent->parentElement();
  }
  return nullptr;
}

// Checks whether |element| is an <iframe> and seems like a captcha based on
// heuristics. The heuristics cannot be perfect and therefore is a subject to
// change, e.g. adding a list of domains of captcha providers to be compared
// with 'src' attribute.
bool IsLikelyCaptchaIframe(const Element& element) {
  auto* iframe_element = DynamicTo<HTMLIFrameElement>(element);
  if (!iframe_element) {
    return false;
  }
  DEFINE_STATIC_LOCAL(String, kCaptcha, ("captcha"));
  return iframe_element->FastGetAttribute(html_names::kSrcAttr)
             .Contains(kCaptcha) ||
         iframe_element->title().Contains(kCaptcha) ||
         iframe_element->GetIdAttribute().Contains(kCaptcha) ||
         iframe_element->GetNameAttribute().Contains(kCaptcha);
}

// Checks whether |element| is a captcha <iframe> or enclosed with such an
// <iframe>.
bool IsLikelyWithinCaptchaIframe(const Element& element,
                                 FocusController::OwnerMap& owner_map) {
  if (IsLikelyCaptchaIframe(element)) {
    return true;
  }
  ScopedFocusNavigation scope =
      ScopedFocusNavigation::CreateFor(element, owner_map);
  Element* scope_owner = scope.Owner();
  return scope_owner && IsLikelyCaptchaIframe(*scope_owner);
}

inline void DispatchBlurEvent(const Document& document,
                              Element& focused_element) {
  focused_element.DispatchBlurEvent(nullptr, mojom::blink::FocusType::kPage);
  if (focused_element == document.FocusedElement()) {
    focused_element.DispatchFocusOutEvent(event_type_names::kFocusout, nullptr);
    if (focused_element == document.FocusedElement())
      focused_element.DispatchFocusOutEvent(event_type_names::kDOMFocusOut,
                                            nullptr);
  }
}

inline void DispatchFocusEvent(const Document& document,
                               Element& focused_element) {
  focused_element.DispatchFocusEvent(nullptr, mojom::blink::FocusType::kPage);
  if (focused_element == document.FocusedElement()) {
    focused_element.DispatchFocusInEvent(event_type_names::kFocusin, nullptr,
                                         mojom::blink::FocusType::kPage);
    if (focused_element == document.FocusedElement()) {
      focused_element.DispatchFocusInEvent(event_type_names::kDOMFocusIn,
                                           nullptr,
                                           mojom::blink::FocusType::kPage);
    }
  }
}

inline void DispatchEventsOnWindowAndFocusedElement(Document* document,
                                                    bool focused) {
  DCHECK(document);
  // If we have a focused element we should dispatch blur on it before we blur
  // the window.  If we have a focused element we should dispatch focus on it
  // after we focus the window.  https://bugs.webkit.org/show_bug.cgi?id=27105

  // Do not fire events while modal dialogs are up.  See
  // https://bugs.webkit.org/show_bug.cgi?id=33962
  if (Page* page = document->GetPage()) {
    if (page->Paused())
      return;
  }

  if (!focused && document->FocusedElement()) {
    Element* focused_element = document->FocusedElement();
    // Use focus_type mojom::blink::FocusType::kPage, same as used in
    // DispatchBlurEvent.
    focused_element->SetFocused(false, mojom::blink::FocusType::kPage);
    focused_element->SetHasFocusWithinUpToAncestor(
        false, nullptr, /*need_snap_container_search=*/false);
    DispatchBlurEvent(*document, *focused_element);
  }

  if (LocalDOMWindow* window = document->domWindow()) {
    window->DispatchEvent(*Event::Create(focused ? event_type_names::kFocus
                                                 : event_type_names::kBlur));
  }
  if (focused && document->FocusedElement()) {
    Element* focused_element(document->FocusedElement());
    // Use focus_type mojom::blink::FocusType::kPage, same as used in
    // DispatchFocusEvent.
    focused_element->SetFocused(true, mojom::blink::FocusType::kPage);
    focused_element->SetHasFocusWithinUpToAncestor(
        true, nullptr, /*need_snap_container_search=*/false);
    DispatchFocusEvent(*document, *focused_element);
  }
}

inline bool HasCustomFocusLogic(const Element& element) {
  auto* html_element = DynamicTo<HTMLElement>(element);
  return html_element && html_element->HasCustomFocusLogic();
}

inline bool IsShadowHostWithoutCustomFocusLogic(const Element& element) {
  return IsShadowHost(element) && !HasCustomFocusLogic(element);
}

inline bool IsNonKeyboardFocusableShadowHost(const Element& element) {
  if (!IsShadowHostWithoutCustomFocusLogic(element) ||
      element.IsShadowHostWithDelegatesFocus()) {
    return false;
  }
  if (!element.IsFocusable()) {
    return true;
  }
  if (element.IsKeyboardFocusableSlow()) {
    return false;
  }
  // This host supports focus, but cannot be keyboard focused. For example:
  // - Tabindex is negative
  // - It is a scroller with focusable children
  // When tabindex is negative, we should not visit the host.
  return !(element.GetIntegralAttribute(html_names::kTabindexAttr, 0) < 0);
}

inline bool IsNonKeyboardFocusableReadingFlowOwner(const Element& element) {
  return IsReadingFlowScopeOwner(&element) &&
         !element.IsKeyboardFocusableSlow();
}

inline bool IsNonKeyboardFocusableScrollMarkerOwner(const Element& element) {
  return IsScrollMarkerFromScrollerInTabsMode(element) &&
         !element.IsKeyboardFocusableSlow();
}

inline bool IsKeyboardFocusableReadingFlowOwner(const Element& element) {
  return IsReadingFlowScopeOwner(&element) && element.IsKeyboardFocusableSlow();
}

inline bool IsKeyboardFocusableScrollMarkerOwner(const Element& element) {
  return IsScrollMarkerFromScrollerInTabsMode(element) &&
         element.IsKeyboardFocusableSlow();
}

inline bool IsKeyboardFocusableShadowHost(const Element& element) {
  return IsShadowHostWithoutCustomFocusLogic(element) &&
         (element.IsKeyboardFocusableSlow() ||
          element.IsShadowHostWithDelegatesFocus());
}

inline bool IsNonFocusableFocusScopeOwner(Element& element) {
  return IsNonKeyboardFocusableShadowHost(element) ||
         IsA<HTMLSlotElement>(element) ||
         IsNonKeyboardFocusableReadingFlowOwner(element) ||
         IsNonKeyboardFocusableScrollMarkerOwner(element);
}

inline bool ShouldVisit(Element& element) {
  DCHECK(!element.IsKeyboardFocusableSlow() ||
         FocusController::AdjustedTabIndex(element) >= 0)
      << "Keyboard focusable element with negative tabindex" << element;
  return element.IsKeyboardFocusableSlow() ||
         element.IsShadowHostWithDelegatesFocus() ||
         IsNonFocusableFocusScopeOwner(element) ||
         IsNonKeyboardFocusableScrollMarkerOwner(element);
}

Element* ScopedFocusNavigation::FindElementWithExactTabIndex(
    int tab_index,
    mojom::blink::FocusType type) {
  // Search is inclusive of start
  for (; CurrentElement(); type == mojom::blink::FocusType::kForward
                               ? MoveToNext()
                               : MoveToPrevious()) {
    Element* current = CurrentElement();
    if (ShouldVisit(*current) &&
        ReadingFlowAdjustedTabIndex(*current) == tab_index &&
        !IsNonEntryFocusgroupItem(*current)) {
      return current;
    }
  }
  return nullptr;
}

Element* ScopedFocusNavigation::NextElementWithGreaterTabIndex(int tab_index) {
  // Search is inclusive of start
  int winning_tab_index = std::numeric_limits<int>::max();
  Element* winner = nullptr;
  for (; CurrentElement(); MoveToNext()) {
    Element* current = CurrentElement();
    int current_tab_index = ReadingFlowAdjustedTabIndex(*current);
    if (ShouldVisit(*current) && current_tab_index > tab_index &&
        !IsNonEntryFocusgroupItem(*current)) {
      if (!winner || current_tab_index < winning_tab_index) {
        winner = current;
        winning_tab_index = current_tab_index;
      }
    }
  }
  SetCurrentElement(winner);
  return winner;
}

Element* ScopedFocusNavigation::PreviousElementWithLowerTabIndex(
    int tab_index) {
  // Search is inclusive of start
  int winning_tab_index = 0;
  Element* winner = nullptr;
  for (; CurrentElement(); MoveToPrevious()) {
    Element* current = CurrentElement();
    int current_tab_index = ReadingFlowAdjustedTabIndex(*current);
    if (ShouldVisit(*current) && current_tab_index < tab_index &&
        current_tab_index > winning_tab_index &&
        !IsNonEntryFocusgroupItem(*current)) {
      winner = current;
      winning_tab_index = current_tab_index;
    }
  }
  SetCurrentElement(winner);
  return winner;
}

// This function adjust the tabindex by the FocusController and by the rules of
// the reading-flow container focus navigation scope. If a reading-flow item
// has a tabindex higher than 0, it should be re-adjusted to 0.
// TODO(dizhangg) Add link to spec when it is available.
int ScopedFocusNavigation::ReadingFlowAdjustedTabIndex(const Element& element) {
  int tab_index = FocusController::AdjustedTabIndex(element);
  if (navigation_.HasReadingFlowContainer()) {
    return std::min(0, tab_index);
  }
  return tab_index;
}

Element* ScopedFocusNavigation::NextFocusableElement() {
  Element* current = CurrentElement();
  if (current) {
    int tab_index = ReadingFlowAdjustedTabIndex(*current);
    // If an element is excluded from the normal tabbing cycle, the next
    // focusable element is determined by tree order.
    if (tab_index < 0) {
      for (MoveToNext(); CurrentElement(); MoveToNext()) {
        current = CurrentElement();
        if (ShouldVisit(*current) &&
            ReadingFlowAdjustedTabIndex(*current) >= 0 &&
            !IsNonEntryFocusgroupItem(*current)) {
          return current;
        }
      }
    } else {
      // First try to find an element with the same tabindex as start that comes
      // after start in the scope.
      MoveToNext();
      if (Element* winner = FindElementWithExactTabIndex(
              tab_index, mojom::blink::FocusType::kForward))
        return winner;
    }
    if (!tab_index) {
      // We've reached the last element in the document with a tabindex of 0.
      // This is the end of the tabbing order.
      return nullptr;
    }
  }

  // Look for the first element in the scope that:
  // 1) has the lowest tabindex that is higher than start's tabindex (or 0, if
  //    start is null), and
  // 2) comes first in the scope, if there's a tie.
  MoveToFirst();
  if (Element* winner = NextElementWithGreaterTabIndex(
          current ? ReadingFlowAdjustedTabIndex(*current) : 0)) {
    return winner;
  }

  // There are no elements with a tabindex greater than start's tabindex,
  // so find the first element with a tabindex of 0.
  MoveToFirst();
  return FindElementWithExactTabIndex(0, mojom::blink::FocusType::kForward);
}

Element* ScopedFocusNavigation::PreviousFocusableElement() {
  // First try to find the last element in the scope that comes before start and
  // has the same tabindex as start.  If start is null, find the last element in
  // the scope with a tabindex of 0.
  int tab_index;
  Element* current = CurrentElement();
  if (current) {
    MoveToPrevious();
    tab_index = ReadingFlowAdjustedTabIndex(*current);
  } else {
    MoveToLast();
    tab_index = 0;
  }

  // However, if an element is excluded from the normal tabbing cycle, the
  // previous focusable element is determined by tree order
  if (tab_index < 0) {
    for (; CurrentElement(); MoveToPrevious()) {
      current = CurrentElement();
      if (ShouldVisit(*current) && ReadingFlowAdjustedTabIndex(*current) >= 0 &&
          !IsNonEntryFocusgroupItem(*current)) {
        return current;
      }
    }
  } else {
    if (Element* winner = FindElementWithExactTabIndex(
            tab_index, mojom::blink::FocusType::kBackward))
      return winner;
  }

  // There are no elements before start with the same tabindex as start, so look
  // for an element that:
  // 1) has the highest non-zero tabindex (that is less than start's tabindex),
  //    and
  // 2) comes last in the scope, if there's a tie.
  tab_index =
      (current && tab_index) ? tab_index : std::numeric_limits<int>::max();
  MoveToLast();
  return PreviousElementWithLowerTabIndex(tab_index);
}

Element* FindFocusableElementRecursivelyForward(
    ScopedFocusNavigation& scope,
    FocusController::OwnerMap& owner_map) {
  // Starting element is exclusive.
  while (Element* found =
             scope.FindFocusableElement(mojom::blink::FocusType::kForward)) {
    if (found->IsShadowHostWithDelegatesFocus()) {
      // If tabindex is positive, invalid, or missing, find focusable element
      // inside its shadow tree.
      if (FocusController::AdjustedTabIndex(*found) >= 0 &&
          IsShadowHostWithoutCustomFocusLogic(*found)) {
        ScopedFocusNavigation inner_scope =
            ScopedFocusNavigation::OwnedByShadowHost(*found, owner_map);
        if (Element* found_in_inner_focus_scope =
                FindFocusableElementRecursivelyForward(inner_scope,
                                                       owner_map)) {
          return found_in_inner_focus_scope;
        }
      }
      // Skip to the next element in the same scope.
      continue;
    }
    if (!IsNonFocusableFocusScopeOwner(*found))
      return found;

    // Now |found| is on a non focusable scope owner (either shadow host or
    // slot) Find inside the inward scope and return it if found. Otherwise
    // continue searching in the same scope.
    ScopedFocusNavigation inner_scope =
        ScopedFocusNavigation::OwnedByNonFocusableFocusScopeOwner(*found,
                                                                  owner_map);
    if (Element* found_in_inner_focus_scope =
            FindFocusableElementRecursivelyForward(inner_scope, owner_map))
      return found_in_inner_focus_scope;
  }
  return nullptr;
}

Element* FindFocusableElementRecursivelyBackward(
    ScopedFocusNavigation& scope,
    FocusController::OwnerMap& owner_map) {
  // Starting element is exclusive.
  while (Element* found =
             scope.FindFocusableElement(mojom::blink::FocusType::kBackward)) {
    // Now |found| is on a focusable shadow host.
    // Find inside shadow backwards. If any focusable element is found, return
    // it, otherwise return the host itself.
    if (IsKeyboardFocusableShadowHost(*found)) {
      ScopedFocusNavigation inner_scope =
          ScopedFocusNavigation::OwnedByShadowHost(*found, owner_map);
      Element* found_in_inner_focus_scope =
          FindFocusableElementRecursivelyBackward(inner_scope, owner_map);
      if (found_in_inner_focus_scope)
        return found_in_inner_focus_scope;
      if (found->IsShadowHostWithDelegatesFocus()) {
        continue;
      }
      return found;
    }

    if (IsKeyboardFocusableScrollMarkerOwner(*found) &&
        RuntimeEnabledFeatures::CSSScrollMarkerGroupModesEnabled() &&
        found != scope.Owner()) {
      ScopedFocusNavigation inner_scope =
          ScopedFocusNavigation::OwnedByScrollMarker(
              const_cast<Element&>(*found), owner_map);
      Element* found_in_inner_focus_scope =
          FindFocusableElementRecursivelyBackward(inner_scope, owner_map);
      if (found_in_inner_focus_scope) {
        return found_in_inner_focus_scope;
      }
      return found;
    }

    // Now |found| is on a focusable reading flow owner. Find inside
    // container backwards. If any focusable element is found, return it,
    // otherwise return the container itself.
    if (IsKeyboardFocusableReadingFlowOwner(*found)) {
      ScopedFocusNavigation inner_scope =
          ScopedFocusNavigation::OwnedByReadingFlow(*found, owner_map);
      Element* found_in_inner_focus_scope =
          FindFocusableElementRecursivelyBackward(inner_scope, owner_map);
      if (found_in_inner_focus_scope) {
        return found_in_inner_focus_scope;
      }
      return found;
    }

    // If delegatesFocus is true and tabindex is negative, skip the whole shadow
    // tree under the shadow host.
    if (found->IsShadowHostWithDelegatesFocus() &&
        FocusController::AdjustedTabIndex(*found) < 0) {
      continue;
    }

    // Now |found| is on a non focusable scope owner (a shadow host or a slot).
    // Find focusable element in descendant scope. If not found, find the next
    // focusable element within the current scope.
    if (IsNonFocusableFocusScopeOwner(*found)) {
      ScopedFocusNavigation inner_scope =
          ScopedFocusNavigation::OwnedByNonFocusableFocusScopeOwner(*found,
                                                                    owner_map);
      if (Element* found_in_inner_focus_scope =
              FindFocusableElementRecursivelyBackward(inner_scope, owner_map))
        return found_in_inner_focus_scope;
      continue;
    }
    if (!found->IsShadowHostWithDelegatesFocus()) {
      return found;
    }
  }
  return nullptr;
}

Element* FindFocusableElementRecursively(mojom::blink::FocusType type,
                                         ScopedFocusNavigation& scope,
                                         FocusController::OwnerMap& owner_map) {
  return (type == mojom::blink::FocusType::kForward)
             ? FindFocusableElementRecursivelyForward(scope, owner_map)
             : FindFocusableElementRecursivelyBackward(scope, owner_map);
}

Element* FindFocusableElementDescendingDownIntoFrameDocument(
    mojom::blink::FocusType type,
    Element* element,
    FocusController::OwnerMap& owner_map) {
  // The element we found might be a HTMLFrameOwnerElement, so descend down the
  // tree until we find either:
  // 1) a focusable element, or
  // 2) the deepest-nested HTMLFrameOwnerElement.
  while (IsA<HTMLFrameOwnerElement>(element)) {
    HTMLFrameOwnerElement& owner = To<HTMLFrameOwnerElement>(*element);
    auto* container_local_frame = DynamicTo<LocalFrame>(owner.ContentFrame());
    if (!container_local_frame)
      break;
    container_local_frame->GetDocument()->UpdateStyleAndLayout(
        DocumentUpdateReason::kFocus);
    ScopedFocusNavigation scope =
        ScopedFocusNavigation::OwnedByIFrame(owner, owner_map);
    Element* found_element =
        FindFocusableElementRecursively(type, scope, owner_map);
    if (!found_element)
      break;
    DCHECK_NE(element, found_element);
    element = found_element;
  }
  return element;
}

namespace {
ScopedFocusNavigation GetScopeFor(Element*& owner,
                                  FocusController::OwnerMap& owner_map) {
  ScopedFocusNavigation new_scope =
      ScopedFocusNavigation::CreateFor(*owner, owner_map);
  while (new_scope.Owner() == owner) {
    // This can happen if a single element is both the root of a scope,
    // *and* the owner of a scope. E.g. <slot popover>. See
    // crbug.com/447888734.
    owner = owner->parentElement();
    if (!owner) {
      break;
    }
    new_scope = ScopedFocusNavigation::CreateFor(*owner, owner_map);
  }
  return new_scope;
}
}  // namespace

Element* FindFocusableElementAcrossFocusScopesForward(
    ScopedFocusNavigation& scope,
    FocusController::OwnerMap& owner_map) {
  const Element* current = scope.CurrentElement();
  Element* found = nullptr;
  if (current) {
    if (IsShadowHostWithoutCustomFocusLogic(*current)) {
      ScopedFocusNavigation inner_scope =
          ScopedFocusNavigation::OwnedByShadowHost(*current, owner_map);
      found = FindFocusableElementRecursivelyForward(inner_scope, owner_map);
    } else if (current->GetOpenPopoverTarget()) {
      ScopedFocusNavigation inner_scope =
          ScopedFocusNavigation::OwnedByPopoverInvoker(*current, owner_map);
      found = FindFocusableElementRecursivelyForward(inner_scope, owner_map);
    } else if (IsReadingFlowScopeOwner(current)) {
      ScopedFocusNavigation inner_scope =
          ScopedFocusNavigation::OwnedByReadingFlow(*current, owner_map);
      found = FindFocusableElementRecursivelyForward(inner_scope, owner_map);
    } else if (RuntimeEnabledFeatures::CSSScrollMarkerGroupModesEnabled()) {
      if (IsScrollMarkerFromScrollerInTabsMode(*current)) {
        ScopedFocusNavigation inner_scope =
            ScopedFocusNavigation::OwnedByScrollMarker(
                const_cast<Element&>(*current), owner_map);
        found = FindFocusableElementRecursivelyForward(inner_scope, owner_map);
      }
    }
  }
  if (!found)
    found = FindFocusableElementRecursivelyForward(scope, owner_map);

  // If there's no focusable element to advance to, move up the focus scopes
  // until we find one.
  ScopedFocusNavigation current_scope = scope;
  while (!found) {
    Element* owner = current_scope.Owner();
    if (!owner)
      break;
    current_scope = GetScopeFor(owner, owner_map);
    found = FindFocusableElementRecursivelyForward(current_scope, owner_map);
  }
  return FindFocusableElementDescendingDownIntoFrameDocument(
      mojom::blink::FocusType::kForward, found, owner_map);
}

Element* FindFocusableElementAcrossFocusScopesBackward(
    ScopedFocusNavigation& scope,
    FocusController::OwnerMap& owner_map) {
  Element* found = FindFocusableElementRecursivelyBackward(scope, owner_map);

  while (found && found->GetOpenPopoverTarget()) {
    ScopedFocusNavigation inner_scope =
        ScopedFocusNavigation::OwnedByPopoverInvoker(*found, owner_map);
    // If no inner element is focusable, then focus should be on the current
    // found popover invoker.
    if (Element* inner_found =
            FindFocusableElementRecursivelyBackward(inner_scope, owner_map)) {
      found = inner_found;
    } else {
      break;
    }
  }

  // If there's no focusable element to advance to, move up the focus scopes
  // until we find one.
  ScopedFocusNavigation current_scope = scope;
  while (!found) {
    Element* owner = current_scope.Owner();
    if (!owner)
      break;
    if ((IsKeyboardFocusableShadowHost(*owner) &&
         !owner->IsShadowHostWithDelegatesFocus()) ||
        owner->GetOpenPopoverTarget() ||
        IsKeyboardFocusableReadingFlowOwner(*owner)) {
      found = owner;
      break;
    }
    current_scope = GetScopeFor(owner, owner_map);
    found = FindFocusableElementRecursivelyBackward(current_scope, owner_map);
  }
  return FindFocusableElementDescendingDownIntoFrameDocument(
      mojom::blink::FocusType::kBackward, found, owner_map);
}

Element* FindFocusableElementAcrossFocusScopes(
    mojom::blink::FocusType type,
    ScopedFocusNavigation& scope,
    FocusController::OwnerMap& owner_map) {
  return (type == mojom::blink::FocusType::kForward)
             ? FindFocusableElementAcrossFocusScopesForward(scope, owner_map)
             : FindFocusableElementAcrossFocusScopesBackward(scope, owner_map);
}

}  // anonymous namespace

FocusController::FocusController(Page* page)
    : page_(page),
      is_active_(false),
      is_focused_(false),
      is_changing_focused_frame_(false),
      is_emulating_focus_(false) {}

// static
const ContainerNode* FocusController::ReadingFlowContainerOrDisplayContents(
    const ContainerNode* node,
    bool get_closest_ancestor) {
  if (!node) {
    return nullptr;
  }
  if (node->IsReadingFlowContainer()) {
    return node;
  }
  if (const Element* element = DynamicTo<Element>(node);
      element && (element->HasDisplayContentsStyle() || get_closest_ancestor)) {
    ContainerNode* closest_layout_parent =
        LayoutTreeBuilderTraversal::LayoutParent(*node);
    if (closest_layout_parent &&
        closest_layout_parent->IsReadingFlowContainer()) {
      return closest_layout_parent;
    }
  }
  return nullptr;
}

void FocusController::SetFocusedFrame(Frame* frame, bool notify_embedder) {
  DCHECK(!frame || frame->GetPage() == page_);
  if (focused_frame_ == frame || (is_changing_focused_frame_ && frame)) {
    return;
  }

  is_changing_focused_frame_ = true;

  // Fenced frames will try to pass focus to a dummy frame that represents the
  // inner frame tree. We instead want to give focus to the outer
  // HTMLFencedFrameElement. This will allow methods like document.activeElement
  // and document.hasFocus() to properly handle when a fenced frame has focus.
  if (frame && IsA<HTMLFrameOwnerElement>(frame->Owner())) {
    auto* fenced_frame = DynamicTo<HTMLFencedFrameElement>(
        To<HTMLFrameOwnerElement>(frame->Owner()));
    if (fenced_frame) {
      // SetFocusedElement will call back to FocusController::SetFocusedFrame.
      // However, `is_changing_focused_frame_` will be true when it is called,
      // causing the function to early return, so we still need the rest of this
      // invocation of the function to run.
      SetFocusedElement(fenced_frame, frame);
    }
  }

  auto* old_frame = DynamicTo<LocalFrame>(focused_frame_.Get());
  auto* new_frame = DynamicTo<LocalFrame>(frame);

  focused_frame_ = frame;

  // Now that the frame is updated, fire events and update the selection focused
  // states of both frames.
  if (old_frame && old_frame->View()) {
    old_frame->Selection().SetFrameIsFocused(false);
    old_frame->DomWindow()->DispatchEvent(
        *Event::Create(event_type_names::kBlur));
  }

  if (new_frame && new_frame->View() && IsFocused()) {
    new_frame->Selection().SetFrameIsFocused(true);
    new_frame->DomWindow()->DispatchEvent(
        *Event::Create(event_type_names::kFocus));
  }

  is_changing_focused_frame_ = false;

  // Checking IsAttached() is necessary, as the frame might have been detached
  // as part of dispatching the focus event above. See https://crbug.com/570874.
  if (notify_embedder && focused_frame_ && focused_frame_->IsAttached())
    focused_frame_->DidFocus();

  NotifyFocusChangedObservers();
}

void FocusController::FocusDocumentView(Frame* frame, bool notify_embedder) {
  DCHECK(!frame || frame->GetPage() == page_);
  if (focused_frame_ == frame)
    return;

  auto* focused_frame = DynamicTo<LocalFrame>(focused_frame_.Get());
  if (focused_frame && focused_frame->View()) {
    Document* document = focused_frame->GetDocument();
    Element* focused_element = document ? document->FocusedElement() : nullptr;
    if (focused_element)
      document->ClearFocusedElement();
  }

  auto* new_focused_frame = DynamicTo<LocalFrame>(frame);
  if (new_focused_frame && new_focused_frame->View()) {
    Document* document = new_focused_frame->GetDocument();
    Element* focused_element = document ? document->FocusedElement() : nullptr;
    if (focused_element)
      DispatchFocusEvent(*document, *focused_element);
  }

  // dispatchBlurEvent/dispatchFocusEvent could have changed the focused frame,
  // or detached the frame.
  if (new_focused_frame && !new_focused_frame->View())
    return;

  SetFocusedFrame(frame, notify_embedder);
}

LocalFrame* FocusController::FocusedFrame() const {
  // All callsites only care about *local* focused frames.
  return DynamicTo<LocalFrame>(focused_frame_.Get());
}

Frame* FocusController::FocusedOrMainFrame() const {
  if (LocalFrame* frame = FocusedFrame())
    return frame;

  // TODO(dcheng, alexmos): https://crbug.com/820786: This is a temporary hack
  // to ensure that we return a LocalFrame, even when the mainFrame is remote.
  // FocusController needs to be refactored to deal with RemoteFrames
  // cross-process focus transfers.
  for (Frame* frame = &page_->MainFrame()->Tree().Top(); frame;
       frame = frame->Tree().TraverseNext()) {
    auto* local_frame = DynamicTo<LocalFrame>(frame);
    if (local_frame)
      return frame;
  }

  return page_->MainFrame();
}

void FocusController::FrameDetached(Frame* detached_frame) {
  if (detached_frame == focused_frame_)
    SetFocusedFrame(nullptr);
}

HTMLFrameOwnerElement* FocusController::FocusedFrameOwnerElement(
    LocalFrame& current_frame) const {
  Frame* focused_frame = focused_frame_.Get();
  for (; focused_frame; focused_frame = focused_frame->Tree().Parent()) {
    if (focused_frame->Tree().Parent() == &current_frame) {
      DCHECK(focused_frame->Owner()->IsLocal());
      return focused_frame->DeprecatedLocalOwner();
    }
  }
  return nullptr;
}

bool FocusController::IsDocumentFocused(const Document& document) const {
  if (!IsActive()) {
    return false;
  }

  if (!focused_frame_) {
    return false;
  }

  if (IsA<HTMLFrameOwnerElement>(focused_frame_->Owner())) {
    auto* fenced_frame = DynamicTo<HTMLFencedFrameElement>(
        To<HTMLFrameOwnerElement>(focused_frame_->Owner()));
    if (fenced_frame && fenced_frame == document.ActiveElement()) {
      return fenced_frame->GetDocument().GetFrame()->Tree().IsDescendantOf(
          document.GetFrame());
    }
  }

  if (!IsFocused()) {
    return false;
  }

  return focused_frame_->Tree().IsDescendantOf(document.GetFrame());
}

void FocusController::FocusHasChanged() {
  bool focused = IsFocused();
  if (!focused) {
    if (auto* focused_or_main_local_frame =
            DynamicTo<LocalFrame>(FocusedOrMainFrame()))
      focused_or_main_local_frame->GetEventHandler().StopAutoscroll();
  }

  // Do not set a focused frame when being unfocused. This might reset
  // is_focused_ to true.
  if (!focused_frame_ && focused)
    SetFocusedFrame(page_->MainFrame());

  // SetFocusedFrame above might reject to update focused_frame_, or
  // focused_frame_ might be changed by blur/focus event handlers.
  auto* focused_local_frame = DynamicTo<LocalFrame>(focused_frame_.Get());
  if (focused_local_frame && focused_local_frame->View()) {
    focused_local_frame->Selection().SetFrameIsFocused(focused);
    DispatchEventsOnWindowAndFocusedElement(focused_local_frame->GetDocument(),
                                            focused);
  }

  NotifyFocusChangedObservers();
}

void FocusController::SetFocused(bool focused) {
  // If we are setting focus, we should be active.
  DCHECK(!focused || is_active_);
  if (is_focused_ == focused)
    return;
  is_focused_ = focused;
  if (!is_emulating_focus_)
    FocusHasChanged();

  // If the page has completely lost focus ensure we clear the focused
  // frame.
  if (!is_focused_ && page_->IsMainFrameFencedFrameRoot()) {
    SetFocusedFrame(nullptr);
  }
}

void FocusController::SetFocusEmulationEnabled(bool emulate_focus) {
  if (emulate_focus == is_emulating_focus_)
    return;
  bool active = IsActive();
  bool focused = IsFocused();
  is_emulating_focus_ = emulate_focus;

  if (!page_->MainFrame() || !page_->MainFrame()->IsLocalFrame()) {
    // If the page has no local main frame, no need to update focus, as the
    // focus emulation will trigger when the page navigated to a local main
    // frame (through `UpdateFocusOnNavigationCommit()`).
    return;
  }

  if (active != IsActive())
    ActiveHasChanged();
  if (focused != IsFocused())
    FocusHasChanged();
}

void FocusController::UpdateFocusOnNavigationCommit(Frame* frame,
                                                    bool was_focused) {
  if (was_focused) {
    SetFocusedFrame(frame);
    return;
  }
  if (is_emulating_focus_ && frame->IsOutermostMainFrame()) {
    SetFocusedFrame(frame);
  }
}

bool FocusController::SetInitialFocus(mojom::blink::FocusType type) {
  bool did_advance_focus = AdvanceFocus(type, true);

  // If focus is being set initially, accessibility needs to be informed that
  // system focus has moved into the web area again, even if focus did not
  // change within WebCore.  PostNotification is called instead of
  // handleFocusedUIElementChanged, because this will send the notification even
  // if the element is the same.
  if (auto* focused_or_main_local_frame =
          DynamicTo<LocalFrame>(FocusedOrMainFrame())) {
    Document* document = focused_or_main_local_frame->GetDocument();
    if (AXObjectCache* cache = document->ExistingAXObjectCache())
      cache->HandleInitialFocus();
  }

  return did_advance_focus;
}

bool FocusController::AdvanceFocus(
    mojom::blink::FocusType type,
    bool initial_focus,
    InputDeviceCapabilities* source_capabilities) {
  switch (type) {
    case mojom::blink::FocusType::kForward:
    case mojom::blink::FocusType::kBackward: {
      // We should never hit this when a RemoteFrame is focused, since the key
      // event that initiated focus advancement should've been routed to that
      // frame's process from the beginning.
      auto* starting_frame = To<LocalFrame>(FocusedOrMainFrame());
      return AdvanceFocusInDocumentOrder(starting_frame, nullptr, type,
                                         initial_focus, source_capabilities);
    }
    case mojom::blink::FocusType::kSpatialNavigation:
      // Fallthrough - SpatialNavigation should use
      // SpatialNavigationController.
    default:
      NOTREACHED();
  }
}

bool FocusController::AdvanceFocusAcrossFrames(
    mojom::blink::FocusType type,
    RemoteFrame* from,
    LocalFrame* to,
    InputDeviceCapabilities* source_capabilities) {
  Element* start = nullptr;

  // If we are shifting focus from a child frame to its parent, the
  // child frame has no more focusable elements, and we should continue
  // looking for focusable elements in the parent, starting from the element
  // of the child frame. This applies both to fencedframes and iframes.
  Element* start_candidate = DynamicTo<HTMLFrameOwnerElement>(from->Owner());
  if (start_candidate && start_candidate->GetDocument().GetFrame() == to) {
    start = start_candidate;
  }

  // If we're coming from a parent frame, we need to restart from the first or
  // last focusable element.
  bool initial_focus = to->Tree().Parent() == from;

  return AdvanceFocusInDocumentOrder(to, start, type, initial_focus,
                                     source_capabilities);
}

#if DCHECK_IS_ON()
inline bool IsNonFocusableShadowHost(const Element& element) {
  return IsShadowHostWithoutCustomFocusLogic(element) && !element.IsFocusable();
}
#endif

bool FocusController::AdvanceFocusInDocumentOrder(
    LocalFrame* frame,
    Element* start,
    mojom::blink::FocusType type,
    bool initial_focus,
    InputDeviceCapabilities* source_capabilities) {
  DCHECK(frame);
  Document* document = frame->GetDocument();
  OwnerMap owner_map;

  Element* current = start;
#if DCHECK_IS_ON()
  DCHECK(!current || !IsNonFocusableShadowHost(*current));
#endif
  if (!current && !initial_focus)
    current = document->SequentialFocusNavigationStartingPoint(type);

  document->UpdateStyleAndLayout(DocumentUpdateReason::kFocus);

  // Per https://drafts.csswg.org/css-overflow-5/#scroll-marker-next-focus
  // we want to start our search from scroll target of ::scroll-marker,
  // which is ultimate originating element for regular scroll marker
  // and TODO(378698659): the first element in ::column's view for column
  // scroll marker, but it's not clear yet what how to implement that.
  // So, `current` is just-activated scroll target of ::scroll-marker,
  // there is no expectation to be able to "go back" to ::scroll-marker.
  if (auto* scroll_marker =
          DynamicTo<ScrollMarkerPseudoElement>(document->FocusedElement());
      scroll_marker && scroll_marker->UltimateOriginatingElement() == current &&
      current->IsKeyboardFocusableSlow()) {
    SetFocusedFrame(document->GetFrame());
    current->Focus(FocusParams(SelectionBehaviorOnFocus::kReset, type,
                               source_capabilities, FocusOptions::Create(),
                               FocusTrigger::kUserGesture));
    return true;
  }

  ScopedFocusNavigation scope =
      (current && current->IsInTreeScope())
          ? ScopedFocusNavigation::CreateFor(*current, owner_map)
          : ScopedFocusNavigation::CreateForDocument(*document, owner_map);
  Element* element =
      FindFocusableElementAcrossFocusScopes(type, scope, owner_map);
  if (!element) {
    // If there's a RemoteFrame on the ancestor chain, we need to continue
    // searching for focusable elements there.
    if (frame->LocalFrameRoot() != frame->Tree().Top()) {
      document->ClearFocusedElement();
      document->SetSequentialFocusNavigationStartingPoint(nullptr);
      SetFocusedFrame(nullptr);
      To<RemoteFrame>(frame->LocalFrameRoot().Tree().Parent())
          ->AdvanceFocus(type, &frame->LocalFrameRoot());
      return true;
    }

    // We didn't find an element to focus, so we should try to pass focus to
    // Chrome.
    if ((!initial_focus || document->GetFrame()->IsFencedFrameRoot()) &&
        page_->GetChromeClient().CanTakeFocus(type)) {
      document->ClearFocusedElement();
      document->SetSequentialFocusNavigationStartingPoint(nullptr);
      SetFocusedFrame(nullptr);
      page_->GetChromeClient().TakeFocus(type);
      return true;
    }

    // Chrome doesn't want focus, so we should wrap focus.
    ScopedFocusNavigation doc_scope = ScopedFocusNavigation::CreateForDocument(
        *To<LocalFrame>(page_->MainFrame())->GetDocument(), owner_map);
    element = FindFocusableElementRecursively(type, doc_scope, owner_map);
    element = FindFocusableElementDescendingDownIntoFrameDocument(type, element,
                                                                  owner_map);

    if (!element) {
      return false;
    }
  }

  if (element == document->FocusedElement()) {
    // Focus is either coming from a remote frame or has wrapped around.
    if (FocusedFrame() != document->GetFrame()) {
      SetFocusedFrame(document->GetFrame());
      DispatchFocusEvent(*document, *element);
    }
    return true;
  }

  // Focus frames rather than frame owners.  Note that we should always attempt
  // to descend into frame owners with remote frames, since we don't know ahead
  // of time whether they contain focusable elements.  If a remote frame
  // doesn't contain any focusable elements, the search will eventually return
  // back to this frame and continue looking for focusable elements after the
  // frame owner.
  auto* owner = DynamicTo<HTMLFrameOwnerElement>(element);
  bool has_remote_frame =
      owner && owner->ContentFrame() && owner->ContentFrame()->IsRemoteFrame();
  if (owner && (has_remote_frame || !IsA<HTMLPlugInElement>(*element) ||
                !element->IsKeyboardFocusableSlow())) {
    // FIXME: We should not focus frames that have no scrollbars, as focusing
    // them isn't useful to the user.
    if (!owner->ContentFrame()) {
      return false;
    }

    document->ClearFocusedElement();

    // If ContentFrame is remote, continue the search for focusable elements in
    // that frame's process. The target ContentFrame's process will grab focus
    // from inside AdvanceFocusInDocumentOrder().
    //
    // ClearFocusedElement() fires events that might detach the contentFrame,
    // hence the need to null-check it again.
    if (auto* remote_frame = DynamicTo<RemoteFrame>(owner->ContentFrame()))
      remote_frame->AdvanceFocus(type, frame);
    else
      SetFocusedFrame(owner->ContentFrame());

    return true;
  }

  DCHECK(element->IsFocusable());

  // FIXME: It would be nice to just be able to call setFocusedElement(element)
  // here, but we can't do that because some elements (e.g. HTMLInputElement
  // and HTMLTextAreaElement) do extra work in their focus() methods.
  Document& new_document = element->GetDocument();

  if (&new_document != document) {
    // Focus is going away from this document, so clear the focused element.
    document->ClearFocusedElement();
    document->SetSequentialFocusNavigationStartingPoint(nullptr);
  }

  Frame* new_frame = new_document.GetFrame();
  SetFocusedFrame(new_frame);
  element->Focus(FocusParams(SelectionBehaviorOnFocus::kReset, type,
                             source_capabilities, FocusOptions::Create(),
                             FocusTrigger::kUserGesture));
  return true;
}

Element* FocusController::FindFocusableElementForImeAutofillAndTesting(
    mojom::blink::FocusType type,
    Element& element,
    OwnerMap& owner_map) {
  CHECK(type == mojom::blink::FocusType::kForward ||
        type == mojom::blink::FocusType::kBackward);
  ScopedFocusNavigation scope =
      ScopedFocusNavigation::CreateFor(element, owner_map);
  return FindFocusableElementAcrossFocusScopes(type, scope, owner_map);
}

Element* FocusController::NextFocusableElementForImeAndAutofill(
    Element* element,
    const mojom::blink::FocusType focus_type) {
  // TODO(ajith.v) Due to crbug.com/781026 when next/previous element is far
  // from current element in terms of tabindex, then it's signalling CPU load.
  // Will investigate further for a proper solution later.
  static const int kFocusTraversalThreshold = 50;
  element->GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kFocus);
  auto* html_element = DynamicTo<HTMLElement>(element);
  if (!html_element)
    return nullptr;

  auto* form_control_element = DynamicTo<HTMLFormControlElement>(element);
  if (!form_control_element && !html_element->isContentEditableForBinding())
    return nullptr;

  HTMLFormElement* form_owner = nullptr;
  if (html_element->isContentEditableForBinding())
    form_owner = Traversal<HTMLFormElement>::FirstAncestor(*element);
  else
    form_owner = form_control_element->formOwner();

  OwnerMap owner_map;
  Element* next_element = FindFocusableElementForImeAutofillAndTesting(
      focus_type, *element, owner_map);
  int traversal = 0;
  for (; next_element && traversal < kFocusTraversalThreshold;
       next_element = FindFocusableElementForImeAutofillAndTesting(
           focus_type, *next_element, owner_map),
       ++traversal) {
    auto* next_html_element = DynamicTo<HTMLElement>(next_element);
    if (!next_html_element)
      continue;
    if (next_html_element->isContentEditableForBinding()) {
      if (form_owner) {
        if (next_element->IsDescendantOf(form_owner)) {
          // |element| and |next_element| belongs to the same <form> element.
          return next_element;
        }
      } else {
        if (!Traversal<HTMLFormElement>::FirstAncestor(*next_html_element)) {
          // Neither this |element| nor the |next_element| has a form owner,
          // i.e. belong to the virtual <form>less form.
          return next_element;
        }
      }
    }
    // Captcha is a sort of an input field that should have user input as well.
    if (IsLikelyWithinCaptchaIframe(*next_html_element, owner_map)) {
      return next_element;
    }
    auto* next_form_control_element =
        DynamicTo<HTMLFormControlElement>(next_element);
    if (!next_form_control_element)
      continue;
    // If it is a submit button, then it is likely the end of the current form
    // (i.e. no next input field to be focused). This return is especially
    // important in a combined form where a single <form> element encloses
    // several user forms (e.g. signin + signup).
    if (next_form_control_element->CanBeSuccessfulSubmitButton()) {
      return nullptr;
    }
    if (next_form_control_element->formOwner() != form_owner ||
        next_form_control_element->IsDisabledOrReadOnly())
      continue;
    LayoutObject* layout = next_element->GetLayoutObject();
    if (layout && layout->IsTextControl()) {
      // TODO(crbug.com/1320441): Extend it for radio buttons and checkboxes.
      return next_element;
    }

    if (IsA<HTMLSelectElement>(next_form_control_element)) {
      return next_element;
    }
  }
  return nullptr;
}

// static
Element*
FocusController::FindScopeOwnerSlotOrScrollMarkerOrReadingFlowContainer(
    const Element& current) {
  Element* element = const_cast<Element*>(&current);
  // We should start from parent element of the ultimate originating element of
  // scroll marker, since the ultimate originating element is itself in scroll
  // marker's scope, so to find scroll marker's parent scope we start from the
  // parent of that element.
  if (auto* scroll_marker = DynamicTo<ScrollMarkerPseudoElement>(element)) {
    element = scroll_marker->UltimateOriginatingElement().parentElement();
  }
  if (element && element->IsPseudoElement()) {
    DCHECK(RuntimeEnabledFeatures::PseudoElementsFocusableEnabled());
    return nullptr;
  }
  while (element) {
    if (HTMLSlotElement* slot_element = element->AssignedSlot()) {
      return slot_element;
    }
    if (auto* scroll_marker =
            element->GetPseudoElement(kPseudoIdScrollMarker)) {
      if (IsScrollMarkerFromScrollerInTabsMode(*scroll_marker)) {
        return scroll_marker;
      }
    }
    element = element->parentElement();
    if (element && IsReadingFlowScopeOwner(element)) {
      return DynamicTo<HTMLElement>(element);
    }
  }
  return nullptr;
}

static bool RelinquishesEditingFocus(const Element& element) {
  DCHECK(IsEditable(element));
  return element.GetDocument().GetFrame() && RootEditableElement(element);
}

bool FocusController::SetFocusedElement(Element* element,
                                        Frame* new_focused_frame) {
  return SetFocusedElement(
      element, new_focused_frame,
      FocusParams(SelectionBehaviorOnFocus::kNone,
                  mojom::blink::FocusType::kNone, nullptr));
}

bool FocusController::SetFocusedElement(Element* element,
                                        Frame* new_focused_frame,
                                        const FocusParams& params) {
  LocalFrame* old_focused_frame = FocusedFrame();
  Document* old_document =
      old_focused_frame ? old_focused_frame->GetDocument() : nullptr;

  Element* old_focused_element =
      old_document ? old_document->FocusedElement() : nullptr;
  if (element && old_focused_element == element)
    return true;

  if (old_focused_element && IsRootEditableElement(*old_focused_element) &&
      !RelinquishesEditingFocus(*old_focused_element))
    return false;

  if (old_focused_frame)
    old_focused_frame->GetInputMethodController().WillChangeFocus();

  Document* new_document = nullptr;
  if (element)
    new_document = &element->GetDocument();
  else if (auto* new_focused_local_frame =
               DynamicTo<LocalFrame>(new_focused_frame))
    new_document = new_focused_local_frame->GetDocument();

  if (new_document && old_document == new_document &&
      new_document->FocusedElement() == element)
    return true;

  if (old_document && old_document != new_document)
    old_document->ClearFocusedElement();

  if (new_focused_frame && !new_focused_frame->GetPage()) {
    SetFocusedFrame(nullptr);
    return false;
  }

  SetFocusedFrame(new_focused_frame);

  if (new_document) {
    bool successfully_focused =
        new_document->SetFocusedElement(element, params);
    if (!successfully_focused)
      return false;
  }

  return true;
}

void FocusController::ActiveHasChanged() {
  Frame* frame = FocusedOrMainFrame();
  if (auto* local_frame = DynamicTo<LocalFrame>(frame)) {
    Document* const document = local_frame->LocalFrameRoot().GetDocument();
    DCHECK(document);
    if (!document->IsActive())
      return;
    // Invalidate all custom scrollbars because they support the CSS
    // window-active attribute. This should be applied to the entire page so
    // we invalidate from the root LocalFrameView instead of just the focused.
    if (LocalFrameView* view = document->View())
      view->InvalidateAllCustomScrollbarsOnActiveChanged();
    local_frame->Selection().PageActivationChanged();
  }
}

void FocusController::SetActive(bool active) {
  if (is_active_ == active)
    return;

  is_active_ = active;
  if (!is_emulating_focus_)
    ActiveHasChanged();
}

void FocusController::RegisterFocusChangedObserver(
    FocusChangedObserver* observer) {
  DCHECK(observer);
  DCHECK(!focus_changed_observers_.Contains(observer));
  focus_changed_observers_.insert(observer);
}

void FocusController::NotifyFocusChangedObservers() const {
  // Since this eventually dispatches an event to the page, the page could add
  // new observer, which would invalidate our iterators; so iterate over a copy
  // of the observer list.
  HeapHashSet<WeakMember<FocusChangedObserver>> observers =
      focus_changed_observers_;
  for (const auto& it : observers)
    it->FocusedFrameChanged();
}

// static
int FocusController::AdjustedTabIndex(const Element& element) {
  if (IsNonKeyboardFocusableShadowHost(element)) {
    return 0;
  }
  if (element.IsShadowHostWithDelegatesFocus() ||
      IsA<HTMLSlotElement>(element) || IsReadingFlowScopeOwner(&element)) {
    // We can't use Element::tabIndex(), which returns -1 for invalid or
    // missing values.
    return element.GetIntegralAttribute(html_names::kTabindexAttr, 0);
  }
  return element.GetIntegralAttribute(html_names::kTabindexAttr,
                                      element.IsFocusable() ? 0 : -1);
}

void FocusController::Trace(Visitor* visitor) const {
  visitor->Trace(page_);
  visitor->Trace(focused_frame_);
  visitor->Trace(focus_changed_observers_);
}

}  // namespace blink
