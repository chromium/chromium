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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_COLLECTION_SUPPORT_HEAP_LINKED_QUEUE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_COLLECTION_SUPPORT_HEAP_LINKED_QUEUE_H_

#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator_impl.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/type_traits.h"

namespace blink {

// HeapLinkedQueue<> is an Oilpan-managed queue that avoids pre-allocation
// of memory and heap fragmentation.
//
// The API was originally implemented on the call stack by LinkedStack<>
// (now removed: https://codereview.chromium.org/2761853003/), and then
// converted into a queue (https://crrev.com/c/3119837).
// See https://codereview.chromium.org/17314010 for the original use-case.
template <typename T>
class HeapLinkedQueue final : public GarbageCollected<HeapLinkedQueue<T>> {
 public:
  HeapLinkedQueue() { CheckType(); }

  inline wtf_size_t size() const;
  inline bool IsEmpty() const;

  inline void push_back(const T&);
  inline const T& front() const;
  inline void pop_front();

  void Trace(Visitor* visitor) const {
    visitor->Trace(head_);
    visitor->Trace(tail_);
  }

 private:
  class Node final : public GarbageCollected<Node> {
   public:
    explicit Node(const T&);

    void Trace(Visitor* visitor) const {
      visitor->Trace(data_);
      visitor->Trace(next_);
    }

    T data_;
    Member<Node> next_;
  };

  static void CheckType() {
    static_assert(WTF::IsMemberType<T>::value,
                  "HeapLinkedQueue supports only Member.");
  }

  Member<Node> head_;
  Member<Node> tail_;
  wtf_size_t size_ = 0;
};

template <typename T>
HeapLinkedQueue<T>::Node::Node(const T& data) : data_(data) {}

template <typename T>
bool HeapLinkedQueue<T>::IsEmpty() const {
  DCHECK(head_ || !size_);
  return !head_;
}

template <typename T>
void HeapLinkedQueue<T>::push_back(const T& data) {
  Node* new_entry = MakeGarbageCollected<Node>(data);
  if (tail_)
    tail_->next_ = new_entry;
  else
    head_ = new_entry;
  tail_ = new_entry;
  ++size_;
}

template <typename T>
const T& HeapLinkedQueue<T>::front() const {
  DCHECK(!IsEmpty());
  return head_->data_;
}

template <typename T>
void HeapLinkedQueue<T>::pop_front() {
  DCHECK(!IsEmpty());
  head_ = head_->next_;
  if (!head_)
    tail_ = nullptr;
  --size_;
}

template <typename T>
wtf_size_t HeapLinkedQueue<T>::size() const {
  return size_;
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_COLLECTION_SUPPORT_HEAP_LINKED_QUEUE_H_
