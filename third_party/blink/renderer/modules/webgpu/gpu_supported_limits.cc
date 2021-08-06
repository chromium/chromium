// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_supported_limits.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_extent_3d_dict.h"

#include <algorithm>

#define SUPPORTED_LIMITS                                                       \
  MAX(unsigned, maxTextureDimension1D, max_texture_dimension_1d_)              \
  MAX(unsigned, maxTextureDimension2D, max_texture_dimension_2d_)              \
  MAX(unsigned, maxTextureDimension3D, max_texture_dimension_3d_)              \
  MAX(unsigned, maxTextureArrayLayers, max_texture_array_layers_)              \
  MAX(unsigned, maxBindGroups, max_bind_groups_)                               \
  MAX(unsigned, maxDynamicUniformBuffersPerPipelineLayout,                     \
      max_dynamic_uniform_buffers_per_pipeline_layout_)                        \
  MAX(unsigned, maxDynamicStorageBuffersPerPipelineLayout,                     \
      max_dynamic_storage_buffers_per_pipeline_layout_)                        \
  MAX(unsigned, maxSampledTexturesPerShaderStage,                              \
      max_sampled_textures_per_shader_stage_)                                  \
  MAX(unsigned, maxSamplersPerShaderStage, max_samplers_per_shader_stage_)     \
  MAX(unsigned, maxStorageBuffersPerShaderStage,                               \
      max_storage_buffers_per_shader_stage_)                                   \
  MAX(unsigned, maxStorageTexturesPerShaderStage,                              \
      max_storage_textures_per_shader_stage_)                                  \
  MAX(unsigned, maxUniformBuffersPerShaderStage,                               \
      max_uniform_buffers_per_shader_stage_)                                   \
  MAX(uint64_t, maxUniformBufferBindingSize, max_uniform_buffer_binding_size_) \
  MAX(uint64_t, maxStorageBufferBindingSize, max_storage_buffer_binding_size_) \
  MIN(unsigned, minUniformBufferOffsetAlignment,                               \
      min_uniform_buffer_offset_alignment_)                                    \
  MIN(unsigned, minStorageBufferOffsetAlignment,                               \
      min_storage_buffer_offset_alignment_)                                    \
  MAX(unsigned, maxVertexBuffers, max_vertex_buffers_)                         \
  MAX(unsigned, maxVertexAttributes, max_vertex_attributes_)                   \
  MAX(unsigned, maxVertexBufferArrayStride, max_vertex_buffer_array_stride_)   \
  MAX(unsigned, maxInterStageShaderComponents,                                 \
      max_inter_stage_shader_components_)                                      \
  MAX(unsigned, maxComputeWorkgroupStorageSize,                                \
      max_compute_workgroup_storage_size_)                                     \
  MAX(unsigned, maxComputeInvocationsPerWorkgroup,                             \
      max_compute_invocations_per_workgroup_)                                  \
  MAX(unsigned, maxComputeWorkgroupSizeX, max_compute_workgroup_size_x_)       \
  MAX(unsigned, maxComputeWorkgroupSizeY, max_compute_workgroup_size_y_)       \
  MAX(unsigned, maxComputeWorkgroupSizeZ, max_compute_workgroup_size_z_)       \
  MAX(unsigned, maxComputeWorkgroupsPerDimension,                              \
      max_compute_workgroups_per_dimension_)

namespace blink {

GPUSupportedLimits::GPUSupportedLimits(
    const Vector<std::pair<String, uint64_t>>& limits) {
  for (const auto& key_value : limits) {
#define MAX(type, name, member)                                     \
  if (key_value.first == #name) {                                   \
    member = std::max(static_cast<type>(key_value.second), member); \
    continue;                                                       \
  }
#define MIN(type, name, member)                                     \
  if (key_value.first == #name) {                                   \
    member = std::min(static_cast<type>(key_value.second), member); \
    continue;                                                       \
  }
    SUPPORTED_LIMITS
#undef MIN
#undef MAX

    // Unrecognized keys should be rejected by validation prior to reaching
    // this point.
    NOTREACHED();
  }
}

#define MAX(type, name, member) \
  type GPUSupportedLimits::name() const { return member; }
#define MIN MAX
SUPPORTED_LIMITS
#undef MIN
#undef MAX

GPUSupportedLimits::ValidationResult GPUSupportedLimits::ValidateLimit(
    const String& name,
    uint64_t value) {
#define MAX(type, limit_name, member)                          \
  if (name == #limit_name) {                                   \
    return value <= limit_name() ? ValidationResult::Valid     \
                                 : ValidationResult::BadValue; \
  }
#define MIN(type, limit_name, member)                          \
  if (name == #limit_name) {                                   \
    return value >= limit_name() ? ValidationResult::Valid     \
                                 : ValidationResult::BadValue; \
  }
  SUPPORTED_LIMITS
#undef MIN
#undef MAX

  // Unknown limit name.
  return ValidationResult::BadName;
}

}  // namespace blink
