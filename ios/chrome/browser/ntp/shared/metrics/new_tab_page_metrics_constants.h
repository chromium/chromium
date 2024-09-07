// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_SHARED_METRICS_NEW_TAB_PAGE_METRICS_CONSTANTS_H_
#define IOS_CHROME_BROWSER_NTP_SHARED_METRICS_NEW_TAB_PAGE_METRICS_CONSTANTS_H_

#pragma mark - Actions

extern const char kFakeboxNTPTappedAction[];
extern const char kFakeViewNTPTappedAction[];
extern const char kMostVisitedVoiceSearchAction[];
extern const char kNTPEntrypointTappedAction[];
extern const char kNTPIdentityDiscTappedAction[];
extern const char kNTPCustomizationNewBadgeShownAction[];
extern const char kNTPCustomizationNewBadgeTappedAction[];

#pragma mark - Histograms

extern const char kActionOnNTPHistogram[];
extern const char kActionOnStartHistogram[];
extern const char kNTPTimeSpentHistogram[];
extern const char kStartTimeSpentHistogram[];
extern const char kNTPImpressionHistogram[];
extern const char kStartImpressionHistogram[];
extern const char kNTPImpressionCustomizationStateHistogram[];
extern const char kNTPOverscrollActionHistogram[];
extern const char kMagicStackSetUpListEnabledHistogram[];
extern const char kMagicStackSafetyCheckEnabledHistogram[];
extern const char kMagicStackTabResumptionEnabledHistogram[];
extern const char kMagicStackParcelTrackingEnabledHistogram[];
extern const char kHomeCustomizationOpenedHistogram[];

#endif  // IOS_CHROME_BROWSER_NTP_SHARED_METRICS_NEW_TAB_PAGE_METRICS_CONSTANTS_H_
