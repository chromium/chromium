// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BASE_BYTE_STRING_MOJOM_TRAITS_H_
#define MOJO_PUBLIC_CPP_BASE_BYTE_STRING_MOJOM_TRAITS_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/array_traits_span.h"
#include "mojo/public/mojom/base/byte_string.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(MOJO_BASE_SHARED_TRAITS)
    StructTraits<mojo_base::mojom::ByteStringDataView, std::string> {
  static base::span<const uint8_t> data(const std::string& string) {
    return base::as_bytes(base::make_span(string));
  }
  static bool Read(mojo_base::mojom::ByteStringDataView data, std::string* out);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BASE_BYTE_STRING_MOJOM_TRAITS_H_
