// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/shared/metrics/new_tab_page_metrics_constants.h"

#pragma mark - Actions

const char kFakeboxNTPTappedAction[] = "MobileFakeboxNTPTapped";
const char kFakeViewNTPTappedAction[] = "MobileFakeViewNTPTapped";
const char kMostVisitedVoiceSearchAction[] = "MobileNTPMostVisitedVoiceSearch";
const char kNTPEntrypointTappedAction[] =
    "Mobile.LensIOS.NewTabPageEntrypointTapped";
const char kNTPIdentityDiscTappedAction[] = "MobileNTPIdentityDiscTapped";
const char kNTPCustomizationNewBadgeShownAction[] =
    "MobileNTPCustomizationNewBadgeShown";
const char kNTPCustomizationNewBadgeTappedAction[] =
    "MobileNTPCustomizationNewBadgeTapped";

#pragma mark - Histograms

const char kActionOnNTPHistogram[] = "IOS.NTP.Click";
const char kActionOnStartHistogram[] = "IOS.Start.Click";
const char kNTPTimeSpentHistogram[] = "NewTabPage.TimeSpent";
const char kStartTimeSpentHistogram[] = "IOS.Start.TimeSpent";
const char kNTPImpressionHistogram[] = "IOS.NTP.Impression";
const char kStartImpressionHistogram[] = "IOS.Start.Impression";
const char kNTPImpressionCustomizationStateHistogram[] =
    "IOS.NTP.Impression.CustomizationState";
const char kNTPOverscrollActionHistogram[] = "IOS.NTP.OverscrollAction";

const char kMagicStackSetUpListEnabledHistogram[] =
    "IOS.HomeCustomization.MagicStack.SetUpList.Enabled";
const char kMagicStackSafetyCheckEnabledHistogram[] =
    "IOS.HomeCustomization.MagicStack.SafetyCheck.Enabled";
const char kMagicStackTabResumptionEnabledHistogram[] =
    "IOS.HomeCustomization.MagicStack.TabResumption.Enabled";
const char kMagicStackParcelTrackingEnabledHistogram[] =
    "IOS.HomeCustomization.MagicStack.ParcelTracking.Enabled";
const char kHomeCustomizationOpenedHistogram[] = "IOS.HomeCustomization.Opened";
