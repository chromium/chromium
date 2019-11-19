// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/renderer/window_features_converter.h"

// This converter only converts WebWindowFeatures members
// which should be used across process boundaries.
// All the members of WebWindowFeatures are listed in web_window_features.h,
// and classified to two groups, one is transferred by this converter
// and one is not.

namespace content {

blink::mojom::WindowFeaturesPtr ConvertWebWindowFeaturesToMojoWindowFeatures(
    const blink::WebWindowFeatures& web_window_features) {
  blink::mojom::WindowFeaturesPtr result = blink::mojom::WindowFeatures::New();
  result->x = web_window_features.x;
  result->has_x = web_window_features.x_set;
  result->y = web_window_features.y;
  result->has_y = web_window_features.y_set;
  result->width = web_window_features.width;
  result->has_width = web_window_features.width_set;
  result->height = web_window_features.height;
  result->has_height = web_window_features.height_set;
  result->menu_bar_visible = web_window_features.menu_bar_visible;
  result->status_bar_visible = web_window_features.status_bar_visible;
  result->tool_bar_visible = web_window_features.tool_bar_visible;
  result->scrollbars_visible = web_window_features.scrollbars_visible;
  return result;
}

blink::WebWindowFeatures ConvertMojoWindowFeaturesToWebWindowFeatures(
    const blink::mojom::WindowFeatures& window_features) {
  blink::WebWindowFeatures result;
  result.x = window_features.x;
  result.x_set = window_features.has_x;
  result.y = window_features.y;
  result.y_set = window_features.has_y;
  result.width = window_features.width;
  result.width_set = window_features.has_width;
  result.height = window_features.height;
  result.height_set = window_features.has_height;
  result.menu_bar_visible = window_features.menu_bar_visible;
  result.status_bar_visible = window_features.status_bar_visible;
  result.tool_bar_visible = window_features.tool_bar_visible;
  result.scrollbars_visible = window_features.scrollbars_visible;
  return result;
}

}  // namespace content
