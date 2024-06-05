// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/scheduler/web_scheduler_tracked_feature.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace scheduler {

TEST(WebSchedulerTrackedFeatureTest, StringToFeature) {
  ASSERT_EQ(WebSchedulerTrackedFeature::kPrinting, StringToFeature("printing"));
  ASSERT_EQ(WebSchedulerTrackedFeature::kDocumentLoaded,
            StringToFeature("document-loaded"));
  ASSERT_EQ(std::nullopt, StringToFeature("FeatureThatNeverExists"));
}

}  // namespace scheduler
}  // namespace blink
