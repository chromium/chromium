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

namespace blink {

float NavigatorDeviceMemory::deviceMemory() const {
  return ApproximatedDeviceMemory::GetApproximatedDeviceMemory();
}

}  // namespace blink
