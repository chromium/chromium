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

#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/range.h"
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
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/core/html/html_plugin_element.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/focus_changed_observer.h"
#include "third_party/blink/renderer/core/page/frame_tree.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/slot_scoped_traversal.h"
#include "third_party/blink/renderer/core/page/spatial_navigation.h"

namespace blink {

namespace {

// This class defines the navigation order.
class FocusNavigation : public GarbageCollected<FocusNavigation> {
 public:
  FocusNavigation(ContainerNode& root, FocusController::OwnerMap& owner_map)
      : root_(&root), owner_map_(owner_map) {}
  FocusNavigation(ContainerNode& root,
                  HTMLSlotElement& slot,
                  FocusController::OwnerMap& owner_map)
      : root_(&root), slot_(&slot), owner_map_(owner_map) {}

  const Element* Next(const Element& current) {
    Element* next = ElementTraversal::Next(current, root_);
    while (next && !IsOwnedByRoot(*next))
      next = ElementTraversal::Next(*next, root_);
    return next;
  }

  const Element* Previous(const Element& current) {
    Element* previous = ElementTraversal::Previous(current, root_);
    if (previous == root_)
      return nullptr;
    while (previous && !IsOwnedByRoot(*previous))
      previous = ElementTraversal::Previous(*previous, root_);
    return previous;
  }

  const Element* First() {
    Element* first = ElementTraversal::FirstChild(*root_);
    while (first && !IsOwnedByRoot(*first))
      first = ElementTraversal::Next(*first, root_);
    return first;
  }

  const Element* Last() {
    Element* last = ElementTraversal::LastWithin(*root_);
    while (last && !IsOwnedByRoot(*last))
      last = ElementTraversal::Previous(*last, root_);
    return last;
  }

  Element* Owner() {
    if (slot_)
      return slot_;

    return FindOwner(*root_);
  }

  void Trace(blink::Visitor* visitor) {
    visitor->Trace(root_);
    visitor->Trace(slot_);
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

  // Owner is the slot node for slot scope and slot fallback contents scope, the
  // shadow host for shadow tree scope and the iframe node for frame scope.
  Element* FindOwner(ContainerNode& node) {
    auto result = owner_map_.find(&node);
    if (result != owner_map_.end())
      return result->value;

    // Fallback contents owner is set to the nearest ancestor slot node even if
    // the slot node have assigned nodes.

    Element* owner = nullptr;
    if (node.AssignedSlot())
      owner = node.AssignedSlot();
    else if (IsA<HTMLSlotElement>(node.parentNode()))
      owner = node.ParentOrShadowHostElement();
    else if (&node == node.ContainingTreeScope().RootNode())
      owner = TreeOwner(&node);
    else if (node.parentNode())
      owner = FindOwner(*node.parentNode());

    owner_map_.insert(&node, owner);
    return owner;
  }

  bool IsOwnedByRoot(ContainerNode& node) { return FindOwner(node) == Owner(); }

  Member<ContainerNode> root_;
  Member<HTMLSlotElement> slot_;
  FocusController::OwnerMap& owner_map_;
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
  Element* FindFocusableElement(WebFocusType type) {
    return (type == kWebFocusTypeForward) ? NextFocusableElement()
                                          : PreviousFocusableElement();
  }

  Element* CurrentElement() const {
    return const_cast<Element*>(current_.Get());
  }
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
  static HTMLSlotElement* FindFallbackScopeOwnerSlot(const Element&);
  static bool IsSlotFallbackScoped(const Element&);
  static bool IsSlotFallbackScopedForThisSlot(const HTMLSlotElement&,
                                              const Element&);

 private:
  ScopedFocusNavigation(ContainerNode& scoping_root_node,
                        const Element* current,
                        FocusController::OwnerMap&);

  Element* FindElementWithExactTabIndex(int tab_index, WebFocusType);
  Element* NextElementWithGreaterTabIndex(int tab_index);
  Element* PreviousElementWithLowerTabIndex(int tab_index);
  Element* NextFocusableElement();
  Element* PreviousFocusableElement();

  void SetCurrentElement(const Element* element) { current_ = element; }
  void MoveToNext();
  void MoveToPrevious();
  void MoveToFirst();
  void MoveToLast();

  Member<const Element> current_;
  Member<FocusNavigation> navigation_;
};

ScopedFocusNavigation::ScopedFocusNavigation(
    ContainerNode& scoping_root_node,
    const Element* current,
    FocusController::OwnerMap& owner_map)
    : current_(current) {
  if (auto* slot = DynamicTo<HTMLSlotElement>(scoping_root_node)) {
    if (slot->AssignedNodes().IsEmpty()) {
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
  return navigation_->Owner();
}

ScopedFocusNavigation ScopedFocusNavigation::CreateFor(
    const Element& current,
    FocusController::OwnerMap& owner_map) {
  if (HTMLSlotElement* slot = SlotScopedTraversal::FindScopeOwnerSlot(current))
    return ScopedFocusNavigation(*slot, &current, owner_map);
  if (HTMLSlotElement* slot =
          ScopedFocusNavigation::FindFallbackScopeOwnerSlot(current))
    return ScopedFocusNavigation(*slot, &current, owner_map);
  return ScopedFocusNavigation(current.ContainingTreeScope().RootNode(),
                               &current, owner_map);
}

ScopedFocusNavigation ScopedFocusNavigation::CreateForDocument(
    Document& document,
    FocusController::OwnerMap& owner_map) {
  return ScopedFocusNavigation(document, nullptr, owner_map);
}

ScopedFocusNavigation ScopedFocusNavigation::OwnedByNonFocusableFocusScopeOwner(
    Element& element,
    FocusController::OwnerMap& owner_map) {
  if (IsShadowHost(element))
    return ScopedFocusNavigation::OwnedByShadowHost(element, owner_map);
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
  To<LocalFrame>(frame.ContentFrame())
      ->GetDocument()
      ->UpdateDistributionForLegacyDistributedNodes();
  return ScopedFocusNavigation(
      *To<LocalFrame>(frame.ContentFrame())->GetDocument(), nullptr, owner_map);
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
      return slot->AssignedNodes().IsEmpty() ? slot : nullptr;
    parent = parent->parentElement();
  }
  return nullptr;
}

bool ScopedFocusNavigation::IsSlotFallbackScoped(const Element& element) {
  return ScopedFocusNavigation::FindFallbackScopeOwnerSlot(element);
}

bool ScopedFocusNavigation::IsSlotFallbackScopedForThisSlot(
    const HTMLSlotElement& slot,
    const Element& current) {
  Element* parent = current.parentElement();
  while (parent) {
    auto* html_slot_element = DynamicTo<HTMLSlotElement>(parent);
    if (html_slot_element && html_slot_element->AssignedNodes().IsEmpty()) {
      return !SlotScopedTraversal::IsSlotScoped(current) &&
             html_slot_element == slot;
    }
    parent = parent->parentElement();
  }
  return false;
}

inline void DispatchBlurEvent(const Document& document,
                              Element& focused_element) {
  focused_element.DispatchBlurEvent(nullptr, kWebFocusTypePage);
  if (focused_element == document.FocusedElement()) {
    focused_element.DispatchFocusOutEvent(event_type_names::kFocusout, nullptr);
    if (focused_element == document.FocusedElement())
      focused_element.DispatchFocusOutEvent(event_type_names::kDOMFocusOut,
                                            nullptr);
  }
}

inline void DispatchFocusEvent(const Document& document,
                               Element& focused_element) {
  focused_element.DispatchFocusEvent(nullptr, kWebFocusTypePage);
  if (focused_element == document.FocusedElement()) {
    focused_element.DispatchFocusInEvent(event_type_names::kFocusin, nullptr,
                                         kWebFocusTypePage);
    if (focused_element == document.FocusedElement()) {
      focused_element.DispatchFocusInEvent(event_type_names::kDOMFocusIn,
                                           nullptr, kWebFocusTypePage);
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
    // Use focus_type kWebFocusTypePage, same as used in DispatchBlurEvent.
    focused_element->SetFocused(false, kWebFocusTypePage);
    focused_element->SetHasFocusWithinUpToAncestor(false, nullptr);
    DispatchBlurEvent(*document, *focused_element);
  }

  if (LocalDOMWindow* window = document->domWindow()) {
    window->DispatchEvent(*Event::Create(focused ? event_type_names::kFocus
                                                 : event_type_names::kBlur));
  }
  if (focused && document->FocusedElement()) {
    Element* focused_element(document->FocusedElement());
    // Use focus_type kWebFocusTypePage, same as used in DispatchFocusEvent.
    focused_element->SetFocused(true, kWebFocusTypePage);
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
  return IsShadowHostWithoutCustomFocusLogic(element) &&
         !(element.ShadowRootIfV1()
               ? (element.IsFocusable() || element.DelegatesFocus())
               : element.IsKeyboardFocusable());
}

inline bool IsKeyboardFocusableShadowHost(const Element& element) {
  return IsShadowHostWithoutCustomFocusLogic(element) &&
         (element.IsKeyboardFocusable() || element.DelegatesFocus());
}

inline bool IsNonFocusableFocusScopeOwner(Element& element) {
  return IsNonKeyboardFocusableShadowHost(element) ||
         IsA<HTMLSlotElement>(element);
}

inline int AdjustedTabIndex(Element& element) {
  if (IsNonKeyboardFocusableShadowHost(element))
    return 0;
  if (element.DelegatesFocus() || IsA<HTMLSlotElement>(element)) {
    // We can't use Element::tabIndex(), which returns -1 for invalid or
    // missing values.
    return element.GetIntegralAttribute(html_names::kTabindexAttr, 0);
  }
  bool default_focusable =
      element.SupportsFocus() ||
      (RuntimeEnabledFeatures::KeyboardFocusableScrollersEnabled() &&
       IsScrollableNode(&element));
  return element.GetIntegralAttribute(html_names::kTabindexAttr,
                                      default_focusable ? 0 : -1);
}

inline bool ShouldVisit(Element& element) {
  return element.IsKeyboardFocusable() || element.DelegatesFocus() ||
         IsNonFocusableFocusScopeOwner(element);
}

Element* ScopedFocusNavigation::FindElementWithExactTabIndex(
    int tab_index,
    WebFocusType type) {
  // Search is inclusive of start
  for (; CurrentElement();
       type == kWebFocusTypeForward ? MoveToNext() : MoveToPrevious()) {
    Element* current = CurrentElement();
    if (ShouldVisit(*current) && AdjustedTabIndex(*current) == tab_index)
      return current;
  }
  return nullptr;
}

Element* ScopedFocusNavigation::NextElementWithGreaterTabIndex(int tab_index) {
  // Search is inclusive of start
  int winning_tab_index = std::numeric_limits<int>::max();
  Element* winner = nullptr;
  for (; CurrentElement(); MoveToNext()) {
    Element* current = CurrentElement();
    int current_tab_index = AdjustedTabIndex(*current);
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
    int current_tab_index = AdjustedTabIndex(*current);
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
    int tab_index = AdjustedTabIndex(*current);
    // If an element is excluded from the normal tabbing cycle, the next
    // focusable element is determined by tree order.
    if (tab_index < 0) {
      for (MoveToNext(); CurrentElement(); MoveToNext()) {
        current = CurrentElement();
        if (ShouldVisit(*current) && AdjustedTabIndex(*current) >= 0)
          return current;
      }
    } else {
      // First try to find an element with the same tabindex as start that comes
      // after start in the scope.
      MoveToNext();
      if (Element* winner =
              FindElementWithExactTabIndex(tab_index, kWebFocusTypeForward))
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
          current ? AdjustedTabIndex(*current) : 0)) {
    return winner;
  }

  // There are no elements with a tabindex greater than start's tabindex,
  // so find the first element with a tabindex of 0.
  MoveToFirst();
  return FindElementWithExactTabIndex(0, kWebFocusTypeForward);
}

Element* ScopedFocusNavigation::PreviousFocusableElement() {
  // First try to find the last element in the scope that comes before start and
  // has the same tabindex as start.  If start is null, find the last element in
  // the scope with a tabindex of 0.
  int tab_index;
  Element* current = CurrentElement();
  if (current) {
    MoveToPrevious();
    tab_index = AdjustedTabIndex(*current);
  } else {
    MoveToLast();
    tab_index = 0;
  }

  // However, if an element is excluded from the normal tabbing cycle, the
  // previous focusable element is determined by tree order
  if (tab_index < 0) {
    for (; CurrentElement(); MoveToPrevious()) {
      current = CurrentElement();
      if (ShouldVisit(*current) && AdjustedTabIndex(*current) >= 0)
        return current;
    }
  } else {
    if (Element* winner =
            FindElementWithExactTabIndex(tab_index, kWebFocusTypeBackward))
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
  while (Element* found = scope.FindFocusableElement(kWebFocusTypeForward)) {
    if (found->DelegatesFocus()) {
      // If tabindex is positive, invalid, or missing, find focusable element
      // inside its shadow tree.
      if (AdjustedTabIndex(*found) >= 0 &&
          IsShadowHostWithoutCustomFocusLogic(*found)) {
        ScopedFocusNavigation inner_scope =
            ScopedFocusNavigation::OwnedByShadowHost(*found, owner_map);
        if (Element* found_in_inner_focus_scope =
                FindFocusableElementRecursivelyForward(inner_scope, owner_map))
          return found_in_inner_focus_scope;
      }
      // Skip to the next element in the same scope.
      continue;
    }
    if (!IsNonFocusableFocusScopeOwner(*found))
      return found;

    // Now |found| is on a non focusable scope owner (either shadow host or
    // <shadow> or slot) Find inside the inward scope and return it if found.
    // Otherwise continue searching in the same scope.
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
  while (Element* found = scope.FindFocusableElement(kWebFocusTypeBackward)) {
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
      if (found->DelegatesFocus())
        continue;
      return found;
    }

    // If delegatesFocus is true and tabindex is negative, skip the whole shadow
    // tree under the shadow host.
    if (found->DelegatesFocus() && AdjustedTabIndex(*found) < 0)
      continue;

    // Now |found| is on a non focusable scope owner (a shadow host, a <shadow>
    // or a slot).  Find focusable element in descendant scope. If not found,
    // find the next focusable element within the current scope.
    if (IsNonFocusableFocusScopeOwner(*found)) {
      ScopedFocusNavigation inner_scope =
          ScopedFocusNavigation::OwnedByNonFocusableFocusScopeOwner(*found,
                                                                    owner_map);
      if (Element* found_in_inner_focus_scope =
              FindFocusableElementRecursivelyBackward(inner_scope, owner_map))
        return found_in_inner_focus_scope;
      continue;
    }
    if (!found->DelegatesFocus())
      return found;
  }
  return nullptr;
}

Element* FindFocusableElementRecursively(WebFocusType type,
                                         ScopedFocusNavigation& scope,
                                         FocusController::OwnerMap& owner_map) {
  return (type == kWebFocusTypeForward)
             ? FindFocusableElementRecursivelyForward(scope, owner_map)
             : FindFocusableElementRecursivelyBackward(scope, owner_map);
}

Element* FindFocusableElementDescendingDownIntoFrameDocument(
    WebFocusType type,
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
    container_local_frame->GetDocument()->UpdateStyleAndLayout();
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
  Element* found;
  if (current && IsShadowHostWithoutCustomFocusLogic(*current)) {
    ScopedFocusNavigation inner_scope =
        ScopedFocusNavigation::OwnedByShadowHost(*current, owner_map);
    Element* found_in_inner_focus_scope =
        FindFocusableElementRecursivelyForward(inner_scope, owner_map);
    found = found_in_inner_focus_scope
                ? found_in_inner_focus_scope
                : FindFocusableElementRecursivelyForward(scope, owner_map);
  } else {
    found = FindFocusableElementRecursivelyForward(scope, owner_map);
  }

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
      kWebFocusTypeForward, found, owner_map);
}

Element* FindFocusableElementAcrossFocusScopesBackward(
    ScopedFocusNavigation& scope,
    FocusController::OwnerMap& owner_map) {
  Element* found = FindFocusableElementRecursivelyBackward(scope, owner_map);

  // If there's no focusable element to advance to, move up the focus scopes
  // until we find one.
  ScopedFocusNavigation current_scope = scope;
  while (!found) {
    Element* owner = current_scope.Owner();
    if (!owner)
      break;
    current_scope = ScopedFocusNavigation::CreateFor(*owner, owner_map);
    if (IsKeyboardFocusableShadowHost(*owner) && !owner->DelegatesFocus()) {
      found = owner;
      break;
    }
    found = FindFocusableElementRecursivelyBackward(current_scope, owner_map);
  }
  return FindFocusableElementDescendingDownIntoFrameDocument(
      kWebFocusTypeBackward, found, owner_map);
}

Element* FindFocusableElementAcrossFocusScopes(
    WebFocusType type,
    ScopedFocusNavigation& scope,
    FocusController::OwnerMap& owner_map) {
  return (type == kWebFocusTypeForward)
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
  if (!IsActive() || !IsFocused())
    return false;

  return focused_frame_ &&
         focused_frame_->Tree().IsDescendantOf(document.GetFrame());
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
  if (is_focused_ == focused)
    return;
  is_focused_ = focused;
  if (!is_emulating_focus_)
    FocusHasChanged();
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

bool FocusController::SetInitialFocus(WebFocusType type) {
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
    WebFocusType type,
    bool initial_focus,
    InputDeviceCapabilities* source_capabilities) {
  switch (type) {
    case kWebFocusTypeForward:
    case kWebFocusTypeBackward: {
      // We should never hit this when a RemoteFrame is focused, since the key
      // event that initiated focus advancement should've been routed to that
      // frame's process from the beginning.
      auto* starting_frame = To<LocalFrame>(FocusedOrMainFrame());
      return AdvanceFocusInDocumentOrder(starting_frame, nullptr, type,
                                         initial_focus, source_capabilities);
    }
    case kWebFocusTypeSpatialNavigation:
      // Fallthrough - SpatialNavigation should use
      // SpatialNavigationController.
    default:
      NOTREACHED();
  }

  return false;
}

bool FocusController::AdvanceFocusAcrossFrames(
    WebFocusType type,
    RemoteFrame* from,
    LocalFrame* to,
    InputDeviceCapabilities* source_capabilities) {
  // If we are shifting focus from a child frame to its parent, the
  // child frame has no more focusable elements, and we should continue
  // looking for focusable elements in the parent, starting from the <iframe>
  // element of the child frame.
  Element* start = nullptr;
  if (from->Tree().Parent() == to) {
    DCHECK(from->Owner()->IsLocal());
    start = To<HTMLFrameOwnerElement>(from->Owner());
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
    WebFocusType type,
    bool initial_focus,
    InputDeviceCapabilities* source_capabilities) {
  DCHECK(frame);
  Document* document = frame->GetDocument();
  document->UpdateDistributionForLegacyDistributedNodes();
  OwnerMap owner_map;

  Element* current = start;
#if DCHECK_IS_ON()
  DCHECK(!current || !IsNonFocusableShadowHost(*current));
#endif
  if (!current && !initial_focus)
    current = document->SequentialFocusNavigationStartingPoint(type);

  document->UpdateStyleAndLayout();
  ScopedFocusNavigation scope =
      current ? ScopedFocusNavigation::CreateFor(*current, owner_map)
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
    if (!initial_focus && page_->GetChromeClient().CanTakeFocus(type)) {
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

    if (!element)
      return false;
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
  if (owner && (has_remote_frame || !IsHTMLPlugInElement(*element) ||
                !element->IsKeyboardFocusable())) {
    // FIXME: We should not focus frames that have no scrollbars, as focusing
    // them isn't useful to the user.
    if (!owner->ContentFrame())
      return false;

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

  element->focus(
      FocusParams(SelectionBehaviorOnFocus::kReset, type, source_capabilities));
  return true;
}

Element* FocusController::FindFocusableElement(WebFocusType type,
                                               Element& element,
                                               OwnerMap& owner_map) {
  // FIXME: No spacial navigation code yet.
  DCHECK(type == kWebFocusTypeForward || type == kWebFocusTypeBackward);
  ScopedFocusNavigation scope =
      ScopedFocusNavigation::CreateFor(element, owner_map);
  return FindFocusableElementAcrossFocusScopes(type, scope, owner_map);
}

Element* FocusController::NextFocusableElementInForm(Element* element,
                                                     WebFocusType focus_type) {
  // TODO(ajith.v) Due to crbug.com/781026 when next/previous element is far
  // from current element in terms of tabindex, then it's signalling CPU load.
  // Will nvestigate further for a proper solution later.
  static const int kFocusTraversalThreshold = 50;
  element->GetDocument().UpdateStyleAndLayout();
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

  if (!form_owner)
    return nullptr;

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
    if (next_html_element->isContentEditableForBinding() &&
        next_element->IsDescendantOf(form_owner))
      return next_element;
    auto* form_element = DynamicTo<HTMLFormControlElement>(next_element);
    if (!form_element)
      continue;
    if (form_element->formOwner() != form_owner ||
        form_element->IsDisabledOrReadOnly())
      continue;
    // Focusless spatial navigation supports all form types. However, submit
    // buttons are explicitly excluded as moving to them isn't necessary - the
    // IME should just submit instead.
    if (RuntimeEnabledFeatures::FocuslessSpatialNavigationEnabled() &&
        page_->GetSettings().GetSpatialNavigationEnabled() &&
        !form_element->CanBeSuccessfulSubmitButton()) {
      return next_element;
    }
    LayoutObject* layout = next_element->GetLayoutObject();
    if (layout && layout->IsTextControl()) {
      // TODO(ajith.v) Extend it for select elements, radio buttons and check
      // boxes
      return next_element;
    }
  }
  return nullptr;
}

Element* FocusController::FindFocusableElementInShadowHost(
    const Element& shadow_host) {
  DCHECK(shadow_host.AuthorShadowRoot());
  OwnerMap owner_map;
  ScopedFocusNavigation scope =
      ScopedFocusNavigation::OwnedByShadowHost(shadow_host, owner_map);
  Element* result = FindFocusableElementAcrossFocusScopes(kWebFocusTypeForward,
                                                          scope, owner_map);
  if (!result)
    return nullptr;
  // Check if |found| is the first focusable element under |element|, and count
  // if it's not.
  const Node* current = &shadow_host;
  while ((current = FlatTreeTraversal::Next(*current))) {
    if (!current->IsElementNode())
      continue;
    if (current == result) {
      // We've reached |found|, which means |found| is the first focusable
      // element so we don't count this.
      break;
    }
    if (ToElement(current)->IsFocusable()) {
      UseCounter::Count(shadow_host.GetDocument(),
                        WebFeature::kDelegateFocusNotFirstInFlatTree);
      break;
    }
  }

  return result;
}

Element* FocusController::FindFocusableElementAfter(Element& element,
                                                    WebFocusType type) {
  if (type != kWebFocusTypeForward && type != kWebFocusTypeBackward)
    return nullptr;
  element.GetDocument().UpdateStyleAndLayout();

  OwnerMap owner_map;
  return FindFocusableElement(type, element, owner_map);
}

static bool RelinquishesEditingFocus(const Element& element) {
  DCHECK(HasEditableStyle(element));
  return element.GetDocument().GetFrame() && RootEditableElement(element);
}

bool FocusController::SetFocusedElement(Element* element,
                                        Frame* new_focused_frame) {
  return SetFocusedElement(
      element, new_focused_frame,
      FocusParams(SelectionBehaviorOnFocus::kNone, kWebFocusTypeNone, nullptr));
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
  for (const auto& it : focus_changed_observers_)
    it->FocusedFrameChanged();
}

void FocusController::Trace(blink::Visitor* visitor) {
  visitor->Trace(page_);
  visitor->Trace(focused_frame_);
  visitor->Trace(focus_changed_observers_);
}

}  // namespace blink
