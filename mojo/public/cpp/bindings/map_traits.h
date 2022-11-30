// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_MAP_TRAITS_H_
#define MOJO_PUBLIC_CPP_BINDINGS_MAP_TRAITS_H_

#include "mojo/public/cpp/bindings/lib/template_util.h"

namespace mojo {

// This must be specialized for any type |T| to be serialized/deserialized as
// a mojom map.
//
// Usually you would like to do a partial specialization for a map template.
// Imagine you want to specialize it for CustomMap<>, you need to implement:
//
//   template <typename K, typename V>
//   struct MapTraits<CustomMap<K, V>> {
//     using Key = K;
//     using Value = V;
//
//     // These two methods are optional. Please see comments in struct_traits.h
//     // Note that unlike with StructTraits, IsNull() is called *twice* during
//     // serialization for MapTraits.
//     static bool IsNull(const CustomMap<K, V>& input);
//     static void SetToNull(CustomMap<K, V>* output);
//
//     static size_t GetSize(const CustomMap<K, V>& input);
//
//     static CustomConstIterator GetBegin(const CustomMap<K, V>& input);
//     static CustomIterator GetBegin(CustomMap<K, V>& input);
//
//     static void AdvanceIterator(CustomConstIterator& iterator);
//     static void AdvanceIterator(CustomIterator& iterator);
//
//     static const K& GetKey(CustomIterator& iterator);
//     static const K& GetKey(CustomConstIterator& iterator);
//
//     static V& GetValue(CustomIterator& iterator);
//     static const V& GetValue(CustomConstIterator& iterator);
//
//     // Returning false results in deserialization failure and causes the
//     // message pipe receiving it to be disconnected. |IK| and |IV| are
//     // separate input key/value template parameters that allows for the
//     // the key/value types to be forwarded.
//     template <typename IK, typename IV>
//     static bool Insert(CustomMap<K, V>& input,
//                        IK&& key,
//                        IV&& value);
//
//     static void SetToEmpty(CustomMap<K, V>* output);
//   };
//
template <typename T>
struct MapTraits {
  static_assert(internal::AlwaysFalse<T>::value,
                "Cannot find the mojo::MapTraits specialization. Did you "
                "forget to include the corresponding header file?");
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_MAP_TRAITS_H_
