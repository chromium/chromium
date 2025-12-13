// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/wgsl_language_features.h"

#include "third_party/blink/renderer/modules/webgpu/dawn_enum_conversions.h"

namespace blink {

WGSLLanguageFeatures::WGSLLanguageFeatures(
    const std::vector<wgpu::WGSLLanguageFeatureName>& features) {
  for (const auto& dawn_feature : features) {
    if (const char* feature = FromDawnEnum(dawn_feature)) {
      features_.insert(feature);
    }
  }
}

bool WGSLLanguageFeatures::hasForBinding(
    ScriptState* script_state,
    const String& feature,
    ExceptionState& exception_state) const {
  return features_.Contains(feature);
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
    String& value) {
  if (iter_ == features_.end()) {
    return false;
  }

  value = *iter_;
  ++iter_;

  return true;
}

}  // namespace blink
