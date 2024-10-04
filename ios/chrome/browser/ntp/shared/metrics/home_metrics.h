// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_SHARED_METRICS_HOME_METRICS_H_
#define IOS_CHROME_BROWSER_NTP_SHARED_METRICS_HOME_METRICS_H_

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"

// These values are persisted to IOS.Start/NTP.Click histograms.
// Entries should not be renumbered and numeric values should never be reused.
// LINT.IfChange
enum class IOSHomeActionType {
  kMostVisitedTile = 0,
  kShortcuts = 1,
  kReturnToRecentTab = 2,
  kFeedCard = 3,
  // Removed: kFakebox = 4,
  kSetUpList = 5,
  kOmnibox = 6,
  kSafetyCheck = 7,
  kParcelTracking = 8,
  kOpenDistantTabResumption = 9,
  kPriceTrackingPromo = 10,
  kMaxValue = kPriceTrackingPromo,
};
// LINT.ThenChange(/tools/metrics/histograms/ios/enums.xml)

extern const char kMagicStackTopModuleImpressionHistogram[];

// Logs a Home action and attributes it to the NTP or Start surface.
void RecordHomeAction(IOSHomeActionType type, bool isStartSurface);

// Logs a Magic Stack card click split by NTP and Start surface.
void RecordMagicStackClick(ContentSuggestionsModuleType type,
                           bool isStartSurface);

// Logs some TabResumption specific metrics on click split by NTP and Start
// surface.
void RecordMagicStackTabResumptionClick(bool isLocal,
                                        bool isStartSurface,
                                        NSUInteger index);

// Logs a Magic Stack freshness event.
void RecordModuleFreshnessSignal(ContentSuggestionsModuleType module_type);

// Logs a metric for when a module of `type` is shown as the first module in the
// Magic Stack.
void LogTopModuleImpressionForType(ContentSuggestionsModuleType module_type);

// Logs some TabResumption specific metrics on impression split by NTP and Start
// surface.
void LogTabResumptionImpression(bool isLocal,
                                bool isStartSurface,
                                NSUInteger index);

#endif  // IOS_CHROME_BROWSER_NTP_SHARED_METRICS_HOME_METRICS_H_
