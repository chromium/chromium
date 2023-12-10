// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_ARRAY_TRAITS_H_
#define MOJO_PUBLIC_CPP_BINDINGS_ARRAY_TRAITS_H_

#include <concepts>

#include "base/numerics/safe_conversions.h"
#include "mojo/public/cpp/bindings/lib/default_construct_tag_internal.h"
#include "mojo/public/cpp/bindings/lib/template_util.h"

namespace mojo {

// This must be specialized for any type |T| to be serialized/deserialized as
// a mojom array.
//
// Usually you would like to do a partial specialization for a container (e.g.
// vector) template. Imagine you want to specialize it for Container<>, you need
// to implement:
//
//   template <typename T>
//   struct ArrayTraits<Container<T>> {
//     using Element = T;
//     // These two statements are optional. Use them if you'd like to serialize
//     // a container that supports iterators but does not support O(1) random
//     // access and so GetAt(...) would be expensive.
//     // using Iterator = Container<T>::iterator;
//     // using ConstIterator = Container<T>::const_iterator;
//
//     // These two methods are optional. Please see comments in struct_traits.h
//     // Note that unlike with StructTraits, IsNull() is called *twice* during
//     // serialization for ArrayTraits.
//     static bool IsNull(const Container<T>& input);
//     static void SetToNull(Container<T>* output);
//
//     static size_t GetSize(const Container<T>& input);
//
//     // These two methods are optional. They are used to access the
//     // underlying storage of the array to speed up copy of POD types.
//     static T* GetData(Container<T>& input);
//     static const T* GetData(const Container<T>& input);
//
//     // The following six methods are optional if the GetAt(...) methods are
//     // implemented. These methods specify how to read the elements of
//     // Container in some sequential order specified by the iterator.
//     //
//     // Acquires an iterator positioned at the first element in the container.
//     static ConstIterator GetBegin(const Container<T>& input);
//     static Iterator GetBegin(Container<T>& input);
//
//     // Advances |iterator| to the next position within the container.
//     static void AdvanceIterator(ConstIterator& iterator);
//     static void AdvanceIterator(Iterator& iterator);
//
//     // Returns a reference to the value at the current position of
//     // |iterator|. Optionally, the ConstIterator version of GetValue can
//     // return by value instead of by reference if it makes sense for the
//     // type.
//     static const T& GetValue(ConstIterator& iterator);
//     static T& GetValue(Iterator& iterator);
//
//     // These two methods are optional if the iterator methods are
//     // implemented.
//     static T& GetAt(Container<T>& input, size_t index);
//     static const T& GetAt(const Container<T>& input, size_t index);
//
//     // Returning false results in deserialization failure and causes the
//     // message pipe receiving it to be disconnected.
//     // Note that mojo does not require that Resize preserve the original
//     // elements in `input` it merely has to set the size of `input` to
//     // `size`.
//     static bool Resize(Container<T>& input, size_t size);
//   };
//
template <typename T>
struct ArrayTraits {
  static_assert(internal::AlwaysFalse<T>::value,
                "Cannot find the mojo::ArrayTraits specialization. Did you "
                "forget to include the corresponding header file?");
};

// Generic specialization for vector-like containers.
template <typename Container>
  requires requires(Container& c, size_t i) {
    typename Container::value_type;
    { c.size() } -> std::same_as<typename Container::size_type>;
    { c.clear() } -> std::same_as<void>;
    { c[i] } -> std::same_as<typename Container::reference>;
  }
struct ArrayTraits<Container> {
  using Element = Container::value_type;

  // vector-like containers have no built-in null.
  static bool IsNull(const Container& c) { return false; }
  static void SetToNull(Container* c) {
    // TODO(dcheng): Should this ever be called? It seems questionable...
    c->clear();
  }

  static auto GetSize(const Container& c) { return c.size(); }

  // Conditional since some vector implementations have specializations which do
  // not provide direct access to an underlying array, e.g. `std::vector<bool>`.
  static auto* GetData(Container& c)
    requires requires {
      { c.data() } -> std::same_as<typename Container::pointer>;
    }
  {
    return c.data();
  }
  static const auto* GetData(const Container& c)
    requires requires {
      { c.data() } -> std::same_as<typename Container::const_pointer>;
    }
  {
    return c.data();
  }

  // The static_casts here are safe, since out-of-range issues would be caught
  // by `Resize()`.
  static decltype(auto) GetAt(Container& c, size_t index) {
    return c[static_cast<typename Container::size_type>(index)];
  }
  static decltype(auto) GetAt(const Container& c, size_t index) {
    return c[static_cast<typename Container::size_type>(index)];
  }

  static bool Resize(Container& c, size_t size) {
    if (c.size() == size) {
      return true;
    }

    if (!base::IsValueInRangeForNumericType<typename Container::size_type>(
            size)) {
      return false;
    }

    if constexpr (std::constructible_from<Element,
                                          ::mojo::DefaultConstruct::Tag>) {
      Container temp;
      temp.reserve(size);
      for (size_t i = 0; i < size; ++i) {
        temp.emplace_back(internal::DefaultConstructTag());
      }
      c.swap(temp);
    } else {
      Container temp(static_cast<typename Container::size_type>(size));
      c.swap(temp);
    }

    return true;
  }
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_ARRAY_TRAITS_H_
