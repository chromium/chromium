// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef MOJO_PUBLIC_CPP_BASE_STRING16_MOJOM_TRAITS_H_
#define MOJO_PUBLIC_CPP_BASE_STRING16_MOJOM_TRAITS_H_

#include <cstdint>
#include <string_view>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "mojo/public/mojom/base/string16.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(MOJO_BASE_TRAITS)
    StructTraits<mojo_base::mojom::String16DataView, std::u16string_view> {
  static base::span<const uint16_t> data(std::u16string_view str) {
    return base::make_span(reinterpret_cast<const uint16_t*>(str.data()),
                           str.size());
  }
};

template <>
struct COMPONENT_EXPORT(MOJO_BASE_TRAITS)
    StructTraits<mojo_base::mojom::String16DataView, std::u16string> {
  static base::span<const uint16_t> data(const std::u16string& str) {
    return StructTraits<mojo_base::mojom::String16DataView,
                        std::u16string_view>::data(str);
  }

  static bool Read(mojo_base::mojom::String16DataView data,
                   std::u16string* out);
};

template <>
struct COMPONENT_EXPORT(MOJO_BASE_TRAITS)
    StructTraits<mojo_base::mojom::BigString16DataView, std::u16string> {
  static mojo_base::BigBuffer data(const std::u16string& str);

  static bool Read(mojo_base::mojom::BigString16DataView data,
                   std::u16string* out);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BASE_STRING16_MOJOM_TRAITS_H_
