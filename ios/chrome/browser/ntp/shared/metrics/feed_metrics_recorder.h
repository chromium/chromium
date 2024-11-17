// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_SHARED_METRICS_FEED_METRICS_RECORDER_H_
#define IOS_CHROME_BROWSER_NTP_SHARED_METRICS_FEED_METRICS_RECORDER_H_

#import <UIKit/UIKit.h>

#import "base/time/time.h"
#import "components/feed/core/v2/public/common_enums.h"
#import "ios/chrome/browser/discover_feed/model/feed_constants.h"
#import "ios/chrome/browser/ntp/shared/metrics/feed_metrics_constants.h"
#import "ios/chrome/browser/ntp/shared/metrics/feed_refresh_state_tracker.h"

@protocol FeedControlDelegate;
@protocol NewTabPageFollowDelegate;
@protocol NewTabPageActionsDelegate;
@class NewTabPageState;
class PrefService;

// Records different metrics for the NTP feeds.
@interface FeedMetricsRecorder : NSObject <FeedRefreshStateTracker>

// The last active new tab page state.
@property(nonatomic, weak) NewTabPageState* NTPState;

// Delegate for getting information relating to Following.
@property(nonatomic, weak) id<NewTabPageFollowDelegate> followDelegate;

// Whether or not the feed is currently being shown on the Start Surface.
@property(nonatomic, assign) BOOL isShownOnStartSurface;

// Delegate for reporting feed actions.
@property(nonatomic, weak) id<NewTabPageActionsDelegate> NTPActionsDelegate;

- (instancetype)initWithPrefService:(PrefService*)prefService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Records the trigger where a feed refresh is requested.
+ (void)recordFeedRefreshTrigger:(FeedRefreshTrigger)trigger;

// Record metrics for when the user has scrolled `scrollDistance` in the Feed.
- (void)recordFeedScrolled:(int)scrollDistance;

// Record metrics for when the user changes the device orientation with the feed
// visible.
- (void)recordDeviceOrientationChanged:(UIDeviceOrientation)orientation;

// Tracks time spent in a specific Feed for a Good Visit.
- (void)recordFeedTypeChangedFromFeed:(FeedType)previousFeed;

// Record when the NTP changes visibility.
- (void)recordNTPDidChangeVisibility:(BOOL)visible;

// Record metrics for when the user has tapped on the feed preview.
- (void)recordDiscoverFeedPreviewTapped;

// Record metrics for when the user selects the 'Learn More' item in the feed
// header menu.
- (void)recordHeaderMenuLearnMoreTapped;

// Record metrics for when the user selects the 'Manage' item in the feed header
// menu.
- (void)recordHeaderMenuManageTapped;

// Record metrics for when the user selects the 'Manage Activity' item in the
// feed header menu.
- (void)recordHeaderMenuManageActivityTapped;

// Record metrics for when the user selects the 'Following' item in the feed
// header menu.
- (void)recordHeaderMenuManageFollowingTapped;

// Record metrics for when the user selects the 'Hidden' item in the feed
// management UI.
- (void)recordHeaderMenuManageHiddenTapped;

// Record metrics for when the user selects the 'Following' item in the feed
// management UI.
- (void)recordHeaderMenuManageFollowingTapped;

// Record metrics for when the user toggles the feed visibility from the feed
// header menu.
- (void)recordDiscoverFeedVisibilityChanged:(BOOL)visible;

// Records metrics for when a user opens an article in the same tab.
- (void)recordOpenURLInSameTab;

// Records metrics for when a user opens an article in a new tab.
- (void)recordOpenURLInNewTab;

// Records metrics for when a user opens an article in an incognito tab.
- (void)recordOpenURLInIncognitoTab;

// Records metrics for when a user adds an article to the Reading List.
- (void)recordAddURLToReadLater;

// Records metrics for when a user opens the Send Feedback form.
- (void)recordTapSendFeedback;

// Records metrics for when a user opens the back of card menu.
- (void)recordOpenBackOfCardMenu;

// Records metrics for when a user closes the back of card menu.
- (void)recordCloseBackOfCardMenu;

// Records metrics for when a user opens the native back of card menu.
- (void)recordOpenNativeBackOfCardMenu;

// Records metrics for when a user displayed a Dialog (e.g. Report content
// dialog.)
- (void)recordShowDialog;

// Records metrics for when a user dismissed a Dialog (e.g. Report content
// dialog.)
- (void)recordDismissDialog;

// Records metrics for when a user dismissed a card (e.g. Hide story, not
// interested in, etc.)
- (void)recordDismissCard;

// Records metrics for when a user undos a dismissed card (e.g. Tapping Undo in
// the Snackbar)
- (void)recordUndoDismissCard;

// Records metrics for when a user committs to a dismissed card (e.g. Undo
// snackbar was dismissed, so Undo can no longer happen.)
- (void)recordCommittDismissCard;

// Records metrics for when a Snackbar has been shown.
- (void)recordShowSnackbar;

// Records an unknown `commandID` performed by the Feed.
- (void)recordCommandID:(int)commandID;

// Records that a card was shown at `index`.
- (void)recordCardShownAtIndex:(NSUInteger)index;

// Records that a card was opened at `index`.
- (void)recordCardTappedAtIndex:(NSUInteger)index;

// Records if a notice card was presented at the time the feed was initially
// loaded. e.g. Launch time, user refreshes, and account switches.
- (void)recordNoticeCardShown:(BOOL)shown;

// Records if activity logging was enabled at the time the feed was initially
// loaded. e.g. Launch time, user refreshes, and account switches.
- (void)recordActivityLoggingEnabled:(BOOL)loggingEnabled;

// Records the `durationInSeconds` it took to Discover feed to Fetch articles.
// `success` is YES if operation was successful.
// DEPRECATED: use -recordFeedArticlesFetchDuration:success:.
- (void)recordFeedArticlesFetchDurationInSeconds:
            (NSTimeInterval)durationInSeconds
                                         success:(BOOL)success;

// Records the `duration` it took to Discover feed to Fetch articles.
// `success` is YES if operation was successful.
- (void)recordFeedArticlesFetchDuration:(base::TimeDelta)duration
                                success:(BOOL)success;

// Records the `durationInSeconds` it took to Discover feed to Fetch more
// articles (e.g. New "infinite feed" articles). `success` is YES if operation
// was successful.
// DEPRECATED: use -recordFeedMoreArticlesFetchDuration:success:.
- (void)recordFeedMoreArticlesFetchDurationInSeconds:
            (NSTimeInterval)durationInSeconds
                                             success:(BOOL)success;

// Records the `duration` it took to Discover feed to Fetch more articles
// (e.g. New "infinite feed" articles). `success` is YES if operation
// was successful.
- (void)recordFeedMoreArticlesFetchDuration:(base::TimeDelta)duration
                                    success:(BOOL)success;

// Records the `durationInSeconds` it took to Discover feed to upload actions.
// `success` is YES if operation was successful.
// DEPRECATED: use -recordFeedUploadActionsDuration:success:.
- (void)recordFeedUploadActionsDurationInSeconds:
            (NSTimeInterval)durationInSeconds
                                         success:(BOOL)success;

// Records the `duration` it took to Discover feed to upload actions.
// `success` is YES if operation was successful.
- (void)recordFeedUploadActionsDuration:(base::TimeDelta)duration
                                success:(BOOL)success;

// Records the native context menu visibility change.
- (void)recordNativeContextMenuVisibilityChanged:(BOOL)shown;

// Records the native pull-down menu visibility change.
- (void)recordNativePulldownMenuVisibilityChanged:(BOOL)shown;

// Records the broken view hierarchy before repairing it.
// TODO(crbug.com/40799579): Remove this when issue is fixed.
- (void)recordBrokenNTPHierarchy:
    (BrokenNTPHierarchyRelationship)brokenRelationship;

// Records that the feed is about to be refreshed.
- (void)recordFeedWillRefresh;

// Records that a given `feedType` was explicitly selected. Logs position in
// previous feed as `index`.
- (void)recordFeedSelected:(FeedType)feedType
    fromPreviousFeedPosition:(NSUInteger)index;

// Records the user's current follow count after a given event `logReason`.
- (void)recordFollowCount:(NSUInteger)followCount
             forLogReason:(FollowCountLogReason)logReason;

// Records the state of the Feed setting based on the `enterprisePolicy` being
// enabled, `feedVisible`, the user being `signedIn`, user having `waaEnabled`
// and `spywEnabled`, and the `lastRefreshTime` for the Feed.
- (void)recordFeedSettingsOnStartForEnterprisePolicy:(BOOL)enterprisePolicy
                                         feedVisible:(BOOL)feedVisible
                                            signedIn:(BOOL)signedIn
                                          waaEnabled:(BOOL)waaEnabled
                                         spywEnabled:(BOOL)spywEnabled
                                     lastRefreshTime:
                                         (base::Time)lastRefreshTime;

// Records a user action for the Following feed sort type being selected.
- (void)recordFollowingFeedSortTypeSelected:(FollowingFeedSortType)sortType;

// Records when the user has scrolled `scrollDistance` in a carousel within a
// cell.
- (void)recordCarouselScrolled:(int)scrollDistance;

// Records the value of the uniformity flag value from Discover.
- (void)recordUniformityFlagValue:(BOOL)flag;

#pragma mark - Follow

// Record metrics for when the user request to follow/unfollow a website,
// according to `followRequestedType`. Ex. The user selects the 'Follow' item in
// the overflow menu.
- (void)recordFollowRequestedWithType:(FollowRequestType)followRequestType;

// Record metrics for when the user tapped "follow" from menu entry point.
- (void)recordFollowFromMenu;

// Record metrics for when the user tapped "unfollow" from menu entry point.
- (void)recordUnfollowFromMenu;

// Record metrics for when the follow confirmation snckbar is shown, according
// to `followConfirmationType`.
- (void)recordFollowConfirmationShownWithType:
    (FollowConfirmationType)followConfirmationType;

// Record metrics for when the follow confirmation snckbar action is tapped,
// according to `followSnackbarActionType`.Ex. the user tapped "GO TO FEED"
// button on the follow succeed snackbar.
- (void)recordFollowSnackbarTappedWithAction:
    (FollowSnackbarActionType)followSnackbarActionType;

// Record metrics for when the user swipes or taps to unfollow a web channel in
// the management UI.
- (void)recordManagementTappedUnfollow;

// Record metrics for when the user taps "UNDO" on the successful unfollow
// confirmation snackbar in the management UI.
- (void)recordManagementTappedRefollowAfterUnfollowOnSnackbar;

// Record metrics for when the user taps "Try Again" on the unfollow error
// confirmation snackbar in the management UI.
- (void)recordManagementTappedUnfollowTryAgainOnSnackbar;

// Record metrics for when the first follow sheet is shown.
- (void)recordFirstFollowShown;

// Record metrics for when the user taps "Go To Feed" on the first follow sheet.
- (void)recordFirstFollowTappedGoToFeed;

// Record metrics for when the user taps "Got it" on the first follow sheet.
- (void)recordFirstFollowTappedGotIt;

// Record metrics for when a Follow Recommendation IPH is shown.
// A follow Recommendation IPH is a textual bublle that tells users that they
// are able to follow a website.
- (void)recordFollowRecommendationIPHShown;

#pragma mark - Sign-in Promo

// Record metrics for when a user triggered a sign-in only flow from Discover
// feed. `hasUserId` is YES when the user has one or more device-level
// identities.
- (void)recordShowSignInOnlyUIWithUserId:(BOOL)hasUserId;

// Record metrics for sign-in related UI from Discover feed personalization
// controls.
- (void)recordShowSignInRelatedUIWithType:(feed::FeedSignInUI)type;

#pragma mark - Sync Promo

// Record metrics for when a user triggered a sync related UI from Discover
// feed sync promo entry point.
- (void)recordShowSyncnRelatedUIWithType:(feed::FeedSyncPromo)type;

@end

#endif  // IOS_CHROME_BROWSER_NTP_SHARED_METRICS_FEED_METRICS_RECORDER_H_
