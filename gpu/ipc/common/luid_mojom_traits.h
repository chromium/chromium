// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_LUID_MOJOM_TRAITS_H_
#define GPU_IPC_COMMON_LUID_MOJOM_TRAITS_H_

#include "base/win/windows_types.h"
#include "gpu/gpu_export.h"
#include "gpu/ipc/common/luid.mojom-shared.h"

namespace mojo {

template <>
struct GPU_EXPORT StructTraits<gpu::mojom::LuidDataView, ::CHROME_LUID> {
  static bool Read(gpu::mojom::LuidDataView data, ::CHROME_LUID* out);

  static int32_t high(const ::CHROME_LUID& input) { return input.HighPart; }

  static uint32_t low(const ::CHROME_LUID& input) { return input.LowPart; }
};

}  // namespace mojo

#endif  // GPU_IPC_COMMON_LUID_MOJOM_TRAITS_H_
