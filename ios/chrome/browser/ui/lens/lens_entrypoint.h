// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_LENS_LENS_ENTRYPOINT_H_
#define IOS_CHROME_BROWSER_UI_LENS_LENS_ENTRYPOINT_H_

// Enum representing the possible Lens entrypoints on iOS.
// Current values should not be renumbered. Please keep in sync with
// "IOSLensEntrypoint" in src/tools/metrics/histograms/enums.xml.
enum class LensEntrypoint {
  ContextMenu = 0,
  HomeScreenWidget = 1,
  NewTabPage = 2,
  Keyboard = 3,
  Spotlight = 4,
  OmniboxPostCapture = 5,
  ImageShareMenu = 6,
  AppIconLongPress = 7,
  kMaxValue = AppIconLongPress,
};

extern const char kIOSLensEntrypoint[];

#endif  // IOS_CHROME_BROWSER_UI_LENS_LENS_ENTRYPOINT_H_
