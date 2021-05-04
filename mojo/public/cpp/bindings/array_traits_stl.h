// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_ARRAY_TRAITS_STL_H_
#define MOJO_PUBLIC_CPP_BINDINGS_ARRAY_TRAITS_STL_H_

#include <array>
#include <map>
#include <set>
#include <unordered_set>
#include <vector>

#include "mojo/public/cpp/bindings/array_traits.h"

namespace mojo {

template <typename T>
struct ArrayTraits<std::unordered_set<T>> {
  using Element = T;
  using ConstIterator = typename std::unordered_set<T>::const_iterator;

  static bool IsNull(const std::unordered_set<T>& input) {
    // std::unordered_set<> is always converted to non-null mojom array.
    return false;
  }

  static size_t GetSize(const std::unordered_set<T>& input) {
    return input.size();
  }

  static ConstIterator GetBegin(const std::unordered_set<T>& input) {
    return input.begin();
  }

  static void AdvanceIterator(ConstIterator& iterator) { ++iterator; }

  static const T& GetValue(ConstIterator& iterator) { return *iterator; }
};

template <typename T>
struct ArrayTraits<std::vector<T>> {
  using Element = T;

  static bool IsNull(const std::vector<T>& input) {
    // std::vector<> is always converted to non-null mojom array.
    return false;
  }

  static void SetToNull(std::vector<T>* output) {
    // std::vector<> doesn't support null state. Set it to empty instead.
    output->clear();
  }

  static size_t GetSize(const std::vector<T>& input) { return input.size(); }

  static T* GetData(std::vector<T>& input) { return input.data(); }

  static const T* GetData(const std::vector<T>& input) { return input.data(); }

  static typename std::vector<T>::reference GetAt(std::vector<T>& input,
                                                  size_t index) {
    return input[index];
  }

  static typename std::vector<T>::const_reference GetAt(
      const std::vector<T>& input,
      size_t index) {
    return input[index];
  }

  static inline bool Resize(std::vector<T>& input, size_t size) {
    // Instead of calling std::vector<T>::resize() directly, this is a hack to
    // make compilers happy. Some compilers (e.g., Mac, Android, Linux MSan)
    // currently don't allow resizing types like
    // std::vector<std::vector<MoveOnlyType>>.
    // Because the deserialization code doesn't care about the original contents
    // of |input|, we discard them directly.
    //
    // The "inline" keyword of this method matters. Without it, we have observed
    // significant perf regression with some tests on Mac. crbug.com/631415
    if (input.size() != size) {
      std::vector<T> temp(size);
      input.swap(temp);
    }

    return true;
  }
};

// This ArrayTraits specialization is used only for serialization.
template <typename T>
struct ArrayTraits<std::set<T>> {
  using Element = T;
  using ConstIterator = typename std::set<T>::const_iterator;

  static bool IsNull(const std::set<T>& input) {
    // std::set<> is always converted to non-null mojom array.
    return false;
  }

  static size_t GetSize(const std::set<T>& input) { return input.size(); }

  static ConstIterator GetBegin(const std::set<T>& input) {
    return input.begin();
  }
  static void AdvanceIterator(ConstIterator& iterator) {
    ++iterator;
  }
  static const T& GetValue(ConstIterator& iterator) {
    return *iterator;
  }
};

template <typename K, typename V>
struct MapValuesArrayView {
  explicit MapValuesArrayView(const std::map<K, V>& map) : map(map) {}
  const std::map<K, V>& map;
};

// Convenience function to create a MapValuesArrayView<> that infers the
// template arguments from its argument type.
template <typename K, typename V>
MapValuesArrayView<K, V> MapValuesToArray(const std::map<K, V>& map) {
  return MapValuesArrayView<K, V>(map);
}

// This ArrayTraits specialization is used only for serialization and converts
// a map<K, V> into an array<V>, discarding the keys.
template <typename K, typename V>
struct ArrayTraits<MapValuesArrayView<K, V>> {
  using Element = V;
  using ConstIterator = typename std::map<K, V>::const_iterator;

  static bool IsNull(const MapValuesArrayView<K, V>& input) {
    // std::map<> is always converted to non-null mojom array.
    return false;
  }

  static size_t GetSize(const MapValuesArrayView<K, V>& input) {
    return input.map.size();
  }
  static ConstIterator GetBegin(const MapValuesArrayView<K, V>& input) {
    return input.map.begin();
  }
  static void AdvanceIterator(ConstIterator& iterator) { ++iterator; }
  static const V& GetValue(ConstIterator& iterator) { return iterator->second; }
};

// This ArrayTraits specialization is used for conversion between
// std::array<T, N> and array<T, N>.
template <typename T, size_t N>
struct ArrayTraits<std::array<T, N>> {
  using Element = T;

  static bool IsNull(const std::array<T, N>& input) { return false; }

  static size_t GetSize(const std::array<T, N>& input) { return N; }

  static const T& GetAt(const std::array<T, N>& input, size_t index) {
    return input[index];
  }
  static T& GetAt(std::array<T, N>& input, size_t index) {
    return input[index];
  }

  // std::array is fixed size but this is called during deserialization.
  static bool Resize(std::array<T, N>& input, size_t size) {
    if (size != N)
      return false;
    return true;
  }
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_ARRAY_TRAITS_STL_H_
