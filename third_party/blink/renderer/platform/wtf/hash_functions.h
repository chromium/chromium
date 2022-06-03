/*
 * Copyright (C) 2005, 2006, 2008 Apple Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_HASH_FUNCTIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_HASH_FUNCTIONS_H_

#include <stdint.h>
#include <memory>
#include <type_traits>
#include "base/bit_cast.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/wtf/hash_table_deleted_value_type.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace WTF {

template <size_t size>
struct IntTypes;
template <>
struct IntTypes<1> {
  typedef int8_t SignedType;
  typedef uint8_t UnsignedType;
};
template <>
struct IntTypes<2> {
  typedef int16_t SignedType;
  typedef uint16_t UnsignedType;
};
template <>
struct IntTypes<4> {
  typedef int32_t SignedType;
  typedef uint32_t UnsignedType;
};
template <>
struct IntTypes<8> {
  typedef int64_t SignedType;
  typedef uint64_t UnsignedType;
};

// integer hash function

// Thomas Wang's 32 Bit Mix Function:
// http://www.cris.com/~Ttwang/tech/inthash.htm
inline unsigned HashInt(uint32_t key) {
  key += ~(key << 15);
  key ^= (key >> 10);
  key += (key << 3);
  key ^= (key >> 6);
  key += ~(key << 11);
  key ^= (key >> 16);
  return key;
}

inline unsigned HashInt(uint16_t key16) {
  uint32_t key = key16;
  return HashInt(key);
}

inline unsigned HashInt(uint8_t key8) {
  uint32_t key = key8;
  return HashInt(key);
}

// Thomas Wang's 64 bit Mix Function:
// http://www.cris.com/~Ttwang/tech/inthash.htm
inline unsigned HashInt(uint64_t key) {
  key += ~(key << 32);
  key ^= (key >> 22);
  key += ~(key << 13);
  key ^= (key >> 8);
  key += (key << 3);
  key ^= (key >> 15);
  key += ~(key << 27);
  key ^= (key >> 31);
  return static_cast<unsigned>(key);
}

// Compound integer hash method:
// http://opendatastructures.org/versions/edition-0.1d/ods-java/node33.html#SECTION00832000000000000000
inline unsigned HashInts(unsigned key1, unsigned key2) {
  unsigned short_random1 = 277951225;          // A random 32-bit value.
  unsigned short_random2 = 95187966;           // A random 32-bit value.
  uint64_t long_random = 19248658165952623LL;  // A random, odd 64-bit value.

  uint64_t product =
      long_random * short_random1 * key1 + long_random * short_random2 * key2;
  unsigned high_bits = static_cast<unsigned>(
      product >> (8 * (sizeof(uint64_t) - sizeof(unsigned))));
  return high_bits;
}

template <typename T>
struct IntHash {
  static unsigned GetHash(T key) {
    return HashInt(
        static_cast<typename IntTypes<sizeof(T)>::UnsignedType>(key));
  }
  static bool Equal(T a, T b) { return a == b; }
  static const bool safe_to_compare_to_empty_or_deleted = true;
};

template <typename T>
struct FloatHash {
  typedef typename IntTypes<sizeof(T)>::UnsignedType Bits;
  static unsigned GetHash(T key) { return HashInt(bit_cast<Bits>(key)); }
  static bool Equal(T a, T b) { return bit_cast<Bits>(a) == bit_cast<Bits>(b); }
  static const bool safe_to_compare_to_empty_or_deleted = true;
};

// pointer identity hash function

template <typename T>
struct PtrHash {
  static unsigned GetHash(T* key) {
#if defined(COMPILER_MSVC)
#pragma warning(push)
// work around what seems to be a bug in MSVC's conversion warnings
#pragma warning(disable : 4244)
#endif
    return IntHash<uintptr_t>::GetHash(reinterpret_cast<uintptr_t>(key));
#if defined(COMPILER_MSVC)
#pragma warning(pop)
#endif
  }
  static bool Equal(T* a, T* b) { return a == b; }
  static bool Equal(std::nullptr_t, T* b) { return !b; }
  static bool Equal(T* a, std::nullptr_t) { return !a; }
  static const bool safe_to_compare_to_empty_or_deleted = true;
};

template <typename T>
struct RefPtrHash : PtrHash<T> {
  using PtrHash<T>::GetHash;
  static unsigned GetHash(const scoped_refptr<T>& key) {
    return GetHash(key.get());
  }
  using PtrHash<T>::Equal;
  static bool Equal(const scoped_refptr<T>& a, const scoped_refptr<T>& b) {
    return a == b;
  }
  static bool Equal(T* a, const scoped_refptr<T>& b) { return a == b; }
  static bool Equal(const scoped_refptr<T>& a, T* b) { return a == b; }
};

template <typename T>
struct UniquePtrHash : PtrHash<T> {
  using PtrHash<T>::GetHash;
  static unsigned GetHash(const std::unique_ptr<T>& key) {
    return GetHash(key.get());
  }
  static bool Equal(const std::unique_ptr<T>& a, const std::unique_ptr<T>& b) {
    return a == b;
  }
  static bool Equal(const std::unique_ptr<T>& a, const T* b) {
    return a.get() == b;
  }
  static bool Equal(const T* a, const std::unique_ptr<T>& b) {
    return a == b.get();
  }
};

// Useful compounding hash functions.
inline void AddIntToHash(unsigned& hash, unsigned key) {
  hash = ((hash << 5) + hash) + key;  // Djb2
}

inline void AddFloatToHash(unsigned& hash, float value) {
  AddIntToHash(hash, FloatHash<float>::GetHash(value));
}

// Default hash function for each type.
template <typename T>
struct DefaultHash;

// Actual implementation of DefaultHash.
//
// The case of |isIntegral| == false is not implemented. If you see a compile
// error saying DefaultHashImpl<T, false> is not defined, that's because the
// default hash functions for T are not defined. You need to implement them
// yourself.
template <typename T, bool isIntegral>
struct DefaultHashImpl;

template <typename T>
struct DefaultHashImpl<T, true> {
  using Hash = IntHash<T>;
};

// Canonical implementation of DefaultHash.
template <typename T>
struct DefaultHash
    : DefaultHashImpl<T, std::is_integral<T>::value || std::is_enum<T>::value> {
};

// Specializations of DefaultHash follow.
template <>
struct DefaultHash<float> {
  using Hash = FloatHash<float>;
};
template <>
struct DefaultHash<double> {
  using Hash = FloatHash<double>;
};

// Specializations for pointer types.
template <typename T>
struct DefaultHash<T*> {
  using Hash = PtrHash<T>;
};
template <typename T>
struct DefaultHash<scoped_refptr<T>> {
  using Hash = RefPtrHash<T>;
};
template <typename T>
struct DefaultHash<std::unique_ptr<T>> {
  using Hash = UniquePtrHash<T>;
};

// Specializations for pairs.

// Generic case (T or U is non-integral):
template <typename T, typename U, bool areBothIntegral>
struct PairHashImpl {
  static unsigned GetHash(const std::pair<T, U>& p) {
    return HashInts(DefaultHash<T>::Hash::GetHash(p.first),
                    DefaultHash<U>::Hash::GetHash(p.second));
  }
  static bool Equal(const std::pair<T, U>& a, const std::pair<T, U>& b) {
    return DefaultHash<T>::Hash::Equal(a.first, b.first) &&
           DefaultHash<U>::Hash::Equal(a.second, b.second);
  }
  static const bool safe_to_compare_to_empty_or_deleted =
      DefaultHash<T>::Hash::safe_to_compare_to_empty_or_deleted &&
      DefaultHash<U>::Hash::safe_to_compare_to_empty_or_deleted;
};

// Special version for pairs of integrals:
template <typename T, typename U>
struct PairHashImpl<T, U, true> {
  static unsigned GetHash(const std::pair<T, U>& p) {
    return HashInts(p.first, p.second);
  }
  static bool Equal(const std::pair<T, U>& a, const std::pair<T, U>& b) {
    return PairHashImpl<T, U, false>::Equal(
        a, b);  // Refer to the generic version.
  }
  static const bool safe_to_compare_to_empty_or_deleted =
      PairHashImpl<T, U, false>::safe_to_compare_to_empty_or_deleted;
};

// Combined version:
template <typename T, typename U>
struct PairHash
    : PairHashImpl<T,
                   U,
                   std::is_integral<T>::value && std::is_integral<U>::value> {};

template <typename T, typename U>
struct DefaultHash<std::pair<T, U>> {
  using Hash = PairHash<T, U>;
};

// Wrapper for integral type to extend to have 0 and max keys.
template <typename T>
struct IntegralWithAllKeys {
  IntegralWithAllKeys() : IntegralWithAllKeys(0, ValueType::kEmpty) {}
  explicit IntegralWithAllKeys(T value)
      : IntegralWithAllKeys(value, ValueType::kValid) {}
  explicit IntegralWithAllKeys(HashTableDeletedValueType)
      : IntegralWithAllKeys(0, ValueType::kDeleted) {}

  bool IsHashTableDeletedValue() const {
    return value_type_ == ValueType::kDeleted;
  }

  unsigned Hash() const {
    return HashInts(value_, static_cast<unsigned>(value_type_));
  }

  bool operator==(const IntegralWithAllKeys& b) const {
    return value_ == b.value_ && value_type_ == b.value_type_;
  }

 private:
  enum class ValueType : uint8_t { kEmpty, kValid, kDeleted };

  IntegralWithAllKeys(T value, ValueType value_type)
      : value_(value), value_type_(value_type) {
    static_assert(std::is_integral<T>::value,
                  "Only integral types are supported.");
  }

  T value_;
  ValueType value_type_;
};

// Specialization for integral type to have all possible values for key
// including 0 and max.
template <typename T>
struct IntegralWithAllKeysHash {
  static unsigned GetHash(const IntegralWithAllKeys<T>& key) {
    return key.Hash();
  }
  static bool Equal(const IntegralWithAllKeys<T>& a,
                    const IntegralWithAllKeys<T>& b) {
    return a == b;
  }
  static const bool safe_to_compare_to_empty_or_deleted = true;
};

template <typename T>
struct DefaultHash<IntegralWithAllKeys<T>> {
  using Hash = IntegralWithAllKeysHash<T>;
};

}  // namespace WTF

using WTF::DefaultHash;
using WTF::IntHash;
using WTF::PtrHash;

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_HASH_FUNCTIONS_H_
