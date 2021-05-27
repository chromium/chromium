// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_SUPPORTED_LIMITS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_SUPPORTED_LIMITS_H_

#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class GPUSupportedLimits final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  enum class ValidationResult { Valid, BadName, BadValue };

  // Create an object with the default limits.
  GPUSupportedLimits() = default;

  explicit GPUSupportedLimits(
      const Vector<std::pair<String, unsigned>>& limits);

  GPUSupportedLimits(const GPUSupportedLimits&) = delete;
  GPUSupportedLimits& operator=(const GPUSupportedLimits&) = delete;

  ValidationResult ValidateLimit(const String& name, unsigned value);

  unsigned maxTextureDimension1D() const;
  unsigned maxTextureDimension2D() const;
  unsigned maxTextureDimension3D() const;
  unsigned maxTextureArrayLayers() const;
  unsigned maxBindGroups() const;
  unsigned maxDynamicUniformBuffersPerPipelineLayout() const;
  unsigned maxDynamicStorageBuffersPerPipelineLayout() const;
  unsigned maxSampledTexturesPerShaderStage() const;
  unsigned maxSamplersPerShaderStage() const;
  unsigned maxStorageBuffersPerShaderStage() const;
  unsigned maxStorageTexturesPerShaderStage() const;
  unsigned maxUniformBuffersPerShaderStage() const;
  unsigned maxUniformBufferBindingSize() const;
  unsigned maxStorageBufferBindingSize() const;
  unsigned maxVertexBuffers() const;
  unsigned maxVertexAttributes() const;
  unsigned maxVertexBufferArrayStride() const;

 private:
  unsigned max_texture_dimension_1d_ = 8192;
  unsigned max_texture_dimension_2d_ = 8192;
  unsigned max_texture_dimension_3d_ = 2048;
  unsigned max_texture_array_layers_ = 2048;
  unsigned max_bind_groups_ = 4;
  unsigned max_dynamic_uniform_buffers_per_pipeline_layout_ = 8;
  unsigned max_dynamic_storage_buffers_per_pipeline_layout_ = 4;
  unsigned max_sampled_textures_per_shader_stage_ = 16;
  unsigned max_samplers_per_shader_stage_ = 16;
  unsigned max_storage_buffers_per_shader_stage_ = 4;
  unsigned max_storage_textures_per_shader_stage_ = 4;
  unsigned max_uniform_buffers_per_shader_stage_ = 12;
  unsigned max_uniform_buffer_binding_size_ = 16384;
  unsigned max_storage_buffer_binding_size_ = 134217728;
  unsigned max_vertex_buffers_ = 8;
  unsigned max_vertex_attributes_ = 16;
  unsigned max_vertex_buffer_array_stride_ = 2048;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_SUPPORTED_LIMITS_H_
