// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of serialization and deserialization for common types, and
// extension points for supporting serialization for additional types.

// Common serialization formats in Chrome:
//  - base::Pickle is fast, produces compact output, but has zero
//  interoperability. No intrinsic support for forwards/backwards compatibility.
//  Corruption that affects size fields will generally be detected and safely
//  rejected. Other kinds of corruption will result in a valid instance of the
//  data type with corrupted data. There are two systems to extend base::Pickle
//  to handle new types: this one, and IPC::ParamTraits.
//  - Mojo provides an undiscoverable serialization facility for Mojo types.
//  This has similar performance and interoperability characteristics to
//  base::Pickle, but is safer and easier to use. However, code in //net cannot
//  depend on Mojo.
//  - protobuf: slow, compact output, good interoperability. Excellent support
//  for forwards/backwards compatibility. Very large impact on binary size due
//  to generated code. Corruption may be silently ignored, detected and
//  rejected, or give corrupted data.
//  - JSON (base::Value, etc.) is very slow, very large output, excellent
//  interoperability. Usually straightforward to implement forwards/backwards
//  compatibility. Corruption may be detected or produce corrupt data.
//  - SQLite is not actually a serialization format, but is often used for
//  persistence. It is moderately slow due to the need to round-trip through
//  SQL. Good tool availability. Good support for forwards/backwards
//  compatibility. Resistant to disk corruption, but needs a recovery path that
//  recreates an empty database.
//  - Structured headers should be used for HTTP headers.

#ifndef NET_BASE_PICKLE_TRAITS_H_
#define NET_BASE_PICKLE_TRAITS_H_

#include <stddef.h>

#include <concepts>
#include <optional>
#include <ranges>
#include <tuple>
#include <type_traits>
#include <utility>

#include "base/bits.h"
#include "base/containers/span.h"
#include "base/pickle.h"

namespace net {

// To make a type serializable by WriteToPickle and deserializable by
// ReadValueFromPickle, add a specialization of PickleTraits with a Serialize
// method that takes a base::Pickle and a value, and a Deserialize method that
// takes a base::PickleIterator and returns the deserialized value wrapped in
// std::optional, or std::nullopt if the input pickle was invalid.
//
// If the type to be deserialized can be completely reproduced by a call to the
// constructor, it is simple to use ReadValuesFromPickle for deserialization.
//
// For example, suppose your type is:
//
//   class MyHostPortPair {
//    public:
//     MyHostPortPair(std::string_view host, uint16_t port);
//
//     const std::string& host() const { return host_; }
//     uint16_t port() const { return port_; }
//
//    private:
//     std::string host_;
//     uint16_t port_;
//   };
//
// Then you can make it serializable with:
//
//   template <>
//   struct PickleTraits<MyHostPortPair> {
//     static void Serialize(base::Pickle& pickle, const MyHostPortPair& value)
//     {
//       WriteToPickle(pickle, value.host(), value.port());
//     }
//
//     static std::optional<MyHostPortPair>
//     Deserialize(base::PickleIterator& iter) {
//       auto result = ReadValuesFromPickle<std::string, uint16_t>(iter);
//       if (!result) {
//         return std::nullopt;
//       }
//
//       // Perform any additional validation of `host` and `port`and return
//       // std::nullopt if invalid.
//       if (host.empty()) {
//         return std::nullopt;
//       }
//
//       auto [host, port] = std::move(result).value();
//       return MyHostPortPair(host, port);
//     }
//   };
//
// If the state of your type cannot be reconstructed by a constructor call, it
// is probably easier to use ReadPickleInto. For example, suppose your class
// looks like this:
//
//   class MyHeaders {
//    public:
//     MyHeaders();
//
//     void Add(std::string_view name, std::string_view value);
//
//    private:
//     std::vector<std::pair<std::string, std::string>> headers_;
//   };
//
// In the private section, add:
//
//     friend struct PickleTraits<MyHeaders>;
//
// Then you can make it serializable with:
//
//   template <>
//   struct PickleTraits<MyHeaders> {
//     static void Serialize(base::Pickle& pickle, const MyHeaders& value) {
//       WriteToPickle(pickle, value.headers_);
//     }
//
//     static std::optional<MyHeaders> Deserialize(base::PickleIterator& iter) {
//       MyHeaders headers;
//       if (!ReadPickleInto(iter, headers.headers_)) {
//         return std::nullopt;
//       }
//       // Perform any additional validation of `headers_` and return
//       // std::nullopt if invalid.
//       if (std::ranges::any_of(
//               headers.headers_,
//               [](std::string_view name) { return name.empty(); },
//               &std::pair<std::string, std::string>::first)) {
//         return std::nullopt;
//       }
//
//       return headers;
//     }
//
//     static size_t PickleSize(const MyHeaders& value) {
//       return EstimatePickleSize(value.headers_);
//     }
//   };
//
// Providing an implementation of PickleSize() is optional, but will permit the
// right amount of memory to be allocated for the Pickle in advance. It is
// particularly useful for types that will be placed in containers.
//
// There's no need to provide a specialization for "const T" as the const will
// always be removed before looking up a PickleTraits specialization.
//
// Simple structs containing only types for which kPickleAsBytes is true and
// which have no padding bytes can be serialized by copying the underlying
// bytes. This is very fast, particularly when stored in a vector. Beware that
// this may give different results from serializing the members individually, so
// you have to make the choice before serializing anything in production. Also,
// there is no way to verify that the result verifies the constraints for the
// type, so it is only suitable for plain old data.
//
// Suppose you have a struct like this:
//
//   struct MyHttpVersion {
//     uint16_t major;
//     uint16_t minor;
//   };
//
// Then you can make it serializable with:
//
//   template <>
//   constexpr bool kPickleAsBytes<MyHttpVersion> = true;
//
// The declarations to serialize a type should always appear in the same header
// file as the type itself, to ensure that it is always serialized and
// deserialized consistently.

//

// Main implementation of serialization and deserialization, and customization
// point for types to add their own serialization. Types are not serializable by
// default.
template <typename T>
struct PickleTraits {};

// This is automatically set to true for built-in integer types. It can be
// specialized to true for structs of integer types. Some mistaken usage can be
// caught by the compiler, for example using on a struct with padding. But other
// kinds, like using on a struct containing pointers will not be automatically
// caught, so use with care.
template <typename T>
constexpr inline bool kPickleAsBytes = false;

// This is useful in implementations of PickleTraits::PickleSize().
// For many types, the size is known at compile time, so use constexpr to help
// the compiler optimize those cases.
template <typename T>
constexpr size_t EstimatePickleSize(const T& value);

// Multiple-value version for predicting the size of WriteToPickle().
template <typename... Args>
  requires(sizeof...(Args) > 1u)
constexpr size_t EstimatePickleSize(const Args&... args);

namespace internal {

// CanSerialize and CanDeserialize are used to determine whether a type can be
// serialized or deserialized. They work by literally checking whether the
// PickleTraits<T>::Serialize and PickleTraits<T>::Deserialize methods are
// callable. As a result, they will give the wrong answer if you try to use them
// on the type you're currently defining PickleTraits for.
template <typename T>
concept CanSerialize = requires(base::Pickle& pickle, const T& value) {
  PickleTraits<std::remove_const_t<T>>::Serialize(pickle, value);
};
template <typename T>
concept CanDeserialize = requires(base::PickleIterator& iter) {
  PickleTraits<std::remove_const_t<T>>::Deserialize(iter);
};

template <typename T>
concept CanSerializeDeserialize = CanSerialize<T> && CanDeserialize<T>;

// These types can be implicitly converted to char and back without loss of
// precision. This permits highly efficient deserialization of contiguous
// containers of these types.
template <typename T>
concept IsCharLike = std::same_as<T, char> || std::same_as<T, uint8_t> ||
                     std::same_as<T, int8_t>;

// A shorthand for the type a range contains.
template <typename T>
using ValueType = std::ranges::range_value_t<T>;

// True for std::string, std::vector<uint8_t>, etc.
template <typename T>
concept IsConstructableFromCharLikeIteratorPair =
    IsCharLike<ValueType<T>> &&
    std::constructible_from<T, const char*, const char*>;

// True for std::u16string, std::vector<int>, etc.
template <typename T>
concept CanResizeAndCopyFromBytes =
    std::ranges::contiguous_range<T> && kPickleAsBytes<ValueType<T>> &&
    std::default_initializable<T> &&
    requires(T t, size_t size, base::span<const uint8_t> byte_span) {
      t.resize(size);
      base::as_writable_byte_span(t).copy_from(byte_span);
    };

// True for std::vector and similar containers.
template <typename T>
concept IsReserveAndPushBackable =
    std::default_initializable<T> &&
    requires(T t, size_t size, const ValueType<T>& value) {
      t.reserve(size);
      t.push_back(value);
    };

// True for std::list, std::map, std::unordered_set, base::flat_set, etc.
template <typename T>
concept IsInsertAtEndable =
    std::default_initializable<T> &&
    requires(T t, const ValueType<T>& value) { t.insert(t.end(), value); };

// We only consider a range serializable if we know a way to deserialize it.
template <typename T>
concept IsSerializableRange =
    std::ranges::sized_range<T> && CanSerializeDeserialize<ValueType<T>> &&
    (IsConstructableFromCharLikeIteratorPair<T> ||
     CanResizeAndCopyFromBytes<T> || IsReserveAndPushBackable<T> ||
     IsInsertAtEndable<T>);

// True for std::tuple, std::pair and std::array.
template <typename T>
constexpr inline bool kIsTupleLike = false;

// std::tuple_size_v<T> does not work here. Clang refuses to ignore the error
// for non-tuple types.
template <typename T>
  requires std::same_as<decltype(std::tuple_size<T>::value), const size_t>
constexpr inline bool kIsTupleLike<T> = true;

// CanSerializeDeserializeTuple is implemented using a consteval function to
// make convenient use of std::index_sequence.
template <typename TupleLike, size_t... I>
  requires(kIsTupleLike<TupleLike> &&
           std::tuple_size_v<TupleLike> == sizeof...(I))
consteval bool CanSerializeDeserializeTupleImpl(std::index_sequence<I...>) {
  return (CanSerializeDeserialize<std::tuple_element_t<I, TupleLike>> && ...);
}

template <typename TupleLike>
  requires kIsTupleLike<TupleLike>
consteval bool CanSerializeDeserializeTupleImpl() {
  return CanSerializeDeserializeTupleImpl<TupleLike>(
      std::make_index_sequence<std::tuple_size_v<TupleLike>>());
}

// A tuple-like type is serializable if all of its elements are serializable.
template <typename TupleLike>
concept CanSerializeDeserializeTuple =
    CanSerializeDeserializeTupleImpl<TupleLike>();

// Convenient shortcut when implementing PickleTraits::PickleSize().
// base::Pickle aligns everything to 32-bit boundaries, so we need to round up
// to a multiple of 4 when calculating how big something will be. See
// base::Pickle::ClaimUninitializedBytesInternal().
constexpr size_t RoundUp(size_t size) {
  return base::bits::AlignUp(size, sizeof(uint32_t));
}

}  // namespace internal

template <typename T>
constexpr size_t EstimatePickleSize(const T& value) {
  if constexpr (requires {
                  {
                    PickleTraits<T>::PickleSize(value)
                  } -> std::same_as<size_t>;
                }) {
    return PickleTraits<T>::PickleSize(value);
  } else {
    return internal::RoundUp(1);  // Everything is padded to at least 4 bytes.
  }
}

template <typename... Args>
  requires(sizeof...(Args) > 1u)
constexpr size_t EstimatePickleSize(const Args&... args) {
  return (EstimatePickleSize(args) + ...);
}

// These are defined in //net/base/pickle.h, but also declared here so that we
// can reference them in non-dependant contexts.
template <typename... Args>
  requires(internal::CanSerialize<Args> && ...)
void WriteToPickle(base::Pickle& pickle, const Args&... args);

template <typename T>
  requires(internal::CanDeserialize<T>)
std::optional<T> ReadValueFromPickle(base::PickleIterator& iter);

// Built-in non-pointer types can be copied if they have unique representations.
// has_unique_object_representations_v is true for bool and enums but it is not
// safe to deserialize them by copying so specifically exclude them. Exclude
// pointers since we shouldn't ever serialize them. Can be specialized to true
// for structs that contain only types for which kCopyAsBytes is true and which
// have no padding bytes.
template <typename T>
  requires(std::has_unique_object_representations_v<T> &&
           std::is_trivially_destructible_v<T> &&
           std::is_trivially_default_constructible_v<T> &&
           !std::is_aggregate_v<T> && !std::is_pointer_v<T> &&
           !std::is_member_pointer_v<T> && !std::is_enum_v<T> &&
           !std::same_as<T, bool>)
constexpr inline bool kPickleAsBytes<T> = true;

// Implementation of PickleTraits for types for which kPickleAsBytes is true,
// including all built-in integer types.
template <typename T>
  requires(kPickleAsBytes<T> && !std::is_const_v<T>)
struct PickleTraits<T> {
  static void Serialize(base::Pickle& pickle, const T& value) {
    // These are intentionally static_asserts rather than requirements to avoid
    // hiding cases where kPickleAsBytes is set incorrectly.
    static_assert(std::has_unique_object_representations_v<T>,
                  "do not set kPickleAsBytes for types where byte equality "
                  "isn't identical to object equality");
    static_assert(std::is_trivially_destructible_v<T>,
                  "do not set kPickleAsBytes for types that are not trivially "
                  "destructible");
    static_assert(std::is_trivially_default_constructible_v<T>,
                  "do not set kPickleAsBytes for types that are not trivially "
                  "default constructible");
    pickle.WriteBytes(base::byte_span_from_ref(value));
  }

  static std::optional<T> Deserialize(base::PickleIterator& iter) {
    static_assert(std::has_unique_object_representations_v<T>,
                  "do not set kPickleAsBytes for types where byte equality "
                  "isn't identical to object equality");
    static_assert(std::is_trivially_destructible_v<T>,
                  "do not set kPickleAsBytes for types that are not trivially "
                  "destructible");
    static_assert(std::is_trivially_default_constructible_v<T>,
                  "do not set kPickleAsBytes for types that are not trivially "
                  "default constructible");
    const char* data = nullptr;
    if (!iter.ReadBytes(&data, sizeof(T))) {
      return std::nullopt;
    }
    CHECK(data);
    T t;

    // SAFETY: The `data` pointer is guaranteed to point to at least `sizeof(T)`
    // bytes by the ReadBytes call above.
    base::byte_span_from_ref(t).copy_from(
        base::as_bytes(UNSAFE_BUFFERS(base::span(data, sizeof(T)))));
    return t;
  }

  static constexpr size_t PickleSize(const T& value) {
    return internal::RoundUp(sizeof(value));
  }
};

// Implementation of PickleTraits for standard containers and types that behave
// like them.
template <typename T>
  requires(internal::IsSerializableRange<T>)
struct PickleTraits<T> {
  using Value = internal::ValueType<T>;

  static void Serialize(base::Pickle& pickle, const T& value) {
    // Intentionally crash for containers that are too large to fit in an int.
    pickle.WriteInt(base::checked_cast<int>(value.size()));
    if constexpr (internal::CanResizeAndCopyFromBytes<T>) {
      // This handles string types and vectors of integers.
      pickle.WriteBytes(base::as_byte_span(value));
    } else {
      // This handles non-contiguous containers and values that need to be
      // written one at a time.
      for (const auto& v : value) {
        WriteToPickle(pickle, v);
      }
    }
  }

  static std::optional<T> Deserialize(base::PickleIterator& iter) {
    int size_as_int = 0;
    if (!iter.ReadInt(&size_as_int) || size_as_int < 0) {
      return std::nullopt;
    }
    // Every non-negative integer will fit in a size_t.
    const size_t size = static_cast<size_t>(size_as_int);
    if (size > iter.RemainingBytes()) {
      // Every item in the container must consume at least 1 byte, so this size
      // cannot possibly be correct.
      return std::nullopt;
    }
    if constexpr (internal::IsConstructableFromCharLikeIteratorPair<T>) {
      // Highly efficient path for std::string, std::vector<uint8_t>, etc.
      const char* data = nullptr;
      static_assert(sizeof(Value) == 1);
      if (!iter.ReadBytes(&data, size)) {
        return std::nullopt;
      }
      CHECK(data);
      // SAFETY: The `data` pointer is guaranteed to point to at least `size`
      // bytes by the ReadBytes call above.
      return T(data, UNSAFE_BUFFERS(data + size));
    } else if constexpr (internal::CanResizeAndCopyFromBytes<T>) {
      // Slightly less efficient path for std::u16string, std::vector<int>, etc.
      T t;
      t.resize(size);  // Pointlessly zero-fills the container, but avoids UB.
      const char* data = nullptr;
      const size_t size_in_bytes = size * sizeof(Value);
      if (!iter.ReadBytes(&data, size_in_bytes)) {
        return std::nullopt;
      }
      CHECK(data);
      // SAFETY: The `data` pointer is guaranteed to point to at least
      // `size_in_bytes` bytes by the ReadBytes call above.
      base::as_writable_byte_span(t).copy_from(
          base::as_bytes(UNSAFE_BUFFERS(base::span(data, size_in_bytes))));
      return t;
    } else if constexpr (internal::IsReserveAndPushBackable<T>) {
      // Slower path for vectors of types that have non-trivial deserialization
      // semantics. Also works for vector-like types like absl::InlinedVector.
      T t;
      t.reserve(size);
      for (size_t i = 0; i < size; ++i) {
        auto maybe_v = ReadValueFromPickle<Value>(iter);
        if (!maybe_v) {
          return std::nullopt;
        }
        t.push_back(std::move(maybe_v).value());
      }
      return t;
    } else {
      // std::list, std::map, std::unordered_set, base::flat_set, etc.
      static_assert(internal::IsInsertAtEndable<T>);
      T t;
      for (size_t i = 0; i < size; ++i) {
        auto maybe_v = ReadValueFromPickle<Value>(iter);
        if (!maybe_v) {
          return std::nullopt;
        }
        t.insert(t.end(), std::move(maybe_v).value());
      }
      return t;
    }
  }

  static constexpr size_t PickleSize(const T& value) {
    if constexpr (internal::CanResizeAndCopyFromBytes<T>) {
      return internal::RoundUp(value.size() * sizeof(Value)) + sizeof(int);
    } else {
      size_t size = sizeof(int);
      for (const auto& v : value) {
        // If the elements of the container are containers, each one may be a
        // different size. If not, the compiler should optimize this down to a
        // multiplication.
        size += EstimatePickleSize(v);
      }
      return size;
    }
  }
};

// Tuple-like types like std::tuple and std::pair.
template <typename T>
  requires internal::CanSerializeDeserializeTuple<T>
struct PickleTraits<T> {
  static void Serialize(base::Pickle& pickle, const T& value) {
    SerializeImpl(pickle, value, kIndexSequence);
  }

  static std::optional<T> Deserialize(base::PickleIterator& iter) {
    return DeserializeImpl(iter, kIndexSequence);
  }

  static constexpr size_t PickleSize(const T& value) {
    return PickleSizeImpl(value, kIndexSequence);
  }

 private:
  template <size_t I>
  using ElementType = std::tuple_element_t<I, T>;

  template <size_t... I>
  static void SerializeImpl(base::Pickle& pickle,
                            const T& value,
                            std::index_sequence<I...>) {
    (WriteToPickle(pickle, std::get<I>(value)), ...);
  }

  template <size_t... I>
  static std::optional<T> DeserializeImpl(base::PickleIterator& iter,
                                          std::index_sequence<I...>) {
    // This is tricky. We cannot expand the template pack directly into the
    // parameters of std::make_tuple() because parameter evaluation order is
    // different on Windows and Linux, and ReadValueFromPickle() has the
    // side-effect of advancing the iterator so order matters. However,
    // initializer elements are guaranteed to be evaluated in order, so we can
    // safely use initializer syntax.
    using TupleOfOptionals = std::tuple<std::optional<ElementType<I>>...>;
    TupleOfOptionals tuple_of_optionals = {
        ReadValueFromPickle<ElementType<I>>(iter)...};
    if (!(std::get<I>(tuple_of_optionals).has_value() && ...)) {
      return std::nullopt;
    }
    return T(std::move(std::get<I>(tuple_of_optionals)).value()...);
  }

  template <size_t... I>
  static constexpr size_t PickleSizeImpl(const T& value,
                                         std::index_sequence<I...>) {
    return (EstimatePickleSize(std::get<I>(value)) + ...);
  }

  static constexpr std::make_index_sequence<std::tuple_size_v<T>>
      kIndexSequence{};
};

// bool is treated specially by base::Pickle.
template <>
struct PickleTraits<bool> {
  static void Serialize(base::Pickle& pickle, bool value) {
    pickle.WriteBool(value);
  }

  static std::optional<bool> Deserialize(base::PickleIterator& iter) {
    bool b;
    if (!iter.ReadBool(&b)) {
      return std::nullopt;
    }
    return b;
  }

  static constexpr size_t PickleSize(bool value) {
    return internal::RoundUp(1);
  }
};

template <typename T>
  requires(internal::CanSerializeDeserialize<std::remove_const_t<T>>)
struct PickleTraits<std::optional<T>> {
  static void Serialize(base::Pickle& pickle, std::optional<T> value) {
    // Write as `uint8_t` to match Deserialize().
    WriteToPickle(pickle, static_cast<uint8_t>(value.has_value()));
    if (value.has_value()) {
      WriteToPickle(pickle, *value);
    }
  }

  static std::optional<std::optional<T>> Deserialize(
      base::PickleIterator& iter) {
    auto maybe_has_value = ReadValueFromPickle<uint8_t>(iter);
    if (!maybe_has_value) {
      return std::nullopt;
    }

    uint8_t has_value = maybe_has_value.value();
    // This is more strict than base::PickleIterator::ReadBool() as it is useful
    // to notice data corruption.
    if (has_value != 0 && has_value != 1) {
      return std::nullopt;
    }

    if (!has_value) {
      // Use the default constructor instead of std::nullopt to avoid confusion.
      return std::optional<T>();
    }

    return ReadValueFromPickle<T>(iter);
  }

  static constexpr size_t PickleSize(const std::optional<T>& value) {
    return EstimatePickleSize(value.has_value()) +
           (value.has_value() ? EstimatePickleSize(value.value()) : 0u);
  }
};

}  // namespace net

#endif  // NET_BASE_PICKLE_TRAITS_H_
