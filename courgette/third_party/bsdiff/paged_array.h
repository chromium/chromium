// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// PagedArray implements an array stored using many fixed-size pages.
//
// PagedArray is a work-around to allow large arrays to be allocated when there
// is too much address space fragmentation for allocating the large arrays as
// contiguous arrays.

#ifndef COURGETTE_THIRD_PARTY_BSDIFF_PAGED_ARRAY_H_
#define COURGETTE_THIRD_PARTY_BSDIFF_PAGED_ARRAY_H_

#include <cstddef>
#include <iterator>
#include <type_traits>

#include "base/check.h"
#include "base/logging.h"
#include "base/process/memory.h"

namespace courgette {

// Page size of 2^18 * sizeof(T) is 1MB for T = int32_t.
constexpr int kPagedArrayDefaultPageLogSize = 18;

template <typename T, int LOG_PAGE_SIZE = kPagedArrayDefaultPageLogSize>
class PagedArray;

// A random access iterator with pointer-like semantics, for PagedArray.
template <typename ContainerType, typename T>
class PagedArray_iterator {
 public:
  using ThisType = PagedArray_iterator<ContainerType, T>;
  using difference_type = ptrdiff_t;
  using value_type = typename std::remove_const<T>::type;
  using reference = T&;
  using pointer = T*;
  using iterator_category = std::random_access_iterator_tag;

  PagedArray_iterator() : array_(nullptr), index_(0U) {}
  PagedArray_iterator(ContainerType* array, size_t index)
      : array_(array), index_(index) {}

  template <typename ContainerType2, typename T2>
  PagedArray_iterator(const PagedArray_iterator<ContainerType2, T2>& it)
      : array_(it.array_), index_(it.index_) {}

  PagedArray_iterator(std::nullptr_t) : array_(nullptr), index_(0) {}

  ~PagedArray_iterator() = default;

  reference operator*() const { return (*array_)[index_]; }
  reference operator[](size_t idx) const { return (*array_)[index_ + idx]; }
  pointer operator->() const { return &(*array_)[index_]; }

  ThisType& operator=(std::nullptr_t) {
    array_ = nullptr;
    index_ = 0;
    return *this;
  }

  ThisType& operator++() {
    ++index_;
    return *this;
  }
  ThisType& operator--() {
    --index_;
    return *this;
  }

  ThisType operator++(int) { return ThisType(array_, index_++); }
  ThisType operator--(int) { return ThisType(array_, index_--); }

  ThisType& operator+=(difference_type delta) {
    index_ += delta;
    return *this;
  }
  ThisType& operator-=(difference_type delta) {
    index_ -= delta;
    return *this;
  }

  ThisType operator+(difference_type offset) const {
    return ThisType(array_, index_ + offset);
  }
  ThisType operator-(difference_type offset) const {
    return ThisType(array_, index_ - offset);
  }

  template <typename ContainerType2, typename T2>
  bool operator==(const PagedArray_iterator<ContainerType2, T2>& it) const {
    return index_ == it.index_ && array_ == it.array_;
  }
  bool operator==(std::nullptr_t) const {
    return index_ == 0 && array_ == nullptr;
  }
  template <typename ContainerType2, typename T2>
  bool operator!=(const PagedArray_iterator<ContainerType2, T2>& it) const {
    return !(*this == it);
  }

  template <typename ContainerType2, typename T2>
  bool operator<(const PagedArray_iterator<ContainerType2, T2>& it) const {
#ifndef NDEBUG
    // For performance, skip the |array_| check in Release builds.
    if (array_ != it.array_)
      return false;
#endif
    return index_ < it.index_;
  }
  template <typename ContainerType2, typename T2>
  bool operator<=(const PagedArray_iterator<ContainerType2, T2>& it) const {
#ifndef NDEBUG
    // For performance, skip the |array_| check in Release builds.
    if (array_ != it.array_)
      return false;
#endif
    return index_ <= it.index_;
  }
  template <typename ContainerType2, typename T2>
  bool operator>(const PagedArray_iterator<ContainerType2, T2>& it) const {
#ifndef NDEBUG
    // For performance, skip the |array_| check in Release builds.
    if (array_ != it.array_)
      return false;
#endif
    return index_ > it.index_;
  }
  template <typename ContainerType2, typename T2>
  bool operator>=(const PagedArray_iterator<ContainerType2, T2>& it) const {
#ifndef NDEBUG
    // For performance, skip the |array_| check in Release builds.
    if (array_ != it.array_)
      return false;
#endif
    return index_ >= it.index_;
  }

  template <typename ContainerType2, typename T2>
  difference_type operator-(
      const PagedArray_iterator<ContainerType2, T2>& it) const {
    return index_ - it.index_;
  }

 private:
  template <typename, typename>
  friend class PagedArray_iterator;

  ContainerType* array_;
  size_t index_;
};

// PagedArray implements an array stored using many fixed-size pages.
template <typename T, int LOG_PAGE_SIZE>
class PagedArray {
  enum {
    // Page size in elements.
    kLogPageSize = LOG_PAGE_SIZE,
    kPageSize = 1 << LOG_PAGE_SIZE
  };

 public:
  using ThisType = PagedArray<T, LOG_PAGE_SIZE>;
  using const_iterator = PagedArray_iterator<const ThisType, const T>;
  using iterator = PagedArray_iterator<ThisType, T>;

  PagedArray() = default;

  PagedArray(const PagedArray&) = delete;
  PagedArray& operator=(const PagedArray&) = delete;

  ~PagedArray() { clear(); }

  iterator begin() { return iterator(this, 0); }
  iterator end() { return iterator(this, size_); }
  const_iterator begin() const { return const_iterator(this, 0); }
  const_iterator end() const { return const_iterator(this, size_); }

  T& operator[](size_t i) {
    size_t page = i >> kLogPageSize;
    size_t offset = i & (kPageSize - 1);
#ifndef NDEBUG
    // Without the #ifndef, DCHECK() will significaltly slow down bsdiff_create
    // even in optimized Release build (about 1.4x).
    DCHECK(page < page_count_);
#endif
    return pages_[page][offset];
  }

  const T& operator[](size_t i) const {
    // Duplicating code here for performance. If we use common code for this
    // then bsdiff_create slows down by ~5% in optimized Release build.
    size_t page = i >> kLogPageSize;
    size_t offset = i & (kPageSize - 1);
#ifndef NDEBUG
    // Without the #ifndef, DCHECK() will significaltly slow down bsdiff_create
    // even in optimized Release build (about 1.4x).
    DCHECK(page < page_count_);
#endif
    return pages_[page][offset];
  }

  // Allocates storage for |size| elements. Returns true on success and false if
  // allocation fails.
  bool Allocate(size_t size) {
    clear();
    size_ = size;
    size_t pages_needed = (size_ + kPageSize - 1) >> kLogPageSize;
    if (!base::UncheckedMalloc(sizeof(T*) * pages_needed,
                               reinterpret_cast<void**>(&pages_))) {
      return false;
    }

    for (page_count_ = 0; page_count_ < pages_needed; ++page_count_) {
      T* block = nullptr;
      if (!base::UncheckedMalloc(sizeof(T) * kPageSize,
                                 reinterpret_cast<void**>(&block))) {
        clear();
        return false;
      }
      pages_[page_count_] = block;
    }
    return true;
  }

  // Releases all storage.  May be called more than once.
  void clear() {
    if (pages_ != nullptr) {
      while (page_count_ != 0) {
        --page_count_;
        free(pages_[page_count_]);
      }
      free(pages_);
      pages_ = nullptr;
    }
  }

 private:
  T** pages_ = nullptr;
  size_t size_ = 0U;
  size_t page_count_ = 0U;
};

}  // namespace courgette

#endif  // COURGETTE_THIRD_PARTY_BSDIFF_PAGED_ARRAY_H_
