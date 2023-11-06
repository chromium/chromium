/*
 * Copyright (C) 2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_COUNTER_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_COUNTER_NODE_H_

#include "base/dcheck_is_on.h"
#include "third_party/blink/renderer/core/css/counters_scope.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

// This implements a counter tree that is used for finding parents in counters()
// lookup, and for propagating count changes when nodes are added or removed.

// Parents represent unique counters and their scope, which are created either
// explicitly by "counter-reset" style rules or implicitly by referring to a
// counter that is not in scope.
// Such nodes are tagged as "reset" nodes, although they are not all due to
// "counter-reset".

// Not that layout tree children are often counter tree siblings due to counter
// scoping rules.

namespace blink {

class Element;
class LayoutObject;
class LayoutCounter;

class CounterNode : public GarbageCollected<CounterNode> {
 public:
  enum Type { kIncrementType = 1 << 0, kResetType = 1 << 1, kSetType = 1 << 2 };

  CounterNode(LayoutObject& object,
              unsigned type_mask,
              int value,
              bool is_reversed = false);
  void Destroy();
  void Trace(Visitor*) const;

  bool ActsAsReset() const { return HasResetType() || !parent_; }
  bool HasUseType() const { return type_mask_ == 0u; }
  bool HasIncrementType() const { return type_mask_ & kIncrementType; }
  bool HasResetType() const { return type_mask_ & kResetType; }
  bool HasSetType() const { return type_mask_ & kSetType; }
  int Value() const { return value_; }
  int CountInParent() const { return count_in_parent_; }
  LayoutObject& Owner() const { return *owner_; }
  Element& OwnerElement() const;
  Element& OwnerNonPseudoElement() const;
  void AddLayoutObject(LayoutCounter*);
  void RemoveLayoutObject(LayoutCounter*);

  // Invalidates the text in the layoutObjects of this counter, if any.
  void ResetLayoutObjects();

  const AtomicString& Identifier() const { return identifier_; }

  CounterNode* PreviousInParent() const { return previous_in_parent_.Get(); }
  void SetPreviousInParent(CounterNode* previous_in_parent) {
    previous_in_parent_ = previous_in_parent;
  }
  bool IsInScope() const { return !!scope_; }
  CountersScope* Scope() const { return scope_.Get(); }
  void SetScope(CountersScope* scope) { scope_ = scope; }
  int ValueAfter() const { return value_after_; }
  void CalculateValueAfter(bool should_reset_increment = false,
                           int num_counters_in_scope = 0) {
    if (IsReversed()) {
      value_after_ = num_counters_in_scope;
      return;
    }
    if (IsReset()) {
      value_after_ = value_;
      return;
    }
    int value_before =
        should_reset_increment && HasIncrementType() ? 0 : value_before_;
    value_after_ =
        base::CheckAdd(value_before, value_).ValueOrDefault(value_before);
  }
  int ValueBefore() const { return value_before_; }
  void SetValueBefore(int value) { value_before_ = value; }
  bool IsReset() const { return HasSetType() || HasResetType(); }
  bool IsReversed() const { return is_reversed_; }

#if DCHECK_IS_ON()
  AtomicString DebugName() const;
#endif  // DCHECK_IS_ON()

  // This finds a closest ancestor style containment boundary, crosses it, and
  // then returns the closest ancestor CounterNode available (for the given
  // `identifier`). Note that the element that specifies contain: style is
  // itself considered to be across the boundary from its subtree.
  static CounterNode* AncestorNodeAcrossStyleContainment(
      const LayoutObject&,
      const AtomicString& identifier);

  // Returns the parent of this CounterNode. If the node is the root, then it
  // instead tries to find a node with the same identifier across the style
  // containment boundary so that it can continue navigating up to the root of
  // the document. This is used for reporting content: counters().
  CounterNode* ParentCrossingStyleContainment(
      const AtomicString& identifier) const;

  CounterNode* Parent() const { return parent_; }
  CounterNode* PreviousSibling() const { return previous_sibling_; }
  CounterNode* NextSibling() const { return next_sibling_; }
  CounterNode* FirstChild() const { return first_child_; }
  CounterNode* LastChild() const { return last_child_; }
  CounterNode* LastDescendant() const;
  CounterNode* PreviousInPreOrder() const;
  CounterNode* NextInPreOrder(const CounterNode* stay_within = nullptr) const;
  CounterNode* NextInPreOrderAfterChildren(
      const CounterNode* stay_within = nullptr) const;

  void InsertAfter(CounterNode* new_child,
                   CounterNode* before_child,
                   const AtomicString& identifier);

  // identifier must match the identifier of this counter.
  void RemoveChild(CounterNode*);

  // Moves all non-reset next siblings of first_node to be children of
  // new_parent. Used when we insert new reset nodes that requires reparenting
  // existing nodes.
  static void MoveNonResetSiblingsToChildOf(CounterNode* first_node,
                                            CounterNode& new_parent,
                                            const AtomicString& identifier);

 private:
  int ComputeCountInParent() const;
  // Invalidates the text in the layoutObject of this counter, if any,
  // and in the layoutObjects of all descendants of this counter, if any.
  void ResetThisAndDescendantsLayoutObjects();
  void Recount();

  unsigned type_mask_;
  int value_;
  int value_before_;
  int count_in_parent_;
  int value_after_;
  bool is_reversed_;
  const Member<LayoutObject> owner_;
  Member<LayoutCounter> root_layout_object_;

  Member<CounterNode> parent_;
  Member<CounterNode> previous_sibling_;
  Member<CounterNode> next_sibling_;
  Member<CounterNode> first_child_;
  Member<CounterNode> last_child_;
  AtomicString identifier_;

  // The counters scope this counter belongs to.
  Member<CountersScope> scope_;
  Member<CounterNode> previous_in_parent_;
};

}  // namespace blink

#if DCHECK_IS_ON()
// Outside the blink namespace for ease of invocation from gdb.
void ShowCounterTree(const blink::CounterNode*);
#endif

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_COUNTER_NODE_H_
