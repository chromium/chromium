// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_METRICS_NEW_TAB_PAGE_METRICS_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_NTP_METRICS_NEW_TAB_PAGE_METRICS_CONSTANTS_H_

#pragma mark - Actions

extern const char kFakeboxNTPTappedAction[];
extern const char kFakeViewNTPTappedAction[];
extern const char kMostVisitedVoiceSearchAction[];
extern const char kNTPEntrypointTappedAction[];
extern const char kNTPIdentityDiscTappedAction[];

#pragma mark - Histograms

extern const char kActionOnNTPHistogram[];
extern const char kActionOnStartHistogram[];
extern const char kNTPTimeSpentHistogram[];
extern const char kStartTimeSpentHistogram[];
extern const char kNTPImpressionHistogram[];
extern const char kStartImpressionHistogram[];
extern const char kNTPOverscrollActionHistogram[];

#endif  // IOS_CHROME_BROWSER_UI_NTP_METRICS_NEW_TAB_PAGE_METRICS_CONSTANTS_H_
