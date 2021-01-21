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
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"
#include "third_party/blink/renderer/platform/wtf/container_annotations.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// ContiguousContainer is a container which stores a list of heterogeneous
// items (in particular, of varying sizes), packed next to one another in
// memory. Items are never relocated, so it is safe to store pointers to them
// for the lifetime of the container (unless the item is removed).
//
// Memory is allocated in a series of buffers (with exponential growth). When an
// item is allocated, it is given only the space it requires (possibly with
// enough padding to preserve alignment), rather than the maximum possible size.
// This allows small and large items to coexist without wasting much space.
//
// Since it stores pointers to all of the items it allocates in a vector, it
// supports efficient iteration and indexing. However, for mutation the
// supported operations are limited to appending to the end of the list and
// replacing the last item.
//
// Clients should instantiate ContiguousContainer; ContiguousContainerBase is an
// artifact of the implementation.

class PLATFORM_EXPORT ContiguousContainerBase {
  DISALLOW_NEW();

 public:
  ContiguousContainerBase(const ContiguousContainerBase&) = delete;
  ContiguousContainerBase& operator=(const ContiguousContainerBase&) = delete;
  ContiguousContainerBase(ContiguousContainerBase&&) = delete;
  ContiguousContainerBase& operator=(ContiguousContainerBase&&) = delete;

 protected:
  // The initial capacity will be allocated when the first item is added.
  ContiguousContainerBase(wtf_size_t max_item_size,
                          wtf_size_t initial_capacity_in_bytes);
  ~ContiguousContainerBase();

  wtf_size_t size() const { return items_.size(); }
  bool IsEmpty() const { return !size(); }
  wtf_size_t CapacityInBytes() const;
  wtf_size_t UsedCapacityInBytes() const;
  wtf_size_t MemoryUsageInBytes() const;

  // These do not invoke constructors or destructors.
  uint8_t* Allocate(wtf_size_t item_size, const char* type_name);

  wtf_size_t LastItemSize() const {
    return static_cast<wtf_size_t>(buffers_.back().End() - items_.back());
  }

  using ItemVector = Vector<uint8_t*>;
  ItemVector items_;

 private:
  class Buffer {
   public:
    Buffer(wtf_size_t buffer_size, const char* type_name)
        : capacity_(static_cast<wtf_size_t>(
              WTF::Partitions::BufferPotentialCapacity(buffer_size))),
          begin_(static_cast<uint8_t*>(
              WTF::Partitions::BufferMalloc(capacity_, type_name))),
          end_(begin_) {
      ANNOTATE_NEW_BUFFER(begin_, capacity_, 0);
    }

    ~Buffer() {
      ANNOTATE_DELETE_BUFFER(begin_, capacity_, UsedCapacity());
      WTF::Partitions::BufferFree(begin_);
    }

    wtf_size_t Capacity() const { return capacity_; }
    wtf_size_t UsedCapacity() const {
      return static_cast<wtf_size_t>(end_ - begin_);
    }
    wtf_size_t UnusedCapacity() const { return Capacity() - UsedCapacity(); }
    bool IsEmpty() const { return UsedCapacity() == 0; }

    uint8_t* Allocate(wtf_size_t item_size) {
      DCHECK_GE(UnusedCapacity(), item_size);
      ANNOTATE_CHANGE_SIZE(begin_, capacity_, UsedCapacity(),
                           UsedCapacity() + item_size);
      uint8_t* result = end_;
      end_ += item_size;
      return result;
    }

    uint8_t* End() const { return end_; }

   private:
    // begin_ <= end_ <= begin_ + capacity_
    wtf_size_t capacity_;
    uint8_t* begin_;
    uint8_t* end_;
  };

  Vector<Buffer> buffers_;
  wtf_size_t max_item_size_;
  wtf_size_t initial_capacity_in_bytes_;
};

// For most cases, no alignment stricter than pointer alignment is required. If
// one of the derived classes has stronger alignment requirements (and the
// static_assert fires), set alignment to the LCM of the derived class
// alignments. For small structs without pointers, it may be possible to reduce
// alignment for tighter packing.

template <class BaseItemType, unsigned alignment = sizeof(void*)>
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
    bool operator<(const IteratorWrapper& other) const {
      return it_ < other.it_;
    }
    ValueType& operator*() const { return *reinterpret_cast<ValueType*>(*it_); }
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
  using iterator = IteratorWrapper<ItemVector::iterator, BaseItemType>;
  using const_iterator =
      IteratorWrapper<ItemVector::const_iterator, const BaseItemType>;
  using reverse_iterator =
      IteratorWrapper<ItemVector::reverse_iterator, BaseItemType>;
  using const_reverse_iterator =
      IteratorWrapper<ItemVector::const_reverse_iterator, const BaseItemType>;

  using value_type = BaseItemType;

  ContiguousContainer(wtf_size_t max_item_size,
                      wtf_size_t initial_capacity_in_bytes)
      : ContiguousContainerBase(Align(max_item_size),
                                initial_capacity_in_bytes) {}
  ~ContiguousContainer() {
    for (auto& item : *this) {
      (void)item;  // MSVC incorrectly reports this variable as unused.
      item.~BaseItemType();
    }
  }

  using ContiguousContainerBase::CapacityInBytes;
  using ContiguousContainerBase::IsEmpty;
  using ContiguousContainerBase::MemoryUsageInBytes;
  using ContiguousContainerBase::size;
  using ContiguousContainerBase::UsedCapacityInBytes;

  iterator begin() { return iterator(items_.begin()); }
  iterator end() { return iterator(items_.end()); }
  const_iterator begin() const { return const_iterator(items_.begin()); }
  const_iterator end() const { return const_iterator(items_.end()); }
  reverse_iterator rbegin() { return reverse_iterator(items_.rbegin()); }
  reverse_iterator rend() { return reverse_iterator(items_.rend()); }
  const_reverse_iterator rbegin() const {
    return const_reverse_iterator(items_.rbegin());
  }
  const_reverse_iterator rend() const {
    return const_reverse_iterator(items_.rend());
  }

  BaseItemType& front() { return *begin(); }
  const BaseItemType& front() const { return *begin(); }
  BaseItemType& back() { return *rbegin(); }
  const BaseItemType& back() const { return *rbegin(); }
  BaseItemType& operator[](wtf_size_t index) { return *(begin() + index); }
  const BaseItemType& operator[](wtf_size_t index) const {
    return *(begin() + index);
  }

  template <class DerivedItemType, typename... Args>
  DerivedItemType& AllocateAndConstruct(Args&&... args) {
    static_assert(WTF::IsSubclass<DerivedItemType, BaseItemType>::value,
                  "Must use subclass of BaseItemType.");
    static_assert(alignment % alignof(DerivedItemType) == 0,
                  "Derived type requires stronger alignment.");
    return *new (AlignedAllocate(sizeof(DerivedItemType)))
        DerivedItemType(std::forward<Args>(args)...);
  }

  // Appends a new item using memcpy, then default-constructs a base item
  // in its place. Use with care.
  BaseItemType& AppendByMoving(BaseItemType& item, wtf_size_t size) {
    DCHECK_GE(size, sizeof(BaseItemType));
    void* new_item = AlignedAllocate(size);
    memcpy(new_item, static_cast<void*>(&item), size);
    new (&item) BaseItemType;
    return *static_cast<BaseItemType*>(new_item);
  }

  // The caller must ensure that |size| (the actual size of |item|) is the same
  // as or smaller than the replaced item.
  BaseItemType& ReplaceLastByMoving(BaseItemType& item, wtf_size_t size) {
    DCHECK_GE(size, sizeof(BaseItemType));
    DCHECK_GE(LastItemSize(), size);
    back().~BaseItemType();
    memcpy(static_cast<void*>(&back()), static_cast<void*>(&item), size);
    new (&item) BaseItemType;
    return back();
  }

 private:
  void* AlignedAllocate(wtf_size_t size) {
    void* result = ContiguousContainerBase::Allocate(
        Align(size), WTF_HEAP_PROFILER_TYPE_NAME(BaseItemType));
    DCHECK_EQ(reinterpret_cast<intptr_t>(result) & (alignment - 1), 0u);
    return result;
  }

  static wtf_size_t Align(wtf_size_t size) {
    wtf_size_t aligned_size = alignment * ((size + alignment - 1) / alignment);
    DCHECK_EQ(aligned_size % alignment, 0u);
    DCHECK_GE(aligned_size, size);
    DCHECK_LT(aligned_size, size + alignment);
    return aligned_size;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CONTIGUOUS_CONTAINER_H_
