/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_HEAP_LINKED_STACK_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_HEAP_LINKED_STACK_H_

#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// HeapLinkedStack<> is an Oilpan-managed stack that avoids pre-allocation
// of memory and heap fragmentation.
//
// The API was originally implemented on the call stack by LinkedStack<>
// (now removed: https://codereview.chromium.org/2761853003/).
// See https://codereview.chromium.org/17314010 for the original use-case.
template <typename T>
class HeapLinkedStack : public GarbageCollected<HeapLinkedStack<T>> {
 public:
  HeapLinkedStack() : size_(0) {}

  bool IsEmpty();

  void Push(const T&);
  const T& Peek();
  void Pop();

  size_t size();

  void Trace(blink::Visitor* visitor) {
    for (Node* current = head_; current; current = current->next_)
      visitor->Trace(current);
  }

 private:
  class Node : public GarbageCollected<Node> {
   public:
    Node(const T&, Node* next);

    void Trace(blink::Visitor* visitor) { visitor->Trace(data_); }

    T data_;
    Member<Node> next_;
  };

  Member<Node> head_;
  size_t size_;
};

template <typename T>
HeapLinkedStack<T>::Node::Node(const T& data, Node* next)
    : data_(data), next_(next) {}

template <typename T>
inline bool HeapLinkedStack<T>::IsEmpty() {
  return !head_;
}

template <typename T>
inline void HeapLinkedStack<T>::Push(const T& data) {
  head_ = MakeGarbageCollected<Node>(data, head_);
  ++size_;
}

template <typename T>
inline const T& HeapLinkedStack<T>::Peek() {
  return head_->data_;
}

template <typename T>
inline void HeapLinkedStack<T>::Pop() {
  DCHECK(head_);
  DCHECK(size_);
  head_ = head_->next_;
  --size_;
}

template <typename T>
inline size_t HeapLinkedStack<T>::size() {
  return size_;
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_HEAP_LINKED_STACK_H_
