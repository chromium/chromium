// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_CODE_POINT_ITERATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_CODE_POINT_ITERATOR_H_

#include <unicode/utf16.h>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/memory/stack_allocated.h"
#include "base/types/to_address.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

//
// A code point iterator for 8-bits or 16-bits strings.
//
// This iterates 32-bit code points from 8-bits or 16-bits strings. In 8-bits
// strings, a code unit is 8-bits, and it's always a code point. In UTF16
// 16-bits strings, a code unit is 16-bits, and a code point is either a code
// unit (16-bits) or two code units (32-bits.)
//
// An instance must not outlive the target.
//
class CodePointIterator {
  STACK_ALLOCATED();

 public:
  // A code point iterator for 16-bits strings.
  class Utf16 {
    STACK_ALLOCATED();

   public:
    // Constructor for creating a 'begin' iterator from a span of 16-bit
    // characters.
    template <typename CharT>
      requires(std::is_integral_v<CharT> && sizeof(CharT) == 2)
    explicit Utf16(base::span<const CharT> span)
        : Utf16(reinterpret_cast<const UChar*>(span.data()), span.size()) {}

    // Constructor for creating a 'begin' iterator from a span of 16-bit
    // characters.
    template <typename CharT>
      requires(std::is_integral_v<CharT> && sizeof(CharT) == 2)
    static Utf16 End(base::span<const CharT> span) {
      return Utf16{reinterpret_cast<const UChar*>(base::to_address(span.end())),
                   0};
    }

    // Returns a `end` iterator for `this` iterator.
    Utf16 EndForThis() const {
      // SAFETY: safe if `data_` points to at least `length_` code units, which
      // should be guaranteed by the constructor or operator++
      auto* end = UNSAFE_BUFFERS(data_ + length_);
      return Utf16{end, 0};
    }

    bool operator==(const Utf16& other) const { return data_ == other.data_; }

    UChar32 operator*() const;
    void operator++();

    // Advance the iterator by the number of code units. Note that this is
    // different from `operator+`, which advances by the number of code points.
    void AdvanceByCodeUnits(wtf_size_t by);

    // Similar to `std::distance`, but in the code units, not code points.
    wtf_size_t DistanceByCodeUnits(const Utf16& other) const {
      return data_ - other.data_;
    }

   private:
    friend class CodePointIterator;

    Utf16(const UChar* data, wtf_size_t length)
        : data_(data), length_(length) {}

    const UChar* data_;
    wtf_size_t length_;
    // Caches the length of the current code point, in the number of code units.
    mutable wtf_size_t code_point_length_ = 0;
  };

  // Create a `begin()` iterator.
  template <class T>
  explicit CodePointIterator(const T& string)
      : CodePointIterator(string.Is8Bit(),
                          string.RawByteSpan().data(),
                          string.length()) {}

  // Create an `end()` iterator.
  template <class T>
  static CodePointIterator End(const T& string) {
    return CodePointIterator(
        string.Is8Bit(),
        string.Is8Bit()
            ? static_cast<const void*>(base::to_address(string.Span8().end()))
            : static_cast<const void*>(base::to_address(string.Span16().end())),
        0);
  }

  UChar32 operator*() const { return is_8bit_ ? *Data() : *utf16_; }
  void operator++() {
    is_8bit_
        ? static_cast<void>(
              // SAFETY: safe to increment as we're not deref, caller
              // should check the returned value is not equal to
              // `CodePointIterator::End(<same string>)` before dereferencing.
              UNSAFE_BUFFERS(++DataRef()))
        : ++utf16_;
  }

  bool operator==(const CodePointIterator& other) const {
    DCHECK_EQ(is_8bit_, other.is_8bit_);
    return utf16_ == other.utf16_;
  }

 private:
  CodePointIterator(bool is_8bit, const void* data, wtf_size_t len)
      : utf16_(static_cast<const UChar*>(data), len), is_8bit_(is_8bit) {}

  // The 8bit string shares the `data_` and `length_` with `Utf16`.
  const uint8_t* Data() const {
    return reinterpret_cast<const uint8_t*>(utf16_.data_);
  }
  const uint8_t*& DataRef() {
    return *reinterpret_cast<const uint8_t**>(&utf16_.data_);
  }

  Utf16 utf16_;
  bool is_8bit_;
};

inline UChar32 CodePointIterator::Utf16::operator*() const {
  // Get a code point, and cache its length to `code_point_length_`.
  UChar32 ch;
  code_point_length_ = 0;
  // SAFETY: call into icu functions. Consumes at least one code unit, and
  // checks against `length_`.
  UNSAFE_BUFFERS(U16_NEXT(data_, code_point_length_, length_, ch));
  return ch;
}

inline void CodePointIterator::Utf16::AdvanceByCodeUnits(wtf_size_t by) {
  CHECK_LE(by, length_);
  // SAFETY: `data_` is safe to increment by `by` as long as `by` <= `length_`,
  // which is checked at the top of the function.
  UNSAFE_BUFFERS(data_ += by);
  length_ -= by;
  code_point_length_ = 0;
}

inline void CodePointIterator::Utf16::operator++() {
  if (!code_point_length_) [[unlikely]] {
    // `code_point_length_` is cached by `operator*()`. If not, compute it.
    // SAFETY: call into icu functions.
    UNSAFE_BUFFERS(U16_FWD_1(data_, code_point_length_, length_););
  }
  AdvanceByCodeUnits(code_point_length_);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_CODE_POINT_ITERATOR_H_
