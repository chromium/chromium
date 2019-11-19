/*
 *  Copyright (C) 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_VECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_VECTOR_H_

#include <string.h>
#include <algorithm>
#include <initializer_list>
#include <iterator>
#include <utility>

#include "base/macros.h"
#include "base/template_util.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partition_allocator.h"
#include "third_party/blink/renderer/platform/wtf/conditional_destructor.h"
#include "third_party/blink/renderer/platform/wtf/construct_traits.h"
#include "third_party/blink/renderer/platform/wtf/container_annotations.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"  // For default Vector template parameters.
#include "third_party/blink/renderer/platform/wtf/hash_table_deleted_value_type.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/vector_traits.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

// For ASAN builds, disable inline buffers completely as they cause various
// issues.
#ifdef ANNOTATE_CONTIGUOUS_CONTAINER
#define INLINE_CAPACITY 0
#else
#define INLINE_CAPACITY inlineCapacity
#endif

namespace WTF {

#if defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
// The allocation pool for nodes is one big chunk that ASAN has no insight
// into, so it can cloak errors. Make it as small as possible to force nodes
// to be allocated individually where ASAN can see them.
static const wtf_size_t kInitialVectorSize = 1;
#else
static const wtf_size_t kInitialVectorSize = 4;
#endif

template <typename T, wtf_size_t inlineBuffer, typename Allocator>
class Deque;

//
// Vector Traits
//

// Bunch of traits for Vector are defined here, with which you can customize
// Vector's behavior. In most cases the default traits are appropriate, so you
// usually don't have to specialize those traits by yourself.
//
// The behavior of the implementation below can be controlled by VectorTraits.
// If you want to change the behavior of your type, take a look at VectorTraits
// (defined in VectorTraits.h), too.

template <bool needsDestruction, typename T>
struct VectorDestructor;

template <typename T>
struct VectorDestructor<false, T> {
  STATIC_ONLY(VectorDestructor);
  static void Destruct(T*, T*) {}
};

template <typename T>
struct VectorDestructor<true, T> {
  STATIC_ONLY(VectorDestructor);
  static void Destruct(T* begin, T* end) {
    for (T* cur = begin; cur != end; ++cur)
      cur->~T();
  }
};

template <bool unusedSlotsMustBeZeroed, typename T>
struct VectorUnusedSlotClearer;

template <typename T>
struct VectorUnusedSlotClearer<false, T> {
  STATIC_ONLY(VectorUnusedSlotClearer);
  static void Clear(T*, T*) {}
#if DCHECK_IS_ON()
  static void CheckCleared(const T*, const T*) {}
#endif
};

template <typename T>
struct VectorUnusedSlotClearer<true, T> {
  STATIC_ONLY(VectorUnusedSlotClearer);
  static void Clear(T* begin, T* end) {
    memset(reinterpret_cast<void*>(begin), 0, sizeof(T) * (end - begin));
  }

#if DCHECK_IS_ON()
  static void CheckCleared(const T* begin, const T* end) {
    const unsigned char* unused_area =
        reinterpret_cast<const unsigned char*>(begin);
    const unsigned char* end_address =
        reinterpret_cast<const unsigned char*>(end);
    DCHECK_GE(end_address, unused_area);
    for (int i = 0; i < end_address - unused_area; ++i)
      DCHECK(!unused_area[i]);
  }
#endif
};

template <bool canInitializeWithMemset, typename T, typename Allocator>
struct VectorInitializer;

template <typename T, typename Allocator>
struct VectorInitializer<false, T, Allocator> {
  STATIC_ONLY(VectorInitializer);
  static void Initialize(T* begin, T* end) {
    for (T* cur = begin; cur != end; ++cur)
      new (NotNull, cur) T;
  }
};

template <typename T, typename Allocator>
struct VectorInitializer<true, T, Allocator> {
  STATIC_ONLY(VectorInitializer);
  static void Initialize(T* begin, T* end) {
    memset(begin, 0,
           reinterpret_cast<char*>(end) - reinterpret_cast<char*>(begin));
  }
};

template <bool canMoveWithMemcpy, typename T, typename Allocator>
struct VectorMover;

template <typename T, typename Allocator>
struct VectorMover<false, T, Allocator> {
  STATIC_ONLY(VectorMover);
  using Traits = ConstructTraits<T, VectorTraits<T>, Allocator>;
  static void Move(T* src, T* src_end, T* dst, bool has_inline_buffer) {
    while (src != src_end) {
      T* newly_created = Traits::Construct(dst, std::move(*src));
      if (has_inline_buffer)
        Traits::NotifyNewElement(newly_created);
      src->~T();
      ++dst;
      ++src;
    }
  }
  static void MoveOverlapping(T* src,
                              T* src_end,
                              T* dst,
                              bool has_inline_buffer) {
    if (src > dst) {
      Move(src, src_end, dst, has_inline_buffer);
    } else {
      T* dst_end = dst + (src_end - src);
      while (src != src_end) {
        --src_end;
        --dst_end;
        T* newly_created = Traits::Construct(dst_end, std::move(*src_end));
        if (has_inline_buffer)
          Traits::NotifyNewElement(newly_created);
        src_end->~T();
      }
    }
  }
  static void Swap(T* src, T* src_end, T* dst) {
    std::swap_ranges(src, src_end, dst);
    const size_t len = src_end - src;
    Traits::NotifyNewElements(src, len);
    Traits::NotifyNewElements(dst, len);
  }
};

template <typename T, typename Allocator>
struct VectorMover<true, T, Allocator> {
  STATIC_ONLY(VectorMover);
  using Traits = ConstructTraits<T, VectorTraits<T>, Allocator>;
  static void Move(const T* src,
                   const T* src_end,
                   T* dst,
                   bool has_inline_buffer) {
    if (LIKELY(dst && src)) {
      memcpy(dst, src,
             reinterpret_cast<const char*>(src_end) -
                 reinterpret_cast<const char*>(src));
      if (has_inline_buffer)
        Traits::NotifyNewElements(dst, src_end - src);
    }
  }
  static void MoveOverlapping(const T* src,
                              const T* src_end,
                              T* dst,
                              bool has_inline_buffer) {
    if (LIKELY(dst && src)) {
      memmove(dst, src,
              reinterpret_cast<const char*>(src_end) -
                  reinterpret_cast<const char*>(src));
      if (has_inline_buffer)
        Traits::NotifyNewElements(dst, src_end - src);
    }
  }
  static void Swap(T* src, T* src_end, T* dst) {
    std::swap_ranges(reinterpret_cast<char*>(src),
                     reinterpret_cast<char*>(src_end),
                     reinterpret_cast<char*>(dst));
    const size_t len = src_end - src;
    Traits::NotifyNewElements(src, len);
    Traits::NotifyNewElements(dst, len);
  }
};

template <bool canCopyWithMemcpy, typename T, typename Allocator>
struct VectorCopier;

template <typename T, typename Allocator>
struct VectorCopier<false, T, Allocator> {
  STATIC_ONLY(VectorCopier);
  template <typename U>
  static void UninitializedCopy(const U* src, const U* src_end, T* dst) {
    while (src != src_end) {
      ConstructTraits<T, VectorTraits<T>, Allocator>::ConstructAndNotifyElement(
          dst, *src);
      ++dst;
      ++src;
    }
  }
};

template <typename T, typename Allocator>
struct VectorCopier<true, T, Allocator> {
  STATIC_ONLY(VectorCopier);
  static void UninitializedCopy(const T* src, const T* src_end, T* dst) {
    if (LIKELY(dst && src)) {
      memcpy(dst, src,
             reinterpret_cast<const char*>(src_end) -
                 reinterpret_cast<const char*>(src));
      ConstructTraits<T, VectorTraits<T>, Allocator>::NotifyNewElements(
          dst, src_end - src);
    }
  }
  template <typename U>
  static void UninitializedCopy(const U* src, const U* src_end, T* dst) {
    VectorCopier<false, T, Allocator>::UninitializedCopy(src, src_end, dst);
  }
};

template <bool canFillWithMemset, typename T, typename Allocator>
struct VectorFiller;

template <typename T, typename Allocator>
struct VectorFiller<false, T, Allocator> {
  STATIC_ONLY(VectorFiller);
  static void UninitializedFill(T* dst, T* dst_end, const T& val) {
    while (dst != dst_end) {
      ConstructTraits<T, VectorTraits<T>, Allocator>::ConstructAndNotifyElement(
          dst, T(val));
      ++dst;
    }
  }
};

template <typename T, typename Allocator>
struct VectorFiller<true, T, Allocator> {
  STATIC_ONLY(VectorFiller);
  static void UninitializedFill(T* dst, T* dst_end, const T& val) {
    static_assert(sizeof(T) == sizeof(char), "size of type should be one");
    memset(dst, val, dst_end - dst);
  }
};

template <bool canCompareWithMemcmp, typename T>
struct VectorComparer;

template <typename T>
struct VectorComparer<false, T> {
  STATIC_ONLY(VectorComparer);
  static bool Compare(const T* a, const T* b, size_t size) {
    DCHECK(a);
    DCHECK(b);
    return std::equal(a, a + size, b);
  }
};

template <typename T>
struct VectorComparer<true, T> {
  STATIC_ONLY(VectorComparer);
  static bool Compare(const T* a, const T* b, size_t size) {
    DCHECK(a);
    DCHECK(b);
    return memcmp(a, b, sizeof(T) * size) == 0;
  }
};

template <typename T>
struct VectorElementComparer {
  STATIC_ONLY(VectorElementComparer);
  template <typename U>
  static bool CompareElement(const T& left, const U& right) {
    return left == right;
  }
};

template <typename T>
struct VectorElementComparer<std::unique_ptr<T>> {
  STATIC_ONLY(VectorElementComparer);
  template <typename U>
  static bool CompareElement(const std::unique_ptr<T>& left, const U& right) {
    return left.get() == right;
  }
};

// A collection of all the traits used by Vector. This is basically an
// implementation detail of Vector, and you probably don't want to change this.
// If you want to customize Vector's behavior, you should specialize
// VectorTraits instead (defined in VectorTraits.h).
template <typename T, typename Allocator>
struct VectorTypeOperations {
  STATIC_ONLY(VectorTypeOperations);
  static void Destruct(T* begin, T* end) {
    VectorDestructor<VectorTraits<T>::kNeedsDestruction, T>::Destruct(begin,
                                                                      end);
  }

  static void Initialize(T* begin, T* end) {
    VectorInitializer<VectorTraits<T>::kCanInitializeWithMemset, T,
                      Allocator>::Initialize(begin, end);
  }

  static void Move(T* src, T* src_end, T* dst, bool has_inline_buffer = true) {
    VectorMover<VectorTraits<T>::kCanMoveWithMemcpy, T, Allocator>::Move(
        src, src_end, dst, has_inline_buffer);
  }

  static void MoveOverlapping(T* src,
                              T* src_end,
                              T* dst,
                              bool has_inline_buffer = true) {
    VectorMover<VectorTraits<T>::kCanMoveWithMemcpy, T,
                Allocator>::MoveOverlapping(src, src_end, dst,
                                            has_inline_buffer);
  }

  static void Swap(T* src, T* src_end, T* dst) {
    VectorMover<VectorTraits<T>::kCanMoveWithMemcpy, T, Allocator>::Swap(
        src, src_end, dst);
  }

  static void UninitializedCopy(const T* src, const T* src_end, T* dst) {
    VectorCopier<VectorTraits<T>::kCanCopyWithMemcpy, T,
                 Allocator>::UninitializedCopy(src, src_end, dst);
  }

  static void UninitializedFill(T* dst, T* dst_end, const T& val) {
    VectorFiller<VectorTraits<T>::kCanFillWithMemset, T,
                 Allocator>::UninitializedFill(dst, dst_end, val);
  }

  static bool Compare(const T* a, const T* b, size_t size) {
    return VectorComparer<VectorTraits<T>::kCanCompareWithMemcmp, T>::Compare(
        a, b, size);
  }

  template <typename U>
  static bool CompareElement(const T& left, U&& right) {
    return VectorElementComparer<T>::CompareElement(left,
                                                    std::forward<U>(right));
  }
};

//
// VectorBuffer
//

// VectorBuffer is an implementation detail of Vector and Deque. It manages
// Vector's underlying buffer, and does operations like allocation or
// expansion.
//
// Not meant for general consumption.

template <typename T, typename Allocator>
class VectorBufferBase {
  DISALLOW_NEW();

 public:
  VectorBufferBase(VectorBufferBase&&) = default;
  VectorBufferBase& operator=(VectorBufferBase&&) = default;

  void AllocateBufferNoBarrier(wtf_size_t new_capacity) {
    DCHECK(new_capacity);
    DCHECK_LE(new_capacity,
              Allocator::template MaxElementCountInBackingStore<T>());
    size_t size_to_allocate = AllocationSize(new_capacity);
    buffer_ = Allocator::template AllocateVectorBacking<T>(size_to_allocate);
    capacity_ = static_cast<wtf_size_t>(size_to_allocate / sizeof(T));
  }

  void AllocateBuffer(wtf_size_t new_capacity) {
    AllocateBufferNoBarrier(new_capacity);
    Allocator::BackingWriteBarrier(buffer_);
  }

  size_t AllocationSize(size_t capacity) const {
    return Allocator::template QuantizedSize<T>(capacity);
  }

  T* Buffer() { return buffer_; }
  const T* Buffer() const { return buffer_; }
  wtf_size_t capacity() const { return capacity_; }

  void ClearUnusedSlots(T* from, T* to) {
    // If the vector backing is garbage-collected and needs tracing or
    // finalizing, we clear out the unused slots so that the visitor or the
    // finalizer does not cause a problem when visiting the unused slots.
    VectorUnusedSlotClearer<
        Allocator::kIsGarbageCollected &&
            (VectorTraits<T>::kNeedsDestruction ||
             IsTraceableInCollectionTrait<VectorTraits<T>>::value),
        T>::Clear(from, to);
  }

  void CheckUnusedSlots(const T* from, const T* to) {
#if DCHECK_IS_ON() && !defined(ANNOTATE_CONTIGUOUS_CONTAINER)
    VectorUnusedSlotClearer<
        Allocator::kIsGarbageCollected &&
            (VectorTraits<T>::kNeedsDestruction ||
             IsTraceableInCollectionTrait<VectorTraits<T>>::value),
        T>::CheckCleared(from, to);
#endif
  }

  void MoveBufferInto(VectorBufferBase& other) {
    other.buffer_ = buffer_;
    other.capacity_ = capacity_;
  }

  // |end| is exclusive, a la STL.
  struct OffsetRange final {
    OffsetRange() : begin(0), end(0) {}
    explicit OffsetRange(wtf_size_t begin, wtf_size_t end)
        : begin(begin), end(end) {
      DCHECK_LE(begin, end);
    }
    bool empty() const { return begin == end; }
    wtf_size_t begin;
    wtf_size_t end;
  };

 protected:
  static VectorBufferBase AllocateTemporaryBuffer(wtf_size_t capacity) {
    VectorBufferBase buffer;
    buffer.AllocateBufferNoBarrier(capacity);
    return buffer;
  }

  VectorBufferBase() : buffer_(nullptr), capacity_(0) {}

  VectorBufferBase(T* buffer, wtf_size_t capacity)
      : buffer_(buffer), capacity_(capacity) {}

  VectorBufferBase(HashTableDeletedValueType value)
      : buffer_(reinterpret_cast<T*>(-1)) {}

  bool IsHashTableDeletedValue() const {
    return buffer_ == reinterpret_cast<T*>(-1);
  }

  T* buffer_;
  wtf_size_t capacity_;
  wtf_size_t size_;

  DISALLOW_COPY_AND_ASSIGN(VectorBufferBase);
};

template <typename T,
          wtf_size_t inlineCapacity,
          typename Allocator = PartitionAllocator>
class VectorBuffer;

template <typename T, typename Allocator>
class VectorBuffer<T, 0, Allocator> : protected VectorBufferBase<T, Allocator> {
 private:
  using Base = VectorBufferBase<T, Allocator>;

 public:
  using OffsetRange = typename Base::OffsetRange;

  VectorBuffer() = default;

  explicit VectorBuffer(wtf_size_t capacity) {
    // Calling malloc(0) might take a lock and may actually do an allocation
    // on some systems.
    if (capacity)
      AllocateBuffer(capacity);
  }

  void Destruct() {
    DeallocateBuffer(buffer_);
    buffer_ = nullptr;
  }

  void DeallocateBuffer(T* buffer_to_deallocate) {
    Allocator::FreeVectorBacking(buffer_to_deallocate);
  }

  bool ExpandBuffer(wtf_size_t new_capacity) {
    size_t size_to_allocate = AllocationSize(new_capacity);
    if (Allocator::ExpandVectorBacking(buffer_, size_to_allocate)) {
      capacity_ = static_cast<wtf_size_t>(size_to_allocate / sizeof(T));
      return true;
    }
    return false;
  }

  inline bool ShrinkBuffer(wtf_size_t new_capacity) {
    DCHECK_LT(new_capacity, capacity());
    size_t size_to_allocate = AllocationSize(new_capacity);
    if (Allocator::ShrinkVectorBacking(buffer_, AllocationSize(capacity()),
                                       size_to_allocate)) {
      capacity_ = static_cast<wtf_size_t>(size_to_allocate / sizeof(T));
      return true;
    }
    return false;
  }

  void ResetBufferPointer() {
    buffer_ = nullptr;
    capacity_ = 0;
  }

  // See the other specialization for the meaning of |thisHole| and |otherHole|.
  // They are irrelevant in this case.
  void SwapVectorBuffer(VectorBuffer<T, 0, Allocator>& other,
                        OffsetRange this_hole,
                        OffsetRange other_hole) {
    static_assert(VectorTraits<T>::kCanSwapUsingCopyOrMove,
                  "Cannot swap using copy or move.");
    std::swap(buffer_, other.buffer_);
    std::swap(capacity_, other.capacity_);
    std::swap(size_, other.size_);
    Allocator::BackingWriteBarrier(buffer_);
    Allocator::BackingWriteBarrier(other.buffer_);
  }

  using Base::AllocateBuffer;
  using Base::AllocationSize;

  using Base::Buffer;
  using Base::capacity;

  using Base::ClearUnusedSlots;
  using Base::CheckUnusedSlots;

  bool HasOutOfLineBuffer() const {
    // When inlineCapacity is 0 we have an out of line buffer if we have a
    // buffer.
    return Buffer();
  }

  T** BufferSlot() { return &buffer_; }

 protected:
  using Base::size_;

 private:
  using Base::buffer_;
  using Base::capacity_;
};

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
class VectorBuffer : protected VectorBufferBase<T, Allocator> {
 private:
  using Base = VectorBufferBase<T, Allocator>;

 public:
  using OffsetRange = typename Base::OffsetRange;

  VectorBuffer() : Base(InlineBuffer(), inlineCapacity) {}

  VectorBuffer(HashTableDeletedValueType value) : Base(value) {}
  bool IsHashTableDeletedValue() const {
    return Base::IsHashTableDeletedValue();
  }

  explicit VectorBuffer(wtf_size_t capacity)
      : Base(InlineBuffer(), inlineCapacity) {
    if (capacity > inlineCapacity)
      Base::AllocateBuffer(capacity);
  }

  void Destruct() {
    DeallocateBuffer(buffer_);
    buffer_ = nullptr;
  }

  NOINLINE void ReallyDeallocateBuffer(T* buffer_to_deallocate) {
    Allocator::FreeVectorBacking(buffer_to_deallocate);
  }

  void DeallocateBuffer(T* buffer_to_deallocate) {
    if (UNLIKELY(buffer_to_deallocate != InlineBuffer()))
      ReallyDeallocateBuffer(buffer_to_deallocate);
  }

  bool ExpandBuffer(wtf_size_t new_capacity) {
    DCHECK_GT(new_capacity, inlineCapacity);
    if (buffer_ == InlineBuffer())
      return false;

    size_t size_to_allocate = AllocationSize(new_capacity);
    if (Allocator::ExpandVectorBacking(buffer_, size_to_allocate)) {
      capacity_ = static_cast<wtf_size_t>(size_to_allocate / sizeof(T));
      return true;
    }
    return false;
  }

  inline bool ShrinkBuffer(wtf_size_t new_capacity) {
    DCHECK_LT(new_capacity, capacity());
    if (new_capacity <= inlineCapacity) {
      // We need to switch to inlineBuffer.  Vector::shrinkCapacity will
      // handle it.
      return false;
    }
    DCHECK_NE(buffer_, InlineBuffer());
    size_t new_size = AllocationSize(new_capacity);
    if (!Allocator::ShrinkVectorBacking(buffer_, AllocationSize(capacity()),
                                        new_size))
      return false;
    capacity_ = static_cast<wtf_size_t>(new_size / sizeof(T));
    return true;
  }

  void ResetBufferPointer() {
    buffer_ = InlineBuffer();
    capacity_ = inlineCapacity;
  }

  void AllocateBuffer(wtf_size_t new_capacity) {
    // FIXME: This should DCHECK(!buffer_) to catch misuse/leaks.
    if (new_capacity > inlineCapacity)
      Base::AllocateBuffer(new_capacity);
    else
      ResetBufferPointer();
  }

  size_t AllocationSize(size_t capacity) const {
    if (capacity <= inlineCapacity)
      return kInlineBufferSize;
    return Base::AllocationSize(capacity);
  }

  // Swap two vector buffers, both of which have the same non-zero inline
  // capacity.
  //
  // If the data is in an out-of-line buffer, we can just pass the pointers
  // across the two buffers.  If the data is in an inline buffer, we need to
  // either swap or move each element, depending on whether each slot is
  // occupied or not.
  //
  // Further complication comes from the fact that VectorBuffer is also used as
  // the backing store of a Deque.  Deque allocates the objects like a ring
  // buffer, so there may be a "hole" (unallocated region) in the middle of the
  // buffer. This function assumes elements in a range [buffer_, buffer_ +
  // size_) are all allocated except for elements within |thisHole|. The same
  // applies for |other.buffer_| and |otherHole|.
  void SwapVectorBuffer(VectorBuffer<T, inlineCapacity, Allocator>& other,
                        OffsetRange this_hole,
                        OffsetRange other_hole) {
    using TypeOperations = VectorTypeOperations<T, Allocator>;

    static_assert(VectorTraits<T>::kCanSwapUsingCopyOrMove,
                  "Cannot swap using copy or move.");

    if (Buffer() != InlineBuffer() && other.Buffer() != other.InlineBuffer()) {
      // The easiest case: both buffers are non-inline. We just need to swap the
      // pointers.
      std::swap(buffer_, other.buffer_);
      std::swap(capacity_, other.capacity_);
      std::swap(size_, other.size_);
      Allocator::BackingWriteBarrier(buffer_);
      Allocator::BackingWriteBarrier(other.buffer_);
      return;
    }

    Allocator::EnterGCForbiddenScope();

    // Otherwise, we at least need to move some elements from one inline buffer
    // to another.
    //
    // Terminology: "source" is a place from which elements are copied, and
    // "destination" is a place to which elements are copied. thisSource or
    // otherSource can be empty (represented by nullptr) when this range or
    // other range is in an out-of-line buffer.
    //
    // We first record which range needs to get moved and where elements in such
    // a range will go. Elements in an inline buffer will go to the other
    // buffer's inline buffer. Elements in an out-of-line buffer won't move,
    // because we can just swap pointers of out-of-line buffers.
    T* this_source_begin = nullptr;
    wtf_size_t this_source_size = 0;
    T* this_destination_begin = nullptr;
    if (Buffer() == InlineBuffer()) {
      this_source_begin = Buffer();
      this_source_size = size_;
      this_destination_begin = other.InlineBuffer();
      if (!this_hole.empty()) {  // Sanity check.
        DCHECK_LT(this_hole.begin, this_hole.end);
        DCHECK_LE(this_hole.end, this_source_size);
      }
    } else {
      // We don't need the hole information for an out-of-line buffer.
      this_hole.begin = this_hole.end = 0;
    }
    T* other_source_begin = nullptr;
    wtf_size_t other_source_size = 0;
    T* other_destination_begin = nullptr;
    if (other.Buffer() == other.InlineBuffer()) {
      other_source_begin = other.Buffer();
      other_source_size = other.size_;
      other_destination_begin = InlineBuffer();
      if (!other_hole.empty()) {
        DCHECK_LT(other_hole.begin, other_hole.end);
        DCHECK_LE(other_hole.end, other_source_size);
      }
    } else {
      other_hole.begin = other_hole.end = 0;
    }

    // Next, we mutate members and do other bookkeeping. We do pointer swapping
    // (for out-of-line buffers) here if we can. From now on, don't assume
    // buffer() or capacity() maintains their original values.
    std::swap(capacity_, other.capacity_);
    if (this_source_begin &&
        !other_source_begin) {  // Our buffer is inline, theirs is not.
      DCHECK_EQ(Buffer(), InlineBuffer());
      DCHECK_NE(other.Buffer(), other.InlineBuffer());
      ANNOTATE_DELETE_BUFFER(buffer_, inlineCapacity, size_);
      buffer_ = other.Buffer();
      other.buffer_ = other.InlineBuffer();
      std::swap(size_, other.size_);
      ANNOTATE_NEW_BUFFER(other.buffer_, inlineCapacity, other.size_);
      Allocator::BackingWriteBarrier(buffer_);
    } else if (!this_source_begin &&
               other_source_begin) {  // Their buffer is inline, ours is not.
      DCHECK_NE(Buffer(), InlineBuffer());
      DCHECK_EQ(other.Buffer(), other.InlineBuffer());
      ANNOTATE_DELETE_BUFFER(other.buffer_, inlineCapacity, other.size_);
      other.buffer_ = Buffer();
      buffer_ = InlineBuffer();
      std::swap(size_, other.size_);
      ANNOTATE_NEW_BUFFER(buffer_, inlineCapacity, size_);
      Allocator::BackingWriteBarrier(other.buffer_);
    } else {  // Both buffers are inline.
      DCHECK(this_source_begin);
      DCHECK(other_source_begin);
      DCHECK_EQ(Buffer(), InlineBuffer());
      DCHECK_EQ(other.Buffer(), other.InlineBuffer());
      ANNOTATE_CHANGE_SIZE(buffer_, inlineCapacity, size_, other.size_);
      ANNOTATE_CHANGE_SIZE(other.buffer_, inlineCapacity, other.size_, size_);
      std::swap(size_, other.size_);
    }

    // We are ready to move elements. We determine an action for each "section",
    // which is a contiguous range such that all elements in the range are
    // treated similarly.
    wtf_size_t section_begin = 0;
    while (section_begin < inlineCapacity) {
      // To determine the end of this section, we list up all the boundaries
      // where the "occupiedness" may change.
      wtf_size_t section_end = inlineCapacity;
      if (this_source_begin && section_begin < this_source_size)
        section_end = std::min(section_end, this_source_size);
      if (!this_hole.empty() && section_begin < this_hole.begin)
        section_end = std::min(section_end, this_hole.begin);
      if (!this_hole.empty() && section_begin < this_hole.end)
        section_end = std::min(section_end, this_hole.end);
      if (other_source_begin && section_begin < other_source_size)
        section_end = std::min(section_end, other_source_size);
      if (!other_hole.empty() && section_begin < other_hole.begin)
        section_end = std::min(section_end, other_hole.begin);
      if (!other_hole.empty() && section_begin < other_hole.end)
        section_end = std::min(section_end, other_hole.end);

      DCHECK_LT(section_begin, section_end);

      // Is the |sectionBegin|-th element of |thisSource| occupied?
      bool this_occupied = false;
      if (this_source_begin && section_begin < this_source_size) {
        // Yes, it's occupied, unless the position is in a hole.
        if (this_hole.empty() || section_begin < this_hole.begin ||
            section_begin >= this_hole.end)
          this_occupied = true;
      }
      bool other_occupied = false;
      if (other_source_begin && section_begin < other_source_size) {
        if (other_hole.empty() || section_begin < other_hole.begin ||
            section_begin >= other_hole.end)
          other_occupied = true;
      }

      if (this_occupied && other_occupied) {
        // Both occupied; swap them. In this case, one's destination must be the
        // other's source (i.e. both ranges are in inline buffers).
        DCHECK_EQ(this_destination_begin, other_source_begin);
        DCHECK_EQ(other_destination_begin, this_source_begin);
        TypeOperations::Swap(this_source_begin + section_begin,
                             this_source_begin + section_end,
                             other_source_begin + section_begin);
      } else if (this_occupied) {
        // Move from ours to theirs.
        TypeOperations::Move(this_source_begin + section_begin,
                             this_source_begin + section_end,
                             this_destination_begin + section_begin);
        Base::ClearUnusedSlots(this_source_begin + section_begin,
                               this_source_begin + section_end);
      } else if (other_occupied) {
        // Move from theirs to ours.
        TypeOperations::Move(other_source_begin + section_begin,
                             other_source_begin + section_end,
                             other_destination_begin + section_begin);
        Base::ClearUnusedSlots(other_source_begin + section_begin,
                               other_source_begin + section_end);
      } else {
        // Both empty; nothing to do.
      }

      section_begin = section_end;
    }

    Allocator::LeaveGCForbiddenScope();
  }

  using Base::Buffer;
  using Base::capacity;

  bool HasOutOfLineBuffer() const {
    return Buffer() && Buffer() != InlineBuffer();
  }

  T** BufferSlot() { return &buffer_; }

 protected:
  using Base::size_;

 private:
  using Base::buffer_;
  using Base::capacity_;

  static const wtf_size_t kInlineBufferSize = inlineCapacity * sizeof(T);
  T* InlineBuffer() { return unsafe_reinterpret_cast_ptr<T*>(inline_buffer_); }
  const T* InlineBuffer() const {
    return unsafe_reinterpret_cast_ptr<const T*>(inline_buffer_);
  }

  alignas(T) char inline_buffer_[kInlineBufferSize];
  template <typename U, wtf_size_t inlineBuffer, typename V>
  friend class Deque;

  DISALLOW_COPY_AND_ASSIGN(VectorBuffer);
};

//
// Vector
//

// Vector is a container that works just like std::vector. WTF's Vector has
// several extra functionalities: inline buffer, behavior customization via
// traits, and Oilpan support. Those are explained in the sections below.
//
// Vector is the most basic container, which stores its element in a contiguous
// buffer. The buffer is expanded automatically when necessary. The elements
// are automatically moved to the new buffer. This event is called a
// reallocation. A reallocation takes O(N)-time (N = number of elements), but
// its occurrences are rare, so its time cost should not be significant,
// compared to the time cost of other operations to the vector.
//
// Time complexity of key operations is as follows:
//
//     * Indexed access -- O(1)
//     * Insertion or removal of an element at the end -- amortized O(1)
//     * Other insertion or removal -- O(N)
//     * Swapping with another vector -- O(1)
//
// 1. Iterator invalidation semantics
//
// Vector provides STL-compatible iterators and reverse iterators. Iterators
// are _invalidated_ on certain occasions. Reading an invalidated iterator
// causes undefined behavior.
//
// Iterators are invalidated on the following situations:
//
//     * When a reallocation happens on a vector, all the iterators for that
//       vector will be invalidated.
//     * Some member functions invalidate part of the existing iterators for
//       the vector; see comments on the individual functions.
//     * [Oilpan only] Heap compaction invalidates all the iterators for any
//       HeapVectors. This means you can only store an iterator on stack, as
//       a local variable.
//
// In this context, pointers or references to an element of a Vector are
// essentially equivalent to iterators, in that they also become invalid
// whenever corresponding iterators are invalidated.
//
// 2. Inline buffer
//
// Vectors may have an _inline buffer_. An inline buffer is a storage area
// that is contained in the vector itself, along with other metadata like
// size_. It is used as a storage space when the vector's elements fit in
// that space. If the inline buffer becomes full and further space is
// necessary, an out-of-line buffer is allocated in the heap, and it will
// take over the role of the inline buffer.
//
// The existence of an inline buffer is indicated by non-zero |inlineCapacity|
// template argument. The value represents the number of elements that can be
// stored in the inline buffer. Zero |inlineCapacity| means the vector has no
// inline buffer.
//
// An inline buffer increases the size of the Vector instances, and, in trade
// for that, it gives you several performance benefits, as long as the number
// of elements do not exceed |inlineCapacity|:
//
//     * No heap allocation will be made.
//     * Memory locality will improve.
//
// Generally, having an inline buffer is useful for vectors that (1) are
// frequently accessed or modified, and (2) contain only a few elements at
// most.
//
// 3. Behavior customization
//
// You usually do not need to customize Vector's behavior, since the default
// behavior is appropriate for normal usage. The behavior is controlled by
// VectorTypeOperations traits template above. Read VectorTypeOperations
// and VectorTraits if you want to change the behavior for your types (i.e.
// if you really want faster vector operations).
//
// The default traits basically do the following:
//
//     * Skip constructor call and fill zeros with memset for simple types;
//     * Skip destructor call for simple types;
//     * Copy or move by memcpy for simple types; and
//     * Customize the comparisons for smart pointer types, so you can look
//       up a std::unique_ptr<T> element with a raw pointer, for instance.
//
// 4. Oilpan
//
// If you want to store garbage collected objects in Vector, (1) use HeapVector
// (defined in HeapAllocator.h) instead of Vector, and (2) make sure your
// garbage-collected type is wrapped with Member, like:
//
//     HeapVector<Member<Node>> nodes;
//
// Unlike normal garbage-collected objects, a HeapVector object itself is
// NOT a garbage-collected object, but its backing buffer is allocated in
// Oilpan heap, and it may still carry garbage-collected objects.
//
// Even though a HeapVector object is not garbage-collected, you still need
// to trace it, if you stored it in your class. Also, you can allocate it
// as a local variable. This is useful when you want to build a vector locally
// and put it in an on-heap vector with swap().
//
// Also, heap compaction, which may happen at any time when Blink code is not
// running (i.e. Blink code does not appear in the call stack), may invalidate
// existing iterators for any HeapVectors. So, essentially, you should always
// allocate an iterator on stack (as a local variable), and you should not
// store iterators in another heap object.

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
class Vector
    : private VectorBuffer<T, INLINE_CAPACITY, Allocator>,
      public ConditionalDestructor<Vector<T, INLINE_CAPACITY, Allocator>,
                                   (INLINE_CAPACITY == 0) &&
                                       Allocator::kIsGarbageCollected> {
  USE_ALLOCATOR(Vector, Allocator);
  using Base = VectorBuffer<T, INLINE_CAPACITY, Allocator>;
  using TypeOperations = VectorTypeOperations<T, Allocator>;
  using OffsetRange = typename Base::OffsetRange;

 public:
  using ValueType = T;
  using value_type = T;

  using iterator = T*;
  using const_iterator = const T*;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  // Create an empty vector.
  inline Vector();
  // Create a vector containing the specified number of default-initialized
  // elements.
  inline explicit Vector(wtf_size_t);
  // Create a vector containing the specified number of elements, each of which
  // is copy initialized from the specified value.
  inline Vector(wtf_size_t, const T&);

  // HashTable support
  Vector(HashTableDeletedValueType value) : Base(value) {}
  bool IsHashTableDeletedValue() const {
    return Base::IsHashTableDeletedValue();
  }

  // Copying.
  Vector(const Vector&);
  template <wtf_size_t otherCapacity>
  explicit Vector(const Vector<T, otherCapacity, Allocator>&);

  Vector& operator=(const Vector&);
  template <wtf_size_t otherCapacity>
  Vector& operator=(const Vector<T, otherCapacity, Allocator>&);

  // Moving.
  Vector(Vector&&);
  Vector& operator=(Vector&&);

  // Construct with an initializer list. You can do e.g.
  //     Vector<int> v({1, 2, 3});
  // or
  //     v = {4, 5, 6};
  Vector(std::initializer_list<T> elements);
  Vector& operator=(std::initializer_list<T> elements);

  // Basic inquiry about the vector's state.
  //
  // capacity() is the maximum number of elements that the Vector can hold
  // without a reallocation. It can be zero.
  wtf_size_t size() const { return size_; }
  wtf_size_t capacity() const { return Base::capacity(); }
  bool IsEmpty() const { return !size(); }

  // at() and operator[]: Obtain the reference of the element that is located
  // at the given index. The reference may be invalidated on a reallocation.
  //
  // at() can be used in cases like:
  //     pointerToVector->at(1);
  // instead of:
  //     (*pointerToVector)[1];
  T& at(wtf_size_t i) {
    CHECK_LT(i, size());
    return Base::Buffer()[i];
  }
  const T& at(wtf_size_t i) const {
    CHECK_LT(i, size());
    return Base::Buffer()[i];
  }

  T& operator[](wtf_size_t i) { return at(i); }
  const T& operator[](wtf_size_t i) const { return at(i); }

  // Return a pointer to the front of the backing buffer. Those pointers get
  // invalidated on a reallocation.
  T* data() { return Base::Buffer(); }
  const T* data() const { return Base::Buffer(); }

  // Iterators and reverse iterators. They are invalidated on a reallocation.
  iterator begin() { return data(); }
  iterator end() { return begin() + size_; }
  const_iterator begin() const { return data(); }
  const_iterator end() const { return begin() + size_; }

  reverse_iterator rbegin() { return reverse_iterator(end()); }
  reverse_iterator rend() { return reverse_iterator(begin()); }
  const_reverse_iterator rbegin() const {
    return const_reverse_iterator(end());
  }
  const_reverse_iterator rend() const {
    return const_reverse_iterator(begin());
  }

  // Quick access to the first and the last element. It is invalid to call
  // these functions when the vector is empty.
  T& front() { return at(0); }
  const T& front() const { return at(0); }
  T& back() { return at(size() - 1); }
  const T& back() const { return at(size() - 1); }

  // Searching.
  //
  // Comparisons are done in terms of compareElement(), which is usually
  // operator==(). find() and reverseFind() returns an index of the element
  // that is found first. If no match is found, kNotFound will be returned.
  template <typename U>
  bool Contains(const U&) const;
  template <typename U>
  wtf_size_t Find(const U&) const;
  template <typename U>
  wtf_size_t ReverseFind(const U&) const;

  // Resize the vector to the specified size.
  //
  // These three functions are essentially similar. They differ in that
  // (1) shrink() has a DCHECK to make sure the specified size is not more than
  // size(), and (2) grow() has a DCHECK to make sure the specified size is
  // not less than size().
  //
  // When a vector shrinks, the extra elements in the back will be destructed.
  // All the iterators pointing to a to-be-destructed element will be
  // invalidated.
  //
  // When a vector grows, new elements will be added in the back, and they
  // will be default-initialized. A reallocation may happen in this case.
  void Shrink(wtf_size_t);
  void Grow(wtf_size_t);
  void resize(wtf_size_t);

  // Increase the capacity of the vector to at least |newCapacity|. The
  // elements in the vector are not affected. This function does not shrink
  // the size of the backing buffer, even if |newCapacity| is small. This
  // function may cause a reallocation.
  void ReserveCapacity(wtf_size_t new_capacity);

  // This is similar to reserveCapacity() but must be called immediately after
  // the vector is default-constructed.
  void ReserveInitialCapacity(wtf_size_t initial_capacity);

  // Shrink the backing buffer so it can contain exactly |size()| elements.
  // This function may cause a reallocation.
  void ShrinkToFit() { ShrinkCapacity(size()); }

  // Shrink the backing buffer if at least 50% of the vector's capacity is
  // unused. If it shrinks, the new buffer contains roughly 25% of unused
  // space. This function may cause a reallocation.
  void ShrinkToReasonableCapacity() {
    if (size() * 2 < capacity())
      ShrinkCapacity(size() + size() / 4 + 1);
  }

  // Remove all the elements. This function actually releases the backing
  // buffer, thus any iterators will get invalidated (including begin()).
  void clear() { ShrinkCapacity(0); }

  // Insertion to the back. All of these functions except uncheckedAppend() may
  // cause a reallocation.
  //
  // push_back(value)
  //     Insert a single element to the back.
  // emplace_back(args...)
  //     Insert a single element constructed as T(args...) to the back. The
  //     element is constructed directly on the backing buffer with placement
  //     new.
  // Append(buffer, size)
  // AppendVector(vector)
  // AppendRange(begin, end)
  //     Insert multiple elements represented by (1) |buffer| and |size|
  //     (for append), (2) |vector| (for AppendVector), or (3) a pair of
  //     iterators (for AppendRange) to the back. The elements will be copied.
  // UncheckedAppend(value)
  //     Insert a single element like push_back(), but this function assumes
  //     the vector has enough capacity such that it can store the new element
  //     without a reallocation. Using this function could improve the
  //     performance when you append many elements repeatedly.
  template <typename U>
  void push_back(U&&);
  template <typename... Args>
  T& emplace_back(Args&&...);
  ALWAYS_INLINE T& emplace_back() {
    Grow(size_ + 1);
    return back();
  }
  template <typename U>
  void Append(const U*, wtf_size_t);
  template <typename U, wtf_size_t otherCapacity, typename V>
  void AppendVector(const Vector<U, otherCapacity, V>&);
  template <typename Iterator>
  void AppendRange(Iterator begin, Iterator end);
  template <typename U>
  void UncheckedAppend(U&&);

  // Insertion to an arbitrary position. All of these functions will take
  // O(size())-time. All of the elements after |position| will be moved to
  // the new locations. |position| must be no more than size(). All of these
  // functions may cause a reallocation. In any case, all the iterators
  // pointing to an element after |position| will be invalidated.
  //
  // insert(position, value)
  //     Insert a single element at |position|, where |position| is an index.
  // insert(position, buffer, size)
  // InsertVector(position, vector)
  //     Insert multiple elements represented by either |buffer| and |size|
  //     or |vector| at |position|. The elements will be copied.
  // InsertAt(position, value)
  //     Insert a single element at |position|, where |position| is an iterator.
  // InsertAt(position, buffer, size)
  //     Insert multiple elements represented by either |buffer| and |size|
  //     or |vector| at |position|. The elements will be copied.
  template <typename U>
  void insert(wtf_size_t position, U&&);
  template <typename U>
  void insert(wtf_size_t position, const U*, wtf_size_t);
  template <typename U>
  void InsertAt(iterator position, U&&);
  template <typename U>
  void InsertAt(iterator position, const U*, wtf_size_t);
  template <typename U, wtf_size_t otherCapacity, typename OtherAllocator>
  void InsertVector(wtf_size_t position,
                    const Vector<U, otherCapacity, OtherAllocator>&);

  // Insertion to the front. All of these functions will take O(size())-time.
  // All of the elements in the vector will be moved to the new locations.
  // All of these functions may cause a reallocation. In any case, all the
  // iterators pointing to any element in the vector will be invalidated.
  //
  // push_front(value)
  //     Insert a single element to the front.
  // push_front(buffer, size)
  // PrependVector(vector)
  //     Insert multiple elements represented by either |buffer| and |size| or
  //     |vector| to the front. The elements will be copied.
  template <typename U>
  void push_front(U&&);
  template <typename U>
  void push_front(const U*, wtf_size_t);
  template <typename U, wtf_size_t otherCapacity, typename OtherAllocator>
  void PrependVector(const Vector<U, otherCapacity, OtherAllocator>&);

  // Remove an element or elements at the specified position. These functions
  // take O(size())-time. All of the elements after the removed ones will be
  // moved to the new locations. All the iterators pointing to any element
  // after |position| will be invalidated.
  void EraseAt(wtf_size_t position);
  void EraseAt(wtf_size_t position, wtf_size_t length);
  iterator erase(iterator position);

  // Remove the last element. Unlike remove(), (1) this function is fast, and
  // (2) only iterators pointing to the last element will be invalidated. Other
  // references will remain valid.
  void pop_back() {
    DCHECK(!IsEmpty());
    Shrink(size() - 1);
  }

  // Filling the vector with the same value. If the vector has shrinked or
  // growed as a result of this call, those events may invalidate some
  // iterators. See comments for shrink() and grow().
  //
  // Fill(value, size) will resize the Vector to |size|, and then copy-assign
  // or copy-initialize all the elements.
  //
  // Fill(value) is a synonym for Fill(value, size()).
  void Fill(const T&, wtf_size_t);
  void Fill(const T& val) { Fill(val, size()); }

  // Swap two vectors quickly.
  void swap(Vector& other) {
    Base::SwapVectorBuffer(other, OffsetRange(), OffsetRange());
  }

  // Reverse the contents.
  void Reverse();

  // Maximum element count supported; allocating a vector
  // buffer with a larger count will fail.
  static size_t MaxCapacity() {
    return Allocator::template MaxElementCountInBackingStore<T>();
  }

  void Finalize() {
    static_assert(!Allocator::kIsGarbageCollected || INLINE_CAPACITY,
                  "GarbageCollected collections without inline capacity cannot "
                  "be finalized.");
    if (!INLINE_CAPACITY && LIKELY(!Base::Buffer())) {
      return;
    }
    ANNOTATE_DELETE_BUFFER(begin(), capacity(), size_);
    if (LIKELY(size_) &&
        !(Allocator::kIsGarbageCollected && this->HasOutOfLineBuffer())) {
      TypeOperations::Destruct(begin(), end());
      size_ = 0;  // Partial protection against use-after-free.
    }

    // For garbage collected vector HeapAllocator::BackingFree() will bail out
    // during sweeping.
    Base::Destruct();
  }

  template <typename VisitorDispatcher, typename A = Allocator>
  std::enable_if_t<A::kIsGarbageCollected> Trace(VisitorDispatcher);

  class GCForbiddenScope {
    STACK_ALLOCATED();

   public:
    GCForbiddenScope() { Allocator::EnterGCForbiddenScope(); }
    ~GCForbiddenScope() { Allocator::LeaveGCForbiddenScope(); }
  };

 protected:
  using Base::CheckUnusedSlots;
  using Base::ClearUnusedSlots;

  T** GetBufferSlot() { return Base::BufferSlot(); }

 private:
  void ExpandCapacity(wtf_size_t new_min_capacity);
  T* ExpandCapacity(wtf_size_t new_min_capacity, T*);
  T* ExpandCapacity(wtf_size_t new_min_capacity, const T* data) {
    return ExpandCapacity(new_min_capacity, const_cast<T*>(data));
  }

  template <typename U>
  U* ExpandCapacity(wtf_size_t new_min_capacity, U*);
  void ShrinkCapacity(wtf_size_t new_capacity);
  template <typename U>
  void AppendSlowCase(U&&);

  bool HasInlineBuffer() const {
    return INLINE_CAPACITY && !this->HasOutOfLineBuffer();
  }

  void ReallocateBuffer(wtf_size_t);

  // This is to prevent compilation of deprecated calls like 'vector.erase(0)'.
  void erase(std::nullptr_t) = delete;

  using Base::size_;
  using Base::Buffer;
  using Base::SwapVectorBuffer;
  using Base::AllocateBuffer;
  using Base::AllocationSize;
};

//
// Vector out-of-line implementation
//

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
inline Vector<T, inlineCapacity, Allocator>::Vector() {
  static_assert(!std::is_polymorphic<T>::value ||
                    !VectorTraits<T>::kCanInitializeWithMemset,
                "Cannot initialize with memset if there is a vtable");
  static_assert(Allocator::kIsGarbageCollected || !IsDisallowNew<T>::value ||
                    !IsTraceable<T>::value,
                "Cannot put DISALLOW_NEW objects that "
                "have trace methods into an off-heap Vector");
  static_assert(Allocator::kIsGarbageCollected ||
                    !IsPointerToGarbageCollectedType<T>::value,
                "Cannot put raw pointers to garbage-collected classes into "
                "an off-heap Vector.  Use HeapVector<Member<T>> instead.");

  ANNOTATE_NEW_BUFFER(begin(), capacity(), 0);
  size_ = 0;
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
inline Vector<T, inlineCapacity, Allocator>::Vector(wtf_size_t size)
    : Base(size) {
  static_assert(!std::is_polymorphic<T>::value ||
                    !VectorTraits<T>::kCanInitializeWithMemset,
                "Cannot initialize with memset if there is a vtable");
  static_assert(Allocator::kIsGarbageCollected || !IsDisallowNew<T>::value ||
                    !IsTraceable<T>::value,
                "Cannot put DISALLOW_NEW objects that "
                "have trace methods into an off-heap Vector");
  static_assert(Allocator::kIsGarbageCollected ||
                    !IsPointerToGarbageCollectedType<T>::value,
                "Cannot put raw pointers to garbage-collected classes into "
                "an off-heap Vector.  Use HeapVector<Member<T>> instead.");

  ANNOTATE_NEW_BUFFER(begin(), capacity(), size);
  size_ = size;
  TypeOperations::Initialize(begin(), end());
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
inline Vector<T, inlineCapacity, Allocator>::Vector(wtf_size_t size,
                                                    const T& val)
    : Base(size) {
  // TODO(yutak): Introduce these assertions. Some use sites call this function
  // in the context where T is an incomplete type.
  //
  // static_assert(!std::is_polymorphic<T>::value ||
  //               !VectorTraits<T>::canInitializeWithMemset,
  //               "Cannot initialize with memset if there is a vtable");
  // static_assert(Allocator::isGarbageCollected ||
  //               !IsDisallowNew<T>::value ||
  //               !IsTraceable<T>::value,
  //               "Cannot put DISALLOW_NEW objects that "
  //               "have trace methods into an off-heap Vector");
  // static_assert(Allocator::isGarbageCollected ||
  //               !IsPointerToGarbageCollectedType<T>::value,
  //               "Cannot put raw pointers to garbage-collected classes into "
  //               "an off-heap Vector.  Use HeapVector<Member<T>> instead.");

  ANNOTATE_NEW_BUFFER(begin(), capacity(), size);
  size_ = size;
  TypeOperations::UninitializedFill(begin(), end(), val);
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
Vector<T, inlineCapacity, Allocator>::Vector(const Vector& other)
    : Base(other.capacity()) {
  ANNOTATE_NEW_BUFFER(begin(), capacity(), other.size());
  size_ = other.size();
  TypeOperations::UninitializedCopy(other.begin(), other.end(), begin());
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
template <wtf_size_t otherCapacity>
Vector<T, inlineCapacity, Allocator>::Vector(
    const Vector<T, otherCapacity, Allocator>& other)
    : Base(other.capacity()) {
  ANNOTATE_NEW_BUFFER(begin(), capacity(), other.size());
  size_ = other.size();
  TypeOperations::UninitializedCopy(other.begin(), other.end(), begin());
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
Vector<T, inlineCapacity, Allocator>& Vector<T, inlineCapacity, Allocator>::
operator=(const Vector<T, inlineCapacity, Allocator>& other) {
  if (UNLIKELY(&other == this))
    return *this;

  if (size() > other.size()) {
    Shrink(other.size());
  } else if (other.size() > capacity()) {
    clear();
    ReserveCapacity(other.size());
    DCHECK(begin());
  }

  ANNOTATE_CHANGE_SIZE(begin(), capacity(), size_, other.size());
  std::copy(other.begin(), other.begin() + size(), begin());
  TypeOperations::UninitializedCopy(other.begin() + size(), other.end(), end());
  size_ = other.size();

  return *this;
}

inline bool TypelessPointersAreEqual(const void* a, const void* b) {
  return a == b;
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
template <wtf_size_t otherCapacity>
Vector<T, inlineCapacity, Allocator>& Vector<T, inlineCapacity, Allocator>::
operator=(const Vector<T, otherCapacity, Allocator>& other) {
  // If the inline capacities match, we should call the more specific
  // template.  If the inline capacities don't match, the two objects
  // shouldn't be allocated the same address.
  DCHECK(!TypelessPointersAreEqual(&other, this));

  if (size() > other.size()) {
    Shrink(other.size());
  } else if (other.size() > capacity()) {
    clear();
    ReserveCapacity(other.size());
    DCHECK(begin());
  }

  ANNOTATE_CHANGE_SIZE(begin(), capacity(), size_, other.size());
  std::copy(other.begin(), other.begin() + size(), begin());
  TypeOperations::UninitializedCopy(other.begin() + size(), other.end(), end());
  size_ = other.size();

  return *this;
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
Vector<T, inlineCapacity, Allocator>::Vector(
    Vector<T, inlineCapacity, Allocator>&& other) {
  size_ = 0;
  // It's a little weird to implement a move constructor using swap but this
  // way we don't have to add a move constructor to VectorBuffer.
  swap(other);
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
Vector<T, inlineCapacity, Allocator>& Vector<T, inlineCapacity, Allocator>::
operator=(Vector<T, inlineCapacity, Allocator>&& other) {
  swap(other);
  return *this;
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
Vector<T, inlineCapacity, Allocator>::Vector(std::initializer_list<T> elements)
    : Base(SafeCast<wtf_size_t>(elements.size())) {
  ANNOTATE_NEW_BUFFER(begin(), capacity(), elements.size());
  size_ = static_cast<wtf_size_t>(elements.size());
  TypeOperations::UninitializedCopy(elements.begin(), elements.end(), begin());
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
Vector<T, inlineCapacity, Allocator>& Vector<T, inlineCapacity, Allocator>::
operator=(std::initializer_list<T> elements) {
  wtf_size_t input_size = SafeCast<wtf_size_t>(elements.size());
  if (size() > input_size) {
    Shrink(input_size);
  } else if (input_size > capacity()) {
    clear();
    ReserveCapacity(input_size);
    DCHECK(begin());
  }

  ANNOTATE_CHANGE_SIZE(begin(), capacity(), size_, input_size);
  std::copy(elements.begin(), elements.begin() + size_, begin());
  TypeOperations::UninitializedCopy(elements.begin() + size_, elements.end(),
                                    end());
  size_ = input_size;

  return *this;
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
template <typename U>
bool Vector<T, inlineCapacity, Allocator>::Contains(const U& value) const {
  return Find(value) != kNotFound;
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
template <typename U>
wtf_size_t Vector<T, inlineCapacity, Allocator>::Find(const U& value) const {
  const T* b = begin();
  const T* e = end();
  for (const T* iter = b; iter < e; ++iter) {
    if (TypeOperations::CompareElement(*iter, value))
      return static_cast<wtf_size_t>(iter - b);
  }
  return kNotFound;
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
template <typename U>
wtf_size_t Vector<T, inlineCapacity, Allocator>::ReverseFind(
    const U& value) const {
  const T* b = begin();
  const T* iter = end();
  while (iter > b) {
    --iter;
    if (TypeOperations::CompareElement(*iter, value))
      return static_cast<wtf_size_t>(iter - b);
  }
  return kNotFound;
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
void Vector<T, inlineCapacity, Allocator>::Fill(const T& val,
                                                wtf_size_t new_size) {
  if (size() > new_size) {
    Shrink(new_size);
  } else if (new_size > capacity()) {
    clear();
    ReserveCapacity(new_size);
    DCHECK(begin());
  }

  ANNOTATE_CHANGE_SIZE(begin(), capacity(), size_, new_size);
  std::fill(begin(), end(), val);
  TypeOperations::UninitializedFill(end(), begin() + new_size, val);
  size_ = new_size;
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
void Vector<T, inlineCapacity, Allocator>::ExpandCapacity(
    wtf_size_t new_min_capacity) {
  wtf_size_t old_capacity = capacity();
  wtf_size_t expanded_capacity = old_capacity;
  // We use a more aggressive expansion strategy for Vectors with inline
  // storage.  This is because they are more likely to be on the stack, so the
  // risk of heap bloat is minimized.  Furthermore, exceeding the inline
  // capacity limit is not supposed to happen in the common case and may
  // indicate a pathological condition or microbenchmark.
  if (INLINE_CAPACITY) {
    expanded_capacity *= 2;
    // Check for integer overflow, which could happen in the 32-bit build.
    CHECK_GT(expanded_capacity, old_capacity);
  } else {
    // This cannot integer overflow.
    // On 64-bit, the "expanded" integer is 32-bit, and any encroachment
    // above 2^32 will fail allocation in allocateBuffer().  On 32-bit,
    // there's not enough address space to hold the old and new buffers.  In
    // addition, our underlying allocator is supposed to always fail on >
    // (2^31 - 1) allocations.
    expanded_capacity += (expanded_capacity / 4) + 1;
  }
  ReserveCapacity(std::max(new_min_capacity,
                           std::max(kInitialVectorSize, expanded_capacity)));
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
T* Vector<T, inlineCapacity, Allocator>::ExpandCapacity(
    wtf_size_t new_min_capacity,
    T* ptr) {
  if (ptr < begin() || ptr >= end()) {
    ExpandCapacity(new_min_capacity);
    return ptr;
  }
  size_t index = ptr - begin();
  ExpandCapacity(new_min_capacity);
  return begin() + index;
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
template <typename U>
inline U* Vector<T, inlineCapacity, Allocator>::ExpandCapacity(
    wtf_size_t new_min_capacity,
    U* ptr) {
  ExpandCapacity(new_min_capacity);
  return ptr;
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
inline void Vector<T, inlineCapacity, Allocator>::resize(wtf_size_t size) {
  if (size <= size_) {
    TypeOperations::Destruct(begin() + size, end());
    ClearUnusedSlots(begin() + size, end());
    ANNOTATE_CHANGE_SIZE(begin(), capacity(), size_, size);
  } else {
    if (size > capacity())
      ExpandCapacity(size);
    ANNOTATE_CHANGE_SIZE(begin(), capacity(), size_, size);
    TypeOperations::Initialize(end(), begin() + size);
  }

  size_ = size;
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
void Vector<T, inlineCapacity, Allocator>::Shrink(wtf_size_t size) {
  DCHECK_LE(size, size_);
  TypeOperations::Destruct(begin() + size, end());
  ClearUnusedSlots(begin() + size, end());
  ANNOTATE_CHANGE_SIZE(begin(), capacity(), size_, size);
  size_ = size;
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
void Vector<T, inlineCapacity, Allocator>::Grow(wtf_size_t size) {
  DCHECK_GE(size, size_);
  if (size > capacity())
    ExpandCapacity(size);
  ANNOTATE_CHANGE_SIZE(begin(), capacity(), size_, size);
  TypeOperations::Initialize(end(), begin() + size);
  size_ = size;
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
void Vector<T, inlineCapacity, Allocator>::ReserveCapacity(
    wtf_size_t new_capacity) {
  if (UNLIKELY(new_capacity <= capacity()))
    return;
  if (!data()) {
    Base::AllocateBuffer(new_capacity);
    return;
  }
#ifdef ANNOTATE_CONTIGUOUS_CONTAINER
  wtf_size_t old_capacity = capacity();
#endif
  // The Allocator::isGarbageCollected check is not needed.  The check is just
  // a static hint for a compiler to indicate that Base::expandBuffer returns
  // false if Allocator is a PartitionAllocator.
  if (Allocator::kIsGarbageCollected && Base::ExpandBuffer(new_capacity)) {
    ANNOTATE_CHANGE_CAPACITY(begin(), old_capacity, size_, capacity());
    return;
  }
  // Reallocating a backing buffer may resurrect a dead object.
  CHECK(Allocator::IsAllocationAllowed());

  ReallocateBuffer(new_capacity);
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
inline void Vector<T, inlineCapacity, Allocator>::ReserveInitialCapacity(
    wtf_size_t initial_capacity) {
  DCHECK(!size_);
  DCHECK(capacity() == INLINE_CAPACITY);
  if (initial_capacity > INLINE_CAPACITY) {
    ANNOTATE_DELETE_BUFFER(begin(), capacity(), size_);
    Base::AllocateBuffer(initial_capacity);
    ANNOTATE_NEW_BUFFER(begin(), capacity(), size_);
  }
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
void Vector<T, inlineCapacity, Allocator>::ShrinkCapacity(
    wtf_size_t new_capacity) {
  if (new_capacity >= capacity())
    return;

  if (new_capacity < size())
    Shrink(new_capacity);

  T* old_buffer = begin();
#ifdef ANNOTATE_CONTIGUOUS_CONTAINER
  wtf_size_t old_capacity = capacity();
#endif
  if (new_capacity > 0) {
    if (Base::ShrinkBuffer(new_capacity)) {
      ANNOTATE_CHANGE_CAPACITY(begin(), old_capacity, size_, capacity());
      return;
    }

    if (!Allocator::IsAllocationAllowed())
      return;

    ReallocateBuffer(new_capacity);
    return;
  }
  Base::ResetBufferPointer();
#ifdef ANNOTATE_CONTIGUOUS_CONTAINER
  if (old_buffer != begin()) {
    ANNOTATE_NEW_BUFFER(begin(), capacity(), size_);
    ANNOTATE_DELETE_BUFFER(old_buffer, old_capacity, size_);
  }
#endif
  Base::DeallocateBuffer(old_buffer);
}

// Templatizing these is better than just letting the conversion happen
// implicitly.
template <typename T, wtf_size_t inlineCapacity, typename Allocator>
template <typename U>
ALWAYS_INLINE void Vector<T, inlineCapacity, Allocator>::push_back(U&& val) {
  DCHECK(Allocator::IsAllocationAllowed());
  if (LIKELY(size() != capacity())) {
    ANNOTATE_CHANGE_SIZE(begin(), capacity(), size_, size_ + 1);
    ConstructTraits<T, VectorTraits<T>, Allocator>::ConstructAndNotifyElement(
        end(), std::forward<U>(val));
    ++size_;
    return;
  }

  AppendSlowCase(std::forward<U>(val));
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
template <typename... Args>
ALWAYS_INLINE T& Vector<T, inlineCapacity, Allocator>::emplace_back(
    Args&&... args) {
  DCHECK(Allocator::IsAllocationAllowed());
  if (UNLIKELY(size() == capacity()))
    ExpandCapacity(size() + 1);

  ANNOTATE_CHANGE_SIZE(begin(), capacity(), size_, size_ + 1);
  T* t =
      ConstructTraits<T, VectorTraits<T>, Allocator>::ConstructAndNotifyElement(
          end(), std::forward<Args>(args)...);
  ++size_;
  return *t;
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
template <typename U>
void Vector<T, inlineCapacity, Allocator>::Append(const U* data,
                                                  wtf_size_t data_size) {
  DCHECK(Allocator::IsAllocationAllowed());
  wtf_size_t new_size = size_ + data_size;
  if (new_size > capacity()) {
    data = ExpandCapacity(new_size, data);
    DCHECK(begin());
  }
  CHECK_GE(new_size, size_);
  T* dest = end();
  ANNOTATE_CHANGE_SIZE(begin(), capacity(), size_, new_size);
  VectorCopier<VectorTraits<T>::kCanCopyWithMemcpy, T,
               Allocator>::UninitializedCopy(data, &data[data_size], dest);
  size_ = new_size;
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
template <typename U>
NOINLINE void Vector<T, inlineCapacity, Allocator>::AppendSlowCase(U&& val) {
  DCHECK_EQ(size(), capacity());

  typename std::remove_reference<U>::type* ptr = &val;
  ptr = ExpandCapacity(size() + 1, ptr);
  DCHECK(begin());

  ANNOTATE_CHANGE_SIZE(begin(), capacity(), size_, size_ + 1);
  ConstructTraits<T, VectorTraits<T>, Allocator>::ConstructAndNotifyElement(
      end(), std::forward<U>(*ptr));
  ++size_;
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
template <typename U, wtf_size_t otherCapacity, typename OtherAllocator>
inline void Vector<T, inlineCapacity, Allocator>::AppendVector(
    const Vector<U, otherCapacity, OtherAllocator>& val) {
  Append(val.begin(), val.size());
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
template <typename Iterator>
void Vector<T, inlineCapacity, Allocator>::AppendRange(Iterator begin,
                                                       Iterator end) {
  for (Iterator it = begin; it != end; ++it)
    push_back(*it);
}

// This version of append saves a branch in the case where you know that the
// vector's capacity is large enough for the append to succeed.
template <typename T, wtf_size_t inlineCapacity, typename Allocator>
template <typename U>
ALWAYS_INLINE void Vector<T, inlineCapacity, Allocator>::UncheckedAppend(
    U&& val) {
#ifdef ANNOTATE_CONTIGUOUS_CONTAINER
  // Vectors in ASAN builds don't have inlineCapacity.
  push_back(std::forward<U>(val));
#else
  DCHECK_LT(size(), capacity());
  ConstructTraits<T, VectorTraits<T>, Allocator>::ConstructAndNotifyElement(
      end(), std::forward<U>(val));
  ++size_;
#endif
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
template <typename U>
inline void Vector<T, inlineCapacity, Allocator>::insert(wtf_size_t position,
                                                         U&& val) {
  DCHECK(Allocator::IsAllocationAllowed());
  CHECK_LE(position, size());
  typename std::remove_reference<U>::type* data = &val;
  if (size() == capacity()) {
    data = ExpandCapacity(size() + 1, data);
    DCHECK(begin());
  }
  ANNOTATE_CHANGE_SIZE(begin(), capacity(), size_, size_ + 1);
  T* spot = begin() + position;
  TypeOperations::MoveOverlapping(spot, end(), spot + 1);
  ConstructTraits<T, VectorTraits<T>, Allocator>::ConstructAndNotifyElement(
      spot, std::forward<U>(*data));
  ++size_;
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
template <typename U>
void Vector<T, inlineCapacity, Allocator>::insert(wtf_size_t position,
                                                  const U* data,
                                                  wtf_size_t data_size) {
  DCHECK(Allocator::IsAllocationAllowed());
  CHECK_LE(position, size());
  wtf_size_t new_size = size_ + data_size;
  if (new_size > capacity()) {
    data = ExpandCapacity(new_size, data);
    DCHECK(begin());
  }
  CHECK_GE(new_size, size_);
  ANNOTATE_CHANGE_SIZE(begin(), capacity(), size_, new_size);
  T* spot = begin() + position;
  TypeOperations::MoveOverlapping(spot, end(), spot + data_size);
  VectorCopier<VectorTraits<T>::kCanCopyWithMemcpy, T,
               Allocator>::UninitializedCopy(data, &data[data_size], spot);
  size_ = new_size;
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
template <typename U>
void Vector<T, inlineCapacity, Allocator>::InsertAt(T* position, U&& val) {
  insert(position - begin(), val);
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
template <typename U>
void Vector<T, inlineCapacity, Allocator>::InsertAt(T* position,
                                                    const U* data,
                                                    wtf_size_t data_size) {
  insert(position - begin(), data, data_size);
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
template <typename U, wtf_size_t otherCapacity, typename OtherAllocator>
inline void Vector<T, inlineCapacity, Allocator>::InsertVector(
    wtf_size_t position,
    const Vector<U, otherCapacity, OtherAllocator>& val) {
  insert(position, val.begin(), val.size());
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
template <typename U>
inline void Vector<T, inlineCapacity, Allocator>::push_front(U&& val) {
  insert(0, std::forward<U>(val));
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
template <typename U>
void Vector<T, inlineCapacity, Allocator>::push_front(const U* data,
                                                      wtf_size_t data_size) {
  insert(0, data, data_size);
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
template <typename U, wtf_size_t otherCapacity, typename OtherAllocator>
inline void Vector<T, inlineCapacity, Allocator>::PrependVector(
    const Vector<U, otherCapacity, OtherAllocator>& val) {
  insert(0, val.begin(), val.size());
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
inline void Vector<T, inlineCapacity, Allocator>::EraseAt(wtf_size_t position) {
  CHECK_LT(position, size());
  T* spot = begin() + position;
  spot->~T();
  TypeOperations::MoveOverlapping(spot + 1, end(), spot);
  ClearUnusedSlots(end() - 1, end());
  ANNOTATE_CHANGE_SIZE(begin(), capacity(), size_, size_ - 1);
  --size_;
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
inline auto Vector<T, inlineCapacity, Allocator>::erase(iterator position)
    -> iterator {
  wtf_size_t index = static_cast<wtf_size_t>(position - begin());
  EraseAt(index);
  return begin() + index;
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
inline void Vector<T, inlineCapacity, Allocator>::EraseAt(wtf_size_t position,
                                                          wtf_size_t length) {
  SECURITY_DCHECK(position <= size());
  if (!length)
    return;
  CHECK_LE(position + length, size());
  T* begin_spot = begin() + position;
  T* end_spot = begin_spot + length;
  TypeOperations::Destruct(begin_spot, end_spot);
  TypeOperations::MoveOverlapping(end_spot, end(), begin_spot);
  ClearUnusedSlots(end() - length, end());
  ANNOTATE_CHANGE_SIZE(begin(), capacity(), size_, size_ - length);
  size_ -= length;
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
inline void Vector<T, inlineCapacity, Allocator>::Reverse() {
  for (wtf_size_t i = 0; i < size_ / 2; ++i)
    std::swap(at(i), at(size_ - 1 - i));
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
inline void swap(Vector<T, inlineCapacity, Allocator>& a,
                 Vector<T, inlineCapacity, Allocator>& b) {
  a.Swap(b);
}

template <typename T,
          wtf_size_t inlineCapacityA,
          wtf_size_t inlineCapacityB,
          typename Allocator>
bool operator==(const Vector<T, inlineCapacityA, Allocator>& a,
                const Vector<T, inlineCapacityB, Allocator>& b) {
  if (a.size() != b.size())
    return false;
  if (a.IsEmpty())
    return true;
  return VectorTypeOperations<T, Allocator>::Compare(a.data(), b.data(),
                                                     a.size());
}

template <typename T,
          wtf_size_t inlineCapacityA,
          wtf_size_t inlineCapacityB,
          typename Allocator>
inline bool operator!=(const Vector<T, inlineCapacityA, Allocator>& a,
                       const Vector<T, inlineCapacityB, Allocator>& b) {
  return !(a == b);
}

// Only defined for HeapAllocator. Used when visiting vector object.
template <typename T, wtf_size_t inlineCapacity, typename Allocator>
template <typename VisitorDispatcher, typename A>
std::enable_if_t<A::kIsGarbageCollected>
Vector<T, inlineCapacity, Allocator>::Trace(VisitorDispatcher visitor) {
  static_assert(Allocator::kIsGarbageCollected,
                "Garbage collector must be enabled.");

  if (this->HasOutOfLineBuffer()) {
    Allocator::TraceVectorBacking(visitor, Buffer(), Base::BufferSlot());
  } else {
    // We should not visit inline buffers, but we still need to register the
    // slot for heap compaction. So, we pass nullptr to this method.
    Allocator::TraceVectorBacking(visitor, static_cast<T*>(nullptr),
                                  Base::BufferSlot());
    if (!Buffer())
      return;
    // Inline buffer requires tracing immediately.
    const T* buffer_begin = Buffer();
    const T* buffer_end = Buffer() + size();
    if (IsTraceableInCollectionTrait<VectorTraits<T>>::value) {
      for (const T* buffer_entry = buffer_begin; buffer_entry != buffer_end;
           buffer_entry++) {
        Allocator::template Trace<T, VectorTraits<T>>(
            visitor, *const_cast<T*>(buffer_entry));
      }
      CheckUnusedSlots(Buffer() + size(), Buffer() + capacity());
    }
  }
}

template <typename T, wtf_size_t inlineCapacity, typename Allocator>
void Vector<T, inlineCapacity, Allocator>::ReallocateBuffer(
    wtf_size_t new_capacity) {
  if (new_capacity <= INLINE_CAPACITY) {
    if (HasInlineBuffer()) {
      Base::ResetBufferPointer();
      return;
    }
    // Shrinking to inline buffer from out-of-line one.
    T *old_begin = begin(), *old_end = end();
#ifdef ANNOTATE_CONTIGUOUS_CONTAINER
    const wtf_size_t old_capacity = capacity();
#endif
    Base::ResetBufferPointer();
    TypeOperations::Move(old_begin, old_end, begin());
    ClearUnusedSlots(old_begin, old_end);
    ANNOTATE_DELETE_BUFFER(old_begin, old_capacity, size_);
    Base::DeallocateBuffer(old_begin);
    return;
  }
  // Shrinking/resizing to out-of-line buffer.
  VectorBufferBase<T, Allocator> buffer =
      Base::AllocateTemporaryBuffer(new_capacity);
  ANNOTATE_NEW_BUFFER(buffer.Buffer(), buffer.capacity(), size_);
  // If there was a new out-of-line buffer allocated, there is no need in
  // calling write barriers for entries in that backing store as it is still
  // white.
  TypeOperations::Move(begin(), end(), buffer.Buffer(), HasInlineBuffer());
  ClearUnusedSlots(begin(), end());
  ANNOTATE_DELETE_BUFFER(begin(), capacity(), size_);
  Base::DeallocateBuffer(begin());
  buffer.MoveBufferInto(*this);
  Allocator::BackingWriteBarrier(begin());
}

}  // namespace WTF

namespace base {

#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ <= 7
// Workaround for g++7 and earlier family.
// Due to https://gcc.gnu.org/bugzilla/show_bug.cgi?id=80654, without this
// base::Optional<WTF::Vector<T>> where T is non-copyable causes a compile
// error. As we know it is not trivially copy constructible, explicitly declare
// so.
//
// It completes the declaration in base/template_util.h that was provided
// for std::vector
template <typename T>
struct is_trivially_copy_constructible<WTF::Vector<T>> : std::false_type {};
#endif

}  // namespace base

using WTF::Vector;

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_VECTOR_H_
