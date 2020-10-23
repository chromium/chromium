// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_LUID_MOJOM_TRAITS_H_
#define GPU_IPC_COMMON_LUID_MOJOM_TRAITS_H_

#include <windows.h>

#include "gpu/ipc/common/luid.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<gpu::mojom::LuidDataView, LUID> {
  static bool Read(gpu::mojom::LuidDataView data, LUID* out) {
    out->HighPart = data.high();
    out->LowPart = data.low();
    return true;
  }

  static int32_t high(const LUID& input) { return input.HighPart; }

  static uint32_t low(const LUID& input) { return input.LowPart; }
};

}  // namespace mojo

#endif  // GPU_IPC_COMMON_LUID_MOJOM_TRAITS_H_
