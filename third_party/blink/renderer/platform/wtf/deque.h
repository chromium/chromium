/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_DEQUE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_DEQUE_H_

// FIXME: Could move what Vector and Deque share into a separate file.
// Deque doesn't actually use Vector.

#include <iterator>

#include "base/macros.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/construct_traits.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace WTF {

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
class DequeIteratorBase;
template <typename T, wtf_size_t inlineCapacity, typename Allocator>
class DequeIterator;
template <typename T, wtf_size_t inlineCapacity, typename Allocator>
class DequeConstIterator;

template <typename T,
          wtf_size_t inlineCapacity = 0,
          typename Allocator = PartitionAllocator>
class Deque : public ConditionalDestructor<Deque<T, INLINE_CAPACITY, Allocator>,
                                           (INLINE_CAPACITY == 0) &&
                                               Allocator::kIsGarbageCollected> {
  USE_ALLOCATOR(Deque, Allocator);

 public:
  typedef DequeIterator<T, inlineCapacity, Allocator> iterator;
  typedef DequeConstIterator<T, inlineCapacity, Allocator> const_iterator;
  typedef std::reverse_iterator<iterator> reverse_iterator;
  typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

  Deque();
  Deque(const Deque&);
  Deque& operator=(const Deque&);
  Deque(Deque&&);
  Deque& operator=(Deque&&);

  void Finalize();

  void Swap(Deque&);

  wtf_size_t size() const {
    return start_ <= end_ ? end_ - start_ : end_ + buffer_.capacity() - start_;
  }
  bool IsEmpty() const { return start_ == end_; }

  iterator begin() { return iterator(this, start_); }
  iterator end() { return iterator(this, end_); }
  const_iterator begin() const { return const_iterator(this, start_); }
  const_iterator end() const { return const_iterator(this, end_); }
  reverse_iterator rbegin() { return reverse_iterator(end()); }
  reverse_iterator rend() { return reverse_iterator(begin()); }
  const_reverse_iterator rbegin() const {
    return const_reverse_iterator(end());
  }
  const_reverse_iterator rend() const {
    return const_reverse_iterator(begin());
  }

  T& front() {
    DCHECK_NE(start_, end_);
    return buffer_.Buffer()[start_];
  }
  const T& front() const {
    DCHECK_NE(start_, end_);
    return buffer_.Buffer()[start_];
  }
  T TakeFirst();

  T& back() {
    DCHECK_NE(start_, end_);
    return *(--end());
  }
  const T& back() const {
    DCHECK_NE(start_, end_);
    return *(--end());
  }
  T TakeLast();

  T& at(wtf_size_t i) {
    CHECK_LT(i, size());
    wtf_size_t right = buffer_.capacity() - start_;
    return i < right ? buffer_.Buffer()[start_ + i]
                     : buffer_.Buffer()[i - right];
  }
  const T& at(wtf_size_t i) const {
    CHECK_LT(i, size());
    wtf_size_t right = buffer_.capacity() - start_;
    return i < right ? buffer_.Buffer()[start_ + i]
                     : buffer_.Buffer()[i - right];
  }

  T& operator[](wtf_size_t i) { return at(i); }
  const T& operator[](wtf_size_t i) const { return at(i); }

  template <typename U>
  void push_front(U&&);
  void erase(iterator&);
  void erase(const_iterator&);

  // STL compatibility.
  template <typename U>
  void push_back(U&&);
  void pop_back();
  void pop_front();
  bool empty() const { return IsEmpty(); }
  template <typename... Args>
  void emplace_back(Args&&...);
  template <typename... Args>
  void emplace_front(Args&&...);

  void clear();

  template <typename VisitorDispatcher, typename A = Allocator>
  std::enable_if_t<A::kIsGarbageCollected> Trace(VisitorDispatcher);

  static_assert(!std::is_polymorphic<T>::value ||
                    !VectorTraits<T>::kCanInitializeWithMemset,
                "Cannot initialize with memset if there is a vtable");
  static_assert(Allocator::kIsGarbageCollected || !IsDisallowNew<T>::value ||
                    !IsTraceable<T>::value,
                "Cannot put DISALLOW_NEW objects that "
                "have trace methods into an off-heap Deque");
  static_assert(Allocator::kIsGarbageCollected ||
                    !IsPointerToGarbageCollectedType<T>::value,
                "Cannot put raw pointers to garbage-collected classes into a "
                "Deque. Use HeapDeque<Member<T>> instead.");

 protected:
  T** GetBufferSlot() { return buffer_.BufferSlot(); }

 private:
  friend class DequeIteratorBase<T, inlineCapacity, Allocator>;

  class BackingBuffer : public VectorBuffer<T, INLINE_CAPACITY, Allocator> {
   private:
    using Base = VectorBuffer<T, INLINE_CAPACITY, Allocator>;
    using Base::size_;

   public:
    BackingBuffer() : Base() {}
    explicit BackingBuffer(wtf_size_t capacity) : Base(capacity) {}

    void SetSize(wtf_size_t size) { size_ = size; }

    DISALLOW_COPY_AND_ASSIGN(BackingBuffer);
  };

  typedef VectorTypeOperations<T, Allocator> TypeOperations;
  typedef DequeIteratorBase<T, inlineCapacity, Allocator> IteratorBase;

  void erase(wtf_size_t position);
  void DestroyAll();
  void ExpandCapacityIfNeeded();
  void ExpandCapacity();

  BackingBuffer buffer_;
  wtf_size_t start_;
  wtf_size_t end_;
};

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
class DequeIteratorBase {
  DISALLOW_NEW();

 protected:
  DequeIteratorBase();
  DequeIteratorBase(const Deque<T, inlineCapacity, Allocator>*, wtf_size_t);
  DequeIteratorBase(const DequeIteratorBase&);
  DequeIteratorBase& operator=(const DequeIteratorBase<T, 0, Allocator>&);
  ~DequeIteratorBase();

  void Assign(const DequeIteratorBase& other) { *this = other; }

  void Increment();
  void Decrement();

  T* Before() const;
  T* After() const;

  bool IsEqual(const DequeIteratorBase&) const;

 private:
  Deque<T, inlineCapacity, Allocator>* deque_;
  unsigned index_;

  friend class Deque<T, inlineCapacity, Allocator>;
};

template <typename T,
          wtf_size_t inlineCapacity = 0,
          typename Allocator = PartitionAllocator>
class DequeIterator : public DequeIteratorBase<T, inlineCapacity, Allocator> {
 private:
  typedef DequeIteratorBase<T, inlineCapacity, Allocator> Base;
  typedef DequeIterator<T, inlineCapacity, Allocator> Iterator;

 public:
  typedef ptrdiff_t difference_type;
  typedef T value_type;
  typedef T* pointer;
  typedef T& reference;
  typedef std::bidirectional_iterator_tag iterator_category;

  DequeIterator(Deque<T, inlineCapacity, Allocator>* deque, wtf_size_t index)
      : Base(deque, index) {}

  DequeIterator(const Iterator& other) : Base(other) {}
  DequeIterator& operator=(const Iterator& other) {
    Base::Assign(other);
    return *this;
  }

  T& operator*() const { return *Base::After(); }
  T* operator->() const { return Base::After(); }

  bool operator==(const Iterator& other) const { return Base::IsEqual(other); }
  bool operator!=(const Iterator& other) const { return !Base::IsEqual(other); }

  Iterator& operator++() {
    Base::Increment();
    return *this;
  }
  // postfix ++ intentionally omitted
  Iterator& operator--() {
    Base::Decrement();
    return *this;
  }
  // postfix -- intentionally omitted
};

template <typename T,
          wtf_size_t inlineCapacity = 0,
          typename Allocator = PartitionAllocator>
class DequeConstIterator
    : public DequeIteratorBase<T, inlineCapacity, Allocator> {
 private:
  typedef DequeIteratorBase<T, inlineCapacity, Allocator> Base;
  typedef DequeConstIterator<T, inlineCapacity, Allocator> Iterator;
  typedef DequeIterator<T, inlineCapacity, Allocator> NonConstIterator;

 public:
  typedef ptrdiff_t difference_type;
  typedef T value_type;
  typedef const T* pointer;
  typedef const T& reference;
  typedef std::bidirectional_iterator_tag iterator_category;

  DequeConstIterator(const Deque<T, inlineCapacity, Allocator>* deque,
                     wtf_size_t index)
      : Base(deque, index) {}

  DequeConstIterator(const Iterator& other) : Base(other) {}
  DequeConstIterator(const NonConstIterator& other) : Base(other) {}
  DequeConstIterator& operator=(const Iterator& other) {
    Base::Assign(other);
    return *this;
  }
  DequeConstIterator& operator=(const NonConstIterator& other) {
    Base::Assign(other);
    return *this;
  }

  const T& operator*() const { return *Base::After(); }
  const T* operator->() const { return Base::After(); }

  bool operator==(const Iterator& other) const { return Base::IsEqual(other); }
  bool operator!=(const Iterator& other) const { return !Base::IsEqual(other); }

  Iterator& operator++() {
    Base::Increment();
    return *this;
  }
  // postfix ++ intentionally omitted
  Iterator& operator--() {
    Base::Decrement();
    return *this;
  }
  // postfix -- intentionally omitted
};

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
inline Deque<T, inlineCapacity, Allocator>::Deque() : start_(0), end_(0) {}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
inline Deque<T, inlineCapacity, Allocator>::Deque(const Deque& other)
    : buffer_(other.buffer_.capacity()),
      start_(other.start_),
      end_(other.end_) {
  const T* other_buffer = other.buffer_.Buffer();
  if (start_ <= end_) {
    TypeOperations::UninitializedCopy(
        other_buffer + start_, other_buffer + end_, buffer_.Buffer() + start_);
  } else {
    TypeOperations::UninitializedCopy(other_buffer, other_buffer + end_,
                                      buffer_.Buffer());
    TypeOperations::UninitializedCopy(other_buffer + start_,
                                      other_buffer + buffer_.capacity(),
                                      buffer_.Buffer() + start_);
  }
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
inline Deque<T, inlineCapacity, Allocator>&
Deque<T, inlineCapacity, Allocator>::operator=(const Deque& other) {
  Deque<T> copy(other);
  Swap(copy);
  return *this;
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
inline Deque<T, inlineCapacity, Allocator>::Deque(Deque&& other)
    : start_(0), end_(0) {
  Swap(other);
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
inline Deque<T, inlineCapacity, Allocator>&
Deque<T, inlineCapacity, Allocator>::operator=(Deque&& other) {
  Swap(other);
  return *this;
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
inline void Deque<T, inlineCapacity, Allocator>::DestroyAll() {
  if (start_ <= end_) {
    TypeOperations::Destruct(buffer_.Buffer() + start_,
                             buffer_.Buffer() + end_);
    buffer_.ClearUnusedSlots(buffer_.Buffer() + start_,
                             buffer_.Buffer() + end_);
  } else {
    TypeOperations::Destruct(buffer_.Buffer(), buffer_.Buffer() + end_);
    buffer_.ClearUnusedSlots(buffer_.Buffer(), buffer_.Buffer() + end_);
    TypeOperations::Destruct(buffer_.Buffer() + start_,
                             buffer_.Buffer() + buffer_.capacity());
    buffer_.ClearUnusedSlots(buffer_.Buffer() + start_,
                             buffer_.Buffer() + buffer_.capacity());
  }
}

// For design of the destructor, please refer to
// [here](https://docs.google.com/document/d/1AoGTvb3tNLx2tD1hNqAfLRLmyM59GM0O-7rCHTT_7_U/)
template <typename T, wtf_size_t inlineCapacity, typename Allocator>
inline void Deque<T, inlineCapacity, Allocator>::Finalize() {
  static_assert(!Allocator::kIsGarbageCollected || INLINE_CAPACITY,
                "GarbageCollected collections without inline capacity cannot "
                "be finalized.");
  if ((!INLINE_CAPACITY && !buffer_.Buffer()))
    return;
  if (!IsEmpty() &&
      !(Allocator::kIsGarbageCollected && buffer_.HasOutOfLineBuffer()))
    DestroyAll();

  // For garbage collected deque HeapAllocator::BackingFree() will bail out
  // during sweeping.
  buffer_.Destruct();
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
inline void Deque<T, inlineCapacity, Allocator>::Swap(Deque& other) {
  typename BackingBuffer::OffsetRange this_hole;
  if (start_ <= end_) {
    buffer_.SetSize(end_);
    this_hole.begin = 0;
    this_hole.end = start_;
  } else {
    buffer_.SetSize(buffer_.capacity());
    this_hole.begin = end_;
    this_hole.end = start_;
  }
  typename BackingBuffer::OffsetRange other_hole;
  if (other.start_ <= other.end_) {
    other.buffer_.SetSize(other.end_);
    other_hole.begin = 0;
    other_hole.end = other.start_;
  } else {
    other.buffer_.SetSize(other.buffer_.capacity());
    other_hole.begin = other.end_;
    other_hole.end = other.start_;
  }

  buffer_.SwapVectorBuffer(other.buffer_, this_hole, other_hole);

  std::swap(start_, other.start_);
  std::swap(end_, other.end_);
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
inline void Deque<T, inlineCapacity, Allocator>::clear() {
  DestroyAll();
  start_ = 0;
  end_ = 0;
  buffer_.DeallocateBuffer(buffer_.Buffer());
  buffer_.ResetBufferPointer();
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
inline void Deque<T, inlineCapacity, Allocator>::ExpandCapacityIfNeeded() {
  if (start_) {
    if (end_ + 1 != start_)
      return;
  } else if (end_) {
    if (end_ != buffer_.capacity() - 1)
      return;
  } else if (buffer_.capacity()) {
    return;
  }

  ExpandCapacity();
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
void Deque<T, inlineCapacity, Allocator>::ExpandCapacity() {
  wtf_size_t old_capacity = buffer_.capacity();
  T* old_buffer = buffer_.Buffer();
  wtf_size_t new_capacity = std::max(16u, old_capacity + old_capacity / 4 + 1);
  if (buffer_.ExpandBuffer(new_capacity)) {
    if (start_ <= end_) {
      // No adjustments to be done.
    } else {
      wtf_size_t new_start = buffer_.capacity() - (old_capacity - start_);
      TypeOperations::MoveOverlapping(old_buffer + start_,
                                      old_buffer + old_capacity,
                                      buffer_.Buffer() + new_start);
      buffer_.ClearUnusedSlots(old_buffer + start_,
                               old_buffer + std::min(old_capacity, new_start));
      start_ = new_start;
    }
    return;
  }
  buffer_.AllocateBuffer(new_capacity);
  if (start_ <= end_) {
    TypeOperations::Move(old_buffer + start_, old_buffer + end_,
                         buffer_.Buffer() + start_);
    buffer_.ClearUnusedSlots(old_buffer + start_, old_buffer + end_);
  } else {
    TypeOperations::Move(old_buffer, old_buffer + end_, buffer_.Buffer());
    buffer_.ClearUnusedSlots(old_buffer, old_buffer + end_);
    wtf_size_t new_start = buffer_.capacity() - (old_capacity - start_);
    TypeOperations::Move(old_buffer + start_, old_buffer + old_capacity,
                         buffer_.Buffer() + new_start);
    buffer_.ClearUnusedSlots(old_buffer + start_, old_buffer + old_capacity);
    start_ = new_start;
  }
  buffer_.DeallocateBuffer(old_buffer);
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
inline T Deque<T, inlineCapacity, Allocator>::TakeFirst() {
  T old_first = std::move(front());
  pop_front();
  return old_first;
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
inline T Deque<T, inlineCapacity, Allocator>::TakeLast() {
  T old_last = std::move(back());
  pop_back();
  return old_last;
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
template <typename U>
inline void Deque<T, inlineCapacity, Allocator>::push_back(U&& value) {
  ExpandCapacityIfNeeded();
  T* new_element = &buffer_.Buffer()[end_];
  if (end_ == buffer_.capacity() - 1)
    end_ = 0;
  else
    ++end_;
  ConstructTraits<T, VectorTraits<T>, Allocator>::ConstructAndNotifyElement(
      new_element, std::forward<U>(value));
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
template <typename U>
inline void Deque<T, inlineCapacity, Allocator>::push_front(U&& value) {
  ExpandCapacityIfNeeded();
  if (!start_)
    start_ = buffer_.capacity() - 1;
  else
    --start_;
  ConstructTraits<T, VectorTraits<T>, Allocator>::ConstructAndNotifyElement(
      &buffer_.Buffer()[start_], std::forward<U>(value));
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
template <typename... Args>
inline void Deque<T, inlineCapacity, Allocator>::emplace_back(Args&&... args) {
  ExpandCapacityIfNeeded();
  T* new_element = &buffer_.Buffer()[end_];
  if (end_ == buffer_.capacity() - 1)
    end_ = 0;
  else
    ++end_;
  ConstructTraits<T, VectorTraits<T>, Allocator>::ConstructAndNotifyElement(
      new_element, std::forward<Args>(args)...);
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
template <typename... Args>
inline void Deque<T, inlineCapacity, Allocator>::emplace_front(Args&&... args) {
  ExpandCapacityIfNeeded();
  if (!start_)
    start_ = buffer_.capacity() - 1;
  else
    --start_;
  ConstructTraits<T, VectorTraits<T>, Allocator>::ConstructAndNotifyElement(
      &buffer_.Buffer()[start_], std::forward<Args>(args)...);
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
inline void Deque<T, inlineCapacity, Allocator>::pop_front() {
  DCHECK(!IsEmpty());
  TypeOperations::Destruct(&buffer_.Buffer()[start_],
                           &buffer_.Buffer()[start_ + 1]);
  buffer_.ClearUnusedSlots(&buffer_.Buffer()[start_],
                           &buffer_.Buffer()[start_ + 1]);
  if (start_ == buffer_.capacity() - 1)
    start_ = 0;
  else
    ++start_;
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
inline void Deque<T, inlineCapacity, Allocator>::pop_back() {
  DCHECK(!IsEmpty());
  if (!end_)
    end_ = buffer_.capacity() - 1;
  else
    --end_;
  TypeOperations::Destruct(&buffer_.Buffer()[end_],
                           &buffer_.Buffer()[end_ + 1]);
  buffer_.ClearUnusedSlots(&buffer_.Buffer()[end_],
                           &buffer_.Buffer()[end_ + 1]);
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
inline void Deque<T, inlineCapacity, Allocator>::erase(iterator& it) {
  erase(it.index_);
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
inline void Deque<T, inlineCapacity, Allocator>::erase(const_iterator& it) {
  erase(it.index_);
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
inline void Deque<T, inlineCapacity, Allocator>::erase(wtf_size_t position) {
  if (position == end_)
    return;

  T* buffer = buffer_.Buffer();
  TypeOperations::Destruct(&buffer[position], &buffer[position + 1]);

  // Find which segment of the circular buffer contained the remove element,
  // and only move elements in that part.
  if (position >= start_) {
    TypeOperations::MoveOverlapping(buffer + start_, buffer + position,
                                    buffer + start_ + 1);
    buffer_.ClearUnusedSlots(buffer + start_, buffer + start_ + 1);
    start_ = (start_ + 1) % buffer_.capacity();
  } else {
    TypeOperations::MoveOverlapping(buffer + position + 1, buffer + end_,
                                    buffer + position);
    buffer_.ClearUnusedSlots(buffer + end_ - 1, buffer + end_);
    end_ = (end_ - 1 + buffer_.capacity()) % buffer_.capacity();
  }
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
inline DequeIteratorBase<T, inlineCapacity, Allocator>::DequeIteratorBase()
    : deque_(0) {}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
inline DequeIteratorBase<T, inlineCapacity, Allocator>::DequeIteratorBase(
    const Deque<T, inlineCapacity, Allocator>* deque,
    wtf_size_t index)
    : deque_(const_cast<Deque<T, inlineCapacity, Allocator>*>(deque)),
      index_(index) {}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
inline DequeIteratorBase<T, inlineCapacity, Allocator>::DequeIteratorBase(
    const DequeIteratorBase& other)
    : deque_(other.deque_), index_(other.index_) {}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
inline DequeIteratorBase<T, inlineCapacity, Allocator>&
DequeIteratorBase<T, inlineCapacity, Allocator>::operator=(
    const DequeIteratorBase<T, 0, Allocator>& other) {
  deque_ = other.deque_;
  index_ = other.index_;
  return *this;
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
inline DequeIteratorBase<T, inlineCapacity, Allocator>::~DequeIteratorBase() =
    default;

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
inline bool DequeIteratorBase<T, inlineCapacity, Allocator>::IsEqual(
    const DequeIteratorBase& other) const {
  return index_ == other.index_;
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
inline void DequeIteratorBase<T, inlineCapacity, Allocator>::Increment() {
  DCHECK_NE(index_, deque_->end_);
  DCHECK(deque_->buffer_.capacity());
  if (index_ == deque_->buffer_.capacity() - 1)
    index_ = 0;
  else
    ++index_;
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
inline void DequeIteratorBase<T, inlineCapacity, Allocator>::Decrement() {
  DCHECK_NE(index_, deque_->start_);
  DCHECK(deque_->buffer_.capacity());
  if (!index_)
    index_ = deque_->buffer_.capacity() - 1;
  else
    --index_;
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
inline T* DequeIteratorBase<T, inlineCapacity, Allocator>::After() const {
  CHECK_NE(index_, deque_->end_);
  return &deque_->buffer_.Buffer()[index_];
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
inline T* DequeIteratorBase<T, inlineCapacity, Allocator>::Before() const {
  CHECK_NE(index_, deque_->start_);
  if (!index_)
    return &deque_->buffer_.buffer()[deque_->buffer_.capacity() - 1];
  return &deque_->buffer_.buffer()[index_ - 1];
}

// This is only defined if the allocator is a HeapAllocator. It is used when
// visiting during a tracing GC.
template <typename T, wtf_size_t inlineCapacity, typename Allocator>
template <typename VisitorDispatcher, typename A>
std::enable_if_t<A::kIsGarbageCollected>
Deque<T, inlineCapacity, Allocator>::Trace(VisitorDispatcher visitor) {
  static_assert(Allocator::kIsGarbageCollected,
                "Garbage collector must be enabled.");
  if (buffer_.HasOutOfLineBuffer()) {
    Allocator::TraceVectorBacking(visitor, buffer_.Buffer(),
                                  buffer_.BufferSlot());
  } else {
    Allocator::TraceVectorBacking(visitor, static_cast<T*>(nullptr),
                                  buffer_.BufferSlot());
    const T* buffer_begin = buffer_.Buffer();
    const T* end = buffer_begin + end_;
    if (IsTraceableInCollectionTrait<VectorTraits<T>>::value) {
      if (start_ <= end_) {
        for (const T* buffer_entry = buffer_begin + start_; buffer_entry != end;
             buffer_entry++) {
          Allocator::template Trace<T, VectorTraits<T>>(
              visitor, *const_cast<T*>(buffer_entry));
        }
      } else {
        for (const T* buffer_entry = buffer_begin; buffer_entry != end;
             buffer_entry++) {
          Allocator::template Trace<T, VectorTraits<T>>(
              visitor, *const_cast<T*>(buffer_entry));
        }
        const T* buffer_end = buffer_.Buffer() + buffer_.capacity();
        for (const T* buffer_entry = buffer_begin + start_;
             buffer_entry != buffer_end; buffer_entry++) {
          Allocator::template Trace<T, VectorTraits<T>>(
              visitor, *const_cast<T*>(buffer_entry));
        }
      }
    }
  }
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
inline void swap(Deque<T, inlineCapacity, Allocator>& a,
                 Deque<T, inlineCapacity, Allocator>& b) {
  a.Swap(b);
}

}  // namespace WTF

using WTF::Deque;

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_DEQUE_H_
