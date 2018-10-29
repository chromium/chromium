// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/privileged/interfaces/compositing/renderer_settings_struct_traits.h"
#include "services/viz/public/cpp/compositing/resource_settings_struct_traits.h"

namespace mojo {

// static
bool StructTraits<viz::mojom::RendererSettingsDataView, viz::RendererSettings>::
    Read(viz::mojom::RendererSettingsDataView data,
         viz::RendererSettings* out) {
  bool success = true;

  out->allow_antialiasing = data.allow_antialiasing();
  out->force_antialiasing = data.force_antialiasing();
  out->force_blending_with_shaders = data.force_blending_with_shaders();
  out->partial_swap_enabled = data.partial_swap_enabled();
  out->finish_rendering_on_resize = data.finish_rendering_on_resize();
  out->should_clear_root_render_pass = data.should_clear_root_render_pass();
  out->release_overlay_resources_after_gpu_query =
      data.release_overlay_resources_after_gpu_query();
  out->tint_gl_composited_content = data.tint_gl_composited_content();
  out->show_overdraw_feedback = data.show_overdraw_feedback();
  out->enable_draw_occlusion = data.enable_draw_occlusion();
  out->highp_threshold_min = data.highp_threshold_min();
  out->slow_down_compositing_scale_factor =
      data.slow_down_compositing_scale_factor();
  out->use_skia_renderer = data.use_skia_renderer();
  out->record_sk_picture = data.record_sk_picture();
  out->use_skia_deferred_display_list = data.use_skia_deferred_display_list();
  out->allow_overlays = data.allow_overlays();
  out->requires_alpha_channel = data.requires_alpha_channel();

#if defined(OS_ANDROID)
  success = data.ReadInitialScreenSize(&out->initial_screen_size);
#endif

  return success;
}

}  // namespace mojo
