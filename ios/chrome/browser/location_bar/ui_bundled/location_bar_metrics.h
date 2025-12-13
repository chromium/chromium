// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_LOCATION_BAR_METRICS_H_
#define IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_LOCATION_BAR_METRICS_H_

enum class LocationBarBadgeType;

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
  kReaderMode = 4,
  kMaxValue = kReaderMode,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:IOSLocationBarLeadingIconType)

// Add enum here

/// Records lens overlay entrypoint available.
void RecordLensEntrypointAvailable();

/// Records lens overlay entrypoint hidden due to `visible_icon`.
void RecordLensEntrypointHidden(IOSLocationBarLeadingIconType visible_icon);

// Records badge updates that get sent to Location Bar Badge. Does not correlate
// with what badge is shown to a user.
void RecordLocationBarBadgeUpdate(LocationBarBadgeType badge_type);

// Records badges that get shown to a user from Location Bar Badge.
void RecordLocationBarBadgeShown(LocationBarBadgeType badge_type);

#endif  // IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_LOCATION_BAR_METRICS_H_
