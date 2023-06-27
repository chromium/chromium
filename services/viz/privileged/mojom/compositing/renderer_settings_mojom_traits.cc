// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/privileged/mojom/compositing/renderer_settings_mojom_traits.h"

#include "build/build_config.h"
#include "services/viz/public/cpp/compositing/resource_settings_mojom_traits.h"

#if BUILDFLAG(IS_ANDROID)
#include "ui/gfx/mojom/color_space_mojom_traits.h"
#endif

namespace mojo {

// static
bool StructTraits<viz::mojom::DebugRendererSettingsDataView,
                  viz::DebugRendererSettings>::
    Read(viz::mojom::DebugRendererSettingsDataView data,
         viz::DebugRendererSettings* out) {
  out->tint_composited_content = data.tint_composited_content();
  out->tint_composited_content_modulate =
      data.tint_composited_content_modulate();
  out->show_overdraw_feedback = data.show_overdraw_feedback();
  out->show_dc_layer_debug_borders = data.show_dc_layer_debug_borders();
  out->show_aggregated_damage = data.show_aggregated_damage();
  return true;
}

// static
bool StructTraits<viz::mojom::RendererSettingsDataView, viz::RendererSettings>::
    Read(viz::mojom::RendererSettingsDataView data,
         viz::RendererSettings* out) {
  out->allow_antialiasing = data.allow_antialiasing();
  out->force_antialiasing = data.force_antialiasing();
  out->force_blending_with_shaders = data.force_blending_with_shaders();
  out->partial_swap_enabled = data.partial_swap_enabled();
  out->should_clear_root_render_pass = data.should_clear_root_render_pass();
  out->release_overlay_resources_after_gpu_query =
      data.release_overlay_resources_after_gpu_query();
  out->highp_threshold_min = data.highp_threshold_min();
  out->slow_down_compositing_scale_factor =
      data.slow_down_compositing_scale_factor();
  out->auto_resize_output_surface = data.auto_resize_output_surface();
  out->requires_alpha_channel = data.requires_alpha_channel();

#if BUILDFLAG(IS_ANDROID)
  if (!data.ReadInitialScreenSize(&out->initial_screen_size))
    return false;

  if (!data.ReadColorSpace(&out->color_space))
    return false;
#endif

#if BUILDFLAG(IS_OZONE)
  if (!data.ReadOverlayStrategies(&out->overlay_strategies))
    return false;
#endif

#if BUILDFLAG(IS_MAC)
  out->display_id = data.display_id();
#endif

  return true;
}

}  // namespace mojo
