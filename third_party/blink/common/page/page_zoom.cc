// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/page/page_zoom.h"

#include <cmath>

namespace blink {

// The minimum and maximum amount of page zoom that is possible, independent
// of other factors such as device scale and page scale (pinch). Historically,
// these values came from WebKitLegacy/mac/WebView/WebView.mm where they are
// named MinimumZoomMultiplier and MaximumZoomMultiplier. But chromium has
// changed to use different limits.
const double kMinimumPageZoomFactor = 0.25;
const double kMaximumPageZoomFactor = 5.0;

// Change the zoom factor by 20% for each zoom level increase from the user.
// Historically, this value came from WebKit in
// WebKitLegacy/mac/WebView/WebView.mm (named as ZoomMultiplierRatio there).
static const double kTextSizeMultiplierRatio = 1.2;

double PageZoomLevelToZoomFactor(double zoom_level) {
  return std::pow(kTextSizeMultiplierRatio, zoom_level);
}

double PageZoomFactorToZoomLevel(double factor) {
  return std::log(factor) / std::log(kTextSizeMultiplierRatio);
}

bool PageZoomValuesEqual(double value_a, double value_b) {
  // Epsilon value for comparing two floating-point zoom values. We don't use
  // std::numeric_limits<> because it is too precise for zoom values. Zoom
  // values lose precision due to factor/level conversions. A value of 0.001
  // is precise enough for zoom value comparisons.
  const double kPageZoomEpsilon = 0.001;
  return (std::fabs(value_a - value_b) <= kPageZoomEpsilon);
}

}  // namespace blink
