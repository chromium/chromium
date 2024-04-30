// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/wgsl_language_features.h"

#include "third_party/blink/renderer/modules/webgpu/dawn_enum_conversions.h"

namespace blink {

WGSLLanguageFeatures::WGSLLanguageFeatures(
    const std::vector<wgpu::WGSLFeatureName>& features) {
  for (const auto& dawn_feature : features) {
    V8WGSLFeatureName v8_feature{
        V8WGSLFeatureName::Enum::kPointerCompositeAccess};
    if (FromDawnEnum(dawn_feature, &v8_feature)) {
      features_.insert(v8_feature.AsString());
    }
  }
}

bool WGSLLanguageFeatures::has(const String& feature) const {
  return features_.Contains(feature);
}

bool WGSLLanguageFeatures::hasForBinding(
    ScriptState* script_state,
    const String& feature,
    ExceptionState& exception_state) const {
  return has(feature);
}

WGSLLanguageFeatures::IterationSource::IterationSource(
    const HashSet<String>& features) {
  features_.ReserveCapacityForSize(features.size());
  for (auto feature : features) {
    features_.insert(feature);
  }
  iter_ = features_.begin();
}

bool WGSLLanguageFeatures::IterationSource::FetchNextItem(
    ScriptState* script_state,
    String& value,
    ExceptionState& exception_state) {
  if (iter_ == features_.end()) {
    return false;
  }

  value = *iter_;
  ++iter_;

  return true;
}

}  // namespace blink
