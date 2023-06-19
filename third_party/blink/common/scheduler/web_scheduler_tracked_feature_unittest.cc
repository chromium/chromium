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

TEST(WebSchedulerTrackedFeatureTest, ToEnumBitMasks) {
  WebSchedulerTrackedFeatures features = {WebSchedulerTrackedFeature::kDummy};
  std::vector<uint64_t> bit_masks = ToEnumBitMasks(features);
  ASSERT_EQ(2u, bit_masks.size());
  ASSERT_EQ(1ull << static_cast<uint32_t>(WebSchedulerTrackedFeature::kDummy),
            bit_masks[0]);
}

}  // namespace scheduler
}  // namespace blink
