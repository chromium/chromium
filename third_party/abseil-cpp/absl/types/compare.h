// Copyright 2018 The Abseil Authors.
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
// compare.h
// -----------------------------------------------------------------------------
//
// This header file defines the `absl::weak_equality`, `absl::strong_equality`,
// `absl::partial_ordering`, `absl::weak_ordering`, and `absl::strong_ordering`
// types for storing the results of three way comparisons.
//
// Example:
//   absl::weak_ordering compare(const std::string& a, const std::string& b);
//
// These are C++11 compatible versions of the C++20 corresponding types
// (`std::weak_equality`, etc.) and are designed to be drop-in replacements
// for code compliant with C++20.

#ifndef ABSL_TYPES_COMPARE_H_
#define ABSL_TYPES_COMPARE_H_

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <type_traits>

#include "absl/base/attributes.h"
#include "absl/meta/type_traits.h"

namespace absl {
namespace compare_internal {

using value_type = int8_t;

template <typename T>
struct Fail {
  static_assert(sizeof(T) < 0, "Only literal `0` is allowed.");
};

// We need the NullPtrT template to avoid triggering the modernize-use-nullptr
// ClangTidy warning in user code.
template <typename NullPtrT = std::nullptr_t>
struct OnlyLiteralZero {
  constexpr OnlyLiteralZero(NullPtrT) noexcept {}  // NOLINT

  // Fails compilation when `nullptr` or integral type arguments other than
  // `int` are passed. This constructor doesn't accept `int` because literal `0`
  // has type `int`. Literal `0` arguments will be implicitly converted to
  // `std::nullptr_t` and accepted by the above constructor, while other `int`
  // arguments will fail to be converted and cause compilation failure.
  template <
      typename T,
      typename = typename std::enable_if<
          std::is_same<T, std::nullptr_t>::value ||
          (std::is_integral<T>::value && !std::is_same<T, int>::value)>::type,
      typename = typename Fail<T>::type>
  OnlyLiteralZero(T);  // NOLINT
};

enum class eq : value_type {
  equal = 0,
  equivalent = equal,
  nonequal = 1,
  nonequivalent = nonequal,
};

enum class ord : value_type { less = -1, greater = 1 };

enum class ncmp : value_type { unordered = -127 };

// These template base classes allow for defining the values of the constants
// in the header file (for performance) without using inline variables (which
// aren't available in C++11).
template <typename T>
struct weak_equality_base {
  ABSL_CONST_INIT static const T equivalent;
  ABSL_CONST_INIT static const T nonequivalent;
};
template <typename T>
const T weak_equality_base<T>::equivalent(eq::equivalent);
template <typename T>
const T weak_equality_base<T>::nonequivalent(eq::nonequivalent);

template <typename T>
struct strong_equality_base {
  ABSL_CONST_INIT static const T equal;
  ABSL_CONST_INIT static const T nonequal;
  ABSL_CONST_INIT static const T equivalent;
  ABSL_CONST_INIT static const T nonequivalent;
};
template <typename T>
const T strong_equality_base<T>::equal(eq::equal);
template <typename T>
const T strong_equality_base<T>::nonequal(eq::nonequal);
template <typename T>
const T strong_equality_base<T>::equivalent(eq::equivalent);
template <typename T>
const T strong_equality_base<T>::nonequivalent(eq::nonequivalent);

template <typename T>
struct partial_ordering_base {
  ABSL_CONST_INIT static const T less;
  ABSL_CONST_INIT static const T equivalent;
  ABSL_CONST_INIT static const T greater;
  ABSL_CONST_INIT static const T unordered;
};
template <typename T>
const T partial_ordering_base<T>::less(ord::less);
template <typename T>
const T partial_ordering_base<T>::equivalent(eq::equivalent);
template <typename T>
const T partial_ordering_base<T>::greater(ord::greater);
template <typename T>
const T partial_ordering_base<T>::unordered(ncmp::unordered);

template <typename T>
struct weak_ordering_base {
  ABSL_CONST_INIT static const T less;
  ABSL_CONST_INIT static const T equivalent;
  ABSL_CONST_INIT static const T greater;
};
template <typename T>
const T weak_ordering_base<T>::less(ord::less);
template <typename T>
const T weak_ordering_base<T>::equivalent(eq::equivalent);
template <typename T>
const T weak_ordering_base<T>::greater(ord::greater);

template <typename T>
struct strong_ordering_base {
  ABSL_CONST_INIT static const T less;
  ABSL_CONST_INIT static const T equal;
  ABSL_CONST_INIT static const T equivalent;
  ABSL_CONST_INIT static const T greater;
};
template <typename T>
const T strong_ordering_base<T>::less(ord::less);
template <typename T>
const T strong_ordering_base<T>::equal(eq::equal);
template <typename T>
const T strong_ordering_base<T>::equivalent(eq::equivalent);
template <typename T>
const T strong_ordering_base<T>::greater(ord::greater);

}  // namespace compare_internal

class weak_equality
    : public compare_internal::weak_equality_base<weak_equality> {
  explicit constexpr weak_equality(compare_internal::eq v) noexcept
      : value_(static_cast<compare_internal::value_type>(v)) {}
  friend struct compare_internal::weak_equality_base<weak_equality>;

 public:
  // Comparisons
  friend constexpr bool operator==(
      weak_equality v, compare_internal::OnlyLiteralZero<>) noexcept {
    return v.value_ == 0;
  }
  friend constexpr bool operator!=(
      weak_equality v, compare_internal::OnlyLiteralZero<>) noexcept {
    return v.value_ != 0;
  }
  friend constexpr bool operator==(compare_internal::OnlyLiteralZero<>,
                                   weak_equality v) noexcept {
    return 0 == v.value_;
  }
  friend constexpr bool operator!=(compare_internal::OnlyLiteralZero<>,
                                   weak_equality v) noexcept {
    return 0 != v.value_;
  }

 private:
  compare_internal::value_type value_;
};

class strong_equality
    : public compare_internal::strong_equality_base<strong_equality> {
  explicit constexpr strong_equality(compare_internal::eq v) noexcept
      : value_(static_cast<compare_internal::value_type>(v)) {}
  friend struct compare_internal::strong_equality_base<strong_equality>;

 public:
  // Conversion
  constexpr operator weak_equality() const noexcept {  // NOLINT
    return value_ == 0 ? weak_equality::equivalent
                       : weak_equality::nonequivalent;
  }
  // Comparisons
  friend constexpr bool operator==(
      strong_equality v, compare_internal::OnlyLiteralZero<>) noexcept {
    return v.value_ == 0;
  }
  friend constexpr bool operator!=(
      strong_equality v, compare_internal::OnlyLiteralZero<>) noexcept {
    return v.value_ != 0;
  }
  friend constexpr bool operator==(compare_internal::OnlyLiteralZero<>,
                                   strong_equality v) noexcept {
    return 0 == v.value_;
  }
  friend constexpr bool operator!=(compare_internal::OnlyLiteralZero<>,
                                   strong_equality v) noexcept {
    return 0 != v.value_;
  }

 private:
  compare_internal::value_type value_;
};

class partial_ordering
    : public compare_internal::partial_ordering_base<partial_ordering> {
  explicit constexpr partial_ordering(compare_internal::eq v) noexcept
      : value_(static_cast<compare_internal::value_type>(v)) {}
  explicit constexpr partial_ordering(compare_internal::ord v) noexcept
      : value_(static_cast<compare_internal::value_type>(v)) {}
  explicit constexpr partial_ordering(compare_internal::ncmp v) noexcept
      : value_(static_cast<compare_internal::value_type>(v)) {}
  friend struct compare_internal::partial_ordering_base<partial_ordering>;

  constexpr bool is_ordered() const noexcept {
    return value_ !=
           compare_internal::value_type(compare_internal::ncmp::unordered);
  }

 public:
  // Conversion
  constexpr operator weak_equality() const noexcept {  // NOLINT
    return value_ == 0 ? weak_equality::equivalent
                       : weak_equality::nonequivalent;
  }
  // Comparisons
  friend constexpr bool operator==(
      partial_ordering v, compare_internal::OnlyLiteralZero<>) noexcept {
    return v.is_ordered() && v.value_ == 0;
  }
  friend constexpr bool operator!=(
      partial_ordering v, compare_internal::OnlyLiteralZero<>) noexcept {
    return !v.is_ordered() || v.value_ != 0;
  }
  friend constexpr bool operator<(
      partial_ordering v, compare_internal::OnlyLiteralZero<>) noexcept {
    return v.is_ordered() && v.value_ < 0;
  }
  friend constexpr bool operator<=(
      partial_ordering v, compare_internal::OnlyLiteralZero<>) noexcept {
    return v.is_ordered() && v.value_ <= 0;
  }
  friend constexpr bool operator>(
      partial_ordering v, compare_internal::OnlyLiteralZero<>) noexcept {
    return v.is_ordered() && v.value_ > 0;
  }
  friend constexpr bool operator>=(
      partial_ordering v, compare_internal::OnlyLiteralZero<>) noexcept {
    return v.is_ordered() && v.value_ >= 0;
  }
  friend constexpr bool operator==(compare_internal::OnlyLiteralZero<>,
                                   partial_ordering v) noexcept {
    return v.is_ordered() && 0 == v.value_;
  }
  friend constexpr bool operator!=(compare_internal::OnlyLiteralZero<>,
                                   partial_ordering v) noexcept {
    return !v.is_ordered() || 0 != v.value_;
  }
  friend constexpr bool operator<(compare_internal::OnlyLiteralZero<>,
                                  partial_ordering v) noexcept {
    return v.is_ordered() && 0 < v.value_;
  }
  friend constexpr bool operator<=(compare_internal::OnlyLiteralZero<>,
                                   partial_ordering v) noexcept {
    return v.is_ordered() && 0 <= v.value_;
  }
  friend constexpr bool operator>(compare_internal::OnlyLiteralZero<>,
                                  partial_ordering v) noexcept {
    return v.is_ordered() && 0 > v.value_;
  }
  friend constexpr bool operator>=(compare_internal::OnlyLiteralZero<>,
                                   partial_ordering v) noexcept {
    return v.is_ordered() && 0 >= v.value_;
  }

 private:
  compare_internal::value_type value_;
};

class weak_ordering
    : public compare_internal::weak_ordering_base<weak_ordering> {
  explicit constexpr weak_ordering(compare_internal::eq v) noexcept
      : value_(static_cast<compare_internal::value_type>(v)) {}
  explicit constexpr weak_ordering(compare_internal::ord v) noexcept
      : value_(static_cast<compare_internal::value_type>(v)) {}
  friend struct compare_internal::weak_ordering_base<weak_ordering>;

 public:
  // Conversions
  constexpr operator weak_equality() const noexcept {  // NOLINT
    return value_ == 0 ? weak_equality::equivalent
                       : weak_equality::nonequivalent;
  }
  constexpr operator partial_ordering() const noexcept {  // NOLINT
    return value_ == 0 ? partial_ordering::equivalent
                       : (value_ < 0 ? partial_ordering::less
                                     : partial_ordering::greater);
  }
  // Comparisons
  friend constexpr bool operator==(
      weak_ordering v, compare_internal::OnlyLiteralZero<>) noexcept {
    return v.value_ == 0;
  }
  friend constexpr bool operator!=(
      weak_ordering v, compare_internal::OnlyLiteralZero<>) noexcept {
    return v.value_ != 0;
  }
  friend constexpr bool operator<(
      weak_ordering v, compare_internal::OnlyLiteralZero<>) noexcept {
    return v.value_ < 0;
  }
  friend constexpr bool operator<=(
      weak_ordering v, compare_internal::OnlyLiteralZero<>) noexcept {
    return v.value_ <= 0;
  }
  friend constexpr bool operator>(
      weak_ordering v, compare_internal::OnlyLiteralZero<>) noexcept {
    return v.value_ > 0;
  }
  friend constexpr bool operator>=(
      weak_ordering v, compare_internal::OnlyLiteralZero<>) noexcept {
    return v.value_ >= 0;
  }
  friend constexpr bool operator==(compare_internal::OnlyLiteralZero<>,
                                   weak_ordering v) noexcept {
    return 0 == v.value_;
  }
  friend constexpr bool operator!=(compare_internal::OnlyLiteralZero<>,
                                   weak_ordering v) noexcept {
    return 0 != v.value_;
  }
  friend constexpr bool operator<(compare_internal::OnlyLiteralZero<>,
                                  weak_ordering v) noexcept {
    return 0 < v.value_;
  }
  friend constexpr bool operator<=(compare_internal::OnlyLiteralZero<>,
                                   weak_ordering v) noexcept {
    return 0 <= v.value_;
  }
  friend constexpr bool operator>(compare_internal::OnlyLiteralZero<>,
                                  weak_ordering v) noexcept {
    return 0 > v.value_;
  }
  friend constexpr bool operator>=(compare_internal::OnlyLiteralZero<>,
                                   weak_ordering v) noexcept {
    return 0 >= v.value_;
  }

 private:
  compare_internal::value_type value_;
};

class strong_ordering
    : public compare_internal::strong_ordering_base<strong_ordering> {
  explicit constexpr strong_ordering(compare_internal::eq v) noexcept
      : value_(static_cast<compare_internal::value_type>(v)) {}
  explicit constexpr strong_ordering(compare_internal::ord v) noexcept
      : value_(static_cast<compare_internal::value_type>(v)) {}
  friend struct compare_internal::strong_ordering_base<strong_ordering>;

 public:
  // Conversions
  constexpr operator weak_equality() const noexcept {  // NOLINT
    return value_ == 0 ? weak_equality::equivalent
                       : weak_equality::nonequivalent;
  }
  constexpr operator strong_equality() const noexcept {  // NOLINT
    return value_ == 0 ? strong_equality::equal : strong_equality::nonequal;
  }
  constexpr operator partial_ordering() const noexcept {  // NOLINT
    return value_ == 0 ? partial_ordering::equivalent
                       : (value_ < 0 ? partial_ordering::less
                                     : partial_ordering::greater);
  }
  constexpr operator weak_ordering() const noexcept {  // NOLINT
    return value_ == 0
               ? weak_ordering::equivalent
               : (value_ < 0 ? weak_ordering::less : weak_ordering::greater);
  }
  // Comparisons
  friend constexpr bool operator==(
      strong_ordering v, compare_internal::OnlyLiteralZero<>) noexcept {
    return v.value_ == 0;
  }
  friend constexpr bool operator!=(
      strong_ordering v, compare_internal::OnlyLiteralZero<>) noexcept {
    return v.value_ != 0;
  }
  friend constexpr bool operator<(
      strong_ordering v, compare_internal::OnlyLiteralZero<>) noexcept {
    return v.value_ < 0;
  }
  friend constexpr bool operator<=(
      strong_ordering v, compare_internal::OnlyLiteralZero<>) noexcept {
    return v.value_ <= 0;
  }
  friend constexpr bool operator>(
      strong_ordering v, compare_internal::OnlyLiteralZero<>) noexcept {
    return v.value_ > 0;
  }
  friend constexpr bool operator>=(
      strong_ordering v, compare_internal::OnlyLiteralZero<>) noexcept {
    return v.value_ >= 0;
  }
  friend constexpr bool operator==(compare_internal::OnlyLiteralZero<>,
                                   strong_ordering v) noexcept {
    return 0 == v.value_;
  }
  friend constexpr bool operator!=(compare_internal::OnlyLiteralZero<>,
                                   strong_ordering v) noexcept {
    return 0 != v.value_;
  }
  friend constexpr bool operator<(compare_internal::OnlyLiteralZero<>,
                                  strong_ordering v) noexcept {
    return 0 < v.value_;
  }
  friend constexpr bool operator<=(compare_internal::OnlyLiteralZero<>,
                                   strong_ordering v) noexcept {
    return 0 <= v.value_;
  }
  friend constexpr bool operator>(compare_internal::OnlyLiteralZero<>,
                                  strong_ordering v) noexcept {
    return 0 > v.value_;
  }
  friend constexpr bool operator>=(compare_internal::OnlyLiteralZero<>,
                                   strong_ordering v) noexcept {
    return 0 >= v.value_;
  }

 private:
  compare_internal::value_type value_;
};

namespace compare_internal {
// We also provide these comparator adapter functions for internal absl use.

// Helper functions to do a boolean comparison of two keys given a boolean
// or three-way comparator.
// SFINAE prevents implicit conversions to bool (such as from int).
template <typename Bool,
          absl::enable_if_t<std::is_same<bool, Bool>::value, int> = 0>
constexpr bool compare_result_as_less_than(const Bool r) { return r; }
constexpr bool compare_result_as_less_than(const absl::weak_ordering r) {
  return r < 0;
}

template <typename Compare, typename K, typename LK>
constexpr bool do_less_than_comparison(const Compare &compare, const K &x,
                                       const LK &y) {
  return compare_result_as_less_than(compare(x, y));
}

// Helper functions to do a three-way comparison of two keys given a boolean or
// three-way comparator.
// SFINAE prevents implicit conversions to int (such as from bool).
template <typename Int,
          absl::enable_if_t<std::is_same<int, Int>::value, int> = 0>
constexpr absl::weak_ordering compare_result_as_ordering(const Int c) {
  return c < 0 ? absl::weak_ordering::less
               : c == 0 ? absl::weak_ordering::equivalent
                        : absl::weak_ordering::greater;
}
constexpr absl::weak_ordering compare_result_as_ordering(
    const absl::weak_ordering c) {
  return c;
}

template <
    typename Compare, typename K, typename LK,
    absl::enable_if_t<!std::is_same<bool, absl::result_of_t<Compare(
                                              const K &, const LK &)>>::value,
                      int> = 0>
constexpr absl::weak_ordering do_three_way_comparison(const Compare &compare,
                                                      const K &x, const LK &y) {
  return compare_result_as_ordering(compare(x, y));
}
template <
    typename Compare, typename K, typename LK,
    absl::enable_if_t<std::is_same<bool, absl::result_of_t<Compare(
                                             const K &, const LK &)>>::value,
                      int> = 0>
constexpr absl::weak_ordering do_three_way_comparison(const Compare &compare,
                                                      const K &x, const LK &y) {
  return compare(x, y) ? absl::weak_ordering::less
                       : compare(y, x) ? absl::weak_ordering::greater
                                       : absl::weak_ordering::equivalent;
}

}  // namespace compare_internal
}  // namespace absl

#endif  // ABSL_TYPES_COMPARE_H_
