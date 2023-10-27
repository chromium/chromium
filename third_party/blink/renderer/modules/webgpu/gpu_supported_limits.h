// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_SUPPORTED_LIMITS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_SUPPORTED_LIMITS_H_

#include <dawn/webgpu.h>

#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class DOMException;

class GPUSupportedLimits final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit GPUSupportedLimits(const WGPUSupportedLimits& limits);

  static void MakeUndefined(WGPURequiredLimits* out);
  static DOMException* Populate(WGPURequiredLimits* out,
                                const Vector<std::pair<String, uint64_t>>& in);

  GPUSupportedLimits(const GPUSupportedLimits&) = delete;
  GPUSupportedLimits& operator=(const GPUSupportedLimits&) = delete;

  unsigned maxTextureDimension1D() const;
  unsigned maxTextureDimension2D() const;
  unsigned maxTextureDimension3D() const;
  unsigned maxTextureArrayLayers() const;
  unsigned maxBindGroups() const;
  unsigned maxBindGroupsPlusVertexBuffers() const;
  unsigned maxBindingsPerBindGroup() const;
  unsigned maxDynamicUniformBuffersPerPipelineLayout() const;
  unsigned maxDynamicStorageBuffersPerPipelineLayout() const;
  unsigned maxSampledTexturesPerShaderStage() const;
  unsigned maxSamplersPerShaderStage() const;
  unsigned maxStorageBuffersPerShaderStage() const;
  unsigned maxStorageTexturesPerShaderStage() const;
  unsigned maxUniformBuffersPerShaderStage() const;
  uint64_t maxUniformBufferBindingSize() const;
  uint64_t maxStorageBufferBindingSize() const;
  unsigned minUniformBufferOffsetAlignment() const;
  unsigned minStorageBufferOffsetAlignment() const;
  unsigned maxVertexBuffers() const;
  uint64_t maxBufferSize() const;
  unsigned maxVertexAttributes() const;
  unsigned maxVertexBufferArrayStride() const;
  unsigned maxInterStageShaderComponents() const;
  unsigned maxInterStageShaderVariables() const;
  unsigned maxColorAttachments() const;
  unsigned maxColorAttachmentBytesPerSample() const;
  unsigned maxComputeWorkgroupStorageSize() const;
  unsigned maxComputeInvocationsPerWorkgroup() const;
  unsigned maxComputeWorkgroupSizeX() const;
  unsigned maxComputeWorkgroupSizeY() const;
  unsigned maxComputeWorkgroupSizeZ() const;
  unsigned maxComputeWorkgroupsPerDimension() const;
  unsigned minSubgroupSize() const;
  unsigned maxSubgroupSize() const;

 private:
  WGPULimits limits_;

  bool subgroup_limits_initialized_;
  WGPUDawnExperimentalSubgroupLimits subgroup_limits_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_SUPPORTED_LIMITS_H_
