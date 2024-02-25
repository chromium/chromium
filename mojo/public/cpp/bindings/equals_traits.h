// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_EQUALS_TRAITS_H_
#define MOJO_PUBLIC_CPP_BINDINGS_EQUALS_TRAITS_H_

#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "mojo/public/cpp/bindings/lib/template_util.h"

namespace mojo {

// EqualsTraits<> allows you to specify comparison functions for mapped mojo
// objects. By default objects can be compared if they implement operator==()
// or have a method named Equals().

template <typename T, typename SFINAE = void>
struct HasEqualsMethod : std::false_type {
  static_assert(sizeof(T), "T must be a complete type.");
};

template <typename T>
struct HasEqualsMethod<T,
                       std::void_t<decltype(std::declval<const T&>().Equals(
                           std::declval<const T&>()))>> : std::true_type {};

template <typename T>
bool Equals(const T& a, const T& b);

template <typename T>
struct EqualsTraits {
  static bool Equals(const T& a, const T& b) {
    if constexpr (HasEqualsMethod<T>::value) {
      return a.Equals(b);
    } else {
      return a == b;
    }
  }
};

template <typename T>
struct EqualsTraits<std::optional<T>> {
  static bool Equals(const std::optional<T>& a, const std::optional<T>& b) {
    if (!a && !b)
      return true;
    if (!a || !b)
      return false;

    // NOTE: Not just Equals() because that's EqualsTraits<>::Equals() and we
    // want mojo::Equals() for things like std::optional<std::vector<T>>.
    return mojo::Equals(*a, *b);
  }
};

template <typename T>
struct EqualsTraits<std::vector<T>> {
  static bool Equals(const std::vector<T>& a, const std::vector<T>& b) {
    if (a.size() != b.size())
      return false;
    for (size_t i = 0; i < a.size(); ++i) {
      if (!mojo::Equals(a[i], b[i]))
        return false;
    }
    return true;
  }
};

template <typename K, typename V>
struct EqualsTraits<base::flat_map<K, V>> {
  static bool Equals(const base::flat_map<K, V>& a,
                     const base::flat_map<K, V>& b) {
    if (a.size() != b.size())
      return false;
    for (const auto& element : a) {
      auto iter = b.find(element.first);
      if (iter == b.end() || !mojo::Equals(element.second, iter->second))
        return false;
    }
    return true;
  }
};

template <typename T>
bool Equals(const T& a, const T& b) {
  return EqualsTraits<T>::Equals(a, b);
}

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_EQUALS_TRAITS_H_
