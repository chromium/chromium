// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/common/shared_image_pool_id_mojom_traits.h"

namespace mojo {

bool StructTraits<
    gpu::mojom::SharedImagePoolIdDataView,
    gpu::SharedImagePoolId>::Read(gpu::mojom::SharedImagePoolIdDataView& input,
                                  gpu::SharedImagePoolId* output) {
  base::UnguessableToken token;
  if (!input.ReadValue(&token)) {
    return false;
  }
  *output = gpu::SharedImagePoolId(token);
  return true;
}

}  // namespace mojo
