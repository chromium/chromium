// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_MAP_TRAITS_WTF_HASH_MAP_H_
#define MOJO_PUBLIC_CPP_BINDINGS_MAP_TRAITS_WTF_HASH_MAP_H_

#include "base/logging.h"
#include "mojo/public/cpp/bindings/map_traits.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"

namespace mojo {

template <typename K, typename V>
struct MapTraits<blink::HashMap<K, V>> {
  using Key = K;
  using Value = V;
  using Iterator = typename blink::HashMap<K, V>::iterator;
  using ConstIterator = typename blink::HashMap<K, V>::const_iterator;

  static bool IsNull(const blink::HashMap<K, V>& input) {
    // blink::HashMap<> is always converted to non-null mojom map.
    return false;
  }

  static void SetToNull(blink::HashMap<K, V>* output) {
    // blink::HashMap<> doesn't support null state. Set it to empty instead.
    output->clear();
  }

  static size_t GetSize(const blink::HashMap<K, V>& input) {
    return input.size();
  }

  static ConstIterator GetBegin(const blink::HashMap<K, V>& input) {
    return input.begin();
  }
  static Iterator GetBegin(blink::HashMap<K, V>& input) {
    return input.begin();
  }

  static void AdvanceIterator(ConstIterator& iterator) { ++iterator; }
  static void AdvanceIterator(Iterator& iterator) { ++iterator; }

  static const K& GetKey(Iterator& iterator) { return iterator->key; }
  static const K& GetKey(ConstIterator& iterator) { return iterator->key; }

  static V& GetValue(Iterator& iterator) { return iterator->value; }
  static const V& GetValue(ConstIterator& iterator) { return iterator->value; }

  template <typename IK, typename IV>
  static bool Insert(blink::HashMap<K, V>& input, IK&& key, IV&& value) {
    if (!blink::HashMap<K, V>::IsValidKey(key)) {
      LOG(ERROR) << "The key value is disallowed by blink::HashMap";
      return false;
    }
    input.insert(std::forward<IK>(key), std::forward<IV>(value));
    return true;
  }

  static void SetToEmpty(blink::HashMap<K, V>* output) { output->clear(); }
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_MAP_TRAITS_WTF_HASH_MAP_H_
