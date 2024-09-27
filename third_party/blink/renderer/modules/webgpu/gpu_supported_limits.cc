// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_supported_limits.h"

#include <algorithm>

#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_extent_3d_dict.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

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
  return wgpu::kLimitU32Undefined;
}

template <>
constexpr uint64_t UndefinedLimitValue<uint64_t>() {
  return wgpu::kLimitU64Undefined;
}
}  // namespace

GPUSupportedLimits::GPUSupportedLimits(const wgpu::SupportedLimits& limits)
    : limits_(limits.limits) {
  for (auto* chain = limits.nextInChain; chain; chain = chain->nextInChain) {
    switch (chain->sType) {
      case (wgpu::SType::DawnExperimentalSubgroupLimits): {
        auto* t = static_cast<wgpu::DawnExperimentalSubgroupLimits*>(
            limits.nextInChain);
        subgroup_limits_ = *t;
        subgroup_limits_.nextInChain = nullptr;
        subgroup_limits_initialized_ = true;
        break;
      }
      default:
        NOTREACHED_IN_MIGRATION();
    }
  }
}

// static
void GPUSupportedLimits::MakeUndefined(wgpu::RequiredLimits* out) {
#define X(name) \
  out->limits.name = UndefinedLimitValue<decltype(wgpu::Limits::name)>();
  SUPPORTED_LIMITS(X)
#undef X
}

// static
bool GPUSupportedLimits::Populate(wgpu::RequiredLimits* out,
                                  const Vector<std::pair<String, uint64_t>>& in,
                                  ScriptPromiseResolverBase* resolver) {
  // TODO(crbug.com/dawn/685): This loop is O(n^2) if the developer
  // passes all of the limits. It could be O(n) with a mapping of
  // String -> wgpu::Limits::*member.
  for (const auto& [limitName, limitRawValue] : in) {
    if (limitName == "maxInterStageShaderComponents") {
      UseCounter::Count(
          resolver->GetExecutionContext(),
          WebFeature::kMaxInterStageShaderComponentsRequiredLimit);
    }
#define X(name)                                                               \
  if (limitName == #name) {                                                   \
    using T = decltype(wgpu::Limits::name);                                   \
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

#define X(name)                                                   \
  decltype(wgpu::Limits::name) GPUSupportedLimits::name() const { \
    return limits_.name;                                          \
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
