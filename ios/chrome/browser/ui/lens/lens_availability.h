// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_LENS_LENS_AVAILABILITY_H_
#define IOS_CHROME_BROWSER_UI_LENS_LENS_AVAILABILITY_H_

// Enum representing the possible Lens avaiability statuses on iOS.
// Current values should not be renumbered. Please keep in sync with
// "IOSLensSupportStatus" in src/tools/metrics/histograms/enums.xml.
enum class LensSupportStatus {
  LensSearchSupported = 0,
  NonGoogleSearchEngine = 1,
  kMaxValue = NonGoogleSearchEngine,
};

extern const char kIOSLensSupportStatusHistogram[];

#endif  // IOS_CHROME_BROWSER_UI_LENS_LENS_AVAILABILITY_H_
