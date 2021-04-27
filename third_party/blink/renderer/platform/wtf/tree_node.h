/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TREE_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TREE_NODE_H_

#include "base/check_op.h"

namespace WTF {

//
// TreeNode is generic, ContainerNode-like linked tree data structure.
// There are a few notable difference between TreeNode and Node:
//
//  * Each TreeNode node is NOT ref counted. The user have to retain its
//    lifetime somehow.
//    FIXME: lifetime management could be parameterized so that ref counted
//    implementations can be used.
//  * It checks invalid input. The callers have to ensure that given
//    parameter is sound.
//  * There is no branch-leaf difference. Every node can be a parent of other
//    node.
//
// FIXME: oilpan: Trace tree node edges to ensure we don't have dangling
// pointers.
// As it is used in HTMLImport it is safe since they all die together.
template <class T>
class TreeNode {
 public:
  typedef T NodeType;

  TreeNode()
      : next_(nullptr),
        previous_(nullptr),
        parent_(nullptr),
        first_child_(nullptr),
        last_child_(nullptr) {}

  NodeType* Next() const { return next_; }
  NodeType* Previous() const { return previous_; }
  NodeType* Parent() const { return parent_; }
  NodeType* FirstChild() const { return first_child_; }
  NodeType* LastChild() const { return last_child_; }
  NodeType* Here() const {
    return static_cast<NodeType*>(const_cast<TreeNode*>(this));
  }

  bool Orphan() const {
    return !parent_ && !next_ && !previous_ && !first_child_ && !last_child_;
  }
  bool HasChildren() const { return first_child_; }

  void InsertBefore(NodeType* new_child, NodeType* ref_child) {
    DCHECK(!new_child->Parent());
    DCHECK(!new_child->Next());
    DCHECK(!new_child->Previous());

    DCHECK(!ref_child || this == ref_child->Parent());

    if (!ref_child) {
      AppendChild(new_child);
      return;
    }

    NodeType* new_previous = ref_child->Previous();
    new_child->parent_ = Here();
    new_child->next_ = ref_child;
    new_child->previous_ = new_previous;
    ref_child->previous_ = new_child;
    if (new_previous)
      new_previous->next_ = new_child;
    else
      first_child_ = new_child;
  }

  void AppendChild(NodeType* child) {
    DCHECK(!child->Parent());
    DCHECK(!child->Next());
    DCHECK(!child->Previous());

    child->parent_ = Here();

    if (!last_child_) {
      DCHECK(!first_child_);
      last_child_ = first_child_ = child;
      return;
    }

    DCHECK(!last_child_->next_);
    NodeType* old_last = last_child_;
    last_child_ = child;

    child->previous_ = old_last;
    old_last->next_ = child;
  }

  NodeType* RemoveChild(NodeType* child) {
    DCHECK_EQ(child->Parent(), this);

    if (first_child_ == child)
      first_child_ = child->Next();
    if (last_child_ == child)
      last_child_ = child->Previous();

    NodeType* old_next = child->Next();
    NodeType* old_previous = child->Previous();
    child->parent_ = child->next_ = child->previous_ = nullptr;

    if (old_next)
      old_next->previous_ = old_previous;
    if (old_previous)
      old_previous->next_ = old_next;

    return child;
  }

  void TakeChildrenFrom(NodeType* old_parent) {
    DCHECK_NE(old_parent, this);
    while (old_parent->HasChildren()) {
      NodeType* child = old_parent->FirstChild();
      old_parent->RemoveChild(child);
      AppendChild(child);
    }
  }

 private:
  NodeType* next_;
  NodeType* previous_;
  NodeType* parent_;
  NodeType* first_child_;
  NodeType* last_child_;
};

template <class T>
inline typename TreeNode<T>::NodeType* TraverseNext(
    const TreeNode<T>* current,
    const TreeNode<T>* stay_within = nullptr) {
  if (typename TreeNode<T>::NodeType* next = current->FirstChild())
    return next;
  if (current == stay_within)
    return nullptr;
  if (typename TreeNode<T>::NodeType* next = current->Next())
    return next;
  for (typename TreeNode<T>::NodeType* parent = current->Parent(); parent;
       parent = parent->Parent()) {
    if (parent == stay_within)
      return nullptr;
    if (typename TreeNode<T>::NodeType* next = parent->Next())
      return next;
  }

  return nullptr;
}

template <class T>
inline typename TreeNode<T>::NodeType* TraverseFirstPostOrder(
    const TreeNode<T>* current) {
  typename TreeNode<T>::NodeType* first = current->Here();
  while (first->FirstChild())
    first = first->FirstChild();
  return first;
}

template <class T>
inline typename TreeNode<T>::NodeType* TraverseNextPostOrder(
    const TreeNode<T>* current,
    const TreeNode<T>* stay_within = nullptr) {
  if (current == stay_within)
    return nullptr;

  typename TreeNode<T>::NodeType* next = current->Next();
  if (!next)
    return current->Parent();
  while (next->FirstChild())
    next = next->FirstChild();
  return next;
}

}  // namespace WTF

using WTF::TreeNode;
using WTF::TraverseNext;
using WTF::TraverseNextPostOrder;

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TREE_NODE_H_
