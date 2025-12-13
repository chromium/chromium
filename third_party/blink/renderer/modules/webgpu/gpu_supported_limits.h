// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_SUPPORTED_LIMITS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_SUPPORTED_LIMITS_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_cpp.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class V8UnionUndefinedOrUnsignedLongLongEnforceRange;

class GPUSupportedLimits final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // TODO(crbug.com/421950205): Make this more like dawn::utils::ComboLimits
  // (for chaining safety) or replace it with a similar webgpu_cpp.h helper.
  struct ComboLimits : public wgpu::Limits,
                       public wgpu::CompatibilityModeLimits {
    ComboLimits();
    ComboLimits(const ComboLimits&) = delete;
    void operator=(ComboLimits&) = delete;
    ComboLimits(ComboLimits&&) = delete;
    void operator=(ComboLimits&&) = delete;

    // This is not copyable or movable to avoid surprises with nextInChain
    // pointers becoming stale (or getting replaced with nullptr).
    // This explicit copy makes it clear what happens.
    void UnlinkedCopyTo(ComboLimits*) const;

    // Sets the nextInChain pointers and returns the base struct. Use this
    // (rather than &comboLimits) whenever passing a ComboLimits to the API.
    wgpu::Limits* GetLinked();
  };

  explicit GPUSupportedLimits(const ComboLimits& limits);

  // Returns true if populated, false if not and the ScriptPromiseResolverBase
  // has been rejected.
  static bool Populate(
      ComboLimits* out,
      const HeapVector<
          std::pair<String,
                    Member<V8UnionUndefinedOrUnsignedLongLongEnforceRange>>>&
          in,
      ScriptPromiseResolverBase*);

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
  unsigned maxInterStageShaderVariables() const;
  unsigned maxColorAttachments() const;
  unsigned maxColorAttachmentBytesPerSample() const;
  unsigned maxComputeWorkgroupStorageSize() const;
  unsigned maxComputeInvocationsPerWorkgroup() const;
  unsigned maxComputeWorkgroupSizeX() const;
  unsigned maxComputeWorkgroupSizeY() const;
  unsigned maxComputeWorkgroupSizeZ() const;
  unsigned maxComputeWorkgroupsPerDimension() const;
  unsigned maxStorageBuffersInFragmentStage() const;
  unsigned maxStorageTexturesInFragmentStage() const;
  unsigned maxStorageBuffersInVertexStage() const;
  unsigned maxStorageTexturesInVertexStage() const;
  unsigned maxImmediateSize() const;

 private:
  ComboLimits limits_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_SUPPORTED_LIMITS_H_
