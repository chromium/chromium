// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_FEATURES_H_
#define IOS_CHROME_BROWSER_NTP_FEATURES_H_

#include "base/feature_list.h"

// Feature flag to enable NTP UI pending loader blocker.
extern const base::Feature kBlockNewTabPagePendingLoad;

// Feature flag to enable feed background refresh.
// Use IsFeedBackgroundRefreshEnabled() instead of this constant directly.
extern const base::Feature kEnableFeedBackgroundRefresh;

// Feature flag to enable the Following feed in the NTP.
// Use IsWebChannelsEnabled() instead of this constant directly.
extern const base::Feature kEnableWebChannels;

// Feature param under `kEnableFeedBackgroundRefresh` to also enable background
// refresh for the Following feed.
extern const char kEnableFollowingFeedBackgroundRefresh[];

// Feature param under `kEnableFeedBackgroundRefresh` to enable server driven
// background refresh schedule.
extern const char kEnableServerDrivenBackgroundRefreshSchedule[];

// Feature param under `kEnableFeedBackgroundRefresh` to enable recurring
// background refresh schedule.
extern const char kEnableRecurringBackgroundRefreshSchedule[];

// Feature param under `kEnableFeedBackgroundRefresh` for the background refresh
// interval in seconds.
extern const char kBackgroundRefreshIntervalInSeconds[];

// Feature param under `kEnableFeedBackgroundRefresh` for the background refresh
// max age in seconds. This value is compared against the age of the feed when
// performing a background refresh. A zero value means the age check is ignored.
extern const char kBackgroundRefreshMaxAgeInSeconds[];

// Whether the Following Feed is enabled on NTP.
bool IsWebChannelsEnabled();

// Whether feed background refresh is enabled. Returns the value in
// NSUserDefaults set by `SaveFeedBackgroundRefreshEnabledForNextColdStart()`.
// This function always returns false if the `IOS_BACKGROUND_MODE_ENABLED`
// buildflag is not defined.
bool IsFeedBackgroundRefreshEnabled();

// Whether feed background refresh is attempted, if background refresh is
// enabled. Disabling this value allows for testing the background fetch
// capability independent of the feed background refresh codepaths.
bool IsAttemptFeedBackgroundRefreshEnabled();

// Saves the current value for feature `kEnableFeedBackgroundRefresh`. This call
// DCHECKs on the availability of `base::FeatureList`.
void SaveFeedBackgroundRefreshEnabledForNextColdStart();

// Returns true if the user should receive a local notification when a feed
// background refresh is completed. Background refresh completion notifications
// are only compiled into non-release versions.
bool IsFeedBackgroundRefreshCompletedNotificationEnabled();

// Whether the Following feed should also be refreshed in the background.
bool IsFollowingFeedBackgroundRefreshEnabled();

// Whether the background refresh schedule should be driven by server values.
bool IsServerDrivenBackgroundRefreshScheduleEnabled();

// Whether a new refresh should be scheduled after completion of a previous
// background refresh.
bool IsRecurringBackgroundRefreshScheduleEnabled();

// The earliest interval to refresh if server value is not used. This value is
// an input into the DiscoverFeedService.
double GetBackgroundRefreshIntervalInSeconds();

// If greater than zero, this value should be used to completely override the
// earliest begin date provided by the DiscoverFeedService.
double GetBackgroundRefreshIntervalOverrideInSeconds();

// Returns the background refresh max age in seconds.
double GetBackgroundRefreshMaxAgeInSeconds();

#endif  // IOS_CHROME_BROWSER_NTP_FEATURES_H_
