// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/screen_details/screen_details_test_utils.h"

#include "base/ranges/algorithm.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace content::test {

base::Value::List GetExpectedScreenDetails() {
  base::Value::List expected_screens;
  auto* screen = display::Screen::GetScreen();
  std::vector<display::Display> displays = screen->GetAllDisplays();

  // Sort the displays by position; x first and then y, to match the API.
  base::ranges::stable_sort(displays, [](auto& a, auto& b) {
    if (a.bounds().x() != b.bounds().x())
      return a.bounds().x() < b.bounds().x();
    return a.bounds().y() < b.bounds().y();
  });
  for (const auto& display : displays) {
    base::Value::Dict dict;
    dict.Set("availHeight", display.work_area().height());
    dict.Set("availLeft", display.work_area().x());
    dict.Set("availTop", display.work_area().y());
    dict.Set("availWidth", display.work_area().width());
    dict.Set("colorDepth", display.color_depth());
    // Handle JS'dict pattern for specifying integer and floating point numbers.
    int int_scale_factor = base::ClampCeil(display.device_scale_factor());
    if (int_scale_factor == display.device_scale_factor())
      dict.Set("devicePixelRatio", int_scale_factor);
    else
      dict.Set("devicePixelRatio", display.device_scale_factor());
    dict.Set("height", display.bounds().height());
    dict.Set("isExtended", displays.size() > 1);
    dict.Set("isInternal", display.IsInternal());
    dict.Set("isPrimary", display.id() == screen->GetPrimaryDisplay().id());
    dict.Set("label", display.label());
    dict.Set("left", display.bounds().x());
    dict.Set("orientation", true);
    dict.Set("pixelDepth", display.color_depth());
    dict.Set("top", display.bounds().y());
    dict.Set("width", display.bounds().width());
    expected_screens.Append(std::move(dict));
  }
  return expected_screens;
}

}  // namespace content::test
