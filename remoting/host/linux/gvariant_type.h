// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_GVARIANT_TYPE_H_
#define REMOTING_HOST_LINUX_GVARIANT_TYPE_H_

#include <glib.h>

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <numeric>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/strings/strcat.h"

namespace remoting::gvariant {

// Holds a type string for a GVariantRef. N is the length of the string (not
// counting the terminating null byte. When a specific N is provided or
// inferred, Type holds the type string inline and can be used at compile time.
// If N == base::dynamic_extent, the type string is instead held in a
// dynamically-allocated extent. Use with class template argument deduction to
// automatically infer N for compile-time constants. E.g.,
//
//     static constexpr Type kTypeString = "i";
//
// will infer N to be 1.
template <std::size_t N = base::dynamic_extent>
class Type;

// Common members of all Types.
class TypeBase {
 public:
  // Access and conversion.
  constexpr const char* c_string() const;
  constexpr std::string_view string_view() const;
  const GVariantType* gvariant_type() const;

  explicit constexpr operator const char*() const;
  explicit constexpr operator std::string_view() const;
  explicit operator const GVariantType*() const;
  explicit constexpr operator Type<>() const;

  // Iteration
  constexpr const char* begin() const;
  constexpr const char* end() const;
  constexpr std::size_t size() const;

  // Comparison
  constexpr bool operator==(const TypeBase& other) const;
  constexpr bool IsSubtypeOf(const TypeBase& supertype) const;
  // Whether there exists a type that is a subtype of both this and other.
  constexpr bool HasCommonTypeWith(const TypeBase& other) const;

  // Properties
  constexpr bool IsValid() const;
  constexpr bool IsDefinite() const;
  constexpr bool IsBasic() const;
  constexpr bool IsStringType() const;  // Is "s", "o", or "g".
  constexpr bool IsContainer() const;
  constexpr bool IsFixedSizeContainer() const;

  // Utility

  // Unpacks a fixed-size container into a vector of element types:
  // "v": {"*"}
  // "(iias)": {"i", "i", "as"}
  // "{sv}": {"s", "v"}
  // If this is not a fixed-size container, returns std::nullopt.
  constexpr std::optional<std::vector<Type<>>> Unpack() const;

  // Type-level version of the above.
  // Returns a tuple of fixed-size Types instead of an optional vector of
  // Type<>.
  template <Type C>
  static consteval /*std::tuple<Type<Ns>...>*/ auto Unpack()
    requires(C.IsFixedSizeContainer());

  // Finds the common supertype between this and other.
  // "s", "s" -> "s"
  // "s", "i" -> "?"
  // "av", "as" -> "a*"
  // "(uu)", "(ui)" -> "(u?)"
  // "(uu)", "(uuu)" -> "r"
  // "a{sv}", "a{ss}" -> "a{s*}"
  constexpr Type<> CommonSuperTypeWith(const TypeBase& other) const;

  // Finds the common supertype of a range of Types.
  template <typename R>
    requires std::ranges::range<R> &&
             std::convertible_to<std::ranges::range_value_t<R>, const TypeBase&>
  static constexpr Type<> CommonSuperTypeOf(const R& types);

  // Type-level version of the above.
  template <Type... Types>
  static consteval /*Type<N>*/ auto CommonSuperTypeOf();

  // Returns the element type of the container. For tuples, this will be the
  // narrowest super type common to all elements as determined by
  // CommonSuperType(). For boxed values ("v"), this will always return "*".
  // If this is not a container, returns std::nullopt.
  constexpr std::optional<Type<>> ContainedType() const;

  // Type-level version of the above.
  template <Type C>
    requires(C.IsContainer())
  static consteval /*Type<N>*/ auto ContainedType();

 private:
  // Returns the contained null-terminated string and string length (not
  // including the terminating null).
  constexpr virtual std::pair<const char*, std::size_t> contents() const = 0;

  // Helpers

  // string must be a view within a null-terminated string.
  static constexpr bool Validate(std::string_view string);
  static constexpr bool IsBasicType(char code);
  // Given the pointer to the start of a type in a type string, returns a
  // pointer to the position immediately following it. E.g., given "a{sv}i",
  // returns a pointer to "i".
  static constexpr const char* SkipType(const char* start);

  // Converts a dynamic Type<> to a fixed Type<N>. get_dynamic must provide a
  // constexpr operator() that returns the Type<>. It will be called exactly
  // once.
  template <typename Callable>
  static consteval /* Type<N> */ auto ToFixed(Callable get_dynamic);

  // Converts a dynamic std::vector<Type<>> to a fixed std::tuple<Type<N>...>.
  // get_dynamic_vector must provide a constexpr operator() that returns the
  // std::vector<Type<>>. It will be called exactly once.
  template <typename Callable>
  static consteval /* std::tuple<Type<N>...> */ auto ToFixedTuple(
      Callable get_dynamic_vector);
};

// Type instance that stores the type string inline. Can be used in constant
// and template contexts. In general, using class template argument deduction to
// deduce the proper N from the arguments is preferred to specifying N manually.
template <std::size_t N>
class Type final : public TypeBase {
 public:
  // Provide N as a member to make checking for fixed-sized Types easier.
  static constexpr std::size_t fixed_size = N;

  // Constructors

  // Construct from string literal.
  //
  // Implicit to allow GVariantRef<"s"> rather than
  // GVariantRef<gvariant::Type("s")>.
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr Type(const char (&string_lit)[N + 1])
      : Type(string_lit, std::integral_constant<std::size_t, N>()) {}

  // Construct from possibly non-null-terminated string of the proper length.
  constexpr Type(const char* string, std::integral_constant<std::size_t, N>) {
    std::copy(string, string + N, type_string_.begin());
    type_string_[N] = '\0';
  }

  // Composite type string concatenating the passed pieces. Each piece must
  // be explicitly convertible to a fixed-size Type.
  template <typename... Pieces>
    requires(sizeof...(Pieces) > 1)
  // Not actually callable with one argument, and marking it explicit would
  // disallow things like GVariantRef<{"a", kSomeType}>
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr Type(Pieces&&... pieces)
    requires(N == (... + decltype(gvariant::Type(
                             std::forward<Pieces>(pieces)))::fixed_size))
  {
    auto position = type_string_.begin();
    // "Loop" through the passed values by invoking a lambda for each argument.
    (
        [&]<std::size_t M>(const Type<M>& type) {
          std::copy(type.begin(), type.end(), position);
          position += type.size();
        }(gvariant::Type(std::forward<Pieces>(pieces))),
        ...);
    *position = '\0';
  }

  // Copyable

  Type(const Type& other) = default;
  Type& operator=(const Type& other) = default;

  // Logically private, but must be public to allow structural equality
  // comparison when Type<N> is used as a template argument.
  std::array<char, N + 1> type_string_;

 private:
  constexpr std::pair<const char*, std::size_t> contents() const override {
    return std::pair(type_string_.data(), N);
  }
};

// Type that holds the type_string in a dynamic allocation. Like std::string,
// constexpr constructors and methods are provided to allow the type to be used
// to hold temporary values at compile time. Also like std::string, and unlike
// a fixed-size Type, a dynamic-extent Type used at compile time must be
// destroyed by the end of constant evaluation. It may not be stored for later
// use by runtime code.
template <>
class Type<base::dynamic_extent> final : public TypeBase {
 public:
  // Dynamic extent constructors.
  explicit constexpr Type(std::string string)
      : type_string_(std::move(string)) {}
  explicit constexpr Type(std::string_view string)
      : Type(std::string(string)) {}
  explicit constexpr Type(const char* string) : Type(std::string(string)) {}
  explicit Type(const GVariantType* type);

  // Dynamic concatenating constructor. Each piece must be explicitly
  // convertible to a std::string_view.
  template <typename... Pieces>
    requires(sizeof...(Pieces) > 1)
  explicit constexpr Type(Pieces&&... pieces)
      : type_string_(
            // StrCat can't be used at compile time, but is more efficient at
            // run time.
            std::is_constant_evaluated()
                ? (std::string() + ... +
                   std::string(static_cast<std::string_view>(pieces)))
                : base::StrCat({static_cast<std::string_view>(pieces)...})) {}

  // Copyable and movable

  constexpr Type(const Type& other) = default;
  constexpr Type& operator=(const Type& other) = default;

  constexpr Type(Type&& other) = default;
  constexpr Type& operator=(Type&& other) = default;

 private:
  constexpr std::pair<const char*, std::size_t> contents() const override {
    return std::pair(type_string_.data(), type_string_.size());
  }
  std::string type_string_;
};

// Deduction guides for Type.

// Compile-time string constants create a fixed-size Type.
template <std::size_t M>
Type(const char (&string_lit)[M]) -> Type<M - 1>;

// Concatenations of any mix of compile-time constants and fixed-size Types
// yield a fixed-size Type.
#if 0
  template <typename... Pieces>
    requires(sizeof...(Pieces) > 1)
  Type(Pieces&&... pieces)
      ->Type<(... + decltype(Type(std::forward<Pieces>(pieces)))::fixed_size)>;
#else
// The above doesn't work in the current version of Clang, but that can be
// worked around by stamping out the first two arguments manually:
template <typename First, typename Second, typename... Rest>
Type(First&& first, Second&& second, Rest&&... rest)
    -> Type<((decltype(Type(std::forward<First>(first)))::fixed_size +
              decltype(Type(std::forward<Second>(second)))::fixed_size) +
             ... + decltype(Type(std::forward<Rest>(rest)))::fixed_size)>;
#endif

// Other constructions will produce a dynamic Type.
template <typename... Pieces>
Type(Pieces&&...) -> Type<base::dynamic_extent>;

// Type implementation.

constexpr const char* TypeBase::c_string() const {
  return contents().first;
}

constexpr std::string_view TypeBase::string_view() const {
  auto contents = this->contents();
  return std::string_view(contents.first, contents.second);
}

constexpr TypeBase::operator const char*() const {
  return c_string();
}

constexpr TypeBase::operator std::string_view() const {
  return string_view();
}

constexpr TypeBase::operator Type<>() const {
  return Type<>(string_view());
}

constexpr const char* TypeBase::begin() const {
  return contents().first;
}

constexpr const char* TypeBase::end() const {
  auto contents = this->contents();
  return contents.first + contents.second;
}

constexpr std::size_t TypeBase::size() const {
  return contents().second;
}

constexpr bool TypeBase::operator==(const TypeBase& other) const {
  return string_view() == other.string_view();
}

constexpr bool TypeBase::IsSubtypeOf(const TypeBase& supertype) const {
  if (!IsValid() || !supertype.IsValid()) {
    return false;
  }

  const char* sub_iter = c_string();
  const char* super_iter = supertype.c_string();

  while (*super_iter != '\0') {
    if (*super_iter == *sub_iter ||
        (*super_iter == '?' && IsBasicType(*sub_iter))) {
      ++sub_iter, ++super_iter;
    } else if (*sub_iter == ')' || *sub_iter == '}' || *sub_iter == '\0') {
      return false;
    } else if (*super_iter == '*' || (*super_iter == 'r' && *sub_iter == '(')) {
      sub_iter = SkipType(sub_iter);
      ++super_iter;
    } else {
      return false;
    }
  }
  return *sub_iter == '\0';
}

constexpr bool TypeBase::HasCommonTypeWith(const TypeBase& other) const {
  if (!IsValid() || !other.IsValid()) {
    return false;
  }

  // This is like IsSubtypeOf, but symmetrical.
  const char* iter1 = c_string();
  const char* iter2 = other.c_string();

  while (*iter1 != '\0' && *iter2 != '\0') {
    if (*iter1 == *iter2 || (*iter1 == '?' && IsBasicType(*iter2)) ||
        (*iter2 == '?' && IsBasicType(*iter1))) {
      ++iter1, ++iter2;
    } else if (*iter1 == ')' || *iter1 == '}' || *iter2 == ')' ||
               *iter2 == '}') {
      return false;
    } else if (*iter1 == '*' || (*iter1 == 'r' && *iter2 == '(')) {
      iter2 = SkipType(iter2);
      ++iter1;
    } else if (*iter2 == '*' || (*iter2 == 'r' && *iter1 == '(')) {
      iter1 = SkipType(iter1);
      ++iter2;
    } else {
      return false;
    }
  }
  return *iter1 == *iter2;
}

constexpr bool TypeBase::IsValid() const {
  return Validate(string_view());
}

constexpr bool TypeBase::IsDefinite() const {
  if (!IsValid()) {
    return false;
  }
  for (char c : *this) {
    // These characters indicate, respectively, any type, any basic type, and
    // any tuple, and the presence of any of them makes a type indefinite.
    if (c == '*' || c == '?' || c == 'r') {
      return false;
    }
  }
  return true;
}

constexpr bool TypeBase::IsBasic() const {
  if (size() != 1) {
    return false;
  }

  return IsBasicType(*begin());
}

constexpr bool TypeBase::IsStringType() const {
  if (size() != 1) {
    return false;
  }

  switch (*begin()) {
    case 's':
    case 'o':
    case 'g':
      return true;
    default:
      return false;
  }
}

constexpr bool TypeBase::IsContainer() const {
  if (!IsValid()) {
    return false;
  }

  switch (*begin()) {
    case 'v':
    case 'a':
    case 'm':
    case 'r':
    case '(':
    case '{':
      return true;
    default:
      return false;
  }
}

constexpr bool TypeBase::IsFixedSizeContainer() const {
  if (!IsValid()) {
    return false;
  }

  switch (*begin()) {
    case 'v':
    case '(':
    case '{':
      return true;
    default:
      return false;
  }
}

constexpr std::optional<std::vector<Type<>>> TypeBase::Unpack() const {
  if (!IsFixedSizeContainer()) {
    return std::nullopt;
  }

  std::vector<Type<>> result;

  if (string_view() == "v") {
    result.emplace_back("*");
  } else {
    const char* position = begin() + 1;  // Skip opening '(' or '{'
    while (*position != ')' && *position != '}') {
      const char* next = SkipType(position);
      result.emplace_back(std::string_view(position, next));
      position = next;
    }
  }
  return result;
}

// static
template <Type C>
consteval /*std::tuple<Type<Ns>...>*/ auto TypeBase::Unpack()
  requires(C.IsFixedSizeContainer())
{
  constexpr auto unpack = []() { return C.Unpack().value(); };

  return ToFixedTuple(unpack);
}

constexpr Type<> TypeBase::CommonSuperTypeWith(const TypeBase& other) const {
  if (!IsValid() || !other.IsValid()) {
    return Type<>("*");
  } else if (*this == other) {
    return Type<>(*this);
  } else if (IsBasic() && other.IsBasic()) {
    return Type<>("?");
  } else if ((*begin() == 'r' && *other.begin() == '(') ||
             (*begin() == '(' && *other.begin() == 'r')) {
    return Type<>("r");
  } else if (*begin() == *other.begin()) {
    // Containers of the same type. (Only way first char can be equal but not
    // the rest.)
    const char container_char = *begin();
    if (container_char == 'a' || container_char == 'm') {
      return Type<>(std::string_view(&container_char, 1),
                    ContainedType().value().CommonSuperTypeWith(
                        other.ContainedType().value()));
    }
    // Must be tuple or dict entry.
    std::vector<Type<>> my_types = Unpack().value();
    std::vector<Type<>> other_types = other.Unpack().value();

    if (my_types.size() != other_types.size()) {
      return Type<>("r");
    }

    std::string super_types;
    super_types.reserve(size() - 2);
    for (std::size_t i = 0; i < my_types.size(); ++i) {
      super_types +=
          my_types[i].CommonSuperTypeWith(other_types[i]).string_view();
    }
    return Type<>(std::string_view(&container_char, 1), super_types,
                  container_char == '(' ? ")" : "}");
  } else {
    return Type<>("*");
  }
}

// static
template <typename R>
  requires std::ranges::range<R> &&
           std::convertible_to<std::ranges::range_value_t<R>, const TypeBase&>
constexpr Type<> TypeBase::CommonSuperTypeOf(const R& types) {
  if (std::ranges::size(types) == 0) {
    return Type<>("*");
  }
  return std::accumulate(
      std::next(std::ranges::begin(types)), std::ranges::end(types),
      Type<>(static_cast<const TypeBase&>(*std::ranges::begin(types))),
      [](const TypeBase& type1, const TypeBase& type2) {
        return type1.CommonSuperTypeWith(type2);
      });
}

// static
template <Type... Types>
consteval /*Type<N>*/ auto TypeBase::CommonSuperTypeOf() {
  constexpr auto find_super_type = []() {
    return CommonSuperTypeOf(
        std::initializer_list<std::reference_wrapper<const TypeBase>>{
            std::cref<TypeBase>(Types)...});
  };
  return ToFixed(find_super_type);
}

constexpr std::optional<Type<>> TypeBase::ContainedType() const {
  if (!IsContainer()) {
    return std::nullopt;
  }
  if (*this == Type("v") || *this == Type("r")) {
    return Type<>("*");
  } else if (*begin() == 'a' || *begin() == 'm') {
    return Type<>(begin() + 1);
  } else {
    // Tuple or dict entry
    std::vector<Type<>> inner_types = Unpack().value();
    return CommonSuperTypeOf(inner_types);
  }
}

// static
template <Type C>
  requires(C.IsContainer())
consteval /*Type<N>*/ auto TypeBase::ContainedType() {
  constexpr auto get_contained_type = []() {
    return C.ContainedType().value();
  };
  return ToFixed(get_contained_type);
}

// static
constexpr bool TypeBase::Validate(std::string_view string) {
  if (string.empty()) {
    return false;
  }

  switch (string.front()) {
    case 'b':
    case 'y':
    case 'n':
    case 'q':
    case 'i':
    case 'u':
    case 'x':
    case 't':
    case 'd':
    case 's':
    case 'o':
    case 'g':
    case 'h':
    case 'v':
    case 'r':
    case '?':
    case '*':
      return string.size() == 1;
    case 'a':
    case 'm':
      return Validate(string.substr(1));
    case '{':
      return string.back() == '}' && IsBasicType(string[1]) &&
             Validate(string.substr(2, string.size() - 3));
    case '(': {
      if (string.back() != ')') {
        return false;
      }
      std::string_view remaining = string.substr(1, string.size() - 2);
      while (remaining.size() != 0) {
        std::size_t type_length = SkipType(remaining.data()) - remaining.data();
        if (type_length > remaining.size() ||
            !Validate(remaining.substr(0, type_length))) {
          return false;
        }
        remaining.remove_prefix(type_length);
      }
      return true;
    }
    default:
      return false;
  }
}

// static
constexpr bool TypeBase::IsBasicType(char code) {
  switch (code) {
    case 'b':
    case 'y':
    case 'n':
    case 'q':
    case 'i':
    case 'u':
    case 'x':
    case 't':
    case 'd':
    case 's':
    case 'o':
    case 'g':
    case 'h':
      // Indefinite type representing any basic type.
    case '?':
      return true;
    default:
      return false;
  }
}

// static
constexpr const char* TypeBase::SkipType(const char* start) {
  // SkipType is used by Validate, and thus shouldn't assume start points to a
  // valid type string. In the event of an invalid type string, SkipType needn't
  // do anything particularly sensible, but it should ensure that (1) it never
  // skips past the terminating null byte and (2) if start isn't already
  // pointing to a null byte, it skips at least one character (to avoid infinite
  // loops).
  while (*start == 'a' || *start == 'm') {
    ++start;
  }
  std::size_t depth = 0;
  do {
    if (*start == '\0') {
      break;
    } else if (*start == '(' || *start == '{') {
      ++depth;
    } else if (*start == ')' || *start == '}') {
      --depth;
    }
    ++start;
  } while (depth != 0);
  return start;
}

// static
template <typename Callable>
consteval /* Type<N> */ auto TypeBase::ToFixed(Callable get_dynamic) {
#if 0
  // This will hopefully be supported in a future version of C++.
  // See https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2024/p3032r2.html
  constexpr Type<> type = get_dynamic();
  return Type(type.c_string(),
              std::integral_constant<std::size_t, type.size()>());
#else
  // In the current C++ version, a compile-time value with dynamic allocation
  // cannot be stored in a constexpr variable, even in a consteval function from
  // which it is guaranteed not to leak to runtime. To avoid calling get_dynamic
  // twice (once to get the size and once to get the data), it is necessary to
  // initially store the result in an almost-sure-to-be-big-enough fixed-size
  // array and then shrink it to the proper size.

  struct IntermediateResult {
    // A const evaluation compiler error will occur if this isn't big enough.
    std::array<char, 10000> data;
    std::size_t size;
  };

  constexpr IntermediateResult intermediate_result = [](Type<> type) {
    IntermediateResult intermediate_result{};
    intermediate_result.size = type.size();
    std::copy(type.begin(), type.end(), intermediate_result.data.begin());
    return intermediate_result;
  }(get_dynamic());

  return Type(intermediate_result.data.data(),
              std::integral_constant<std::size_t, intermediate_result.size>());
#endif
}

// static
template <typename Callable>
consteval /* std::tuple<Type<N>...> */ auto TypeBase::ToFixedTuple(
    Callable get_dynamic_vector) {
  // Like with ToFixed(), this function copies the result into an almost-sure
  // to-be-big-enough fixed-size arrays that are allowed to be stored in a
  // constexpr variable to avoid calling get_dynamic_vector() multiple times.

  struct IntermediateResult {
    // A const evaluation compiler error will occur if this isn't big enough.
    std::array<char, 10000> data;
    std::array<std::size_t, 10000> sizes;
    std::size_t count;
  };

  constexpr IntermediateResult intermediate_result =
      [](std::vector<Type<>> types) {
        IntermediateResult intermediate_result{};
        intermediate_result.count = types.size();
        std::size_t offset = 0;
        for (std::size_t i = 0; i < types.size(); ++i) {
          intermediate_result.sizes[i] = types[i].size();
          std::copy(types[i].begin(), types[i].end(),
                    intermediate_result.data.begin() + offset);
          offset += types[i].size();
        }
        return intermediate_result;
      }(get_dynamic_vector());

  return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
    std::size_t offset = 0;
    // Uses brace initialization to guarantee in-order evaluation of arguments.
    return std::tuple{[&]() {
      const char* data = intermediate_result.data.data() + offset;
      constexpr std::size_t size = intermediate_result.sizes[Is];
      offset += size;
      return Type(data, std::integral_constant<std::size_t, size>());
    }()...};
  }(std::make_index_sequence<intermediate_result.count>());
}

}  // namespace remoting::gvariant

#endif  // REMOTING_HOST_LINUX_GVARIANT_TYPE_H_
