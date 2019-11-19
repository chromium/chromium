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

#include "third_party/blink/renderer/core/layout/counter_node.h"

#include "base/numerics/checked_math.h"
#include "third_party/blink/renderer/core/layout/layout_counter.h"

#if DCHECK_IS_ON()
#include <stdio.h>
#endif

namespace blink {

CounterNode::CounterNode(LayoutObject& o, bool has_reset_type, int value)
    : has_reset_type_(has_reset_type),
      value_(value),
      count_in_parent_(0),
      owner_(o),
      root_layout_object_(nullptr),
      parent_(nullptr),
      previous_sibling_(nullptr),
      next_sibling_(nullptr),
      first_child_(nullptr),
      last_child_(nullptr) {}

CounterNode::~CounterNode() {
  // Ideally this would be an assert and this would never be reached. In reality
  // this happens a lot so we need to handle these cases. The node is still
  // connected to the tree so we need to detach it.
  if (parent_ || previous_sibling_ || next_sibling_ || first_child_ ||
      last_child_) {
    CounterNode* old_parent = nullptr;
    CounterNode* old_previous_sibling = nullptr;
    // Instead of calling removeChild() we do this safely as the tree is likely
    // broken if we get here.
    if (parent_) {
      if (parent_->first_child_ == this)
        parent_->first_child_ = next_sibling_;
      if (parent_->last_child_ == this)
        parent_->last_child_ = previous_sibling_;
      old_parent = parent_;
      parent_ = nullptr;
    }
    if (previous_sibling_) {
      if (previous_sibling_->next_sibling_ == this)
        previous_sibling_->next_sibling_ = next_sibling_;
      old_previous_sibling = previous_sibling_;
      previous_sibling_ = nullptr;
    }
    if (next_sibling_) {
      if (next_sibling_->previous_sibling_ == this)
        next_sibling_->previous_sibling_ = old_previous_sibling;
      next_sibling_ = nullptr;
    }
    if (first_child_) {
      // The node's children are reparented to the old parent.
      for (CounterNode* child = first_child_; child;) {
        CounterNode* next_child = child->next_sibling_;
        CounterNode* next_sibling = nullptr;
        child->parent_ = old_parent;
        if (old_previous_sibling) {
          next_sibling = old_previous_sibling->next_sibling_;
          child->previous_sibling_ = old_previous_sibling;
          old_previous_sibling->next_sibling_ = child;
          child->next_sibling_ = next_sibling;
          next_sibling->previous_sibling_ = child;
          old_previous_sibling = child;
        }
        child = next_child;
      }
    }
  }
  ResetLayoutObjects();
}

scoped_refptr<CounterNode> CounterNode::Create(LayoutObject& owner,
                                               bool has_reset_type,
                                               int value) {
  return base::AdoptRef(new CounterNode(owner, has_reset_type, value));
}

CounterNode* CounterNode::NextInPreOrderAfterChildren(
    const CounterNode* stay_within) const {
  if (this == stay_within)
    return nullptr;

  const CounterNode* current = this;
  CounterNode* next = current->next_sibling_;
  for (; !next; next = current->next_sibling_) {
    current = current->parent_;
    if (!current || current == stay_within)
      return nullptr;
  }
  return next;
}

CounterNode* CounterNode::NextInPreOrder(const CounterNode* stay_within) const {
  if (CounterNode* next = first_child_)
    return next;

  return NextInPreOrderAfterChildren(stay_within);
}

CounterNode* CounterNode::LastDescendant() const {
  CounterNode* last = last_child_;
  if (!last)
    return nullptr;

  while (CounterNode* last_child = last->last_child_)
    last = last_child;

  return last;
}

CounterNode* CounterNode::PreviousInPreOrder() const {
  CounterNode* previous = previous_sibling_;
  if (!previous)
    return parent_;

  while (CounterNode* last_child = previous->last_child_)
    previous = last_child;

  return previous;
}

int CounterNode::ComputeCountInParent() const {
  // According to the spec, if an increment would overflow or underflow the
  // counter, we are allowed to ignore the increment.
  // https://drafts.csswg.org/css-lists-3/#valdef-counter-reset-custom-ident-integer
  int increment = ActsAsReset() ? 0 : value_;
  if (previous_sibling_) {
    return base::CheckAdd(previous_sibling_->count_in_parent_, increment)
        .ValueOrDefault(previous_sibling_->count_in_parent_);
  }
  DCHECK_EQ(parent_->first_child_, this);
  return base::CheckAdd(parent_->value_, increment)
      .ValueOrDefault(parent_->value_);
}

void CounterNode::AddLayoutObject(LayoutCounter* value) {
  if (!value) {
    NOTREACHED();
    return;
  }
  if (value->counter_node_) {
    NOTREACHED();
    value->counter_node_->RemoveLayoutObject(value);
  }
  DCHECK(!value->next_for_same_counter_);
  for (LayoutCounter* iterator = root_layout_object_; iterator;
       iterator = iterator->next_for_same_counter_) {
    if (iterator == value) {
      NOTREACHED();
      return;
    }
  }
  value->next_for_same_counter_ = root_layout_object_;
  root_layout_object_ = value;
  if (value->counter_node_ != this) {
    if (value->counter_node_) {
      NOTREACHED();
      value->counter_node_->RemoveLayoutObject(value);
    }
    value->counter_node_ = this;
  }
}

void CounterNode::RemoveLayoutObject(LayoutCounter* value) {
  if (!value) {
    NOTREACHED();
    return;
  }
  if (value->counter_node_ && value->counter_node_ != this) {
    NOTREACHED();
    value->counter_node_->RemoveLayoutObject(value);
  }
  LayoutCounter* previous = nullptr;
  for (LayoutCounter* iterator = root_layout_object_; iterator;
       iterator = iterator->next_for_same_counter_) {
    if (iterator == value) {
      if (previous)
        previous->next_for_same_counter_ = value->next_for_same_counter_;
      else
        root_layout_object_ = value->next_for_same_counter_;
      value->next_for_same_counter_ = nullptr;
      value->counter_node_ = nullptr;
      return;
    }
    previous = iterator;
  }
  NOTREACHED();
}

void CounterNode::ResetLayoutObjects() {
  while (root_layout_object_) {
    // This makes m_rootLayoutObject point to the next layoutObject if any since
    // it disconnects the m_rootLayoutObject from this.
    root_layout_object_->Invalidate();
  }
}

void CounterNode::ResetThisAndDescendantsLayoutObjects() {
  CounterNode* node = this;
  do {
    node->ResetLayoutObjects();
    node = node->NextInPreOrder(this);
  } while (node);
}

void CounterNode::Recount() {
  for (CounterNode* node = this; node; node = node->next_sibling_) {
    int old_count = node->count_in_parent_;
    int new_count = node->ComputeCountInParent();
    if (old_count == new_count)
      break;
    node->count_in_parent_ = new_count;
    node->ResetThisAndDescendantsLayoutObjects();
  }
}

void CounterNode::InsertAfter(CounterNode* new_child,
                              CounterNode* ref_child,
                              const AtomicString& identifier) {
  DCHECK(new_child);
  DCHECK(!new_child->parent_);
  DCHECK(!new_child->previous_sibling_);
  DCHECK(!new_child->next_sibling_);
  // If the refChild is not our child we can not complete the request. This
  // hardens against bugs in LayoutCounter.
  // When layoutObjects are reparented it may request that we insert counter
  // nodes improperly.
  if (ref_child && ref_child->parent_ != this)
    return;

  if (new_child->has_reset_type_) {
    while (last_child_ != ref_child)
      LayoutCounter::DestroyCounterNode(last_child_->Owner(), identifier);
  }

  CounterNode* next;

  if (ref_child) {
    next = ref_child->next_sibling_;
    ref_child->next_sibling_ = new_child;
  } else {
    next = first_child_;
    first_child_ = new_child;
  }

  new_child->parent_ = this;
  new_child->previous_sibling_ = ref_child;

  if (next) {
    DCHECK_EQ(next->previous_sibling_, ref_child);
    next->previous_sibling_ = new_child;
    new_child->next_sibling_ = next;
  } else {
    DCHECK_EQ(last_child_, ref_child);
    last_child_ = new_child;
  }

  if (!new_child->first_child_ || new_child->has_reset_type_) {
    new_child->count_in_parent_ = new_child->ComputeCountInParent();
    new_child->ResetThisAndDescendantsLayoutObjects();
    if (next)
      next->Recount();
    return;
  }

  // The code below handles the case when a formerly root increment counter is
  // loosing its root position and therefore its children become next siblings.
  CounterNode* last = new_child->last_child_;
  CounterNode* first = new_child->first_child_;

  DCHECK(last);
  new_child->next_sibling_ = first;
  if (last_child_ == new_child)
    last_child_ = last;

  first->previous_sibling_ = new_child;

  // The case when the original next sibling of the inserted node becomes a
  // child of one of the former children of the inserted node is not handled
  // as it is believed to be impossible since:
  // 1. if the increment counter node lost it's root position as a result of
  //    another counter node being created, it will be inserted as the last
  //    child so next is null.
  // 2. if the increment counter node lost it's root position as a result of a
  //    layoutObject being inserted into the document's layout tree, all its
  //    former children counters are attached to children of the inserted
  //    layoutObject and hence cannot be in scope for counter nodes attached
  // to layoutObjects that were already in the document's layout tree.
  last->next_sibling_ = next;
  if (next) {
    DCHECK_EQ(next->previous_sibling_, new_child);
    next->previous_sibling_ = last;
  } else {
    last_child_ = last;
  }
  for (next = first;; next = next->next_sibling_) {
    next->parent_ = this;
    if (last == next)
      break;
  }

  new_child->first_child_ = nullptr;
  new_child->last_child_ = nullptr;
  new_child->count_in_parent_ = new_child->ComputeCountInParent();
  new_child->ResetLayoutObjects();
  first->Recount();
}

void CounterNode::RemoveChild(CounterNode* old_child) {
  DCHECK(old_child);
  DCHECK(!old_child->first_child_);
  DCHECK(!old_child->last_child_);

  CounterNode* next = old_child->next_sibling_;
  CounterNode* previous = old_child->previous_sibling_;

  old_child->next_sibling_ = nullptr;
  old_child->previous_sibling_ = nullptr;
  old_child->parent_ = nullptr;

  if (previous) {
    previous->next_sibling_ = next;
  } else {
    DCHECK_EQ(first_child_, old_child);
    first_child_ = next;
  }

  if (next) {
    next->previous_sibling_ = previous;
  } else {
    DCHECK_EQ(last_child_, old_child);
    last_child_ = previous;
  }

  if (next)
    next->Recount();
}

void CounterNode::MoveNonResetSiblingsToChildOf(
    CounterNode* first_node,
    CounterNode& new_parent,
    const AtomicString& identifier) {
  if (!first_node)
    return;

  scoped_refptr<CounterNode> cur_node = first_node;
  scoped_refptr<CounterNode> old_parent = first_node->Parent();
  while (cur_node) {
    scoped_refptr<CounterNode> next = cur_node->NextSibling();
    if (!cur_node->ActsAsReset()) {
      old_parent->RemoveChild(cur_node.get());
      new_parent.InsertAfter(cur_node.get(), new_parent.LastChild(),
                             identifier);
    }
    cur_node = next;
  }
}

#if DCHECK_IS_ON()

static void ShowTreeAndMark(const CounterNode* node) {
  const CounterNode* root = node;
  while (root->Parent())
    root = root->Parent();

  for (const CounterNode* current = root; current;
       current = current->NextInPreOrder()) {
    fprintf(stderr, "%c", (current == node) ? '*' : ' ');
    for (const CounterNode* parent = current; parent && parent != root;
         parent = parent->Parent())
      fprintf(stderr, "    ");
    fprintf(stderr, "%p %s: %d %d P:%p PS:%p NS:%p R:%p\n", current,
            current->ActsAsReset() ? "reset____" : "increment",
            current->Value(), current->CountInParent(), current->Parent(),
            current->PreviousSibling(), current->NextSibling(),
            &current->Owner());
  }
  fflush(stderr);
}

#endif

}  // namespace blink

#if DCHECK_IS_ON()

void showCounterTree(const blink::CounterNode* counter) {
  if (counter)
    ShowTreeAndMark(counter);
  else
    fprintf(stderr, "Cannot showCounterTree for (nil).\n");
}

#endif
