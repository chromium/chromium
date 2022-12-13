// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_supported_features.h"

namespace blink {

GPUSupportedFeatures::GPUSupportedFeatures() = default;

GPUSupportedFeatures::GPUSupportedFeatures(
    const Vector<String>& feature_names) {
  for (const auto& feature : feature_names) {
    AddFeatureName(feature);
  }
}

void GPUSupportedFeatures::AddFeatureName(const String& feature_name) {
  features_.insert(feature_name);
}

bool GPUSupportedFeatures::has(const String& feature) const {
  return features_.Contains(feature);
}

bool GPUSupportedFeatures::hasForBinding(
    ScriptState* script_state,
    const String& feature,
    ExceptionState& exception_state) const {
  return has(feature);
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
