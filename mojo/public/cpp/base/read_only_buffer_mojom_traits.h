// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BASE_READ_ONLY_BUFFER_MOJOM_TRAITS_H_
#define MOJO_PUBLIC_CPP_BASE_READ_ONLY_BUFFER_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "base/containers/span.h"
#include "mojo/public/mojom/base/read_only_buffer.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(MOJO_BASE_SHARED_TRAITS)
    StructTraits<mojo_base::mojom::ReadOnlyBufferDataView,
                 base::span<const uint8_t>> {
  static base::span<const uint8_t> buffer(base::span<const uint8_t> input) {
    return input;
  }

  static bool Read(mojo_base::mojom::ReadOnlyBufferDataView input,
                   base::span<const uint8_t>* out);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BASE_READ_ONLY_BUFFER_MOJOM_TRAITS_H_
