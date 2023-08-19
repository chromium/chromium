// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/features.h"

#import <Foundation/Foundation.h>

#import "base/metrics/field_trial_params.h"
#import "components/variations/service/variations_service.h"
#import "components/version_info/channel.h"
#import "ios/chrome/app/background_mode_buildflags.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/common/channel_info.h"

namespace {

// Whether feed background refresh is enabled. This only checks if the feature
// is enabled, not if the capability was enabled at startup.
bool IsFeedBackgroundRefreshEnabledOnly() {
  return base::FeatureList::IsEnabled(kEnableFeedBackgroundRefresh);
}

// Whether feed is refreshed in the background soon after the app is
// backgrounded. This only checks if the feature is enabled, not if the
// capability was enabled at startup.
bool IsFeedAppCloseBackgroundRefreshEnabledOnly() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kEnableFeedInvisibleForegroundRefresh,
      kEnableFeedAppCloseBackgroundRefresh,
      /*default=*/false);
}

// Returns the override value from the Foreground Refresh section of Feed
// Refresh Settings in Experimental Settings in the Settings App.
bool IsFeedOverrideForegroundDefaultsEnabled() {
  if (GetChannel() == version_info::Channel::STABLE) {
    return false;
  }
  return [[NSUserDefaults standardUserDefaults]
      boolForKey:@"FeedOverrideForegroundDefaultsEnabled"];
}

}  // namespace

BASE_FEATURE(kEnableWebChannels,
             "EnableWebChannels",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableFeedBackgroundRefresh,
             "EnableFeedBackgroundRefresh",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableFeedInvisibleForegroundRefresh,
             "EnableFeedInvisibleForegroundRefresh",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCreateDiscoverFeedServiceEarly,
             "CreateDiscoverFeedServiceEarly",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableFeedCardMenuSignInPromo,
             "EnableFeedCardMenuSignInPromo",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableFeedAblation,
             "EnableFeedAblation",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableFeedExperimentTagging,
             "EnableFeedExperimentTagging",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kIOSSetUpList, "IOSSetUpList", base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFeedDisableHotStartRefresh,
             "FeedDisableHotStartRefresh",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableFollowUIUpdate,
             "EnableFollowUIUpdate",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDiscoverFeedSportCard,
             "DiscoverFeedSportCard",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Key for NSUserDefaults containing a bool indicating whether the next run
// should enable feed background refresh capability. This is used because
// registering for background refreshes must happen early in app initialization
// and FeatureList is not yet available. Enabling or disabling background
// refresh features will always take effect after two cold starts after the
// feature has been changed on the server (once for the Finch configuration, and
// another for reading the stored value from NSUserDefaults).
NSString* const kEnableFeedBackgroundRefreshCapabilityForNextColdStart =
    @"EnableFeedBackgroundRefreshCapabilityForNextColdStart";

const char kEnableFollowingFeedBackgroundRefresh[] =
    "EnableFollowingFeedBackgroundRefresh";
const char kEnableServerDrivenBackgroundRefreshSchedule[] =
    "EnableServerDrivenBackgroundRefreshSchedule";
const char kEnableRecurringBackgroundRefreshSchedule[] =
    "EnableRecurringBackgroundRefreshSchedule";
const char kMaxCacheAgeInSeconds[] = "MaxCacheAgeInSeconds";
const char kBackgroundRefreshIntervalInSeconds[] =
    "BackgroundRefreshIntervalInSeconds";
const char kBackgroundRefreshMaxAgeInSeconds[] =
    "BackgroundRefreshMaxAgeInSeconds";
const char kEnableFeedSessionCloseForegroundRefresh[] =
    "EnableFeedSessionCloseForegroundRefresh";
const char kEnableFeedAppCloseForegroundRefresh[] =
    "EnableFeedAppCloseForegroundRefresh";
const char kEnableFeedAppCloseBackgroundRefresh[] =
    "EnableFeedAppCloseBackgroundRefresh";
const char kFeedRefreshEngagementCriteriaType[] =
    "FeedRefreshEngagementCriteriaType";
const char kAppCloseBackgroundRefreshIntervalInSeconds[] =
    "AppCloseBackgroundRefreshIntervalInSeconds";
const char kFeedRefreshTimerTimeoutInSeconds[] =
    "FeedRefreshTimerTimeoutInSeconds";
const char kFeedSeenRefreshThresholdInSeconds[] =
    "FeedSeenRefreshThresholdInSeconds";
const char kFeedUnseenRefreshThresholdInSeconds[] =
    "FeedUnseenRefreshThresholdInSeconds";
const char kEnableFeedUseInteractivityInvalidationForForegroundRefreshes[] =
    "EnableFeedUseInteractivityInvalidationForForegroundRefreshes";

bool IsWebChannelsEnabled() {
  variations::VariationsService* variations_service =
      GetApplicationContext()->GetVariationsService();
  if (variations_service &&
      variations_service->GetStoredPermanentCountry() == "us") {
    return true;
  }
  return base::FeatureList::IsEnabled(kEnableWebChannels);
}

bool IsDiscoverFeedServiceCreatedEarly() {
  return base::FeatureList::IsEnabled(kCreateDiscoverFeedServiceEarly);
}

bool IsFeedBackgroundRefreshEnabled() {
  return IsFeedBackgroundRefreshCapabilityEnabled() &&
         IsFeedBackgroundRefreshEnabledOnly();
}

bool IsFeedBackgroundRefreshCapabilityEnabled() {
#if !BUILDFLAG(IOS_BACKGROUND_MODE_ENABLED)
  return false;
#else
  static bool feedBackgroundRefreshEnabled =
      [[NSUserDefaults standardUserDefaults]
          boolForKey:kEnableFeedBackgroundRefreshCapabilityForNextColdStart];
  return feedBackgroundRefreshEnabled;
#endif  // BUILDFLAG(IOS_BACKGROUND_MODE_ENABLED)
}

void SaveFeedBackgroundRefreshCapabilityEnabledForNextColdStart() {
  DCHECK(base::FeatureList::GetInstance());
  BOOL enabled = IsFeedBackgroundRefreshEnabledOnly() ||
                 IsFeedAppCloseBackgroundRefreshEnabledOnly();
  [[NSUserDefaults standardUserDefaults]
      setBool:enabled
       forKey:kEnableFeedBackgroundRefreshCapabilityForNextColdStart];
}

void SetFeedRefreshTimestamp(NSDate* timestamp, NSString* NSUserDefaultsKey) {
  NSDateFormatter* dateFormatter = [[NSDateFormatter alloc] init];
  dateFormatter.dateStyle = NSDateFormatterShortStyle;
  dateFormatter.timeStyle = NSDateFormatterShortStyle;
  dateFormatter.locale = [NSLocale autoupdatingCurrentLocale];
  [[NSUserDefaults standardUserDefaults]
      setObject:[dateFormatter stringFromDate:timestamp]
         forKey:NSUserDefaultsKey];
}

bool IsFeedOverrideDefaultsEnabled() {
  if (GetChannel() == version_info::Channel::STABLE) {
    return false;
  }
  return [[NSUserDefaults standardUserDefaults]
      boolForKey:@"FeedOverrideDefaultsEnabled"];
}

bool IsFeedBackgroundRefreshCompletedNotificationEnabled() {
  if (GetChannel() == version_info::Channel::STABLE) {
    return false;
  }
  return IsFeedBackgroundRefreshCapabilityEnabled() &&
         [[NSUserDefaults standardUserDefaults]
             boolForKey:@"FeedBackgroundRefreshNotificationEnabled"];
}

bool IsFollowingFeedBackgroundRefreshEnabled() {
  if (IsFeedOverrideDefaultsEnabled()) {
    return [[NSUserDefaults standardUserDefaults]
        boolForKey:@"FollowingFeedBackgroundRefreshEnabled"];
  }
  return base::GetFieldTrialParamByFeatureAsBool(
      kEnableFeedBackgroundRefresh, kEnableFollowingFeedBackgroundRefresh,
      /*default=*/false);
}

bool IsServerDrivenBackgroundRefreshScheduleEnabled() {
  if (IsFeedOverrideDefaultsEnabled()) {
    return [[NSUserDefaults standardUserDefaults]
        boolForKey:@"FeedServerDrivenBackgroundRefreshScheduleEnabled"];
  }
  return base::GetFieldTrialParamByFeatureAsBool(
      kEnableFeedBackgroundRefresh,
      kEnableServerDrivenBackgroundRefreshSchedule, /*default=*/false);
}

bool IsRecurringBackgroundRefreshScheduleEnabled() {
  if (IsFeedOverrideDefaultsEnabled()) {
    return [[NSUserDefaults standardUserDefaults]
        boolForKey:@"FeedRecurringBackgroundRefreshScheduleEnabled"];
  }
  return base::GetFieldTrialParamByFeatureAsBool(
      kEnableFeedBackgroundRefresh, kEnableRecurringBackgroundRefreshSchedule,
      /*default=*/false);
}

double GetFeedMaxCacheAgeInSeconds() {
  if (IsFeedOverrideDefaultsEnabled()) {
    return [[NSUserDefaults standardUserDefaults]
        doubleForKey:@"FeedMaxCacheAgeInSeconds"];
  }
  return base::GetFieldTrialParamByFeatureAsDouble(kEnableFeedBackgroundRefresh,
                                                   kMaxCacheAgeInSeconds,
                                                   /*default=*/8 * 60 * 60);
}

double GetBackgroundRefreshIntervalInSeconds() {
  if (IsFeedOverrideDefaultsEnabled()) {
    return [[NSUserDefaults standardUserDefaults]
        doubleForKey:@"FeedBackgroundRefreshIntervalInSeconds"];
  }
  return base::GetFieldTrialParamByFeatureAsDouble(
      kEnableFeedBackgroundRefresh, kBackgroundRefreshIntervalInSeconds,
      /*default=*/60 * 60);
}

double GetBackgroundRefreshMaxAgeInSeconds() {
  return base::GetFieldTrialParamByFeatureAsDouble(
      kEnableFeedBackgroundRefresh, kBackgroundRefreshMaxAgeInSeconds,
      /*default=*/0);
}

bool IsFeedInvisibleForegroundRefreshEnabled() {
  return base::FeatureList::IsEnabled(kEnableFeedInvisibleForegroundRefresh);
}

bool IsFeedSessionCloseForegroundRefreshEnabled() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kEnableFeedInvisibleForegroundRefresh,
      kEnableFeedSessionCloseForegroundRefresh,
      /*default=*/false);
}

bool IsFeedAppCloseForegroundRefreshEnabled() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kEnableFeedInvisibleForegroundRefresh,
      kEnableFeedAppCloseForegroundRefresh,
      /*default=*/false);
}

bool IsFeedAppCloseBackgroundRefreshEnabled() {
  return IsFeedBackgroundRefreshCapabilityEnabled() &&
         IsFeedAppCloseBackgroundRefreshEnabledOnly();
}

FeedRefreshEngagementCriteriaType GetFeedRefreshEngagementCriteriaType() {
  return (FeedRefreshEngagementCriteriaType)
      base::GetFieldTrialParamByFeatureAsInt(
          kEnableFeedInvisibleForegroundRefresh,
          kFeedRefreshEngagementCriteriaType,
          /*default_value=*/
          (int)FeedRefreshEngagementCriteriaType::kSimpleEngagement);
}

double GetAppCloseBackgroundRefreshIntervalInSeconds() {
  double override_value = [[NSUserDefaults standardUserDefaults]
      doubleForKey:@"AppCloseBackgroundRefreshIntervalInSeconds"];
  if (override_value > 0.0) {
    return override_value;
  }
  return base::GetFieldTrialParamByFeatureAsDouble(
      kEnableFeedInvisibleForegroundRefresh,
      kAppCloseBackgroundRefreshIntervalInSeconds,
      /*default=*/base::Minutes(5).InSecondsF());
}

double GetFeedRefreshTimerTimeoutInSeconds() {
  double override_value = [[NSUserDefaults standardUserDefaults]
      doubleForKey:@"FeedRefreshTimerTimeoutInSeconds"];
  if (override_value > 0.0) {
    return override_value;
  }
  return base::GetFieldTrialParamByFeatureAsDouble(
      kEnableFeedInvisibleForegroundRefresh, kFeedRefreshTimerTimeoutInSeconds,
      /*default=*/base::Minutes(5).InSecondsF());
}

double GetFeedSeenRefreshThresholdInSeconds() {
  double override_value = [[NSUserDefaults standardUserDefaults]
      doubleForKey:@"FeedSeenRefreshThresholdInSeconds"];
  if (override_value > 0.0) {
    return override_value;
  }
  return base::GetFieldTrialParamByFeatureAsDouble(
      kEnableFeedInvisibleForegroundRefresh, kFeedSeenRefreshThresholdInSeconds,
      /*default=*/base::Hours(1).InSecondsF());
}

double GetFeedUnseenRefreshThresholdInSeconds() {
  double override_value = [[NSUserDefaults standardUserDefaults]
      doubleForKey:@"FeedUnseenRefreshThresholdInSeconds"];
  if (override_value > 0.0) {
    return override_value;
  }
  return base::GetFieldTrialParamByFeatureAsDouble(
      kEnableFeedInvisibleForegroundRefresh,
      kFeedUnseenRefreshThresholdInSeconds,
      /*default=*/base::Hours(6).InSecondsF());
}

bool IsFeedUseInteractivityInvalidationForForegroundRefreshesEnabled() {
  if (IsFeedOverrideForegroundDefaultsEnabled()) {
    return [[NSUserDefaults standardUserDefaults]
        doubleForKey:
            @"FeedUseInteractivityInvalidationForForegroundRefreshesEnabled"];
  }
  return base::GetFieldTrialParamByFeatureAsBool(
      kEnableFeedInvisibleForegroundRefresh,
      kEnableFeedUseInteractivityInvalidationForForegroundRefreshes,
      /*default=*/false);
}

bool IsFeedCardMenuSignInPromoEnabled() {
  return base::FeatureList::IsEnabled(kEnableFeedCardMenuSignInPromo);
}

bool IsFeedAblationEnabled() {
  return base::FeatureList::IsEnabled(kEnableFeedAblation);
}

bool IsFeedExperimentTaggingEnabled() {
  return base::FeatureList::IsEnabled(kEnableFeedExperimentTagging);
}

bool IsIOSSetUpListEnabled() {
  return base::FeatureList::IsEnabled(kIOSSetUpList);
}

bool IsFeedHotStartRefreshDisabled() {
  return base::FeatureList::IsEnabled(kFeedDisableHotStartRefresh);
}

bool IsFollowUIUpdateEnabled() {
  return base::FeatureList::IsEnabled(kEnableFollowUIUpdate);
}
