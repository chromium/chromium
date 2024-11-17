// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_GVARIANT_REF_H_
#define REMOTING_HOST_LINUX_GVARIANT_REF_H_

#include <glib-object.h>
#include <glib.h>

#include <algorithm>
#include <array>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <locale>
#include <map>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/types/expected.h"
#include "remoting/host/linux/gvariant_type.h"

// Provides a scoped wrapper, GVariantRef, around a GLib Variant to handle
// reference counting, conversion to and from other C++ types, and additional
// convenience methods when the GVariant's type is statically known.
//
// A good familiarity with the GVariant type system is recommended to use or
// read this code: https://docs.gtk.org/glib/struct.VariantType.html

namespace remoting {
namespace gvariant {

// A GVariantRef is a wrapper around a GLib Variant that handles scoped
// reference counting and convenient, typesafe conversion between GVariants and
// standard types.
//
// GVariants are logically immutable once created, and this class's only data
// member is a reference-counted pointer. Thus, instances can be copied and
// passed around cheaply.
//
// Sample usage:
//
// Creating a GVariantRef:
//
//     // Creates a GVariantRef with a compile-time type of "*" (can hold any
//     // value) and initialize it with a value of type "as" (array of strings)
//     // constructed from the provided span.
//     GVariantRef<> variant =
//         GVariantRef<>::From(base::span{{"Bob", "Sally", "Alice"}});
//
// If a type string is provided as a template argument, the GVariantRef may only
// hold values that match that type string, which may be indefinite. E.g., a
// GVariantRef<"s"> may only strings, while a GVariantRef<"{?*}"> may hold any
// dictionary entry. The type may be narrowed using TryFrom:
//
//     GVariantRef<> variant = ...;
//     // expectedVariant will be a base::expected with either the requested
//     // GVariantRef or an error message suitable for logging.
//     auto expectedVariant = GVariantRef<"as">::TryFrom(variant);
//     if (expectedVariant.has_value()) {
//         // expectedVariant.value() is guaranteed to be an array of strings
//         // and can be iterated through, infalibly converted to a
//         // std::vector<std::string>, passed to functions expected GVariantRef
//         // arrays, et cetera.
//     }
//
// Conversion can combine narrowing and converting from C++ types:
//
//     std::tuple<GVariantRef<>> tuple = ...;
//     auto expectedVariant = GVariantRef<"(s)">::TryFrom(tuple);
//
// Getting values from a GVariantRef:
//
//     GVariantRef<> variant = ...;
//     // Returns a base::expected with the value or an error message.
//     auto value = result.TryInto<std::vector<std::string>>();
//
// If the requested conversion is valid for all values of the GVariantRef's
// static type, you can use Into instead to get the value directly:
//
//     GVariantRef<"s"> variant = ...;
//     std::string value = variant.Into<std::string>();
//
// Like TryFrom, TryInto can also be used to attempt to narrow a GVariantRef's
// static type, and can be combined with conversions to C++ types:
//
//     GVariantRef<> variant = ...;  // Compile-time type string is "*";
//     // Value will be returned if variant's run-time type is a subtype of
//     // "a(s*)". Otherwise, an error string will be returned.
//     auto result2 = variant.TryInto<std::vector<GVariantRef<"(s*)">>>();
//
// If the GVariantRef is known to be a container (that is, it's static type is
// a boxed value, an array, a maybe value, a tuple, or a dictionary entry), it
// can be used with range-based for:
//
//     GVariantRef<"a(iu)"> variant1 = ...;
//     for (auto item : variant1) { /* item is a GVariantRef<"(iu)"> */}
//     GVariantRef<"r"> variant2 = ...;
//     for (auto item : variant1) { /* item is a GVariantRef<"*"> */}
//
// If it is known to be a container of a fixed size (boxed value, fixed-size
// tuple (not "r"), or dictionary entry), it can be used with structured
// binding:
//
//     GVariantRef<"(uba{sv})"> variant = ...;
//     // Yields GVariantRef<"u">, GVariantRef<"b">, and GVariantRef<"a{sv}">
//     auto [first, second, third] = variant;
//
// For convenience, GVariantRefs known to be a dictionary type (the compile-time
// type string is a subtype of "a{?*}") provide a LookUp() method to find the
// first value with a matching key.
//
//     GVariantRef<"a{is}"> variant = ...;
//     std::optional<GVariantRef<"s">> value = variant.LookUp(5);
//
// Finally, values can be extracted from containers using Destructure() and
// TryDestructure(). Destructure requires a known fixed-size container, while
// TryDestructure checks if the GVariantRef holds a matching container at
// runtime.
//
//     GVariantRef<"(ii(ssssv))"> variant = ...;
//     std::uint32_t i1, i2;
//     std::string s1, s2, s3, s4;
//     GVariantRef<> v;
//     // Inner containers can be unpacked by passing (possibly nested) tuples
//     // of lvalue references.
//     variant.Destructure(i1, i2, std::forward_as_tuple(s1, s2, s3, s4,
//                                                       std::tie(v)));
//
//     GVariantRef<"mb"> variant = ...;
//     bool b;
//     // result will contain an error if the maybe value is unexpectedly empty.
//     base::expected<void> result = variant.TryDestructure(b);
//
// Access to the underlying GVariant pointer can be obtained via raw() and
// release(). A GVariantRef can be created from a raw GVariant pointer via
// Ref(), RefSink(), and Take().
//
// For convenience, the GVariantRef class is exported to the remoting namespace.
//
// This header provides conversions to and from the following types. Additional
// support for additional types can be added by providing an appropriate
// specialization of gvariant::Mapping<T>.
//
// Basic fixed types:
//
// bool (type code "b")
// std::uint8_t (type code "y")
// std::int16_t (type code "n")
// std::uint16_t (type code "q")
// std::int32_t (type code "i")
// std::uint32_t (type code "u")
// std::int64_t (type code "x")
// std::uint64_t (type code "t")
// double (type code "d")
//
// Strings:
//
// std::string (type code "s")
// const char * ([Try]From() only, type code "s")
// std::string_view ([Try]From() only, type code "s")
//
// Note: Strings must be valid UTF-8. For convenience, both From() and TryFrom()
// are provided. If the string (or all strings in an aggregate type) are
// known to be valid UTF-8, From() may be used. Otherwise TryFrom() should be
// used. Calling From() with a string containing invalid UTF-8 will
// result in a crash.
//
// Containers:
//
// std::optional<T> (type code "mT") - To use with From(), the type string of T
//     must be definite. E.g., it may not contain an unboxed heterogeneous
//     std::variant or unconstrained GVariantRef. Otherwise, TryFrom() must be
//     used. Alternatively, GVariantRef::FilledMaybe can be used for indefinite
//     types that will never actually be absent. Note that maybe values are not
//     currently supported by D-Bus.
// std::vector<T> (type code "aT") - To use with From(), the type string of T
//     must be definite. Otherwise, TryFrom() must be used.
// std::map<K, T> (type code "a{KT}") - To use with From(), the type string of K
//     and T must be definite. Otherwise, TryFrom() must be used.
// std::pair<K, T> (type code "{KT}")
// std::ranges::range<T> ([Try]From() only, type code "aE" where
//     E = std::ranges::range_value_t<T>) - The type string of T must be
//     definite to use with From(). Otherwise, TryFrom() must be used.
// std::tuple<...> (type code "(...)")
// std::variant<...> (type code "S", where S is the narrowest common supertype
//     of all of the variant alternatives) - Despite the name, when used with
//     From(), the type will be the type of the active variant, not "v". If the
//     type should be "v", use a gvariant::Boxed<std::variant<...>>. When used
//     with [Try]Into(), each variant alternative will be
//     tried in turn until one succeeds or all are exhausted. Similarly to
//     From(), if the type in the GVariant is expected to be "v" rather than a
//     concrete type, use with gvariant::Boxed. From() and TryFrom() are
//     provided if all of the alternatives provide the respected method.
//     TryInto() is provided if provided by any of the alternatives. Into() is
//     provided if the GVariantRef at hand can be infallibly converted to any
//     of the variant's alternatives. E.g., the following is valid:
//
//         GVariantRef<"i"> gvariant = ...;
//         auto v = gvariant.Into<std::variant<std::int32, bool>>();
//
// Special types:
//
// GVariantRef<C> (type code "C") - GVariantRef can itself be used with
//     [Try]From() or [Try]Into(). This can be used to widen or narrow the
//     type or to hold inner values as nested GVariants. Like with
//     std::variant, the type of the GVariantRef is used directly. If the
//     expected/desired type is "v", wrap it in a gvariant::Boxed.
// gvariant::Ignored ([Try]Into() only, type code "*") - Discard the value at
//     this position instead of decoding it.
// decltype(std::ignore) - Same as gvariant::Ignored. Allows passing std::ignore
//     to [Try]Destructure().
// gvariant::Boxed<T> (type code "v") - Wrapper for a value that appears boxed
//     inside a nested variant, rather than directly. A Boxed<T> is definite
//     regardless of whether the contained type is. Provides [Try]From() and
//     [Try]Into() if the inner type provides them for GVariantRef<"*">.
// gvariant::FilledMaybe<T> (type code "mT") - Wrapper for a value that
//     appears inside a maybe, but will always be present. Presence of
//     [Try]From() and TryInto() mirror the inner type. Into() is never
//     provided.
// gvariant::EmptyArrayOf<C> (type code "aC") - Represents an empty array of
//     the type C, which should be an instance of GVariantRef::Type. C must be
//     definite to use with [Try]From(). Into() is never provided; TryInto()
//     must be used to check if the array is, in fact, empty.
// gvariant::ObjectPath (type code "o") - Wrapper around a std::string that
//     contains a DBus object path.
// gvariant::TypeSignature (type code "g") - Wrapper around a std::string
//     that contains a DBus type signature.
template <Type C = Type("*")>
class GVariantRef;

// A struct specialized for each type supporting conversion to or from a
// GVariant.
//
// All specializations must provide a kType field, which should be in the form
// of a static constexpr Type. If the exact type can only be determined at
// runtime, an indefinite type can be used (e.g., "*").
//
//     static constexpr Type kType{"i"};
//
// A given type may support conversion to a GVariant, from a GVariant, or
// both. An example of a type that only supports conversion to a GVariant is
// a C-style string (const char *). An example of a type that only supports
// conversion from a GVariant is GVariantRef::Ignored.
//
// To support conversion to a GVariant, the specialization should provide a
// static From or TryFrom method that takes a value of the type and returns a
// GVariantRef<kType>, e.g.,
//
//     static GVariantRef<kType> From(const T&);
//     static base::expected<GVariantRef<kType>> TryFrom(const T&);
//
// Usually a specialization will provide one or the other.
//
// To support conversion from a GVariant, the specialization should provide a
// static Into or TryInto method that takes a GVariantRef<kType> and returns a
// value of the type, e.g.,
//
//     static T Into(const GVariantRef<kType>&);
//     static std::expected<T, std::string> TryInto(const GVariantRef<kType>&);
//
// Usually a specialization will provide one or the other, with one exception:
// In the event that infallible conversion is possible for only some subtypes of
// kType, Into may be specified only for those subtypes. In that case, TryInto
// must also be specified. E.g., if a conversion can take an int or a string,
// the definition might look like this:
//
//     static constexpr Type kType{"?"};
//     static std::expected<T, std::string> TryInto(const GVariantRef<kType>&);
//     static T Into(const GVariantRef<"i">&);
//     static T Into(const GVariantRef<"s">&);
//
// In the event a Try* conversion fails, the method should return an error
// string suitable for inclusion in a log message.
template <typename T>
struct Mapping;

// Common members of all GVariantRefs.
class GVariantBase {
 public:
  // Allows creating an owned reference given a reference to GVariantBase.
  explicit operator GVariantRef<>() const;

  // Returns the inner GVariant reference without incrementing the count. It is
  // the caller's responsibility to increment the ref count if it is to be held
  // longer than the containing remoting::GVariant instance. Otherwise, the
  // caller should not call unref on it.
  GVariant* raw() const;

  // Takes owneriship of the inner GVariant, leaving this in a moved-from state.
  // It is the caller's responsibility to call unref when they are done with it.
  [[nodiscard]] GVariant* release() &&;

  // Returns the type of the value currently held in the GVariant. Will always
  // be definite.
  Type<> GetType() const;

 protected:
  // clang-format off
  static constexpr struct {} kTake;
  static constexpr struct {} kRef;
  static constexpr struct {} kRefSink;
  // clang-format on
  GVariantBase();
  GVariantBase(decltype(kTake), GVariant* variant);
  GVariantBase(decltype(kRef), GVariant* variant);
  GVariantBase(decltype(kRefSink), GVariant* variant);
  GVariantBase(const GVariantBase& other);
  GVariantBase(GVariantBase&& other);
  GVariantBase& operator=(const GVariantBase& other);
  GVariantBase& operator=(GVariantBase&& other);
  ~GVariantBase();

  bool operator==(const GVariantBase& other) const;

 private:
  raw_ptr<GVariant> variant_;
};

template <Type C>
class GVariantRef : public GVariantBase {
 public:
  static constexpr auto kType = C;

  // Default constructor constructs an empty GVariantRef in the same state as
  // one that has been moved from. No operations are valid until a value has
  // been assigned.
  GVariantRef() = default;

  // Copyable and movable. Copies are cheap, only bumping the reference count.
  // The only valid operations for a moved-from instance are dropping it and
  // assigning a new value.
  GVariantRef(const GVariantRef& other) = default;
  GVariantRef(GVariantRef&& other) = default;
  GVariantRef& operator=(const GVariantRef& other) = default;
  GVariantRef& operator=(GVariantRef&& other) = default;
  ~GVariantRef() = default;

  // Allow implicit conversion from subtype to supertype.
  template <Type D>
  // NOLINTNEXTLINE(google-explicit-constructor)
  GVariantRef(const GVariantRef<D>& other)
    requires(D.IsSubtypeOf(C));

  template <Type D>
  // NOLINTNEXTLINE(google-explicit-constructor)
  GVariantRef(GVariantRef<D>&& other)
    requires(D.IsSubtypeOf(C));

  // Type conversions

  // Constructs a new GVariantRef from the provided value.
  template <typename T>
  static GVariantRef From(const T& value)
    requires(Mapping<T>::kType.IsSubtypeOf(C) &&
             requires { Mapping<T>::From(value); });

  // Constructs a new GVariantRef from the provided value, if possible.
  template <typename T>
  static base::expected<GVariantRef, std::string> TryFrom(const T& value)
    requires(Mapping<T>::kType.HasCommonTypeWith(C) &&
             (requires { Mapping<T>::TryFrom(value); } ||
              requires { Mapping<T>::From(value); }));

  // Builds a value of the provided type from the contents of the GVariant.
  // Calls will only compile if they are guaranteed to succeed at runtime.
  template <typename T>
  T Into() const
    requires(C.IsSubtypeOf(Mapping<T>::kType) &&
             requires { Mapping<T>::Into(*this); });

  // Builds a value of the provided type from the contents of the GVariant, if
  // possible. Fails to compile if the conversion can statically be determined
  // never to succeed.
  template <typename T>
  base::expected<T, std::string> TryInto() const
    requires(C.HasCommonTypeWith(Mapping<T>::kType) &&
             (requires(GVariantRef<Mapping<T>::kType> v) {
                Mapping<T>::TryInto(v);
              } ||
              requires(GVariantRef<Mapping<T>::kType> v) {
                Mapping<T>::Into(v);
              }));

  // Unpacks a container GVariant into lvalue references. The GVariant must be a
  // boxed value, a tuple, or a dictionary entry. Each argument must be a lvalue
  // reference or a tuple to unpack a nested container. Each tuple should
  // similarly consist of lvalue references and tuples. std::tie and
  // std::forward_as_tuple can be useful for constructing such tuples. Calls to
  // Destructure will only compile if it can be guaranteed at compile time that
  // the operation will succeed.
  template <typename... Types>
  void Destructure(Types&&... refs) const;

  // As above, but allows conversions that could fail at runtime. The GVariant
  // must be a boxed value, a maybe value, a tuple, an array, or a dictionary
  // entry. If the types or number of values don't match at runtime, an error
  // message will be returned. In this case, the reference lvalues will be in an
  // indeterminate state, as some values may have been read prior to the error
  // occurring.
  template <typename... Types>
  base::expected<void, std::string> TryDestructure(Types&&... refs) const;

  // Iterate through the values of a container GVariant. The value type will be
  // GVariant<TypeBase::ContainedType(C)>.
  auto begin() const
    requires(C.IsContainer());
  auto end() const
    requires(C.IsContainer());
  std::size_t size() const
    requires(C.IsContainer());

  // Get the Ith element from a fixed-size container GVariant. Specializations
  // of std::tuple_size and std::tuple_element are also provided to make fixed-
  // size container GVariants tuple-like.
  template <std::size_t I>
  auto get() const
    requires(C.IsFixedSizeContainer());

  // Look up the provided key in a dictionary GVariant. The provided argument
  // must be convertible to the key type via TryFrom. Returns a
  // std::optional<GVariantRef<[value type]>> containing the found value, or
  // nullopt if the key was not found. Note that this performs a linear search
  // through the dictionary. If the dictionary is large and many lookups will be
  // performed, it might be more efficient to convert to a std::map or another
  // datastructure first.
  template <typename T>
  auto LookUp(const T& key)
    requires(C.IsSubtypeOf(Type("a{?*}")) && requires {
      decltype((*begin()).template get<0>())::TryFrom(key);
    });

  // Access the contained string without copying. C must be "s", "o", or "g".
  std::string_view string_view() const
    requires(C.IsStringType());

  // Access the contained string without copying. C must be "s", "o", or "g".
  const char* c_string() const
    requires(C.IsStringType());

  // Create from a raw GVariant*. Can only be used with GVariantRef<Type("*")>.
  // To get a narrower GVariantRef, first create one with "*" and then use
  // TryInto (or TryFrom) to convert to the narrower type.

  // Takes ownership of the reference. If it is floating, it will be sunk. May
  // not be null.
  //
  // This is the right method to call with a floating or caller-owned
  // GVariant* when one wants to pass ownership to the new GVariantRef. Most
  // GLib functions returning a GVariant* will return either a floating
  // reference (e.g., `g_variant_new`) or a caller owned reference (e.g.,
  // `get_child_value`), making their return value suitable to be passed to this
  // method (after checking for null, if the function is falible).
  static GVariantRef Take(GVariant* variant)
    requires(C == Type("*"));

  // Takes a new reference unconditionally. If object is floating, it will
  // remain so. May not be null.
  //
  // This is the right method to call with a borrowed GVariant* (such as is
  // returned by `raw()` or might be owned by a different class/struct) or a
  // caller-owned GVariant* of which the caller wants to maintain ownership.
  static GVariantRef Ref(GVariant* variant)
    requires(C == Type("*"));

  // If the reference count is floating, takes ownership and sinks it.
  // Otherwise, takes a new reference. May not be null.
  //
  // This is usually the right method to call with a GVariant* that was passed
  // into the caller as an argument. That gives the caller's caller the
  // flexibility to pass in either a newly-constructed GVariant (in which case
  // the reference will be floating, and GVariantRef will take ownership) or an
  // existing GVariant* (in which case GVariantRef will take its own reference).
  static GVariantRef RefSink(GVariant* variant)
    requires(C == Type("*"));

  // Same as above, but can be used with any GVariantRef type. The caller must
  // ensure the passed pointer is actually of the appropriate type. Passing a
  // pointer that does not match C can result in undefined behavior.
  static GVariantRef TakeUnchecked(GVariant* variant);
  static GVariantRef RefUnchecked(GVariant* variant);
  static GVariantRef RefSinkUnchecked(GVariant* variant);

  template <Type D>
  bool operator==(const GVariantRef<D>& other) const
    requires(C.HasCommonTypeWith(D));

 private:
  using GVariantBase::GVariantBase;
};

// Wrapper types and special types

// Can be used as a nested type in a call to [Try]Into() as a placeholder for a
// value the caller isn't interested in.
struct Ignored {};

// Wrapper for a value to specify that it will appear in the GVariant "boxed".
// That is, as a nested variant (type "v") holding the value, rather than the
// value directly.
template <typename T>
struct Boxed {
  T value;

  bool operator==(const Boxed& other) const = default;
};

// Wrapper for a value that should appear inside a maybe, but will always be
// present.
template <typename T>
struct FilledMaybe {
  T value;

  bool operator==(const FilledMaybe& other) const = default;
};

// Represents an empty array of the given type.
template <Type C>
struct EmptyArrayOf {};

// Represents a D-Bus object path.
class ObjectPath {
 public:
  static base::expected<ObjectPath, std::string> TryFrom(std::string path);
  const std::string& value() const;

  bool operator==(const ObjectPath& other) const = default;

 private:
  explicit ObjectPath(std::string);
  std::string path_;
  friend struct Mapping<ObjectPath>;
};

// Represents a D-Bus type signature.
class TypeSignature {
 public:
  static base::expected<TypeSignature, std::string> TryFrom(
      std::string signature);
  const std::string& value() const;

  bool operator==(const TypeSignature& other) const = default;

 private:
  explicit TypeSignature(std::string);
  std::string signature_;
  friend struct Mapping<TypeSignature>;
};

// Iterator type for a container GVariant
template <Type C>
class Iterator;

template <Type C>
Iterator<C> operator+(std::ptrdiff_t n, const Iterator<C>& iter);

template <Type C>
class Iterator {
 public:
  Iterator() = default;
  // Copyable and movable
  Iterator(const Iterator& other) = default;
  Iterator(Iterator&& other) = default;
  Iterator& operator=(const Iterator& other) = default;
  Iterator& operator=(Iterator&& other) = default;

  // Iterator interface

  // LegacyForwardIterator requires reference_type be &value_type or
  // const &value_type, so this implementation is only a LegacyInputIterator.
  using iterator_category = std::input_iterator_tag;

  // The std::forward_iterator and higher concepts impose no such requirement,
  // so this can be a full random-access iterator.
  using iterator_concept = std::random_access_iterator_tag;

  using value_type = GVariantRef<C>;
  using reference_type = GVariantRef<C>;
  using difference_type = std::ptrdiff_t;

  // input iterator
  value_type operator*() const;
  Iterator& operator++();
  Iterator operator++(int);

  // forward iterator
  bool operator==(const Iterator& other) const;

  // bidirectional iterator
  Iterator& operator--();
  Iterator operator--(int);

  // random access iterator
  std::partial_ordering operator<=>(const Iterator& other) const;
  difference_type operator-(const Iterator& other) const;
  Iterator operator+(difference_type n) const;
  friend Iterator operator+ <C>(difference_type n, const Iterator& iter);
  Iterator operator-(difference_type n) const;
  Iterator& operator+=(difference_type n);
  Iterator& operator-=(difference_type n);
  value_type operator[](difference_type i) const;

 private:
  Iterator(GVariantRef<> variant, std::size_t i);
  std::size_t i_ = 0;
  GVariantRef<> variant_;

  template <Type D>
  friend class GVariantRef;
};

// GVariantRef implementation

template <Type C>
template <Type D>
GVariantRef<C>::GVariantRef(const GVariantRef<D>& other)
  requires(D.IsSubtypeOf(C))
    : GVariantBase(kRef, other.raw()) {}

template <Type C>
template <Type D>
GVariantRef<C>::GVariantRef(GVariantRef<D>&& other)
  requires(D.IsSubtypeOf(C))
    : GVariantBase(kTake, std::move(other).release()) {}

// static
template <Type C>
template <typename T>
GVariantRef<C> GVariantRef<C>::From(const T& value)
  requires(Mapping<T>::kType.IsSubtypeOf(C) &&
           requires { Mapping<T>::From(value); })
{
  return Mapping<T>::From(value);
}

// static
template <Type C>
template <typename T>
base::expected<GVariantRef<C>, std::string> GVariantRef<C>::TryFrom(
    const T& value)
  requires(Mapping<T>::kType.HasCommonTypeWith(C) &&
           (requires { Mapping<T>::TryFrom(value); } ||
            requires { Mapping<T>::From(value); }))
{
  if constexpr (requires { Mapping<T>::TryFrom(value); }) {
    return Mapping<T>::TryFrom(value).and_then(
        [](auto value) { return value.template TryInto<GVariantRef>(); });
  } else {
    return Mapping<T>::From(value).template TryInto<GVariantRef>();
  }
}

template <Type C>
template <typename T>
T GVariantRef<C>::Into() const
  requires(C.IsSubtypeOf(Mapping<T>::kType) &&
           requires { Mapping<T>::Into(*this); })
{
  return Mapping<T>::Into(*this);
}

template <Type C>
template <typename T>
base::expected<T, std::string> GVariantRef<C>::TryInto() const
  requires(C.HasCommonTypeWith(Mapping<T>::kType) &&
           (requires(GVariantRef<Mapping<T>::kType> v) {
              Mapping<T>::TryInto(v);
            } ||
            requires(GVariantRef<Mapping<T>::kType> v) {
              Mapping<T>::Into(v);
            }))
{
  if constexpr (!C.IsSubtypeOf(Mapping<T>::kType)) {
    if (GetType().IsSubtypeOf(Mapping<T>::kType)) {
      return GVariantRef<Mapping<T>::kType>::RefUnchecked(raw())
          .template TryInto<T>();
    } else {
      return base::unexpected(
          base::StrCat({"Expected type: ", Mapping<T>::kType.string_view(),
                        " Found: ", GetType().string_view()}));
    }
  } else if constexpr (requires { Mapping<T>::TryInto(*this); }) {
    return Mapping<T>::TryInto(*this);
  } else {
    return base::ok(Mapping<T>::Into(*this));
  }
}

template <Type C>
template <typename... Types>
void GVariantRef<C>::Destructure(Types&&... refs) const {
  static_assert((... && (std::is_lvalue_reference_v<Types> ||
                         requires { std::tuple_size_v<Types>; })));
  constexpr auto contained_types = TypeBase::Unpack<C>();
  static_assert(std::tuple_size_v<decltype(contained_types)> == sizeof...(refs),
                "Incorrect number of elements.");
  [&]<std::size_t... Is>(std::index_sequence<Is...>) {
    (
        [&]() {
          auto inner_variant =
              GVariantRef<std::get<Is>(contained_types)>::TakeUnchecked(
                  g_variant_get_child_value(raw(), Is));
          if constexpr (std::is_lvalue_reference_v<Types>) {
            refs =
                inner_variant.template Into<std::remove_reference_t<Types>>();
          } else {
            std::apply(
                [&]<typename... Ts>(Ts&&... subref) {
                  return inner_variant.Destructure(std::forward<Ts>(subref)...);
                },
                std::forward<Types>(refs));
          }
        }(),
        ...);
  }(std::index_sequence_for<Types...>());
}

template <Type C>
template <typename... Types>
base::expected<void, std::string> GVariantRef<C>::TryDestructure(
    Types&&... refs) const {
  static_assert((... && (std::is_lvalue_reference_v<Types> ||
                         requires { std::tuple_size_v<Types>; })));

  if (!g_variant_is_container(raw())) {
    return base::unexpected("Destructured GVariant is not a container.");
  }

  if (std::size_t size = g_variant_n_children(raw()); size != sizeof...(refs)) {
    return base::unexpected(base::StringPrintf(
        "Incorrect number of elements. Expected: %zd Found: %zd",
        sizeof...(refs), size));
  }

  base::expected<void, std::string> result;

  [&]<std::size_t... Is>(std::index_sequence<Is...>) {
    ([&]() {
      GVariantRef<> inner_variant =
          GVariantRef<>::Take(g_variant_get_child_value(raw(), Is));
      if constexpr (std::is_lvalue_reference_v<Types>) {
        auto value = inner_variant.TryInto<std::remove_reference_t<Types>>();
        if (!value.has_value()) {
          result = base::unexpected(std::move(value).error());
          return false;
        }
        refs = value.value();
      } else {
        auto nested_result = std::apply(
            [&]<typename... Ts>(Ts&&... subref) {
              return inner_variant.TryDestructure(std::forward<Ts>(subref)...);
            },
            std::forward<Types>(refs));
        if (!nested_result.has_value()) {
          result = std::move(nested_result);
          return false;
        }
      }
      return true;
    }() &&
     ...);
  }(std::index_sequence_for<Types...>());

  return result;
}

template <Type C>
auto GVariantRef<C>::begin() const
  requires(C.IsContainer())
{
  return Iterator<TypeBase::ContainedType<C>()>(*this, 0);
}

template <Type C>
auto GVariantRef<C>::end() const
  requires(C.IsContainer())
{
  return Iterator<TypeBase::ContainedType<C>()>(*this,
                                                g_variant_n_children(raw()));
}

template <Type C>
std::size_t GVariantRef<C>::size() const
  requires(C.IsContainer())
{
  return g_variant_n_children(raw());
}

template <Type C>
template <std::size_t I>
auto GVariantRef<C>::get() const
  requires(C.IsFixedSizeContainer())
{
  return GVariantRef<std::get<I>(TypeBase::Unpack<C>())>::TakeUnchecked(
      g_variant_get_child_value(raw(), I));
}

// Define free version as well for generic code that calls get(tuple_like) using
// argument-dependent lookup.
template <std::size_t I, Type C>
  requires(C.IsFixedSizeContainer())
auto get(const GVariantRef<C>& variant) {
  return variant.template get<I>();
}

template <Type C>
template <typename T>
auto GVariantRef<C>::LookUp(const T& needle)
  requires(C.IsSubtypeOf(Type("a{?*}")) &&
           requires {
             decltype((*begin()).template get<0>())::TryFrom(needle);
           })
{
  using KeyType = decltype((*begin()).template get<0>());
  using ValueType = decltype((*begin()).template get<1>());
  auto needle_variant = KeyType::TryFrom(needle);
  if (!needle_variant.has_value()) {
    // If the value is something that can't be converted to a GVariant (e.g.,
    // a non-UTF-8 string), it's probably safe to say it's not in the
    // dictionary.
    return std::optional<ValueType>();
  }
  for (auto [key, value] : *this) {
    if (key == needle_variant.value()) {
      return std::optional<ValueType>(std::move(value));
    }
  }
  return std::optional<ValueType>();
}

template <Type C>
std::string_view GVariantRef<C>::string_view() const
  requires(C.IsStringType())
{
  gsize length;
  const char* string = g_variant_get_string(raw(), &length);
  return std::string_view(string, length);
}

template <Type C>
const char* GVariantRef<C>::c_string() const
  requires(C.IsStringType())
{
  return g_variant_get_string(raw(), nullptr);
}

// static
template <Type C>
GVariantRef<C> GVariantRef<C>::TakeUnchecked(GVariant* variant) {
  DCHECK(g_variant_is_of_type(variant, C.gvariant_type()));
  return GVariantRef<C>(kTake, variant);
}

// static
template <Type C>
GVariantRef<C> GVariantRef<C>::RefUnchecked(GVariant* variant) {
  DCHECK(g_variant_is_of_type(variant, C.gvariant_type()));
  return GVariantRef<C>(kRef, variant);
}

// static
template <Type C>
GVariantRef<C> GVariantRef<C>::RefSinkUnchecked(GVariant* variant) {
  DCHECK(g_variant_is_of_type(variant, C.gvariant_type()));
  return GVariantRef<C>(kRefSink, variant);
}

template <Type C>
template <Type D>
bool GVariantRef<C>::operator==(const GVariantRef<D>& other) const
  requires(C.HasCommonTypeWith(D))
{
  return GVariantBase::operator==(other);
}

// Iterator implementation

template <Type C>
GVariantRef<C> Iterator<C>::operator*() const {
  return GVariantRef<C>::TakeUnchecked(
      g_variant_get_child_value(variant_.raw(), i_));
}

template <Type C>
Iterator<C>& Iterator<C>::operator++() {
  ++i_;
  return *this;
}

template <Type C>
Iterator<C> Iterator<C>::operator++(int) {
  return Iterator(variant_, i_++);
}

template <Type C>
bool Iterator<C>::operator==(const Iterator& other) const {
  return variant_.raw() == other.variant_.raw() && i_ == other.i_;
}

template <Type C>
Iterator<C>& Iterator<C>::operator--() {
  --i_;
  return *this;
}

template <Type C>
Iterator<C> Iterator<C>::operator--(int) {
  return Iterator(variant_, i_--);
}

template <Type C>
std::partial_ordering Iterator<C>::operator<=>(const Iterator& other) const {
  if (variant_.raw() != other.variant_.raw()) {
    return std::partial_ordering::unordered;
  }
  return i_ <=> other.i_;
}

template <Type C>
std::ptrdiff_t Iterator<C>::operator-(const Iterator& other) const {
  return static_cast<std::ptrdiff_t>(i_) -
         static_cast<std::ptrdiff_t>(other.i_);
}

template <Type C>
Iterator<C> Iterator<C>::operator+(std::ptrdiff_t n) const {
  return Iterator(variant_, static_cast<std::ptrdiff_t>(i_) + n);
}

template <Type C>
Iterator<C> operator+(std::ptrdiff_t n, const Iterator<C>& iter) {
  return Iterator<C>(iter.variant_, n + static_cast<std::ptrdiff_t>(iter.i_));
}

template <Type C>
Iterator<C> Iterator<C>::operator-(std::ptrdiff_t n) const {
  return Iterator(variant_, static_cast<std::ptrdiff_t>(i_) - n);
}

template <Type C>
Iterator<C>& Iterator<C>::operator+=(std::ptrdiff_t n) {
  i_ = static_cast<std::size_t>(static_cast<std::ptrdiff_t>(i_) + n);
  return *this;
}

template <Type C>
Iterator<C>& Iterator<C>::operator-=(std::ptrdiff_t n) {
  i_ = static_cast<std::size_t>(static_cast<std::ptrdiff_t>(i_) - n);
  return *this;
}

template <Type C>
GVariantRef<C> Iterator<C>::operator[](std::ptrdiff_t i) const {
  return GVariantRef<C>::TakeUnchecked(g_variant_get_child_value(
      variant_.raw(),
      static_cast<std::size_t>(static_cast<std::ptrdiff_t>(i_) + i)));
}

template <Type C>
Iterator<C>::Iterator(GVariantRef<> variant, std::size_t i)
    : i_(i), variant_(variant) {}

// Mapping implementations

// Possibly cv-qualified reference can be used with *From() but not *Into().
template <typename T>
  requires(!std::same_as<T, std::decay_t<T>>)
struct Mapping<T> {
  static constexpr Type kType = Mapping<std::decay_t<const T&>>::kType;

  static auto From(const T& value)
      // Typically one wouldn't want a requires clause that just mirrors the
      // function body. Unfortunately, it is necessary to allow other generic
      // mappings to detect when From is absent, since requires expressions only
      // care about what is valid according to the declaration, not whether the
      // resulting instantiation would actually compile.
    requires(requires {
      GVariantRef<kType>::template From<std::decay_t<const T&>>(value);
    })
  {
    return GVariantRef<kType>::template From<std::decay_t<const T&>>(value);
  }

  static auto TryFrom(const T& value)
    requires(requires {
      GVariantRef<kType>::template TryFrom<std::decay_t<const T&>>(value);
    })
  {
    return GVariantRef<kType>::template TryFrom<std::decay_t<const T&>>(value);
  }
};

// Basic fixed values.

template <>
struct Mapping<bool> {
  static constexpr Type kType{"b"};
  static GVariantRef<kType> From(bool value);
  static bool Into(const GVariantRef<kType>& variant);
};

template <>
struct Mapping<std::uint8_t> {
  static constexpr Type kType{"y"};
  static GVariantRef<kType> From(std::uint8_t value);
  static std::uint8_t Into(const GVariantRef<kType>& variant);
};

template <>
struct Mapping<std::int16_t> {
  static constexpr Type kType{"n"};
  static GVariantRef<kType> From(std::int16_t value);
  static std::int16_t Into(const GVariantRef<kType>& variant);
};

template <>
struct Mapping<std::uint16_t> {
  static constexpr Type kType{"q"};
  static GVariantRef<kType> From(std::uint16_t value);
  static std::uint16_t Into(const GVariantRef<kType>& variant);
};

template <>
struct Mapping<std::int32_t> {
  static constexpr Type kType{"i"};
  static GVariantRef<kType> From(std::int32_t value);
  static std::int32_t Into(const GVariantRef<kType>& variant);
};

template <>
struct Mapping<std::uint32_t> {
  static constexpr Type kType{"u"};
  static GVariantRef<kType> From(std::uint32_t value);
  static std::uint32_t Into(const GVariantRef<kType>& variant);
};

template <>
struct Mapping<std::int64_t> {
  static constexpr Type kType{"x"};
  static GVariantRef<kType> From(std::int64_t value);
  static std::int64_t Into(const GVariantRef<kType>& variant);
};

template <>
struct Mapping<std::uint64_t> {
  static constexpr Type kType{"t"};
  static GVariantRef<kType> From(std::uint64_t value);
  static std::uint64_t Into(const GVariantRef<kType>& variant);
};

template <>
struct Mapping<double> {
  static constexpr Type kType{"d"};
  static GVariantRef<kType> From(double value);
  static double Into(const GVariantRef<kType>& variant);
};

// Strings.

template <>
struct Mapping<std::string> {
  static constexpr Type kType{"s"};
  // Crashes if string is not valid UTF-8.
  static GVariantRef<kType> From(const std::string& value);
  // Fails if string is not valid UTF-8.
  static base::expected<GVariantRef<kType>, std::string> TryFrom(
      const std::string& value);
  static std::string Into(const GVariantRef<kType>& variant);
};

template <>
struct Mapping<std::string_view> {
  static constexpr Type kType{"s"};
  // Crashes if string is not valid UTF-8.
  static GVariantRef<kType> From(std::string_view value);
  // Fails if string is not valid UTF-8.
  static base::expected<GVariantRef<kType>, std::string> TryFrom(
      std::string_view value);
};

template <>
struct Mapping<const char*> {
  static constexpr Type kType{"s"};
  // Crashes if string is not valid UTF-8.
  static GVariantRef<kType> From(const char* value);
  // Fails if string is not valid UTF-8.
  static base::expected<GVariantRef<kType>, std::string> TryFrom(
      const char* value);
};

// Containers

template <typename T>
struct Mapping<std::optional<T>> {
  static constexpr Type kInnerType = Mapping<T>::kType;
  static constexpr Type kType{"m", kInnerType};

  static GVariantRef<kType> From(const std::optional<T>& value)
    requires(kInnerType.IsDefinite() &&
             requires(T v) { GVariantRef<kInnerType>::From(v); })
  {
    std::optional<GVariantRef<kInnerType>> variant;
    GVariant* child = nullptr;
    if (value) {
      variant = GVariantRef<kInnerType>::From(*value);
      child = variant->raw();
    }
    return GVariantRef<kType>::TakeUnchecked(
        g_variant_new_maybe(kInnerType.gvariant_type(), child));
  }

  static base::expected<GVariantRef<kType>, std::string> TryFrom(
      const std::optional<T>& value)
    requires(requires(T v) { GVariantRef<kInnerType>::TryFrom(v); })
  {
    if (value.has_value()) {
      auto result = GVariantRef<kInnerType>::TryFrom(*value);
      if (!result.has_value()) {
        return base::unexpected(std::move(result).error());
      }
      return base::ok(GVariantRef<kType>::From(FilledMaybe{result.value()}));
    } else if constexpr (kInnerType.IsDefinite()) {
      return base::ok(
          GVariantRef<kType>::From(std::optional<GVariantRef<kInnerType>>()));
    } else {
      return base::unexpected(
          "Can't convert indefinite optional with no value.");
    }
  }

  static std::optional<T> Into(const GVariantRef<kType>& variant)
    requires(requires(GVariantRef<kInnerType> v) { v.template Into<T>(); })
  {
    GVariant* child = g_variant_get_maybe(variant.raw());
    if (child) {
      return GVariantRef<kInnerType>::TakeUnchecked(child).template Into<T>();
    } else {
      return std::nullopt;
    }
  }

  static base::expected<std::optional<T>, std::string> TryInto(
      const GVariantRef<kType>& variant)
    requires(requires(GVariantRef<kInnerType> v) { v.template TryInto<T>(); })
  {
    auto optional =
        variant.template Into<std::optional<GVariantRef<kInnerType>>>();

    if (optional.has_value()) {
      return optional->template TryInto<T>();
    } else {
      return base::ok(std::nullopt);
    }
  }
};

namespace internal {
template <Type kType, Type kInnerType, typename R>
GVariantRef<kType> FromRange(const R& value)
  requires(kInnerType.IsDefinite())
{
  GVariantBuilder builder;
  g_variant_builder_init(&builder, kType.gvariant_type());
  for (const auto& item : value) {
    g_variant_builder_add_value(&builder,
                                GVariantRef<kInnerType>::From(item).raw());
  }
  return GVariantRef<kType>::TakeUnchecked(g_variant_builder_end(&builder));
}

template <Type kType, Type kInnerType, typename R>
static base::expected<GVariantRef<kType>, std::string> TryFromRange(
    const R& value) {
  if (!kInnerType.IsDefinite() && value.empty()) {
    return base::unexpected("Can't convert empty indefinite array");
  }

  std::optional<Type<>> inner_type;
  GVariantBuilder builder;
  g_variant_builder_init(&builder, kType.gvariant_type());
  for (const auto& item : value) {
    auto converted = GVariantRef<kInnerType>::TryFrom(item);
    if (!converted.has_value()) {
      g_variant_builder_clear(&builder);
      return base::unexpected(std::move(converted).error());
    }
    if constexpr (!kInnerType.IsDefinite()) {
      if (!inner_type.has_value()) {
        inner_type = converted->GetType();
      } else if (!converted->GetType().IsSubtypeOf(inner_type.value())) {
        g_variant_builder_clear(&builder);
        return base::unexpected("Mismatched types in array");
      }
    }
    g_variant_builder_add_value(&builder, converted->raw());
  }
  return base::ok(
      GVariantRef<kType>::TakeUnchecked(g_variant_builder_end(&builder)));
}
}  // namespace internal

// If needed, a further specialization could be added for vectors and
// contiguous ranges of fixed basic types (bools, bytes, ints, and doubles)
// to use g_variant_{new,get}_fixed_array() rather than processing each
// element individually. This would make handling, e.g., large blobs of binary
// data much more efficient.

template <typename T>
struct Mapping<std::vector<T>> {
  static constexpr Type kInnerType = Mapping<T>::kType;
  static constexpr Type kType{"a", kInnerType};

  static GVariantRef<kType> From(const std::vector<T>& value)
    requires(kInnerType.IsDefinite() &&
             requires(T v) { GVariantRef<kInnerType>::From(v); })
  {
    return internal::FromRange<kType, kInnerType>(value);
  }

  static base::expected<GVariantRef<kType>, std::string> TryFrom(
      const std::vector<T>& value)
    requires(requires(T v) { GVariantRef<kInnerType>::TryFrom(v); })
  {
    return internal::TryFromRange<kType, kInnerType>(value);
  }

  static std::vector<T> Into(const GVariantRef<kType>& variant)
    requires(requires(GVariantRef<kInnerType> v) { v.template Into<T>(); })
  {
    std::vector<T> result;
    GVariantIter iter;
    g_variant_iter_init(&iter, variant.raw());
    result.reserve(g_variant_iter_n_children(&iter));
    while (GVariant* next = g_variant_iter_next_value(&iter)) {
      auto item_gvariant = GVariantRef<kInnerType>::TakeUnchecked(next);
      result.push_back(item_gvariant.template Into<T>());
    }
    return result;
  }

  static base::expected<std::vector<T>, std::string> TryInto(
      const GVariantRef<kType>& variant)
    requires(requires(GVariantRef<kInnerType> v) { v.template TryInto<T>(); })
  {
    std::vector<T> result;
    GVariantIter iter;
    g_variant_iter_init(&iter, variant.raw());
    result.reserve(g_variant_iter_n_children(&iter));
    while (GVariant* next = g_variant_iter_next_value(&iter)) {
      auto item_gvariant = GVariantRef<>::Take(next);
      auto item_result = item_gvariant.TryInto<T>();
      if (item_result.has_value()) {
        result.push_back(std::move(item_result).value());
      } else {
        return base::unexpected(std::move(item_result).error());
      }
    }
    return result;
  }
};

template <typename K, typename T>
  requires(Mapping<K>::kType.IsBasic())
struct Mapping<std::map<K, T>> {
  static constexpr Type kKeyType = Mapping<K>::kType;
  static constexpr Type kValueType = Mapping<T>::kType;
  static constexpr Type kInnerType{"{", kKeyType, kValueType, "}"};
  static constexpr Type kType{"a", kInnerType};

  static GVariantRef<kType> From(const std::map<K, T>& value)
    requires(kInnerType.IsDefinite() &&
             requires(std::pair<K, T> v) { GVariantRef<kInnerType>::From(v); })
  {
    return internal::FromRange<kType, kInnerType>(value);
  }

  static base::expected<GVariantRef<kType>, std::string> TryFrom(
      const std::map<K, T>& value)
    requires(requires(std::pair<K, T> v) {
      GVariantRef<kInnerType>::TryFrom(v);
    })
  {
    return internal::TryFromRange<kType, kInnerType>(value);
  }

  static std::map<K, T> Into(const GVariantRef<kType>& variant)
    requires(requires(GVariantRef<kInnerType> v) {
      v.template Into<std::pair<K, T>>();
    })
  {
    std::map<K, T> result;
    GVariantIter iter;
    g_variant_iter_init(&iter, variant.raw());
    while (GVariant* next = g_variant_iter_next_value(&iter)) {
      auto item_gvariant = GVariantRef<kInnerType>::TakeUnchecked(next);
      result.insert(item_gvariant.template Into<std::pair<K, T>>());
    }
    return result;
  }

  static base::expected<std::map<K, T>, std::string> TryInto(
      const GVariantRef<kType>& variant)
    requires(requires(GVariantRef<kInnerType> v) {
      v.template TryInto<std::pair<K, T>>();
    })
  {
    std::map<K, T> result;
    GVariantIter iter;
    g_variant_iter_init(&iter, variant.raw());
    while (GVariant* next = g_variant_iter_next_value(&iter)) {
      auto item_gvariant = GVariantRef<>::Take(next);
      auto item_result = item_gvariant.TryInto<std::pair<K, T>>();
      if (item_result.has_value()) {
        result.insert(std::move(item_result).value());
      } else {
        return base::unexpected(std::move(item_result).error());
      }
    }
    return result;
  }
};

template <typename K, typename T>
  requires(Mapping<K>::kType.IsBasic())
struct Mapping<std::pair<K, T>> {
  static constexpr Type kKeyType = Mapping<K>::kType;
  static constexpr Type kValueType = Mapping<T>::kType;
  static constexpr Type kType{"{", kKeyType, kValueType, "}"};

  static GVariantRef<kType> From(const std::pair<K, T>& pair)
    requires(requires(K k, T v) {
      GVariantRef<kKeyType>::From(k);
      GVariantRef<kValueType>::From(v);
    })
  {
    auto key = GVariantRef<kKeyType>::From(pair.first);
    auto value = GVariantRef<kValueType>::From(pair.second);
    return GVariantRef<kType>::TakeUnchecked(
        g_variant_new_dict_entry(key.raw(), value.raw()));
  }

  static base::expected<GVariantRef<kType>, std::string> TryFrom(
      const std::pair<K, T>& pair)
    requires(requires(K k, T v) {
      GVariantRef<kKeyType>::TryFrom(k);
      GVariantRef<kValueType>::TryFrom(v);
    })
  {
    auto key = GVariantRef<kKeyType>::TryFrom(pair.first);
    if (!key.has_value()) {
      return base::unexpected(std::move(key).error());
    }
    auto value = GVariantRef<kValueType>::TryFrom(pair.second);
    if (!value.has_value()) {
      return base::unexpected(std::move(value).error());
    }
    return GVariantRef<kType>::From(std::pair(key.value(), value.value()));
  }

  static std::pair<K, T> Into(const GVariantRef<kType>& variant)
    requires(requires(GVariantRef<kKeyType> k, GVariantRef<kValueType> v) {
      k.template Into<K>();
      v.template Into<T>();
    })
  {
    GVariantIter iter;
    g_variant_iter_init(&iter, variant.raw());
    auto key_gvariant =
        GVariantRef<kKeyType>::TakeUnchecked(g_variant_iter_next_value(&iter));
    auto value_gvariant = GVariantRef<kValueType>::TakeUnchecked(
        g_variant_iter_next_value(&iter));
    return std::pair(key_gvariant.template Into<K>(),
                     value_gvariant.template Into<T>());
  }

  static base::expected<std::pair<K, T>, std::string> TryInto(
      const GVariantRef<kType>& variant)
    requires(requires(GVariantRef<kKeyType> k, GVariantRef<kValueType> v) {
      k.template TryInto<K>();
      v.template TryInto<T>();
    })
  {
    auto gvariants = variant.template Into<
        std::pair<GVariantRef<kKeyType>, GVariantRef<kValueType>>>();
    auto key_result = gvariants.first.template TryInto<K>();
    if (!key_result.has_value()) {
      return base::unexpected(std::move(key_result).error());
    }
    auto value_result = gvariants.second.template TryInto<T>();
    if (!value_result.has_value()) {
      return base::unexpected(std::move(value_result).error());
    }
    return std::pair(std::move(key_result).value(),
                     std::move(value_result).value());
  }
};

template <typename R>
// If the type can decay, let that happen first to avoid ambiguities.
// E.g., a std::vector<double>& could either be used as a range directly or
// decayed into a std::vector<double>. Resolving in favor of decay is
// desirable so that string constants decay into a const char* and are treated
// as a C string rather than a range of chars.
  requires(std::ranges::range<R> && std::same_as<R, std::decay_t<R>>)
struct Mapping<R> {
  static constexpr Type kInnerType =
      Mapping<std::ranges::range_value_t<R>>::kType;
  static constexpr Type kType{"a", kInnerType};

  static GVariantRef<kType> From(const R& value)
    requires(kInnerType.IsDefinite() &&
             requires(std::ranges::range_value_t<R> v) {
               GVariantRef<kInnerType>::From(v);
             })
  {
    return internal::FromRange<kType, kInnerType>(value);
  }

  static base::expected<GVariantRef<kType>, std::string> TryFrom(const R& value)
    requires(requires(std::ranges::range_value_t<R> v) {
      GVariantRef<kInnerType>::TryFrom(v);
    })
  {
    return internal::TryFromRange<kType, kInnerType>(value);
  }
};

template <typename... Types>
struct Mapping<std::tuple<Types...>> {
  static constexpr Type kType{"(", Mapping<Types>::kType..., ")"};

  static GVariantRef<kType> From(const std::tuple<Types...>& value)
    requires(requires(Types... v) {
      (GVariantRef<Mapping<Types>::kType>::From(v), ...);
    })
  {
    GVariantBuilder builder;
    g_variant_builder_init(&builder, kType.gvariant_type());

    std::apply(
        [&](const Types&... values) {
          (g_variant_builder_add_value(
               &builder,
               GVariantRef<Mapping<Types>::kType>::From(values).raw()),
           ...);
        },
        value);

    return GVariantRef<kType>::TakeUnchecked(g_variant_builder_end(&builder));
  }

  static base::expected<GVariantRef<kType>, std::string> TryFrom(
      const std::tuple<Types...>& value)
    requires(requires(Types... v) {
      (GVariantRef<Mapping<Types>::kType>::TryFrom(v), ...);
    })
  {
    auto conversion_result = std::apply(
        [](const Types&... item) { return TupleTryFrom<Types...>(item...); },
        value);
    if (!conversion_result.has_value()) {
      return base::unexpected(std::move(conversion_result).error());
    }
    return GVariantRef<kType>::From(conversion_result.value());
  }

  static std::tuple<Types...> Into(const GVariantRef<kType>& variant)
    requires(requires(GVariantRef<Mapping<Types>::kType>... v) {
      (v.template Into<Types>(), ...);
    })
  {
    GVariantIter iter;
    g_variant_iter_init(&iter, variant.raw());
    // Must use uniform-initialization syntax since (in contrast to
    // function-call syntax) it is specified to evalulate values in order.
    auto gvariant_items =
        std::tuple{GVariantRef<Mapping<Types>::kType>::TakeUnchecked(
            g_variant_iter_next_value(&iter))...};
    return std::apply(
        [](const GVariantRef<Mapping<Types>::kType>&... items) {
          return std::tuple(items.template Into<Types>()...);
        },
        gvariant_items);
  }

  static base::expected<std::tuple<Types...>, std::string> TryInto(
      const GVariantRef<kType>& variant)
    requires(requires(GVariantRef<Mapping<Types>::kType>... v) {
      (v.template TryInto<Types>(), ...);
    })
  {
    auto gvariant_items =
        variant
            .template Into<std::tuple<GVariantRef<Mapping<Types>::kType>...>>();
    return std::apply(
        [](const GVariantRef<Mapping<Types>::kType>&... items) {
          return TupleTryInto<Types...>(items...);
        },
        gvariant_items);
  }

 private:
  // Attempt to turn a tuple of Ts into a tuple of GVariantRefs
  template <typename T = void>
  static base::expected<std::tuple<>, std::string> TupleTryFrom() {
    return base::ok(std::tuple());
  }

  template <typename T, typename... Ts>
  static base::expected<std::tuple<GVariantRef<Mapping<T>::kType>,
                                   GVariantRef<Mapping<Ts>::kType>...>,
                        std::string>
  TupleTryFrom(const T& first, const Ts&... rest) {
    auto first_result = GVariantRef<Mapping<T>::kType>::TryFrom(first);
    if (!first_result.has_value()) {
      return base::unexpected(std::move(first_result).error());
    }
    auto rest_result = TupleTryFrom<Ts...>(rest...);
    if (!rest_result.has_value()) {
      return base::unexpected(std::move(first_result).error());
    }
    return std::tuple_cat(std::tuple(first_result.value()),
                          rest_result.value());
  }

  // Attempt to turn a tuple of GVariantRefs into a tuple of Ts
  template <typename T = void>
  static base::expected<std::tuple<>, std::string> TupleTryInto() {
    return base::ok(std::tuple());
  }

  template <typename T, typename... Ts>
  static base::expected<std::tuple<T, Ts...>, std::string> TupleTryInto(
      const GVariantRef<Mapping<T>::kType>& first,
      const GVariantRef<Mapping<Ts>::kType>&... rest) {
    auto first_result = first.template TryInto<T>();
    if (!first_result.has_value()) {
      return base::unexpected(std::move(first_result).error());
    }
    auto rest_result = TupleTryInto<Ts...>(rest...);
    if (!rest_result.has_value()) {
      return base::unexpected(std::move(first_result).error());
    }
    return std::tuple_cat(std::tuple(first_result.value()),
                          rest_result.value());
  }
};

template <typename... Types>
  requires(sizeof...(Types) > 0)
struct Mapping<std::variant<Types...>> {
  static constexpr Type kType =
      TypeBase::CommonSuperTypeOf<Mapping<Types>::kType...>();

  static GVariantRef<kType> From(const std::variant<Types...>& value)
    requires(requires(Types... v) { (GVariantRef<kType>::From(v), ...); })
  {
    return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
      std::optional<GVariantRef<kType>> result;
      ((std::ignore =
            value.index() == Is &&
            (result.emplace(GVariantRef<kType>::From(std::get<Is>(value))),
             false)),
       ...);
      return std::move(*result);
    }(std::index_sequence_for<Types...>());
  }

  static base::expected<GVariantRef<kType>, std::string> TryFrom(
      const std::variant<Types...>& value)
    requires(requires(Types... v) { (GVariantRef<kType>::TryFrom(v), ...); })
  {
    return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
      std::optional<base::expected<GVariantRef<kType>, std::string>> result;
      ((std::ignore =
            value.index() == Is &&
            (result.emplace(GVariantRef<kType>::TryFrom(std::get<Is>(value))),
             false)),
       ...);
      return std::move(*result);
    }(std::index_sequence_for<Types...>());
  }

  template <Type C>
  static std::variant<Types...> Into(const GVariantRef<C>& variant)
      // Infallible Into is provided if at least one alternative provides an
      // infallible Into for the provided type.
    requires(... || requires { variant.template Into<Types>(); })
  {
    // Try TryInto() first so where possible Into and TryInto produce the same
    // variant.
    auto result = VariantTryInto<0, Types...>(variant);
    if (result.has_value()) {
      return std::move(result).value();
    }

    // To get here, one of the types must provide both a fallible and infallible
    // conversion, and the fallible version failed. Loop through again and call
    // the infallible version to get a value to return.
    return VariantInto<C, 0, Types...>(variant);
  }

  static base::expected<std::variant<Types...>, std::string> TryInto(
      const GVariantRef<kType>& variant)
      // TryInto is only provided if it is provided by at least one alternative.
    requires(... || requires { variant.template TryInto<Types>(); })
  {
    return VariantTryInto<0, Types...>(variant);
  }

 private:
  template <std::size_t I>
  static base::expected<std::variant<Types...>, std::string> VariantTryInto(
      const GVariantRef<kType>& variant) {
    return base::unexpected("No variant alternative could decode value");
  }

  template <std::size_t I, typename T, typename... Ts>
  static base::expected<std::variant<Types...>, std::string> VariantTryInto(
      const GVariantRef<kType>& variant) {
    if constexpr (requires { variant.template TryInto<T>(); }) {
      auto alternative_result = variant.template TryInto<T>();
      if (alternative_result.has_value()) {
        return base::ok(std::variant<Types...>(
            std::in_place_index<I>, std::move(alternative_result).value()));
      }
    }
    return VariantTryInto<I + 1, Ts...>(variant);
  }

  // No base case needed, as at least one type is guaranteed to provide an
  // infallible conversion.
  template <Type C, std::size_t I, typename T, typename... Ts>
  static std::variant<Types...> VariantInto(const GVariantRef<C>& variant) {
    if constexpr (requires { variant.template Into<T>(); }) {
      return std::variant<Types...>(std::in_place_index<I>,
                                    variant.template Into<T>());
    } else {
      return VariantInto<C, I + 1, Ts...>(variant);
    }
  }
};

// Special Types

template <Type C>
struct Mapping<GVariantRef<C>> {
  static constexpr Type kType = C;
  static GVariantRef<kType> From(GVariantRef<kType> value) { return value; }
  static GVariantRef<kType> Into(GVariantRef<kType> value) { return value; }
};

template <>
struct Mapping<Ignored> {
  static constexpr Type kType{"*"};
  static Ignored Into(const GVariantRef<kType>& variant);
};

template <>
struct Mapping<decltype(std::ignore)> {
  static constexpr Type kType{"*"};
  static decltype(std::ignore) Into(const GVariantRef<kType>& variant);
};

template <typename T>
struct Mapping<Boxed<T>> {
  static constexpr Type kType{"v"};

  static GVariantRef<kType> From(const Boxed<T>& value)
    requires(requires(T v) { GVariantRef<>::From(v); })
  {
    return GVariantRef<kType>::TakeUnchecked(
        g_variant_new_variant(GVariantRef<>::From(value.value).raw()));
  }

  static base::expected<GVariantRef<kType>, std::string> TryFrom(
      const Boxed<T>& value)
    requires(requires(T v) { GVariantRef<>::TryFrom(v); })
  {
    auto result = GVariantRef<>::TryFrom(value.value);
    if (!result.has_value()) {
      return base::unexpected(std::move(result).error());
    }
    return base::ok(GVariantRef<kType>::From(Boxed{result.value()}));
  }

  static Boxed<T> Into(const GVariantRef<kType>& variant)
    requires(requires(GVariantRef<> v) { v.Into<T>(); })
  {
    return Boxed{
        GVariantRef<>::Take(g_variant_get_variant(variant.raw())).Into<T>()};
  }

  static base::expected<Boxed<T>, std::string> TryInto(
      const GVariantRef<kType>& variant)
    requires(requires(GVariantRef<> v) { v.TryInto<T>(); })
  {
    return variant.Into<Boxed<GVariantRef<>>>().value.TryInto<T>().transform(
        [](auto v) { return Boxed{std::move(v)}; });
  }
};

template <typename T>
struct Mapping<FilledMaybe<T>> {
  static constexpr Type kInnerType = Mapping<T>::kType;
  static constexpr Type kType{"m", kInnerType};

  static GVariantRef<kType> From(const FilledMaybe<T>& value)
    requires(requires(T v) { GVariantRef<kInnerType>::From(v); })
  {
    return GVariantRef<kType>::TakeUnchecked(g_variant_new_maybe(
        nullptr, GVariantRef<kInnerType>::From(value.value).raw()));
  }

  static base::expected<GVariantRef<kType>, std::string> TryFrom(
      const FilledMaybe<T>& value)
    requires(requires(T v) { GVariantRef<kInnerType>::TryFrom(v); })
  {
    auto result = GVariantRef<kInnerType>::TryFrom(value.value);
    if (!result.has_value()) {
      return base::unexpected(std::move(result).error());
    }
    return base::ok(GVariantRef<kType>::From(FilledMaybe{result.value()}));
  }

  static base::expected<FilledMaybe<T>, std::string> TryInto(
      const GVariantRef<kType>& variant)
    requires(requires(GVariantRef<kInnerType> v) { v.template TryInto<T>(); })
  {
    GVariant* contents = g_variant_get_maybe(variant.raw());
    if (!contents) {
      return base::unexpected("Maybe value unexpectedly empty");
    }

    return GVariantRef<kInnerType>::TakeUnchecked(contents)
        .template TryInto<T>()
        .transform([](auto v) { return FilledMaybe{std::move(v)}; });
  }
};

template <Type C>
struct Mapping<EmptyArrayOf<C>> {
  static constexpr Type kType{"a", C};

  static GVariantRef<kType> From(const EmptyArrayOf<C>& value)
    requires(C.IsDefinite())
  {
    return GVariantRef<kType>::TakeUnchecked(
        g_variant_new_array(C.gvariant_type(), nullptr, 0));
  }

  static base::expected<EmptyArrayOf<C>, std::string> TryInto(
      const GVariantRef<kType>& variant) {
    if (auto size = g_variant_n_children(variant.raw()); size != 0) {
      return base::unexpected("Array unexpectedly not empty.");
    }

    return EmptyArrayOf<C>{};
  }
};

template <>
struct Mapping<ObjectPath> {
  static constexpr Type kType{"o"};
  static GVariantRef<kType> From(const ObjectPath& value);
  static ObjectPath Into(const GVariantRef<kType>& variant);
};

template <>
struct Mapping<TypeSignature> {
  static constexpr Type kType{"g"};
  static GVariantRef<kType> From(const TypeSignature& value);
  static TypeSignature Into(const GVariantRef<kType>& variant);
};

}  // namespace gvariant

using gvariant::GVariantRef;

}  // namespace remoting

// Make tuple-like
template <remoting::gvariant::Type C>
  requires(C.IsFixedSizeContainer())
struct std::tuple_size<remoting::GVariantRef<C>>
    : public std::tuple_size<
          decltype(remoting::gvariant::TypeBase::Unpack<C>())> {};

template <std::size_t I, remoting::gvariant::Type C>
  requires(C.IsFixedSizeContainer())
struct std::tuple_element<I, remoting::GVariantRef<C>> {
  using type = remoting::GVariantRef<std::get<I>(
      remoting::gvariant::TypeBase::Unpack<C>())>;
};

#endif  // REMOTING_HOST_LINUX_GVARIANT_REF_H_
