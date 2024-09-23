// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/page/page_zoom.h"

#include <cmath>

#include "build/build_config.h"

namespace blink {

static constexpr double kPresetBrowserZoomFactorsArray[] = {
    0.25, 1 / 3.0, 0.5,  2 / 3.0, 0.75, 0.8, 0.9, 1.0, 1.1,
    1.25, 1.5,     1.75, 2.0,     2.5,  3.0, 4.0, 5.0};
const base::span<const double> kPresetBrowserZoomFactors(
    kPresetBrowserZoomFactorsArray);

#if !BUILDFLAG(IS_ANDROID)
// The minimum and maximum amount of page zoom that is possible, independent
// of other factors such as device scale and page scale (pinch). Historically,
// these values came from WebKitLegacy/mac/WebView/WebView.mm where they are
// named MinimumZoomMultiplier and MaximumZoomMultiplier. But chromium has
// changed to use different limits.
const double kMinimumBrowserZoomFactor = 0.25;
const double kMaximumBrowserZoomFactor = 5.0;
#else
// On Android, the OS-level font size is considered when calculating zoom
// factor. At the OS-level, we support a range of 85% - 200%, and at the
// browser-level we support 50% - 300%. The max we support is therefore: 3.0 * 2
// = 6.0, and the min is 0.5 * .85 = .425 (depending on settings).
const double kMinimumBrowserZoomFactor = 0.425;
const double kMaximumBrowserZoomFactor = 6.0;
#endif

// Change the zoom factor by 20% for each zoom level increase from the user.
// Historically, this value came from WebKit in
// WebKitLegacy/mac/WebView/WebView.mm (named as ZoomMultiplierRatio there).
static const double kTextSizeMultiplierRatio = 1.2;

double ZoomLevelToZoomFactor(double zoom_level) {
  return std::pow(kTextSizeMultiplierRatio, zoom_level);
}

double ZoomFactorToZoomLevel(double zoom_factor) {
  return std::log(zoom_factor) / std::log(kTextSizeMultiplierRatio);
}

bool ZoomValuesEqual(double value_a, double value_b) {
  // Epsilon value for comparing two floating-point zoom values. We don't use
  // std::numeric_limits<> because it is too precise for zoom values. Zoom
  // values lose precision due to factor/level conversions. A value of 0.001
  // is precise enough for zoom value comparisons.
  const double kPageZoomEpsilon = 0.001;
  return (std::fabs(value_a - value_b) <= kPageZoomEpsilon);
}

}  // namespace blink
