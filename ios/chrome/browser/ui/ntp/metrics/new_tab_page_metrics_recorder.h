// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_METRICS_NEW_TAB_PAGE_METRICS_RECORDER_H_
#define IOS_CHROME_BROWSER_UI_NTP_METRICS_NEW_TAB_PAGE_METRICS_RECORDER_H_

#import "base/mac/foundation_util.h"

namespace base {
class TimeDelta;
}

// The feed visibility when an Home impression is logged.
// These match tools/metrics/histograms/enums.xml.
enum class IOSNTPImpressionType {
  kFeedDisabled = 0,
  kFeedVisible = 1,
  kFeedCollapsed = 2,
  kMaxValue = kFeedCollapsed,
};

// These values are persisted to IOS.NTP.OverscrollAction histograms.
// Entries should not be renumbered and numeric values should never be reused.
enum class OverscrollActionType {
  kOpenedNewTab = 0,
  kPullToRefresh = 1,
  kCloseTab = 2,
  kMaxValue = kCloseTab,
};

// These values are persisted to IOS.Start/NTP.Click histograms.
// Entries should not be renumbered and numeric values should never be reused.
enum class IOSHomeActionType {
  kMostVisitedTile = 0,
  kShortcuts = 1,
  kReturnToRecentTab = 2,
  kFeedCard = 3,
  kFakebox = 4,
  kSetUpList = 5,
  kMaxValue = kSetUpList,
};

// Metrics recorder for the new tab page.
@interface NewTabPageMetricsRecorder : NSObject

// Logs a metric for the time spent on the Home surface before leaving the
// surface. `startSurface` is YES if Start is being shown, NO if a new tab page
// is being opened.
- (void)recordTimeSpentInHome:(base::TimeDelta)timeSpent
               isStartSurface:(BOOL)startSurface;

// Logs a metric with the feed visibility when Home is shown. `startSurface` is
// YES if Start is being shown, NO if a new tab page is being opened.
- (void)recordHomeImpression:(IOSNTPImpressionType)impressionType
              isStartSurface:(BOOL)startSurface;

// Logs a metric for an overscroll action on the NTP.
- (void)recordOverscrollActionForType:(OverscrollActionType)type;

// Logs a metric for the lens button being tapped in the fake omnibox.
- (void)recordLensTapped;

// Logs a metric for the voice search button being tapped in the NTP header.
- (void)recordVoiceSearchTapped;

// Logs a metric when the hidden tap view on top of the NTP is tapped to focus
// the omnibox.
- (void)recordFakeTapViewTapped;

// Logs a metric for the fake omnibox being tapped in the NTP.
- (void)recordFakeOmniboxTapped;

// Logs a metric for the identity disc being tapped in the NTP.
- (void)recordIdentityDiscTapped;

// Logs a Home action and attributes it to the NTP or Start surface.
- (void)recordHomeActionType:(IOSHomeActionType)type
              onStartSurface:(BOOL)isStartSurface;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_METRICS_NEW_TAB_PAGE_METRICS_RECORDER_H_
