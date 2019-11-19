// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_CAPABILITIES_H_
#define GPU_COMMAND_BUFFER_COMMON_CAPABILITIES_H_

#include <stdint.h>
#include <vector>

#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/gpu_export.h"
#include "ui/gfx/buffer_types.h"

// From gl2.h. We want to avoid including gl headers because client-side and
// service-side headers conflict.
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_LOW_FLOAT 0x8DF0
#define GL_MEDIUM_FLOAT 0x8DF1
#define GL_HIGH_FLOAT 0x8DF2
#define GL_LOW_INT 0x8DF3
#define GL_MEDIUM_INT 0x8DF4
#define GL_HIGH_INT 0x8DF5

namespace gpu {

// NOTE: When adding members to this struct, also add corresponding
// entries in gpu/ipc/common/gpu_command_buffer_traits_multi.h.

struct GPU_EXPORT Capabilities {
  struct ShaderPrecision {
    ShaderPrecision() : min_range(0), max_range(0), precision(0) {}
    int min_range;
    int max_range;
    int precision;
  };

  struct GPU_EXPORT PerStagePrecisions {
    PerStagePrecisions();

    ShaderPrecision low_int;
    ShaderPrecision medium_int;
    ShaderPrecision high_int;
    ShaderPrecision low_float;
    ShaderPrecision medium_float;
    ShaderPrecision high_float;
  };

  Capabilities();
  Capabilities(const Capabilities& other);
  ~Capabilities();

  template <typename T>
  void VisitStagePrecisions(unsigned stage,
                            PerStagePrecisions* precisions,
                            const T& visitor) {
    visitor(stage, GL_LOW_INT, &precisions->low_int);
    visitor(stage, GL_MEDIUM_INT, &precisions->medium_int);
    visitor(stage, GL_HIGH_INT, &precisions->high_int);
    visitor(stage, GL_LOW_FLOAT, &precisions->low_float);
    visitor(stage, GL_MEDIUM_FLOAT, &precisions->medium_float);
    visitor(stage, GL_HIGH_FLOAT, &precisions->high_float);
  }

  template <typename T>
  void VisitPrecisions(const T& visitor) {
    VisitStagePrecisions(GL_VERTEX_SHADER, &vertex_shader_precisions, visitor);
    VisitStagePrecisions(GL_FRAGMENT_SHADER, &fragment_shader_precisions,
                         visitor);
  }

  PerStagePrecisions vertex_shader_precisions;
  PerStagePrecisions fragment_shader_precisions;
  int max_combined_texture_image_units = 0;
  int max_cube_map_texture_size = 0;
  int max_fragment_uniform_vectors = 0;
  int max_renderbuffer_size = 0;
  int max_texture_image_units = 0;
  int max_texture_size = 0;
  int max_varying_vectors = 0;
  int max_vertex_attribs = 0;
  int max_vertex_texture_image_units = 0;
  int max_vertex_uniform_vectors = 0;
  // MAX_VIEWPORT_DIMS[2]
  int max_viewport_width = 0;
  int max_viewport_height = 0;
  int num_compressed_texture_formats = 0;
  int num_shader_binary_formats = 0;
  int num_stencil_bits = 0;  // For the default framebuffer.
  int bind_generates_resource_chromium = 0;

  int max_3d_texture_size = 0;
  int max_array_texture_layers = 0;
  int max_color_attachments = 0;
  int64_t max_combined_fragment_uniform_components = 0;
  int max_combined_uniform_blocks = 0;
  int64_t max_combined_vertex_uniform_components = 0;
  int max_copy_texture_chromium_size = 0;
  int max_draw_buffers = 0;
  int64_t max_element_index = 0;
  int max_elements_indices = 0;
  int max_elements_vertices = 0;
  int max_fragment_input_components = 0;
  int max_fragment_uniform_blocks = 0;
  int max_fragment_uniform_components = 0;
  int max_program_texel_offset = 0;
  int max_samples = 0;
  int64_t max_server_wait_timeout = 0;
  float max_texture_lod_bias = 0.f;
  int max_transform_feedback_interleaved_components = 0;
  int max_transform_feedback_separate_attribs = 0;
  int max_transform_feedback_separate_components = 0;
  int64_t max_uniform_block_size = 0;
  int max_uniform_buffer_bindings = 0;
  int max_atomic_counter_buffer_bindings = 0;
  int max_shader_storage_buffer_bindings = 0;
  int shader_storage_buffer_offset_alignment = 1;
  int max_varying_components = 0;
  int max_vertex_output_components = 0;
  int max_vertex_uniform_blocks = 0;
  int max_vertex_uniform_components = 0;
  int min_program_texel_offset = 0;
  int num_extensions = 0;
  int num_program_binary_formats = 0;
  int uniform_buffer_offset_alignment = 1;
  // Describes how many buffers a surface uses in the swap chain. Default to 2
  // since double buffering is the default in most cases.
  int num_surface_buffers = 2;

  bool post_sub_buffer = false;
  bool swap_buffers_with_bounds = false;
  bool commit_overlay_planes = false;
  bool egl_image_external = false;
  bool texture_format_astc = false;
  bool texture_format_atc = false;
  bool texture_format_bgra8888 = false;
  bool texture_format_dxt1 = false;
  bool texture_format_dxt5 = false;
  bool texture_format_etc1 = false;
  bool texture_format_etc1_npot = false;
  bool texture_rectangle = false;
  bool iosurface = false;
  bool texture_usage = false;
  bool texture_storage = false;
  bool discard_framebuffer = false;
  bool sync_query = false;
  bool blend_equation_advanced = false;
  bool blend_equation_advanced_coherent = false;
  bool texture_rg = false;
  bool texture_norm16 = false;
  bool texture_half_float_linear = false;
  bool color_buffer_half_float_rgba = false;
  bool image_ycbcr_422 = false;
  bool image_ycbcr_420v = false;
  bool image_ycbcr_420v_disabled_for_video_frames = false;
  bool image_xr30 = false;
  bool image_xb30 = false;
  bool image_ycbcr_p010 = false;
  bool render_buffer_format_bgra8888 = false;
  bool occlusion_query = false;
  bool occlusion_query_boolean = false;
  bool timer_queries = false;
  bool surfaceless = false;
  bool flips_vertically = false;
  bool msaa_is_slow = false;
  bool disable_one_component_textures = false;
  bool gpu_rasterization = false;
  bool avoid_stencil_buffers = false;
  bool multisample_compatibility = false;
  // True if DirectComposition layers are enabled.
  bool dc_layers = false;
  bool use_dc_overlays_for_video = false;
  bool protected_video_swap_chain = false;
  bool gpu_vsync = false;
  bool shared_image_swap_chain = false;

  // When this parameter is true, a CHROMIUM image created with RGB format will
  // actually have RGBA format. The client is responsible for handling most of
  // the complexities associated with this. See
  // gpu/GLES2/extensions/CHROMIUM/CHROMIUM_image.txt for more
  // details.
  bool chromium_image_rgb_emulation = false;

  // When true, non-empty post sub buffer calls are unsupported.
  bool disable_non_empty_post_sub_buffers = false;

  bool disable_2d_canvas_copy_on_write = false;

  bool texture_npot = false;

  bool texture_storage_image = false;

  bool supports_oop_raster = false;

  bool chromium_gpu_fence = false;

  bool unpremultiply_and_dither_copy = false;

  bool separate_stencil_ref_mask_writemask = false;

  bool use_gpu_fences_for_overlay_planes = false;

  bool chromium_nonblocking_readback = false;

  bool mesa_framebuffer_flip_y = false;

  int major_version = 2;
  int minor_version = 0;

  // Used by OOP raster.
  bool context_supports_distance_field_text = true;
  uint64_t glyph_cache_max_texture_bytes = 0.f;

  GpuMemoryBufferFormatSet gpu_memory_buffer_formats = {
      gfx::BufferFormat::BGR_565,   gfx::BufferFormat::RGBA_4444,
      gfx::BufferFormat::RGBA_8888, gfx::BufferFormat::RGBX_8888,
      gfx::BufferFormat::YVU_420,
  };
  std::vector<gfx::BufferUsageAndFormat> texture_target_exception_list;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_CAPABILITIES_H_
