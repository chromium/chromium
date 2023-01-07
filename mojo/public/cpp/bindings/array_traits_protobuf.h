// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_ARRAY_TRAITS_PROTOBUF_H_
#define MOJO_PUBLIC_CPP_BINDINGS_ARRAY_TRAITS_PROTOBUF_H_

#include "base/numerics/safe_conversions.h"
#include "mojo/public/cpp/bindings/array_traits.h"
#include "third_party/protobuf/src/google/protobuf/repeated_field.h"

namespace mojo {

template <typename T>
struct ArrayTraits<::google::protobuf::RepeatedPtrField<T>> {
  using Element = T;

  static bool IsNull(const ::google::protobuf::RepeatedPtrField<T>& input) {
    // Always convert RepeatedPtrField to a non-null mojom array.
    return false;
  }

  static T* GetData(::google::protobuf::RepeatedPtrField<T>& input) {
    return input.data();
  }

  static const T* GetData(
      const ::google::protobuf::RepeatedPtrField<T>& input) {
    return input.data();
  }

  static T& GetAt(::google::protobuf::RepeatedPtrField<T>& input,
                  size_t index) {
    return input.at(index);
  }

  static const T& GetAt(const ::google::protobuf::RepeatedPtrField<T>& input,
                        size_t index) {
    return input.at(index);
  }

  static size_t GetSize(const ::google::protobuf::RepeatedPtrField<T>& input) {
    return input.size();
  }

  static bool Resize(::google::protobuf::RepeatedPtrField<T>& input,
                     size_t new_size) {
    if (!base::IsValueInRangeForNumericType<int>(new_size)) {
      return false;
    }

    // We call Reserve() to set the capacity and then add elements to increase
    // the container size to the requested value. We can't rely on Reserve()
    // alone as that will resize the container but size() will still report the
    // previous number of elements which will cause deserialization failures.
    // Unfortunately there isn't an AddRange() or similar function available so
    // we need to add elements one at a time in a loop.
    int requested_size = base::checked_cast<int>(new_size);
    input.Reserve(requested_size);
    while (input.size() < requested_size) {
      input.Add();
    }

    return true;
  }
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_ARRAY_TRAITS_PROTOBUF_H_
