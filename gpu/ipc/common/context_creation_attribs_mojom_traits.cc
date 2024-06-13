// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/common/context_creation_attribs_mojom_traits.h"

#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"
#include "ui/gl/mojom/gpu_preference_mojom_traits.h"

namespace mojo {

bool StructTraits<gpu::mojom::ContextCreationAttribsDataView,
                  gpu::ContextCreationAttribs>::
    Read(gpu::mojom::ContextCreationAttribsDataView data,
         gpu::ContextCreationAttribs* out) {
  if (!data.ReadGpuPreference(&out->gpu_preference) ||
      !data.ReadContextType(&out->context_type)) {
    return false;
  }

  out->bind_generates_resource = data.bind_generates_resource();
  out->fail_if_major_perf_caveat = data.fail_if_major_perf_caveat();
  out->lose_context_when_out_of_memory = data.lose_context_when_out_of_memory();
  out->enable_gles2_interface = data.enable_gles2_interface();
  out->enable_grcontext = data.enable_grcontext();
  out->enable_raster_interface = data.enable_raster_interface();
  out->enable_oop_rasterization = data.enable_oop_rasterization();
  return true;
}

}  // namespace mojo
