// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/metrics/new_tab_page_metrics_recorder.h"

#import "base/mac/foundation_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/time/time.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation NewTabPageMetricsRecorder

#pragma mark - Public

- (void)recordTimeSpentInNTP:(base::TimeDelta)timeSpent {
  UmaHistogramMediumTimes("NewTabPage.TimeSpent", timeSpent);
}

- (void)recordNTPImpression:(IOSNTPImpressionType)impressionType {
  UMA_HISTOGRAM_ENUMERATION("IOS.NTP.Impression", impressionType,
                            IOSNTPImpressionType::kMaxValue);
  [self recordImpressionForTileAblation];
}

- (void)recordOverscrollActionForType:(OverscrollActionType)type {
  UMA_HISTOGRAM_ENUMERATION("IOS.NTP.OverscrollAction", type);
}

- (void)recordLensTapped {
  base::RecordAction(
      base::UserMetricsAction("Mobile.LensIOS.NewTabPageEntrypointTapped"));
}

- (void)recordVoiceSearchTapped {
  base::RecordAction(
      base::UserMetricsAction("MobileNTPMostVisitedVoiceSearch"));
}

- (void)recordFakeTapViewTapped {
  base::RecordAction(base::UserMetricsAction("MobileFakeViewNTPTapped"));
}

- (void)recordFakeOmniboxTapped {
  base::RecordAction(base::UserMetricsAction("MobileFakeboxNTPTapped"));
}

- (void)recordIdentityDiscTapped {
  base::RecordAction(base::UserMetricsAction("MobileNTPIdentityDiscTapped"));
}

- (void)recordHomeActionType:(IOSHomeActionType)type
              onStartSurface:(BOOL)isStartSurface {
  if (isStartSurface) {
    UMA_HISTOGRAM_ENUMERATION("IOS.Home.ActionOnStartSurface", type);
  } else {
    UMA_HISTOGRAM_ENUMERATION("IOS.Home.ActionOnNTP", type);
  }
}

#pragma mark - Private

// Records an NTP impression for the tile ablation retention feature.
- (void)recordImpressionForTileAblation {
  base::Time now = base::Time::Now();
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  if ([defaults boolForKey:kDoneWithTileAblationKey]) {
    return;
  }
  // Find/Set first NTP impression ever.
  NSDate* firstImpressionRecordedTileAblationExperiment =
      base::mac::ObjCCast<NSDate>(
          [defaults objectForKey:kFirstImpressionRecordedTileAblationKey]);
  int impressions = [defaults integerForKey:kNumberOfNTPImpressionsRecordedKey];
  // Record first NTP impression.
  if (firstImpressionRecordedTileAblationExperiment == nil) {
    [defaults setObject:now.ToNSDate()
                 forKey:kFirstImpressionRecordedTileAblationKey];
    [defaults setObject:now.ToNSDate() forKey:kLastNTPImpressionRecordedKey];
    [defaults setInteger:1 forKey:kNumberOfNTPImpressionsRecordedKey];
    return;
  }
  NSDate* lastImpressionTileAblation = base::mac::ObjCCast<NSDate>(
      [defaults objectForKey:kLastNTPImpressionRecordedKey]);
  // Check when the last impression happened.
  if (now - base::Time::FromNSDate(lastImpressionTileAblation) >=
      base::Minutes(kTileAblationImpressionThresholdMinutes)) {
    // Count impression for MVT/Shortcuts Experiment.
    [defaults setObject:now.ToNSDate() forKey:kLastNTPImpressionRecordedKey];
    [defaults setInteger:impressions + 1
                  forKey:kNumberOfNTPImpressionsRecordedKey];
  }
}

@end
