// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "build/build_config.h"
#include "components/viz/common/buildflags.h"
#include "components/viz/common/display/renderer_settings.h"
#include "services/viz/privileged/mojom/compositing/renderer_settings.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"
#include "ui/gfx/mojom/color_space_mojom_traits.h"

namespace viz {

namespace {

#if BUILDFLAG(IS_OZONE)
auto AnyOverlayStrategy() {
  return fuzztest::ElementOf({OverlayStrategy::kFullscreen,
                              OverlayStrategy::kSingleOnTop,
                              OverlayStrategy::kUnderlay
#if BUILDFLAG(ENABLE_CAST_OVERLAY_STRATEGY)
                              ,
                              OverlayStrategy::kUnderlayCast
#endif
  });
}
#endif

auto AnyOcclusionCullerSettings() {
  return fuzztest::Map(
      [](int maximum_occluder_complexity, int quad_split_limit,
         int minimum_fragments_reduced, int occluder_minium_visible_quad_size,
         bool generate_complex_occluder_for_rounded_corners,
         int minumum_quad_size_with_rounded_corners) {
        RendererSettings::OcclusionCullerSettings settings;
        settings.maximum_occluder_complexity = maximum_occluder_complexity;
        settings.quad_split_limit = quad_split_limit;
        settings.minimum_fragments_reduced = minimum_fragments_reduced;
        settings.occluder_minium_visible_quad_size =
            occluder_minium_visible_quad_size;
        settings.generate_complex_occluder_for_rounded_corners =
            generate_complex_occluder_for_rounded_corners;
        settings.minumum_quad_size_with_rounded_corners =
            minumum_quad_size_with_rounded_corners;
        return settings;
      },
      fuzztest::Arbitrary<int>(), fuzztest::Arbitrary<int>(),
      fuzztest::Arbitrary<int>(), fuzztest::Arbitrary<int>(),
      fuzztest::Arbitrary<bool>(), fuzztest::Arbitrary<int>());
}

auto AnyRendererSettings() {
  return fuzztest::Map(
      [](bool allow_antialiasing, bool force_antialiasing,
         bool force_blending_with_shaders, bool partial_swap_enabled,
         bool should_clear_root_render_pass,
         bool release_overlay_resources_after_gpu_query,
         bool dont_round_texture_sizes_for_pixel_tests, int highp_threshold_min,
         bool auto_resize_output_surface, bool requires_alpha_channel,
         bool disable_render_pass_bypassing,
         bool force_non_scanout_backing_for_pixel_tests,
         int slow_down_compositing_scale_factor,
         RendererSettings::OcclusionCullerSettings occlusion_culler_settings
#if BUILDFLAG(IS_OZONE)
         ,
         const std::vector<OverlayStrategy>& overlay_strategies
#endif
#if BUILDFLAG(IS_MAC)
         ,
         int64_t display_id
#endif
      ) {
        RendererSettings settings;
        settings.allow_antialiasing = allow_antialiasing;
        settings.force_antialiasing = force_antialiasing;
        settings.force_blending_with_shaders = force_blending_with_shaders;
        settings.partial_swap_enabled = partial_swap_enabled;
        settings.should_clear_root_render_pass = should_clear_root_render_pass;
        settings.release_overlay_resources_after_gpu_query =
            release_overlay_resources_after_gpu_query;
        settings.dont_round_texture_sizes_for_pixel_tests =
            dont_round_texture_sizes_for_pixel_tests;
        settings.highp_threshold_min = highp_threshold_min;
        settings.auto_resize_output_surface = auto_resize_output_surface;
        settings.requires_alpha_channel = requires_alpha_channel;
        settings.disable_render_pass_bypassing = disable_render_pass_bypassing;
        settings.force_non_scanout_backing_for_pixel_tests =
            force_non_scanout_backing_for_pixel_tests;
        settings.slow_down_compositing_scale_factor =
            slow_down_compositing_scale_factor;
        settings.occlusion_culler_settings = occlusion_culler_settings;
#if BUILDFLAG(IS_OZONE)
        settings.overlay_strategies = overlay_strategies;
#endif
#if BUILDFLAG(IS_MAC)
        settings.display_id = display_id;
#endif
        return settings;
      },
      fuzztest::Arbitrary<bool>(), fuzztest::Arbitrary<bool>(),
      fuzztest::Arbitrary<bool>(), fuzztest::Arbitrary<bool>(),
      fuzztest::Arbitrary<bool>(), fuzztest::Arbitrary<bool>(),
      fuzztest::Arbitrary<bool>(), fuzztest::Arbitrary<int>(),
      fuzztest::Arbitrary<bool>(), fuzztest::Arbitrary<bool>(),
      fuzztest::Arbitrary<bool>(), fuzztest::Arbitrary<bool>(),
      fuzztest::Arbitrary<int>(), AnyOcclusionCullerSettings()
#if BUILDFLAG(IS_OZONE)
                                      ,
      fuzztest::VectorOf(AnyOverlayStrategy())
#endif
#if BUILDFLAG(IS_MAC)
          ,
      fuzztest::Arbitrary<int64_t>()
#endif
  );
}

auto AnyDebugRendererSettings() {
  return fuzztest::Map(
      [](bool tint_composited_content, bool tint_composited_content_modulate,
         bool show_overdraw_feedback, bool show_dc_layer_debug_borders,
         bool show_aggregated_damage) {
        DebugRendererSettings settings;
        settings.tint_composited_content = tint_composited_content;
        settings.tint_composited_content_modulate =
            tint_composited_content_modulate;
        settings.show_overdraw_feedback = show_overdraw_feedback;
        settings.show_dc_layer_debug_borders = show_dc_layer_debug_borders;
        settings.show_aggregated_damage = show_aggregated_damage;
        return settings;
      },
      fuzztest::Arbitrary<bool>(), fuzztest::Arbitrary<bool>(),
      fuzztest::Arbitrary<bool>(), fuzztest::Arbitrary<bool>(),
      fuzztest::Arbitrary<bool>());
}

}  // namespace

void RendererSettingsFuzz(const RendererSettings& input) {
  RendererSettings output;
  mojom::RendererSettings::Deserialize(
      mojom::RendererSettings::Serialize(&input), &output);
}
FUZZ_TEST(StructTraitsTest, RendererSettingsFuzz)
    .WithDomains(AnyRendererSettings());

void DebugRendererSettingsFuzz(const DebugRendererSettings& input) {
  DebugRendererSettings output;
  mojom::DebugRendererSettings::Deserialize(
      mojom::DebugRendererSettings::Serialize(&input), &output);
}
FUZZ_TEST(StructTraitsTest, DebugRendererSettingsFuzz)
    .WithDomains(AnyDebugRendererSettings());

namespace {

using StructTraitsTest = testing::Test;

TEST_F(StructTraitsTest, RendererSettings) {
  RendererSettings input;

  // Set |input| to non-default values.
  input.allow_antialiasing = false;
  input.force_antialiasing = true;
  input.force_blending_with_shaders = true;
  input.partial_swap_enabled = true;
  input.should_clear_root_render_pass = false;
  input.release_overlay_resources_after_gpu_query = true;
  input.highp_threshold_min = -1;
  input.occlusion_culler_settings.quad_split_limit = 10;
  input.occlusion_culler_settings.maximum_occluder_complexity = 1;
  input.occlusion_culler_settings.minimum_fragments_reduced = 100;
  input.occlusion_culler_settings
      .generate_complex_occluder_for_rounded_corners = true;

  RendererSettings output;
  mojom::RendererSettings::Deserialize(
      mojom::RendererSettings::Serialize(&input), &output);
  EXPECT_EQ(input.allow_antialiasing, output.allow_antialiasing);
  EXPECT_EQ(input.force_antialiasing, output.force_antialiasing);
  EXPECT_EQ(input.force_blending_with_shaders,
            output.force_blending_with_shaders);
  EXPECT_EQ(input.partial_swap_enabled, output.partial_swap_enabled);
  EXPECT_EQ(input.should_clear_root_render_pass,
            output.should_clear_root_render_pass);
  EXPECT_EQ(input.release_overlay_resources_after_gpu_query,
            output.release_overlay_resources_after_gpu_query);
  EXPECT_EQ(input.highp_threshold_min, output.highp_threshold_min);
  EXPECT_EQ(input.occlusion_culler_settings.quad_split_limit,
            output.occlusion_culler_settings.quad_split_limit);
  EXPECT_EQ(input.occlusion_culler_settings.maximum_occluder_complexity,
            output.occlusion_culler_settings.maximum_occluder_complexity);
  EXPECT_EQ(input.occlusion_culler_settings.minimum_fragments_reduced,
            output.occlusion_culler_settings.minimum_fragments_reduced);
  EXPECT_EQ(input.occlusion_culler_settings
                .generate_complex_occluder_for_rounded_corners,
            output.occlusion_culler_settings
                .generate_complex_occluder_for_rounded_corners);
}

TEST_F(StructTraitsTest, DebugRendererSettings) {
  DebugRendererSettings input;

  // Set |input| to non-default values.
  input.show_overdraw_feedback = true;
  input.tint_composited_content = true;
  input.show_dc_layer_debug_borders = true;
  input.tint_composited_content_modulate = true;

  DebugRendererSettings output;
  mojom::DebugRendererSettings::Deserialize(
      mojom::DebugRendererSettings::Serialize(&input), &output);
  EXPECT_EQ(input.show_overdraw_feedback, output.show_overdraw_feedback);
  EXPECT_EQ(input.tint_composited_content, output.tint_composited_content);
  EXPECT_EQ(input.tint_composited_content_modulate,
            output.tint_composited_content_modulate);
  EXPECT_EQ(input.show_dc_layer_debug_borders,
            output.show_dc_layer_debug_borders);
}

}  // namespace

}  // namespace viz
