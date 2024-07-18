// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef MOJO_PUBLIC_CPP_BINDINGS_ARRAY_TRAITS_SPAN_H_
#define MOJO_PUBLIC_CPP_BINDINGS_ARRAY_TRAITS_SPAN_H_

#include <cstddef>

#include "base/containers/span.h"
#include "mojo/public/cpp/bindings/array_traits.h"

namespace mojo {

template <typename T, size_t Extent>
struct ArrayTraits<base::span<T, Extent>> {
  using Element = T;

  static size_t GetSize(const base::span<T>& input) { return input.size(); }

  static T* GetData(base::span<T>& input) { return input.data(); }

  static const T* GetData(const base::span<T>& input) { return input.data(); }

  static T& GetAt(base::span<T>& input, size_t index) {
    return input.data()[index];
  }

  static const T& GetAt(const base::span<T>& input, size_t index) {
    return input.data()[index];
  }

  static bool Resize(base::span<T>& input, size_t size) {
    if (size > input.size())
      return false;
    input = input.first(size);
    return true;
  }
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_ARRAY_TRAITS_SPAN_H_
