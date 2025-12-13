// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_supported_features.h"

namespace blink {

std::optional<V8GPUFeatureName::Enum> GPUSupportedFeatures::ToV8FeatureNameEnum(
    wgpu::FeatureName f) {
  switch (f) {
    case wgpu::FeatureName::Depth32FloatStencil8:
      return V8GPUFeatureName::Enum::kDepth32FloatStencil8;
    case wgpu::FeatureName::TimestampQuery:
      return V8GPUFeatureName::Enum::kTimestampQuery;
    case wgpu::FeatureName::ChromiumExperimentalTimestampQueryInsidePasses:
      return V8GPUFeatureName::Enum::
          kChromiumExperimentalTimestampQueryInsidePasses;
    case wgpu::FeatureName::TextureCompressionBC:
      return V8GPUFeatureName::Enum::kTextureCompressionBc;
    case wgpu::FeatureName::TextureCompressionBCSliced3D:
      return V8GPUFeatureName::Enum::kTextureCompressionBcSliced3d;
    case wgpu::FeatureName::TextureCompressionETC2:
      return V8GPUFeatureName::Enum::kTextureCompressionEtc2;
    case wgpu::FeatureName::TextureCompressionASTC:
      return V8GPUFeatureName::Enum::kTextureCompressionAstc;
    case wgpu::FeatureName::TextureCompressionASTCSliced3D:
      return V8GPUFeatureName::Enum::kTextureCompressionAstcSliced3d;
    case wgpu::FeatureName::IndirectFirstInstance:
      return V8GPUFeatureName::Enum::kIndirectFirstInstance;
    case wgpu::FeatureName::DepthClipControl:
      return V8GPUFeatureName::Enum::kDepthClipControl;
    case wgpu::FeatureName::RG11B10UfloatRenderable:
      return V8GPUFeatureName::Enum::kRg11B10UfloatRenderable;
    case wgpu::FeatureName::BGRA8UnormStorage:
      return V8GPUFeatureName::Enum::kBgra8UnormStorage;
    case wgpu::FeatureName::ShaderF16:
      return V8GPUFeatureName::Enum::kShaderF16;
    case wgpu::FeatureName::Float32Filterable:
      return V8GPUFeatureName::Enum::kFloat32Filterable;
    case wgpu::FeatureName::Float32Blendable:
      return V8GPUFeatureName::Enum::kFloat32Blendable;
    case wgpu::FeatureName::DualSourceBlending:
      return V8GPUFeatureName::Enum::kDualSourceBlending;
    case wgpu::FeatureName::Subgroups:
      return V8GPUFeatureName::Enum::kSubgroups;
    case wgpu::FeatureName::TextureComponentSwizzle:
      return V8GPUFeatureName::Enum::kTextureComponentSwizzle;
    case wgpu::FeatureName::CoreFeaturesAndLimits:
      return V8GPUFeatureName::Enum::kCoreFeaturesAndLimits;
    case wgpu::FeatureName::ClipDistances:
      return V8GPUFeatureName::Enum::kClipDistances;
    case wgpu::FeatureName::MultiDrawIndirect:
      return V8GPUFeatureName::Enum::kChromiumExperimentalMultiDrawIndirect;
    case wgpu::FeatureName::ChromiumExperimentalSubgroupMatrix:
      return V8GPUFeatureName::Enum::kChromiumExperimentalSubgroupMatrix;
    case wgpu::FeatureName::PrimitiveIndex:
      return V8GPUFeatureName::Enum::kPrimitiveIndex;
    case wgpu::FeatureName::TextureFormatsTier1:
      return V8GPUFeatureName::Enum::kTextureFormatsTier1;
    case wgpu::FeatureName::TextureFormatsTier2:
      return V8GPUFeatureName::Enum::kTextureFormatsTier2;
    default:
      return std::nullopt;
  }
}

GPUSupportedFeatures::GPUSupportedFeatures() = default;

GPUSupportedFeatures::GPUSupportedFeatures(
    const wgpu::SupportedFeatures& supported_features) {
  // SAFETY: Required from caller
  const auto features_span = UNSAFE_BUFFERS(base::span<const wgpu::FeatureName>(
      supported_features.features, supported_features.featureCount));
  for (const auto& f : features_span) {
    auto feature_name_enum_optional = ToV8FeatureNameEnum(f);
    if (feature_name_enum_optional) {
      V8GPUFeatureName::Enum feature_name_enum =
          feature_name_enum_optional.value();
      AddFeatureName(V8GPUFeatureName(feature_name_enum));
    }
  }
}

void GPUSupportedFeatures::AddFeatureName(const V8GPUFeatureName feature_name) {
  // features_ and features_bitset_ must be kept synched.
  features_.insert(feature_name.AsString());
  features_bitset_.set(static_cast<size_t>(feature_name.AsEnum()));
}

bool GPUSupportedFeatures::Has(const V8GPUFeatureName::Enum feature) const {
  return features_bitset_.test(static_cast<size_t>(feature));
}

bool GPUSupportedFeatures::hasForBinding(
    ScriptState* script_state,
    const String& feature,
    ExceptionState& exception_state) const {
  return features_.Contains(feature);
}

GPUSupportedFeatures::IterationSource::IterationSource(
    const HashSet<String>& features) {
  features_.ReserveCapacityForSize(features.size());
  for (auto feature : features) {
    features_.insert(feature);
  }
  iter_ = features_.begin();
}

bool GPUSupportedFeatures::IterationSource::FetchNextItem(
    ScriptState* script_state,
    String& value) {
  if (iter_ == features_.end()) {
    return false;
  }

  value = *iter_;
  ++iter_;

  return true;
}

}  // namespace blink
