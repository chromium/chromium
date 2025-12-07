// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_SHARED_IMAGE_POOL_ID_MOJOM_TRAITS_H_
#define GPU_IPC_COMMON_SHARED_IMAGE_POOL_ID_MOJOM_TRAITS_H_

#include "gpu/command_buffer/common/shared_image_pool_id.h"
#include "gpu/ipc/common/gpu_ipc_common_export.h"
#include "gpu/ipc/common/shared_image_pool_id.mojom.h"
#include "mojo/public/cpp/base/unguessable_token_mojom_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct GPU_IPC_COMMON_EXPORT StructTraits<gpu::mojom::SharedImagePoolIdDataView,
                                          gpu::SharedImagePoolId> {
  static const base::UnguessableToken& value(
      const gpu::SharedImagePoolId& input) {
    return input.GetToken();
  }

  static bool Read(gpu::mojom::SharedImagePoolIdDataView& input,
                   gpu::SharedImagePoolId* output);
};

}  // namespace mojo

#endif  // GPU_IPC_COMMON_SHARED_IMAGE_POOL_ID_MOJOM_TRAITS_H_
