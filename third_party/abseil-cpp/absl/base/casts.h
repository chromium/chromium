//
// Copyright 2017 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// -----------------------------------------------------------------------------
// File: casts.h
// -----------------------------------------------------------------------------
//
// This header file defines casting templates to fit use cases not covered by
// the standard casts provided in the C++ standard. As with all cast operations,
// use these with caution and only if alternatives do not exist.

#ifndef ABSL_BASE_CASTS_H_
#define ABSL_BASE_CASTS_H_

#include <cstring>
#include <memory>
#include <type_traits>
#include <utility>

#include "absl/base/internal/identity.h"
#include "absl/base/macros.h"
#include "absl/meta/type_traits.h"

namespace absl {
ABSL_NAMESPACE_BEGIN

namespace internal_casts {

template <class Dest, class Source>
struct is_bitcastable
    : std::integral_constant<
          bool,
          sizeof(Dest) == sizeof(Source) &&
              type_traits_internal::is_trivially_copyable<Source>::value &&
              type_traits_internal::is_trivially_copyable<Dest>::value
#if !ABSL_HAVE_BUILTIN(__builtin_bit_cast)
              && std::is_default_constructible<Dest>::value
#endif
          > {
};

}  // namespace internal_casts

// implicit_cast()
//
// Performs an implicit conversion between types following the language
// rules for implicit conversion; if an implicit conversion is otherwise
// allowed by the language in the given context, this function performs such an
// implicit conversion.
//
// Example:
//
//   // If the context allows implicit conversion:
//   From from;
//   To to = from;
//
//   // Such code can be replaced by:
//   implicit_cast<To>(from);
//
// An `implicit_cast()` may also be used to annotate numeric type conversions
// that, although safe, may produce compiler warnings (such as `long` to `int`).
// Additionally, an `implicit_cast()` is also useful within return statements to
// indicate a specific implicit conversion is being undertaken.
//
// Example:
//
//   return implicit_cast<double>(size_in_bytes) / capacity_;
//
// Annotating code with `implicit_cast()` allows you to explicitly select
// particular overloads and template instantiations, while providing a safer
// cast than `reinterpret_cast()` or `static_cast()`.
//
// Additionally, an `implicit_cast()` can be used to allow upcasting within a
// type hierarchy where incorrect use of `static_cast()` could accidentally
// allow downcasting.
//
// Finally, an `implicit_cast()` can be used to perform implicit conversions
// from unrelated types that otherwise couldn't be implicitly cast directly;
// C++ will normally only implicitly cast "one step" in such conversions.
//
// That is, if C is a type which can be implicitly converted to B, with B being
// a type that can be implicitly converted to A, an `implicit_cast()` can be
// used to convert C to B (which the compiler can then implicitly convert to A
// using language rules).
//
// Example:
//
//   // Assume an object C is convertible to B, which is implicitly convertible
//   // to A
//   A a = implicit_cast<B>(C);
//
// Such implicit cast chaining may be useful within template logic.
template <typename To>
constexpr To implicit_cast(typename absl::internal::identity_t<To> to) {
  return to;
}

// bit_cast()
//
// Creates a value of the new type `Dest` whose representation is the same as
// that of the argument, which is of (deduced) type `Source` (a "bitwise cast";
// every bit in the value representation of the result is equal to the
// corresponding bit in the object representation of the source). Source and
// destination types must be of the same size, and both types must be trivially
// copyable.
//
// As with most casts, use with caution. A `bit_cast()` might be needed when you
// need to treat a value as the value of some other type, for example, to access
// the individual bits of an object which are not normally accessible through
// the object's type, such as for working with the binary representation of a
// floating point value:
//
//   float f = 3.14159265358979;
//   int i = bit_cast<int32_t>(f);
//   // i = 0x40490fdb
//
// Reinterpreting and accessing a value directly as a different type (as shown
// below) usually results in undefined behavior.
//
// Example:
//
//   // WRONG
//   float f = 3.14159265358979;
//   int i = reinterpret_cast<int&>(f);    // Wrong
//   int j = *reinterpret_cast<int*>(&f);  // Equally wrong
//   int k = *bit_cast<int*>(&f);          // Equally wrong
//
// Reinterpret-casting results in undefined behavior according to the ISO C++
// specification, section [basic.lval]. Roughly, this section says: if an object
// in memory has one type, and a program accesses it with a different type, the
// result is undefined behavior for most "different type".
//
// Using bit_cast on a pointer and then dereferencing it is no better than using
// reinterpret_cast. You should only use bit_cast on the value itself.
//
// Such casting results in type punning: holding an object in memory of one type
// and reading its bits back using a different type. A `bit_cast()` avoids this
// issue by copying the object representation to a new value, which avoids
// introducing this undefined behavior (since the original value is never
// accessed in the wrong way).
//
// NOTE: The requirements here are more strict than the bit_cast of standard
// proposal P0476 when __builtin_bit_cast is not available.
// Specifically, this implementation also requires `Dest` to be
// default-constructible.
template <
    typename Dest, typename Source,
    typename std::enable_if<internal_casts::is_bitcastable<Dest, Source>::value,
                            int>::type = 0>
#if ABSL_HAVE_BUILTIN(__builtin_bit_cast)
inline constexpr Dest bit_cast(const Source& source) {
  return __builtin_bit_cast(Dest, source);
}
#else
inline Dest bit_cast(const Source& source) {
  Dest dest;
  memcpy(static_cast<void*>(std::addressof(dest)),
         static_cast<const void*>(std::addressof(source)), sizeof(dest));
  return dest;
}
#endif

// NOTE: This overload is only picked if the requirements of bit_cast are
// not met. It is therefore UB, but is provided temporarily as previous
// versions of this function template were unchecked. Do not use this in
// new code.
template <
    typename Dest, typename Source,
    typename std::enable_if<
        !internal_casts::is_bitcastable<Dest, Source>::value,
        int>::type = 0>
ABSL_DEPRECATED(
    "absl::bit_cast type requirements were violated. Update the types "
    "being used such that they are the same size and are both "
    "TriviallyCopyable.")
inline Dest bit_cast(const Source& source) {
  static_assert(sizeof(Dest) == sizeof(Source),
                "Source and destination types should have equal sizes.");

  Dest dest;
  memcpy(&dest, &source, sizeof(dest));
  return dest;
}

ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_BASE_CASTS_H_
