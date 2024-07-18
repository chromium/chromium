// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef MOJO_PUBLIC_CPP_BASE_WSTRING_MOJOM_TRAITS_H_
#define MOJO_PUBLIC_CPP_BASE_WSTRING_MOJOM_TRAITS_H_

#include <string>

#include "base/containers/span.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "mojo/public/mojom/base/wstring.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(MOJO_BASE_TRAITS)
    StructTraits<mojo_base::mojom::WStringDataView, std::wstring> {
  static base::span<const uint16_t> data(const std::wstring& str) {
    return base::make_span(reinterpret_cast<const uint16_t*>(str.data()),
                           str.size());
  }

  static bool Read(mojo_base::mojom::WStringDataView data, std::wstring* out);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BASE_WSTRING_MOJOM_TRAITS_H_
