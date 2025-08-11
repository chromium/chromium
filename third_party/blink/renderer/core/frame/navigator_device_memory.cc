// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/navigator_device_memory.h"

#include "third_party/blink/public/common/device_memory/approximated_device_memory.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_metric_builder.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_metrics.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-shared.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {

// TODO(435582603): Hard-coding this to a common value is a reasonable start,
// but it likely makes sense to vary the hard-coded number by platform and
// form-factor in order to maintain plausibility over time.
constexpr float kReducedDeviceMemoryValue = 8.0;

}  // namespace

float NavigatorDeviceMemory::deviceMemory() const {
  if (RuntimeEnabledFeatures::ReduceDeviceMemoryEnabled()) {
    return kReducedDeviceMemoryValue;
  }
  return ApproximatedDeviceMemory::GetApproximatedDeviceMemory();
}

}  // namespace blink
