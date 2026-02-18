// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_CAPABILITIES_MOJOM_TRAITS_H_
#define GPU_IPC_COMMON_CAPABILITIES_MOJOM_TRAITS_H_

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/ipc/common/capabilities.mojom-shared.h"
#include "gpu/ipc/common/gpu_ipc_common_export.h"
#include "services/viz/public/cpp/compositing/shared_image_format_mojom_traits.h"

namespace mojo {

template <>
struct GPU_IPC_COMMON_EXPORT StructTraits<gpu::mojom::CapabilitiesDataView,
                                          gpu::Capabilities> {
  static int32_t max_texture_size(const gpu::Capabilities& cap) {
    return cap.max_texture_size;
  }
  static bool egl_image_external(const gpu::Capabilities& cap) {
    return cap.egl_image_external;
  }
  static bool texture_format_bgra8888(const gpu::Capabilities& cap) {
    return cap.texture_format_bgra8888;
  }
  static bool texture_format_etc1_npot(const gpu::Capabilities& cap) {
    return cap.texture_format_etc1_npot;
  }
  static bool disable_mac_swangle_rgbx(const gpu::Capabilities& cap) {
    return cap.disable_mac_swangle_rgbx;
  }
  static bool sync_query(const gpu::Capabilities& cap) {
    return cap.sync_query;
  }
  static bool texture_rg(const gpu::Capabilities& cap) {
    return cap.texture_rg;
  }
  static bool texture_norm16(const gpu::Capabilities& cap) {
    return cap.texture_norm16;
  }
  static bool texture_half_float_linear(const gpu::Capabilities& cap) {
    return cap.texture_half_float_linear;
  }
  static bool image_ar30(const gpu::Capabilities& cap) {
    return cap.image_ar30;
  }
  static bool image_ab30(const gpu::Capabilities& cap) {
    return cap.image_ab30;
  }
  static bool render_buffer_format_bgra8888(const gpu::Capabilities& cap) {
    return cap.render_buffer_format_bgra8888;
  }
  static bool msaa_is_slow(const gpu::Capabilities& cap) {
    return cap.msaa_is_slow;
  }
  static bool avoid_stencil_buffers(const gpu::Capabilities& cap) {
    return cap.avoid_stencil_buffers;
  }
  static bool supports_rgb_to_yuv_conversion(const gpu::Capabilities& cap) {
    return cap.supports_rgb_to_yuv_conversion;
  }
  static bool supports_yuv_readback(const gpu::Capabilities& cap) {
    return cap.supports_yuv_readback;
  }
  static bool mesa_framebuffer_flip_y(const gpu::Capabilities& cap) {
    return cap.mesa_framebuffer_flip_y;
  }
  static bool context_supports_distance_field_text(
      const gpu::Capabilities& cap) {
    return cap.context_supports_distance_field_text;
  }
  static bool using_vulkan_context(const gpu::Capabilities& cap) {
    return cap.using_vulkan_context;
  }
  static const base::flat_map<uint32_t, std::vector<uint64_t>>&
  drm_formats_and_modifiers(const gpu::Capabilities& cap) {
    return cap.drm_formats_and_modifiers;
  }
  static uint64_t drm_device_id(const gpu::Capabilities& cap) {
    return cap.drm_device_id;
  }

  static bool Read(gpu::mojom::CapabilitiesDataView data,
                   gpu::Capabilities* out);
};

template <>
struct GPU_IPC_COMMON_EXPORT StructTraits<
    gpu::mojom::ShaderPrecisionDataView,
    gpu::GLCapabilities::ShaderPrecision> {
  static int32_t min_range(const gpu::GLCapabilities::ShaderPrecision& sp) {
    return sp.min_range;
  }
  static int32_t max_range(const gpu::GLCapabilities::ShaderPrecision& sp) {
    return sp.max_range;
  }
  static int32_t precision(const gpu::GLCapabilities::ShaderPrecision& sp) {
    return sp.precision;
  }

  static bool Read(gpu::mojom::ShaderPrecisionDataView data,
                   gpu::GLCapabilities::ShaderPrecision* out);
};

template <>
struct GPU_IPC_COMMON_EXPORT StructTraits<
    gpu::mojom::PerStagePrecisionsDataView,
    gpu::GLCapabilities::PerStagePrecisions> {
  static const gpu::GLCapabilities::ShaderPrecision& low_int(
      const gpu::GLCapabilities::PerStagePrecisions& psp) {
    return psp.low_int;
  }
  static const gpu::GLCapabilities::ShaderPrecision& medium_int(
      const gpu::GLCapabilities::PerStagePrecisions& psp) {
    return psp.medium_int;
  }
  static const gpu::GLCapabilities::ShaderPrecision& high_int(
      const gpu::GLCapabilities::PerStagePrecisions& psp) {
    return psp.high_int;
  }
  static const gpu::GLCapabilities::ShaderPrecision& low_float(
      const gpu::GLCapabilities::PerStagePrecisions& psp) {
    return psp.low_float;
  }
  static const gpu::GLCapabilities::ShaderPrecision& medium_float(
      const gpu::GLCapabilities::PerStagePrecisions& psp) {
    return psp.medium_float;
  }
  static const gpu::GLCapabilities::ShaderPrecision& high_float(
      const gpu::GLCapabilities::PerStagePrecisions& psp) {
    return psp.high_float;
  }

  static bool Read(gpu::mojom::PerStagePrecisionsDataView data,
                   gpu::GLCapabilities::PerStagePrecisions* out);
};

template <>
struct GPU_IPC_COMMON_EXPORT StructTraits<gpu::mojom::GLCapabilitiesDataView,
                                          gpu::GLCapabilities> {
  static const gpu::GLCapabilities::PerStagePrecisions&
  vertex_shader_precisions(const gpu::GLCapabilities& cap) {
    return cap.vertex_shader_precisions;
  }
  static const gpu::GLCapabilities::PerStagePrecisions&
  fragment_shader_precisions(const gpu::GLCapabilities& cap) {
    return cap.fragment_shader_precisions;
  }
  static int32_t major_version(const gpu::GLCapabilities& cap) {
    return cap.major_version;
  }
  static int32_t minor_version(const gpu::GLCapabilities& cap) {
    return cap.minor_version;
  }
  static int32_t max_combined_texture_image_units(
      const gpu::GLCapabilities& cap) {
    return cap.max_combined_texture_image_units;
  }
  static int32_t max_cube_map_texture_size(const gpu::GLCapabilities& cap) {
    return cap.max_cube_map_texture_size;
  }
  static int32_t max_fragment_uniform_vectors(const gpu::GLCapabilities& cap) {
    return cap.max_fragment_uniform_vectors;
  }
  static int32_t max_renderbuffer_size(const gpu::GLCapabilities& cap) {
    return cap.max_renderbuffer_size;
  }
  static int32_t max_texture_image_units(const gpu::GLCapabilities& cap) {
    return cap.max_texture_image_units;
  }
  static int32_t max_varying_vectors(const gpu::GLCapabilities& cap) {
    return cap.max_varying_vectors;
  }
  static int32_t max_vertex_attribs(const gpu::GLCapabilities& cap) {
    return cap.max_vertex_attribs;
  }
  static int32_t max_vertex_texture_image_units(
      const gpu::GLCapabilities& cap) {
    return cap.max_vertex_texture_image_units;
  }
  static int32_t max_vertex_uniform_vectors(const gpu::GLCapabilities& cap) {
    return cap.max_vertex_uniform_vectors;
  }
  static int32_t max_viewport_width(const gpu::GLCapabilities& cap) {
    return cap.max_viewport_width;
  }
  static int32_t max_viewport_height(const gpu::GLCapabilities& cap) {
    return cap.max_viewport_height;
  }
  static int32_t num_compressed_texture_formats(
      const gpu::GLCapabilities& cap) {
    return cap.num_compressed_texture_formats;
  }
  static int32_t num_shader_binary_formats(const gpu::GLCapabilities& cap) {
    return cap.num_shader_binary_formats;
  }
  static int32_t max_3d_texture_size(const gpu::GLCapabilities& cap) {
    return cap.max_3d_texture_size;
  }
  static int32_t max_array_texture_layers(const gpu::GLCapabilities& cap) {
    return cap.max_array_texture_layers;
  }
  static int32_t max_color_attachments(const gpu::GLCapabilities& cap) {
    return cap.max_color_attachments;
  }
  static int64_t max_combined_fragment_uniform_components(
      const gpu::GLCapabilities& cap) {
    return cap.max_combined_fragment_uniform_components;
  }
  static int32_t max_combined_uniform_blocks(const gpu::GLCapabilities& cap) {
    return cap.max_combined_uniform_blocks;
  }
  static int64_t max_combined_vertex_uniform_components(
      const gpu::GLCapabilities& cap) {
    return cap.max_combined_vertex_uniform_components;
  }
  static int32_t max_draw_buffers(const gpu::GLCapabilities& cap) {
    return cap.max_draw_buffers;
  }
  static int64_t max_element_index(const gpu::GLCapabilities& cap) {
    return cap.max_element_index;
  }
  static int32_t max_elements_indices(const gpu::GLCapabilities& cap) {
    return cap.max_elements_indices;
  }
  static int32_t max_elements_vertices(const gpu::GLCapabilities& cap) {
    return cap.max_elements_vertices;
  }
  static int32_t max_fragment_input_components(const gpu::GLCapabilities& cap) {
    return cap.max_fragment_input_components;
  }
  static int32_t max_fragment_uniform_blocks(const gpu::GLCapabilities& cap) {
    return cap.max_fragment_uniform_blocks;
  }
  static int32_t max_fragment_uniform_components(
      const gpu::GLCapabilities& cap) {
    return cap.max_fragment_uniform_components;
  }
  static int32_t max_program_texel_offset(const gpu::GLCapabilities& cap) {
    return cap.max_program_texel_offset;
  }
  static int32_t max_samples(const gpu::GLCapabilities& cap) {
    return cap.max_samples;
  }
  static int64_t max_server_wait_timeout(const gpu::GLCapabilities& cap) {
    return cap.max_server_wait_timeout;
  }
  static float max_texture_lod_bias(const gpu::GLCapabilities& cap) {
    return cap.max_texture_lod_bias;
  }
  static int32_t max_transform_feedback_interleaved_components(
      const gpu::GLCapabilities& cap) {
    return cap.max_transform_feedback_interleaved_components;
  }
  static int32_t max_transform_feedback_separate_attribs(
      const gpu::GLCapabilities& cap) {
    return cap.max_transform_feedback_separate_attribs;
  }
  static int32_t max_transform_feedback_separate_components(
      const gpu::GLCapabilities& cap) {
    return cap.max_transform_feedback_separate_components;
  }
  static int64_t max_uniform_block_size(const gpu::GLCapabilities& cap) {
    return cap.max_uniform_block_size;
  }
  static int32_t max_uniform_buffer_bindings(const gpu::GLCapabilities& cap) {
    return cap.max_uniform_buffer_bindings;
  }
  static int32_t max_varying_components(const gpu::GLCapabilities& cap) {
    return cap.max_varying_components;
  }
  static int32_t max_vertex_output_components(const gpu::GLCapabilities& cap) {
    return cap.max_vertex_output_components;
  }
  static int32_t max_vertex_uniform_blocks(const gpu::GLCapabilities& cap) {
    return cap.max_vertex_uniform_blocks;
  }
  static int32_t max_vertex_uniform_components(const gpu::GLCapabilities& cap) {
    return cap.max_vertex_uniform_components;
  }
  static int32_t min_program_texel_offset(const gpu::GLCapabilities& cap) {
    return cap.min_program_texel_offset;
  }
  static int32_t num_program_binary_formats(const gpu::GLCapabilities& cap) {
    return cap.num_program_binary_formats;
  }
  static int32_t uniform_buffer_offset_alignment(
      const gpu::GLCapabilities& cap) {
    return cap.uniform_buffer_offset_alignment;
  }
  static bool occlusion_query_boolean(const gpu::GLCapabilities& cap) {
    return cap.occlusion_query_boolean;
  }
  static bool timer_queries(const gpu::GLCapabilities& cap) {
    return cap.timer_queries;
  }
  static int32_t max_texture_size(const gpu::GLCapabilities& cap) {
    return cap.max_texture_size;
  }
  static bool sync_query(const gpu::GLCapabilities& cap) {
    return cap.sync_query;
  }

  static bool Read(gpu::mojom::GLCapabilitiesDataView data,
                   gpu::GLCapabilities* out);
};

}  // namespace mojo

#endif  // GPU_IPC_COMMON_CAPABILITIES_MOJOM_TRAITS_H_
