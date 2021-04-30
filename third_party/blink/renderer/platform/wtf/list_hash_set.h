/*
 * Copyright (C) 2005, 2006, 2007, 2008, 2011, 2012 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2011, Benjamin Poulain <ikipou@gmail.com>
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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_LIST_HASH_SET_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_LIST_HASH_SET_H_

#include <memory>
#include <type_traits>

#include "base/dcheck_is_on.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partition_allocator.h"
#include "third_party/blink/renderer/platform/wtf/conditional_destructor.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace WTF {

// ListHashSet provides a Set interface like HashSet, but also has a
// predictable iteration order. It has O(1) insertion, removal, and test for
// containership. It maintains a linked list through its contents such that
// iterating it yields values in the order in which they were inserted.
//
// ListHashSet iterators are not invalidated by mutation of the collection,
// unless they point to removed items. This means, for example, that you can
// safely modify the container while iterating over it, as long as you don't
// remove the current item.
//
// Prefer to use LinkedHashSet instead where possible
// (https://crbug.com/614112). We would like to eventually remove ListHashSet
// in favor of LinkedHashSet, because the latter supports WeakMember<T>.
template <typename Value,
          size_t inlineCapacity,
          typename HashFunctions,
          typename Allocator>
class ListHashSet;

template <typename Set>
class ListHashSetIterator;
template <typename Set>
class ListHashSetConstIterator;
template <typename Set>
class ListHashSetReverseIterator;
template <typename Set>
class ListHashSetConstReverseIterator;

template <typename ValueArg, typename Allocator>
class MallocedListHashSetNode;
template <typename ValueArg, wtf_size_t inlineCapacity>
struct MallocedListHashSetAllocator;

template <typename HashArg>
struct ListHashSetNodeHashFunctions;
template <typename HashArg, typename NodeArg>
struct ListHashSetTranslator;
template <typename Value, wtf_size_t inlineCapacity, typename Allocator>
struct ListHashSetTraits;

template <typename Value, wtf_size_t inlineCapacity>
struct ListHashSetTraits<Value,
                         inlineCapacity,
                         MallocedListHashSetAllocator<Value, inlineCapacity>>
    : public HashTraits<MallocedListHashSetNode<
          Value,
          MallocedListHashSetAllocator<Value, inlineCapacity>>*> {
  using Allocator = MallocedListHashSetAllocator<Value, inlineCapacity>;
  using Node = MallocedListHashSetNode<Value, Allocator>;
};

template <typename ValueArg, typename AllocatorArg>
class MallocedListHashSetNode {
  DISALLOW_NEW();

 public:
  using NodeAllocator = AllocatorArg;
  using PointerType = MallocedListHashSetNode*;
  using Value = ValueArg;

  template <typename U>
  static MallocedListHashSetNode* Create(NodeAllocator* allocator, U&& value) {
    return new (allocator->AllocateNode())
        MallocedListHashSetNode(std::forward<U>(value));
  }

  template <typename U>
  explicit MallocedListHashSetNode(U&& value)
      : value_(std::forward<U>(value)) {}

  MallocedListHashSetNode() = delete;
  MallocedListHashSetNode(const MallocedListHashSetNode&) = delete;
  MallocedListHashSetNode& operator=(const MallocedListHashSetNode&) = delete;

  void Destroy(NodeAllocator* allocator) {
    this->~MallocedListHashSetNode();
    allocator->Deallocate(this);
  }

  MallocedListHashSetNode* Next() const { return next_; }
  MallocedListHashSetNode* Prev() const { return prev_; }

  ValueArg value_;
  PointerType prev_ = nullptr;
  PointerType next_ = nullptr;
#if DCHECK_IS_ON()
  bool is_allocated_ = true;
#endif
};

// This allocator is only used for non-Heap ListHashSets.
template <typename ValueArg, wtf_size_t inlineCapacity>
struct MallocedListHashSetAllocator : public PartitionAllocator {
  using TableAllocator = PartitionAllocator;
  using Node = MallocedListHashSetNode<ValueArg, MallocedListHashSetAllocator>;

  class AllocatorProvider {
    DISALLOW_NEW();

   public:
    AllocatorProvider() = default;
    void CreateAllocatorIfNeeded() {
      if (!allocator_)
        allocator_ = new MallocedListHashSetAllocator;
    }

    void ReleaseAllocator() {
      delete allocator_;
      allocator_ = nullptr;
    }

    void Swap(AllocatorProvider& other) {
      std::swap(allocator_, other.allocator_);
    }

    MallocedListHashSetAllocator* Get() const {
      DCHECK(allocator_);
      return allocator_;
    }

   private:
    // Not using std::unique_ptr as this pointer should be deleted at
    // releaseAllocator() method rather than at destructor.
    MallocedListHashSetAllocator* allocator_ = nullptr;
  };

  MallocedListHashSetAllocator() : free_list_(Pool()) {
    memset(pool_, 0, sizeof(pool_));
  }

  Node* AllocateNode() {
    Node* result = free_list_;

    if (!result) {
      return static_cast<Node*>(WTF::Partitions::FastMalloc(
          sizeof(Node), WTF_HEAP_PROFILER_TYPE_NAME(Node)));
    }

#if DCHECK_IS_ON()
    DCHECK(!result->is_allocated_);
#endif

    Node* next = result->Next();
#if DCHECK_IS_ON()
    DCHECK(!next || !next->is_allocated_);
#endif
    if (!next && !is_done_with_initial_free_list_) {
      next = result + 1;
      if (next == PastPool()) {
        is_done_with_initial_free_list_ = true;
        next = nullptr;
      } else {
        DCHECK(InPool(next));
#if DCHECK_IS_ON()
        DCHECK(!next->is_allocated_);
#endif
      }
    }
    free_list_ = next;

    return result;
  }

  void Deallocate(Node* node) {
    if (InPool(node)) {
#if DCHECK_IS_ON()
      node->is_allocated_ = false;
#endif
      node->next_ = free_list_;
      free_list_ = node;
      return;
    }

    WTF::Partitions::FastFree(node);
  }

  bool InPool(Node* node) { return node >= Pool() && node < PastPool(); }

 private:
  Node* Pool() { return reinterpret_cast_ptr<Node*>(pool_); }
  Node* PastPool() { return Pool() + kPoolSize; }

  Node* free_list_;
  bool is_done_with_initial_free_list_ = false;
#if defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
  // The allocation pool for nodes is one big chunk that ASAN has no insight
  // into, so it can cloak errors. Make it as small as possible to force nodes
  // to be allocated individually where ASAN can see them.
  static const size_t kPoolSize = 1;
#else
  static const size_t kPoolSize = inlineCapacity;
#endif
  alignas(Node) char pool_[sizeof(Node) * kPoolSize];
};

// Note that for a ListHashSet you cannot specify the HashTraits as a template
// argument. It uses the default hash traits for the ValueArg type.
template <typename ValueArg,
          size_t inlineCapacity = 256,
          typename HashArg = typename DefaultHash<ValueArg>::Hash,
          typename AllocatorArg =
              MallocedListHashSetAllocator<ValueArg, inlineCapacity>>
class ListHashSet
    : public ConditionalDestructor<
          ListHashSet<ValueArg, inlineCapacity, HashArg, AllocatorArg>,
          AllocatorArg::kIsGarbageCollected> {
  using Allocator = AllocatorArg;
  USE_ALLOCATOR(ListHashSet, Allocator);

  using NodeTraits = ListHashSetTraits<ValueArg, inlineCapacity, Allocator>;
  using Node = typename NodeTraits::Node;
  using NodeHash = ListHashSetNodeHashFunctions<HashArg>;
  using BaseTranslator = ListHashSetTranslator<HashArg, Node>;

  typedef HashTable<typename Node::PointerType,
                    typename Node::PointerType,
                    IdentityExtractor,
                    NodeHash,
                    NodeTraits,
                    NodeTraits,
                    typename Allocator::TableAllocator>
      ImplType;
  typedef HashTableIterator<typename Node::PointerType,
                            typename Node::PointerType,
                            IdentityExtractor,
                            NodeHash,
                            NodeTraits,
                            NodeTraits,
                            typename Allocator::TableAllocator>
      ImplTypeIterator;
  typedef HashTableConstIterator<typename Node::PointerType,
                                 typename Node::PointerType,
                                 IdentityExtractor,
                                 NodeHash,
                                 NodeTraits,
                                 NodeTraits,
                                 typename Allocator::TableAllocator>
      ImplTypeConstIterator;

  typedef HashArg HashFunctions;

 public:
  typedef ValueArg ValueType;
  typedef HashTraits<ValueType> ValueTraits;
  typedef typename ValueTraits::PeekInType ValuePeekInType;

  typedef ListHashSetIterator<ListHashSet> iterator;
  typedef ListHashSetConstIterator<ListHashSet> const_iterator;
  friend class ListHashSetIterator<ListHashSet>;
  friend class ListHashSetConstIterator<ListHashSet>;

  typedef ListHashSetReverseIterator<ListHashSet> reverse_iterator;
  typedef ListHashSetConstReverseIterator<ListHashSet> const_reverse_iterator;
  friend class ListHashSetReverseIterator<ListHashSet>;
  friend class ListHashSetConstReverseIterator<ListHashSet>;

  struct AddResult final {
    STACK_ALLOCATED();

   public:
    friend class ListHashSet<ValueArg, inlineCapacity, HashArg, AllocatorArg>;
    AddResult(Node* node, bool is_new_entry)
        : stored_value(&node->value_), is_new_entry(is_new_entry) {}
    ValueType* stored_value;
    bool is_new_entry;
  };

  ListHashSet();
  ListHashSet(const ListHashSet&);
  ListHashSet(ListHashSet&&);
  ListHashSet& operator=(const ListHashSet&);
  ListHashSet& operator=(ListHashSet&&);
  void Finalize();

  void Swap(ListHashSet&);

  unsigned size() const { return impl_.size(); }
  unsigned Capacity() const { return impl_.Capacity(); }
  bool IsEmpty() const { return impl_.IsEmpty(); }

  iterator begin() { return MakeIterator(head_); }
  iterator end() { return MakeIterator(nullptr); }
  const_iterator begin() const { return MakeConstIterator(head_); }
  const_iterator end() const { return MakeConstIterator(nullptr); }

  reverse_iterator rbegin() { return MakeReverseIterator(tail_); }
  reverse_iterator rend() { return MakeReverseIterator(nullptr); }
  const_reverse_iterator rbegin() const {
    return MakeConstReverseIterator(tail_);
  }
  const_reverse_iterator rend() const {
    return MakeConstReverseIterator(nullptr);
  }

  ValueType& front();
  const ValueType& front() const;
  void RemoveFirst();

  ValueType& back();
  const ValueType& back() const;
  void pop_back();

  iterator find(ValuePeekInType);
  const_iterator find(ValuePeekInType) const;
  bool Contains(ValuePeekInType) const;

  // An alternate version of find() that finds the object by hashing and
  // comparing with some other type, to avoid the cost of type conversion.
  // The HashTranslator interface is defined in HashSet.
  template <typename HashTranslator, typename T>
  iterator Find(const T&);
  template <typename HashTranslator, typename T>
  const_iterator Find(const T&) const;
  template <typename HashTranslator, typename T>
  bool Contains(const T&) const;

  // The return value of insert is a pair of a pointer to the stored value, and
  // a bool that is true if an new entry was added.
  template <typename IncomingValueType>
  AddResult insert(IncomingValueType&&);

  // Add the value to the end of the collection. If the value was already in
  // the list, it is moved to the end.
  template <typename IncomingValueType>
  AddResult AppendOrMoveToLast(IncomingValueType&&);

  // Add the value to the beginning of the collection. If the value was
  // already in the list, it is moved to the beginning.
  template <typename IncomingValueType>
  AddResult PrependOrMoveToFirst(IncomingValueType&&);

  template <typename IncomingValueType>
  AddResult InsertBefore(ValuePeekInType before_value,
                         IncomingValueType&& new_value);
  template <typename IncomingValueType>
  AddResult InsertBefore(iterator, IncomingValueType&&);

  void erase(ValuePeekInType value) { return erase(find(value)); }
  void erase(iterator);
  void clear();
  template <typename Collection>
  void RemoveAll(const Collection& other) {
    WTF::RemoveAll(*this, other);
  }

  ValueType Take(iterator);
  ValueType Take(ValuePeekInType);
  ValueType TakeFirst();

  template <typename VisitorDispatcher, typename A = AllocatorArg>
  std::enable_if_t<A::kIsGarbageCollected> Trace(VisitorDispatcher) const;

 protected:
  typename ImplType::ValueType** GetBufferSlot() {
    return impl_.GetBufferSlot();
  }

 private:
  void Unlink(Node*);
  void UnlinkAndDelete(Node*);
  void AppendNode(Node*);
  void PrependNode(Node*);
  void InsertNodeBefore(Node* before_node, Node* new_node);
  void DeleteAllNodes();
  Allocator* GetAllocator() { return allocator_provider_.Get(); }
  void CreateAllocatorIfNeeded() {
    allocator_provider_.CreateAllocatorIfNeeded();
  }

  iterator MakeIterator(Node* position) { return iterator(this, position); }
  const_iterator MakeConstIterator(Node* position) const {
    return const_iterator(this, position);
  }
  reverse_iterator MakeReverseIterator(Node* position) {
    return reverse_iterator(this, position);
  }
  const_reverse_iterator MakeConstReverseIterator(Node* position) const {
    return const_reverse_iterator(this, position);
  }

  ImplType impl_;
  typename Node::PointerType head_;
  typename Node::PointerType tail_;
  typename Allocator::AllocatorProvider allocator_provider_;
};

template <typename HashArg>
struct ListHashSetNodeHashFunctions {
  STATIC_ONLY(ListHashSetNodeHashFunctions);
  template <typename T>
  static unsigned GetHash(const T& key) {
    return HashArg::GetHash(key->value_);
  }
  template <typename U, typename V>
  static bool Equal(const U& a, const V& b) {
    return HashArg::Equal(a->value_, b->value_);
  }
  static const bool safe_to_compare_to_empty_or_deleted = false;
};

template <typename Set>
class ListHashSetIterator {
  DISALLOW_NEW();

 private:
  typedef typename Set::const_iterator const_iterator;
  typedef typename Set::Node Node;
  typedef typename Set::ValueType ValueType;
  typedef ValueType& ReferenceType;
  typedef ValueType* PointerType;

  ListHashSetIterator(const Set* set, Node* position)
      : iterator_(set, position) {}

 public:
  ListHashSetIterator() = default;

  // default copy, assignment and destructor are OK

  PointerType Get() const { return const_cast<PointerType>(iterator_.Get()); }
  ReferenceType operator*() const { return *Get(); }
  PointerType operator->() const { return Get(); }

  ListHashSetIterator& operator++() {
    ++iterator_;
    return *this;
  }
  ListHashSetIterator& operator--() {
    --iterator_;
    return *this;
  }

  // Postfix ++ and -- intentionally omitted.

  // Comparison.
  bool operator==(const ListHashSetIterator& other) const {
    return iterator_ == other.iterator_;
  }
  bool operator!=(const ListHashSetIterator& other) const {
    return iterator_ != other.iterator_;
  }

  operator const_iterator() const { return iterator_; }

 private:
  Node* GetNode() { return iterator_.GetNode(); }

  const_iterator iterator_;

  template <typename T, size_t inlineCapacity, typename U, typename V>
  friend class ListHashSet;
};

template <typename Set>
class ListHashSetConstIterator {
  DISALLOW_NEW();

 private:
  typedef typename Set::const_iterator const_iterator;
  typedef typename Set::Node Node;
  typedef typename Set::ValueType ValueType;
  typedef const ValueType& ReferenceType;
  typedef const ValueType* PointerType;

  friend class ListHashSetIterator<Set>;

  ListHashSetConstIterator(const Set* set, Node* position)
      : set_(set), position_(position) {}

 public:
  ListHashSetConstIterator() = default;

  PointerType Get() const { return &position_->value_; }
  ReferenceType operator*() const { return *Get(); }
  PointerType operator->() const { return Get(); }

  ListHashSetConstIterator& operator++() {
    DCHECK(position_);
    position_ = position_->Next();
    return *this;
  }

  ListHashSetConstIterator& operator--() {
    DCHECK_NE(position_, set_->head_);
    if (!position_)
      position_ = set_->tail_;
    else
      position_ = position_->Prev();
    return *this;
  }

  // Postfix ++ and -- intentionally omitted.

  // Comparison.
  bool operator==(const ListHashSetConstIterator& other) const {
    return position_ == other.position_;
  }
  bool operator!=(const ListHashSetConstIterator& other) const {
    return position_ != other.position_;
  }

 private:
  Node* GetNode() { return position_; }

  const Set* set_;
  Node* position_;

  template <typename T, size_t inlineCapacity, typename U, typename V>
  friend class ListHashSet;
};

template <typename Set>
class ListHashSetReverseIterator {
  DISALLOW_NEW();

 private:
  typedef typename Set::const_reverse_iterator const_reverse_iterator;
  typedef typename Set::Node Node;
  typedef typename Set::ValueType ValueType;
  typedef ValueType& ReferenceType;
  typedef ValueType* PointerType;

  ListHashSetReverseIterator(const Set* set, Node* position)
      : iterator_(set, position) {}

 public:
  ListHashSetReverseIterator() = default;

  // default copy, assignment and destructor are OK

  PointerType Get() const { return const_cast<PointerType>(iterator_.Get()); }
  ReferenceType operator*() const { return *Get(); }
  PointerType operator->() const { return Get(); }

  ListHashSetReverseIterator& operator++() {
    ++iterator_;
    return *this;
  }
  ListHashSetReverseIterator& operator--() {
    --iterator_;
    return *this;
  }

  // Postfix ++ and -- intentionally omitted.

  // Comparison.
  bool operator==(const ListHashSetReverseIterator& other) const {
    return iterator_ == other.iterator_;
  }
  bool operator!=(const ListHashSetReverseIterator& other) const {
    return iterator_ != other.iterator_;
  }

  operator const_reverse_iterator() const { return iterator_; }

 private:
  Node* GetNode() { return iterator_.node(); }

  const_reverse_iterator iterator_;

  template <typename T, size_t inlineCapacity, typename U, typename V>
  friend class ListHashSet;
};

template <typename Set>
class ListHashSetConstReverseIterator {
  DISALLOW_NEW();

 private:
  typedef typename Set::reverse_iterator reverse_iterator;
  typedef typename Set::Node Node;
  typedef typename Set::ValueType ValueType;
  typedef const ValueType& ReferenceType;
  typedef const ValueType* PointerType;

  friend class ListHashSetReverseIterator<Set>;

  ListHashSetConstReverseIterator(const Set* set, Node* position)
      : set_(set), position_(position) {}

 public:
  ListHashSetConstReverseIterator() = default;

  PointerType Get() const { return &position_->value_; }
  ReferenceType operator*() const { return *Get(); }
  PointerType operator->() const { return Get(); }

  ListHashSetConstReverseIterator& operator++() {
    DCHECK(position_);
    position_ = position_->Prev();
    return *this;
  }

  ListHashSetConstReverseIterator& operator--() {
    DCHECK_NE(position_, set_->tail_);
    if (!position_)
      position_ = set_->head_;
    else
      position_ = position_->Next();
    return *this;
  }

  // Postfix ++ and -- intentionally omitted.

  // Comparison.
  bool operator==(const ListHashSetConstReverseIterator& other) const {
    return position_ == other.position_;
  }
  bool operator!=(const ListHashSetConstReverseIterator& other) const {
    return position_ != other.position_;
  }

 private:
  Node* GetNode() { return position_; }

  const Set* set_;
  Node* position_;

  template <typename T, size_t inlineCapacity, typename U, typename V>
  friend class ListHashSet;
};

template <typename HashFunctions, typename NodeArg>
struct ListHashSetTranslator {
  STATIC_ONLY(ListHashSetTranslator);

  using Node = NodeArg;

  template <typename T>
  static unsigned GetHash(const T& key) {
    return HashFunctions::GetHash(key);
  }
  template <typename T, typename U>
  static bool Equal(const T& a, const U& b) {
    return HashFunctions::Equal(a->value_, b);
  }
  template <typename Key, typename Allocator>
  static void Translate(typename Node::PointerType& location,
                        Key&& key,
                        Allocator& allocator) {
    // PointerType is
    // - Member<Node> for the Heap version, supporting concurrency using
    //   atomics;
    // - Node* for the PA version;
    location = Node::Create(&allocator, std::forward<Key>(key));
  }
};

template <typename T, size_t inlineCapacity, typename U, typename Allocator>
inline ListHashSet<T, inlineCapacity, U, Allocator>::ListHashSet()
    : head_(nullptr), tail_(nullptr) {
  static_assert(
      Allocator::kIsGarbageCollected ||
          !IsPointerToGarbageCollectedType<T>::value,
      "Cannot put raw pointers to garbage-collected classes into "
      "an off-heap ListHashSet. Use HeapListHashSet<Member<T>> instead.");
}

template <typename T, size_t inlineCapacity, typename U, typename V>
inline ListHashSet<T, inlineCapacity, U, V>::ListHashSet(
    const ListHashSet& other)
    : head_(nullptr), tail_(nullptr) {
  const_iterator end = other.end();
  for (const_iterator it = other.begin(); it != end; ++it)
    insert(*it);
}

template <typename T, size_t inlineCapacity, typename U, typename V>
inline ListHashSet<T, inlineCapacity, U, V>::ListHashSet(ListHashSet&& other)
    : head_(nullptr), tail_(nullptr) {
  Swap(other);
}

template <typename T, size_t inlineCapacity, typename U, typename V>
inline ListHashSet<T, inlineCapacity, U, V>&
ListHashSet<T, inlineCapacity, U, V>::operator=(const ListHashSet& other) {
  ListHashSet tmp(other);
  Swap(tmp);
  return *this;
}

template <typename T, size_t inlineCapacity, typename U, typename V>
inline ListHashSet<T, inlineCapacity, U, V>&
ListHashSet<T, inlineCapacity, U, V>::operator=(ListHashSet&& other) {
  Swap(other);
  return *this;
}

template <typename T, size_t inlineCapacity, typename U, typename V>
inline void ListHashSet<T, inlineCapacity, U, V>::Swap(ListHashSet& other) {
  impl_.swap(other.impl_);
  std::swap(head_, other.head_);
  std::swap(tail_, other.tail_);
  allocator_provider_.Swap(other.allocator_provider_);
}

template <typename T, size_t inlineCapacity, typename U, typename V>
inline void ListHashSet<T, inlineCapacity, U, V>::Finalize() {
  static_assert(!Allocator::kIsGarbageCollected,
                "GCed collections can't be finalized");
  DeleteAllNodes();
  allocator_provider_.ReleaseAllocator();
}

template <typename T, size_t inlineCapacity, typename U, typename V>
inline T& ListHashSet<T, inlineCapacity, U, V>::front() {
  DCHECK(!IsEmpty());
  return head_->value_;
}

template <typename T, size_t inlineCapacity, typename U, typename V>
inline void ListHashSet<T, inlineCapacity, U, V>::RemoveFirst() {
  DCHECK(!IsEmpty());
  impl_.erase(head_);
  UnlinkAndDelete(head_);
}

template <typename T, size_t inlineCapacity, typename U, typename V>
inline const T& ListHashSet<T, inlineCapacity, U, V>::front() const {
  DCHECK(!IsEmpty());
  return head_->value_;
}

template <typename T, size_t inlineCapacity, typename U, typename V>
inline T& ListHashSet<T, inlineCapacity, U, V>::back() {
  DCHECK(!IsEmpty());
  return tail_->value_;
}

template <typename T, size_t inlineCapacity, typename U, typename V>
inline const T& ListHashSet<T, inlineCapacity, U, V>::back() const {
  DCHECK(!IsEmpty());
  return tail_->value_;
}

template <typename T, size_t inlineCapacity, typename U, typename V>
inline void ListHashSet<T, inlineCapacity, U, V>::pop_back() {
  DCHECK(!IsEmpty());
  impl_.erase(tail_);
  UnlinkAndDelete(tail_);
}

template <typename T, size_t inlineCapacity, typename U, typename V>
inline typename ListHashSet<T, inlineCapacity, U, V>::iterator
ListHashSet<T, inlineCapacity, U, V>::find(ValuePeekInType value) {
  ImplTypeIterator it = impl_.template Find<BaseTranslator>(value);
  if (it == impl_.end())
    return end();
  return MakeIterator(*it);
}

template <typename T, size_t inlineCapacity, typename U, typename V>
inline typename ListHashSet<T, inlineCapacity, U, V>::const_iterator
ListHashSet<T, inlineCapacity, U, V>::find(ValuePeekInType value) const {
  ImplTypeConstIterator it = impl_.template Find<BaseTranslator>(value);
  if (it == impl_.end())
    return end();
  return MakeConstIterator(*it);
}

template <typename Translator>
struct ListHashSetTranslatorAdapter {
  STATIC_ONLY(ListHashSetTranslatorAdapter);
  template <typename T>
  static unsigned GetHash(const T& key) {
    return Translator::GetHash(key);
  }
  template <typename T, typename U>
  static bool Equal(const T& a, const U& b) {
    return Translator::Equal(a->value_, b);
  }
};

template <typename ValueType, size_t inlineCapacity, typename U, typename V>
template <typename HashTranslator, typename T>
inline typename ListHashSet<ValueType, inlineCapacity, U, V>::iterator
ListHashSet<ValueType, inlineCapacity, U, V>::Find(const T& value) {
  ImplTypeConstIterator it =
      impl_.template Find<ListHashSetTranslatorAdapter<HashTranslator>>(value);
  if (it == impl_.end())
    return end();
  return MakeIterator(*it);
}

template <typename ValueType, size_t inlineCapacity, typename U, typename V>
template <typename HashTranslator, typename T>
inline typename ListHashSet<ValueType, inlineCapacity, U, V>::const_iterator
ListHashSet<ValueType, inlineCapacity, U, V>::Find(const T& value) const {
  ImplTypeConstIterator it =
      impl_.template Find<ListHashSetTranslatorAdapter<HashTranslator>>(value);
  if (it == impl_.end())
    return end();
  return MakeConstIterator(*it);
}

template <typename ValueType, size_t inlineCapacity, typename U, typename V>
template <typename HashTranslator, typename T>
inline bool ListHashSet<ValueType, inlineCapacity, U, V>::Contains(
    const T& value) const {
  return impl_.template Contains<ListHashSetTranslatorAdapter<HashTranslator>>(
      value);
}

template <typename T, size_t inlineCapacity, typename U, typename V>
inline bool ListHashSet<T, inlineCapacity, U, V>::Contains(
    ValuePeekInType value) const {
  return impl_.template Contains<BaseTranslator>(value);
}

template <typename T, size_t inlineCapacity, typename U, typename V>
template <typename IncomingValueType>
typename ListHashSet<T, inlineCapacity, U, V>::AddResult
ListHashSet<T, inlineCapacity, U, V>::insert(IncomingValueType&& value) {
  CreateAllocatorIfNeeded();
  // The second argument is a const ref. This is useful for the HashTable
  // because it lets it take lvalues by reference, but for our purposes it's
  // inconvenient, since it constrains us to be const, whereas the allocator
  // actually changes when it does allocations.
  auto result = impl_.template insert<BaseTranslator>(
      std::forward<IncomingValueType>(value), *this->GetAllocator());
  if (result.is_new_entry)
    AppendNode(*result.stored_value);
  return AddResult(*result.stored_value, result.is_new_entry);
}

template <typename T, size_t inlineCapacity, typename U, typename V>
template <typename IncomingValueType>
typename ListHashSet<T, inlineCapacity, U, V>::AddResult
ListHashSet<T, inlineCapacity, U, V>::AppendOrMoveToLast(
    IncomingValueType&& value) {
  CreateAllocatorIfNeeded();
  auto result = impl_.template insert<BaseTranslator>(
      std::forward<IncomingValueType>(value), *this->GetAllocator());
  Node* node = *result.stored_value;
  if (!result.is_new_entry)
    Unlink(node);
  AppendNode(node);
  return AddResult(*result.stored_value, result.is_new_entry);
}

template <typename T, size_t inlineCapacity, typename U, typename V>
template <typename IncomingValueType>
typename ListHashSet<T, inlineCapacity, U, V>::AddResult
ListHashSet<T, inlineCapacity, U, V>::PrependOrMoveToFirst(
    IncomingValueType&& value) {
  CreateAllocatorIfNeeded();
  auto result = impl_.template insert<BaseTranslator>(
      std::forward<IncomingValueType>(value), *this->GetAllocator());
  Node* node = *result.stored_value;
  if (!result.is_new_entry)
    Unlink(node);
  PrependNode(node);
  return AddResult(*result.stored_value, result.is_new_entry);
}

template <typename T, size_t inlineCapacity, typename U, typename V>
template <typename IncomingValueType>
typename ListHashSet<T, inlineCapacity, U, V>::AddResult
ListHashSet<T, inlineCapacity, U, V>::InsertBefore(
    iterator it,
    IncomingValueType&& new_value) {
  CreateAllocatorIfNeeded();
  auto result = impl_.template insert<BaseTranslator>(
      std::forward<IncomingValueType>(new_value), *this->GetAllocator());
  if (result.is_new_entry)
    InsertNodeBefore(it.GetNode(), *result.stored_value);
  return AddResult(*result.stored_value, result.is_new_entry);
}

template <typename T, size_t inlineCapacity, typename U, typename V>
template <typename IncomingValueType>
typename ListHashSet<T, inlineCapacity, U, V>::AddResult
ListHashSet<T, inlineCapacity, U, V>::InsertBefore(
    ValuePeekInType before_value,
    IncomingValueType&& new_value) {
  CreateAllocatorIfNeeded();
  return InsertBefore(find(before_value),
                      std::forward<IncomingValueType>(new_value));
}

template <typename T, size_t inlineCapacity, typename U, typename V>
inline void ListHashSet<T, inlineCapacity, U, V>::erase(iterator it) {
  if (it == end())
    return;
  impl_.erase(it.GetNode());
  UnlinkAndDelete(it.GetNode());
}

template <typename T, size_t inlineCapacity, typename U, typename V>
inline void ListHashSet<T, inlineCapacity, U, V>::clear() {
  DeleteAllNodes();
  impl_.clear();
  head_ = nullptr;
  tail_ = nullptr;
}

template <typename T, size_t inlineCapacity, typename U, typename V>
auto ListHashSet<T, inlineCapacity, U, V>::Take(iterator it) -> ValueType {
  if (it == end())
    return ValueTraits::EmptyValue();

  impl_.erase(it.GetNode());
  ValueType result = std::move(it.GetNode()->value_);
  UnlinkAndDelete(it.GetNode());

  return result;
}

template <typename T, size_t inlineCapacity, typename U, typename V>
auto ListHashSet<T, inlineCapacity, U, V>::Take(ValuePeekInType value)
    -> ValueType {
  return Take(find(value));
}

template <typename T, size_t inlineCapacity, typename U, typename V>
auto ListHashSet<T, inlineCapacity, U, V>::TakeFirst() -> ValueType {
  DCHECK(!IsEmpty());
  impl_.erase(head_);
  ValueType result = std::move(head_->value_);
  UnlinkAndDelete(head_);

  return result;
}

template <typename T, size_t inlineCapacity, typename U, typename Allocator>
void ListHashSet<T, inlineCapacity, U, Allocator>::Unlink(Node* node) {
  if (!node->prev_) {
    DCHECK_EQ(node, head_);
    head_ = node->Next();
  } else {
    DCHECK_NE(node, head_);
    node->prev_->next_ = node->next_;
  }

  if (!node->next_) {
    DCHECK_EQ(node, tail_);
    tail_ = node->Prev();
  } else {
    DCHECK_NE(node, tail_);
    node->next_->prev_ = node->prev_;
  }
}

template <typename T, size_t inlineCapacity, typename U, typename V>
void ListHashSet<T, inlineCapacity, U, V>::UnlinkAndDelete(Node* node) {
  Unlink(node);
  node->Destroy(this->GetAllocator());
}

template <typename T, size_t inlineCapacity, typename U, typename V>
void ListHashSet<T, inlineCapacity, U, V>::AppendNode(Node* node) {
  node->prev_ = tail_;
  node->next_ = nullptr;

  if (tail_) {
    DCHECK(head_);
    tail_->next_ = node;
  } else {
    DCHECK(!head_);
    head_ = node;
  }

  tail_ = node;
}

template <typename T, size_t inlineCapacity, typename U, typename V>
void ListHashSet<T, inlineCapacity, U, V>::PrependNode(Node* node) {
  node->prev_ = nullptr;
  node->next_ = head_;

  if (head_)
    head_->prev_ = node;
  else
    tail_ = node;

  head_ = node;
}

template <typename T, size_t inlineCapacity, typename U, typename V>
void ListHashSet<T, inlineCapacity, U, V>::InsertNodeBefore(Node* before_node,
                                                            Node* new_node) {
  if (!before_node)
    return AppendNode(new_node);

  new_node->next_ = before_node;
  new_node->prev_ = before_node->prev_;
  if (before_node->prev_)
    before_node->prev_->next_ = new_node;
  before_node->prev_ = new_node;

  if (!new_node->prev_)
    head_ = new_node;
}

template <typename T, size_t inlineCapacity, typename U, typename V>
void ListHashSet<T, inlineCapacity, U, V>::DeleteAllNodes() {
  if (!head_)
    return;

  for (Node *node = head_, *next = head_->Next(); node;
       node = next, next = node ? node->Next() : nullptr)
    node->Destroy(this->GetAllocator());
}

template <typename T, size_t inlineCapacity, typename U, typename V>
template <typename VisitorDispatcher, typename A>
std::enable_if_t<A::kIsGarbageCollected>
ListHashSet<T, inlineCapacity, U, V>::Trace(VisitorDispatcher visitor) const {
  static_assert(!IsWeak<T>::value,
                "HeapListHashSet does not support weakness, consider using "
                "HeapLinkedHashSet instead.");
  impl_.Trace(visitor);
  visitor->Trace(head_);
  visitor->Trace(tail_);
}

}  // namespace WTF

using WTF::ListHashSet;

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_LIST_HASH_SET_H_
