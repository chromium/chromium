// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef COMPONENTS_ZUCCHINI_BUFFER_VIEW_H_
#define COMPONENTS_ZUCCHINI_BUFFER_VIEW_H_

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <type_traits>

#include "base/check_op.h"
#include "base/ranges/algorithm.h"
#include "components/zucchini/algorithm.h"

namespace zucchini {

// Describes a region within a buffer, with starting offset and size.
struct BufferRegion {
  // The region data are stored as |offset| and |size|, but often it is useful
  // to represent it as an interval [lo(), hi()) = [offset, offset + size).
  size_t lo() const { return offset; }
  size_t hi() const { return offset + size; }

  // Returns whether the Region fits in |[0, container_size)|. Special case:
  // a size-0 region starting at |container_size| fits.
  bool FitsIn(size_t container_size) const {
    return offset <= container_size && container_size - offset >= size;
  }

  // Returns |v| clipped to the inclusive range |[lo(), hi()]|.
  size_t InclusiveClamp(size_t v) const {
    return zucchini::InclusiveClamp(v, lo(), hi());
  }

  // Region data use size_t to match BufferViewBase::size_type, to make it
  // convenient to index into buffer view.
  size_t offset;
  size_t size;
};

namespace internal {

// TODO(huangs): Rename to BasicBufferView.
// BufferViewBase should not be used directly; it is an implementation used for
// both BufferView and MutableBufferView.
template <class T>
class BufferViewBase {
 public:
  using value_type = T;
  using reference = T&;
  using pointer = T*;
  using iterator = T*;
  using const_iterator = typename std::add_const<T>::type*;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;

  static BufferViewBase FromRange(iterator first, iterator last) {
    DCHECK_GE(last, first);
    BufferViewBase ret;
    ret.first_ = first;
    ret.last_ = last;
    return ret;
  }

  BufferViewBase() = default;

  BufferViewBase(iterator first, size_type size)
      : first_(first), last_(first_ + size) {
    DCHECK_GE(last_, first_);
  }

  template <class U>
  BufferViewBase(const BufferViewBase<U>& that)
      : first_(that.begin()), last_(that.end()) {}

  template <class U>
  BufferViewBase(BufferViewBase<U>&& that)
      : first_(that.begin()), last_(that.end()) {}

  BufferViewBase(const BufferViewBase&) = default;
  BufferViewBase& operator=(const BufferViewBase&) = default;

  // Iterators

  iterator begin() const { return first_; }
  iterator end() const { return last_; }
  const_iterator cbegin() const { return begin(); }
  const_iterator cend() const { return end(); }

  // Capacity

  bool empty() const { return first_ == last_; }
  size_type size() const { return last_ - first_; }

  // Returns whether the buffer is large enough to cover |region|.
  bool covers(const BufferRegion& region) const {
    return region.FitsIn(size());
  }

  // Returns whether the buffer is large enough to cover an array starting at
  // |offset| with |num| elements, each taking |elt_size| bytes.
  bool covers_array(size_t offset, size_t num, size_t elt_size) {
    DCHECK_GT(elt_size, 0U);
    // Use subtraction and division to avoid overflow.
    return offset <= size() && (size() - offset) / elt_size >= num;
  }

  // Element access

  // Returns the raw value at specified location |pos|.
  // If |pos| is not within the range of the buffer, the process is terminated.
  reference operator[](size_type pos) const {
    CHECK_LT(pos, size());
    return first_[pos];
  }

  // Returns a sub-buffer described by |region|.
  BufferViewBase operator[](BufferRegion region) const {
    DCHECK_LE(region.offset, size());
    DCHECK_LE(region.size, size() - region.offset);
    return {begin() + region.offset, region.size};
  }

  template <class U>
  U read(size_type pos) const {
    // TODO(huangs): Use can_access<U>(pos) after fixing can_access().
    CHECK_LE(sizeof(U), size());
    CHECK_LE(pos, size() - sizeof(U));
    U ret = {};
    ::memcpy(&ret, begin() + pos, sizeof(U));
    return ret;
  }

  template <class U>
  void write(size_type pos, const U& value) {
    // TODO(huangs): Use can_access<U>(pos) after fixing can_access().
    CHECK_LE(sizeof(U), size());
    CHECK_LE(pos, size() - sizeof(U));
    ::memcpy(begin() + pos, &value, sizeof(U));
  }

  template <class U>
  bool can_access(size_type pos) const {
    return pos < size() && size() - pos >= sizeof(U);
  }

  // Returns a BufferRegion describing the full view, with offset = 0. If the
  // BufferViewBase is derived from another, this does *not* return the
  // original region used for its definition (hence "local").
  BufferRegion local_region() const { return BufferRegion{0, size()}; }

  bool equals(BufferViewBase other) const {
    return base::ranges::equal(*this, other);
  }

  // Modifiers

  void shrink(size_type new_size) {
    DCHECK_LE(first_ + new_size, last_);
    last_ = first_ + new_size;
  }

  // Moves the start of the view forward by n bytes.
  void remove_prefix(size_type n) {
    DCHECK_LE(n, size());
    first_ += n;
  }

  // Moves the start of the view to |it|, which is in range [begin(), end()).
  void seek(iterator it) {
    DCHECK_GE(it, begin());
    DCHECK_LE(it, end());
    first_ = it;
  }

  // Given |origin| that contains |*this|, minimally increase |first_| (possibly
  // by 0) so that |first_ <= last_|, and |first_ - origin.first_| is a multiple
  // of |alignment|. On success, updates |first_| and returns true. Otherwise
  // returns false.
  bool AlignOn(BufferViewBase origin, size_type alignment) {
    DCHECK_GT(alignment, 0U);
    DCHECK_LE(origin.first_, first_);
    DCHECK_GE(origin.last_, last_);
    size_type aligned_size =
        AlignCeil(static_cast<size_type>(first_ - origin.first_), alignment);
    if (aligned_size > static_cast<size_type>(last_ - origin.first_))
      return false;
    first_ = origin.first_ + aligned_size;
    return true;
  }

 private:
  iterator first_ = nullptr;
  iterator last_ = nullptr;
};

}  // namespace internal

// Classes to encapsulate a contiguous sequence of raw data, without owning the
// encapsulated memory regions. These are intended to be used as value types.

using ConstBufferView = internal::BufferViewBase<const uint8_t>;
using MutableBufferView = internal::BufferViewBase<uint8_t>;

}  // namespace zucchini

#endif  // COMPONENTS_ZUCCHINI_BUFFER_VIEW_H_
