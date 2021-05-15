// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_HASH_UTIL_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_HASH_UTIL_H_

#include <cstring>
#include <functional>
#include <type_traits>
#include <vector>

#include "mojo/public/cpp/bindings/lib/template_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace mojo {
namespace internal {

template <typename T>
size_t HashCombine(size_t seed, const T& value) {
  // Based on proposal in:
  // http://www.open-std.org/JTC1/SC22/WG21/docs/papers/2005/n1756.pdf
  return seed ^ (std::hash<T>()(value) + (seed << 6) + (seed >> 2));
}

template <typename T>
struct HasHashMethod {
  template <typename U>
  static char Test(decltype(&U::Hash));
  template <typename U>
  static int Test(...);
  static const bool value = sizeof(Test<T>(0)) == sizeof(char);

 private:
  EnsureTypeIsComplete<T> check_t_;
};

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
struct HashTraits<absl::optional<std::vector<T>>, false> {
  static size_t Hash(size_t seed, const absl::optional<std::vector<T>>& value) {
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
