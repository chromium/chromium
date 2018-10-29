/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_BIT_VECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_BIT_VECTOR_H_

#include "base/bit_cast.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/wtf_export.h"

namespace WTF {

// This is a space-efficient, resizable bitvector class. In the common case it
// occupies one word, but if necessary, it will inflate this one word to point
// to a single chunk of out-of-line allocated storage to store an arbitrary
// number of bits.
//
// - The bitvector remembers the bound of how many bits can be stored, but this
//   may be slightly greater (by as much as some platform-specific constant)
//   than the last argument passed to ensureSize().
//
// - The bitvector can resize itself automatically (set, clear, get) or can be
//   used in a manual mode, which is faster (quickSet, quickClear, quickGet,
//   ensureSize).
//
// - Accesses assert that you are within bounds.
//
// - Bits are automatically initialized to zero.
//
// On the other hand, this BitVector class may not be the fastest around, since
// it does conditionals on every get/set/clear. But it is great if you need to
// juggle a lot of variable-length BitVectors and you're worried about wasting
// space.

class WTF_EXPORT BitVector {
  DISALLOW_NEW();

 public:
  BitVector() : bits_or_pointer_(MakeInlineBits(0)) {}

  explicit BitVector(size_t num_bits) : bits_or_pointer_(MakeInlineBits(0)) {
    EnsureSize(num_bits);
  }

  BitVector(const BitVector& other) : bits_or_pointer_(MakeInlineBits(0)) {
    (*this) = other;
  }

  ~BitVector() {
    if (IsInline())
      return;
    OutOfLineBits::Destroy(GetOutOfLineBits());
  }

  BitVector& operator=(const BitVector& other) {
    if (IsInline() && other.IsInline())
      bits_or_pointer_ = other.bits_or_pointer_;
    else
      SetSlow(other);
    return *this;
  }

  size_t size() const {
    if (IsInline())
      return MaxInlineBits();
    return GetOutOfLineBits()->NumBits();
  }

  void EnsureSize(size_t num_bits) {
    if (num_bits <= size())
      return;
    ResizeOutOfLine(num_bits);
  }

  // Like ensureSize(), but supports reducing the size of the bitvector.
  void Resize(size_t num_bits);

  void ClearAll();

  bool QuickGet(size_t bit) const {
    SECURITY_CHECK(bit < size());
    return !!(Bits()[bit / BitsInPointer()] &
              (static_cast<uintptr_t>(1) << (bit & (BitsInPointer() - 1))));
  }

  void QuickSet(size_t bit) {
    SECURITY_CHECK(bit < size());
    Bits()[bit / BitsInPointer()] |=
        (static_cast<uintptr_t>(1) << (bit & (BitsInPointer() - 1)));
  }

  void QuickClear(size_t bit) {
    SECURITY_CHECK(bit < size());
    Bits()[bit / BitsInPointer()] &=
        ~(static_cast<uintptr_t>(1) << (bit & (BitsInPointer() - 1)));
  }

  void QuickSet(size_t bit, bool value) {
    if (value)
      QuickSet(bit);
    else
      QuickClear(bit);
  }

  bool Get(size_t bit) const {
    if (bit >= size())
      return false;
    return QuickGet(bit);
  }

  void Set(size_t bit) {
    EnsureSize(bit + 1);
    QuickSet(bit);
  }

  void EnsureSizeAndSet(size_t bit, size_t size) {
    EnsureSize(size);
    QuickSet(bit);
  }

  void Clear(size_t bit) {
    if (bit >= size())
      return;
    QuickClear(bit);
  }

  void Set(size_t bit, bool value) {
    if (value)
      Set(bit);
    else
      Clear(bit);
  }

 private:
  static unsigned BitsInPointer() { return sizeof(void*) << 3; }

  static unsigned MaxInlineBits() { return BitsInPointer() - 1; }

  static size_t ByteCount(size_t bit_count) { return (bit_count + 7) >> 3; }

  static uintptr_t MakeInlineBits(uintptr_t bits) {
    DCHECK(!(bits & (static_cast<uintptr_t>(1) << MaxInlineBits())));
    return bits | (static_cast<uintptr_t>(1) << MaxInlineBits());
  }

  class WTF_EXPORT OutOfLineBits {
    DISALLOW_NEW();

   public:
    size_t NumBits() const { return num_bits_; }
    size_t NumWords() const {
      return (num_bits_ + BitsInPointer() - 1) / BitsInPointer();
    }
    uintptr_t* Bits() { return bit_cast<uintptr_t*>(this + 1); }
    const uintptr_t* Bits() const {
      return bit_cast<const uintptr_t*>(this + 1);
    }

    static OutOfLineBits* Create(size_t num_bits);

    static void Destroy(OutOfLineBits*);

   private:
    OutOfLineBits(size_t num_bits) : num_bits_(num_bits) {}

    size_t num_bits_;
  };

  bool IsInline() const { return bits_or_pointer_ >> MaxInlineBits(); }

  const OutOfLineBits* GetOutOfLineBits() const {
    return bit_cast<const OutOfLineBits*>(bits_or_pointer_ << 1);
  }
  OutOfLineBits* GetOutOfLineBits() {
    return bit_cast<OutOfLineBits*>(bits_or_pointer_ << 1);
  }

  void ResizeOutOfLine(size_t num_bits);
  void SetSlow(const BitVector& other);

  uintptr_t* Bits() {
    if (IsInline())
      return &bits_or_pointer_;
    return GetOutOfLineBits()->Bits();
  }

  const uintptr_t* Bits() const {
    if (IsInline())
      return &bits_or_pointer_;
    return GetOutOfLineBits()->Bits();
  }

  uintptr_t bits_or_pointer_;
};

}  // namespace WTF

using WTF::BitVector;

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_BIT_VECTOR_H_
