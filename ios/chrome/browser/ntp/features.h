// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_FEATURES_H_
#define IOS_CHROME_BROWSER_NTP_FEATURES_H_

#import <Foundation/Foundation.h>

#include "base/feature_list.h"

// Feature flag to enable feed background refresh.
// Use IsFeedBackgroundRefreshEnabled() instead of this constant directly.
BASE_DECLARE_FEATURE(kEnableFeedBackgroundRefresh);

// Feature flag to enable feed foreground refresh. Check feature params instead
// of using this constant.
BASE_DECLARE_FEATURE(kEnableFeedForegroundRefresh);

// Feature flag to enable the Following feed in the NTP.
// Use IsWebChannelsEnabled() instead of this constant directly.
BASE_DECLARE_FEATURE(kEnableWebChannels);

// Feature flag to enable Feed bottom sign-in promo feature, which displays a
// sign-in promotion card at the bottom of the Discover Feed for signed out
// users. Use IsFeedBottomSignInPromoEnabled() instead of this constant
// directly.
BASE_DECLARE_FEATURE(kEnableFeedBottomSignInPromo);

// Feature flag to enable Feed card menu promo feature, which displays a sign-in
// promotion UI when signed out users click on personalization options within
// the feed card menu.
// Use IsFeedCardMenuSignInPromoEnabled() instead of this constant directly.
BASE_DECLARE_FEATURE(kEnableFeedCardMenuSignInPromo);

// Feature flag to disable the feed.
BASE_DECLARE_FEATURE(kEnableFeedAblation);

// Feature flag to enable feed experiment tagging.
BASE_DECLARE_FEATURE(kEnableFeedExperimentTagging);

// Feature param under `kEnableFeedBackgroundRefresh` to also enable background
// refresh for the Following feed.
extern const char kEnableFollowingFeedBackgroundRefresh[];

// Feature param under `kEnableFeedBackgroundRefresh` to enable server driven
// background refresh schedule.
extern const char kEnableServerDrivenBackgroundRefreshSchedule[];

// Feature param under `kEnableFeedBackgroundRefresh` to enable recurring
// background refresh schedule.
extern const char kEnableRecurringBackgroundRefreshSchedule[];

// Feature param under `kEnableFeedBackgroundRefresh` for the max age that the
// cache is still considered fresh.
extern const char kMaxCacheAgeInSeconds[];

// Feature param under `kEnableFeedBackgroundRefresh` for the background refresh
// interval in seconds.
extern const char kBackgroundRefreshIntervalInSeconds[];

// Feature param under `kEnableFeedBackgroundRefresh` for the background refresh
// max age in seconds. This value is compared against the age of the feed when
// performing a background refresh. A zero value means the age check is ignored.
extern const char kBackgroundRefreshMaxAgeInSeconds[];

// Feature param under `kEnableFeedForegroundRefresh` to enable refresh
// following a Feed session.
extern const char kEnableFeedRefreshPostFeedSession[];

// Feature param under `kEnableFeedForegroundRefresh` to enable refresh on app
// backgrounding.
extern const char kEnableFeedRefreshOnAppBackgrounding[];

// Feature param under `kEnableFeedForegroundRefresh` for the time interval used
// to set the session end timer.
extern const char kFeedSessionEndTimerTimeoutInSeconds[];

// Feature param under `kEnableFeedForegroundRefresh` for the refresh threshold
// when the last refresh was seen.
extern const char kFeedSeenRefreshThresholdInSeconds[];

// Feature param under `kEnableFeedForegroundRefresh` for the refresh threshold
// when the last refresh was unseen.
extern const char kFeedUnseenRefreshThresholdInSeconds[];

// Whether the Following Feed is enabled on NTP.
bool IsWebChannelsEnabled();

// Whether the Discover service is created early, alongside the app creation.
bool IsDiscoverFeedServiceCreatedEarly();

// Whether feed background refresh is enabled. Returns the value in
// NSUserDefaults set by `SaveFeedBackgroundRefreshEnabledForNextColdStart()`.
// This function always returns false if the `IOS_BACKGROUND_MODE_ENABLED`
// buildflag is not defined.
bool IsFeedBackgroundRefreshEnabled();

// Whether Good Visits metric logging is enabled.
bool IsGoodVisitsMetricEnabled();

// Saves the current value for feature `kEnableFeedBackgroundRefresh`. This call
// DCHECKs on the availability of `base::FeatureList`.
void SaveFeedBackgroundRefreshEnabledForNextColdStart();

// Sets `timestamp` for key `NSUserDefaultsKey` to be displayed in Experimental
// Settings in the Settings App. This is not available in stable.
void SetFeedRefreshTimestamp(NSDate* timestamp, NSString* NSUserDefaultsKey);

// Returns the override value from Experimental Settings in the Settings App. If
// enabled, all values in Experimental Settings will override all corresponding
// defaults.
bool IsFeedOverrideDefaultsEnabled();

// Returns true if the user should receive a local notification when a feed
// background refresh is completed. Background refresh completion notifications
// are only enabled by Experimental Settings.
bool IsFeedBackgroundRefreshCompletedNotificationEnabled();

// Whether the Following feed should also be refreshed in the background.
bool IsFollowingFeedBackgroundRefreshEnabled();

// Whether the background refresh schedule should be driven by server values.
bool IsServerDrivenBackgroundRefreshScheduleEnabled();

// Whether a new refresh should be scheduled after completion of a previous
// background refresh.
bool IsRecurringBackgroundRefreshScheduleEnabled();

// Returns the max age that the cache is still considered fresh. In other words,
// the feed freshness threshold.
double GetFeedMaxCacheAgeInSeconds();

// The earliest interval to refresh if server value is not used. This value is
// an input into the DiscoverFeedService.
double GetBackgroundRefreshIntervalInSeconds();

// Returns the background refresh max age in seconds.
double GetBackgroundRefreshMaxAgeInSeconds();

// Whether feed is refreshed after the user ends a Feed session, but while the
// app is still in the foreground (e.g., user switches tabs, user navigates away
// from Feed in current tab).
bool IsFeedRefreshPostFeedSessionEnabled();

// Whether feed is refreshed at the moment the app is backgrounding. This is
// different from background refresh.
bool IsFeedRefreshOnAppBackgroundingEnabled();

// Returns the time interval used to set the session end timer.
double GetFeedSessionEndTimerTimeoutInSeconds();

// Returns the refresh threshold (aka feed expiration) for a feed that has been
// seen.
double GetFeedSeenRefreshThresholdInSeconds();

// Returns the refresh threshold (aka feed expiration) for an unseen feed.
double GetFeedUnseenRefreshThresholdInSeconds();

// YES if enabled Feed bottom sign-in promo.
bool IsFeedBottomSignInPromoEnabled();

// YES if enabled Feed card menu promo.
bool IsFeedCardMenuSignInPromoEnabled();

// Whether the feed is disabled.
bool IsFeedAblationEnabled();

// Whether the feed experiment tagging is enabled.
bool IsFeedExperimentTaggingEnabled();

#endif  // IOS_CHROME_BROWSER_NTP_FEATURES_H_
