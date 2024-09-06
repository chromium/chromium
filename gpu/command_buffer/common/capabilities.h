// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_CAPABILITIES_H_
#define GPU_COMMAND_BUFFER_COMMON_CAPABILITIES_H_

#include <stdint.h>
#include <vector>

#include "base/containers/flat_map.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/gpu_export.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/surface_origin.h"

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
  Capabilities();
  Capabilities(const Capabilities& other);
  ~Capabilities();

  // Note this may be smaller than GL_MAX_TEXTURE_SIZE for a GLES context.
  int max_texture_size = 0;
  int max_copy_texture_chromium_size = 0;
  bool egl_image_external = false;
  bool egl_image_external_essl3 = false;
  bool texture_format_bgra8888 = false;
  bool texture_format_etc1_npot = false;
  bool sync_query = false;
  bool texture_rg = false;
  bool texture_norm16 = false;
  bool texture_half_float_linear = false;
  bool image_ycbcr_420v = false;
  bool image_ar30 = false;
  bool image_ab30 = false;
  bool image_ycbcr_p010 = false;
  bool render_buffer_format_bgra8888 = false;
  bool msaa_is_slow = false;
  bool disable_one_component_textures = false;
  bool gpu_rasterization = false;
  bool avoid_stencil_buffers = false;
  bool angle_rgbx_internal_format = false;

  bool disable_2d_canvas_copy_on_write = false;

  bool supports_rgb_to_yuv_conversion = false;
  bool supports_yuv_readback = false;

  bool chromium_gpu_fence = false;

  bool mesa_framebuffer_flip_y = false;

  // Used by OOP raster.
  bool context_supports_distance_field_text = true;

  bool using_vulkan_context = false;

  GpuMemoryBufferFormatSet gpu_memory_buffer_formats = {
      gfx::BufferFormat::BGR_565,   gfx::BufferFormat::RGBA_4444,
      gfx::BufferFormat::RGBA_8888, gfx::BufferFormat::RGBX_8888,
      gfx::BufferFormat::YVU_420,
  };

  base::flat_map<uint32_t, std::vector<uint64_t>> drm_formats_and_modifiers;
  uint64_t drm_device_id = 0;
};

struct GPU_EXPORT GLCapabilities {
  GLCapabilities();
  GLCapabilities(const GLCapabilities& other);
  ~GLCapabilities();

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

  int major_version = 2;
  int minor_version = 0;

  int max_combined_texture_image_units = 0;
  int max_cube_map_texture_size = 0;
  int max_fragment_uniform_vectors = 0;
  int max_renderbuffer_size = 0;
  int max_texture_image_units = 0;
  int max_varying_vectors = 0;
  int max_vertex_attribs = 0;
  int max_vertex_texture_image_units = 0;
  int max_vertex_uniform_vectors = 0;
  // MAX_VIEWPORT_DIMS[2]
  int max_viewport_width = 0;
  int max_viewport_height = 0;
  int num_compressed_texture_formats = 0;
  int num_shader_binary_formats = 0;
  int bind_generates_resource_chromium = 0;

  int max_3d_texture_size = 0;
  int max_array_texture_layers = 0;
  int max_color_attachments = 0;
  int64_t max_combined_fragment_uniform_components = 0;
  int max_combined_uniform_blocks = 0;
  int64_t max_combined_vertex_uniform_components = 0;
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
  int num_program_binary_formats = 0;
  int uniform_buffer_offset_alignment = 1;

  bool occlusion_query_boolean = false;
  bool timer_queries = false;

  // Note this may be smaller than GL_MAX_TEXTURE_SIZE for a GLES context.
  int max_texture_size = 0;
  bool sync_query = false;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_CAPABILITIES_H_
