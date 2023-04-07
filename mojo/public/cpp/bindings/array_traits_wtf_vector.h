// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_ARRAY_TRAITS_WTF_VECTOR_H_
#define MOJO_PUBLIC_CPP_BINDINGS_ARRAY_TRAITS_WTF_VECTOR_H_

#include "mojo/public/cpp/bindings/array_traits.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace mojo {

template <typename U, WTF::wtf_size_t InlineCapacity>
struct ArrayTraits<WTF::Vector<U, InlineCapacity>> {
  using Element = U;

  static bool IsNull(const WTF::Vector<U, InlineCapacity>& input) {
    // WTF::Vector<> is always converted to non-null mojom array.
    return false;
  }

  static void SetToNull(WTF::Vector<U, InlineCapacity>* output) {
    // WTF::Vector<> doesn't support null state. Set it to empty instead.
    output->clear();
  }

  static size_t GetSize(const WTF::Vector<U, InlineCapacity>& input) {
    return input.size();
  }

  static U* GetData(WTF::Vector<U, InlineCapacity>& input) {
    return input.data();
  }

  static const U* GetData(const WTF::Vector<U, InlineCapacity>& input) {
    return input.data();
  }

  static U& GetAt(WTF::Vector<U, InlineCapacity>& input, size_t index) {
    return input[static_cast<wtf_size_t>(index)];
  }

  static const U& GetAt(const WTF::Vector<U, InlineCapacity>& input,
                        size_t index) {
    return input[static_cast<wtf_size_t>(index)];
  }

  static bool Resize(WTF::Vector<U, InlineCapacity>& input, size_t size) {
    if (!base::IsValueInRangeForNumericType<wtf_size_t>(size))
      return false;
    WTF::Vector<U, InlineCapacity> temp(static_cast<wtf_size_t>(size));
    input.swap(temp);
    return true;
  }
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_ARRAY_TRAITS_WTF_VECTOR_H_
