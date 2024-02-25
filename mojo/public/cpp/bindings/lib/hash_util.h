// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_HASH_UTIL_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_HASH_UTIL_H_

#include <cstring>
#include <functional>
#include <optional>
#include <type_traits>
#include <vector>

#include "mojo/public/cpp/bindings/lib/template_util.h"

namespace mojo {
namespace internal {

template <typename T>
size_t HashCombine(size_t seed, const T& value) {
  // Based on proposal in:
  // http://www.open-std.org/JTC1/SC22/WG21/docs/papers/2005/n1756.pdf
  return seed ^ (std::hash<T>()(value) + (seed << 6) + (seed >> 2));
}

template <typename T, typename SFINAE = void>
struct HasHashMethod : std::false_type {
  static_assert(sizeof(T), "T must be a complete type.");
};

template <typename T>
struct HasHashMethod<T,
                     std::void_t<decltype(std::declval<T>().Hash(size_t{0}))>>
    : std::true_type {};

template <typename T, bool has_hash_method = HasHashMethod<T>::value>
struct HashTraits;

template <typename T>
size_t Hash(size_t seed, const T& value);

template <typename T>
struct HashTraits<T, true> {
  static size_t Hash(size_t seed, const T& value) { return value.Hash(seed); }
};

template <typename T>
struct HashTraits<T, false> {
  static size_t Hash(size_t seed, const T& value) {
    return HashCombine(seed, value);
  }
};

template <typename T>
struct HashTraits<std::vector<T>, false> {
  static size_t Hash(size_t seed, const std::vector<T>& value) {
    for (const auto& element : value) {
      seed = HashCombine(seed, element);
    }
    return seed;
  }
};

template <typename T>
struct HashTraits<std::optional<std::vector<T>>, false> {
  static size_t Hash(size_t seed, const std::optional<std::vector<T>>& value) {
    if (!value)
      return HashCombine(seed, 0);

    return Hash(seed, *value);
  }
};

template <typename T>
size_t Hash(size_t seed, const T& value) {
  return HashTraits<T>::Hash(seed, value);
}

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_HASH_UTIL_H_
