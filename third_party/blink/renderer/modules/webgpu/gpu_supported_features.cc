// Copyright 2021 The Chromium Authors. All rights reserved.
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

bool GPUSupportedFeatures::hasForBinding(
    ScriptState* script_state,
    const String& feature,
    ExceptionState& exception_state) const {
  DCHECK(feature);
  return features_.find(feature) != features_.end();
}

GPUSupportedFeatures::IterationSource::IterationSource(
    const HashSet<String>& features) {
  features_.ReserveCapacityForSize(features.size());
  for (auto feature : features) {
    features_.insert(feature);
  }
  iter_ = features_.begin();
}

bool GPUSupportedFeatures::IterationSource::Next(
    ScriptState* script_state,
    String& key,
    String& value,
    ExceptionState& exception_state) {
  if (iter_ == features_.end()) {
    return false;
  }

  key = value = *iter_;
  ++iter_;

  return true;
}

}  // namespace blink
