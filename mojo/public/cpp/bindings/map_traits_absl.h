// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_MAP_TRAITS_ABSL_H_
#define MOJO_PUBLIC_CPP_BINDINGS_MAP_TRAITS_ABSL_H_

#include "mojo/public/cpp/bindings/map_traits.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace mojo {

template <typename K, typename V>
struct MapTraits<absl::flat_hash_map<K, V>> {
  using Key = K;
  using Value = V;
  using Iterator = typename absl::flat_hash_map<K, V>::iterator;
  using ConstIterator = typename absl::flat_hash_map<K, V>::const_iterator;

  static size_t GetSize(const absl::flat_hash_map<K, V>& input) {
    return input.size();
  }

  static ConstIterator GetBegin(const absl::flat_hash_map<K, V>& input) {
    return input.begin();
  }

  static void AdvanceIterator(ConstIterator& iterator) { ++iterator; }

  static const Key& GetKey(ConstIterator& iterator) { return iterator->first; }

  static const Value& GetValue(ConstIterator& iterator) {
    return iterator->second;
  }

  static bool Insert(absl::flat_hash_map<K, V>& input, Key key, Value&& value) {
    // Return false if the key already exists (duplicate keys are invalid).
    auto [it, inserted] = input.insert({std::move(key), std::move(value)});
    return inserted;
  }

  static void SetToEmpty(absl::flat_hash_map<K, V>* output) { output->clear(); }
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_MAP_TRAITS_ABSL_H_
