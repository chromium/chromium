// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_ARRAY_TRAITS_WEB_VECTOR_H_
#define MOJO_PUBLIC_CPP_BINDINGS_ARRAY_TRAITS_WEB_VECTOR_H_

#include "mojo/public/cpp/bindings/array_traits.h"
#include "third_party/blink/public/platform/web_vector.h"

namespace mojo {

template <typename U>
struct ArrayTraits<blink::WebVector<U>> {
  using Element = U;

  static bool IsNull(const blink::WebVector<U>& input) {
    // blink::WebVector<> is always converted to non-null mojom array.
    return false;
  }

  static void SetToNull(blink::WebVector<U>* output) {
    // blink::WebVector<> doesn't support null state. Set it to empty instead.
    output->Clear();
  }

  static size_t GetSize(const blink::WebVector<U>& input) {
    return input.size();
  }

  static U* GetData(blink::WebVector<U>& input) { return input.Data(); }

  static const U* GetData(const blink::WebVector<U>& input) {
    return input.Data();
  }

  static U& GetAt(blink::WebVector<U>& input, size_t index) {
    return input[index];
  }

  static const U& GetAt(const blink::WebVector<U>& input, size_t index) {
    return input[index];
  }

  static bool Resize(blink::WebVector<U>& input, size_t size) {
    // WebVector DCHECKs if the new size is larger than capacity().  Call
    // reserve() first to be safe.
    input.reserve(size);
    input.resize(size);
    return true;
  }
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_ARRAY_TRAITS_WEB_VECTOR_H_
