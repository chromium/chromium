// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ZUCCHINI_BUFFER_VIEW_H_
#define COMPONENTS_ZUCCHINI_BUFFER_VIEW_H_

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <type_traits>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/memory/raw_span.h"
#include "components/zucchini/algorithm.h"

namespace zucchini {

// Describes a region within a buffer, with starting offset and size.
struct BufferRegion {
  bool operator==(const BufferRegion& other) const = default;

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
template <class ElementType>
class GSL_POINTER BufferViewBase {
 public:
  using element_type = ElementType;
  using span_type = base::raw_span<element_type, DanglingUntriaged>;

  using pointer = span_type::pointer;
  using reference = span_type::reference;
  using const_reference = span_type::const_reference;
  using size_type = span_type::size_type;

  // TODO(crbug.com/439964610): `iterator` and `const_iterator` should be
  // `span_type::iterator` and `span_type::const_iterator` which are backed by
  // `base::CheckedContiguousIterator` hence safe, however for backwards
  // compatibility the code using BufferViewBase assumes iterator is always a
  // pointer.
  using iterator = span_type::pointer;
  using const_iterator = span_type::const_pointer;

  static BufferViewBase FromRange(pointer first, pointer last) {
    return BufferViewBase(UNSAFE_TODO(span_type(first, last)));
  }

  constexpr BufferViewBase() noexcept = default;
  // Support a conversion from BufferViewBase<T> to BufferViewBase<const T>.
  template <typename T>
    requires(std::same_as<std::remove_const_t<element_type>, T> &&
             !std::same_as<element_type, T>)
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr BufferViewBase(const BufferViewBase<T>& other)
      : span_(other.span_) {}
  // TODO(crbug.com/439964610): The second argument type should be
  // `base::StrictNumeric<size_type>`.
  constexpr BufferViewBase(pointer first, size_type count)
      : UNSAFE_TODO(span_(first, count)) {}

  constexpr BufferViewBase(const BufferViewBase& other) noexcept = default;
  constexpr BufferViewBase(BufferViewBase&& other) noexcept = default;
  constexpr BufferViewBase& operator=(const BufferViewBase& other) noexcept =
      default;
  constexpr BufferViewBase& operator=(BufferViewBase&& other) noexcept =
      default;

  // Iterators

  iterator begin() const { return base::to_address(span_.begin()); }
  iterator end() const { return base::to_address(span_.end()); }
  const_iterator cbegin() const { return base::to_address(span_.cbegin()); }
  const_iterator cend() const { return base::to_address(span_.cend()); }

  // Capacity

  constexpr size_type size() const noexcept { return span_.size(); }
  [[nodiscard]] constexpr bool empty() const noexcept { return span_.empty(); }

  // Returns whether the buffer is large enough to cover |region|.
  bool covers(const BufferRegion& region) const {
    return region.FitsIn(size());
  }

  // Returns whether the buffer is large enough to cover an array starting at
  // |offset| with |num| elements, each taking |elt_size| bytes.
  bool covers_array(size_type offset, size_type num, size_type elt_size) const {
    DCHECK_GT(elt_size, 0U);
    // Use subtraction and division to avoid overflow.
    return offset <= size() && (size() - offset) / elt_size >= num;
  }

  // Element access

  // Returns a reference to the raw value at specified location |pos|.
  // If |pos| is not within the range of the buffer, the process is terminated.
  constexpr const_reference operator[](size_type index) const {
    return span_[index];
  }
  constexpr reference operator[](size_type index) { return span_[index]; }

  // Returns a sub-buffer described by |region|.
  BufferViewBase operator[](BufferRegion region) const {
    return span_.subspan(region.offset, region.size);
  }

  // TODO(crbug.com/439964610): The argument type should be
  // `base::StrictNumeric<size_type>`.
  template <class T>
  T read(size_type pos) const {
    T value;
    base::byte_span_from_ref(value).copy_from_nonoverlapping(
        base::as_bytes(span_.subspan(pos)).first(sizeof value));
    return value;
  }

  // TODO(crbug.com/439964610): The first argument type should be
  // `base::StrictNumeric<size_type>`.
  template <class T>
  void write(size_type pos, const T& value) {
    base::as_writable_bytes(span_.subspan(pos))
        .copy_prefix_from(base::byte_span_from_ref(value));
  }

  // TODO(crbug.com/439964610): The argument type should be
  // `base::StrictNumeric<size_type>`.
  template <class T>
  bool can_access(size_type pos) const {
    return pos < size() && size() - pos >= sizeof(T);
  }

  // Returns a BufferRegion describing the full view, with offset = 0. If the
  // BufferViewBase is derived from another, this does *not* return the
  // original region used for its definition (hence "local").
  BufferRegion local_region() const { return BufferRegion{0, size()}; }

  bool equals(const BufferViewBase& other) const {
    return span_ == other.span_;
  }

  // Modifiers

  // TODO(crbug.com/439964610): The argument type should be
  // `base::StrictNumeric<size_type>`.
  void shrink(size_type new_size) { span_ = span_.first(new_size); }

  // Moves the start of the view forward by n bytes.
  // TODO(crbug.com/439964610): The argument type should be
  // `base::StrictNumeric<size_type>`.
  void remove_prefix(size_type n) { span_ = span_.subspan(n); }

  // Moves the start of the view to |it|, which is in range [begin(), end()).
  void seek(pointer it) {
    CHECK_LE(span_.data(), it);
    const size_type offset = it - span_.data();
    span_ = span_.subspan(offset);
  }

  // Given |origin| that contains |*this|, minimally increase |first_| (possibly
  // by 0) so that |first_ <= last_|, and |first_ - origin.first_| is a multiple
  // of |alignment|. On success, updates |first_| and returns true. Otherwise
  // returns false.
  bool AlignOn(const BufferViewBase& origin,
               base::StrictNumeric<size_type> alignment_arg) {
    static_assert(sizeof(element_type) == 1u);

    const size_type alignment = static_cast<size_type>(alignment_arg);

    DCHECK_GT(alignment, 0u);
    CHECK_LE(origin.begin(), begin());
    CHECK_LE(end(), origin.end());

    const size_type offset =
        static_cast<size_type>(span_.data() - origin.span_.data());
    const size_type aligned_size = AlignCeil(offset, alignment);
    if (aligned_size - offset > span_.size_bytes()) {
      return false;
    }
    span_ = span_.subspan(aligned_size - offset);
    return true;
  }

 private:
  template <typename T>
  friend class BufferViewBase;

  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr BufferViewBase(base::span<element_type> other) noexcept
      : span_(other) {}

  span_type span_;
};

}  // namespace internal

// Classes to encapsulate a contiguous sequence of raw data, without owning the
// encapsulated memory regions. These are intended to be used as value types.

using ConstBufferView = internal::BufferViewBase<const uint8_t>;
using MutableBufferView = internal::BufferViewBase<uint8_t>;

}  // namespace zucchini

#endif  // COMPONENTS_ZUCCHINI_BUFFER_VIEW_H_
