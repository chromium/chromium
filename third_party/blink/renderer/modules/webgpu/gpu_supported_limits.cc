// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_supported_limits.h"

#include <algorithm>

#define SUPPORTED_LIMITS                                                      \
  X(maxTextureDimension1D, max_texture_dimension_1d_)                         \
  X(maxTextureDimension2D, max_texture_dimension_2d_)                         \
  X(maxTextureDimension3D, max_texture_dimension_3d_)                         \
  X(maxTextureArrayLayers, max_texture_array_layers_)                         \
  X(maxBindGroups, max_bind_groups_)                                          \
  X(maxDynamicUniformBuffersPerPipelineLayout,                                \
    max_dynamic_uniform_buffers_per_pipeline_layout_)                         \
  X(maxDynamicStorageBuffersPerPipelineLayout,                                \
    max_dynamic_storage_buffers_per_pipeline_layout_)                         \
  X(maxSampledTexturesPerShaderStage, max_sampled_textures_per_shader_stage_) \
  X(maxSamplersPerShaderStage, max_samplers_per_shader_stage_)                \
  X(maxStorageBuffersPerShaderStage, max_storage_buffers_per_shader_stage_)   \
  X(maxStorageTexturesPerShaderStage, max_storage_textures_per_shader_stage_) \
  X(maxUniformBuffersPerShaderStage, max_uniform_buffers_per_shader_stage_)   \
  X(maxUniformBufferBindingSize, max_uniform_buffer_binding_size_)            \
  X(maxStorageBufferBindingSize, max_storage_buffer_binding_size_)            \
  X(maxVertexBuffers, max_vertex_buffers_)                                    \
  X(maxVertexAttributes, max_vertex_attributes_)                              \
  X(maxVertexBufferArrayStride, max_vertex_buffer_array_stride_)

namespace blink {

GPUSupportedLimits::GPUSupportedLimits(
    const Vector<std::pair<String, unsigned>>& limits) {
  for (const auto& key_value : limits) {
#define X(name, member)                          \
  if (key_value.first == #name) {                \
    member = std::max(key_value.second, member); \
    continue;                                    \
  }
    SUPPORTED_LIMITS
#undef X

    // Unrecognized keys should be rejected by validation prior to reaching
    // this point.
    NOTREACHED();
  }
}

#define X(name, member) \
  unsigned GPUSupportedLimits::name() const { return member; }
SUPPORTED_LIMITS
#undef X

GPUSupportedLimits::ValidationResult GPUSupportedLimits::ValidateLimit(
    const String& name,
    unsigned value) {
#define X(limit_name, member)                                  \
  if (name == #limit_name) {                                   \
    return value <= limit_name() ? ValidationResult::Valid     \
                                 : ValidationResult::BadValue; \
  }
  SUPPORTED_LIMITS
#undef X

  // Unknown limit name.
  return ValidationResult::BadName;
}

}  // namespace blink
