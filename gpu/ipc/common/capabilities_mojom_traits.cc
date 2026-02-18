// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/common/capabilities_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<gpu::mojom::CapabilitiesDataView, gpu::Capabilities>::Read(
    gpu::mojom::CapabilitiesDataView data,
    gpu::Capabilities* out) {
  out->max_texture_size = data.max_texture_size();
  out->egl_image_external = data.egl_image_external();
  out->texture_format_bgra8888 = data.texture_format_bgra8888();
  out->texture_format_etc1_npot = data.texture_format_etc1_npot();
  out->disable_mac_swangle_rgbx = data.disable_mac_swangle_rgbx();
  out->sync_query = data.sync_query();
  out->texture_rg = data.texture_rg();
  out->texture_norm16 = data.texture_norm16();
  out->texture_half_float_linear = data.texture_half_float_linear();
  out->image_ar30 = data.image_ar30();
  out->image_ab30 = data.image_ab30();
  out->render_buffer_format_bgra8888 = data.render_buffer_format_bgra8888();
  out->msaa_is_slow = data.msaa_is_slow();
  out->avoid_stencil_buffers = data.avoid_stencil_buffers();
  out->supports_rgb_to_yuv_conversion = data.supports_rgb_to_yuv_conversion();
  out->supports_yuv_readback = data.supports_yuv_readback();
  out->mesa_framebuffer_flip_y = data.mesa_framebuffer_flip_y();
  out->context_supports_distance_field_text =
      data.context_supports_distance_field_text();
  out->using_vulkan_context = data.using_vulkan_context();
  if (!data.ReadDrmFormatsAndModifiers(&out->drm_formats_and_modifiers)) {
    return false;
  }
  out->drm_device_id = data.drm_device_id();

  return true;
}

// static
bool StructTraits<gpu::mojom::ShaderPrecisionDataView,
                  gpu::GLCapabilities::ShaderPrecision>::
    Read(gpu::mojom::ShaderPrecisionDataView data,
         gpu::GLCapabilities::ShaderPrecision* out) {
  out->min_range = data.min_range();
  out->max_range = data.max_range();
  out->precision = data.precision();
  return true;
}

// static
bool StructTraits<gpu::mojom::PerStagePrecisionsDataView,
                  gpu::GLCapabilities::PerStagePrecisions>::
    Read(gpu::mojom::PerStagePrecisionsDataView data,
         gpu::GLCapabilities::PerStagePrecisions* out) {
  return data.ReadLowInt(&out->low_int) &&
         data.ReadMediumInt(&out->medium_int) &&
         data.ReadHighInt(&out->high_int) &&
         data.ReadLowFloat(&out->low_float) &&
         data.ReadMediumFloat(&out->medium_float) &&
         data.ReadHighFloat(&out->high_float);
}

// static
bool StructTraits<gpu::mojom::GLCapabilitiesDataView, gpu::GLCapabilities>::
    Read(gpu::mojom::GLCapabilitiesDataView data, gpu::GLCapabilities* out) {
  if (!data.ReadVertexShaderPrecisions(&out->vertex_shader_precisions) ||
      !data.ReadFragmentShaderPrecisions(&out->fragment_shader_precisions)) {
    return false;
  }
  out->major_version = data.major_version();
  out->minor_version = data.minor_version();
  out->max_combined_texture_image_units =
      data.max_combined_texture_image_units();
  out->max_cube_map_texture_size = data.max_cube_map_texture_size();
  out->max_fragment_uniform_vectors = data.max_fragment_uniform_vectors();
  out->max_renderbuffer_size = data.max_renderbuffer_size();
  out->max_texture_image_units = data.max_texture_image_units();
  out->max_varying_vectors = data.max_varying_vectors();
  out->max_vertex_attribs = data.max_vertex_attribs();
  out->max_vertex_texture_image_units = data.max_vertex_texture_image_units();
  out->max_vertex_uniform_vectors = data.max_vertex_uniform_vectors();
  out->max_viewport_width = data.max_viewport_width();
  out->max_viewport_height = data.max_viewport_height();
  out->num_compressed_texture_formats = data.num_compressed_texture_formats();
  out->num_shader_binary_formats = data.num_shader_binary_formats();
  out->max_3d_texture_size = data.max_3d_texture_size();
  out->max_array_texture_layers = data.max_array_texture_layers();
  out->max_color_attachments = data.max_color_attachments();
  out->max_combined_fragment_uniform_components =
      data.max_combined_fragment_uniform_components();
  out->max_combined_uniform_blocks = data.max_combined_uniform_blocks();
  out->max_combined_vertex_uniform_components =
      data.max_combined_vertex_uniform_components();
  out->max_draw_buffers = data.max_draw_buffers();
  out->max_element_index = data.max_element_index();
  out->max_elements_indices = data.max_elements_indices();
  out->max_elements_vertices = data.max_elements_vertices();
  out->max_fragment_input_components = data.max_fragment_input_components();
  out->max_fragment_uniform_blocks = data.max_fragment_uniform_blocks();
  out->max_fragment_uniform_components = data.max_fragment_uniform_components();
  out->max_program_texel_offset = data.max_program_texel_offset();
  out->max_samples = data.max_samples();
  out->max_server_wait_timeout = data.max_server_wait_timeout();
  out->max_texture_lod_bias = data.max_texture_lod_bias();
  out->max_transform_feedback_interleaved_components =
      data.max_transform_feedback_interleaved_components();
  out->max_transform_feedback_separate_attribs =
      data.max_transform_feedback_separate_attribs();
  out->max_transform_feedback_separate_components =
      data.max_transform_feedback_separate_components();
  out->max_uniform_block_size = data.max_uniform_block_size();
  out->max_uniform_buffer_bindings = data.max_uniform_buffer_bindings();
  out->max_varying_components = data.max_varying_components();
  out->max_vertex_output_components = data.max_vertex_output_components();
  out->max_vertex_uniform_blocks = data.max_vertex_uniform_blocks();
  out->max_vertex_uniform_components = data.max_vertex_uniform_components();
  out->min_program_texel_offset = data.min_program_texel_offset();
  out->num_program_binary_formats = data.num_program_binary_formats();
  out->uniform_buffer_offset_alignment = data.uniform_buffer_offset_alignment();
  out->occlusion_query_boolean = data.occlusion_query_boolean();
  out->timer_queries = data.timer_queries();
  out->max_texture_size = data.max_texture_size();
  out->sync_query = data.sync_query();
  return true;
}

}  // namespace mojo
