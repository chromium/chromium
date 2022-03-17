// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_LINKED_LIST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_LINKED_LIST_H_

#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

// GridLinkedList has structure of doubly linked list, and is garbage
// collected. Its use is intended for CSS Grid Layout.
//
// In order to use, define a class that inherits GridLinkedListNodeBase.
// This will give MyNode previous and next pointers.
//
//   class MyNode : public GridLinkedListNodeBase<MyNode> {
//     ...
//   };
//
// When initializing MyNode and GridLinkedList instance, make them garbage
// collected.
//
//   Persistent<GridLinkedList<MyNode>> gll =
//       MakeGarbageCollected<GridLinkedList<MyNode>>();
//
//   MyNode* node1 = MakeGarbageCollected<MyNode>(var1);
//
// For adding and removing nodes, use the following functions.
//
//   gll->Append(node1);    add the given node at tail
//   gll->Push(node2);      add the given node at head
//   gll->Remove(node3);    remove the given node from list and connect the
//                          node before and after
//
// In order to obtain information of list, use
//
//   gll->IsEmpty();        returns true if the list is empty
//   gll->Size();           returns the size of list
//

namespace blink {

// A class defining the type of node in the GridLinkedList should inherit
// GridLinkedListNodeBase. This will give previous and next pointer for the
// node, as well as apply garbage collection to all nodes.
template <typename NodeType>
class GridLinkedListNodeBase
    : public GarbageCollected<GridLinkedListNodeBase<NodeType>> {
 public:
  GridLinkedListNodeBase() = default;

  NodeType* Prev() const { return prev_; }
  NodeType* Next() const { return next_; }
  void SetPrev(NodeType* prev) { prev_ = prev; }
  void SetNext(NodeType* next) { next_ = next; }

  void Trace(Visitor* visitor) const {
    visitor->Trace(prev_);
    visitor->Trace(next_);
  }

 private:
  Member<NodeType> prev_;
  Member<NodeType> next_;
};

// GridLinkedList has data structure of doubly linked list, and its use is
// intended for CSS Grid Layout. NodeType must inherit GridLinkedListNodeBase.
template <typename NodeType>
class GridLinkedList : public GarbageCollected<GridLinkedList<NodeType>> {
 public:
  GridLinkedList() {
    static_assert(IsGarbageCollectedType<NodeType>::value,
                  "NodeType must be a garbage collected object.");
  }

  NodeType* Head() const { return head_; }
  NodeType* Tail() const { return tail_; }

  bool IsEmpty() { return !head_; }

  // Returns the size of the list. O(n).
  int Size();

  // Adds node at the tail of the grid linked list. The node to add should not
  // have adjacent nodes, nor has already been added to the list.
  void Append(NodeType* node);

  // Adds node at the head of the grid linked list. The node to add should not
  // have adjacent nodes, nor has already been added to the list.
  void Push(NodeType* node);

  // Removes specified node from the list. If they exist, previous node and the
  // next node will be connected. This function should not be called when the
  // list is empty.
  void Remove(NodeType* node);

  // Set objects to trace for garbage collection
  void Trace(Visitor* visitor) const {
    visitor->Trace(head_);
    visitor->Trace(tail_);
  }

 private:
  Member<NodeType> head_;
  Member<NodeType> tail_;
};

template <typename NodeType>
int GridLinkedList<NodeType>::Size() {
  int len = 0;
  NodeType* node = head_;
  while (node) {
    node = node->Next();
    len++;
  }
  return len;
}

template <typename NodeType>
void GridLinkedList<NodeType>::Append(NodeType* node) {
  DCHECK(node);
  DCHECK(!node->Prev());
  DCHECK(!node->Next());
  DCHECK_NE(node, head_);
  if (!head_) {
    DCHECK(!tail_);
    head_ = node;
    tail_ = node;
  } else {
    node->SetPrev(tail_);
    tail_->SetNext(node);
    tail_ = node;
  }
}

template <typename NodeType>
void GridLinkedList<NodeType>::Push(NodeType* node) {
  DCHECK(node);
  DCHECK(!node->Prev());
  DCHECK(!node->Next());
  DCHECK_NE(node, head_);
  if (!head_) {
    DCHECK(!tail_);
    head_ = node;
    tail_ = node;
  } else {
    head_->SetPrev(node);
    node->SetNext(head_);
    head_ = node;
  }
}

template <typename NodeType>
void GridLinkedList<NodeType>::Remove(NodeType* node) {
  DCHECK(node);
  if (node->Prev()) {
    DCHECK_NE(node, head_);
    node->Prev()->SetNext(node->Next());
  } else {
    DCHECK_EQ(node, head_);
    head_ = node->Next();
  }
  if (node->Next()) {
    DCHECK_NE(node, tail_);
    node->Next()->SetPrev(node->Prev());
  } else {
    DCHECK_EQ(node, tail_);
    tail_ = node->Prev();
  }
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_LINKED_LIST_H_
