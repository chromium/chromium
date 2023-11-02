// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_MAP_TRAITS_PROTOBUF_H_
#define MOJO_PUBLIC_CPP_BINDINGS_MAP_TRAITS_PROTOBUF_H_

#include <map>
#include <utility>

#include "base/logging.h"
#include "mojo/public/cpp/bindings/map_traits.h"
#include "third_party/protobuf/src/google/protobuf/map.h"

namespace mojo {

template <typename K, typename V>
struct MapTraits<::google::protobuf::Map<K, V>> {
  using Key = K;
  using Value = V;
  using Iterator = typename ::google::protobuf::Map<K, V>::iterator;
  using ConstIterator = typename ::google::protobuf::Map<K, V>::const_iterator;

  static bool IsNull(const ::google::protobuf::Map<K, V>& input) {
    // Protobuf Maps are always converted to a non-null mojom map.
    return false;
  }

  static void SetToNull(::google::protobuf::Map<K, V>* output) {
    // Protobuf Maps don't have a null state. Set it to empty instead.
    output->clear();
  }

  static size_t GetSize(const ::google::protobuf::Map<K, V>& input) {
    return input.size();
  }

  static ConstIterator GetBegin(const ::google::protobuf::Map<K, V>& input) {
    return input.begin();
  }
  static Iterator GetBegin(::google::protobuf::Map<K, V>& input) {
    return input.begin();
  }

  static void AdvanceIterator(ConstIterator& iterator) { iterator++; }
  static void AdvanceIterator(Iterator& iterator) { iterator++; }

  static const K& GetKey(Iterator& iterator) { return iterator->first; }
  static const K& GetKey(ConstIterator& iterator) { return iterator->first; }

  static V& GetValue(Iterator& iterator) { return iterator->second; }
  static const V& GetValue(ConstIterator& iterator) { return iterator->second; }

  template <typename MaybeConstKeyType, typename MaybeConstValueType>
  static bool Insert(::google::protobuf::Map<K, V>& input,
                     MaybeConstKeyType&& key,
                     MaybeConstValueType&& value) {
    input.insert({std::forward<MaybeConstKeyType>(key),
                  std::forward<MaybeConstValueType>(value)});
    return true;
  }

  static void SetToEmpty(::google::protobuf::Map<K, V>* output) {
    output->clear();
  }
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_MAP_TRAITS_PROTOBUF_H_
