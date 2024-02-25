// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/performance_navigation_timing.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_navigation_type.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class PerformanceNavigationTimingTest : public PageTestBase {
 protected:
  AtomicString GetNavigationTimingType(WebNavigationType type) {
    return PerformanceNavigationTiming::GetNavigationTimingType(type);
  }
};

TEST_F(PerformanceNavigationTimingTest, GetNavigationTimingType) {
  GetPage().SetVisibilityState(mojom::blink::PageVisibilityState::kHidden,
                               /*is_initial_state=*/false);
  AtomicString returned_type =
      GetNavigationTimingType(kWebNavigationTypeBackForward);
  EXPECT_EQ(returned_type, "back_forward");

  GetPage().SetVisibilityState(mojom::blink::PageVisibilityState::kVisible,
                               /*is_initial_state=*/false);
  returned_type =
      GetNavigationTimingType(kWebNavigationTypeFormResubmittedBackForward);
  EXPECT_EQ(returned_type, "back_forward");

  returned_type =
      GetNavigationTimingType(kWebNavigationTypeFormResubmittedReload);
  EXPECT_EQ(returned_type, "reload");

  returned_type = GetNavigationTimingType(kWebNavigationTypeRestore);
  EXPECT_EQ(returned_type, "back_forward");
}
}  // namespace blink
