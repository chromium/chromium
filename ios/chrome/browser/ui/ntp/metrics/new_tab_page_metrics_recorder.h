// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_METRICS_NEW_TAB_PAGE_METRICS_RECORDER_H_
#define IOS_CHROME_BROWSER_UI_NTP_METRICS_NEW_TAB_PAGE_METRICS_RECORDER_H_

#import "base/mac/foundation_util.h"

namespace base {
class TimeDelta;
}

// The feed visibility when an NTP impression is logged.
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

// Metrics recorder for the new tab page.
@interface NewTabPageMetricsRecorder : NSObject

// Logs a metric for the "Return to Recent Tab" tile being shown.
- (void)recordTimeSpentInNTP:(base::TimeDelta)timeSpent;

// Logs a metric with the feed visibility when the NTP is shown.
- (void)recordNTPImpression:(IOSNTPImpressionType)impressionType;

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

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_METRICS_NEW_TAB_PAGE_METRICS_RECORDER_H_
