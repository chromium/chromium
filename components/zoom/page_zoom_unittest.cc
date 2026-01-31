// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zoom/page_zoom.h"

#include <algorithm>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/page/page_zoom.h"

namespace {
bool ZoomValueExists(const std::vector<double>& zoom_values,
                     double expected_zoom_value) {
  for (double zoom_value : zoom_values) {
    if (blink::ZoomValuesEqual(zoom_value, expected_zoom_value)) {
      return true;
    }
  }
  return false;
}
}  // namespace

TEST(PageTestZoom, PresetZoomFactors) {
  // Fetch a vector of preset zoom factors, including a custom value that we
  // already know is not going to be in the list.
  double custom_value = 1.05;  // 105%
  std::vector<double> factors = zoom::PageZoom::PresetZoomFactors(custom_value);

  // Expect at least 10 zoom factors.
  EXPECT_GE(factors.size(), 10U);

  // Expect the first and last items to match the minimum and maximum values.
  EXPECT_DOUBLE_EQ(factors.front(), blink::kMinimumBrowserZoomFactor);
  EXPECT_DOUBLE_EQ(factors.back(), blink::kMaximumBrowserZoomFactor);

  EXPECT_TRUE(std::is_sorted(factors.begin(), factors.end()));

  EXPECT_TRUE(ZoomValueExists(factors, custom_value));
  EXPECT_TRUE(ZoomValueExists(factors, 1.0));  // 100%
}

TEST(PageTestZoom, PresetZoomLevels) {
  // Fetch a vector of preset zoom levels, including a custom value that we
  // already know is not going to be in the list.
  double custom_value = 0.1;
  std::vector<double> levels = zoom::PageZoom::PresetZoomLevels(custom_value);

  // Expect at least 10 zoom levels.
  EXPECT_GE(levels.size(), 10U);

  EXPECT_TRUE(std::is_sorted(levels.begin(), levels.end()));

  EXPECT_TRUE(ZoomValueExists(levels, custom_value));
  EXPECT_TRUE(ZoomValueExists(levels, 0));  // 100% expressed as a zoom level
}

TEST(PageTestZoom, InvalidCustomFactor) {
  double too_low = 0.01;
  std::vector<double> factors = zoom::PageZoom::PresetZoomFactors(too_low);
  EXPECT_FALSE(blink::ZoomValuesEqual(factors.front(), too_low));

  double too_high = 99.0;
  factors = zoom::PageZoom::PresetZoomFactors(too_high);
  EXPECT_FALSE(blink::ZoomValuesEqual(factors.back(), too_high));
}

TEST(PageTestZoom, InvalidCustomLevel) {
  double too_low = -99.0;
  std::vector<double> levels = zoom::PageZoom::PresetZoomLevels(too_low);
  EXPECT_FALSE(blink::ZoomValuesEqual(levels.front(), too_low));

  double too_high = 99.0;
  levels = zoom::PageZoom::PresetZoomLevels(too_high);
  EXPECT_FALSE(blink::ZoomValuesEqual(levels.back(), too_high));
}
