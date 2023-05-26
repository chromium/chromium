// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/metrics/new_tab_page_metrics_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - Actions

const char kFakeboxNTPTappedAction[] = "MobileFakeboxNTPTapped";
const char kFakeViewNTPTappedAction[] = "MobileFakeViewNTPTapped";
const char kMostVisitedVoiceSearchAction[] = "MobileNTPMostVisitedVoiceSearch";
const char kNTPEntrypointTappedAction[] =
    "Mobile.LensIOS.NewTabPageEntrypointTapped";
const char kNTPIdentityDiscTappedAction[] = "MobileNTPIdentityDiscTapped";

#pragma mark - Histograms

const char kActionOnNTPHistogram[] = "IOS.NTP.Click";
const char kActionOnStartHistogram[] = "IOS.Start.Click";
const char kNTPTimeSpentHistogram[] = "NewTabPage.TimeSpent";
const char kStartTimeSpentHistogram[] = "IOS.Start.TimeSpent";
const char kNTPImpressionHistogram[] = "IOS.NTP.Impression";
const char kStartImpressionHistogram[] = "IOS.Start.Impression";
const char kNTPOverscrollActionHistogram[] = "IOS.NTP.OverscrollAction";
