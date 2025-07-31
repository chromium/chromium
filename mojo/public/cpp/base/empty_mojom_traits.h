// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BASE_EMPTY_MOJOM_TRAITS_H_
#define MOJO_PUBLIC_CPP_BASE_EMPTY_MOJOM_TRAITS_H_

#include <variant>

#include "mojo/public/cpp/bindings/struct_traits.h"
#include "mojo/public/mojom/base/empty.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<mojo_base::mojom::EmptyDataView, ::std::monostate> {
  static bool Read(mojo_base::mojom::EmptyDataView data,
                   std::monostate* out) {
    return true;
  }
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BASE_EMPTY_MOJOM_TRAITS_H_
