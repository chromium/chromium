// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "content/browser/accessibility/accessibility_browsertest.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/scoped_accessibility_mode_override.h"
#include "content/shell/browser/shell.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class AccessibilityMetricsBrowserTest : public AccessibilityBrowserTest {
 protected:
  base::HistogramTester histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(AccessibilityMetricsBrowserTest, EventProcessingTime) {
  // Make two changes to the accessibility mode so that two distinct calls are
  // made to RenderAccessibility::SetMode.
  SetInitialAccessibilityMode(ui::kAXModeBasic);

  LoadSampleParagraph();

  auto* const web_contents = shell()->web_contents();
  AccessibilityNotificationWaiter waiter(web_contents);
  ScopedAccessibilityModeOverride complete(ui::kAXModeComplete);
  ASSERT_TRUE(waiter.WaitForNotification(/*all_frames=*/true));

  // There should be at least one recording to the "first" histogram, and at
  // least one to the non-suffxed ("others") histogram.
  ASSERT_FALSE(histogram_tester_
                   .GetAllSamples("Accessibility.EventProcessingTime3.First")
                   .empty());
  ASSERT_FALSE(histogram_tester_
                   .GetAllSamples("Accessibility.EventProcessingTime3.NotFirst")
                   .empty());
}

}  // namespace content
