// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_supported_limits.h"

#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_extent_3d_dict.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"

#include <algorithm>

#define SUPPORTED_LIMITS(X)                    \
  X(maxTextureDimension1D)                     \
  X(maxTextureDimension2D)                     \
  X(maxTextureDimension3D)                     \
  X(maxTextureArrayLayers)                     \
  X(maxBindGroups)                             \
  X(maxBindGroupsPlusVertexBuffers)            \
  X(maxBindingsPerBindGroup)                   \
  X(maxDynamicUniformBuffersPerPipelineLayout) \
  X(maxDynamicStorageBuffersPerPipelineLayout) \
  X(maxSampledTexturesPerShaderStage)          \
  X(maxSamplersPerShaderStage)                 \
  X(maxStorageBuffersPerShaderStage)           \
  X(maxStorageTexturesPerShaderStage)          \
  X(maxUniformBuffersPerShaderStage)           \
  X(maxUniformBufferBindingSize)               \
  X(maxStorageBufferBindingSize)               \
  X(minUniformBufferOffsetAlignment)           \
  X(minStorageBufferOffsetAlignment)           \
  X(maxVertexBuffers)                          \
  X(maxBufferSize)                             \
  X(maxVertexAttributes)                       \
  X(maxVertexBufferArrayStride)                \
  X(maxInterStageShaderComponents)             \
  X(maxInterStageShaderVariables)              \
  X(maxColorAttachments)                       \
  X(maxColorAttachmentBytesPerSample)          \
  X(maxComputeWorkgroupStorageSize)            \
  X(maxComputeInvocationsPerWorkgroup)         \
  X(maxComputeWorkgroupSizeX)                  \
  X(maxComputeWorkgroupSizeY)                  \
  X(maxComputeWorkgroupSizeZ)                  \
  X(maxComputeWorkgroupsPerDimension)

namespace blink {

namespace {
template <typename T>
constexpr T UndefinedLimitValue();

template <>
constexpr uint32_t UndefinedLimitValue<uint32_t>() {
  return WGPU_LIMIT_U32_UNDEFINED;
}

template <>
constexpr uint64_t UndefinedLimitValue<uint64_t>() {
  return WGPU_LIMIT_U64_UNDEFINED;
}
}  // namespace

GPUSupportedLimits::GPUSupportedLimits(const WGPUSupportedLimits& limits)
    : limits_(limits.limits) {
  for (auto* chain = limits.nextInChain; chain; chain = chain->next) {
    switch (chain->sType) {
      case (WGPUSType_DawnExperimentalSubgroupLimits): {
        auto* t = reinterpret_cast<WGPUDawnExperimentalSubgroupLimits*>(
            limits.nextInChain);
        subgroup_limits_ = *t;
        subgroup_limits_.chain.next = nullptr;
        subgroup_limits_initialized_ = true;
        break;
      }
      default:
        NOTREACHED();
    }
  }
}

// static
void GPUSupportedLimits::MakeUndefined(WGPURequiredLimits* out) {
#define X(name) \
  out->limits.name = UndefinedLimitValue<decltype(WGPULimits::name)>();
  SUPPORTED_LIMITS(X)
#undef X
}

// static
bool GPUSupportedLimits::Populate(WGPURequiredLimits* out,
                                  const Vector<std::pair<String, uint64_t>>& in,
                                  ScriptPromiseResolver* resolver) {
  // TODO(crbug.com/dawn/685): This loop is O(n^2) if the developer
  // passes all of the limits. It could be O(n) with a mapping of
  // String -> WGPULimits::*member.
  for (const auto& [limitName, limitRawValue] : in) {
#define X(name)                                                               \
  if (limitName == #name) {                                                   \
    using T = decltype(WGPULimits::name);                                     \
    base::CheckedNumeric<T> value{limitRawValue};                             \
    if (!value.IsValid() || value.ValueOrDie() == UndefinedLimitValue<T>()) { \
      resolver->RejectWithDOMException(                                       \
          DOMExceptionCode::kOperationError,                                  \
          "Required " #name " limit (" + String::Number(limitRawValue) +      \
              ") exceeds the maximum representable value for its type.");     \
      return false;                                                           \
    }                                                                         \
    out->limits.name = value.ValueOrDie();                                    \
    continue;                                                                 \
  }
    SUPPORTED_LIMITS(X)
#undef X
    resolver->RejectWithDOMException(
        DOMExceptionCode::kOperationError,
        "The limit \"" + limitName + "\" is not recognized.");
    return false;
  }
  return true;
}

#define X(name)                                                 \
  decltype(WGPULimits::name) GPUSupportedLimits::name() const { \
    return limits_.name;                                        \
  }
SUPPORTED_LIMITS(X)
#undef X

unsigned GPUSupportedLimits::minSubgroupSize() const {
  // Return the undefined limits value if subgroup limits is not acquired.
  if (!subgroup_limits_initialized_) {
    return UndefinedLimitValue<unsigned>();
  }
  return subgroup_limits_.minSubgroupSize;
}

unsigned GPUSupportedLimits::maxSubgroupSize() const {
  // Return the undefined limits value if subgroup limits is not acquired.
  if (!subgroup_limits_initialized_) {
    return UndefinedLimitValue<unsigned>();
  }
  return subgroup_limits_.maxSubgroupSize;
}

}  // namespace blink
