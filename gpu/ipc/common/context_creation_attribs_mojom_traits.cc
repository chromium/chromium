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
  if (!data.ReadOffscreenFramebufferSize(&out->offscreen_framebuffer_size) ||
      !data.ReadGpuPreference(&out->gpu_preference) ||
      !data.ReadContextType(&out->context_type) ||
      !data.ReadColorSpace(&out->color_space)) {
    return false;
  }
  out->alpha_size = data.alpha_size();
  out->blue_size = data.blue_size();
  out->green_size = data.green_size();
  out->red_size = data.red_size();
  out->depth_size = data.depth_size();
  out->stencil_size = data.stencil_size();
  out->samples = data.samples();
  out->sample_buffers = data.sample_buffers();
  out->buffer_preserved = data.buffer_preserved();
  out->bind_generates_resource = data.bind_generates_resource();
  out->fail_if_major_perf_caveat = data.fail_if_major_perf_caveat();
  out->lose_context_when_out_of_memory = data.lose_context_when_out_of_memory();
  out->should_use_native_gmb_for_backbuffer =
      data.should_use_native_gmb_for_backbuffer();
  out->own_offscreen_surface = data.own_offscreen_surface();
  out->single_buffer = data.single_buffer();
  out->enable_gles2_interface = data.enable_gles2_interface();
  out->enable_grcontext = data.enable_grcontext();
  out->enable_raster_interface = data.enable_raster_interface();
  out->enable_oop_rasterization = data.enable_oop_rasterization();
  out->enable_swap_timestamps_if_supported =
      data.enable_swap_timestamps_if_supported();
  return true;
}

}  // namespace mojo
