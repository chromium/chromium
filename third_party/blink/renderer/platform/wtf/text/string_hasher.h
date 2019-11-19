/*
 * Copyright (C) 2005, 2006, 2008, 2010, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_STRING_HASHER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_STRING_HASHER_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"

namespace WTF {

// Paul Hsieh's SuperFastHash
// http://www.azillionmonkeys.com/qed/hash.html

// LChar data is interpreted as Latin-1-encoded (zero extended to 16 bits).

// NOTE: The hash computation here must stay in sync with
// build/scripts/hasher.py.

// Golden ratio. Arbitrary start value to avoid mapping all zeros to a hash
// value of zero.
static const unsigned kStringHashingStartValue = 0x9E3779B9U;

class StringHasher {
  DISALLOW_NEW();

 public:
  static const unsigned kFlagCount =
      8;  // Save 8 bits for StringImpl to use as flags.

  StringHasher()
      : hash_(kStringHashingStartValue),
        has_pending_character_(false),
        pending_character_(0) {}

  // The hasher hashes two characters at a time, and thus an "aligned" hasher is
  // one where an even number of characters have been added. Callers that
  // always add characters two at a time can use the "assuming aligned"
  // functions.
  void AddCharactersAssumingAligned(UChar a, UChar b) {
    DCHECK(!has_pending_character_);
    hash_ += a;
    hash_ = (hash_ << 16) ^ ((b << 11) ^ hash_);
    hash_ += hash_ >> 11;
  }

  void AddCharacter(UChar character) {
    if (has_pending_character_) {
      has_pending_character_ = false;
      AddCharactersAssumingAligned(pending_character_, character);
      return;
    }

    pending_character_ = character;
    has_pending_character_ = true;
  }

  void AddCharacters(UChar a, UChar b) {
    if (has_pending_character_) {
#if DCHECK_IS_ON()
      has_pending_character_ = false;
#endif
      AddCharactersAssumingAligned(pending_character_, a);
      pending_character_ = b;
#if DCHECK_IS_ON()
      has_pending_character_ = true;
#endif
      return;
    }

    AddCharactersAssumingAligned(a, b);
  }

  template <typename T, UChar Converter(T)>
  void AddCharactersAssumingAligned(const T* data, unsigned length) {
    DCHECK(!has_pending_character_);

    bool remainder = length & 1;
    length >>= 1;

    while (length--) {
      AddCharactersAssumingAligned(Converter(data[0]), Converter(data[1]));
      data += 2;
    }

    if (remainder)
      AddCharacter(Converter(*data));
  }

  template <typename T>
  void AddCharactersAssumingAligned(const T* data, unsigned length) {
    AddCharactersAssumingAligned<T, DefaultConverter>(data, length);
  }

  template <typename T, UChar Converter(T)>
  void AddCharacters(const T* data, unsigned length) {
    if (has_pending_character_ && length) {
      has_pending_character_ = false;
      AddCharactersAssumingAligned(pending_character_, Converter(*data++));
      --length;
    }
    AddCharactersAssumingAligned<T, Converter>(data, length);
  }

  template <typename T>
  void AddCharacters(const T* data, unsigned length) {
    AddCharacters<T, DefaultConverter>(data, length);
  }

  unsigned HashWithTop8BitsMasked() const {
    unsigned result = AvalancheBits();

    // Reserving space from the high bits for flags preserves most of the hash's
    // value, since hash lookup typically masks out the high bits anyway.
    result &= (1U << (sizeof(result) * 8 - kFlagCount)) - 1;

    // This avoids ever returning a hash code of 0, since that is used to
    // signal "hash not computed yet". Setting the high bit maintains
    // reasonable fidelity to a hash code of 0 because it is likely to yield
    // exactly 0 when hash lookup masks out the high bits.
    if (!result)
      result = 0x80000000 >> kFlagCount;

    return result;
  }

  unsigned GetHash() const {
    unsigned result = AvalancheBits();

    // This avoids ever returning a hash code of 0, since that is used to
    // signal "hash not computed yet". Setting the high bit maintains
    // reasonable fidelity to a hash code of 0 because it is likely to yield
    // exactly 0 when hash lookup masks out the high bits.
    if (!result)
      result = 0x80000000;

    return result;
  }

  template <typename T, UChar Converter(T)>
  static unsigned ComputeHashAndMaskTop8Bits(const T* data, unsigned length) {
    StringHasher hasher;
    hasher.AddCharactersAssumingAligned<T, Converter>(data, length);
    return hasher.HashWithTop8BitsMasked();
  }

  template <typename T>
  static unsigned ComputeHashAndMaskTop8Bits(const T* data, unsigned length) {
    return ComputeHashAndMaskTop8Bits<T, DefaultConverter>(data, length);
  }

  template <typename T, UChar Converter(T)>
  static unsigned ComputeHash(const T* data, unsigned length) {
    StringHasher hasher;
    hasher.AddCharactersAssumingAligned<T, Converter>(data, length);
    return hasher.GetHash();
  }

  template <typename T>
  static unsigned ComputeHash(const T* data, unsigned length) {
    return ComputeHash<T, DefaultConverter>(data, length);
  }

  static unsigned HashMemory(const void* data, unsigned length) {
    // FIXME: Why does this function use the version of the hash that drops the
    // top 8 bits?  We want that for all string hashing so we can use those
    // bits in StringImpl and hash strings consistently, but I don't see why
    // we'd want that for general memory hashing.
    DCHECK(!(length % 2));
    return ComputeHashAndMaskTop8Bits<UChar>(static_cast<const UChar*>(data),
                                             length / sizeof(UChar));
  }

  template <size_t length>
  static unsigned HashMemory(const void* data) {
    static_assert(!(length % 2), "length must be a multiple of two");
    return HashMemory(data, length);
  }

 private:
  static UChar DefaultConverter(UChar character) { return character; }

  static UChar DefaultConverter(LChar character) { return character; }

  unsigned AvalancheBits() const {
    unsigned result = hash_;

    // Handle end case.
    if (has_pending_character_) {
      result += pending_character_;
      result ^= result << 11;
      result += result >> 17;
    }

    // Force "avalanching" of final 31 bits.
    result ^= result << 3;
    result += result >> 5;
    result ^= result << 2;
    result += result >> 15;
    result ^= result << 10;

    return result;
  }

  unsigned hash_;
  bool has_pending_character_;
  UChar pending_character_;
};

}  // namespace WTF

using WTF::StringHasher;

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_STRING_HASHER_H_
