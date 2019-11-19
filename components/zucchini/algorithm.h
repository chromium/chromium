// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ZUCCHINI_ALGORITHM_H_
#define COMPONENTS_ZUCCHINI_ALGORITHM_H_

#include <stddef.h>

#include <algorithm>
#include <type_traits>
#include <vector>

#include "base/logging.h"

// Collection of simple utilities used in for low-level computation.

namespace zucchini {

// Safely determines whether |[begin, begin + size)| is in |[0, bound)|. Note:
// The special case |[bound, bound)| is not considered to be in |[0, bound)|.
template <typename T>
bool RangeIsBounded(T begin, T size, size_t bound) {
  static_assert(std::is_unsigned<T>::value, "Value type must be unsigned.");
  return begin < bound && size <= bound - begin;
}

// Safely determines whether |value| lies in |[begin, begin + size)|. Works
// properly even if |begin + size| overflows -- although such ranges are
// considered pathological, and should fail validation elsewhere.
template <typename T>
bool RangeCovers(T begin, T size, T value) {
  static_assert(std::is_unsigned<T>::value, "Value type must be unsigned.");
  return begin <= value && value - begin < size;
}

// Returns the integer in inclusive range |[lo, hi]| that's closest to |value|.
// This departs from the usual usage of semi-inclusive ranges, but is useful
// because (1) sentinels can use this, (2) a valid output always exists. It is
// assumed that |lo <= hi|.
template <class T>
T InclusiveClamp(T value, T lo, T hi) {
  static_assert(std::is_unsigned<T>::value, "Value type must be unsigned.");
  DCHECK_LE(lo, hi);
  return value <= lo ? lo : (value >= hi ? hi : value);
}

// Returns the minimum multiple of |m| that's no less than |x|. Assumes |m > 0|
// and |x| is sufficiently small so that no overflow occurs.
template <class T>
constexpr T AlignCeil(T x, T m) {
  static_assert(std::is_unsigned<T>::value, "Value type must be unsigned.");
  return T((x + m - 1) / m) * m;
}

// Specialized alignment helpers that returns the increment to |pos| to get the
// next n-aligned value, where n is in {2, 4}. This is useful for aligning
// iterators relative to a base iterator using:
//   it += IncrementForAlignCeil2(it - base);
template <class T>
inline int IncrementForAlignCeil2(T pos) {
  return static_cast<int>(pos & 1);  // Optimized from (-pos) & 1.
}

template <class T>
inline int IncrementForAlignCeil4(T pos) {
  return static_cast<int>((-pos) & 3);
}

// Sorts values in |container| and removes duplicates.
template <class T>
void SortAndUniquify(std::vector<T>* container) {
  std::sort(container->begin(), container->end());
  container->erase(std::unique(container->begin(), container->end()),
                   container->end());
  container->shrink_to_fit();
}

// Extracts a single bit at |pos| from integer |v|.
template <int pos, typename T>
constexpr T GetBit(T v) {
  return (v >> pos) & 1;
}

// Extracts bits in inclusive range [|lo|, |hi|] from integer |v|, and returns
// the sign-extend result. For example, let the (MSB-first) bits in a 32-bit int
// |v| be:
//   xxxxxxxx xxxxxSii iiiiiiii iyyyyyyy,
//               hi^          lo^       => lo = 7, hi = 18
// To extract "Sii iiiiiiii i", calling
//   GetSignedBits<7, 18>(v);
// produces the sign-extended result:
//   SSSSSSSS SSSSSSSS SSSSSiii iiiiiiii.
template <int lo, int hi, typename T>
constexpr typename std::make_signed<T>::type GetSignedBits(T v) {
  constexpr int kNumBits = sizeof(T) * 8;
  using SignedType = typename std::make_signed<T>::type;
  // Assumes 0 <= |lo| <= |hi| < |kNumBits|.
  // How this works:
  // (1) Shift-left by |kNumBits - 1 - hi| to clear "left" bits.
  // (2) Shift-right by |kNumBits - 1 - hi + lo| to clear "right" bits. The
  //     input is casted to a signed type to perform sign-extension.
  return static_cast<SignedType>(v << (kNumBits - 1 - hi)) >>
         (kNumBits - 1 - hi + lo);
}

// Similar to GetSignedBits(), but returns the zero-extended result. For the
// above example, calling
//   GetUnsignedBits<7, 18>(v);
// results in:
//   00000000 00000000 0000Siii iiiiiiii.
template <int lo, int hi, typename T>
constexpr typename std::make_unsigned<T>::type GetUnsignedBits(T v) {
  constexpr int kNumBits = sizeof(T) * 8;
  using UnsignedType = typename std::make_unsigned<T>::type;
  return static_cast<UnsignedType>(v << (kNumBits - 1 - hi)) >>
         (kNumBits - 1 - hi + lo);
}

// Copies bits at |pos| in |v| to all higher bits, and returns the result as the
// same int type as |v|.
template <typename T>
constexpr T SignExtend(int pos, T v) {
  int kNumBits = sizeof(T) * 8;
  int kShift = kNumBits - 1 - pos;
  return static_cast<typename std::make_signed<T>::type>(v << kShift) >> kShift;
}

// Optimized version where |pos| becomes a template parameter.
template <int pos, typename T>
constexpr T SignExtend(T v) {
  constexpr int kNumBits = sizeof(T) * 8;
  constexpr int kShift = kNumBits - 1 - pos;
  return static_cast<typename std::make_signed<T>::type>(v << kShift) >> kShift;
}

// Determines whether |v|, if interpreted as a signed integer, is representable
// using |digs| bits. |1 <= digs <= sizeof(T)| is assumed.
template <int digs, typename T>
constexpr bool SignedFit(T v) {
  return v == SignExtend<digs - 1, T>(v);
}

}  // namespace zucchini

#endif  // COMPONENTS_ZUCCHINI_ALGORITHM_H_
