// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/shared/metrics/new_tab_page_metrics_recorder.h"

#import "base/apple/foundation_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/ntp/shared/metrics/new_tab_page_metrics_constants.h"

@implementation NewTabPageMetricsRecorder

#pragma mark - Public

- (void)recordTimeSpentInHome:(base::TimeDelta)timeSpent
               isStartSurface:(BOOL)startSurface {
  if (startSurface) {
    UmaHistogramMediumTimes(kStartTimeSpentHistogram, timeSpent);
  } else {
    UmaHistogramMediumTimes(kNTPTimeSpentHistogram, timeSpent);
  }
}

- (void)recordHomeImpression:(IOSNTPImpressionType)impressionType
              isStartSurface:(BOOL)startSurface {
  if (startSurface) {
    UMA_HISTOGRAM_ENUMERATION(kStartImpressionHistogram, impressionType,
                              IOSNTPImpressionType::kMaxValue);
  } else {
    UMA_HISTOGRAM_ENUMERATION(kNTPImpressionHistogram, impressionType,
                              IOSNTPImpressionType::kMaxValue);
  }
}

- (void)recordCustomizationState:
    (IOSNTPImpressionCustomizationState)impressionType {
  UMA_HISTOGRAM_ENUMERATION(kNTPImpressionCustomizationStateHistogram,
                            impressionType,
                            IOSNTPImpressionCustomizationState::kMaxValue);
}

- (void)recordOverscrollActionForType:(OverscrollActionType)type {
  UMA_HISTOGRAM_ENUMERATION(kNTPOverscrollActionHistogram, type);
}

- (void)recordLensTapped {
  base::RecordAction(base::UserMetricsAction(kNTPEntrypointTappedAction));
}

- (void)recordVoiceSearchTapped {
  base::RecordAction(base::UserMetricsAction(kMostVisitedVoiceSearchAction));
}

- (void)recordFakeTapViewTapped {
  base::RecordAction(base::UserMetricsAction(kFakeViewNTPTappedAction));
}

- (void)recordFakeOmniboxTapped {
  base::RecordAction(base::UserMetricsAction(kFakeboxNTPTappedAction));
}

- (void)recordIdentityDiscTapped {
  base::RecordAction(base::UserMetricsAction(kNTPIdentityDiscTappedAction));
}

- (void)recordMagicStackCustomizationStateWithSetUpList:(BOOL)setUpListEnabled
                                            safetyCheck:(BOOL)safetyCheckEnabled
                                          tabResumption:
                                              (BOOL)tabResumptionEnabled
                                         parcelTracking:
                                             (BOOL)parcelTrackingEnabled {
  base::UmaHistogramBoolean(kMagicStackSetUpListEnabledHistogram,
                            setUpListEnabled);
  base::UmaHistogramBoolean(kMagicStackSafetyCheckEnabledHistogram,
                            safetyCheckEnabled);
  base::UmaHistogramBoolean(kMagicStackTabResumptionEnabledHistogram,
                            tabResumptionEnabled);
  base::UmaHistogramBoolean(kMagicStackParcelTrackingEnabledHistogram,
                            parcelTrackingEnabled);
}

- (void)recordHomeCustomizationMenuOpenedFromEntrypoint:
    (HomeCustomizationEntrypoint)entrypoint {
  UMA_HISTOGRAM_ENUMERATION(kHomeCustomizationOpenedHistogram, entrypoint,
                            HomeCustomizationEntrypoint::kMaxValue);
}

@end
