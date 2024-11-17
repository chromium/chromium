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
  V8NavigationTimingType::Enum GetNavigationTimingType(WebNavigationType type) {
    return PerformanceNavigationTiming::GetNavigationTimingType(type);
  }
};

TEST_F(PerformanceNavigationTimingTest, GetNavigationTimingType) {
  GetPage().SetVisibilityState(mojom::blink::PageVisibilityState::kHidden,
                               /*is_initial_state=*/false);
  V8NavigationTimingType::Enum returned_type =
      GetNavigationTimingType(kWebNavigationTypeBackForward);
  EXPECT_EQ(returned_type, V8NavigationTimingType::Enum::kBackForward);

  GetPage().SetVisibilityState(mojom::blink::PageVisibilityState::kVisible,
                               /*is_initial_state=*/false);
  returned_type =
      GetNavigationTimingType(kWebNavigationTypeFormResubmittedBackForward);
  EXPECT_EQ(returned_type, V8NavigationTimingType::Enum::kBackForward);

  returned_type =
      GetNavigationTimingType(kWebNavigationTypeFormResubmittedReload);
  EXPECT_EQ(returned_type, V8NavigationTimingType::Enum::kReload);

  returned_type = GetNavigationTimingType(kWebNavigationTypeRestore);
  EXPECT_EQ(returned_type, V8NavigationTimingType::Enum::kBackForward);
}
}  // namespace blink
