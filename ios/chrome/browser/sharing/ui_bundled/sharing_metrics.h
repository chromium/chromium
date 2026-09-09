// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARING_UI_BUNDLED_SHARING_METRICS_H_
#define IOS_CHROME_BROWSER_SHARING_UI_BUNDLED_SHARING_METRICS_H_

// Enum for the location where the "Share This Page" button is shown or used.
// LINT.IfChange(ShareThisPageLocation)
enum class ShareThisPageLocation {
  kOmniboxLongPress = 0,      // Obsolete M155.
  kOverflowMenu = 1,          // Obsolete M155.
  kOmniboxVerbatimMatch = 2,  // Obsolete.
  kLocationBar = 3,
  kMaxValue = kLocationBar,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/mobile/enums.xml:ShareThisPageLocation)

#endif  // IOS_CHROME_BROWSER_SHARING_UI_BUNDLED_SHARING_METRICS_H_
