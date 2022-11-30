// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_WTF_CLONE_EQUALS_UTIL_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_WTF_CLONE_EQUALS_UTIL_H_

#include <type_traits>

#include "mojo/public/cpp/bindings/clone_traits.h"
#include "mojo/public/cpp/bindings/equals_traits.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace mojo {

template <typename T>
struct CloneTraits<WTF::Vector<T>> {
  static WTF::Vector<T> Clone(const WTF::Vector<T>& input) {
    WTF::Vector<T> result;
    result.reserve(input.size());
    for (const auto& element : input)
      result.push_back(mojo::Clone(element));

    return result;
  }
};

template <typename K, typename V>
struct CloneTraits<WTF::HashMap<K, V>> {
  static WTF::HashMap<K, V> Clone(const WTF::HashMap<K, V>& input) {
    WTF::HashMap<K, V> result;
    for (const auto& element : input)
      result.insert(mojo::Clone(element.key), mojo::Clone(element.value));

    return result;
  }
};

template <typename T>
struct EqualsTraits<WTF::Vector<T>> {
  static bool Equals(const WTF::Vector<T>& a, const WTF::Vector<T>& b) {
    if (a.size() != b.size())
      return false;
    for (wtf_size_t i = 0; i < a.size(); ++i) {
      if (!mojo::Equals(a[i], b[i]))
        return false;
    }
    return true;
  }
};

template <typename K, typename V>
struct EqualsTraits<WTF::HashMap<K, V>> {
  static bool Equals(const WTF::HashMap<K, V>& a, const WTF::HashMap<K, V>& b) {
    if (a.size() != b.size())
      return false;

    auto a_end = a.end();
    auto b_end = b.end();

    for (auto iter = a.begin(); iter != a_end; ++iter) {
      auto b_iter = b.find(iter->key);
      if (b_iter == b_end || !mojo::Equals(iter->value, b_iter->value))
        return false;
    }
    return true;
  }
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_WTF_CLONE_EQUALS_UTIL_H_
