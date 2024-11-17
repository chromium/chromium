// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_LOCATION_BAR_METRICS_H_
#define IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_LOCATION_BAR_METRICS_H_

// Designates the leading icon type in the location bar.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(IOSLocationBarLeadingIconType)
enum class IOSLocationBarLeadingIconType {
  kNone = 0,
  kMessage = 1,
  kPriceTracking = 2,
  kLensOverlay = 3,
  kMaxValue = kLensOverlay,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:IOSLocationBarLeadingIconType)

/// Records lens overlay entrypoint available.
void RecordLensEntrypointAvailable();

/// Records lens overlay entrypoint hidden due to `visible_icon`.
void RecordLensEntrypointHidden(IOSLocationBarLeadingIconType visible_icon);

#endif  // IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_LOCATION_BAR_METRICS_H_
