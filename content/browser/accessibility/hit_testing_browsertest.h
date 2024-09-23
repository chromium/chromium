// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_HIT_TESTING_BROWSERTEST_H_
#define CONTENT_BROWSER_ACCESSIBILITY_HIT_TESTING_BROWSERTEST_H_

#include "content/browser/accessibility/accessibility_content_browsertest.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"

namespace gfx {

class Point;

}  // namespace gfx

namespace content {

class AccessibilityHitTestingBrowserTest
    : public AccessibilityContentBrowserTest,
      public ::testing::WithParamInterface<double> {
 public:
  AccessibilityHitTestingBrowserTest();
  ~AccessibilityHitTestingBrowserTest() override;

  void SetUpCommandLine(base::CommandLine* command_line) override;

  struct TestPassToString {
    std::string operator()(const ::testing::TestParamInfo<double>& info) const;
  };

 protected:
  ui::BrowserAccessibilityManager* GetRootBrowserAccessibilityManager();
  float GetDeviceScaleFactor();
  float GetPageScaleFactor();
  gfx::Rect GetViewBoundsInScreenCoordinates();
  gfx::Point CSSToFramePoint(gfx::Point css_point);
  gfx::Point CSSToPhysicalPixelPoint(gfx::Point css_point);
  gfx::Point FrameToCSSPoint(gfx::Point css_point);

  // Test the hit test action that fires an event.
  ui::BrowserAccessibility* HitTestAndWaitForResultWithEvent(
      const gfx::Point& point,
      ax::mojom::Event event_to_fire);
  ui::BrowserAccessibility* HitTestAndWaitForResult(const gfx::Point& point);

  // Test the hit test mojo RPC that calls a callback function.
  ui::BrowserAccessibility* AsyncHitTestAndWaitForCallback(
      const gfx::Point& point);

  // Test the caching async hit test.
  ui::BrowserAccessibility* CallCachingAsyncHitTest(
      const gfx::Point& page_point);

  ui::BrowserAccessibility* CallNearestLeafNode(const gfx::Point& page_point);
  void SynchronizeThreads();
  std::string FormatHitTestAccessibilityTree();
  std::string GetScopedTrace(gfx::Point css_point);
  void SimulatePinchZoom(float desired_page_scale);

  float page_scale_ = 1.0f;
  gfx::Point scroll_offset_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_HIT_TESTING_BROWSERTEST_H_
