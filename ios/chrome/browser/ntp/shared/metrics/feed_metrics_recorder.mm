// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/shared/metrics/feed_metrics_recorder.h"

#import "base/apple/foundation_util.h"
#import "base/debug/dump_without_crashing.h"
#import "base/json/values_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/feed/core/common/pref_names.h"
#import "components/feed/core/v2/public/ios/notice_card_tracker.h"
#import "components/feed/core/v2/public/ios/prefs.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/metrics/model/constants.h"
#import "ios/chrome/browser/ntp/shared/metrics/feed_metrics_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_actions_delegate.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/shared/public/features/features.h"

namespace {

// The number of days for the Activity Buckets calculations.
constexpr base::TimeDelta kRangeForActivityBuckets = base::Days(28);

}  // namespace

using feed::FeedEngagementType;
using feed::FeedUserActionType;

@interface FeedMetricsRecorder ()
// Tracking property to avoid duplicate recordings of
// FeedEngagementType::kFeedEngagedSimple.
@property(nonatomic, assign) BOOL engagedSimpleReportedDiscover;
// Tracking property to avoid duplicate recordings of
// FeedEngagementType::kFeedEngaged.
@property(nonatomic, assign) BOOL engagedReportedDiscover;
// Tracking property to avoid duplicate recordings of
// FeedEngagementType::kFeedScrolled.
@property(nonatomic, assign) BOOL scrolledReportedDiscover;
// Tracking property to avoid duplicate recordings of
// FeedEngagementType::kGoodVisit.
@property(nonatomic, assign) BOOL goodVisitReportedDiscover;

// Tracking property to avoid duplicate recordings of the Activity Buckets
// metric.
@property(nonatomic, strong) NSDate* activityBucketLastReportedDate;

// Tracks whether user has engaged with the latest refreshed content. The term
// "engaged" is defined by its usage in this file. For example, it may be
// similar to `engagedSimpleReportedDiscover`.
@property(nonatomic, assign, getter=hasEngagedWithLatestRefreshedContent)
    BOOL engagedWithLatestRefreshedContent;

// Tracking property to record a scroll for Good Visits.
@property(nonatomic, assign) BOOL goodVisitScroll;
// The timestamp when the first metric is being recorded for this session.
@property(nonatomic, assign) base::Time sessionStartTime;
@property(nonatomic, assign)
    base::Time lastInteractionTimeForDiscoverGoodVisits;
// The timestamp when the feed becomes visible again for Good Visits. It
// is reset when a new Good Visit session starts
@property(nonatomic, assign) base::Time feedBecameVisibleTime;
// The time the user has spent in the feed during a Good Visit session.
// This property is preserved across NTP usages if they are part of the same
// Good Visit Session.
@property(nonatomic, assign) NSTimeInterval discoverPreviousTimeInFeedGV;

// The aggregate of time a user has spent in the feed for
// `ContentSuggestions.Feed.TimeSpentInFeed`
@property(nonatomic, assign) base::TimeDelta timeSpentInFeed;

// YES if the NTP is visible.
@property(nonatomic, assign) BOOL isNTPVisible;

// The ProfileIOS PrefService.
@property(nonatomic, assign) PrefService* prefService;

@end

@implementation FeedMetricsRecorder {
  // Used to track Feed events for IPH display.
  raw_ptr<feature_engagement::Tracker> _featureEngagementTracker;
}

- (instancetype)initWithPrefService:(PrefService*)prefService
           featureEngagementTracker:
               (feature_engagement::Tracker*)featureEngagementTracker {
  DCHECK(prefService);
  self = [super init];
  if (self) {
    _prefService = prefService;
    _featureEngagementTracker = featureEngagementTracker;
  }
  return self;
}

#pragma mark - Public

+ (void)recordFeedRefreshTrigger:(FeedRefreshTrigger)trigger {
  base::UmaHistogramEnumeration(kDiscoverFeedRefreshTrigger, trigger);
}

- (void)recordFeedScrolled:(int)scrollDistance {
  self.goodVisitScroll = YES;
  [self checkEngagementGoodVisitWithInteraction:NO];
  // Log scrolled into Discover feed.
  if (!self.scrolledReportedDiscover) {
    base::UmaHistogramEnumeration(kDiscoverFeedEngagementTypeHistogram,
                                  FeedEngagementType::kFeedScrolled);
    self.scrolledReportedDiscover = YES;
  }
  [self recordEngagement:scrollDistance interacted:NO];
}

- (void)recordDeviceOrientationChanged:(UIDeviceOrientation)orientation {
  if (orientation == UIDeviceOrientationPortrait) {
    base::RecordAction(base::UserMetricsAction(
        kDiscoverFeedHistogramDeviceOrientationChangedToPortrait));
  } else if (orientation == UIDeviceOrientationLandscapeLeft ||
             orientation == UIDeviceOrientationLandscapeRight) {
    base::RecordAction(base::UserMetricsAction(
        kDiscoverFeedHistogramDeviceOrientationChangedToLandscape));
  }
}

- (void)recordNTPDidChangeVisibility:(BOOL)visible {
  self.isNTPVisible = visible;

  if (visible) {
    // Sets `feedBecameVisibleTime` before any time based check is ran to
    // prevent negative values from non-initialized variables.
    self.feedBecameVisibleTime = base::Time::Now();
    [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                    kOpenedFeedSurface
                                  asInteraction:NO];

    base::Time lastInteractionTimeForDiscoverGoodVisitsDate =
        self.prefService->GetTime(kLastInteractionTimeForDiscoverGoodVisits);
    if (lastInteractionTimeForDiscoverGoodVisitsDate != base::Time()) {
      self.lastInteractionTimeForDiscoverGoodVisits =
          lastInteractionTimeForDiscoverGoodVisitsDate;
    }

    // Total time spent in feed metrics.
    self.timeSpentInFeed = base::Seconds(
        self.prefService->GetDouble(kTimeSpentInFeedAggregateKey));
    [self computeActivityBuckets];
    [self recordTimeSpentInFeedIfDayIsDone];

    self.discoverPreviousTimeInFeedGV =
        self.prefService->GetDouble(kLongDiscoverFeedVisitTimeAggregateKey);

    if (self.discoverPreviousTimeInFeedGV < 0) {
      base::debug::DumpWithoutCrashing();
    }

    // Checks if there is a timestamp in PrefService for when a user clicked
    // on an article in order to be able to trigger a non-short click
    // interaction.
    base::Time articleVisitStart =
        self.prefService->GetTime(kArticleVisitTimestampKey);

    if (articleVisitStart != base::Time()) {
      // Report Good Visit if user came back to the NTP after spending
      // kNonShortClickSeconds in a feed article.
      if (base::Time::Now() - articleVisitStart >
          base::Seconds(kNonShortClickSeconds)) {
        // Trigger a GV for the feed.
        [self recordEngagedGoodVisits];
      }
      // Clear PrefService for new session.
      self.prefService->ClearPref(kArticleVisitTimestampKey);
    }
  } else {
    // Calculates total aggregate for the time in feed aggregate metric.

    // When the user opens the browser directly to a website while they
    // originally were on the NTP. Set `feedBecameVisibleTime` to now if it has
    // never been set before.
    base::Time now = base::Time::Now();
    if (self.feedBecameVisibleTime.is_null()) {
      self.feedBecameVisibleTime = now;
    }
    self.timeSpentInFeed = now - self.feedBecameVisibleTime;

    [self checkEngagementGoodVisitWithInteraction:NO];
    self.prefService->SetDouble(kTimeSpentInFeedAggregateKey,
                                self.timeSpentInFeed.InSecondsF());
    self.prefService->SetDouble(kLongDiscoverFeedVisitTimeAggregateKey,
                                self.discoverPreviousTimeInFeedGV);
  }
}

- (void)recordDiscoverFeedPreviewTapped {
  [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                  kTappedDiscoverFeedPreview
                                asInteraction:YES];
  base::RecordAction(
      base::UserMetricsAction(kDiscoverFeedUserActionPreviewTapped));
}

- (void)recordOpenURLInSameTab {
  [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::kTappedOnCard
                                asInteraction:YES];
  base::RecordAction(
      base::UserMetricsAction(kDiscoverFeedUserActionOpenSameTab));
  [self handleURLOpened];
}

- (void)recordOpenURLInNewTab {
  [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                  kTappedOpenInNewTab
                                asInteraction:YES];
  base::RecordAction(
      base::UserMetricsAction(kDiscoverFeedUserActionOpenNewTab));
  [self handleURLOpened];
}

- (void)recordOpenURLInIncognitoTab {
  [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                  kTappedOpenInNewIncognitoTab
                                asInteraction:YES];
  base::RecordAction(
      base::UserMetricsAction(kDiscoverFeedUserActionOpenIncognitoTab));
  [self handleURLOpened];
}

- (void)recordAddURLToReadLater {
  [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                  kAddedToReadLater
                                asInteraction:YES];
  base::RecordAction(
      base::UserMetricsAction(kDiscoverFeedUserActionReadLaterTapped));
}

- (void)recordTapSendFeedback {
  [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                  kTappedSendFeedback
                                asInteraction:YES];
  base::RecordAction(
      base::UserMetricsAction(kDiscoverFeedUserActionSendFeedbackOpened));
}

- (void)recordOpenBackOfCardMenu {
  [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                  kOpenedContextMenu
                                asInteraction:YES];
  base::RecordAction(
      base::UserMetricsAction(kDiscoverFeedUserActionContextMenuOpened));
}

- (void)recordCloseBackOfCardMenu {
  [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                  kClosedContextMenu
                                asInteraction:YES];
  base::RecordAction(
      base::UserMetricsAction(kDiscoverFeedUserActionCloseContextMenu));
}

- (void)recordOpenNativeBackOfCardMenu {
  [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                  kOpenedNativeActionSheet
                                asInteraction:YES];
  base::RecordAction(
      base::UserMetricsAction(kDiscoverFeedUserActionNativeActionSheetOpened));
}

- (void)recordShowDialog {
  [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::kOpenedDialog
                                asInteraction:YES];
  base::RecordAction(
      base::UserMetricsAction(kDiscoverFeedUserActionReportContentOpened));
}

- (void)recordDismissDialog {
  [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::kClosedDialog
                                asInteraction:YES];
  base::RecordAction(
      base::UserMetricsAction(kDiscoverFeedUserActionReportContentClosed));
}

- (void)recordDismissCard {
  [self
      recordDiscoverFeedUserActionHistogram:FeedUserActionType::kEphemeralChange
                              asInteraction:YES];
  base::RecordAction(base::UserMetricsAction(kDiscoverFeedUserActionHideStory));
}

- (void)recordUndoDismissCard {
  [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                  kEphemeralChangeRejected
                                asInteraction:YES];
}

- (void)recordCommittDismissCard {
  [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                  kEphemeralChangeCommited
                                asInteraction:YES];
}

- (void)recordShowSnackbar {
  [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::kShowSnackbar
                                asInteraction:NO];
}

- (void)recordCommandID:(int)commandID {
  base::UmaHistogramSparse(kDiscoverFeedUserActionCommandHistogram, commandID);
}

- (void)recordCardShownAtIndex:(NSUInteger)index {
  base::UmaHistogramExactLinear(kDiscoverFeedCardShownAtIndex, index,
                                kMaxCardsInFeed);
}

- (void)recordCardTappedAtIndex:(NSUInteger)index {
  base::UmaHistogramExactLinear(kDiscoverFeedURLOpened, 0, 1);
}

- (void)recordNoticeCardShown:(BOOL)shown {
  base::UmaHistogramBoolean(kDiscoverFeedNoticeCardFulfilled, shown);
  feed::prefs::SetLastFetchHadNoticeCard(*self.prefService, shown);
}

- (void)recordFeedArticlesFetchDurationInSeconds:
            (NSTimeInterval)durationInSeconds
                                         success:(BOOL)success {
  [self recordFeedArticlesFetchDuration:base::Seconds(durationInSeconds)
                                success:success];
}

- (void)recordFeedArticlesFetchDuration:(base::TimeDelta)duration
                                success:(BOOL)success {
  if (success) {
    base::UmaHistogramMediumTimes(
        kDiscoverFeedArticlesFetchNetworkDurationSuccess, duration);
  } else {
    base::UmaHistogramMediumTimes(
        kDiscoverFeedArticlesFetchNetworkDurationFailure, duration);
  }
  [self recordNetworkRequestDuration:duration];
}

- (void)recordFeedMoreArticlesFetchDurationInSeconds:
            (NSTimeInterval)durationInSeconds
                                             success:(BOOL)success {
  [self recordFeedMoreArticlesFetchDuration:base::Seconds(durationInSeconds)
                                    success:success];
}

- (void)recordFeedMoreArticlesFetchDuration:(base::TimeDelta)duration
                                    success:(BOOL)success {
  if (success) {
    base::UmaHistogramMediumTimes(
        kDiscoverFeedMoreArticlesFetchNetworkDurationSuccess, duration);
  } else {
    base::UmaHistogramMediumTimes(
        kDiscoverFeedMoreArticlesFetchNetworkDurationFailure, duration);
  }
  [self recordNetworkRequestDuration:duration];
}

- (void)recordFeedUploadActionsDurationInSeconds:
            (NSTimeInterval)durationInSeconds
                                         success:(BOOL)success {
  [self recordFeedUploadActionsDuration:base::Seconds(durationInSeconds)
                                success:success];
}

- (void)recordFeedUploadActionsDuration:(base::TimeDelta)duration
                                success:(BOOL)success {
  if (success) {
    base::UmaHistogramMediumTimes(
        kDiscoverFeedUploadActionsNetworkDurationSuccess, duration);
  } else {
    base::UmaHistogramMediumTimes(
        kDiscoverFeedUploadActionsNetworkDurationFailure, duration);
  }
  [self recordNetworkRequestDuration:duration];
}

- (void)recordNativeContextMenuVisibilityChanged:(BOOL)shown {
  if (shown) {
    [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                    kOpenedNativeContextMenu
                                  asInteraction:YES];
    base::RecordAction(base::UserMetricsAction(
        kDiscoverFeedUserActionNativeContextMenuOpened));
  } else {
    [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                    kClosedNativeContextMenu
                                  asInteraction:YES];
    base::RecordAction(base::UserMetricsAction(
        kDiscoverFeedUserActionNativeContextMenuClosed));
  }
}

- (void)recordNativePulldownMenuVisibilityChanged:(BOOL)shown {
  if (shown) {
    [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                    kOpenedNativePulldownMenu
                                  asInteraction:YES];
    base::RecordAction(base::UserMetricsAction(
        kDiscoverFeedUserActionNativePulldownMenuOpened));
  } else {
    [self recordDiscoverFeedUserActionHistogram:FeedUserActionType::
                                                    kClosedNativePulldownMenu
                                  asInteraction:YES];
    base::RecordAction(base::UserMetricsAction(
        kDiscoverFeedUserActionNativePulldownMenuClosed));
  }
}

- (void)recordActivityLoggingEnabled:(BOOL)loggingEnabled {
  base::UmaHistogramBoolean(kDiscoverFeedActivityLoggingEnabled,
                            loggingEnabled);
  self.prefService->SetBoolean(feed::prefs::kLastFetchHadLoggingEnabled,
                               loggingEnabled);
}

- (void)recordBrokenNTPHierarchy:(BrokenNTPHierarchyRelationship)relationship {
  base::UmaHistogramEnumeration(kDiscoverFeedBrokenNTPHierarchy, relationship);
  base::RecordAction(base::UserMetricsAction(kNTPViewHierarchyFixed));
}

- (void)recordFeedWillRefresh {
  base::RecordAction(base::UserMetricsAction(kFeedWillRefresh));
  // The feed will have new content so reset the engagement tracking variable.
  // TODO(crbug.com/40260057): We need to know whether the feed was actually
  // refreshed, and not just when it was triggered.
  self.engagedWithLatestRefreshedContent = NO;
}

- (void)recordFeedSettingsOnStartForEnterprisePolicy:(BOOL)enterprisePolicy
                                         feedVisible:(BOOL)feedVisible
                                            signedIn:(BOOL)signedIn
                                          waaEnabled:(BOOL)waaEnabled
                                         spywEnabled:(BOOL)spywEnabled
                                     lastRefreshTime:
                                         (base::Time)lastRefreshTime {
  UserSettingsOnStart settings =
      [self userSettingsOnStartForEnterprisePolicy:enterprisePolicy
                                       feedVisible:feedVisible
                                          signedIn:signedIn
                                        waaEnabled:waaEnabled
                                   lastRefreshTime:lastRefreshTime];
  base::UmaHistogramEnumeration(kFeedUserSettingsOnStart, settings);
}

- (void)recordCarouselScrolled:(int)scrollDistance {
  [self recordEngagement:scrollDistance interacted:NO];
}

- (void)recordFeedHandlingError:(NSString*)action {
  base::UmaHistogramBoolean(
      kFeedHandlingErrorPrefix + base::SysNSStringToUTF8(action), YES);
}

- (void)recordUniformityFlagValue:(BOOL)flag {
  base::UmaHistogramBoolean(kDiscoverUniformityFlag, flag);
}

- (void)recordShowSignInOnlyUIWithUserId:(BOOL)hasUserId {
  base::RecordAction(
      hasUserId ? base::UserMetricsAction(kShowFeedSignInOnlyUIWithUserId)
                : base::UserMetricsAction(kShowFeedSignInOnlyUIWithoutUserId));
}

- (void)recordShowSignInRelatedUIWithType:(feed::FeedSignInUI)type {
  base::UmaHistogramEnumeration(kFeedSignInUI, type);
  switch (type) {
    case feed::FeedSignInUI::kShowSignInOnlyFlow:
      return base::RecordAction(
          base::UserMetricsAction(kShowSignInOnlyFlowFromFeed));
    case feed::FeedSignInUI::kShowSignInDisableToast:
      return base::RecordAction(
          base::UserMetricsAction(kShowSignInDisableToastFromFeed));
  }
}

- (void)recordShowSyncnRelatedUIWithType:(feed::FeedSyncPromo)type {
  base::UmaHistogramEnumeration(kFeedSyncPromo, type);
  switch (type) {
    case feed::FeedSyncPromo::kShowSyncFlow:
      return base::RecordAction(base::UserMetricsAction(kShowSyncFlowFromFeed));
    case feed::FeedSyncPromo::kShowDisableToast:
      return base::RecordAction(
          base::UserMetricsAction(kShowDisableToastFromFeed));
  }
}

#pragma mark - Private

// Returns the UserSettingsOnStart value based on the user settings.
- (UserSettingsOnStart)
    userSettingsOnStartForEnterprisePolicy:(BOOL)enterprisePolicy
                               feedVisible:(BOOL)feedVisible
                                  signedIn:(BOOL)signedIn
                                waaEnabled:(BOOL)waaEnabled
                           lastRefreshTime:(base::Time)lastRefreshTime {
  if (!enterprisePolicy) {
    return UserSettingsOnStart::kFeedNotEnabledByPolicy;
  }

  if (!feedVisible) {
    if (signedIn) {
      return UserSettingsOnStart::kFeedNotVisibleSignedIn;
    }
    return UserSettingsOnStart::kFeedNotVisibleSignedOut;
  }

  if (!signedIn) {
    return UserSettingsOnStart::kSignedOut;
  }

  const base::TimeDelta delta = base::Time::Now() - lastRefreshTime;
  const BOOL hasRecentData =
      delta >= base::TimeDelta() && delta <= kUserSettingsMaxAge;
  if (!hasRecentData) {
    return UserSettingsOnStart::kSignedInNoRecentData;
  }

  if (waaEnabled) {
    return UserSettingsOnStart::kSignedInWaaOnDpOff;
  } else {
    return UserSettingsOnStart::kSignedInWaaOffDpOff;
  }
}

// Records histogram metrics for Discover feed user actions. If `isInteraction`,
// also logs an interaction to the visible feed.
- (void)recordDiscoverFeedUserActionHistogram:(FeedUserActionType)actionType
                                asInteraction:(BOOL)isInteraction {
  base::UmaHistogramEnumeration(kDiscoverFeedUserActionHistogram, actionType);
  if (isInteraction) {
    [self recordInteraction];
  }

  // Check if actionType warrants a Good Explicit Visit
  // If actionType is any of the cases below, trigger a Good Explicit
  // interaction by calling recordEngagementGoodVisit
  switch (actionType) {
    case FeedUserActionType::kAddedToReadLater:
    case FeedUserActionType::kOpenedNativeContextMenu:
    case FeedUserActionType::kTappedOpenInNewIncognitoTab:
      [self checkEngagementGoodVisitWithInteraction:YES];
      break;
    default:
      // Default will handle the remaining FeedUserActionTypes that
      // do not trigger a Good Explicit interaction, but might trigger a good
      // visit due to other checks e.g. Using the feed for
      // `kGoodVisitTimeInFeedSeconds`.
      [self checkEngagementGoodVisitWithInteraction:NO];
      break;
  }
}

// Logs engagement daily for the Activity Buckets Calculation.
- (void)logDailyActivity {
  const base::Time now = base::Time::Now();

  // Check if the array is initialized.
  base::Value::List lastReportedArray =
      self.prefService->GetList(kActivityBucketLastReportedDateArrayKey)
          .Clone();

  // Adds a daily entry to the `lastReportedArray` array
  // only once when the user engages.
  if ((lastReportedArray.size() > 0 &&
       (now - ValueToTime(lastReportedArray.back()).value()) >=
           base::Days(1)) ||
      lastReportedArray.size() == 0) {
    lastReportedArray.Append(TimeToValue(now));
    self.prefService->SetList(kActivityBucketLastReportedDateArrayKey,
                              std::move(lastReportedArray));
  }
}

// Calculates the amount of dates the user has been active for the past 28 days.
- (void)computeActivityBuckets {
  const base::Time now = base::Time::Now();

  base::Time lastActivityBucket =
      self.prefService->GetTime(kActivityBucketLastReportedDateKey);
  // If the `lastActivityBucket` is not set, set it to now to
  // prevent the first day from logging a metric.
  if (lastActivityBucket == base::Time()) {
    lastActivityBucket = now;
    self.prefService->SetTime(kActivityBucketLastReportedDateKey,
                              lastActivityBucket);
  }

  // Nothing to do if the activity was reported recently.
  if ((now - lastActivityBucket) < base::Days(1)) {
    return;
  }

  // Calculate activity buckets.
  // Check if the array is initialized.
  const base::Value::List& lastReportedArray =
      self.prefService->GetList(kActivityBucketLastReportedDateArrayKey);
  base::Value::List newLastReportedArray;

  // Do not save in newLastReportedArray dates > 28 days.
  for (NSUInteger i = 0; i < lastReportedArray.size(); ++i) {
    std::optional<base::Time> date = ValueToTime(lastReportedArray[i]);
    if (!date.has_value()) {
      continue;
    }
    if ((now - date.value()) <= kRangeForActivityBuckets) {
      newLastReportedArray.Append(TimeToValue(date.value()));
    }
  }

  FeedActivityBucket activityBucket = FeedActivityBucket::kNoActivity;
  // Check how many items in array.
  switch (newLastReportedArray.size()) {
    case 0:
      activityBucket = FeedActivityBucket::kNoActivity;
      break;
    case 1 ... 7:
      activityBucket = FeedActivityBucket::kLowActivity;
      break;
    case 8 ... 15:
      activityBucket = FeedActivityBucket::kMediumActivity;
      break;
    case 16 ... 28:
      activityBucket = FeedActivityBucket::kHighActivity;
      break;
    default:
      // This should never be reached, as dates should never be > 28 days.
      NOTREACHED();
  }
  self.prefService->SetInteger(kActivityBucketKey,
                               static_cast<int>(activityBucket));
  self.prefService->SetList(kActivityBucketLastReportedDateArrayKey,
                            std::move(newLastReportedArray));

  // Activity Buckets Daily Run.
  [self recordActivityBuckets:activityBucket];
  self.prefService->SetTime(kActivityBucketLastReportedDateKey,
                            base::Time::Now());
}

// Records the engagement buckets.
- (void)recordActivityBuckets:(FeedActivityBucket)activityBucket {
  base::UmaHistogramEnumeration(kAllFeedsActivityBucketsHistogram,
                                activityBucket);
}

// Records Feed engagement.
- (void)recordEngagement:(int)scrollDistance interacted:(BOOL)interacted {
  scrollDistance = abs(scrollDistance);

  // Determine if this interaction is part of a new 'session'.
  base::Time now = base::Time::Now();
  base::TimeDelta visitTimeout = base::Minutes(kMinutesBetweenSessions);
  if (now - self.sessionStartTime > visitTimeout) {
    [self finalizeSession];
  }

  // Reset the last active time for session measurement.
  self.sessionStartTime = now;

  // Report the user as engaged-simple if they have scrolled any amount or
  // interacted with the card, and it has not already been reported for this
  // Chrome run.
  if (scrollDistance > 0 || interacted) {
    [self recordEngagedSimple];
    self.engagedWithLatestRefreshedContent = YES;
  }

  // Report the user as engaged if they have scrolled more than the threshold or
  // interacted with the card, and it has not already been reported this
  // Chrome run.
  if (scrollDistance > kMinScrollThreshold || interacted) {
    [self recordEngaged];
  }
}

// Checks if a Good Visit should be recorded. `interacted` is YES if it was
// triggered by an explicit interaction. (e.g. Opening a new Tab in Incognito.)
- (void)checkEngagementGoodVisitWithInteraction:(BOOL)interacted {
  // Certain actions can be dispatched by a background thread, such as showing a
  // snackbar. We shouldn't access the PrefService in the background, so these
  // are ignored.
  if (![NSThread isMainThread]) {
    return;
  }
  // Determine if this interaction is part of a new session.
  base::Time now = base::Time::Now();
  if ((now - self.lastInteractionTimeForDiscoverGoodVisits) >
      base::Minutes(kMinutesBetweenSessions)) {
    [self resetGoodVisitSession];
  }
  self.lastInteractionTimeForDiscoverGoodVisits = now;
  // Report a Good Visit if any of the conditions below is YES and
  // no Good Visit has been recorded for the past `kMinutesBetweenSessions`:
  // 1. Good Explicit Interaction (add to reading list, long press, open in
  // new incognito tab ...).
  if (interacted) {
    [self recordEngagedGoodVisits];
    return;
  }
  // 2. Good time in feed (`kGoodVisitTimeInFeedSeconds` with >= 1 scroll in an
  // entire session).
  [self updateTimeSpentForCurrentGoodVisitSessionInFeed];
  if (self.discoverPreviousTimeInFeedGV > kGoodVisitTimeInFeedSeconds &&
      self.goodVisitScroll) {
    [self recordEngagedGoodVisits];
  }
}

// Records any direct interaction with the Feed, this doesn't include scrolling.
- (void)recordInteraction {
  [self recordEngagement:0 interacted:YES];
  // Log interaction for the feed.
  base::UmaHistogramEnumeration(kDiscoverFeedEngagementTypeHistogram,
                                FeedEngagementType::kFeedInteracted);

  // Log interaction with the Feature Engagement Tracker.
  if (base::FeatureList::IsEnabled(kFeedSwipeInProductHelp)) {
    _featureEngagementTracker->NotifyEvent(
        feature_engagement::events::kIOSActionOnFeed);
  }
}

// Records simple engagement for the current feed.
- (void)recordEngagedSimple {
  // Log simple engagment for Discover feed.
  if (!self.engagedSimpleReportedDiscover) {
    base::UmaHistogramEnumeration(kDiscoverFeedEngagementTypeHistogram,
                                  FeedEngagementType::kFeedEngagedSimple);
    self.engagedSimpleReportedDiscover = YES;
  }
}

// Records engagement for the currently selected feed.
- (void)recordEngaged {
  // If neither feed has been engaged with, log "AllFeeds" engagement.
  if (!self.engagedReportedDiscover) {
    // If the user has engaged with a feed, this is recorded as a user default.
    // This can be used for things which require feed engagement as a condition,
    // such as the top-of-feed signin promo.
    NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
    [defaults setBool:YES forKey:kEngagedWithFeedKey];

    // Log engagement for Activity Buckets.
    [self logDailyActivity];
  }

  // Log engagment for Discover feed.
  if (!self.engagedReportedDiscover) {
    base::UmaHistogramEnumeration(kDiscoverFeedEngagementTypeHistogram,
                                  FeedEngagementType::kFeedEngaged);
    self.engagedReportedDiscover = YES;
  }
  base::RecordAction(base::UserMetricsAction(kDiscoverFeedUserActionEngaged));
}

- (void)recordEngagedGoodVisits {
  if (self.goodVisitReportedDiscover) {
    return;
  }
  base::UmaHistogramEnumeration(kDiscoverFeedEngagementTypeHistogram,
                                FeedEngagementType::kGoodVisit);
  self.goodVisitReportedDiscover = YES;
}

// Calculates the time the user has spent in the feed during a good
// visit session.
- (void)updateTimeSpentForCurrentGoodVisitSessionInFeed {
  // Add the time spent since last recording.
  base::Time now = base::Time::Now();
  base::TimeDelta additionalTimeInFeed = now - self.feedBecameVisibleTime;
  if (additionalTimeInFeed.is_negative()) {
    self.feedBecameVisibleTime = now;
    return;
  }
  self.discoverPreviousTimeInFeedGV += additionalTimeInFeed.InSecondsF();
}

// Resets the session tracking values, this occurs if there's been
// `kMinutesBetweenSessions` minutes between sessions.
- (void)finalizeSession {
  // If simple engagement hasn't been logged, then there's no session to
  // finalize.
  if (!self.engagedSimpleReportedDiscover) {
    return;
  }
  self.engagedReportedDiscover = NO;
  self.engagedSimpleReportedDiscover = NO;
  self.scrolledReportedDiscover = NO;
}

// Resets the Good Visits session tracking values, this occurs if there's been
// kMinutesBetweenSessions minutes between sessions.
- (void)resetGoodVisitSession {
  // Reset defaults for new session.
  self.prefService->ClearPref(kArticleVisitTimestampKey);
  self.prefService->ClearPref(kLongDiscoverFeedVisitTimeAggregateKey);

  base::Time now = base::Time::Now();

  self.lastInteractionTimeForDiscoverGoodVisits = now;
  self.prefService->SetTime(kLastInteractionTimeForDiscoverGoodVisits, now);

  self.feedBecameVisibleTime = now;
  self.goodVisitScroll = NO;
  self.goodVisitReportedDiscover = NO;
  self.discoverPreviousTimeInFeedGV = 0;
}

// Records the time a user has spent in the feed for a day when 24hrs have
// passed.
- (void)recordTimeSpentInFeedIfDayIsDone {
  // The midnight time for the day in which the
  // `ContentSuggestions.Feed.TimeSpentInFeed` was last recorded.
  const base::Time lastInteractionReported =
      self.prefService->GetTime(kLastDayTimeInFeedReportedKey);

  DCHECK(self.timeSpentInFeed >= base::Seconds(0));

  BOOL shouldResetData = NO;
  if (lastInteractionReported != base::Time()) {
    base::Time now = base::Time::Now();
    base::TimeDelta sinceDayStart = (now - lastInteractionReported);
    if (sinceDayStart >= base::Days(1)) {
      // Check if the user has spent any time in the feed.
      if (self.timeSpentInFeed > base::Seconds(0)) {
        base::UmaHistogramLongTimes(kTimeSpentInFeedHistogram,
                                    self.timeSpentInFeed);
      }
      shouldResetData = YES;
    }
  } else {
    shouldResetData = YES;
  }

  if (shouldResetData) {
    // Update the last report time in PrefService.
    self.prefService->SetTime(kLastDayTimeInFeedReportedKey, base::Time::Now());
    // Reset time spent in feed aggregate.
    self.timeSpentInFeed = base::Seconds(0);
    self.prefService->SetDouble(kTimeSpentInFeedAggregateKey,
                                self.timeSpentInFeed.InSecondsF());
  }
}

// Records the `duration` it took to Discover feed to perform any
// network operation.
- (void)recordNetworkRequestDuration:(base::TimeDelta)duration {
  base::UmaHistogramMediumTimes(kDiscoverFeedNetworkDuration, duration);
}

// Called when a URL was opened regardless of the target surface (e.g. New Tab,
// Same Tab, Incognito Tab, etc.).
- (void)handleURLOpened {
  // Save the time of the open so we can then calculate how long the user spent
  // in that page.
  self.prefService->SetTime(kArticleVisitTimestampKey, base::Time::Now());
  [self.NTPActionsDelegate feedArticleOpened];
}

@end
