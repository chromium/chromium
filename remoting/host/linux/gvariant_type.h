// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef REMOTING_HOST_LINUX_GVARIANT_TYPE_H_
#define REMOTING_HOST_LINUX_GVARIANT_TYPE_H_

#include <glib.h>

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

#include "base/check.h"
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

  // Verifies that |string| is a valid type string representing a single,
  // complete type.
  static constexpr bool Validate(std::string_view string);
  static constexpr bool IsBasicType(char code);
  // Given a view within a type string, returns the length of the first single
  // complete type. E.g. given "a{sv}i)", returns 5 (the length of "a{sv}"). If
  // |view| does not begin with a valid complete type, the returned length is
  // unspecified, but will be no greater than view.length().
  static constexpr size_t TypeLength(std::string_view view);

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
      : Type(base::span(string_lit).template first<N>()) {
    CHECK(string_lit[N] == '\0')
        << "This constructor expects a null-terminated string";
  }

  // Construct from possibly non-null-terminated string of the proper length.
  explicit constexpr Type(base::span<const char, N> string) {
    base::span(type_string_).template first<N>().copy_from(string);
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
    base::span<char> remaining = type_string_;
    // "Loop" through the passed values by invoking a lambda for each argument.
    (
        [&]<std::size_t M>(const Type<M>& type) {
          remaining.take_first(type.string_view().size())
              .copy_from(type.string_view());
        }(gvariant::Type(std::forward<Pieces>(pieces))),
        ...);
    remaining.front() = '\0';
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
  auto [string, length] = this->contents();
  return std::string_view(string, length);
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

constexpr bool TypeBase::operator==(const TypeBase& other) const {
  return string_view() == other.string_view();
}

constexpr bool TypeBase::IsSubtypeOf(const TypeBase& supertype) const {
  if (!IsValid() || !supertype.IsValid()) {
    return false;
  }

  std::string_view sub_view = string_view();
  std::string_view super_view = supertype.string_view();

  while (!sub_view.empty() && !super_view.empty()) {
    if (super_view.front() == sub_view.front() ||
        (super_view.front() == '?' && IsBasicType(sub_view.front()))) {
      sub_view.remove_prefix(1);
      super_view.remove_prefix(1);
    } else if (sub_view.front() == ')' || sub_view.front() == '}') {
      return false;
    } else if (super_view.front() == '*' ||
               (super_view.front() == 'r' && sub_view.front() == '(')) {
      sub_view.remove_prefix(TypeLength(sub_view));
      super_view.remove_prefix(1);
    } else {
      return false;
    }
  }
  return sub_view.empty() && super_view.empty();
}

constexpr bool TypeBase::HasCommonTypeWith(const TypeBase& other) const {
  if (!IsValid() || !other.IsValid()) {
    return false;
  }

  // This is like IsSubtypeOf, but symmetrical.
  std::string_view view1 = string_view();
  std::string_view view2 = other.string_view();

  while (!view1.empty() && !view2.empty()) {
    if (view1.front() == view2.front() ||
        (view1.front() == '?' && IsBasicType(view2.front())) ||
        (view2.front() == '?' && IsBasicType(view1.front()))) {
      view1.remove_prefix(1);
      view2.remove_prefix(1);
    } else if (view1.front() == ')' || view1.front() == '}' ||
               view2.front() == ')' || view2.front() == '}') {
      return false;
    } else if (view1.front() == '*' ||
               (view1.front() == 'r' && view2.front() == '(')) {
      view1.remove_prefix(1);
      view2.remove_prefix(TypeLength(view2));
    } else if (view2.front() == '*' ||
               (view2.front() == 'r' && view1.front() == '(')) {
      view1.remove_prefix(TypeLength(view1));
      view2.remove_prefix(1);
    } else {
      return false;
    }
  }
  return view1.empty() && view2.empty();
}

constexpr bool TypeBase::IsValid() const {
  return Validate(string_view());
}

constexpr bool TypeBase::IsDefinite() const {
  if (!IsValid()) {
    return false;
  }
  for (char c : this->string_view()) {
    // These characters indicate, respectively, any type, any basic type, and
    // any tuple, and the presence of any of them makes a type indefinite.
    if (c == '*' || c == '?' || c == 'r') {
      return false;
    }
  }
  return true;
}

constexpr bool TypeBase::IsBasic() const {
  if (string_view().size() != 1) {
    return false;
  }

  return IsBasicType(string_view().front());
}

constexpr bool TypeBase::IsStringType() const {
  if (string_view().size() != 1) {
    return false;
  }

  switch (string_view().front()) {
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

  switch (string_view().front()) {
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

  switch (string_view().front()) {
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

  std::string_view view = string_view();
  std::vector<Type<>> result;

  if (view == "v") {
    result.emplace_back("*");
  } else {
    view.remove_prefix(1);  // Skip opening '(' or '{'
    while (view.front() != ')' && view.front() != '}') {
      std::size_t type_length = TypeLength(view);
      result.emplace_back(view.substr(0, type_length));
      view.remove_prefix(type_length);
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
  } else if ((string_view().front() == 'r' &&
              other.string_view().front() == '(') ||
             (string_view().front() == '(' &&
              other.string_view().front() == 'r')) {
    return Type<>("r");
  } else if (string_view().front() == other.string_view().front()) {
    // Containers of the same type. (Only way first char can be equal but not
    // the rest.)
    const char container_char = string_view().front();
    if (container_char == 'a' || container_char == 'm') {
      return Type<>(std::string_view(&container_char, 1),
                    ContainedType().value().CommonSuperTypeWith(
                        other.ContainedType().value()));
    }
    // Must be tuple or dict entry.
    std::vector<Type<>> my_types = Unpack().value();
    std::vector<Type<>> other_types = other.Unpack().value();

    if (my_types.size() != other_types.size()) {
      // Dict entries always have two entries, so they must be tuples.
      return Type<>("r");
    }

    std::string super_types;
    // A supertype is no longer than either subtype.
    super_types.reserve(string_view().size());
    super_types += container_char;
    for (std::size_t i = 0; i < my_types.size(); ++i) {
      super_types +=
          my_types[i].CommonSuperTypeWith(other_types[i]).string_view();
    }
    super_types += container_char == '(' ? ")" : "}";
    return Type<>(super_types);
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
  } else if (string_view().front() == 'a' || string_view().front() == 'm') {
    return Type<>(string_view().substr(1));
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
        std::size_t type_length = TypeLength(remaining);
        if (!Validate(remaining.substr(0, type_length))) {
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
constexpr size_t TypeBase::TypeLength(std::string_view view) {
  std::size_t initial_length = view.length();
  while (!view.empty() && (view.front() == 'a' || view.front() == 'm')) {
    view.remove_prefix(1);
  }
  std::size_t depth = 0;
  do {
    if (view.empty()) {
      break;
    } else if (view.front() == '(' || view.front() == '{') {
      ++depth;
    } else if (view.front() == ')' || view.front() == '}') {
      --depth;
    }
    view.remove_prefix(1);
  } while (depth != 0);
  return initial_length - view.length();
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
    intermediate_result.size = type.string_view().size();
    base::span(intermediate_result.data)
        .first(type.string_view().size())
        .copy_from(type.string_view());
    return intermediate_result;
  }(get_dynamic());

  return Type(base::span(intermediate_result.data)
                  .template first<intermediate_result.size>());
#endif
}

// static
template <typename Callable>
consteval /* std::tuple<Type<N>...> */ auto TypeBase::ToFixedTuple(
    Callable get_dynamic_vector) {
  // Like with ToFixed(), this function copies the result into almost-sure-to-
  // be-big-enough fixed-size arrays that are allowed to be stored in a
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
        base::span<char> data_span = intermediate_result.data;
        for (std::size_t i = 0; i < types.size(); ++i) {
          intermediate_result.sizes[i] = types[i].string_view().size();
          data_span.take_first(types[i].string_view().size())
              .copy_from(types[i].string_view());
        }
        return intermediate_result;
      }(get_dynamic_vector());

  return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
    base::span<const char> data_span = intermediate_result.data;
    // Uses brace initialization to guarantee in-order evaluation of arguments.
    return std::tuple{
        Type(data_span.take_first<intermediate_result.sizes[Is]>())...};
  }(std::make_index_sequence<intermediate_result.count>());
}

}  // namespace remoting::gvariant

#endif  // REMOTING_HOST_LINUX_GVARIANT_TYPE_H_
