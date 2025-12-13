// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BASE_BYTE_SIZE_MOJOM_TRAITS_H_
#define MOJO_PUBLIC_CPP_BASE_BYTE_SIZE_MOJOM_TRAITS_H_

#include "base/byte_size.h"
#include "base/component_export.h"
#include "mojo/public/mojom/base/byte_size.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(MOJO_BASE_SHARED_TRAITS)
    StructTraits<mojo_base::mojom::ByteSizeDataView, base::ByteSize> {
  static uint64_t size(const base::ByteSize& byte_size) {
    return byte_size.InBytes();
  }
  static bool Read(mojo_base::mojom::ByteSizeDataView data,
                   base::ByteSize* byte_size);
};

template <>
struct COMPONENT_EXPORT(MOJO_BASE_SHARED_TRAITS)
    StructTraits<mojo_base::mojom::ByteSizeDeltaDataView, base::ByteSizeDelta> {
  static int64_t delta(const base::ByteSizeDelta& byte_size_delta) {
    return byte_size_delta.InBytes();
  }
  static bool Read(mojo_base::mojom::ByteSizeDeltaDataView data,
                   base::ByteSizeDelta* byte_size_delta);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BASE_BYTE_SIZE_MOJOM_TRAITS_H_
