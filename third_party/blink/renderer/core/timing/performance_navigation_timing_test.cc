// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/performance_navigation_timing.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class PerformanceNavigationTimingTest : public PageTestBase {
 protected:
  AtomicString GetNavigationType(WebNavigationType type, Document* document) {
    return PerformanceNavigationTiming::GetNavigationType(type, document);
  }
};

TEST_F(PerformanceNavigationTimingTest, GetNavigationType) {
  GetPage().SetVisibilityState(PageVisibilityState::kHidden,
                               /*initial_state=*/false);
  AtomicString returned_type =
      GetNavigationType(kWebNavigationTypeBackForward, &GetDocument());
  EXPECT_EQ(returned_type, "back_forward");

  GetPage().SetVisibilityState(PageVisibilityState::kVisible,
                               /*initial_state=*/false);
  returned_type =
      GetNavigationType(kWebNavigationTypeFormResubmitted, &GetDocument());
  EXPECT_EQ(returned_type, "navigate");
}
}  // namespace blink
