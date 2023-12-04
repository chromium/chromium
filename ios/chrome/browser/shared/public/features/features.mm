// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/public/features/features.h"

#import "base/containers/contains.h"
#import "base/metrics/field_trial_params.h"
#import "components/country_codes/country_codes.h"
#import "components/version_info/channel.h"
#import "ios/chrome/app/background_mode_buildflags.h"
#import "ios/chrome/common/channel_info.h"
#import "ui/base/device_form_factor.h"

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

BASE_FEATURE(kIOSKeyboardAccessoryUpgrade,
             "kIOSKeyboardAccessoryUpgrade",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kIOSPaymentsBottomSheet,
             "IOSPaymentsBottomSheet",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTestFeature, "TestFeature", base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSafetyCheckMagicStack,
             "SafetyCheckMagicStack",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSharedHighlightingIOS,
             "SharedHighlightingIOS",
             base::FEATURE_ENABLED_BY_DEFAULT);

// TODO(crbug.com/1128242): Remove this flag after the refactoring work is
// finished.
BASE_FEATURE(kModernTabStrip,
             "ModernTabStrip",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kIncognitoNtpRevamp,
             "IncognitoNtpRevamp",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDefaultBrowserIntentsShowSettings,
             "DefaultBrowserIntentsShowSettings",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kIOSBrowserEditMenuMetrics,
             "IOSBrowserEditMenuMetrics",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kNonModalDefaultBrowserPromoCooldownRefactor,
             "NonModalDefaultBrowserPromoCooldownRefactor",
             base::FEATURE_DISABLED_BY_DEFAULT);

constexpr base::FeatureParam<int>
    kNonModalDefaultBrowserPromoCooldownRefactorParam{
        &kNonModalDefaultBrowserPromoCooldownRefactor,
        /*name=*/"cooldown-days", /*default_value=*/14};

BASE_FEATURE(kDefaultBrowserGenericTailoredPromoTrain,
             "DefaultBrowserGenericTailoredPromoTrain",
             base::FEATURE_DISABLED_BY_DEFAULT);

constexpr base::FeatureParam<DefaultBrowserPromoGenericTailoredArm>::Option
    kDefaultBrowserPromoGenericTailoredArmOptions[] = {
        {DefaultBrowserPromoGenericTailoredArm::kOnlyGeneric, "only-generic"},
        {DefaultBrowserPromoGenericTailoredArm::kOnlyTailored,
         "only-tailored"}};

const base::FeatureParam<DefaultBrowserPromoGenericTailoredArm>
    kDefaultBrowserPromoGenericTailoredParam{
        &kDefaultBrowserGenericTailoredPromoTrain, "experiment-arm",
        DefaultBrowserPromoGenericTailoredArm::kOnlyGeneric,
        &kDefaultBrowserPromoGenericTailoredArmOptions};

BASE_FEATURE(kDefaultBrowserVideoPromo,
             "DefaultBrowserVideoPromo",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kIOSEditMenuPartialTranslateNoIncognitoParam[] =
    "IOSEditMenuPartialTranslateNoIncognitoParam";

BASE_FEATURE(kIOSEditMenuPartialTranslate,
             "IOSEditMenuPartialTranslate",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsPartialTranslateEnabled() {
  if (@available(iOS 16, *)) {
    return base::FeatureList::IsEnabled(kIOSEditMenuPartialTranslate);
  }
  return false;
}

bool ShouldShowPartialTranslateInIncognito() {
  if (!IsPartialTranslateEnabled()) {
    return false;
  }
  return !base::GetFieldTrialParamByFeatureAsBool(
      kIOSEditMenuPartialTranslate,
      kIOSEditMenuPartialTranslateNoIncognitoParam, false);
}

const char kIOSEditMenuSearchWithTitleParamTitle[] =
    "IOSEditMenuSearchWithTitleParam";
const char kIOSEditMenuSearchWithTitleSearchParam[] = "Search";
const char kIOSEditMenuSearchWithTitleSearchWithParam[] = "SearchWith";
const char kIOSEditMenuSearchWithTitleWebSearchParam[] = "WebSearch";
BASE_FEATURE(kIOSEditMenuSearchWith,
             "IOSEditMenuSearchWith",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsSearchWithEnabled() {
  if (@available(iOS 16, *)) {
    return base::FeatureList::IsEnabled(kIOSEditMenuSearchWith);
  }
  return false;
}

BASE_FEATURE(kIOSEditMenuHideSearchWeb,
             "IOSEditMenuHideSearchWeb",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kIOSNewOmniboxImplementation,
             "kIOSNewOmniboxImplementation",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableLensInOmniboxCopiedImage,
             "EnableLensInOmniboxCopiedImage",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableTraitCollectionWorkAround,
             "EnableTraitCollectionWorkAround",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableUIButtonConfiguration,
             "EnableUIButtonConfiguration",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsUIButtonConfigurationEnabled() {
  return base::FeatureList::IsEnabled(kEnableUIButtonConfiguration);
}

BASE_FEATURE(kRemoveExcessNTPs,
             "RemoveExcessNTPs",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableShortenedPasswordAutoFillInstruction,
             "EnableShortenedPasswordAutoFillInstruction",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableExpKitAppleCalendar,
             "EnableExpKitAppleCalendar",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTCRexKillSwitch,
             "kTCRexKillSwitch",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabGridNewTransitions,
             "TabGridNewTransitions",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsNewTabGridTransitionsEnabled() {
  return base::FeatureList::IsEnabled(kTabGridNewTransitions);
}

BASE_FEATURE(kNonModalDefaultBrowserPromoImpressionLimit,
             "NonModalDefaultBrowserPromoImpressionLimit",
             base::FEATURE_ENABLED_BY_DEFAULT);

constexpr base::FeatureParam<int>
    kNonModalDefaultBrowserPromoImpressionLimitParam{
        &kNonModalDefaultBrowserPromoImpressionLimit,
        /*name=*/"impression-limit", /*default_value=*/3};

BASE_FEATURE(kNotificationSettingsMenuItem,
             "NotificationSettingsMenuItem",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSpotlightOpenTabsSource,
             "SpotlightOpenTabsSource",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSpotlightReadingListSource,
             "SpotlightReadingListSource",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSpotlightDonateNewIntents,
             "SpotlightDonateNewIntents",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kConsistencyNewAccountInterface,
             "ConsistencyNewAccountInterface",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsConsistencyNewAccountInterfaceEnabled() {
  return base::FeatureList::IsEnabled(kConsistencyNewAccountInterface);
}

BASE_FEATURE(kNewNTPOmniboxLayout,
             "kNewNTPOmniboxLayout",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kBottomOmniboxSteadyState,
             "BottomOmniboxSteadyState",
             base::FEATURE_ENABLED_BY_DEFAULT);

const char kBottomOmniboxDefaultSettingParam[] =
    "BottomOmniboxDefaultSettingParam";
const char kBottomOmniboxDefaultSettingParamTop[] = "Top";
const char kBottomOmniboxDefaultSettingParamBottom[] = "Bottom";
const char kBottomOmniboxDefaultSettingParamSafariSwitcher[] =
    "BottomSafariSwitcher";
BASE_FEATURE(kBottomOmniboxDefaultSetting,
             "BottomOmniboxDefaultSetting",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kBottomOmniboxDeviceSwitcherResults,
             "BottomOmniboxDeviceSwitcherResults",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsBottomOmniboxSteadyStateEnabled() {
  // Bottom omnibox is only available on phones.
  if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_PHONE) {
    return false;
  }
  return base::FeatureList::IsEnabled(kBottomOmniboxSteadyState);
}

bool IsBottomOmniboxDeviceSwitcherResultsEnabled() {
  return IsBottomOmniboxSteadyStateEnabled() &&
         base::FeatureList::IsEnabled(kBottomOmniboxDeviceSwitcherResults);
}

BASE_FEATURE(kBottomOmniboxPromoFRE,
             "BottomOmniboxPromoFRE",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kBottomOmniboxPromoAppLaunch,
             "BottomOmniboxPromoAppLaunch",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kBottomOmniboxPromoParam[] = "BottomOmniboxPromoParam";
const char kBottomOmniboxPromoParamForced[] = "Forced";

bool IsBottomOmniboxPromoFlagEnabled(BottomOmniboxPromoType type) {
  if (!IsBottomOmniboxSteadyStateEnabled()) {
    return false;
  }
  if ((type == BottomOmniboxPromoType::kFRE ||
       type == BottomOmniboxPromoType::kAny) &&
      base::FeatureList::IsEnabled(kBottomOmniboxPromoFRE)) {
    return true;
  }
  if ((type == BottomOmniboxPromoType::kAppLaunch ||
       type == BottomOmniboxPromoType::kAny) &&
      base::FeatureList::IsEnabled(kBottomOmniboxPromoAppLaunch)) {
    return true;
  }
  return false;
}

BASE_FEATURE(kBottomOmniboxPromoDefaultPosition,
             "BottomOmniboxPromoDefaultPosition",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kBottomOmniboxPromoDefaultPositionParam[] =
    "BottomOmniboxPromoDefaultPositionParam";
const char kBottomOmniboxPromoDefaultPositionParamTop[] = "Top";
const char kBottomOmniboxPromoDefaultPositionParamBottom[] = "Bottom";

BASE_FEATURE(kOnlyAccessClipboardAsync,
             "OnlyAccessClipboardAsync",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kDefaultBrowserVideoInSettings,
             "DefaultBrowserVideoInSettings",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kDefaultBrowserTriggerCriteriaExperiment,
             "DefaultBrowserTriggerCriteriaExperiment",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFullScreenPromoOnOmniboxCopyPaste,
             "FullScreenPromoOnOmniboxCopyPaste",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kThemeColorInTopToolbar,
             "ThemeColorInTopToolbar",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kDynamicThemeColor,
             "DynamicThemeColor",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kDynamicBackgroundColor,
             "DynamicBackgroundColor",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabGridCompositionalLayout,
             "TabGridCompositionalLayout",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsTabGridCompositionalLayoutEnabled() {
  return base::FeatureList::IsEnabled(kTabGridCompositionalLayout);
}

BASE_FEATURE(kTabGridRefactoring,
             "TabGridRefactoring",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsSafetyCheckMagicStackEnabled() {
  return base::FeatureList::IsEnabled(kSafetyCheckMagicStack);
}

BASE_FEATURE(kIOSSaveToDrive,
             "IOSSaveToDrive",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kIOSSaveToPhotos,
             "IOSSaveToPhotos",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableUIEditMenuInteraction,
             "EnableUIEditMenuInteraction",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kHistoryOptInForRestoreShortyAndReSignin,
             "HistoryOptInForRestoreShortyAndReSignin",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableBatchUploadFromBookmarksManager,
             "EnableBatchUploadFromBookmarksManager",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableReviewAccountSettingsPromo,
             "EnableReviewAccountSettingsPromo",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableWebChannels,
             "EnableWebChannels",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableFeedBackgroundRefresh,
             "EnableFeedBackgroundRefresh",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableFeedInvisibleForegroundRefresh,
             "EnableFeedInvisibleForegroundRefresh",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCreateDiscoverFeedServiceEarly,
             "CreateDiscoverFeedServiceEarly",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableFeedAblation,
             "EnableFeedAblation",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableFeedExperimentTagging,
             "EnableFeedExperimentTagging",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kFeedDisableHotStartRefresh,
             "FeedDisableHotStartRefresh",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableFollowUIUpdate,
             "EnableFollowUIUpdate",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDiscoverFeedSportCard,
             "DiscoverFeedSportCard",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kContentPushNotificationsExperimentType[] =
    "ContentPushNotificationsExperimentType";

BASE_FEATURE(kContentPushNotifications,
             "ContentPushNotifications",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kIOSLargeFakebox,
             "IOSLargeFakebox",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kIOSHideFeedWithSearchChoice,
             "IOSHideFeedWithSearchChoice",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFullscreenImprovement,
             "FullscreenImprovement",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabGroupsInGrid,
             "TabGroupsInGrid",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kIOSExternalActionURLs,
             "IOSExternalActionURLs",
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
const char kIOSHideFeedWithSearchChoiceTargeted[] =
    "IOSHideFeedWithSearchChoiceTargeted";

bool IsWebChannelsEnabled() {
  std::string launched_countries[5] = {"AU", "GB", "NZ", "US", "ZA"};
  if (base::Contains(launched_countries,
                     country_codes::GetCurrentCountryCode())) {
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
      /*default=*/true);
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

bool IsIOSHideFeedWithSearchChoiceTargeted() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kIOSHideFeedWithSearchChoice, kIOSHideFeedWithSearchChoiceTargeted,
      /*default=*/false);
}

bool IsFeedAblationEnabled() {
  return base::FeatureList::IsEnabled(kEnableFeedAblation);
}

bool IsFeedExperimentTaggingEnabled() {
  return base::FeatureList::IsEnabled(kEnableFeedExperimentTagging);
}

bool IsFeedHotStartRefreshDisabled() {
  return base::FeatureList::IsEnabled(kFeedDisableHotStartRefresh);
}

bool IsFollowUIUpdateEnabled() {
  return base::FeatureList::IsEnabled(kEnableFollowUIUpdate);
}

bool IsContentPushNotificationsEnabled() {
  return base::FeatureList::IsEnabled(kContentPushNotifications);
}

NotificationsExperimentType ContentNotificationsExperimentTypeEnabled() {
  // This translates to the `NotificationsExperimentType` enum.
  // Value 0 corresponds to `Enabled` on the feature flag. Only activates the
  // Settings tab for content notifications.
  return static_cast<NotificationsExperimentType>(
      base::GetFieldTrialParamByFeatureAsInt(
          kContentPushNotifications, kContentPushNotificationsExperimentType,
          0));
}

bool IsContentPushNotificationsPromoEnabled() {
  return (ContentNotificationsExperimentTypeEnabled() ==
          NotificationsExperimentTypePromoEnabled);
}

bool IsContentPushNotificationsSetUpListEnabled() {
  return (ContentNotificationsExperimentTypeEnabled() ==
          NotificationsExperimentTypeSetUpListsEnabled);
}

bool IsIOSLargeFakeboxEnabled() {
  return base::FeatureList::IsEnabled(kIOSLargeFakebox);
}

bool IsIOSHideFeedWithSearchChoiceEnabled() {
  return base::FeatureList::IsEnabled(kIOSHideFeedWithSearchChoice);
}

bool IsKeyboardAccessoryUpgradeEnabled() {
  return base::FeatureList::IsEnabled(kIOSKeyboardAccessoryUpgrade);
}

// Feature disabled by default.
BASE_FEATURE(kMagicStack, "MagicStack", base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableFeedContainment,
             "EnableFeedContainment",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabResumption,
             "TabResumption",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kMagicStackMostVisitedModuleParam[] = "MagicStackMostVisitedModule";

const char kReducedSpaceParam[] = "ReducedNTPTopSpace";

const char kHideIrrelevantModulesParam[] = "HideIrrelevantModules";

const char kSetUpListCompactedTimeThresholdDays[] =
    "SetUpListCompactedTimeThresholdDays";

const char kHomeModuleMinimumPadding[] = "HomeModuleMinimumPadding";

// A parameter to indicate whether the native UI is enabled for the discover
// feed.
const char kDiscoverFeedIsNativeUIEnabled[] = "DiscoverFeedIsNativeUIEnabled";

const char kTabResumptionParameterName[] = "variant";
const char kTabResumptionMostRecentTabOnlyParam[] =
    "tab-resumption-recent-tab-only";
const char kTabResumptionAllTabsParam[] = "tab-resumption-all-tabs";
const char kTabResumptionAllTabsOneDayThresholdParam[] =
    "tab-resumption-all-tabs-one-day-threshold";

bool IsMagicStackEnabled() {
  return base::FeatureList::IsEnabled(kMagicStack);
}

bool IsFeedContainmentEnabled() {
  return base::FeatureList::IsEnabled(kEnableFeedContainment);
}

int HomeModuleMinimumPadding() {
  return base::GetFieldTrialParamByFeatureAsInt(kEnableFeedContainment,
                                                kHomeModuleMinimumPadding, 30);
}

bool IsTabResumptionEnabled() {
  return IsMagicStackEnabled() && base::FeatureList::IsEnabled(kTabResumption);
}

bool IsTabResumptionEnabledForMostRecentTabOnly() {
  CHECK(IsTabResumptionEnabled());
  std::string feature_param = base::GetFieldTrialParamValueByFeature(
      kTabResumption, kTabResumptionParameterName);
  return feature_param == kTabResumptionMostRecentTabOnlyParam;
}

const base::TimeDelta TabResumptionForXDevicesTimeThreshold() {
  CHECK(!IsTabResumptionEnabledForMostRecentTabOnly());

  std::string feature_param = base::GetFieldTrialParamValueByFeature(
      kTabResumption, kTabResumptionParameterName);
  if (feature_param == kTabResumptionAllTabsOneDayThresholdParam) {
    return base::Days(1);
  }
  return base::Hours(12);
}

bool ShouldPutMostVisitedSitesInMagicStack() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kMagicStack, kMagicStackMostVisitedModuleParam, false);
}

double ReducedNTPTopMarginSpaceForMagicStack() {
  return base::GetFieldTrialParamByFeatureAsDouble(kMagicStack,
                                                   kReducedSpaceParam, 0);
}

bool ShouldHideIrrelevantModules() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kMagicStack, kHideIrrelevantModulesParam, false);
}

int TimeUntilShowingCompactedSetUpList() {
  return base::GetFieldTrialParamByFeatureAsInt(
      kMagicStack, kSetUpListCompactedTimeThresholdDays, 3);
}

bool IsExternalActionSchemeHandlingEnabled() {
  return base::FeatureList::IsEnabled(kIOSExternalActionURLs);
}
