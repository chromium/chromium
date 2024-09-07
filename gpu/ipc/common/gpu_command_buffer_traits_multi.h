// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// no-include-guard-because-multiply-included
// Multiply-included message file, hence no include guard here.

#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/context_creation_attribs.h"
#include "gpu/gpu_export.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/param_traits_macros.h"
#include "ui/gfx/ipc/buffer_types/gfx_param_traits.h"
#include "ui/gfx/ipc/geometry/gfx_param_traits.h"
#include "ui/gl/gpu_preference.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT GPU_EXPORT

IPC_ENUM_TRAITS_MAX_VALUE(gpu::error::Error, gpu::error::kErrorLast)
IPC_ENUM_TRAITS_MAX_VALUE(gpu::error::ContextLostReason,
                          gpu::error::kContextLostReasonLast)
IPC_ENUM_TRAITS_MIN_MAX_VALUE(
    gpu::CommandBufferNamespace,
    gpu::CommandBufferNamespace::INVALID,
    gpu::CommandBufferNamespace::NUM_COMMAND_BUFFER_NAMESPACES - 1)
IPC_ENUM_TRAITS_MAX_VALUE(gl::GpuPreference, gl::GpuPreference::kMaxValue)
IPC_ENUM_TRAITS_MAX_VALUE(gpu::ContextType, gpu::CONTEXT_TYPE_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(gfx::SurfaceOrigin, gfx::SurfaceOrigin::kBottomLeft)

IPC_STRUCT_TRAITS_BEGIN(gpu::GLCapabilities::ShaderPrecision)
  IPC_STRUCT_TRAITS_MEMBER(min_range)
  IPC_STRUCT_TRAITS_MEMBER(max_range)
  IPC_STRUCT_TRAITS_MEMBER(precision)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(gpu::GLCapabilities::PerStagePrecisions)
  IPC_STRUCT_TRAITS_MEMBER(low_int)
  IPC_STRUCT_TRAITS_MEMBER(medium_int)
  IPC_STRUCT_TRAITS_MEMBER(high_int)
  IPC_STRUCT_TRAITS_MEMBER(low_float)
  IPC_STRUCT_TRAITS_MEMBER(medium_float)
  IPC_STRUCT_TRAITS_MEMBER(high_float)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(gpu::Capabilities)
  IPC_STRUCT_TRAITS_MEMBER(max_texture_size)
  IPC_STRUCT_TRAITS_MEMBER(max_copy_texture_chromium_size)

  IPC_STRUCT_TRAITS_MEMBER(egl_image_external)
  IPC_STRUCT_TRAITS_MEMBER(texture_format_bgra8888)
  IPC_STRUCT_TRAITS_MEMBER(texture_format_etc1_npot)
  IPC_STRUCT_TRAITS_MEMBER(sync_query)
  IPC_STRUCT_TRAITS_MEMBER(texture_rg)
  IPC_STRUCT_TRAITS_MEMBER(texture_norm16)
  IPC_STRUCT_TRAITS_MEMBER(texture_half_float_linear)
  IPC_STRUCT_TRAITS_MEMBER(image_ycbcr_420v)
  IPC_STRUCT_TRAITS_MEMBER(image_ar30)
  IPC_STRUCT_TRAITS_MEMBER(image_ab30)
  IPC_STRUCT_TRAITS_MEMBER(image_ycbcr_p010)
  IPC_STRUCT_TRAITS_MEMBER(render_buffer_format_bgra8888)
  IPC_STRUCT_TRAITS_MEMBER(msaa_is_slow)
  IPC_STRUCT_TRAITS_MEMBER(disable_one_component_textures)
  IPC_STRUCT_TRAITS_MEMBER(gpu_rasterization)
  IPC_STRUCT_TRAITS_MEMBER(angle_rgbx_internal_format)
  IPC_STRUCT_TRAITS_MEMBER(avoid_stencil_buffers)
  IPC_STRUCT_TRAITS_MEMBER(disable_2d_canvas_copy_on_write)
  IPC_STRUCT_TRAITS_MEMBER(supports_rgb_to_yuv_conversion)
  IPC_STRUCT_TRAITS_MEMBER(supports_yuv_readback)
  IPC_STRUCT_TRAITS_MEMBER(chromium_gpu_fence)
  IPC_STRUCT_TRAITS_MEMBER(context_supports_distance_field_text)
  IPC_STRUCT_TRAITS_MEMBER(using_vulkan_context)
  IPC_STRUCT_TRAITS_MEMBER(mesa_framebuffer_flip_y)

  IPC_STRUCT_TRAITS_MEMBER(gpu_memory_buffer_formats)
  IPC_STRUCT_TRAITS_MEMBER(drm_formats_and_modifiers)
  IPC_STRUCT_TRAITS_MEMBER(drm_device_id)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(gpu::GLCapabilities)
  IPC_STRUCT_TRAITS_MEMBER(vertex_shader_precisions)
  IPC_STRUCT_TRAITS_MEMBER(fragment_shader_precisions)

  IPC_STRUCT_TRAITS_MEMBER(major_version)
  IPC_STRUCT_TRAITS_MEMBER(minor_version)

  IPC_STRUCT_TRAITS_MEMBER(max_combined_texture_image_units)
  IPC_STRUCT_TRAITS_MEMBER(max_cube_map_texture_size)
  IPC_STRUCT_TRAITS_MEMBER(max_fragment_uniform_vectors)
  IPC_STRUCT_TRAITS_MEMBER(max_renderbuffer_size)
  IPC_STRUCT_TRAITS_MEMBER(max_texture_image_units)
  IPC_STRUCT_TRAITS_MEMBER(max_varying_vectors)
  IPC_STRUCT_TRAITS_MEMBER(max_vertex_attribs)
  IPC_STRUCT_TRAITS_MEMBER(max_vertex_texture_image_units)
  IPC_STRUCT_TRAITS_MEMBER(max_vertex_uniform_vectors)
  IPC_STRUCT_TRAITS_MEMBER(num_compressed_texture_formats)
  IPC_STRUCT_TRAITS_MEMBER(num_shader_binary_formats)
  IPC_STRUCT_TRAITS_MEMBER(bind_generates_resource_chromium)

  IPC_STRUCT_TRAITS_MEMBER(max_3d_texture_size)
  IPC_STRUCT_TRAITS_MEMBER(max_array_texture_layers)
  IPC_STRUCT_TRAITS_MEMBER(max_color_attachments)
  IPC_STRUCT_TRAITS_MEMBER(max_combined_fragment_uniform_components)
  IPC_STRUCT_TRAITS_MEMBER(max_combined_uniform_blocks)
  IPC_STRUCT_TRAITS_MEMBER(max_combined_vertex_uniform_components)
  IPC_STRUCT_TRAITS_MEMBER(max_draw_buffers)
  IPC_STRUCT_TRAITS_MEMBER(max_element_index)
  IPC_STRUCT_TRAITS_MEMBER(max_elements_indices)
  IPC_STRUCT_TRAITS_MEMBER(max_elements_vertices)
  IPC_STRUCT_TRAITS_MEMBER(max_fragment_input_components)
  IPC_STRUCT_TRAITS_MEMBER(max_fragment_uniform_blocks)
  IPC_STRUCT_TRAITS_MEMBER(max_fragment_uniform_components)
  IPC_STRUCT_TRAITS_MEMBER(max_program_texel_offset)
  IPC_STRUCT_TRAITS_MEMBER(max_samples)
  IPC_STRUCT_TRAITS_MEMBER(max_server_wait_timeout)
  IPC_STRUCT_TRAITS_MEMBER(max_texture_lod_bias)
  IPC_STRUCT_TRAITS_MEMBER(max_transform_feedback_interleaved_components)
  IPC_STRUCT_TRAITS_MEMBER(max_transform_feedback_separate_attribs)
  IPC_STRUCT_TRAITS_MEMBER(max_transform_feedback_separate_components)
  IPC_STRUCT_TRAITS_MEMBER(max_uniform_block_size)
  IPC_STRUCT_TRAITS_MEMBER(max_uniform_buffer_bindings)
  IPC_STRUCT_TRAITS_MEMBER(max_atomic_counter_buffer_bindings)
  IPC_STRUCT_TRAITS_MEMBER(max_shader_storage_buffer_bindings)
  IPC_STRUCT_TRAITS_MEMBER(shader_storage_buffer_offset_alignment)
  IPC_STRUCT_TRAITS_MEMBER(max_varying_components)
  IPC_STRUCT_TRAITS_MEMBER(max_vertex_output_components)
  IPC_STRUCT_TRAITS_MEMBER(max_vertex_uniform_blocks)
  IPC_STRUCT_TRAITS_MEMBER(max_vertex_uniform_components)
  IPC_STRUCT_TRAITS_MEMBER(min_program_texel_offset)
  IPC_STRUCT_TRAITS_MEMBER(num_program_binary_formats)
  IPC_STRUCT_TRAITS_MEMBER(uniform_buffer_offset_alignment)

  IPC_STRUCT_TRAITS_MEMBER(occlusion_query_boolean)
  IPC_STRUCT_TRAITS_MEMBER(timer_queries)

  IPC_STRUCT_TRAITS_MEMBER(max_texture_size)
  IPC_STRUCT_TRAITS_MEMBER(sync_query)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(gpu::CommandBuffer::State)
  IPC_STRUCT_TRAITS_MEMBER(get_offset)
  IPC_STRUCT_TRAITS_MEMBER(token)
  IPC_STRUCT_TRAITS_MEMBER(release_count)
  IPC_STRUCT_TRAITS_MEMBER(error)
  IPC_STRUCT_TRAITS_MEMBER(context_lost_reason)
  IPC_STRUCT_TRAITS_MEMBER(generation)
  IPC_STRUCT_TRAITS_MEMBER(set_get_buffer_count)
IPC_STRUCT_TRAITS_END()
