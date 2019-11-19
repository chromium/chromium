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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_LINKED_HASH_SET_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_LINKED_HASH_SET_H_

#include "base/macros.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partition_allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/sanitizers.h"

namespace WTF {

// LinkedHashSet provides a Set interface like HashSet, but also has a
// predictable iteration order. It has O(1) insertion, removal, and test for
// containership. It maintains a linked list through its contents such that
// iterating it yields values in the order in which they were inserted.
//
// LinkedHashSet iterators are invalidated by mutation of the set. This means,
// for example, that you cannot modify the container while iterating
// over it (this will DCHECK in debug). Instead, you should either copy the
// entries to a vector before iterating, or maintain a separate list of pending
// updates.
//
// Unlike ListHashSet, this container supports WeakMember<T>.
template <typename Value,
          typename HashFunctions,
          typename HashTraits,
          typename Allocator>
class LinkedHashSet;

template <typename LinkedHashSet>
class LinkedHashSetIterator;
template <typename LinkedHashSet>
class LinkedHashSetConstIterator;
template <typename LinkedHashSet>
class LinkedHashSetReverseIterator;
template <typename LinkedHashSet>
class LinkedHashSetConstReverseIterator;

template <typename Value, typename HashFunctions, typename Allocator>
struct LinkedHashSetTranslator;
template <typename Value, typename Allocator>
struct LinkedHashSetExtractor;
template <typename Value, typename ValueTraits, typename Allocator>
struct LinkedHashSetTraits;

class LinkedHashSetNodeBase {
  DISALLOW_NEW();

 public:
  LinkedHashSetNodeBase() : prev_(this), next_(this) {}

  NO_SANITIZE_ADDRESS
  void Unlink() {
    if (!next_)
      return;
    DCHECK(prev_);
    DCHECK(next_->prev_ == this);
    DCHECK(prev_->next_ == this);
    next_->prev_ = prev_;
    prev_->next_ = next_;
  }

  ~LinkedHashSetNodeBase() { Unlink(); }

  void InsertBefore(LinkedHashSetNodeBase& other) {
    other.next_ = this;
    other.prev_ = prev_;
    prev_->next_ = &other;
    prev_ = &other;
    DCHECK(other.next_);
    DCHECK(other.prev_);
    DCHECK(next_);
    DCHECK(prev_);
  }

  void InsertAfter(LinkedHashSetNodeBase& other) {
    other.prev_ = this;
    other.next_ = next_;
    next_->prev_ = &other;
    next_ = &other;
    DCHECK(other.next_);
    DCHECK(other.prev_);
    DCHECK(next_);
    DCHECK(prev_);
  }

  LinkedHashSetNodeBase(LinkedHashSetNodeBase* prev,
                        LinkedHashSetNodeBase* next)
      : prev_(prev), next_(next) {
    DCHECK((prev && next) || (!prev && !next));
  }

  LinkedHashSetNodeBase* prev_;
  LinkedHashSetNodeBase* next_;

 protected:
  // If we take a copy of a node we can't copy the next and prev pointers,
  // since they point to something that does not point at us. This is used
  // inside the shouldExpand() "if" in HashTable::add.
  LinkedHashSetNodeBase(const LinkedHashSetNodeBase& other)
      : prev_(nullptr), next_(nullptr) {}

  LinkedHashSetNodeBase(LinkedHashSetNodeBase&& other)
      : prev_(other.prev_), next_(other.next_) {
    other.prev_ = nullptr;
    other.next_ = nullptr;
    if (next_) {
      prev_->next_ = this;
      next_->prev_ = this;
    }
  }

 private:
  // Should not be used.
  LinkedHashSetNodeBase& operator=(const LinkedHashSetNodeBase& other) = delete;
};

template <typename ValueArg>
class LinkedHashSetNode : public LinkedHashSetNodeBase {
  DISALLOW_NEW();

 public:
  LinkedHashSetNode(const ValueArg& value,
                    LinkedHashSetNodeBase* prev,
                    LinkedHashSetNodeBase* next)
      : LinkedHashSetNodeBase(prev, next), value_(value) {}

  LinkedHashSetNode(ValueArg&& value,
                    LinkedHashSetNodeBase* prev,
                    LinkedHashSetNodeBase* next)
      : LinkedHashSetNodeBase(prev, next), value_(std::move(value)) {}

  LinkedHashSetNode(LinkedHashSetNode&& other)
      : LinkedHashSetNodeBase(std::move(other)),
        value_(std::move(other.value_)) {}

  ValueArg value_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LinkedHashSetNode);
};

template <typename T>
struct IsWeak<LinkedHashSetNode<T>>
    : std::integral_constant<bool, IsWeak<T>::value> {};

template <typename ValueArg,
          typename HashFunctions = typename DefaultHash<ValueArg>::Hash,
          typename TraitsArg = HashTraits<ValueArg>,
          typename Allocator = PartitionAllocator>
class LinkedHashSet {
  USE_ALLOCATOR(LinkedHashSet, Allocator);

 private:
  typedef ValueArg Value;
  typedef TraitsArg Traits;
  typedef LinkedHashSetNode<Value> Node;
  typedef LinkedHashSetNodeBase NodeBase;
  typedef LinkedHashSetTranslator<Value, HashFunctions, Allocator>
      NodeHashFunctions;
  typedef LinkedHashSetTraits<Value, Traits, Allocator> NodeHashTraits;

  typedef HashTable<Node,
                    Node,
                    IdentityExtractor,
                    NodeHashFunctions,
                    NodeHashTraits,
                    NodeHashTraits,
                    Allocator>
      ImplType;

 public:
  typedef LinkedHashSetIterator<LinkedHashSet> iterator;
  friend class LinkedHashSetIterator<LinkedHashSet>;
  typedef LinkedHashSetConstIterator<LinkedHashSet> const_iterator;
  friend class LinkedHashSetConstIterator<LinkedHashSet>;

  typedef LinkedHashSetReverseIterator<LinkedHashSet> reverse_iterator;
  friend class LinkedHashSetReverseIterator<LinkedHashSet>;
  typedef LinkedHashSetConstReverseIterator<LinkedHashSet>
      const_reverse_iterator;
  friend class LinkedHashSetConstReverseIterator<LinkedHashSet>;

  struct AddResult final {
    STACK_ALLOCATED();

   public:
    AddResult(const typename ImplType::AddResult& hash_table_add_result)
        : stored_value(&hash_table_add_result.stored_value->value_),
          is_new_entry(hash_table_add_result.is_new_entry) {}

    Value* stored_value;
    bool is_new_entry;
  };

  typedef typename HashTraits<Value>::PeekInType ValuePeekInType;

  LinkedHashSet();
  LinkedHashSet(const LinkedHashSet&);
  LinkedHashSet(LinkedHashSet&&);
  LinkedHashSet& operator=(const LinkedHashSet&);
  LinkedHashSet& operator=(LinkedHashSet&&);

  // Needs finalization. The anchor needs to unlink itself from the chain.
  ~LinkedHashSet();

  static void Finalize(void* pointer) {
    reinterpret_cast<LinkedHashSet*>(pointer)->~LinkedHashSet();
  }
  void FinalizeGarbageCollectedObject() { Finalize(this); }

  void Swap(LinkedHashSet&);

  unsigned size() const { return impl_.size(); }
  unsigned Capacity() const { return impl_.Capacity(); }
  bool IsEmpty() const { return impl_.IsEmpty(); }

  iterator begin() { return MakeIterator(FirstNode()); }
  iterator end() { return MakeIterator(Anchor()); }
  const_iterator begin() const { return MakeConstIterator(FirstNode()); }
  const_iterator end() const { return MakeConstIterator(Anchor()); }

  reverse_iterator rbegin() { return MakeReverseIterator(LastNode()); }
  reverse_iterator rend() { return MakeReverseIterator(Anchor()); }
  const_reverse_iterator rbegin() const {
    return MakeConstReverseIterator(LastNode());
  }
  const_reverse_iterator rend() const {
    return MakeConstReverseIterator(Anchor());
  }

  Value& front();
  const Value& front() const;
  void RemoveFirst();

  Value& back();
  const Value& back() const;
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

  // The return value of insert is a pair of a pointer to the stored value,
  // and a bool that is true if an new entry was added.
  template <typename IncomingValueType>
  AddResult insert(IncomingValueType&&);

  // Same as insert() except that the return value is an
  // iterator. Useful in cases where it's needed to have the
  // same return value as find() and where it's not possible to
  // use a pointer to the storedValue.
  template <typename IncomingValueType>
  iterator AddReturnIterator(IncomingValueType&&);

  // Add the value to the end of the collection. If the value was already in
  // the list, it is moved to the end.
  template <typename IncomingValueType>
  AddResult AppendOrMoveToLast(IncomingValueType&&);

  // Add the value to the beginning of the collection. If the value was already
  // in the list, it is moved to the beginning.
  template <typename IncomingValueType>
  AddResult PrependOrMoveToFirst(IncomingValueType&&);

  template <typename IncomingValueType>
  AddResult InsertBefore(ValuePeekInType before_value,
                         IncomingValueType&& new_value);
  template <typename IncomingValueType>
  AddResult InsertBefore(iterator it, IncomingValueType&& new_value) {
    return impl_.template insert<NodeHashFunctions>(
        std::forward<IncomingValueType>(new_value), it.GetNode());
  }

  void erase(ValuePeekInType);
  void erase(iterator);
  void clear() { impl_.clear(); }
  template <typename Collection>
  void RemoveAll(const Collection& other) {
    WTF::RemoveAll(*this, other);
  }

  template <typename VisitorDispatcher>
  void Trace(VisitorDispatcher visitor) {
    impl_.Trace(visitor);
    // Should the underlying table be moved by GC, register a callback
    // that fixes up the interior pointers that the (Heap)LinkedHashSet keeps.
    if (impl_.table_) {
      Allocator::RegisterBackingStoreCallback(
          visitor, impl_.table_,
          NodeHashTraits::template MoveBackingCallback<ImplType>);
    }
  }

  int64_t Modifications() const { return impl_.Modifications(); }
  void CheckModifications(int64_t mods) const {
    impl_.CheckModifications(mods);
  }

 protected:
  typename ImplType::ValueType** GetBufferSlot() {
    return impl_.GetBufferSlot();
  }

 private:
  Node* Anchor() { return reinterpret_cast<Node*>(&anchor_); }
  const Node* Anchor() const { return reinterpret_cast<const Node*>(&anchor_); }
  Node* FirstNode() { return reinterpret_cast<Node*>(anchor_.next_); }
  const Node* FirstNode() const {
    return reinterpret_cast<const Node*>(anchor_.next_);
  }
  Node* LastNode() { return reinterpret_cast<Node*>(anchor_.prev_); }
  const Node* LastNode() const {
    return reinterpret_cast<const Node*>(anchor_.prev_);
  }

  iterator MakeIterator(const Node* position) {
    return iterator(position, this);
  }
  const_iterator MakeConstIterator(const Node* position) const {
    return const_iterator(position, this);
  }
  reverse_iterator MakeReverseIterator(const Node* position) {
    return reverse_iterator(position, this);
  }
  const_reverse_iterator MakeConstReverseIterator(const Node* position) const {
    return const_reverse_iterator(position, this);
  }

  ImplType impl_;
  NodeBase anchor_;
};

template <typename Value, typename HashFunctions, typename Allocator>
struct LinkedHashSetTranslator {
  STATIC_ONLY(LinkedHashSetTranslator);
  typedef LinkedHashSetNode<Value> Node;
  typedef LinkedHashSetNodeBase NodeBase;
  typedef typename HashTraits<Value>::PeekInType ValuePeekInType;
  static unsigned GetHash(const Node& node) {
    return HashFunctions::GetHash(node.value_);
  }
  static unsigned GetHash(const ValuePeekInType& key) {
    return HashFunctions::GetHash(key);
  }
  static bool Equal(const Node& a, const ValuePeekInType& b) {
    return HashFunctions::Equal(a.value_, b);
  }
  static bool Equal(const Node& a, const Node& b) {
    return HashFunctions::Equal(a.value_, b.value_);
  }
  template <typename IncomingValueType>
  static void Translate(Node& location,
                        IncomingValueType&& key,
                        NodeBase* anchor) {
    anchor->InsertBefore(location);
    location.value_ = std::forward<IncomingValueType>(key);
  }

  // Empty (or deleted) slots have the next_ pointer set to null, but we
  // don't do anything to the other fields, which may contain junk.
  // Therefore you can't compare a newly constructed empty value with a
  // slot and get the right answer.
  static const bool safe_to_compare_to_empty_or_deleted = false;
};

template <typename Value, typename Allocator>
struct LinkedHashSetExtractor {
  STATIC_ONLY(LinkedHashSetExtractor);
  static const Value& Extract(const LinkedHashSetNode<Value>& node) {
    return node.value_;
  }
};

template <typename Value, typename ValueTraitsArg, typename Allocator>
struct LinkedHashSetTraits
    : public SimpleClassHashTraits<LinkedHashSetNode<Value>> {
  STATIC_ONLY(LinkedHashSetTraits);
  using Node = LinkedHashSetNode<Value>;
  using NodeBase = LinkedHashSetNodeBase;
  typedef ValueTraitsArg ValueTraits;

  // The slot is empty when the next_ field is zero so it's safe to zero
  // the backing.
  static const bool kEmptyValueIsZero = ValueTraits::kEmptyValueIsZero;

  static const bool kHasIsEmptyValueFunction = true;
  static bool IsEmptyValue(const Node& node) { return !node.next_; }
  static Node EmptyValue() {
    return Node(ValueTraits::EmptyValue(), nullptr, nullptr);
  }

  static const int kDeletedValue = -1;

  static void ConstructDeletedValue(Node& slot, bool) {
    slot.next_ = reinterpret_cast<Node*>(kDeletedValue);
  }
  static bool IsDeletedValue(const Node& slot) {
    return slot.next_ == reinterpret_cast<Node*>(kDeletedValue);
  }

  // Whether we need to trace and do weak processing depends on the traits of
  // the type inside the node.
  template <typename U = void>
  struct IsTraceableInCollection {
    STATIC_ONLY(IsTraceableInCollection);
    static const bool value =
        ValueTraits::template IsTraceableInCollection<>::value;
  };

  static constexpr bool kHasMovingCallback = true;

  template <typename HashTable, typename Visitor>
  static void RegisterMovingCallback(Visitor* visitor,
                                     typename HashTable::ValueType* allocated) {
    Allocator::RegisterBackingStoreCallback(visitor, allocated,
                                            MoveBackingCallback<HashTable>);
  }

  template <typename HashTable>
  static void MoveBackingCallback(void* from, void* to, size_t size) {
    // Note: the hash table move may have been overlapping; linearly scan the
    // entire table and fixup interior pointers into the old region with
    // correspondingly offset ones into the new.
    const size_t table_size = size / sizeof(Node);
    Node* table = reinterpret_cast<Node*>(to);
    NodeBase* from_start = reinterpret_cast<NodeBase*>(from);
    NodeBase* from_end =
        reinterpret_cast<NodeBase*>(reinterpret_cast<uintptr_t>(from) + size);
    NodeBase* anchor_node = nullptr;
    for (Node* element = table + table_size - 1; element >= table; element--) {
      Node& node = *element;
      if (HashTable::IsEmptyOrDeletedBucket(node))
        continue;
      if (node.next_ >= from_start && node.next_ < from_end) {
        const size_t diff = reinterpret_cast<uintptr_t>(node.next_) -
                            reinterpret_cast<uintptr_t>(from);
        node.next_ =
            reinterpret_cast<NodeBase*>(reinterpret_cast<uintptr_t>(to) + diff);
      } else {
        DCHECK(!anchor_node || node.next_ == anchor_node);
        anchor_node = node.next_;
      }
      if (node.prev_ >= from_start && node.prev_ < from_end) {
        const size_t diff = reinterpret_cast<uintptr_t>(node.prev_) -
                            reinterpret_cast<uintptr_t>(from);
        node.prev_ =
            reinterpret_cast<NodeBase*>(reinterpret_cast<uintptr_t>(to) + diff);
      } else {
        DCHECK(!anchor_node || node.prev_ == anchor_node);
        anchor_node = node.prev_;
      }
    }
    // During incremental marking, HeapLinkedHashSet object may be marked, but
    // later the mutator can destroy it. The compaction code will execute this
    // callback, but the anchor will have already been unlinked.
    if (!anchor_node) {
      return;
    }
    {
      DCHECK(anchor_node->prev_ >= from_start && anchor_node->prev_ < from_end);
      const size_t diff = reinterpret_cast<uintptr_t>(anchor_node->prev_) -
                          reinterpret_cast<uintptr_t>(from);
      anchor_node->prev_ =
          reinterpret_cast<NodeBase*>(reinterpret_cast<uintptr_t>(to) + diff);
    }
    {
      DCHECK(anchor_node->next_ >= from_start && anchor_node->next_ < from_end);
      const size_t diff = reinterpret_cast<uintptr_t>(anchor_node->next_) -
                          reinterpret_cast<uintptr_t>(from);
      anchor_node->next_ =
          reinterpret_cast<NodeBase*>(reinterpret_cast<uintptr_t>(to) + diff);
    }
  }
};

template <typename LinkedHashSetType>
class LinkedHashSetIterator {
  DISALLOW_NEW();

 private:
  typedef typename LinkedHashSetType::Node Node;
  typedef typename LinkedHashSetType::Traits Traits;

  typedef typename LinkedHashSetType::Value& ReferenceType;
  typedef typename LinkedHashSetType::Value* PointerType;

  typedef LinkedHashSetConstIterator<LinkedHashSetType> const_iterator;

  Node* GetNode() { return const_cast<Node*>(iterator_.GetNode()); }

 protected:
  LinkedHashSetIterator(const Node* position, LinkedHashSetType* container)
      : iterator_(position, container) {}

 public:
  // Default copy, assignment and destructor are OK.

  PointerType Get() const { return const_cast<PointerType>(iterator_.Get()); }
  ReferenceType operator*() const { return *Get(); }
  PointerType operator->() const { return Get(); }

  LinkedHashSetIterator& operator++() {
    ++iterator_;
    return *this;
  }
  LinkedHashSetIterator& operator--() {
    --iterator_;
    return *this;
  }

  // Postfix ++ and -- intentionally omitted.

  // Comparison.
  bool operator==(const LinkedHashSetIterator& other) const {
    return iterator_ == other.iterator_;
  }
  bool operator!=(const LinkedHashSetIterator& other) const {
    return iterator_ != other.iterator_;
  }

  operator const_iterator() const { return iterator_; }

 protected:
  const_iterator iterator_;
  template <typename T, typename U, typename V, typename W>
  friend class LinkedHashSet;
};

template <typename LinkedHashSetType>
class LinkedHashSetConstIterator {
  DISALLOW_NEW();

 private:
  typedef typename LinkedHashSetType::Node Node;
  typedef typename LinkedHashSetType::Traits Traits;

  typedef const typename LinkedHashSetType::Value& ReferenceType;
  typedef const typename LinkedHashSetType::Value* PointerType;

  const Node* GetNode() const { return static_cast<const Node*>(position_); }

 protected:
  LinkedHashSetConstIterator(const LinkedHashSetNodeBase* position,
                             const LinkedHashSetType* container)
      : position_(position)
#if DCHECK_IS_ON()
        ,
        container_(container),
        container_modifications_(container->Modifications())
#endif
  {
  }

 public:
  PointerType Get() const {
    CheckModifications();
    return &static_cast<const Node*>(position_)->value_;
  }
  ReferenceType operator*() const { return *Get(); }
  PointerType operator->() const { return Get(); }

  LinkedHashSetConstIterator& operator++() {
    DCHECK(position_);
    CheckModifications();
    position_ = position_->next_;
    return *this;
  }

  LinkedHashSetConstIterator& operator--() {
    DCHECK(position_);
    CheckModifications();
    position_ = position_->prev_;
    return *this;
  }

  // Postfix ++ and -- intentionally omitted.

  // Comparison.
  bool operator==(const LinkedHashSetConstIterator& other) const {
    return position_ == other.position_;
  }
  bool operator!=(const LinkedHashSetConstIterator& other) const {
    return position_ != other.position_;
  }

 private:
  const LinkedHashSetNodeBase* position_;
#if DCHECK_IS_ON()
  void CheckModifications() const {
    container_->CheckModifications(container_modifications_);
  }
  const LinkedHashSetType* container_;
  int64_t container_modifications_;
#else
  void CheckModifications() const {}
#endif
  template <typename T, typename U, typename V, typename W>
  friend class LinkedHashSet;
  friend class LinkedHashSetIterator<LinkedHashSetType>;
};

template <typename LinkedHashSetType>
class LinkedHashSetReverseIterator
    : public LinkedHashSetIterator<LinkedHashSetType> {
  typedef LinkedHashSetReverseIterator<LinkedHashSetType> reverse_iterator;
  typedef LinkedHashSetIterator<LinkedHashSetType> Superclass;
  typedef LinkedHashSetConstReverseIterator<LinkedHashSetType>
      const_reverse_iterator;
  typedef typename LinkedHashSetType::Node Node;

 protected:
  LinkedHashSetReverseIterator(const Node* position,
                               LinkedHashSetType* container)
      : Superclass(position, container) {}

 public:
  LinkedHashSetReverseIterator& operator++() {
    Superclass::operator--();
    return *this;
  }
  LinkedHashSetReverseIterator& operator--() {
    Superclass::operator++();
    return *this;
  }

  // Postfix ++ and -- intentionally omitted.

  operator const_reverse_iterator() const {
    return *reinterpret_cast<const_reverse_iterator*>(
        const_cast<reverse_iterator*>(this));
  }

  template <typename T, typename U, typename V, typename W>
  friend class LinkedHashSet;
};

template <typename LinkedHashSetType>
class LinkedHashSetConstReverseIterator
    : public LinkedHashSetConstIterator<LinkedHashSetType> {
  typedef LinkedHashSetConstIterator<LinkedHashSetType> Superclass;
  typedef typename LinkedHashSetType::Node Node;

 public:
  LinkedHashSetConstReverseIterator(const Node* position,
                                    const LinkedHashSetType* container)
      : Superclass(position, container) {}

  LinkedHashSetConstReverseIterator& operator++() {
    Superclass::operator--();
    return *this;
  }
  LinkedHashSetConstReverseIterator& operator--() {
    Superclass::operator++();
    return *this;
  }

  // Postfix ++ and -- intentionally omitted.

  template <typename T, typename U, typename V, typename W>
  friend class LinkedHashSet;
};

inline void SwapAnchor(LinkedHashSetNodeBase& a, LinkedHashSetNodeBase& b) {
  DCHECK(a.prev_);
  DCHECK(a.next_);
  DCHECK(b.prev_);
  DCHECK(b.next_);
  swap(a.prev_, b.prev_);
  swap(a.next_, b.next_);
  if (b.next_ == &a) {
    DCHECK_EQ(b.prev_, &a);
    b.next_ = &b;
    b.prev_ = &b;
  } else {
    b.next_->prev_ = &b;
    b.prev_->next_ = &b;
  }
  if (a.next_ == &b) {
    DCHECK_EQ(a.prev_, &b);
    a.next_ = &a;
    a.prev_ = &a;
  } else {
    a.next_->prev_ = &a;
    a.prev_->next_ = &a;
  }
}

inline void swap(LinkedHashSetNodeBase& a, LinkedHashSetNodeBase& b) {
  DCHECK_NE(a.next_, &a);
  DCHECK_NE(b.next_, &b);
  swap(a.prev_, b.prev_);
  swap(a.next_, b.next_);
  if (b.next_) {
    b.next_->prev_ = &b;
    b.prev_->next_ = &b;
  }
  if (a.next_) {
    a.next_->prev_ = &a;
    a.prev_->next_ = &a;
  }
}

template <typename T, typename U, typename V, typename Allocator>
inline LinkedHashSet<T, U, V, Allocator>::LinkedHashSet() {
  static_assert(
      Allocator::kIsGarbageCollected ||
          !IsPointerToGarbageCollectedType<T>::value,
      "Cannot put raw pointers to garbage-collected classes into "
      "an off-heap LinkedHashSet. Use HeapLinkedHashSet<Member<T>> instead.");
}

template <typename T, typename U, typename V, typename W>
inline LinkedHashSet<T, U, V, W>::LinkedHashSet(const LinkedHashSet& other)
    : anchor_() {
  const_iterator end = other.end();
  for (const_iterator it = other.begin(); it != end; ++it)
    insert(*it);
}

template <typename T, typename U, typename V, typename W>
inline LinkedHashSet<T, U, V, W>::LinkedHashSet(LinkedHashSet&& other)
    : anchor_() {
  Swap(other);
}

template <typename T, typename U, typename V, typename W>
inline LinkedHashSet<T, U, V, W>& LinkedHashSet<T, U, V, W>::operator=(
    const LinkedHashSet& other) {
  LinkedHashSet tmp(other);
  Swap(tmp);
  return *this;
}

template <typename T, typename U, typename V, typename W>
inline LinkedHashSet<T, U, V, W>& LinkedHashSet<T, U, V, W>::operator=(
    LinkedHashSet&& other) {
  Swap(other);
  return *this;
}

template <typename T, typename U, typename V, typename W>
inline void LinkedHashSet<T, U, V, W>::Swap(LinkedHashSet& other) {
  impl_.swap(other.impl_);
  SwapAnchor(anchor_, other.anchor_);
}

template <typename T, typename U, typename V, typename Allocator>
inline LinkedHashSet<T, U, V, Allocator>::~LinkedHashSet() {
  // The destructor of anchor_ will implicitly be called here, which will
  // unlink the anchor from the collection.
}

template <typename T, typename U, typename V, typename W>
inline T& LinkedHashSet<T, U, V, W>::front() {
  DCHECK(!IsEmpty());
  return FirstNode()->value_;
}

template <typename T, typename U, typename V, typename W>
inline const T& LinkedHashSet<T, U, V, W>::front() const {
  DCHECK(!IsEmpty());
  return FirstNode()->value_;
}

template <typename T, typename U, typename V, typename W>
inline void LinkedHashSet<T, U, V, W>::RemoveFirst() {
  DCHECK(!IsEmpty());
  impl_.erase(static_cast<Node*>(anchor_.next_));
}

template <typename T, typename U, typename V, typename W>
inline T& LinkedHashSet<T, U, V, W>::back() {
  DCHECK(!IsEmpty());
  return LastNode()->value_;
}

template <typename T, typename U, typename V, typename W>
inline const T& LinkedHashSet<T, U, V, W>::back() const {
  DCHECK(!IsEmpty());
  return LastNode()->value_;
}

template <typename T, typename U, typename V, typename W>
inline void LinkedHashSet<T, U, V, W>::pop_back() {
  DCHECK(!IsEmpty());
  impl_.erase(static_cast<Node*>(anchor_.prev_));
}

template <typename T, typename U, typename V, typename W>
inline typename LinkedHashSet<T, U, V, W>::iterator
LinkedHashSet<T, U, V, W>::find(ValuePeekInType value) {
  LinkedHashSet::Node* node =
      impl_.template Lookup<LinkedHashSet::NodeHashFunctions, ValuePeekInType>(
          value);
  if (!node)
    return end();
  return MakeIterator(node);
}

template <typename T, typename U, typename V, typename W>
inline typename LinkedHashSet<T, U, V, W>::const_iterator
LinkedHashSet<T, U, V, W>::find(ValuePeekInType value) const {
  const LinkedHashSet::Node* node =
      impl_.template Lookup<LinkedHashSet::NodeHashFunctions, ValuePeekInType>(
          value);
  if (!node)
    return end();
  return MakeConstIterator(node);
}

template <typename Translator>
struct LinkedHashSetTranslatorAdapter {
  STATIC_ONLY(LinkedHashSetTranslatorAdapter);
  template <typename T>
  static unsigned GetHash(const T& key) {
    return Translator::GetHash(key);
  }
  template <typename T, typename U>
  static bool Equal(const T& a, const U& b) {
    return Translator::Equal(a.value_, b);
  }
};

template <typename Value, typename U, typename V, typename W>
template <typename HashTranslator, typename T>
inline typename LinkedHashSet<Value, U, V, W>::iterator
LinkedHashSet<Value, U, V, W>::Find(const T& value) {
  typedef LinkedHashSetTranslatorAdapter<HashTranslator> TranslatedFunctions;
  const LinkedHashSet::Node* node =
      impl_.template Lookup<TranslatedFunctions, const T&>(value);
  if (!node)
    return end();
  return MakeIterator(node);
}

template <typename Value, typename U, typename V, typename W>
template <typename HashTranslator, typename T>
inline typename LinkedHashSet<Value, U, V, W>::const_iterator
LinkedHashSet<Value, U, V, W>::Find(const T& value) const {
  typedef LinkedHashSetTranslatorAdapter<HashTranslator> TranslatedFunctions;
  const LinkedHashSet::Node* node =
      impl_.template Lookup<TranslatedFunctions, const T&>(value);
  if (!node)
    return end();
  return MakeConstIterator(node);
}

template <typename Value, typename U, typename V, typename W>
template <typename HashTranslator, typename T>
inline bool LinkedHashSet<Value, U, V, W>::Contains(const T& value) const {
  return impl_
      .template Contains<LinkedHashSetTranslatorAdapter<HashTranslator>>(value);
}

template <typename T, typename U, typename V, typename W>
inline bool LinkedHashSet<T, U, V, W>::Contains(ValuePeekInType value) const {
  return impl_.template Contains<NodeHashFunctions>(value);
}

template <typename Value,
          typename HashFunctions,
          typename Traits,
          typename Allocator>
template <typename IncomingValueType>
typename LinkedHashSet<Value, HashFunctions, Traits, Allocator>::AddResult
LinkedHashSet<Value, HashFunctions, Traits, Allocator>::insert(
    IncomingValueType&& value) {
  return impl_.template insert<NodeHashFunctions>(
      std::forward<IncomingValueType>(value), &anchor_);
}

template <typename T, typename U, typename V, typename W>
template <typename IncomingValueType>
typename LinkedHashSet<T, U, V, W>::iterator
LinkedHashSet<T, U, V, W>::AddReturnIterator(IncomingValueType&& value) {
  typename ImplType::AddResult result =
      impl_.template insert<NodeHashFunctions>(
          std::forward<IncomingValueType>(value), &anchor_);
  return MakeIterator(result.stored_value);
}

template <typename T, typename U, typename V, typename W>
template <typename IncomingValueType>
typename LinkedHashSet<T, U, V, W>::AddResult
LinkedHashSet<T, U, V, W>::AppendOrMoveToLast(IncomingValueType&& value) {
  typename ImplType::AddResult result =
      impl_.template insert<NodeHashFunctions>(
          std::forward<IncomingValueType>(value), &anchor_);
  Node* node = result.stored_value;
  if (!result.is_new_entry) {
    node->Unlink();
    anchor_.InsertBefore(*node);
  }
  return result;
}

template <typename T, typename U, typename V, typename W>
template <typename IncomingValueType>
typename LinkedHashSet<T, U, V, W>::AddResult
LinkedHashSet<T, U, V, W>::PrependOrMoveToFirst(IncomingValueType&& value) {
  typename ImplType::AddResult result =
      impl_.template insert<NodeHashFunctions>(
          std::forward<IncomingValueType>(value), anchor_.next_);
  Node* node = result.stored_value;
  if (!result.is_new_entry) {
    node->Unlink();
    anchor_.InsertAfter(*node);
  }
  return result;
}

template <typename T, typename U, typename V, typename W>
template <typename IncomingValueType>
typename LinkedHashSet<T, U, V, W>::AddResult
LinkedHashSet<T, U, V, W>::InsertBefore(ValuePeekInType before_value,
                                        IncomingValueType&& new_value) {
  return InsertBefore(find(before_value),
                      std::forward<IncomingValueType>(new_value));
}

template <typename T, typename U, typename V, typename W>
inline void LinkedHashSet<T, U, V, W>::erase(iterator it) {
  if (it == end())
    return;
  impl_.erase(it.GetNode());
}

template <typename T, typename U, typename V, typename W>
inline void LinkedHashSet<T, U, V, W>::erase(ValuePeekInType value) {
  erase(find(value));
}

template <typename T, typename Allocator>
inline void swap(LinkedHashSetNode<T>& a, LinkedHashSetNode<T>& b) {
  typedef LinkedHashSetNodeBase Base;
  // The key and value cannot be swapped atomically, and it would be
  // wrong to have a GC when only one was swapped and the other still
  // contained garbage (eg. from a previous use of the same slot).
  // Therefore we forbid a GC until both the key and the value are
  // swapped.
  Allocator::EnterGCForbiddenScope();
  swap(static_cast<Base&>(a), static_cast<Base&>(b));
  swap(a.value_, b.value_);
  Allocator::LeaveGCForbiddenScope();
}

}  // namespace WTF

using WTF::LinkedHashSet;

#endif /* WTF_LinkedHashSet_h */
