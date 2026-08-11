// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/availability.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-blink.h"
#include "third_party/blink/renderer/modules/ai/ai_metrics.h"

namespace blink {

TEST(AvailabilityTest, HandleModelAvailabilityCheckResultNullExecutionContext) {
  EXPECT_EQ(
      HandleModelAvailabilityCheckResult(
          /*execution_context=*/nullptr,
          AIMetrics::AISessionType::kLanguageModel,
          mojom::blink::ModelAvailabilityCheckResult::kUnavailableUnknown),
      Availability::kUnavailable);
}

}  // namespace blink
