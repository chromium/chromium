// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_OPTIMIZATION_GUIDE_HINTS_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_OPTIMIZATION_GUIDE_HINTS_H_

#include "base/optional.h"
#include "third_party/blink/public/mojom/optimization_guide/optimization_guide.mojom-shared.h"

namespace blink {

struct WebOptimizationGuideHints {
  base::Optional<mojom::DelayAsyncScriptExecutionDelayType>
      delay_async_script_execution_delay_type;

  base::Optional<mojom::DelayCompetingLowPriorityRequestsDelayType>
      delay_competing_low_priority_requests_delay_type;
  base::Optional<mojom::DelayCompetingLowPriorityRequestsPriorityThreshold>
      delay_competing_low_priority_requests_priority_threshold;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_OPTIMIZATION_GUIDE_HINTS_H_
