// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/touch_adjustment.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_screen_info.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

namespace {

class FakeChromeClient : public RenderingTestChromeClient {
 public:
  FakeChromeClient() = default;

  void SetDeviceScaleFactor(float device_scale_factor) {
    screen_info_.device_scale_factor = device_scale_factor;
  }

  WebScreenInfo GetScreenInfo(LocalFrame&) const override {
    return screen_info_;
  }

 private:
  WebScreenInfo screen_info_;
};

}  // namespace

class TouchAdjustmentTest : public RenderingTest {
 protected:
  TouchAdjustmentTest()
      : RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()),
        chrome_client_(MakeGarbageCollected<FakeChromeClient>()) {}

  LocalFrame& GetFrame() const { return *GetDocument().GetFrame(); }

  FakeChromeClient& GetChromeClient() const override { return *chrome_client_; }

  void SetZoomAndScale(float device_scale_factor,
                       float browser_zoom_factor,
                       float page_scale_factor) {
    device_scale_factor_ = device_scale_factor;
    page_scale_factor_ = page_scale_factor;

    GetChromeClient().SetDeviceScaleFactor(device_scale_factor);
    GetFrame().SetPageZoomFactor(device_scale_factor * browser_zoom_factor);
    GetPage().SetPageScaleFactor(page_scale_factor);
  }

  const LayoutSize max_touch_area_dip_unscaled = LayoutSize(32, 32);
  const LayoutSize min_touch_area_dip_unscaled = LayoutSize(20, 20);

 private:
  Persistent<FakeChromeClient> chrome_client_;

  float device_scale_factor_;
  float page_scale_factor_;
};

TEST_F(TouchAdjustmentTest, AdjustmentRangeUpperboundScale) {
  // touch_area is set to always exceed the upper bound so we are really
  // checking the upper bound behavior below.
  LayoutSize touch_area(100, 100);

  LayoutSize result;
  // adjustment range is shrunk to default upper bound (32, 32)
  // when there is no zoom or scale.
  SetZoomAndScale(1 /* dsf */, 1 /* browser_zoom */, 1 /* page_scale */);
  result = GetHitTestRectForAdjustment(GetFrame(), touch_area);
  EXPECT_EQ(result, max_touch_area_dip_unscaled);

  // Browser zoom without dsf change is not changing the upper bound.
  SetZoomAndScale(1 /* dsf */, 2 /* browser_zoom */, 1 /* page_scale */);
  result = GetHitTestRectForAdjustment(GetFrame(), touch_area);
  EXPECT_EQ(result, max_touch_area_dip_unscaled);

  SetZoomAndScale(1 /* dsf */, 0.5,
                  /* browser_zoom */ 1 /* page_scale */);
  result = GetHitTestRectForAdjustment(GetFrame(), touch_area);
  EXPECT_EQ(result, max_touch_area_dip_unscaled);

  // When has page scale factor, upper bound is scaled.
  SetZoomAndScale(1 /* dsf */, 1 /* browser_zoom */, 2 /* page_scale */);
  result = GetHitTestRectForAdjustment(GetFrame(), touch_area);
  EXPECT_EQ(result, max_touch_area_dip_unscaled * (1.f / 2));

  // touch_area is in physical pixel, should change with dsf change.
  SetZoomAndScale(2 /* dsf */, 1 /* browser_zoom */, 1 /* page_scale */);
  result = GetHitTestRectForAdjustment(GetFrame(), touch_area);
  EXPECT_EQ(result, max_touch_area_dip_unscaled * 2.f);

  SetZoomAndScale(0.5 /* dsf */, 1 /* browser_zoom */, 1 /* page_scale */);
  result = GetHitTestRectForAdjustment(GetFrame(), touch_area);
  EXPECT_EQ(result, max_touch_area_dip_unscaled * 0.5f);

  // When DeviceScaleFactorDeprecated() is not 1, zoom-for-dsf is disabled,
  // touch_area should be in dip.
  SetZoomAndScale(2 /* dsf */, 1 /* browser_zoom */, 1 /* page_scale */);
  GetPage().SetDeviceScaleFactorDeprecated(0.5);
  result = GetHitTestRectForAdjustment(GetFrame(), touch_area);
  EXPECT_EQ(result, max_touch_area_dip_unscaled);
}

TEST_F(TouchAdjustmentTest, AdjustmentRangeLowerboundScale) {
  // touch_area is set to 0 to always lower than minimal range.
  LayoutSize touch_area(0, 0);
  LayoutSize result;

  // Browser zoom without dsf change is not changing the size.
  SetZoomAndScale(1 /* dsf */, 2 /* browser_zoom */, 1 /* page_scale */);
  result = GetHitTestRectForAdjustment(GetFrame(), touch_area);
  EXPECT_EQ(result, min_touch_area_dip_unscaled);

  // touch_area is in physical pixel, should change with dsf change.
  SetZoomAndScale(2 /* dsf */, 1 /* browser_zoom */, 1 /* page_scale */);
  result = GetHitTestRectForAdjustment(GetFrame(), touch_area);
  EXPECT_EQ(result, min_touch_area_dip_unscaled * 2.f);

  // Adjustment range is changed with page scale.
  SetZoomAndScale(1 /* dsf */, 1 /* browser_zoom */, 2 /* page_scale */);
  result = GetHitTestRectForAdjustment(GetFrame(), touch_area);
  EXPECT_EQ(result, min_touch_area_dip_unscaled * (1.f / 2));
}

}  // namespace blink
