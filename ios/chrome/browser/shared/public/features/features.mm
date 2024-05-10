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

}  // namespace

BASE_FEATURE(kIOSKeyboardAccessoryUpgrade,
             "kIOSKeyboardAccessoryUpgrade",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTestFeature, "TestFeature", base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSafetyCheckMagicStack,
             "SafetyCheckMagicStack",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kSafetyCheckMagicStackAutorunHoursThreshold[] =
    "SafetyCheckMagicStackAutorunHoursThreshold";

// How many hours between each autorun of the Safety Check in the Magic Stack.
const base::TimeDelta TimeDelayForSafetyCheckAutorun() {
  int delay = base::GetFieldTrialParamByFeatureAsInt(
      kSafetyCheckMagicStack, kSafetyCheckMagicStackAutorunHoursThreshold,
      /*default_value=*/24);
  return base::Hours(delay);
}

BASE_FEATURE(kSharedHighlightingIOS,
             "SharedHighlightingIOS",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kShareInWebContextMenuIOS,
             "ShareInWebContextMenuIOS",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kModernTabStrip,
             "ModernTabStrip",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kModernTabStripParameterName[] = "modern-tab-strip-new-tab-button";
const char kModernTabStripNTBDynamicParam[] = "dynamic";
const char kModernTabStripNTBStaticParam[] = "static";

BASE_FEATURE(kDefaultBrowserIntentsShowSettings,
             "DefaultBrowserIntentsShowSettings",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kIOSBrowserEditMenuMetrics,
             "IOSBrowserEditMenuMetrics",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kIOSDockingPromoExperimentType[] = "IOSDockingPromoExperimentType";
const char kIOSDockingPromoNewUserInactiveThresholdHours[] =
    "IOSDockingPromoNewUserInactiveThresholdHours";
const char kIOSDockingPromoOldUserInactiveThresholdHours[] =
    "IOSDockingPromoOldUserInactiveThresholdHours";
const char kIOSDockingPromoNewUserInactiveThreshold[] =
    "IOSDockingPromoNewUserInactiveThreshold";
const char kIOSDockingPromoOldUserInactiveThreshold[] =
    "IOSDockingPromoOldUserInactiveThreshold";

BASE_FEATURE(kIOSDockingPromo,
             "IOSDockingPromo",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kIOSDockingPromoFixedTriggerLogicKillswitch,
             "IOSDockingPromoFixedTriggerLogicKillswitch",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kIOSDockingPromoPreventDeregistrationKillswitch,
             "IOSDockingPromoPreventDeregistrationKillswitch",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kNonModalDefaultBrowserPromoCooldownRefactor,
             "NonModalDefaultBrowserPromoCooldownRefactor",
             base::FEATURE_DISABLED_BY_DEFAULT);

constexpr base::FeatureParam<int>
    kNonModalDefaultBrowserPromoCooldownRefactorParam{
        &kNonModalDefaultBrowserPromoCooldownRefactor,
        /*name=*/"cooldown-days", /*default_value=*/14};

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

BASE_FEATURE(kEnableColorLensAndVoiceIconsInHomeScreenWidget,
             "kEnableColorLensAndVoiceIconsInHomeScreenWidget",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableLensInOmniboxCopiedImage,
             "EnableLensInOmniboxCopiedImage",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableTraitCollectionWorkAround,
             "EnableTraitCollectionWorkAround",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kRemoveExcessNTPs,
             "RemoveExcessNTPs",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableShortenedPasswordAutoFillInstruction,
             "EnableShortenedPasswordAutoFillInstruction",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableStartupImprovements,
             "EnableStartupImprovements",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTCRexKillSwitch,
             "kTCRexKillSwitch",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabGridNewTransitions,
             "TabGridNewTransitions",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsNewTabGridTransitionsEnabled() {
  return base::FeatureList::IsEnabled(kTabGridNewTransitions);
}

BASE_FEATURE(kContextualPanelForceShowEntrypoint,
             "ContextualPanelForceShowEntrypoint",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsContextualPanelForceShowEntrypointEnabled() {
  return base::FeatureList::IsEnabled(kContextualPanelForceShowEntrypoint);
}

BASE_FEATURE(kContextualPanel,
             "ContextualPanel",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsContextualPanelEnabled() {
  return base::FeatureList::IsEnabled(kContextualPanel);
}

constexpr base::FeatureParam<int> kLargeContextualPanelEntrypointDelayInSeconds{
    &kContextualPanel,
    /*name=*/"large-entrypoint-delay-seconds", /*default_value=*/2};

int LargeContextualPanelEntrypointDelayInSeconds() {
  return kLargeContextualPanelEntrypointDelayInSeconds.Get();
}

constexpr base::FeatureParam<int>
    kLargeContextualPanelEntrypointDisplayedInSeconds{
        &kContextualPanel,
        /*name=*/"large-entrypoint-displayed-seconds", /*default_value=*/5};

int LargeContextualPanelEntrypointDisplayedInSeconds() {
  return kLargeContextualPanelEntrypointDisplayedInSeconds.Get();
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
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kNewNTPOmniboxLayout,
             "kNewNTPOmniboxLayout",
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

BASE_FEATURE(kBottomOmniboxPromoFRE,
             "BottomOmniboxPromoFRE",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kBottomOmniboxPromoAppLaunch,
             "BottomOmniboxPromoAppLaunch",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kBottomOmniboxPromoParam[] = "BottomOmniboxPromoParam";
const char kBottomOmniboxPromoParamForced[] = "Forced";

bool IsBottomOmniboxPromoFlagEnabled(BottomOmniboxPromoType type) {
  if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_PHONE) {
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

BASE_FEATURE(kBottomOmniboxPromoRegionFilter,
             "BottomOmniboxPromoRegionFilter",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kOnlyAccessClipboardAsync,
             "OnlyAccessClipboardAsync",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kDefaultBrowserVideoInSettings,
             "DefaultBrowserVideoInSettings",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kThemeColorInTopToolbar,
             "ThemeColorInTopToolbar",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabGridAlwaysBounce,
             "TabGridAlwaysBounce",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabGridCompositionalLayout,
             "TabGridCompositionalLayout",
             base::FEATURE_ENABLED_BY_DEFAULT);

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
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kHistoryOptInForRestoreShortyAndReSignin,
             "HistoryOptInForRestoreShortyAndReSignin",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableBatchUploadFromBookmarksManager,
             "EnableBatchUploadFromBookmarksManager",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableReviewAccountSettingsPromo,
             "EnableReviewAccountSettingsPromo",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kLinkAccountSettingsToPrivacyFooter,
             "LinkAccountSettingsToPrivacyFooter",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableWebChannels,
             "EnableWebChannels",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableFeedBackgroundRefresh,
             "EnableFeedBackgroundRefresh",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCreateDiscoverFeedServiceEarly,
             "CreateDiscoverFeedServiceEarly",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableFeedAblation,
             "EnableFeedAblation",
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

BASE_FEATURE(kFullscreenImprovement,
             "FullscreenImprovement",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabGroupsInGrid,
             "TabGroupsInGrid",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabGroupsIPad,
             "TabGroupsIPad",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsTabGroupInGridEnabled() {
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    return base::FeatureList::IsEnabled(kTabGroupsIPad);
  }
  return base::FeatureList::IsEnabled(kTabGroupsInGrid);
}

BASE_FEATURE(kTabGroupSync, "TabGroupSync", base::FEATURE_DISABLED_BY_DEFAULT);

bool IsTabGroupSyncEnabled() {
  return IsTabGroupInGridEnabled() &&
         base::FeatureList::IsEnabled(kTabGroupSync);
}

BASE_FEATURE(kDisableLensCamera,
             "DisableLensCamera",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kOmniboxColorIcons,
             "OmniboxColorIcons",
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

bool IsDockingPromoEnabled() {
  return base::FeatureList::IsEnabled(kIOSDockingPromo);
}

DockingPromoDisplayTriggerArm DockingPromoExperimentTypeEnabled() {
  return static_cast<DockingPromoDisplayTriggerArm>(
      base::GetFieldTrialParamByFeatureAsInt(
          kIOSDockingPromo, kIOSDockingPromoExperimentType,
          /*default_value=*/(int)DockingPromoDisplayTriggerArm::kAfterFRE));
}

const base::TimeDelta InactiveThresholdForNewUsersUntilDockingPromoShown() {
  return base::GetFieldTrialParamByFeatureAsTimeDelta(
      kIOSDockingPromo, kIOSDockingPromoNewUserInactiveThreshold,
      /*default_value=*/
      base::Hours(HoursInactiveForNewUsersUntilShowingDockingPromo()));
}

const base::TimeDelta InactiveThresholdForOldUsersUntilDockingPromoShown() {
  return base::GetFieldTrialParamByFeatureAsTimeDelta(
      kIOSDockingPromo, kIOSDockingPromoOldUserInactiveThreshold,
      /*default_value=*/
      base::Hours(HoursInactiveForOldUsersUntilShowingDockingPromo()));
}

int HoursInactiveForNewUsersUntilShowingDockingPromo() {
  return base::GetFieldTrialParamByFeatureAsInt(
      kIOSDockingPromo, kIOSDockingPromoNewUserInactiveThresholdHours,
      /*default_value=*/24);
}

int HoursInactiveForOldUsersUntilShowingDockingPromo() {
  return base::GetFieldTrialParamByFeatureAsInt(
      kIOSDockingPromo, kIOSDockingPromoOldUserInactiveThresholdHours,
      /*default_value=*/72);
}

bool IsWebChannelsEnabled() {
  std::string launched_countries[6] = {"AU", "CA", "GB", "NZ", "US", "ZA"};
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
  [[NSUserDefaults standardUserDefaults]
      setBool:IsFeedBackgroundRefreshEnabledOnly()
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

bool IsFeedAblationEnabled() {
  return base::FeatureList::IsEnabled(kEnableFeedAblation);
}

bool IsFollowUIUpdateEnabled() {
  std::string launched_countries[1] = {"US"};
  if (base::Contains(launched_countries,
                     country_codes::GetCurrentCountryCode())) {
    return true;
  }
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

bool IsContentPushNotificationsProvisionalEnabled() {
  return (ContentNotificationsExperimentTypeEnabled() ==
              NotificationsExperimentTypeProvisional ||
          ContentNotificationsExperimentTypeEnabled() ==
              NotificationsExperimentTypeProvisionalBypass);
}

// TODO(b/322348322): Remove provisional notifications bypass conditions testing
// flag param.
bool IsContentPushNotificationsProvisionalBypass() {
  return (ContentNotificationsExperimentTypeEnabled() ==
          NotificationsExperimentTypeProvisionalBypass);
}

bool IsContentPushNotificationsPromoRegistrationOnly() {
  return (ContentNotificationsExperimentTypeEnabled() ==
          NotificationsExperimentTypePromoRegistrationOnly);
}

bool IsContentPushNotificationsProvisionalRegistrationOnly() {
  return (ContentNotificationsExperimentTypeEnabled() ==
          NotificationsExperimentTypeProvisionalRegistrationOnly);
}

bool IsContentPushNotificationsSetUpListRegistrationOnly() {
  return (ContentNotificationsExperimentTypeEnabled() ==
          NotificationsExperimentTypeSetUpListsRegistrationOnly);
}

bool IsIOSLargeFakeboxEnabled() {
  return base::FeatureList::IsEnabled(kIOSLargeFakebox);
}

bool IsKeyboardAccessoryUpgradeEnabled() {
  return base::FeatureList::IsEnabled(kIOSKeyboardAccessoryUpgrade) &&
         ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_TABLET;
}

// Feature disabled by default.
BASE_FEATURE(kMagicStack, "MagicStack", base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableFeedContainment,
             "EnableFeedContainment",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabResumption, "TabResumption", base::FEATURE_ENABLED_BY_DEFAULT);

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

bool IsFeedContainmentEnabled() {
  return base::FeatureList::IsEnabled(kEnableFeedContainment);
}

CGFloat HomeModuleMinimumPadding() {
  return base::GetFieldTrialParamByFeatureAsDouble(
      kEnableFeedContainment, kHomeModuleMinimumPadding, 8.0);
}

bool IsTabResumptionEnabled() {
  return base::FeatureList::IsEnabled(kTabResumption);
}

bool IsTabResumptionEnabledForMostRecentTabOnly() {
  CHECK(IsTabResumptionEnabled());
  std::string feature_param = base::GetFieldTrialParamValueByFeature(
      kTabResumption, kTabResumptionParameterName);
  return feature_param != kTabResumptionAllTabsParam;
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
                                                   kReducedSpaceParam, 20);
}

bool ShouldHideIrrelevantModules() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kMagicStack, kHideIrrelevantModulesParam, false);
}

int TimeUntilShowingCompactedSetUpList() {
  return base::GetFieldTrialParamByFeatureAsInt(
      kMagicStack, kSetUpListCompactedTimeThresholdDays, 0);
}

BASE_FEATURE(kInactiveNavigationAfterAppLaunchKillSwitch,
             "kInactiveNavigationAfterAppLaunchKillSwitch",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kIOSTipsNotifications,
             "IOSTipsNotifications",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kIOSTipsNotificationsTriggerTimeParam[] = "trigger_time";
const char kIOSTipsNotificationsEnabledParam[] = "enabled";

bool IsIOSTipsNotificationsEnabled() {
  return base::FeatureList::IsEnabled(kIOSTipsNotifications);
}

BASE_FEATURE(kIOSMagicStackCollectionView,
             "IOSMagicStackCollectionView",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsIOSMagicStackCollectionViewEnabled() {
  return base::FeatureList::IsEnabled(kIOSMagicStackCollectionView);
}

BASE_FEATURE(kDisableFullscreenScrolling,
             "DisableFullscreenScrolling",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsPinnedTabsEnabled() {
  return ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_TABLET;
}

BASE_FEATURE(kPrefetchSystemCapabilitiesOnFirstRun,
             "PrefetchSystemCapabilitiesOnFirstRun",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsPrefetchingSystemCapabilitiesOnFirstRun() {
  return base::FeatureList::IsEnabled(kPrefetchSystemCapabilitiesOnFirstRun);
}

BASE_FEATURE(kSegmentationPlatformIosModuleRankerCaching,
             "SegmentationPlatformIosModuleRankerCaching",
             base::FEATURE_DISABLED_BY_DEFAULT);
