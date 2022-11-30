// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_MAP_TRAITS_FLAT_MAP_H_
#define MOJO_PUBLIC_CPP_BINDINGS_MAP_TRAITS_FLAT_MAP_H_

#include "base/containers/flat_map.h"
#include "mojo/public/cpp/bindings/map_traits.h"

namespace mojo {

template <typename K, typename V, typename Compare>
struct MapTraits<base::flat_map<K, V, Compare>> {
  using Key = K;
  using Value = V;
  using Iterator = typename base::flat_map<K, V, Compare>::iterator;
  using ConstIterator = typename base::flat_map<K, V, Compare>::const_iterator;

  static size_t GetSize(const base::flat_map<K, V, Compare>& input) {
    return input.size();
  }

  static ConstIterator GetBegin(const base::flat_map<K, V, Compare>& input) {
    return input.begin();
  }
  static Iterator GetBegin(base::flat_map<K, V, Compare>& input) {
    return input.begin();
  }

  static void AdvanceIterator(ConstIterator& iterator) { iterator++; }
  static void AdvanceIterator(Iterator& iterator) { iterator++; }

  static const K& GetKey(Iterator& iterator) { return iterator->first; }
  static const K& GetKey(ConstIterator& iterator) { return iterator->first; }

  static V& GetValue(Iterator& iterator) { return iterator->second; }
  static const V& GetValue(ConstIterator& iterator) { return iterator->second; }

  template <typename MaybeConstKeyType, typename MaybeConstValueType>
  static bool Insert(base::flat_map<K, V, Compare>& input,
                     MaybeConstKeyType&& key,
                     MaybeConstValueType&& value) {
    input.emplace(std::forward<MaybeConstKeyType>(key),
                  std::forward<MaybeConstValueType>(value));
    return true;
  }

  static void SetToEmpty(base::flat_map<K, V, Compare>* output) {
    output->clear();
  }
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_MAP_TRAITS_FLAT_MAP_H_
