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
struct CloneTraits<blink::Vector<T>> {
  static blink::Vector<T> Clone(const blink::Vector<T>& input) {
    blink::Vector<T> result;
    result.reserve(input.size());
    for (const auto& element : input)
      result.push_back(mojo::Clone(element));

    return result;
  }
};

template <typename K, typename V>
struct CloneTraits<blink::HashMap<K, V>> {
  static blink::HashMap<K, V> Clone(const blink::HashMap<K, V>& input) {
    blink::HashMap<K, V> result;
    for (const auto& element : input)
      result.insert(mojo::Clone(element.key), mojo::Clone(element.value));

    return result;
  }
};

template <typename T>
struct EqualsTraits<blink::Vector<T>> {
  static bool Equals(const blink::Vector<T>& a, const blink::Vector<T>& b) {
    if (a.size() != b.size())
      return false;
    for (blink::wtf_size_t i = 0; i < a.size(); ++i) {
      if (!mojo::Equals(a[i], b[i]))
        return false;
    }
    return true;
  }
};

template <typename K, typename V>
struct EqualsTraits<blink::HashMap<K, V>> {
  static bool Equals(const blink::HashMap<K, V>& a,
                     const blink::HashMap<K, V>& b) {
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
