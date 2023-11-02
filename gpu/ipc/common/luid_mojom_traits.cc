// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/common/luid_mojom_traits.h"

#include <windows.h>

#include "gpu/gpu_export.h"
#include "gpu/ipc/common/luid.mojom-shared.h"

namespace mojo {

// static
bool StructTraits<gpu::mojom::LuidDataView, ::CHROME_LUID>::Read(
    gpu::mojom::LuidDataView data,
    ::CHROME_LUID* out) {
  out->HighPart = data.high();
  out->LowPart = data.low();
  return true;
}

}  // namespace mojo
