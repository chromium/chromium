// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Extensible serialization and deserialization functions for base::Pickle.
//
// This file provides a way to serialize and deserialize arbitrary types to and
// from a base::Pickle. The PickleTraits<T> class template is used to define
// serialization and deserialization for a type T.
//
// By default, all built-in integer types, bools, almost all std::, absl:: and
// base:: container types are supported, along with std::optional, tuple and
// pair types. Supported types may be nested to arbitrary depth, for example
// std::map<std::string, std::vector<int>>.
//
// To serialize a value of type T, call:
//
//   base::Pickle pickle;
//   net::WriteToPickle(pickle, value);
//
// To deserialize a value of type T, call:
//
//   auto pickle = base::Pickle::WithData(data);
//   std::optional<T> value = net::ReadValueFromPickle<T>(iter);
//
// When deserialization fails, the return value will be std::nullopt.
//
// Alternatively, when deserializing a class object, it may be more convenient
// to use ReadPickleInto(), eg.
//
//   auto pickle = base::Pickle::WithData(data);
//   if (!net::ReadPickleInto(pickle, instance.member1_, instance.member2_)) {
//     return std::nullopt;
//   }
//   return instance;
//
// See pickle_traits.h for how to define serialization and deserialization for
// your own types.
//
// Limitations:
//  - Trying to serialize a container with more than INT_MAX elements will
//    result in a CHECK failure. base::Pickle is probably not the right tool for
//    the job if you need to serialize more than 2G elements.
//  - Serializing size_t will give incompatible results on 32-bit and 64-bit
//    platforms. This is one reason why containers are serialized with a 32-bit
//    value for the length.
//
// Intentionally unsupported:
//  - pointer types
//
// Not currently supported:
//  - float, double, long double
//  - std::forward_list
//  - std::array
//  - std::variant
//  - enums

#ifndef NET_BASE_PICKLE_H_
#define NET_BASE_PICKLE_H_

#include <optional>
#include <tuple>

#include "base/pickle.h"
#include "net/base/pickle_traits.h"

namespace net {

// Serializes `args` to `pickle`.
template <typename... Args>
  requires(internal::CanSerialize<Args> && ...)
void WriteToPickle(base::Pickle& pickle, const Args&... args) {
  static_assert((!std::is_const_v<Args> && ...));
  pickle.Reserve(EstimatePickleSize(args...));
  // "," is used in place of ";" here so that we can use template pack
  // expansion.
  (PickleTraits<Args>::Serialize(pickle, args), ...);
}

// Deserializes a single value of type T from `iter`. Returns std::nullopt on
// failure.
template <typename T>
  requires(internal::CanDeserialize<T>)
std::optional<T> ReadValueFromPickle(base::PickleIterator& iter) {
  // Always remove const qualifier before attempting to deserialize.
  // Deserialization always creates a new value, and some deserializers will
  // choke on value they can't write to.
  return PickleTraits<std::remove_const_t<T>>::Deserialize(iter);
}

// Deserializes multiple values from `iter` and returns them as an optional
// tuple. Example usage:
//
//   auto pickle = base::Pickle::WithData(data);
//   auto maybe_value =
//       net::ReadValuesFromPickle<int, std::string>(iter);
//   if (!maybe_value) {
//     return std::nullopt;
//   }
//   auto [int_param, string_param] = std::move(maybe_value).value();
//   return MyType(int_param, string_param);
//
// Returns std::nullopt on failure.
template <typename... Args>
  requires(internal::CanDeserialize<Args> && ...)
std::optional<std::tuple<Args...>> ReadValuesFromPickle(
    base::PickleIterator& iter) {
  return ReadValueFromPickle<std::tuple<Args...>>(iter);
}

// Deserializes multiple values from `iter` and stores them in `args`. Returns
// false and doesn not modify `args` on failure.
template <typename... Args>
  requires(internal::CanDeserialize<Args> && ...)
[[nodiscard]] bool ReadPickleInto(base::PickleIterator& iter, Args&... args) {
  auto maybe_value = ReadValuesFromPickle<Args...>(iter);
  if (!maybe_value) {
    return false;
  }
  std::tie(args...) = std::move(maybe_value).value();
  return true;
}

namespace internal {

// Create a PickleIterator `iter` from `pickle` and call `f(iter, args)`. with
// it. If the input was completely consumed, return the result, otherwise
// return a value indicating failure (std::nullopt or false).
template <typename F, typename... Args>
std::invoke_result_t<F, base::PickleIterator&, Args&&...>
CallWithPickleIterator(const base::Pickle& pickle, F&& f, Args&&... args) {
  base::PickleIterator iter(pickle);
  auto result = std::forward<F>(f)(iter, std::forward<Args>(args)...);
  if (!iter.ReachedEnd()) {
    // This return statement will turn into std::nullopt or false as needed.
    return {};
  }
  return result;
}

}  // namespace internal

// Convenience version of ReadValueFromPickle that takes a base::Pickle
// instead of a base::PickleIterator. Expects the pickle to be completely
// consumed.
template <typename T>
  requires(internal::CanDeserialize<T>)
std::optional<T> ReadValueFromPickle(const base::Pickle& pickle) {
  // Need to specify the type to disambiguate the function pointer.
  using FunctionPointerType = std::optional<T> (*)(base::PickleIterator&);
  FunctionPointerType read_value_from_pickle = &ReadValueFromPickle<T>;
  return internal::CallWithPickleIterator(pickle, read_value_from_pickle);
}

// Convenience version of ReadValuesFromPickle that takes a base::Pickle
// instead of a base::PickleIterator. Expects the pickle to be completely
// consumed.
template <typename... Args>
  requires(internal::CanDeserialize<Args> && ...)
std::optional<std::tuple<Args...>> ReadValuesFromPickle(
    const base::Pickle& pickle) {
  // Need to specify the type to disambiguate the function pointer.
  using FunctionPointerType =
      std::optional<std::tuple<Args...>> (*)(base::PickleIterator&);
  FunctionPointerType read_values_from_pickle = &ReadValuesFromPickle<Args...>;
  return internal::CallWithPickleIterator(pickle, read_values_from_pickle);
}

// Convenience version of ReadPickleInto that takes a base::Pickle instead of
// a base::PickleIterator. Expects the pickle to be completely consumed.
template <typename... Args>
  requires(internal::CanDeserialize<Args> && ...)
[[nodiscard]] bool ReadPickleInto(const base::Pickle& pickle, Args&... args) {
  return internal::CallWithPickleIterator(pickle, &ReadPickleInto<Args...>,
                                          args...);
}

}  // namespace net

#endif  // NET_BASE_PICKLE_H_
