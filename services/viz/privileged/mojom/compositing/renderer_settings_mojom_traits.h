// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef SERVICES_VIZ_PRIVILEGED_MOJOM_COMPOSITING_RENDERER_SETTINGS_MOJOM_TRAITS_H_
#define SERVICES_VIZ_PRIVILEGED_MOJOM_COMPOSITING_RENDERER_SETTINGS_MOJOM_TRAITS_H_

#include <vector>

#include "build/build_config.h"
#include "components/viz/common/display/renderer_settings.h"
#include "services/viz/privileged/cpp/overlay_strategy_mojom_traits.h"
#include "services/viz/privileged/mojom/compositing/renderer_settings.mojom-shared.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"

#if BUILDFLAG(IS_OZONE)
#include "components/viz/common/display/overlay_strategy.h"
#endif

namespace mojo {
template <>
struct StructTraits<viz::mojom::DebugRendererSettingsDataView,
                    viz::DebugRendererSettings> {
  static bool tint_composited_content(const viz::DebugRendererSettings& input) {
    return input.tint_composited_content;
  }

  static bool tint_composited_content_modulate(
      const viz::DebugRendererSettings& input) {
    return input.tint_composited_content_modulate;
  }

  static bool show_overdraw_feedback(const viz::DebugRendererSettings& input) {
    return input.show_overdraw_feedback;
  }

  static bool show_dc_layer_debug_borders(
      const viz::DebugRendererSettings& input) {
    return input.show_dc_layer_debug_borders;
  }

  static bool show_aggregated_damage(const viz::DebugRendererSettings& input) {
    return input.show_aggregated_damage;
  }

  static bool Read(viz::mojom::DebugRendererSettingsDataView data,
                   viz::DebugRendererSettings* out);
};

template <>
struct StructTraits<viz::mojom::RendererSettingsDataView,
                    viz::RendererSettings> {
  static bool apply_simple_frame_rate_throttling(
      const viz::RendererSettings& input) {
    return input.apply_simple_frame_rate_throttling;
  }

  static bool allow_antialiasing(const viz::RendererSettings& input) {
    return input.allow_antialiasing;
  }

  static bool force_antialiasing(const viz::RendererSettings& input) {
    return input.force_antialiasing;
  }

  static bool force_blending_with_shaders(const viz::RendererSettings& input) {
    return input.force_blending_with_shaders;
  }

  static bool partial_swap_enabled(const viz::RendererSettings& input) {
    return input.partial_swap_enabled;
  }

  static bool should_clear_root_render_pass(
      const viz::RendererSettings& input) {
    return input.should_clear_root_render_pass;
  }

  static bool release_overlay_resources_after_gpu_query(
      const viz::RendererSettings& input) {
    return input.release_overlay_resources_after_gpu_query;
  }

  static int highp_threshold_min(const viz::RendererSettings& input) {
    return input.highp_threshold_min;
  }

  static int slow_down_compositing_scale_factor(
      const viz::RendererSettings& input) {
    return input.slow_down_compositing_scale_factor;
  }

  static bool auto_resize_output_surface(const viz::RendererSettings& input) {
    return input.auto_resize_output_surface;
  }

  static bool requires_alpha_channel(const viz::RendererSettings& input) {
    return input.requires_alpha_channel;
  }

#if BUILDFLAG(IS_ANDROID)
  static gfx::Size initial_screen_size(const viz::RendererSettings& input) {
    return input.initial_screen_size;
  }

  static gfx::ColorSpace color_space(const viz::RendererSettings& input) {
    return input.color_space;
  }
#endif

#if BUILDFLAG(IS_OZONE)
  static std::vector<viz::OverlayStrategy> overlay_strategies(
      const viz::RendererSettings& input) {
    return input.overlay_strategies;
  }
#endif

  static bool Read(viz::mojom::RendererSettingsDataView data,
                   viz::RendererSettings* out);
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PRIVILEGED_MOJOM_COMPOSITING_RENDERER_SETTINGS_MOJOM_TRAITS_H_
