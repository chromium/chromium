// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/public/features/features.h"

#import "base/containers/contains.h"
#import "base/metrics/field_trial_params.h"
#import "components/country_codes/country_codes.h"
#import "components/segmentation_platform/embedder/home_modules/tips_ephemeral_module_constants.h"
#import "components/segmentation_platform/public/features.h"
#import "components/version_info/channel.h"
#import "ios/chrome/app/background_mode_buildflags.h"
#import "ios/chrome/browser/safety_check_notifications/utils/constants.h"
#import "ios/chrome/common/channel_info.h"
#import "ui/base/device_form_factor.h"

namespace {

// Whether feed background refresh is enabled. This only checks if the feature
// is enabled, not if the capability was enabled at startup.
bool IsFeedBackgroundRefreshEnabledOnly() {
  return base::FeatureList::IsEnabled(kEnableFeedBackgroundRefresh);
}

}  // namespace

BASE_FEATURE(kSegmentedDefaultBrowserPromo,
             "SegmentedDefaultBrowserPromo",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsSegmentedDefaultBrowserPromoEnabled() {
  return base::FeatureList::IsEnabled(kSegmentedDefaultBrowserPromo);
}

BASE_FEATURE(kIOSKeyboardAccessoryUpgrade,
             "IOSKeyboardAccessoryUpgrade",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kIOSKeyboardAccessoryUpgradeShortManualFillMenu,
             "IOSKeyboardAccessoryUpgradeShortManualFillMenu",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTestFeature, "TestFeature", base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSafetyCheckMagicStack,
             "SafetyCheckMagicStack",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSafetyCheckModuleHiddenIfNoIssuesKillswitch,
             "SafetyCheckModuleHiddenIfNoIssuesKillswitch",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSafetyCheckNotifications,
             "SafetyCheckNotifications",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kOmahaServiceRefactor,
             "OmahaServiceRefactor",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kTipsLensShopExperimentType[] = "TipsLensShopExperimentType";

const char kSafetyCheckNotificationsExperimentType[] =
    "SafetyCheckNotificationsExperimentType";

const char kSafetyCheckNotificationsImpressionTrigger[] =
    "SafetyCheckNotificationsImpressionTrigger";

const char kSafetyCheckNotificationsImpressionLimit[] =
    "SafetyCheckNotificationsImpressionLimit";

const char kSafetyCheckAllowPasswordsNotifications[] =
    "SafetyCheckAllowPasswordsNotifications";

const char kSafetyCheckAllowSafeBrowsingNotifications[] =
    "SafetyCheckAllowSafeBrowsingNotifications";

const char kSafetyCheckAllowUpdateChromeNotifications[] =
    "SafetyCheckAllowUpdateChromeNotifications";

const char kSafetyCheckMagicStackAutorunHoursThreshold[] =
    "SafetyCheckMagicStackAutorunHoursThreshold";

const char kSafetyCheckNotificationsProvisionalEnabled[] =
    "SafetyCheckNotificationsProvisionalEnabled";

const char kSafetyCheckNotificationsUserInactiveThreshold[] =
    "SafetyCheckNotificationsUserInactiveThreshold";

// This helper should return true by default, as this parameter primarily serves
// as a killswitch.
bool AreSafetyCheckPasswordsNotificationsAllowed() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kSafetyCheckNotifications, kSafetyCheckAllowPasswordsNotifications,
      /*default_value=*/true);
}

// This helper should return true by default, as this parameter primarily serves
// as a killswitch.
bool AreSafetyCheckSafeBrowsingNotificationsAllowed() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kSafetyCheckNotifications, kSafetyCheckAllowSafeBrowsingNotifications,
      /*default_value=*/true);
}

// This helper should return true by default, as this parameter primarily serves
// as a killswitch.
bool AreSafetyCheckUpdateChromeNotificationsAllowed() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kSafetyCheckNotifications, kSafetyCheckAllowUpdateChromeNotifications,
      /*default_value=*/true);
}

bool ProvisionalSafetyCheckNotificationsEnabled() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kSafetyCheckNotifications, kSafetyCheckNotificationsProvisionalEnabled,
      /*default_value=*/
      true);
}

const base::TimeDelta InactiveThresholdForSafetyCheckNotifications() {
  return base::GetFieldTrialParamByFeatureAsTimeDelta(
      kSafetyCheckNotifications, kSafetyCheckNotificationsUserInactiveThreshold,
      /*default_value=*/
      kSafetyCheckNotificationDefaultDelay);
}

// How many hours between each autorun of the Safety Check in the Magic Stack.
const base::TimeDelta TimeDelayForSafetyCheckAutorun() {
  int delay = base::GetFieldTrialParamByFeatureAsInt(
      kSafetyCheckMagicStack, kSafetyCheckMagicStackAutorunHoursThreshold,
      /*default_value=*/720);
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

extern const char kModernTabStripCloserNTB[] = "modern-tab-strip-closer-ntb";
extern const char kModernTabStripDarkerBackground[] =
    "modern-tab-strip-darker-background";
extern const char kModernTabStripNTBNoBackground[] =
    "modern-tab-strip-ntb-no-background";
extern const char kModernTabStripBlackBackground[] =
    "modern-tab-strip-black-background";
extern const char kModernTabStripBiggerNTB[] = "modern-tab-strip-bigger-ntb";

extern const char kModernTabStripDarkerBackgroundV3[] =
    "modern-tab-strip-darker-background-v3";
extern const char kModernTabStripCloseButtonsVisible[] =
    "modern-tab-strip-close-buttons-visible";
extern const char kModernTabStripInactiveTabsHighContrast[] =
    "modern-tab-strip-inactive-tabs-high-contrast";
extern const char kModernTabStripHighContrastNTB[] =
    "modern-tab-strip-high-contrast-ntb";

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

BASE_FEATURE(kIOSDockingPromoForEligibleUsersOnly,
             "IOSDockingPromoForEligibleUsersOnly",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kIOSDockingPromoFixedTriggerLogicKillswitch,
             "IOSDockingPromoFixedTriggerLogicKillswitch",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kIOSDockingPromoPreventDeregistrationKillswitch,
             "IOSDockingPromoPreventDeregistrationKillswitch",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kNonModalDefaultBrowserPromoCooldownRefactor,
             "NonModalDefaultBrowserPromoCooldownRefactor",
             base::FEATURE_ENABLED_BY_DEFAULT);

constexpr base::FeatureParam<int>
    kNonModalDefaultBrowserPromoCooldownRefactorParam{
        &kNonModalDefaultBrowserPromoCooldownRefactor,
        /*name=*/"cooldown-days", /*default_value=*/14};

BASE_FEATURE(kIOSEditMenuHideSearchWeb,
             "IOSEditMenuHideSearchWeb",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableColorLensAndVoiceIconsInHomeScreenWidget,
             "kEnableColorLensAndVoiceIconsInHomeScreenWidget",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableLensInOmniboxCopiedImage,
             "EnableLensInOmniboxCopiedImage",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableLensOverlay,
             "EnableLensOverlay",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Update to the correct milestone after launch.
const base::NotFatalUntil kLensOverlayNotFatalUntil = base::NotFatalUntil::M200;

BASE_FEATURE(kLensOverlayForceShowOnboardingScreen,
             "EnableLensOverlayForceShowOnboardingScreen",
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
        /*name=*/"large-entrypoint-displayed-seconds", /*default_value=*/10};

int LargeContextualPanelEntrypointDisplayedInSeconds() {
  return kLargeContextualPanelEntrypointDisplayedInSeconds.Get();
}

constexpr base::FeatureParam<bool> kContextualPanelEntrypointHighlightDuringIPH{
    &kContextualPanel,
    /*name=*/"entrypoint-highlight-iph", /*default_value=*/true};

bool ShouldHighlightContextualPanelEntrypointDuringIPH() {
  return kContextualPanelEntrypointHighlightDuringIPH.Get();
}

constexpr base::FeatureParam<bool> kContextualPanelEntrypointRichIPH{
    &kContextualPanel,
    /*name=*/"entrypoint-rich-iph", /*default_value=*/true};

bool ShouldShowRichContextualPanelEntrypointIPH() {
  return kContextualPanelEntrypointRichIPH.Get();
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

const char kBottomOmniboxDefaultSettingParam[] =
    "BottomOmniboxDefaultSettingParam";
const char kBottomOmniboxDefaultSettingParamTop[] = "Top";
const char kBottomOmniboxDefaultSettingParamBottom[] = "Bottom";
const char kBottomOmniboxDefaultSettingParamSafariSwitcher[] =
    "BottomSafariSwitcher";
BASE_FEATURE(kBottomOmniboxDefaultSetting,
             "BottomOmniboxDefaultSetting",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kOnlyAccessClipboardAsync,
             "OnlyAccessClipboardAsync",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kThemeColorInTopToolbar,
             "ThemeColorInTopToolbar",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsSafetyCheckMagicStackEnabled() {
  return base::FeatureList::IsEnabled(kSafetyCheckMagicStack);
}

bool ShouldHideSafetyCheckModuleIfNoIssues() {
  return base::FeatureList::IsEnabled(
      kSafetyCheckModuleHiddenIfNoIssuesKillswitch);
}

bool IsSafetyCheckNotificationsEnabled() {
  return base::FeatureList::IsEnabled(kSafetyCheckNotifications);
}

bool IsOmahaServiceRefactorEnabled() {
  return base::FeatureList::IsEnabled(kOmahaServiceRefactor);
}

SafetyCheckNotificationsExperimentalArm
SafetyCheckNotificationsExperimentTypeEnabled() {
  return static_cast<SafetyCheckNotificationsExperimentalArm>(
      base::GetFieldTrialParamByFeatureAsInt(
          kSafetyCheckNotifications, kSafetyCheckNotificationsExperimentType,
          /*default_value=*/
          (int)SafetyCheckNotificationsExperimentalArm::kVerbose));
}

SafetyCheckNotificationsImpressionTrigger
SafetyCheckNotificationsImpressionTriggerEnabled() {
  return static_cast<SafetyCheckNotificationsImpressionTrigger>(
      base::GetFieldTrialParamByFeatureAsInt(
          kSafetyCheckNotifications, kSafetyCheckNotificationsImpressionTrigger,
          /*default_value=*/
          (int)SafetyCheckNotificationsImpressionTrigger::kAlways));
}

int SafetyCheckNotificationsImpressionLimit() {
  return base::GetFieldTrialParamByFeatureAsInt(
      kSafetyCheckNotifications, kSafetyCheckNotificationsImpressionLimit,
      /*default_value=*/
      3);
}

bool IsTipsMagicStackEnabled() {
  return IsSegmentationTipsManagerEnabled();
}

TipsLensShopExperimentType TipsLensShopExperimentTypeEnabled() {
  return static_cast<
      TipsLensShopExperimentType>(base::GetFieldTrialParamByFeatureAsInt(
      segmentation_platform::features::kSegmentationPlatformTipsEphemeralCard,
      kTipsLensShopExperimentType,
      /*default_value=*/
      (int)TipsLensShopExperimentType::kWithoutProductImage));
}

BASE_FEATURE(kIOSChooseFromDrive,
             "IOSChooseFromDrive",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kIOSSaveToDrive,
             "IOSSaveToDrive",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kIOSSaveToPhotos,
             "IOSSaveToPhotos",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kIOSDownloadNoUIUpdateInBackground,
             "IOSDownloadNoUIUpdateInBackground",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

const char kContentPushNotificationsExperimentType[] =
    "ContentPushNotificationsExperimentType";

BASE_FEATURE(kContentPushNotifications,
             "ContentPushNotifications",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kContentNotificationExperiment,
             "ContentNotificationExperiment",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsContentNotificationExperimentEnabled() {
  return base::FeatureList::IsEnabled(kContentNotificationExperiment);
}

BASE_FEATURE(kContentNotificationProvisionalIgnoreConditions,
             "ContentNotificationProvisionalIgnoreConditions",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsContentNotificationProvisionalIgnoreConditions() {
  return base::FeatureList::IsEnabled(
      kContentNotificationProvisionalIgnoreConditions);
}

BASE_FEATURE(kContentNotificationDeliveredNAU,
             "ContentNotificationDeliveredNAU",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kDeliveredNAUMaxPerSession[] = "DeliveredNAUMaxPerSession";

BASE_FEATURE(kIOSLargeFakebox,
             "IOSLargeFakebox",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFullscreenImprovement,
             "FullscreenImprovement",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabGroupsIPad,
             "TabGroupsIPad",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsTabGroupInGridEnabled() {
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    return base::FeatureList::IsEnabled(kTabGroupsIPad) &&
           base::FeatureList::IsEnabled(kModernTabStrip);
  }
  return true;
}

BASE_FEATURE(kTabGroupSync, "TabGroupSync", base::FEATURE_DISABLED_BY_DEFAULT);

bool IsTabGroupSyncEnabled() {
  return IsTabGroupInGridEnabled() &&
         base::FeatureList::IsEnabled(kTabGroupSync);
}

BASE_FEATURE(kTabGroupIndicator,
             "TabGroupIndicator",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsTabGroupIndicatorEnabled() {
  return IsTabGroupInGridEnabled() &&
         base::FeatureList::IsEnabled(kTabGroupIndicator);
}

BASE_FEATURE(kNewSyncOptInIllustration,
             "NewSyncOptInIllustration",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsNewSyncOptInIllustration() {
  return base::FeatureList::IsEnabled(kNewSyncOptInIllustration);
}

BASE_FEATURE(kDisableLensCamera,
             "DisableLensCamera",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kOmniboxColorIcons,
             "OmniboxColorIcons",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kClearDeviceDataOnSignOutForManagedUsers,
             "ClearDeviceDataOnSignOutForManagedUsers",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDownloadedPDFOpening,
             "DownloadedPDFOpening",
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

bool IsDockingPromoForEligibleUsersOnlyEnabled() {
  return base::FeatureList::IsEnabled(kIOSDockingPromoForEligibleUsersOnly);
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
          NotificationsExperimentTypeProvisional);
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

bool IsKeyboardAccessoryUpgradeWithShortManualFillMenuEnabled() {
  return IsKeyboardAccessoryUpgradeEnabled() &&
         base::FeatureList::IsEnabled(
             kIOSKeyboardAccessoryUpgradeShortManualFillMenu);
}

// Feature disabled by default.
BASE_FEATURE(kMagicStack, "MagicStack", base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTabResumption, "TabResumption", base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTabResumption2,
             "TabResumption2",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabResumption2Reason,
             "TabResumption2Reason",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kMagicStackMostVisitedModuleParam[] = "MagicStackMostVisitedModule";

const char kReducedSpaceParam[] = "ReducedNTPTopSpace";

const char kHideIrrelevantModulesParam[] = "HideIrrelevantModules";

const char kSetUpListCompactedTimeThresholdDays[] =
    "SetUpListCompactedTimeThresholdDays";

// A parameter to indicate whether the native UI is enabled for the discover
// feed.
const char kDiscoverFeedIsNativeUIEnabled[] = "DiscoverFeedIsNativeUIEnabled";

const char kTabResumptionParameterName[] = "variant";
const char kTabResumptionMostRecentTabOnlyParam[] =
    "tab-resumption-recent-tab-only";
const char kTabResumptionAllTabsParam[] = "tab-resumption-all-tabs";

const char kTabResumptionThresholdParameterName[] =
    "tab-resumption-sync-threshold";

bool IsTabResumptionEnabled() {
  return base::FeatureList::IsEnabled(kTabResumption);
}

bool IsTabResumptionEnabledForMostRecentTabOnly() {
  CHECK(IsTabResumptionEnabled());
  std::string feature_param = base::GetFieldTrialParamValueByFeature(
      kTabResumption, kTabResumptionParameterName);
  return feature_param == kTabResumptionMostRecentTabOnlyParam;
}

bool IsTabResumption2_0Enabled() {
  if (!IsTabResumptionEnabled()) {
    return false;
  }
  return base::FeatureList::IsEnabled(kTabResumption2);
}

bool IsTabResumption2ReasonEnabled() {
  if (!IsTabResumption2_0Enabled()) {
    return false;
  }
  return base::FeatureList::IsEnabled(kTabResumption2Reason);
}

const base::TimeDelta TabResumptionForXDevicesTimeThreshold() {
  CHECK(!IsTabResumptionEnabledForMostRecentTabOnly());

  // Default to 12 hours.
  int threshold = base::GetFieldTrialParamByFeatureAsInt(
      kTabResumption, kTabResumptionThresholdParameterName,
      /*default_value*/ 12 * 3600);
  return base::Seconds(threshold);
}

BASE_FEATURE(kTabResumption1_5,
             "TabResumption1_5",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsTabResumption1_5Enabled() {
  return IsTabResumptionEnabled() &&
         base::FeatureList::IsEnabled(kTabResumption1_5);
}

const char kTR15SeeMoreButtonParam[] = "tr15-see-more-button";

bool IsTabResumption1_5SeeMoreEnabled() {
  return IsTabResumption1_5Enabled() &&
         base::GetFieldTrialParamByFeatureAsBool(kTabResumption1_5,
                                                 kTR15SeeMoreButtonParam, true);
}

BASE_FEATURE(kTabResumptionImages,
             "TabResumptionImages",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kTabResumptionImagesTypes[] = "tr-images-type";
const char kTabResumptionImagesTypesSalient[] = "salient";
const char kTabResumptionImagesTypesThumbnails[] = "thumbnails";

bool IsTabResumptionImagesSalientEnabled() {
  if (!IsTabResumption1_5Enabled() ||
      !base::FeatureList::IsEnabled(kTabResumptionImages)) {
    return false;
  }
  std::string image_type = base::GetFieldTrialParamByFeatureAsString(
      kTabResumptionImages, kTabResumptionImagesTypes, "");

  return image_type == kTabResumptionImagesTypesSalient || image_type == "";
}

bool IsTabResumptionImagesThumbnailsEnabled() {
  if (!IsTabResumption1_5Enabled() ||
      !base::FeatureList::IsEnabled(kTabResumptionImages)) {
    return false;
  }
  std::string image_type = base::GetFieldTrialParamByFeatureAsString(
      kTabResumptionImages, kTabResumptionImagesTypes, "");

  return image_type == kTabResumptionImagesTypesThumbnails || image_type == "";
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

const char kIOSTipsNotificationsUnknownTriggerTimeParam[] =
    "unknown_trigger_time";
const char kIOSTipsNotificationsActiveSeekerTriggerTimeParam[] =
    "active_seeker_trigger_time";
const char kIOSTipsNotificationsLessEngagedTriggerTimeParam[] =
    "less_engaged_trigger_time";
const char kIOSTipsNotificationsEnabledParam[] = "enabled";
const char kIOSTipsNotificationsOrderParam[] = "tips_notifications_order";
const char kIOSTipsNotificationsDismissLimitParam[] =
    "tips_notifications_dismiss_limit";

bool IsIOSTipsNotificationsEnabled() {
  return base::FeatureList::IsEnabled(kIOSTipsNotifications);
}

BASE_FEATURE(kDisableFullscreenScrolling,
             "DisableFullscreenScrolling",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsPinnedTabsEnabled() {
  return ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_TABLET;
}

BASE_FEATURE(kSegmentationPlatformIosModuleRankerCaching,
             "SegmentationPlatformIosModuleRankerCaching",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsSegmentationTipsManagerEnabled() {
  return base::FeatureList::IsEnabled(
      segmentation_platform::features::kSegmentationPlatformTipsEphemeralCard);
}

BASE_FEATURE(kDefaultBrowserPromoIPadExperimentalString,
             "DefaultBrowserPromoIPadExperimentalString",
             base::FEATURE_DISABLED_BY_DEFAULT);

BOOL UseIPadTailoredStringForDefaultBrowserPromo() {
  return ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET &&
         base::FeatureList::IsEnabled(
             kDefaultBrowserPromoIPadExperimentalString);
}

BASE_FEATURE(kSpotlightNeverRetainIndex,
             "SpotlightNeverRetainIndex",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kIOSSaveToPhotosImprovements,
             "SaveToPhotosImprovements",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kSaveToPhotosContextMenuImprovementParam[] =
    "save-to-photos-context-menu-improvement";
const char kSaveToPhotosTitleImprovementParam[] =
    "save-to-photos-title-improvement";
const char kSaveToPhotosAccountDefaultChoiceImprovementParam[] =
    "save-to-photos-account-default-choice-improvement";

bool IsSaveToPhotosActionImprovementEnabled() {
  return base::FeatureList::IsEnabled(kIOSSaveToPhotosImprovements) &&
         base::GetFieldTrialParamByFeatureAsBool(
             kIOSSaveToPhotosImprovements,
             kSaveToPhotosContextMenuImprovementParam, true);
}

bool IsSaveToPhotosTitleImprovementEnabled() {
  return base::FeatureList::IsEnabled(kIOSSaveToPhotosImprovements) &&
         base::GetFieldTrialParamByFeatureAsBool(
             kIOSSaveToPhotosImprovements, kSaveToPhotosTitleImprovementParam,
             true);
}

bool IsSaveToPhotosAccountPickerImprovementEnabled() {
  return base::FeatureList::IsEnabled(kIOSSaveToPhotosImprovements) &&
         base::GetFieldTrialParamByFeatureAsBool(
             kIOSSaveToPhotosImprovements,
             kSaveToPhotosAccountDefaultChoiceImprovementParam, true);
}

BASE_FEATURE(kHomeCustomization,
             "HomeCustomization",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsHomeCustomizationEnabled() {
  return base::FeatureList::IsEnabled(kHomeCustomization);
}

BASE_FEATURE(kEnableAppBackgroundRefresh,
             "EnableAppBackgroundRefresh",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsAppBackgroundRefreshEnabled() {
  version_info::Channel channel = ::GetChannel();
  if (channel == version_info::Channel::BETA ||
      channel == version_info::Channel::STABLE) {
    return false;
  }

  return base::FeatureList::IsEnabled(kEnableAppBackgroundRefresh);
}

BASE_FEATURE(kHomeMemoryImprovements,
             "HomeMemoryImprovements",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsHomeMemoryImprovementsEnabled() {
  return base::FeatureList::IsEnabled(kHomeMemoryImprovements);
}

BASE_FEATURE(kRichBubbleWithoutImage,
             "RichBubbleWithoutImage",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsRichBubbleWithoutImageEnabled() {
  return base::FeatureList::IsEnabled(kRichBubbleWithoutImage);
}

BASE_FEATURE(kIdentityConfirmationSnackbar,
             "IdentityConfirmationSnackbar",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Feature parameters for kIdentityConfirmationSnackbar.
constexpr base::FeatureParam<base::TimeDelta>
    kIdentityConfirmationMinDisplayInterval1{
        &kIdentityConfirmationSnackbar,
        /*name=*/"IdentityConfirmationMinDisplayInterval1",
        /*default_value=*/base::Days(1)};
constexpr base::FeatureParam<base::TimeDelta>
    kIdentityConfirmationMinDisplayInterval2{
        &kIdentityConfirmationSnackbar,
        /*name=*/"IdentityConfirmationMinDisplayInterval2",
        /*default_value=*/base::Days(7)};
constexpr base::FeatureParam<base::TimeDelta>
    kIdentityConfirmationMinDisplayInterval3{
        &kIdentityConfirmationSnackbar,
        /*name=*/"IdentityConfirmationMinDisplayInterval3",
        /*default_value=*/base::Days(30)};

BASE_FEATURE(kEnableTraitCollectionRegistration,
             "EnableTraitCollectionRegistration",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kBlueDotOnToolsMenuButton,
             "BlueDotOnToolsMenuButton",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsBlueDotOnToolsMenuButtoneEnabled() {
  return base::FeatureList::IsEnabled(kBlueDotOnToolsMenuButton);
}

BASE_FEATURE(kSeparateProfilesForManagedAccounts,
             "SeparateProfilesForManagedAccounts",
             base::FEATURE_DISABLED_BY_DEFAULT);
