// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_COLLECTION_SUPPORT_HEAP_LIST_HASH_SET_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_COLLECTION_SUPPORT_HEAP_LIST_HASH_SET_H_

#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/list_hash_set.h"
#include "third_party/blink/renderer/platform/wtf/type_traits.h"

namespace blink {

class HeapListHashSetAllocator;
template <typename ValueArg>
class HeapListHashSetNode;

template <typename ValueArg>
class HeapListHashSetNode final
    : public GarbageCollected<HeapListHashSetNode<ValueArg>> {
 public:
  using NodeAllocator = HeapListHashSetAllocator;
  using PointerType = Member<HeapListHashSetNode>;
  using Value = ValueArg;

  template <typename U>
  static HeapListHashSetNode* Create(NodeAllocator* allocator, U&& value) {
    return MakeGarbageCollected<HeapListHashSetNode>(std::forward<U>(value));
  }

  template <typename U>
  explicit HeapListHashSetNode(U&& value) noexcept
      : value_(std::forward<U>(value)) {
    static_assert(std::is_trivially_destructible<Value>::value,
                  "Garbage collected types used in ListHashSet must be "
                  "trivially destructible");
  }

  HeapListHashSetNode() = delete;
  HeapListHashSetNode(const HeapListHashSetNode&) = delete;
  HeapListHashSetNode& operator=(const HeapListHashSetNode&) = delete;

  void Destroy(NodeAllocator* allocator) {}

  HeapListHashSetNode* Next() const { return next_; }
  HeapListHashSetNode* Prev() const { return prev_; }

  void Trace(Visitor* visitor) const {
    visitor->Trace(prev_);
    visitor->Trace(next_);
    visitor->Trace(value_);
  }

  ValueArg value_;
  PointerType prev_ = nullptr;
  PointerType next_ = nullptr;
};

// Empty allocator as HeapListHashSetNode directly allocates using
// MakeGarbageCollected().
class HeapListHashSetAllocator final {
  DISALLOW_NEW();

 public:
  using TableAllocator = HeapAllocator;

  static constexpr bool kIsGarbageCollected = true;

  struct AllocatorProvider final {
    void CreateAllocatorIfNeeded() {}
    HeapListHashSetAllocator* Get() { return nullptr; }
    void Swap(AllocatorProvider& other) {}
  };
};

template <typename ValueArg,
          wtf_size_t inlineCapacity = 0,  // The inlineCapacity is just a dummy
                                          // to match ListHashSet (off-heap).
          typename HashArg = typename DefaultHash<ValueArg>::Hash>
class HeapListHashSet final
    : public GarbageCollected<
          HeapListHashSet<ValueArg, inlineCapacity, HashArg>>,
      public ListHashSet<ValueArg,
                         inlineCapacity,
                         HashArg,
                         HeapListHashSetAllocator> {
 public:
  HeapListHashSet() { CheckType(); }

  void Trace(Visitor* v) const {
    ListHashSet<ValueArg, inlineCapacity, HashArg,
                HeapListHashSetAllocator>::Trace(v);
  }

 private:
  static void CheckType() {
    static_assert(WTF::IsMemberOrWeakMemberType<ValueArg>::value,
                  "HeapListHashSet supports only Member and WeakMember.");
    static_assert(std::is_trivially_destructible<HeapListHashSet>::value,
                  "HeapListHashSet must be trivially destructible.");
    static_assert(WTF::IsTraceable<ValueArg>::value,
                  "For sets without traceable elements, use ListHashSet<> "
                  "instead of HeapListHashSet<>.");
  }
};

}  // namespace blink

namespace WTF {

template <typename Value, wtf_size_t inlineCapacity>
struct ListHashSetTraits<Value, inlineCapacity, blink::HeapListHashSetAllocator>
    : public HashTraits<blink::Member<blink::HeapListHashSetNode<Value>>> {
  using Allocator = blink::HeapListHashSetAllocator;
  using Node = blink::HeapListHashSetNode<Value>;

  static constexpr bool kCanTraceConcurrently =
      HashTraits<Value>::kCanTraceConcurrently;
};

}  // namespace WTF

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_COLLECTION_SUPPORT_HEAP_LIST_HASH_SET_H_
