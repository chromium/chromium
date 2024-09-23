// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "components/viz/common/display/renderer_settings.h"
#include "services/viz/privileged/mojom/compositing/renderer_settings.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/mojom/color_space_mojom_traits.h"

namespace viz {

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
