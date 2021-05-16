// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_OPTIMIZATION_GUIDE_HINTS_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_OPTIMIZATION_GUIDE_HINTS_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/optimization_guide/optimization_guide.mojom-shared.h"

namespace blink {

struct WebOptimizationGuideHints {
  absl::optional<mojom::DelayAsyncScriptExecutionDelayType>
      delay_async_script_execution_delay_type;

  absl::optional<mojom::DelayCompetingLowPriorityRequestsDelayType>
      delay_competing_low_priority_requests_delay_type;
  absl::optional<mojom::DelayCompetingLowPriorityRequestsPriorityThreshold>
      delay_competing_low_priority_requests_priority_threshold;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_OPTIMIZATION_GUIDE_HINTS_H_
