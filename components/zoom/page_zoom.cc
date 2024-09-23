// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zoom/page_zoom.h"

#include <stddef.h>

#include <algorithm>
#include <functional>

#include "base/metrics/user_metrics.h"
#include "components/zoom/page_zoom_constants.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_zoom.h"
#include "third_party/blink/public/common/page/page_zoom.h"

using base::UserMetricsAction;

namespace {

enum PageZoomValueType {
  PAGE_ZOOM_VALUE_TYPE_FACTOR,
  PAGE_ZOOM_VALUE_TYPE_LEVEL,
};

std::vector<double> PresetZoomValues(PageZoomValueType value_type,
                                     double custom_value) {
  // Generate a vector of zoom values from an array of known preset
  // factors. The values in content::kPresetBrowserZoomFactors will already be
  // in sorted order.
  std::vector<double> zoom_values;
  zoom_values.reserve(blink::kPresetBrowserZoomFactors.size());
  bool found_custom = false;
  for (double zoom_value : blink::kPresetBrowserZoomFactors) {
    if (value_type == PAGE_ZOOM_VALUE_TYPE_LEVEL)
      zoom_value = blink::ZoomFactorToZoomLevel(zoom_value);
    if (blink::ZoomValuesEqual(zoom_value, custom_value)) {
      found_custom = true;
    }
    zoom_values.push_back(zoom_value);
  }
  // If the preset array did not contain the custom value, insert the value
  // while preserving the ordering.
  double min = zoom_values.front();
  double max = zoom_values.back();
  if (!found_custom && custom_value > min && custom_value < max) {
    zoom_values.insert(
        std::upper_bound(zoom_values.begin(), zoom_values.end(), custom_value),
        custom_value);
  }
  return zoom_values;
}

}  // namespace anonymous

namespace zoom {

// static
std::vector<double> PageZoom::PresetZoomFactors(double custom_factor) {
  return PresetZoomValues(PAGE_ZOOM_VALUE_TYPE_FACTOR, custom_factor);
}

// static
std::vector<double> PageZoom::PresetZoomLevels(double custom_level) {
  return PresetZoomValues(PAGE_ZOOM_VALUE_TYPE_LEVEL, custom_level);
}

// static
void PageZoom::Zoom(content::WebContents* web_contents,
                    content::PageZoom zoom) {
  zoom::ZoomController* zoom_controller =
      zoom::ZoomController::FromWebContents(web_contents);
  if (!zoom_controller)
    return;

  double current_zoom_level = zoom_controller->GetZoomLevel();
  double default_zoom_level = zoom_controller->GetDefaultZoomLevel();

  if (zoom == content::PAGE_ZOOM_RESET) {
    zoom_controller->SetZoomLevel(default_zoom_level);
    web_contents->SetPageScale(1.f);
    base::RecordAction(UserMetricsAction("ZoomNormal"));
    return;
  }

  // Generate a vector of zoom levels from an array of known presets along with
  // the default level added if necessary.
  std::vector<double> zoom_levels = PresetZoomLevels(default_zoom_level);

  if (zoom == content::PAGE_ZOOM_OUT) {
    // Find the zoom level that is next lower than the current level for this
    // page.
    auto next_lower = std::upper_bound(zoom_levels.rbegin(), zoom_levels.rend(),
                                       current_zoom_level, std::greater<>());
    // If the next level is within epsilon of the current, keep going until
    // we're taking a meaningful step.
    while (next_lower != zoom_levels.rend() &&
           blink::ZoomValuesEqual(*next_lower, current_zoom_level)) {
      ++next_lower;
    }
    if (next_lower == zoom_levels.rend()) {
      base::RecordAction(UserMetricsAction("ZoomMinus_AtMinimum"));
    } else {
      zoom_controller->SetZoomLevel(*next_lower);
      base::RecordAction(UserMetricsAction("ZoomMinus"));
    }
  } else {
    // Find the zoom level that is next higher than the current level for this
    // page.
    auto next_higher = std::upper_bound(zoom_levels.begin(), zoom_levels.end(),
                                        current_zoom_level);
    // If the next level is within epsilon of the current, keep going until
    // we're taking a meaningful step.
    while (next_higher != zoom_levels.end() &&
           blink::ZoomValuesEqual(*next_higher, current_zoom_level)) {
      ++next_higher;
    }
    if (next_higher == zoom_levels.end()) {
      base::RecordAction(UserMetricsAction("ZoomPlus_AtMaximum"));
    } else {
      zoom_controller->SetZoomLevel(*next_higher);
      base::RecordAction(UserMetricsAction("ZoomPlus"));
    }
  }
}

}  // namespace zoom
