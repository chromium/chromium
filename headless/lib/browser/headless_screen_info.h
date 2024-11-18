// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_SCREEN_INFO_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_SCREEN_INFO_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/types/expected.h"
#include "headless/public/headless_export.h"
#include "ui/gfx/geometry/rect.h"

namespace headless {

struct HEADLESS_EXPORT HeadlessScreenInfo {
  gfx::Rect bounds = gfx::Rect(800, 600);
  int color_depth = 24;
  float device_pixel_ratio = 1.0f;
  bool is_internal = false;
  std::string label;

  bool operator==(const HeadlessScreenInfo& other) const;

  // Parse one or more screen specifications returning a list of headless
  // screen infos or an error string.
  //
  // Example of the screen specifications: { 0,0 800x600 colorDepth=24 }
  //
  // Screen origin and size are the only positional parameters. Both can be
  // omitted.
  //
  // Available named parameters:
  //  colorDepth=24
  //  devicePixelRatio=1
  //  isInternal=true
  //  label='primary monitor'
  //
  // The first screen specified is assumed to be the primary screen. If origin
  // is omitted for a secondary screen it will be automatically calculated to
  // position the screen at the right of the previous screen, for example:
  //
  // {800x600}{800x600} is equivalent to specifying:
  //
  // {0,0 800x600} {800,0 800x600}

  static base::expected<std::vector<HeadlessScreenInfo>, std::string>
  FromString(std::string_view screen_info);

  std::string ToString() const;
};

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_SCREEN_INFO_H_
