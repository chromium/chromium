// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SCREEN_DETAILS_SCREEN_DETAILS_TEST_UTILS_H_
#define CONTENT_BROWSER_SCREEN_DETAILS_SCREEN_DETAILS_TEST_UTILS_H_

#include "base/values.h"

namespace content::test {

// JS to get ScreenDetails in a list of dictionaries to facilitate comparison.
constexpr char kGetScreenDetailsScript[] = R"JS(
  (async () => {
    const screenDetails = await self.getScreenDetails();
    let result = [];
    for (let s of screenDetails.screens) {
      result.push({ availHeight: s.availHeight,
                    availLeft: s.availLeft,
                    availTop: s.availTop,
                    availWidth: s.availWidth,
                    colorDepth: s.colorDepth,
                    devicePixelRatio: s.devicePixelRatio,
                    height: s.height,
                    isExtended: s.isExtended,
                    isInternal: s.isInternal,
                    isPrimary: s.isPrimary,
                    label: s.label,
                    left: s.left,
                    orientation: s.orientation != null,
                    pixelDepth: s.pixelDepth,
                    top: s.top,
                    width: s.width });
    }
    return result;
  })();
)JS";

// Get display::Screen info in a list of dictionaries to facilitate comparison.
base::Value::List GetExpectedScreenDetails();

}  // namespace content::test

#endif  // CONTENT_BROWSER_SCREEN_DETAILS_SCREEN_DETAILS_TEST_UTILS_H_
