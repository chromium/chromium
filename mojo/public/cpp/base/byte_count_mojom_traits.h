// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BASE_BYTE_COUNT_MOJOM_TRAITS_H_
#define MOJO_PUBLIC_CPP_BASE_BYTE_COUNT_MOJOM_TRAITS_H_

#include "base/byte_count.h"
#include "base/component_export.h"
#include "mojo/public/mojom/base/byte_count.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(MOJO_BASE_SHARED_TRAITS)
    StructTraits<mojo_base::mojom::ByteCountDataView, base::ByteCount> {
  static int64_t count(const base::ByteCount& byte_count) {
    return byte_count.InBytes();
  }
  static bool Read(mojo_base::mojom::ByteCountDataView data,
                   base::ByteCount* byte_count);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BASE_BYTE_COUNT_MOJOM_TRAITS_H_
