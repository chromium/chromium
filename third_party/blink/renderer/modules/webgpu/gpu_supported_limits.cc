// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_supported_limits.h"

#include <algorithm>

#include "base/numerics/checked_math.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_undefined_unsignedlonglongenforcerange.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_extent_3d_dict.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"

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
  X(maxInterStageShaderVariables)              \
  X(maxColorAttachments)                       \
  X(maxColorAttachmentBytesPerSample)          \
  X(maxComputeWorkgroupStorageSize)            \
  X(maxComputeInvocationsPerWorkgroup)         \
  X(maxComputeWorkgroupSizeX)                  \
  X(maxComputeWorkgroupSizeY)                  \
  X(maxComputeWorkgroupSizeZ)                  \
  X(maxComputeWorkgroupsPerDimension)          \
  X(maxStorageBuffersInFragmentStage)          \
  X(maxStorageTexturesInFragmentStage)         \
  X(maxStorageBuffersInVertexStage)            \
  X(maxStorageTexturesInVertexStage)           \
  X(maxImmediateSize)

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

// GPUSupportedLimits

GPUSupportedLimits::GPUSupportedLimits(const ComboLimits& limits) {
  limits.UnlinkedCopyTo(&limits_);
}

// static
bool GPUSupportedLimits::Populate(
    ComboLimits* out,
    const HeapVector<
        std::pair<String,
                  Member<V8UnionUndefinedOrUnsignedLongLongEnforceRange>>>& in,
    ScriptPromiseResolverBase* resolver) {
  auto* context = resolver->GetExecutionContext();
  // TODO(crbug.com/dawn/685): This loop is O(n^2) if the developer
  // passes all of the limits. It could be O(n) with a mapping of
  // String -> wgpu::Limits::*member.
  for (const auto& [limitName, limitRawValue] : in) {
#define X(name)                                                               \
  if (limitName == #name) {                                                   \
    using T = decltype(GPUSupportedLimits::ComboLimits::name);                \
    if (limitRawValue->IsUndefined()) {                                       \
      continue;                                                               \
    }                                                                         \
    uint64_t limitRawIntegerValue =                                           \
        limitRawValue->GetAsUnsignedLongLongEnforceRange();                   \
    base::CheckedNumeric<T> value{limitRawIntegerValue};                      \
    if (!value.IsValid() || value.ValueOrDie() == UndefinedLimitValue<T>()) { \
      resolver->RejectWithDOMException(                                       \
          DOMExceptionCode::kOperationError,                                  \
          StrCat(                                                             \
              {"Required " #name " limit (",                                  \
               String::Number(limitRawIntegerValue),                          \
               ") exceeds the maximum representable value for its type."}));  \
      return false;                                                           \
    }                                                                         \
    out->name = value.ValueOrDie();                                           \
    continue;                                                                 \
  }
    SUPPORTED_LIMITS(X)
#undef X
    if (limitRawValue->IsUndefined()) {
      auto* console_message = MakeGarbageCollected<ConsoleMessage>(
          mojom::blink::ConsoleMessageSource::kRendering,
          mojom::blink::ConsoleMessageLevel::kWarning,
          StrCat({"The limit \"", limitName, "\" is not recognized."}));
      context->AddConsoleMessage(console_message);
    } else {
      resolver->RejectWithDOMException(
          DOMExceptionCode::kOperationError,
          StrCat({"The limit \"", limitName,
                  "\" with a non-undefined value is not recognized."}));
      return false;
    }
  }
  return true;
}

#define X(name)                                                              \
  decltype(GPUSupportedLimits::ComboLimits::name) GPUSupportedLimits::name() \
      const {                                                                \
    return limits_.name;                                                     \
  }
SUPPORTED_LIMITS(X)
#undef X

// GPUSupportedLimits::ComboLimits

GPUSupportedLimits::ComboLimits::ComboLimits() = default;

void GPUSupportedLimits::ComboLimits::UnlinkedCopyTo(
    GPUSupportedLimits::ComboLimits* o) const {
  *static_cast<wgpu::Limits*>(o) = *this;
  o->wgpu::Limits::nextInChain = nullptr;
  *static_cast<wgpu::CompatibilityModeLimits*>(o) = *this;
  o->wgpu::CompatibilityModeLimits::nextInChain = nullptr;
}

wgpu::Limits* GPUSupportedLimits::ComboLimits::GetLinked() {
  this->wgpu::Limits::nextInChain =
      static_cast<wgpu::CompatibilityModeLimits*>(this);
  this->wgpu::CompatibilityModeLimits::nextInChain = nullptr;
  return this;
}

}  // namespace blink
