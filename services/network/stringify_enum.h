// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Utility functions for stringifying enum values. NOT PORTABLE C++. Do NOT use
// for implementing production features! Only for logging. Tested in GCC and
// Clang.

#ifndef SERVICES_NETWORK_STRINGIFY_ENUM_H_
#define SERVICES_NETWORK_STRINGIFY_ENUM_H_

#include <stddef.h>

#include <algorithm>
#include <array>
#include <limits>
#include <ostream>
#include <string_view>
#include <type_traits>
#include <utility>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/strings/string_util.h"
#include "base/types/cxx23_to_underlying.h"

namespace network {

namespace internal {

// MaxValue() finds the max value of an enum, compensating for the variety of
// conventions used in Chromium. Returns the maximum size_t value if no max
// could be found. May also return a ridiculously large value if the max is
// actually negative, but by limiting the maximum value we accept we can protect
// ourselves and avoid generating an enormous table.
template <typename Enum>
consteval size_t MaxValue() {
  if constexpr (requires { Enum::kMaxValue; }) {
    // The convention used by mojo.
    return static_cast<size_t>(Enum::kMaxValue);
  } else if constexpr (requires { Enum::MAX; }) {
    // Used in //net
    return static_cast<size_t>(Enum::MAX);
  } else if constexpr (requires { Enum::kLast; }) {
    // Used in a few places.
    return static_cast<size_t>(Enum::kLast);
  } else if constexpr (requires { Enum::kMax; }) {
    // Somewhat common.
    return static_cast<size_t>(Enum::kMax);
  } else if constexpr (requires { Enum::kCount; }) {
    // Popular because the compiler will assign it for you.
    return static_cast<size_t>(Enum::kCount) - 1;
  } else if constexpr (requires { Enum::COUNT; }) {
    // Historically popular.
    return static_cast<size_t>(Enum::COUNT) - 1;
  } else {
    // The maximum value for this enum could not be determined. Give up.
    return std::numeric_limits<size_t>::max();
  }
}

// Returns the name of the compile-time-constant enum value
// `uniquely_named_value` as a std::string_view. If `Enum` is a class enum type,
// then the returned name will include the enum type, ie. "EnumType::kValue",
// otherwise it will just include the value, matching normal usage. Works by
// parsing the name of the function as supplied by the compiler. This is the
// non-portable part. Returns a string_view which references into the function
// name. The referenced string can be extremely long, so should ideally not be
// referenced in the binary.
template <typename Enum, Enum uniquely_named_value>
consteval std::string_view GetEnumName() {
  constexpr std::string_view kMatchString = "uniquely_named_value = ";
  constexpr std::string_view kNamespaceSeparator = "::";
  // `PRETTY_FUNCTION` on GCC returns a string like
  // "constexpr std::string_view GetEnumName() [with Enum =
  // network::{anonymous}::EnumName; Enum uniquely_named_value =
  // network::<unnamed>::EnumName::kValue; std::string_view =
  // std::basic_string_view<char>]"
  //
  // Clang returns a string like
  // "std::string_view GetEnumName() [Enum = network::(anonymous
  // namespace)::EnumName, uniquely_named_value = network::(anonymous
  // namespace)::EnumName::kValue]"
  //
  // Both GCC and Clang don't include "EnumName::" before "kValue" when it is
  // not a class enum.
  //
  // TODO(crbug.com/373461931): MSVC returns a string like
  // "class std::basic_string_view<char,struct std::char_traits<char> > __cdecl
  // GetEnumName<enum
  // network::`anonymous-namespace'::EnumName,network::`anonymous-namespace'::EnumName::kValue>(void)"
  // Add support to parse this format if needed.
  constexpr std::string_view kPrettyFunction = PRETTY_FUNCTION;
  const size_t match_pos = kPrettyFunction.find(kMatchString);
  if (match_pos == std::string_view::npos) {
    // Compiler is not clang or GCC, or they have changed their
    // PRETTY_FUNCTION format.
    return {};
  }
  const auto is_cpp_identifier_char = [](char c) {
    // "()<> " are included for anonymous namespace support.
    return base::IsAsciiAlphaNumeric(c) || c == '_' || c == ':' || c == '(' ||
           c == ')' || c == '<' || c == '>' || c == ' ';
  };
  const std::string_view remaining_chars =
      kPrettyFunction.substr(match_pos + kMatchString.size());

  const auto end_pos =
      std::ranges::find_if_not(remaining_chars, is_cpp_identifier_char);

  const std::string_view including_namespaces =
      remaining_chars.substr(0, end_pos - remaining_chars.begin());
  const size_t final_namespace_separator_pos =
      including_namespaces.rfind(kNamespaceSeparator);
  if (final_namespace_separator_pos == std::string_view::npos) {
    // No namespace after all.
    return including_namespaces;
  }

  std::string_view value_name = including_namespaces.substr(
      final_namespace_separator_pos + kNamespaceSeparator.size());

  // If the compiler didn't have a name for this value, then we'd prefer to
  // return nothing. In this case, GCC outputs something like
  // "(network::<unnamed>::EnumName)1" and Clang outputs something like
  // "(network::(anonymous namespace)::EnumName)1". We can detect both formats
  // by seeing if the string ends in ")\d+".
  if (const size_t final_closing_paren = value_name.rfind(')');
      final_closing_paren != std::string_view::npos) {
    const std::string_view after_final_paren =
        value_name.substr(final_closing_paren + 1);
    if (std::ranges::all_of(after_final_paren, base::IsAsciiDigit<char>)) {
      return {};
    }
  }

  if constexpr (!std::is_convertible_v<Enum, std::underlying_type_t<Enum>>) {
    // This is a class enum. Include the name of the enum in the output.
    if (final_namespace_separator_pos > 0) {
      const size_t previous_namespace_separator_pos =
          including_namespaces.rfind(kNamespaceSeparator,
                                     final_namespace_separator_pos - 1);
      if (previous_namespace_separator_pos != std::string_view::npos) {
        value_name = including_namespaces.substr(
            previous_namespace_separator_pos + kNamespaceSeparator.size());
      }
    }
  }
  return value_name;
}

// Copies a string view into an array so that the original backing string does
// not have to be retained in the binary. Does not include a trailing '\0'.
template <size_t size>
class ExtractStringView {
 public:
  // `view.size()` must match `size`.
  consteval explicit ExtractStringView(std::string_view view) {
    CHECK(view.size() == size);
    std::ranges::copy(view, storage_.begin());
  }

  // Returns the stored string as a string view.
  constexpr std::string_view AsStringView() const {
    return std::string_view(storage_.data(), storage_.size());
  }

 private:
  std::array<char, size> storage_;
};

// Storage for an array of value names for `Enum`, with `size` entries.
template <typename Enum, size_t size = MaxValue<Enum>() + 1>
class EnumTable final {
 public:
  constexpr EnumTable() = default;

  // Copy and assignment shouldn't be needed.
  EnumTable(const EnumTable&) = delete;
  EnumTable& operator=(const EnumTable&) = delete;

  constexpr std::string_view GetNameForValue(Enum value) const {
    const size_t n = static_cast<size_t>(value);
    return n < size ? storage_[n] : std::string_view();
  }

 private:
  static constexpr auto kIndexSequence = std::make_index_sequence<size>();

  template <size_t index>
  static constexpr std::string_view ValueName() {
    return GetEnumName<Enum, static_cast<Enum>(index)>();
  }

  // Constructs a tuple of enum value names from `Enum` consisting of one
  // ExtractStringView object per `index`.
  template <size_t... index>
  static constexpr auto MakeExtractStringViewTuple(
      std::index_sequence<index...>) {
    return std::make_tuple(
        ExtractStringView<ValueName<index>().size()>(ValueName<index>())...);
  }

  // The return type of MakeExtractStringViewTuple().
  using TupleType = decltype(MakeExtractStringViewTuple(kIndexSequence));

  // Given a tuple created by MakeExtractStringViewTuple(), returns an array of
  // one string_view per tuple entry. The value at index 0 of the array is the
  // name of the enum entry with value 0, etc.
  template <size_t... index>
  constexpr std::array<std::string_view, size> MakeStringViewArrayFromTuple(
      const TupleType& tuple,
      std::index_sequence<index...>) {
    return {std::get<index>(tuple).AsStringView()...};
  }

  TupleType storage_tuple_ = MakeExtractStringViewTuple(kIndexSequence);
  std::array<std::string_view, size> storage_ =
      MakeStringViewArrayFromTuple(storage_tuple_, kIndexSequence);
};

// Precalculated storage for arrays of value names. Accessing
// kEnumTable<SomeEnum> will cause the value names to be calculated for SomeEnum
// at compile time and stored in the binary.
template <typename Enum>
inline constexpr EnumTable<Enum> kEnumTable;

namespace enum_stream_concepts {

// A non-class enum is implicitly convertible to an integer type, and so will
// use an integer stream operator by default. It is difficult to distinguish
// between a use of the integer stream operator and a user-defined stream
// operator, so we override all the integer stream operators inside this
// namespace.

// A dummy type to indicate that we shouldn't use a streaming operator.
struct UnwantedConversion {};

// Fake definitions for all the integer operators defined by the C++ standard,
// allowing us to detect if one of these would be used. These need to be inside
// their own namespace so they are only seen by the concepts here. These should
// match exactly the types specified by the standard. This includes types not
// normally permitted in Chromium code, so they need "NOLINT" annotations.
UnwantedConversion operator<<(std::ostream&, char);
UnwantedConversion operator<<(std::ostream&, signed char);           // NOLINT
UnwantedConversion operator<<(std::ostream&, unsigned char);         // NOLINT
UnwantedConversion operator<<(std::ostream&, short value);           // NOLINT
UnwantedConversion operator<<(std::ostream&, unsigned short value);  // NOLINT
UnwantedConversion operator<<(std::ostream&, int value);
UnwantedConversion operator<<(std::ostream&, unsigned int value);   // NOLINT
UnwantedConversion operator<<(std::ostream&, long value);           // NOLINT
UnwantedConversion operator<<(std::ostream&, unsigned long value);  // NOLINT
UnwantedConversion operator<<(std::ostream&, long long value);      // NOLINT
// NOLINTNEXTLINE
UnwantedConversion operator<<(std::ostream&, unsigned long long value);

template <typename T>
concept OutputStreamable =
    requires(std::ostream& os, T&& t) { os << std::forward<T>(t); };

template <typename T>
concept StreamOperatorIsUnwanted = requires(std::ostream& os, T t) {
  { os << t } -> std::same_as<UnwantedConversion>;
};

}  // namespace enum_stream_concepts

template <typename T>
concept EnumHasUserDefinedStreamOperator =
    std::is_enum_v<T> && enum_stream_concepts::OutputStreamable<T> &&
    !enum_stream_concepts::StreamOperatorIsUnwanted<T>;

// To avoid accidentally creating large tables for sparse enums, avoid
// stringifying any enum where the maximum value is too large.
inline constexpr size_t kMaxMaxValue = 50;

}  // namespace internal

// Returns the name of the enum entry `value` as it appears in the source code.
// Returns an empty string if the name could not be determined. A side-effect of
// calling this function is that a table of the names of all the enum entries
// for this enum will be generated and stored in the binary.
//
// Relies on non-standard C++ features, but should fail gracefully (ie. return
// an empty string) on other compilers.
template <typename Enum>
constexpr std::string_view GetEnumValueName(Enum value)
  requires(std::is_enum_v<Enum> &&
           internal::MaxValue<Enum>() <= internal::kMaxMaxValue)
{
  return internal::kEnumTable<Enum>.GetNameForValue(value);
}

// As a convenience for generic code, this version just returns an empty
// string_view if the requirements of GetEnumValueName() are not met.
template <typename Enum>
constexpr std::string_view GetEnumValueNameForGenericCode(Enum value) {
  if constexpr (requires { GetEnumValueName(value); }) {
    return GetEnumValueName(value);
  } else {
    return {};
  }
}

// Streams the stringification of `value` to `os`. It has a couple of extra
// features:
// 1. It will use an existing operator<< for the enum if one exists, avoiding
//    generating the table and wasting binary space.
// 2. It will output "Unknown (n)" where the name of `value` could not be
//    determined, where "n" is `value` as a number.
template <typename Enum>
  requires(std::is_enum_v<std::remove_cvref_t<Enum>>)
void StreamEnumValueTo(std::ostream& os, Enum&& value) {
  // Require "operator<<" to be specialized for "Enum" so we don't accidentally
  // trigger the implicit conversion to int for non-class enums.
  if constexpr (internal::EnumHasUserDefinedStreamOperator<
                    std::remove_cvref_t<Enum>>) {
    os << std::forward<Enum>(value);
  } else {
    const std::string_view name = GetEnumValueNameForGenericCode(value);
    if (name.empty()) {
      os << "Unknown (" << base::to_underlying(value) << ")";
    } else {
      os << name;
    }
  }
}

}  // namespace network

#endif  // SERVICES_NETWORK_STRINGIFY_ENUM_H_
