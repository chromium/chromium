// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CONTIGUOUS_CONTAINER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CONTIGUOUS_CONTAINER_H_

#include <cstddef>
#include <iterator>
#include <memory>
#include <utility>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/type_traits.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// ContiguousContainer is a container which stores a list of heterogeneous
// objects (in particular, of varying sizes), packed next to one another in
// memory. Objects are never relocated, so it is safe to store pointers to them
// for the lifetime of the container (unless the object is removed).
//
// Memory is allocated in a series of buffers (with exponential growth). When an
// object is allocated, it is given only the space it requires (possibly with
// enough padding to preserve alignment), rather than the maximum possible size.
// This allows small and large objects to coexist without wasting much space.
//
// Since it stores pointers to all of the objects it allocates in a vector, it
// supports efficient iteration and indexing. However, for mutation the
// supported operations are limited to appending to, and removing from, the end
// of the list.
//
// Clients should instantiate ContiguousContainer; ContiguousContainerBase is an
// artifact of the implementation.

class PLATFORM_EXPORT ContiguousContainerBase {
  DISALLOW_NEW();

 protected:
  explicit ContiguousContainerBase(size_t max_object_size);
  ContiguousContainerBase(ContiguousContainerBase&&);
  ~ContiguousContainerBase();

  ContiguousContainerBase& operator=(ContiguousContainerBase&&);

  size_t size() const { return elements_.size(); }
  bool IsEmpty() const { return !size(); }
  size_t CapacityInBytes() const;
  size_t UsedCapacityInBytes() const;
  size_t MemoryUsageInBytes() const;

  // These do not invoke constructors or destructors.
  void ReserveInitialCapacity(size_t, const char* type_name);
  void* Allocate(size_t object_size, const char* type_name);
  void RemoveLast();
  void Clear();
  void Swap(ContiguousContainerBase&);

  // Discards excess buffer capacity. Intended for use when no more appending
  // is anticipated.
  void ShrinkToFit();

  Vector<void*> elements_;

 private:
  class Buffer;

  Buffer* AllocateNewBufferForNextAllocation(size_t, const char* type_name);

  Vector<std::unique_ptr<Buffer>> buffers_;
  unsigned end_index_;
  size_t max_object_size_;

  DISALLOW_COPY_AND_ASSIGN(ContiguousContainerBase);
};

// For most cases, no alignment stricter than pointer alignment is required. If
// one of the derived classes has stronger alignment requirements (and the
// static_assert fires), set alignment to the LCM of the derived class
// alignments. For small structs without pointers, it may be possible to reduce
// alignment for tighter packing.

template <class BaseElementType, unsigned alignment = sizeof(void*)>
class ContiguousContainer : public ContiguousContainerBase {
 private:
  // Declares itself as a forward iterator, but also supports a few more
  // things. The whole random access iterator interface is a bit much.
  template <typename BaseIterator, typename ValueType>
  class IteratorWrapper
      : public std::iterator<std::forward_iterator_tag, ValueType> {
    DISALLOW_NEW();

   public:
    IteratorWrapper() = default;
    bool operator==(const IteratorWrapper& other) const {
      return it_ == other.it_;
    }
    bool operator!=(const IteratorWrapper& other) const {
      return it_ != other.it_;
    }
    ValueType& operator*() const { return *static_cast<ValueType*>(*it_); }
    ValueType* operator->() const { return &operator*(); }
    IteratorWrapper operator+(std::ptrdiff_t n) const {
      return IteratorWrapper(it_ + n);
    }
    IteratorWrapper operator++(int) {
      IteratorWrapper tmp = *this;
      ++it_;
      return tmp;
    }
    std::ptrdiff_t operator-(const IteratorWrapper& other) const {
      return it_ - other.it_;
    }
    IteratorWrapper& operator++() {
      ++it_;
      return *this;
    }

   private:
    explicit IteratorWrapper(const BaseIterator& it) : it_(it) {}
    BaseIterator it_;
    friend class ContiguousContainer;
  };

 public:
  using iterator = IteratorWrapper<Vector<void*>::iterator, BaseElementType>;
  using const_iterator =
      IteratorWrapper<Vector<void*>::const_iterator, const BaseElementType>;
  using reverse_iterator =
      IteratorWrapper<Vector<void*>::reverse_iterator, BaseElementType>;
  using const_reverse_iterator =
      IteratorWrapper<Vector<void*>::const_reverse_iterator,
                      const BaseElementType>;

  using value_type = BaseElementType;

  explicit ContiguousContainer(size_t max_object_size)
      : ContiguousContainerBase(Align(max_object_size)) {}

  ContiguousContainer(size_t max_object_size, size_t initial_size_bytes)
      : ContiguousContainer(max_object_size) {
    ReserveInitialCapacity(std::max(max_object_size, initial_size_bytes),
                           WTF_HEAP_PROFILER_TYPE_NAME(BaseElementType));
  }

  ContiguousContainer(ContiguousContainer&& source)
      : ContiguousContainerBase(std::move(source)) {}

  ~ContiguousContainer() {
    for (auto& element : *this) {
      (void)element;  // MSVC incorrectly reports this variable as unused.
      element.~BaseElementType();
    }
  }

  ContiguousContainer& operator=(ContiguousContainer&& source) {
    // Must clear in the derived class to ensure that element destructors
    // care called.
    Clear();

    ContiguousContainerBase::operator=(std::move(source));
    return *this;
  }

  using ContiguousContainerBase::CapacityInBytes;
  using ContiguousContainerBase::IsEmpty;
  using ContiguousContainerBase::MemoryUsageInBytes;
  using ContiguousContainerBase::ShrinkToFit;
  using ContiguousContainerBase::size;
  using ContiguousContainerBase::UsedCapacityInBytes;

  iterator begin() { return iterator(elements_.begin()); }
  iterator end() { return iterator(elements_.end()); }
  const_iterator begin() const { return const_iterator(elements_.begin()); }
  const_iterator end() const { return const_iterator(elements_.end()); }
  reverse_iterator rbegin() { return reverse_iterator(elements_.rbegin()); }
  reverse_iterator rend() { return reverse_iterator(elements_.rend()); }
  const_reverse_iterator rbegin() const {
    return const_reverse_iterator(elements_.rbegin());
  }
  const_reverse_iterator rend() const {
    return const_reverse_iterator(elements_.rend());
  }

  BaseElementType& First() { return *begin(); }
  const BaseElementType& First() const { return *begin(); }
  BaseElementType& Last() { return *rbegin(); }
  const BaseElementType& Last() const { return *rbegin(); }
  BaseElementType& operator[](size_t index) { return *(begin() + index); }
  const BaseElementType& operator[](size_t index) const {
    return *(begin() + index);
  }

  template <class DerivedElementType, typename... Args>
  DerivedElementType& AllocateAndConstruct(Args&&... args) {
    static_assert(WTF::IsSubclass<DerivedElementType, BaseElementType>::value,
                  "Must use subclass of BaseElementType.");
    static_assert(alignment % alignof(DerivedElementType) == 0,
                  "Derived type requires stronger alignment.");
    return *new (AlignedAllocate(sizeof(DerivedElementType)))
        DerivedElementType(std::forward<Args>(args)...);
  }

  void RemoveLast() {
    DCHECK(!IsEmpty());
    Last().~BaseElementType();
    ContiguousContainerBase::RemoveLast();
  }

  DISABLE_CFI_PERF
  void Clear() {
    for (auto& element : *this) {
      (void)element;  // MSVC incorrectly reports this variable as unused.
      element.~BaseElementType();
    }
    ContiguousContainerBase::Clear();
  }

  void Swap(ContiguousContainer& other) {
    ContiguousContainerBase::Swap(other);
  }

  // Appends a new element using memcpy, then default-constructs a base
  // element in its place. Use with care.
  BaseElementType& AppendByMoving(BaseElementType& item, size_t size) {
    DCHECK_GE(size, sizeof(BaseElementType));
    void* new_item = AlignedAllocate(size);
    memcpy(new_item, static_cast<void*>(&item), size);
    new (&item) BaseElementType;
    return *static_cast<BaseElementType*>(new_item);
  }

 private:
  void* AlignedAllocate(size_t size) {
    void* result = ContiguousContainerBase::Allocate(
        Align(size), WTF_HEAP_PROFILER_TYPE_NAME(BaseElementType));
    DCHECK_EQ(reinterpret_cast<intptr_t>(result) & (alignment - 1), 0u);
    return result;
  }

  static size_t Align(size_t size) {
    size_t aligned_size = alignment * ((size + alignment - 1) / alignment);
    DCHECK_EQ(aligned_size % alignment, 0u);
    DCHECK_GE(aligned_size, size);
    DCHECK_LT(aligned_size, size + alignment);
    return aligned_size;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CONTIGUOUS_CONTAINER_H_
