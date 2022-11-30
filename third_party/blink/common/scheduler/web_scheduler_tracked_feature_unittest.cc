// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/scheduler/web_scheduler_tracked_feature.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace scheduler {

TEST(WebSchedulerTrackedFeatureTest, StringToFeature) {
  ASSERT_EQ(WebSchedulerTrackedFeature::kWebSocket,
            StringToFeature("WebSocket"));
  ASSERT_EQ(WebSchedulerTrackedFeature::kDocumentLoaded,
            StringToFeature("DocumentLoaded"));
  ASSERT_EQ(absl::nullopt, StringToFeature("FeatureThatNeverExists"));
}

}  // namespace scheduler
}  // namespace blink
