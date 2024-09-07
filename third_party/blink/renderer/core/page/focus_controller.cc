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

#include "third_party/blink/public/mojom/input/focus_type.mojom-blink.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
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
#include "third_party/blink/renderer/core/page/frame_tree.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/spatial_navigation.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {

const Element* MaybeAdjustSearchElementForFocusGroup(const Element& element,
                                                     bool get_last) {
  auto* scroll_marker = DynamicTo<ScrollMarkerPseudoElement>(element);
  if (!scroll_marker) {
    return &element;
  }
  CHECK(scroll_marker->ScrollMarkerGroup());
  const auto& scroll_markers =
      scroll_marker->ScrollMarkerGroup()->ScrollMarkers();
  return get_last ? scroll_markers.back() : scroll_markers.front();
}

// https://open-ui.org/components/focusgroup.explainer/#last-focused-memory
Element* MaybeRestoreFocusedElementForFocusGroup(Element* element) {
  if (!element) {
    return nullptr;
  }
  auto* scroll_marker = DynamicTo<ScrollMarkerPseudoElement>(element);
  if (!scroll_marker) {
    return element;
  }
  CHECK(scroll_marker->ScrollMarkerGroup());
  if (!scroll_marker->ScrollMarkerGroup()->Selected()) {
    return scroll_marker;
  }
  return scroll_marker->ScrollMarkerGroup()->Selected();
}

bool IsOpenPopoverWithInvoker(const Node* node) {
  auto* popover = DynamicTo<HTMLElement>(node);
  return popover && popover->HasPopoverAttribute() && popover->popoverOpen() &&
         popover->GetPopoverData()->invoker();
}

const Element* InclusiveAncestorOpenPopoverWithInvoker(const Element* element) {
  for (; element; element = FlatTreeTraversal::ParentElement(*element)) {
    if (IsOpenPopoverWithInvoker(element)) {
      return element;  // Return the popover
    }
  }
  return nullptr;
}

bool IsOpenPopoverInvoker(const Node* node) {
  auto* invoker = DynamicTo<HTMLFormControlElement>(node);
  if (!invoker)
    return false;
  HTMLElement* popover = const_cast<HTMLFormControlElement*>(invoker)
                             ->popoverTargetElement()
                             .popover;
  // There could be more than one invoker for a given popover. Only return true
  // if this invoker was the one that was actually used.
  return popover && popover->popoverOpen() &&
         popover->GetPopoverData()->invoker() == invoker;
}

// If node is a reading-flow container or a display: contents element whose
// layout parent is a reading-flow container, return that container.
// This is a helper for SetReadingFlowInfo.
const ContainerNode* ReadingFlowContainerOrDisplayContents(
    const ContainerNode* node) {
  if (!node) {
    return nullptr;
  }
  if (node->IsReadingFlowContainer()) {
    return node;
  }
  if (const Element* element = DynamicTo<Element>(node);
      element && element->HasDisplayContentsStyle()) {
    ContainerNode* closest_layout_parent =
        LayoutTreeBuilderTraversal::LayoutParent(*node);
    if (closest_layout_parent &&
        closest_layout_parent->IsReadingFlowContainer()) {
      return closest_layout_parent;
    }
  }
  return nullptr;
}

bool IsReadingFlowScopeOwner(const ContainerNode* node) {
  return ReadingFlowContainerOrDisplayContents(node);
}

// This class defines the navigation order.
class FocusNavigation : public GarbageCollected<FocusNavigation> {
 public:
  FocusNavigation(ContainerNode& root, FocusController::OwnerMap& owner_map)
      : root_(&root), owner_map_(owner_map) {
    Element* element = DynamicTo<Element>(root);
    if (ShadowRoot* shadow_root = DynamicTo<ShadowRoot>(root)) {
      // We need to check the shadow host when the root is a shadow root.
      element = &shadow_root->host();
    }
    if (auto* container = ReadingFlowContainerOrDisplayContents(element)) {
      SetReadingFlowInfo(*container);
    }
  }
  FocusNavigation(ContainerNode& root,
                  HTMLSlotElement& slot,
                  FocusController::OwnerMap& owner_map)
      : root_(&root), slot_(&slot), owner_map_(owner_map) {
    // Slot scope might have to follow reading flow if its closest layout
    // parent is a reading flow container.
    // TODO(crbug.com/336358906): Re-evaluate for content-visibility case.
    if (auto* container = ReadingFlowContainerOrDisplayContents(&slot)) {
      SetReadingFlowInfo(*container);
    }
  }

#if DCHECK_IS_ON()
  // Elements that have position absolute/fixed or display: contents will not
  // be sorted in reading-flow order. They should be visited at the end of
  // the reading flow elements, in DOM order.
  bool ShouldBeAtEndOfReadingFlow(const Element& element) {
    if (LayoutObject* layout = element.GetLayoutObject()) {
      return layout->IsFixedPositioned() || layout->IsAbsolutePositioned();
    }
    return element.HasDisplayContentsStyle();
  }
#endif

  void SetReadingFlowInfo(const ContainerNode& reading_flow_container) {
    DCHECK(reading_flow_container.GetLayoutBox());
    DCHECK(!reading_flow_container_);
    reading_flow_container_ = reading_flow_container;
    auto* children = MakeGarbageCollected<HeapVector<Member<Element>>>(
        reading_flow_container_->GetLayoutBox()->ReadingFlowElements());
    // Layout box only includes elements that are in the reading flow
    // container's layout. If a child is not in the sorted ReadingFlowElements,
    // we add them after in DOM order.
    for (Element& child : ElementTraversal::ChildrenOf(*root_)) {
      if (!children->Contains(child)) {
#if DCHECK_IS_ON()
        DCHECK(ShouldBeAtEndOfReadingFlow(child));
#endif
        children->push_back(child);
      }
    }
    reading_flow_next_elements_.ReserveCapacityForSize(children->size());
    reading_flow_previous_elements_.ReserveCapacityForSize(children->size());
    Element* prev_element = nullptr;
    for (Element* child : *children) {
      // Pseudo elements in reading-flow are not focusable and should not be
      // included in the elements to traverse.
      if (child->IsPseudoElement()) {
        continue;
      }
      if (!IsOwnedByRoot(*child)) {
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

  const Element* NextReadingFlowItem(const Element* next_in_dom_order) {
    if (reading_flow_next_elements_.empty()) {
      return nullptr;
    }
    // Our DOM walk landed on next_in_dom_order, which is a reading-flow
    // item or a nullptr. Because we need to instead walk in reading-flow,
    // we need to find the prior reading-flow item, and step forward from
    // there.
    Member<const Element> prior_reading_flow_item;
    for (Element& child : ElementTraversal::ChildrenOf(*root_)) {
      // If next_in_dom_order is nullptr, this condition is never met. The
      // prior_reading_flow_item will be the last reading-flow item visited
      // in dom order.
      if (&child == next_in_dom_order) {
        break;
      }
      if (reading_flow_next_elements_.Contains(&child)) {
        prior_reading_flow_item = child;
      }
    }
    // Now step forward in reading_flow_elements to find the correct next
    // reading-flow item.
    return reading_flow_next_elements_.at(prior_reading_flow_item);
  }

  const Element* NextInDomOrder(const Element& current) {
    Element* next;
    if (RuntimeEnabledFeatures::PseudoElementsFocusableEnabled()) {
      const Element* adjusted_current =
          MaybeAdjustSearchElementForFocusGroup(current, /*get_last=*/true);
      next = ElementTraversal::NextIncludingPseudo(*adjusted_current, root_);
      while (next && !IsOwnedByRoot(*next)) {
        next = ElementTraversal::NextIncludingPseudo(*next, root_);
      }
      next = MaybeRestoreFocusedElementForFocusGroup(next);
    } else {
      next = ElementTraversal::Next(current, root_);
      while (next && !IsOwnedByRoot(*next)) {
        next = ElementTraversal::Next(*next, root_);
      }
    }
    return next;
  }

  // Given current element, find next element to traverse:
  // 1. Find next in dom order that is within the scope of the root.
  // 2. If current scope is in a reading-flow container and the next in dom
  //    order element is either null or a fragment child of the root, use the
  //    reading flow instead.
  const Element* Next(const Element& current) {
    const Element* dom_next = NextInDomOrder(current);
    if (reading_flow_container_ &&
        (!dom_next || reading_flow_next_elements_.Contains(dom_next))) {
      return NextReadingFlowItem(dom_next);
    }
    return dom_next;
  }

  const Element* PreviousReadingFlowItem(
      const Element& current_reading_flow_item) {
    if (reading_flow_previous_elements_.empty()) {
      return nullptr;
    }
    const Element* previous_reading_flow_item =
        reading_flow_previous_elements_.at(&current_reading_flow_item);
    // If we are currently at the last reading flow item, return as there are
    // no more previous items to visit.
    if (!previous_reading_flow_item) {
      return nullptr;
    }
    // We visit all inclusive descendants of previous_reading_flow_item to find
    // the last root owned DOM child.
    for (const Element* child =
             ElementTraversal::LastWithinOrSelf(*previous_reading_flow_item);
         child; child = ElementTraversal::Previous(
                    *child, previous_reading_flow_item)) {
      if (IsOwnedByRoot(const_cast<Element&>(*child))) {
        return child;
      }
    }
    // If no previous child owned by root is found, return null. This shouldn't
    // happen because all items in reading_flow_previous_elements_ are owned
    // by root.
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }

  const Element* PreviousInDomOrder(const Element& current) {
    Element* previous;
    if (RuntimeEnabledFeatures::PseudoElementsFocusableEnabled()) {
      const Element* adjusted_current =
          MaybeAdjustSearchElementForFocusGroup(current, /*get_last=*/false);
      previous =
          ElementTraversal::PreviousIncludingPseudo(*adjusted_current, root_);
      if (previous == root_) {
        return nullptr;
      }
      while (previous && !IsOwnedByRoot(*previous)) {
        previous = ElementTraversal::PreviousIncludingPseudo(*previous, root_);
      }
      previous = MaybeRestoreFocusedElementForFocusGroup(previous);
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

  // Given current element, find next element to traverse:
  // 1. If current scope is in a reading-flow container and the current element
  //    is a fragment child of the root, use the reading flow.
  // 2. Else, use the DOM tree order.
  const Element* Previous(const Element& current) {
    return reading_flow_container_ &&
                   reading_flow_previous_elements_.Contains(&current)
               ? PreviousReadingFlowItem(current)
               : PreviousInDomOrder(current);
  }

  const Element* First() {
    if (reading_flow_first_element_) {
      return reading_flow_first_element_;
    }
    Element* first = ElementTraversal::FirstChild(*root_);
    while (first && !IsOwnedByRoot(*first))
      first = ElementTraversal::Next(*first, root_);
    return first;
  }

  const Element* Last() {
    const Element* last;
    if (reading_flow_last_element_) {
      last = ElementTraversal::LastWithinOrSelf(*reading_flow_last_element_);
    } else {
      last = ElementTraversal::LastWithin(*root_);
    }
    while (last && !IsOwnedByRoot(const_cast<Element&>(*last))) {
      last = ElementTraversal::Previous(*last, root_);
    }
    return last;
  }

  Element* Owner() {
    if (slot_) {
      return slot_.Get();
    }
    if (IsReadingFlowScopeOwner(root_)) {
      return DynamicTo<Element>(*root_);
    }
    return FindOwner(*root_);
  }

  void Trace(Visitor* visitor) const {
    visitor->Trace(root_);
    visitor->Trace(slot_);
    visitor->Trace(reading_flow_container_);
    visitor->Trace(reading_flow_first_element_);
    visitor->Trace(reading_flow_last_element_);
    visitor->Trace(reading_flow_next_elements_);
    visitor->Trace(reading_flow_previous_elements_);
  }

 private:
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
  // - If node is in a reading-flow container, owner is that container (found
  //   by traversing ancestors).
  // - If node is in slot fallback content scope, owner is the parent or
  //   shadowHost element.
  // - If node is in shadow tree scope, owner is the parent or shadowHost
  //   element.
  // - If node is in frame scope, owner is the iframe node.
  // - If node is inside an open popover with an invoker, owner is the invoker.
  Element* FindOwner(ContainerNode& node) {
    auto result = owner_map_.find(&node);
    if (result != owner_map_.end())
      return result->value.Get();

    // Fallback contents owner is set to the nearest ancestor slot node even if
    // the slot node have assigned nodes.

    Element* owner = nullptr;
    Element* owner_slot_or_reading_flow_container = nullptr;
    if (Element* element = DynamicTo<Element>(node)) {
      owner_slot_or_reading_flow_container =
          FocusController::FindScopeOwnerSlotOrReadingFlowContainer(*element);
    }
    if (owner_slot_or_reading_flow_container) {
      owner = owner_slot_or_reading_flow_container;
    } else if (IsA<HTMLSlotElement>(node.parentNode())) {
      owner = node.ParentOrShadowHostElement();
    } else if (&node == node.GetTreeScope().RootNode()) {
      owner = TreeOwner(&node);
    } else if (IsOpenPopoverWithInvoker(&node)) {
      owner = DynamicTo<HTMLElement>(node)->GetPopoverData()->invoker();
    } else if (node.parentNode()) {
      owner = FindOwner(*node.parentNode());
    }

    owner_map_.insert(&node, owner);
    return owner;
  }

  bool IsOwnedByRoot(ContainerNode& node) { return FindOwner(node) == Owner(); }

  Member<ContainerNode> root_;
  Member<HTMLSlotElement> slot_;
  FocusController::OwnerMap& owner_map_;
  // This member is the reading-flow container if it is exists.
  Member<const ContainerNode> reading_flow_container_;
  // These members are the first and last reading flow elements in
  // the reading flow container if it has children.
  Member<Element> reading_flow_first_element_;
  Member<Element> reading_flow_last_element_;
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
  Element* Owner() const;

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
  Element* NextFocusableElement();
  Element* PreviousFocusableElement();

  void SetCurrentElement(const Element* element) { current_ = element; }
  void MoveToNext();
  void MoveToPrevious();
  void MoveToFirst();
  void MoveToLast();

  const Element* current_;
  FocusNavigation* navigation_;
};

ScopedFocusNavigation::ScopedFocusNavigation(
    ContainerNode& scoping_root_node,
    const Element* current,
    FocusController::OwnerMap& owner_map)
    : current_(current) {
  if (auto* slot = DynamicTo<HTMLSlotElement>(scoping_root_node)) {
    if (slot->AssignedNodes().empty()) {
      navigation_ = MakeGarbageCollected<FocusNavigation>(scoping_root_node,
                                                          *slot, owner_map);
    } else {
      // Here, slot->AssignedNodes() are non null, so the slot must be inside
      // the shadow tree.
      DCHECK(scoping_root_node.ContainingShadowRoot());
      navigation_ = MakeGarbageCollected<FocusNavigation>(
          scoping_root_node.ContainingShadowRoot()->host(), *slot, owner_map);
    }
  } else {
    navigation_ =
        MakeGarbageCollected<FocusNavigation>(scoping_root_node, owner_map);
  }
  DCHECK(navigation_);
}

void ScopedFocusNavigation::MoveToNext() {
  DCHECK(CurrentElement());
  SetCurrentElement(navigation_->Next(*CurrentElement()));
}

void ScopedFocusNavigation::MoveToPrevious() {
  DCHECK(CurrentElement());
  SetCurrentElement(navigation_->Previous(*CurrentElement()));
}

void ScopedFocusNavigation::MoveToFirst() {
  SetCurrentElement(navigation_->First());
}

void ScopedFocusNavigation::MoveToLast() {
  SetCurrentElement(navigation_->Last());
}

Element* ScopedFocusNavigation::Owner() const {
  Element* owner = navigation_->Owner();
  // TODO(crbug.com/335909581): If the returned owner is a reading-flow
  // container and a popover, we want the scope owner to be the invoker.
  if (IsOpenPopoverWithInvoker(owner) && owner->IsReadingFlowContainer()) {
    return DynamicTo<HTMLElement>(owner)->GetPopoverData()->invoker();
  }
  return owner;
}

ScopedFocusNavigation ScopedFocusNavigation::CreateFor(
    const Element& current,
    FocusController::OwnerMap& owner_map) {
  if (HTMLElement* owner =
          FocusController::FindScopeOwnerSlotOrReadingFlowContainer(current)) {
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
  DCHECK(IsA<HTMLFormControlElement>(invoker));
  HTMLElement* popover =
      DynamicTo<HTMLFormControlElement>(const_cast<Element&>(invoker))
          ->popoverTargetElement()
          .popover;
  DCHECK(IsOpenPopoverWithInvoker(popover));
  return ScopedFocusNavigation(*popover, nullptr, owner_map);
}

ScopedFocusNavigation ScopedFocusNavigation::OwnedByReadingFlow(
    const Element& owner,
    FocusController::OwnerMap& owner_map) {
  DCHECK(IsReadingFlowScopeOwner(&owner));
  HTMLElement& element = const_cast<HTMLElement&>(To<HTMLElement>(owner));
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
    focused_element->SetHasFocusWithinUpToAncestor(false, nullptr);
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
    focused_element->SetHasFocusWithinUpToAncestor(true, nullptr);
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
  if (element.IsKeyboardFocusable()) {
    return false;
  }
  // This host supports focus, but cannot be keyboard focused. For example:
  // - Tabindex is negative
  // - It is a scroller with focusable children
  // When tabindex is negative, we should not visit the host.
  return !(element.GetIntegralAttribute(html_names::kTabindexAttr, 0) < 0);
}

inline bool IsNonKeyboardFocusableReadingFlowOwner(const Element& element) {
  return IsReadingFlowScopeOwner(&element) && !element.IsKeyboardFocusable();
}

inline bool IsKeyboardFocusableReadingFlowOwner(const Element& element) {
  return IsReadingFlowScopeOwner(&element) && element.IsKeyboardFocusable();
}

inline bool IsKeyboardFocusableShadowHost(const Element& element) {
  return IsShadowHostWithoutCustomFocusLogic(element) &&
         (element.IsKeyboardFocusable() ||
          element.IsShadowHostWithDelegatesFocus());
}

inline bool IsNonFocusableFocusScopeOwner(Element& element) {
  return IsNonKeyboardFocusableShadowHost(element) ||
         IsA<HTMLSlotElement>(element) ||
         IsNonKeyboardFocusableReadingFlowOwner(element);
}

inline bool ShouldVisit(Element& element) {
  DCHECK(!element.IsKeyboardFocusable() ||
         FocusController::AdjustedTabIndex(element) >= 0)
      << "Keyboard focusable element with negative tabindex" << element;
  return element.IsKeyboardFocusable() ||
         element.IsShadowHostWithDelegatesFocus() ||
         IsNonFocusableFocusScopeOwner(element);
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
        FocusController::AdjustedTabIndex(*current) == tab_index) {
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
    int current_tab_index = FocusController::AdjustedTabIndex(*current);
    if (ShouldVisit(*current) && current_tab_index > tab_index) {
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
    int current_tab_index = FocusController::AdjustedTabIndex(*current);
    if (ShouldVisit(*current) && current_tab_index < tab_index &&
        current_tab_index > winning_tab_index) {
      winner = current;
      winning_tab_index = current_tab_index;
    }
  }
  SetCurrentElement(winner);
  return winner;
}

Element* ScopedFocusNavigation::NextFocusableElement() {
  Element* current = CurrentElement();
  if (current) {
    int tab_index = FocusController::AdjustedTabIndex(*current);
    // If an element is excluded from the normal tabbing cycle, the next
    // focusable element is determined by tree order.
    if (tab_index < 0) {
      for (MoveToNext(); CurrentElement(); MoveToNext()) {
        current = CurrentElement();
        if (ShouldVisit(*current) &&
            FocusController::AdjustedTabIndex(*current) >= 0) {
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
          current ? FocusController::AdjustedTabIndex(*current) : 0)) {
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
    tab_index = FocusController::AdjustedTabIndex(*current);
  } else {
    MoveToLast();
    tab_index = 0;
  }

  // However, if an element is excluded from the normal tabbing cycle, the
  // previous focusable element is determined by tree order
  if (tab_index < 0) {
    for (; CurrentElement(); MoveToPrevious()) {
      current = CurrentElement();
      if (ShouldVisit(*current) &&
          FocusController::AdjustedTabIndex(*current) >= 0) {
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

Element* FindFocusableElementAcrossFocusScopesForward(
    ScopedFocusNavigation& scope,
    FocusController::OwnerMap& owner_map) {
  const Element* current = scope.CurrentElement();
  Element* found = nullptr;
  if (current && IsShadowHostWithoutCustomFocusLogic(*current)) {
    ScopedFocusNavigation inner_scope =
        ScopedFocusNavigation::OwnedByShadowHost(*current, owner_map);
    found = FindFocusableElementRecursivelyForward(inner_scope, owner_map);
  } else if (IsOpenPopoverInvoker(current)) {
    ScopedFocusNavigation inner_scope =
        ScopedFocusNavigation::OwnedByPopoverInvoker(*current, owner_map);
    found = FindFocusableElementRecursivelyForward(inner_scope, owner_map);
  } else if (current && IsReadingFlowScopeOwner(current)) {
    ScopedFocusNavigation inner_scope =
        ScopedFocusNavigation::OwnedByReadingFlow(*current, owner_map);
    found = FindFocusableElementRecursivelyForward(inner_scope, owner_map);
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
    current_scope = ScopedFocusNavigation::CreateFor(*owner, owner_map);
    found = FindFocusableElementRecursivelyForward(current_scope, owner_map);
  }
  return FindFocusableElementDescendingDownIntoFrameDocument(
      mojom::blink::FocusType::kForward, found, owner_map);
}

Element* FindFocusableElementAcrossFocusScopesBackward(
    ScopedFocusNavigation& scope,
    FocusController::OwnerMap& owner_map) {
  Element* found = FindFocusableElementRecursivelyBackward(scope, owner_map);

  while (IsOpenPopoverInvoker(found)) {
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
    current_scope = ScopedFocusNavigation::CreateFor(*owner, owner_map);
    if ((IsKeyboardFocusableShadowHost(*owner) &&
         !owner->IsShadowHostWithDelegatesFocus()) ||
        IsOpenPopoverInvoker(owner) ||
        IsKeyboardFocusableReadingFlowOwner(*owner)) {
      found = owner;
      break;
    }
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

void FocusController::SetFocusedFrame(Frame* frame, bool notify_embedder) {
  DCHECK(!frame || frame->GetPage() == page_);
  if (focused_frame_ == frame || (is_changing_focused_frame_ && frame))
    return;

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
  if (active != IsActive())
    ActiveHasChanged();
  if (focused != IsFocused())
    FocusHasChanged();
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
  // TODO (liviutinta) remove TRACE after fixing crbug.com/1063548
  TRACE_EVENT0("input", "FocusController::AdvanceFocus");
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
      NOTREACHED_IN_MIGRATION();
  }

  return false;
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
  // TODO (liviutinta) remove TRACE after fixing crbug.com/1063548
  TRACE_EVENT0("input", "FocusController::AdvanceFocusInDocumentOrder");
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
      // TODO (liviutinta) remove TRACE after fixing crbug.com/1063548
      TRACE_EVENT_INSTANT1(
          "input", "FocusController::AdvanceFocusInDocumentOrder",
          TRACE_EVENT_SCOPE_THREAD, "reason_for_no_focus_element",
          "no_recursive_focusable_element");
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
                !element->IsKeyboardFocusable())) {
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

  SetFocusedFrame(new_document.GetFrame());

  element->Focus(FocusParams(SelectionBehaviorOnFocus::kReset, type,
                             source_capabilities, FocusOptions::Create(),
                             FocusTrigger::kUserGesture));
  return true;
}

Element* FocusController::FindFocusableElement(mojom::blink::FocusType type,
                                               Element& element,
                                               OwnerMap& owner_map) {
  // FIXME: No spacial navigation code yet.
  DCHECK(type == mojom::blink::FocusType::kForward ||
         type == mojom::blink::FocusType::kBackward);
  ScopedFocusNavigation scope =
      ScopedFocusNavigation::CreateFor(element, owner_map);
  return FindFocusableElementAcrossFocusScopes(type, scope, owner_map);
}

Element* FocusController::NextFocusableElementForImeAndAutofill(
    Element* element,
    mojom::blink::FocusType focus_type) {
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
  Element* next_element = FindFocusableElement(focus_type, *element, owner_map);
  int traversal = 0;
  for (; next_element && traversal < kFocusTraversalThreshold;
       next_element =
           FindFocusableElement(focus_type, *next_element, owner_map),
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

// This is an implementation of step 2 of the "shadow host" branch of
// https://html.spec.whatwg.org/C/#get-the-focusable-area
Element* FocusController::FindFocusableElementInShadowHost(
    const Element& shadow_host) {
  CHECK(!RuntimeEnabledFeatures::NewGetFocusableAreaBehaviorEnabled());
  // We have no behavior difference by focus trigger. Skip step 2.1.

  // 2.2. Otherwise, let possible focus delegates be the list of all
  //   focusable areas whose DOM anchor is a descendant of focus target
  //   in the flat tree.
  // 2.3. Return the first focusable area in tree order of their DOM
  //   anchors in possible focus delegates, or null if possible focus
  //   delegates is empty.
  Node* current = const_cast<Element*>(&shadow_host);
  while ((current = FlatTreeTraversal::Next(*current, &shadow_host))) {
    if (auto* current_element = DynamicTo<Element>(current)) {
      if (current_element->IsFocusable())
        return current_element;
    }
  }
  return nullptr;
}

// static
HTMLElement* FocusController::FindScopeOwnerSlotOrReadingFlowContainer(
    const Element& current) {
  Element* element = const_cast<Element*>(&current);
  if (element->IsPseudoElement()) {
    DCHECK(RuntimeEnabledFeatures::PseudoElementsFocusableEnabled());
    return nullptr;
  }
  while (element) {
    if (HTMLSlotElement* slot_element = element->AssignedSlot()) {
      return slot_element;
    }
    element = element->parentElement();
    if (element && IsReadingFlowScopeOwner(element)) {
      return DynamicTo<HTMLElement>(element);
    }
  }
  return nullptr;
}

Element* FocusController::FindFocusableElementAfter(
    Element& element,
    mojom::blink::FocusType type) {
  if (type != mojom::blink::FocusType::kForward &&
      type != mojom::blink::FocusType::kBackward)
    return nullptr;
  element.GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kFocus);

  OwnerMap owner_map;
  return FindFocusableElement(type, element, owner_map);
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
