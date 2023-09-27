// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/common/shared_image_capabilities_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<gpu::mojom::SharedImageCapabilitiesDataView,
                  gpu::SharedImageCapabilities>::
    Read(gpu::mojom::SharedImageCapabilitiesDataView data,
         gpu::SharedImageCapabilities* out) {
  out->supports_scanout_shared_images = data.supports_scanout_shared_images();
  return true;
}

}  // namespace mojo
