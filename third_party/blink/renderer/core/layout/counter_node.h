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

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"

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

class LayoutObject;
class LayoutCounter;

class CounterNode : public RefCounted<CounterNode> {
  USING_FAST_MALLOC(CounterNode);

 public:
  enum Type { kIncrementType = 1 << 0, kResetType = 1 << 1, kSetType = 1 << 2 };

  static scoped_refptr<CounterNode> Create(LayoutObject&,
                                           unsigned type_mask,
                                           int value);
  ~CounterNode();
  bool ActsAsReset() const { return HasResetType() || !parent_; }
  bool HasResetType() const { return type_mask_ & kResetType; }
  bool HasSetType() const { return type_mask_ & kSetType; }
  int Value() const { return value_; }
  int CountInParent() const { return count_in_parent_; }
  LayoutObject& Owner() const { return *owner_; }
  void AddLayoutObject(LayoutCounter*);
  void RemoveLayoutObject(LayoutCounter*);

  // Invalidates the text in the layoutObjects of this counter, if any.
  void ResetLayoutObjects();

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
  CounterNode(LayoutObject&, unsigned type_mask, int value);
  int ComputeCountInParent() const;
  // Invalidates the text in the layoutObject of this counter, if any,
  // and in the layoutObjects of all descendants of this counter, if any.
  void ResetThisAndDescendantsLayoutObjects();
  void Recount();

  unsigned type_mask_;
  int value_;
  int count_in_parent_;
  LayoutObject* const owner_;
  LayoutCounter* root_layout_object_;

  CounterNode* parent_;
  CounterNode* previous_sibling_;
  CounterNode* next_sibling_;
  CounterNode* first_child_;
  CounterNode* last_child_;
};

}  // namespace blink

#if DCHECK_IS_ON()
// Outside the blink namespace for ease of invocation from gdb.
void showCounterTree(const blink::CounterNode*);
#endif

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_COUNTER_NODE_H_
