// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_LITERAL_BUFFER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_LITERAL_BUFFER_H_

#include <algorithm>
#include <memory>
#include <type_traits>
#include "base/bits.h"
#include "base/compiler_specific.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

// For ASAN builds, disable inline buffers completely as they cause various
// issues.
#ifdef ANNOTATE_CONTIGUOUS_CONTAINER
#define BUFFER_INLINE_CAPACITY 0
#else
#define BUFFER_INLINE_CAPACITY kInlineSize
#endif

// LiteralBuffer is an optimized version of Vector for LChar and UChar
// characters. In particular `AddChar` is faster than `push_back`, since
// it avoids unnecessary register spills. See https://crbug.com/1205338.
template <typename T, wtf_size_t kInlineSize>
class LiteralBuffer {
  static_assert(std::is_same<LChar, T>::value || std::is_same<UChar, T>::value,
                "T must be a character type");

 public:
  LiteralBuffer() = default;
  LiteralBuffer(const LiteralBuffer& other) { *this = other; }
  LiteralBuffer(LiteralBuffer&& other) { *this = std::move(other); }

  ~LiteralBuffer() {
    if (!is_stored_inline())
      WTF::Partitions::BufferFree(begin_);
  }

  template <wtf_size_t kOtherInlineSize>
  LiteralBuffer& operator=(const LiteralBuffer<T, kOtherInlineSize>& other) {
    if (this->begin_ == other.data())
      return *this;
    wtf_size_t other_size = other.size();
    if (capacity() < other_size) {
      // Create large-enough heap-allocated storage.
      if (!is_stored_inline())
        WTF::Partitions::BufferFree(begin_);
      begin_ = static_cast<T*>(WTF::Partitions::BufferMalloc(
          AllocationSize(other_size), "LiteralBuffer"));
      end_of_storage_ = begin_ + other_size;
    }
    std::copy_n(other.data(), other_size, begin_);
    end_ = begin_ + other_size;
    return *this;
  }

  LiteralBuffer& operator=(LiteralBuffer&& other) {
    if (this == &other)
      return *this;
    if (!other.is_stored_inline()) {
      if (!is_stored_inline())
        WTF::Partitions::BufferFree(begin_);
      begin_ = other.begin_;
      end_ = other.end_;
      end_of_storage_ = other.end_of_storage_;
      other.begin_ = &other.inline_storage[0];
      other.end_ = other.begin_;
      other.end_of_storage_ = other.begin_ + BUFFER_INLINE_CAPACITY;
    } else {
      DCHECK_GE(capacity(), other.size());  // Sanity check.
      wtf_size_t other_size = other.size();
      std::copy_n(other.data(), other_size, begin_);
      end_ = begin_ + other_size;
    }
    return *this;
  }

  ALWAYS_INLINE const T* data() const { return begin_; }
  ALWAYS_INLINE wtf_size_t size() const {
    return base::checked_cast<wtf_size_t>(end_ - begin_);
  }

  ALWAYS_INLINE bool IsEmpty() const { return size() == 0; }

  ALWAYS_INLINE const T& operator[](wtf_size_t index) const {
    CHECK_GT(size(), index);
    return begin_[index];
  }

  // Clear without freeing any storage.
  ALWAYS_INLINE void clear() { end_ = begin_; }

  ALWAYS_INLINE void AddChar(T val) {
    if (UNLIKELY(end_ == end_of_storage_))
      end_ = Grow();
    *end_++ = val;
  }

  template <typename OtherT, wtf_size_t kOtherSize>
  void AppendLiteral(const LiteralBuffer<OtherT, kOtherSize>& val) {
    static_assert(sizeof(T) >= sizeof(OtherT),
                  "T is not big enough to contain OtherT");
    size_t count = val.size();
    size_t new_size = size() + count;
    if (capacity() < new_size)
      Grow(new_size);
    std::copy_n(val.data(), count, end_);
    end_ += count;
  }

  String AsString() const { return String(data(), size()); }

  String AsString8() const {
    if (std::is_same<T, LChar>::value)
      return AsString();
    return String::Make8BitFrom16BitSource(data(), size());
  }

  AtomicString AsAtomicString() const { return AtomicString(data(), size()); }

 private:
  // NOTE: we use pointers to the beginning and the end of the buffer, instead
  // of tuple (begin, size, capacity). This makes access of the next characters
  // faster when `AddChar` is inlined, since `end_` is readily available in a
  // register.
  T* begin_ = &inline_storage[0];
  T* end_ = begin_;
  T* end_of_storage_ = begin_ + BUFFER_INLINE_CAPACITY;
  T inline_storage[BUFFER_INLINE_CAPACITY];

  size_t AllocationSize(size_t capacity) {
    return WTF::PartitionAllocator::QuantizedSize<T>(capacity);
  }

  ALWAYS_INLINE size_t capacity() const { return end_of_storage_ - begin_; }

  ALWAYS_INLINE bool is_stored_inline() const {
    return begin_ == &inline_storage[0];
  }

  size_t RoundUpToPowerOfTwo(size_t value) {
    constexpr size_t digits = 8 * sizeof(size_t);
    static_assert(digits == 32 || digits == 64,
                  "size_t must be either 32 or 64 bits");
    DCHECK_LE(value, size_t{1} << (digits - 1));
    if (value)
      --value;
    return size_t{1} << (digits - base::bits::CountLeadingZeroBits(value));
  }

  // Grows the backing store by a factor of two. Returns the new end of the used
  // storage (this reduces binary size).
  NOINLINE T* Grow() { return Grow(0); }

  // Grows the backing store by a factor of two, and at least to `min_capacity`.
  NOINLINE T* Grow(size_t min_capacity) {
    DCHECK_GE(end_, begin_);
    size_t in_use = end_ - begin_;
    size_t new_capacity =
        RoundUpToPowerOfTwo(std::max(min_capacity, 2 * capacity()));
    T* new_storage = static_cast<T*>(WTF::Partitions::BufferMalloc(
        AllocationSize(new_capacity), "LiteralBuffer"));
    std::copy_n(begin_, in_use, new_storage);
    if (!is_stored_inline())
      WTF::Partitions::BufferFree(begin_);
    begin_ = new_storage;
    end_ = new_storage + in_use;
    end_of_storage_ = new_storage + new_capacity;
    return end_;
  }
};

#undef BUFFER_INLINE_CAPACITY

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_LITERAL_BUFFER_H_
