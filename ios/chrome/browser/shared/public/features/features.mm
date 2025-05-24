// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/public/features/features.h"

#import <string>
#import <vector>

#import "base/containers/contains.h"
#import "base/metrics/field_trial_params.h"
#import "components/country_codes/country_codes.h"
#import "components/data_sharing/public/features.h"
#import "components/segmentation_platform/public/features.h"
#import "components/sync/base/features.h"
#import "components/version_info/channel.h"
#import "ios/chrome/app/background_mode_buildflags.h"
#import "ios/chrome/browser/ntp/shared/metrics/feed_metrics_constants.h"
#import "ios/chrome/browser/safety_check_notifications/utils/constants.h"
#import "ios/chrome/common/channel_info.h"
#import "ui/base/device_form_factor.h"

BASE_FEATURE(kSegmentedDefaultBrowserPromo,
             "SegmentedDefaultBrowserPromo",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kSegmentedDefaultBrowserExperimentType[] =
    "SegmentedDefaultBrowserExperimentType";

bool IsSegmentedDefaultBrowserPromoEnabled() {
  return base::FeatureList::IsEnabled(kSegmentedDefaultBrowserPromo);
}

SegmentedDefaultBrowserExperimentType
SegmentedDefaultBrowserExperimentTypeEnabled() {
  return static_cast<SegmentedDefaultBrowserExperimentType>(
      base::GetFieldTrialParamByFeatureAsInt(
          kSegmentedDefaultBrowserPromo, kSegmentedDefaultBrowserExperimentType,
          /*default_value=*/
          (int)SegmentedDefaultBrowserExperimentType::kStaticPromo));
}

BASE_FEATURE(kIOSKeyboardAccessoryUpgradeForIPad,
             "IOSKeyboardAccessoryUpgradeForIPad",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kIOSKeyboardAccessoryUpgradeShortManualFillMenu,
             "IOSKeyboardAccessoryUpgradeShortManualFillMenu",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTestFeature, "TestFeature", base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSafetyCheckMagicStack,
             "SafetyCheckMagicStack",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSafetyCheckAutorunByManagerKillswitch,
             "SafetyCheckAutorunByManagerKillswitch",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

const char kTipsSafeBrowsingExperimentType[] = "TipsSafeBrowsingExperimentType";

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

const char kSafetyCheckNotificationsSuppressDelayIfPresent[] =
    "SafetyCheckNotificationsSuppressDelayIfPresent";

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

const base::TimeDelta SuppressDelayForSafetyCheckNotificationsIfPresent() {
  return base::GetFieldTrialParamByFeatureAsTimeDelta(
      kSafetyCheckNotifications,
      kSafetyCheckNotificationsSuppressDelayIfPresent,
      /*default_value=*/kSafetyCheckNotificationSuppressDelayIfPresent);
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

BASE_FEATURE(kEnableLensInOmniboxCopiedImage,
             "EnableLensInOmniboxCopiedImage",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableLensOverlay,
             "EnableLensOverlay",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableLensViewFinderUnifiedExperience,
             "EnableLensViewFinderUnifiedExperience",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableLensContextMenuUnifiedExperience,
             "EnableLensContextMenuUnifiedExperience",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Update to the correct milestone after launch.
// Also update in components/omnibox/browser/autocomplete_result.cc.
const base::NotFatalUntil kLensOverlayNotFatalUntil = base::NotFatalUntil::M200;

BASE_FEATURE(kLensLoadAIMInLensResultPage,
             "LensLoadAIMInLensResultPage",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensOverlayDisablePriceInsights,
             "LensOverlayDisablePriceInsights",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensOverlayPriceInsightsCounterfactual,
             "LensOverlayPriceInsightsCounterfactual",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensOverlayEnableIPadCompatibility,
             "EnableLensOverlayForceIPadSupport",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensOverlayEnableLandscapeCompatibility,
             "EnableLensOverlayLandscapeSupport",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensOverlayEnableLVFEscapeHatch,
             "LensOverlayEnableLVFEscapeHatch",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kLensOverlayEnableLocationBarEntrypoint,
             "LensOverlayEnableLocationBarEntrypoint",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kLensOverlayEnableLocationBarEntrypointOnSRP,
             "LensOverlayEnableLocationBarEntrypointOnSRP",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensOverlayEnableSameTabNavigation,
             "EnableLensOverlaySameTabNavigation",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kLensOverlayForceShowOnboardingScreen,
             "EnableLensOverlayForceShowOnboardingScreen",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kLensOverlayOnboardingParam[] = "kLensOverlayOnboardingParam";
const char kLensOverlayOnboardingParamSpeedbumpMenu[] =
    "kLensOverlayOnboardingParamSpeedbumpMenu";
const char kLensOverlayOnboardingParamUpdatedStrings[] =
    "kLensOverlayOnboardingParamUpdatedStrings";
const char kLensOverlayOnboardingParamUpdatedStringsAndVisuals[] =
    "kLensOverlayOnboardingParamUpdatedStringsAndVisuals";

BASE_FEATURE(kLensOverlayAlternativeOnboarding,
             "LensOverlayAlternativeOnboarding",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensOverlayNavigationHistory,
             "LensOverlayNavigationHistory",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableTraitCollectionWorkAround,
             "EnableTraitCollectionWorkAround",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kRemoveExcessNTPs,
             "RemoveExcessNTPs",
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
             base::FEATURE_ENABLED_BY_DEFAULT);

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
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kThemeColorInTopToolbar,
             "ThemeColorInTopToolbar",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsSafetyCheckAutorunByManagerEnabled() {
  return base::FeatureList::IsEnabled(kSafetyCheckAutorunByManagerKillswitch);
}

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
          (int)SafetyCheckNotificationsExperimentalArm::kSuccinct));
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
      (int)TipsLensShopExperimentType::kWithProductImage));
}

TipsSafeBrowsingExperimentType TipsSafeBrowsingExperimentTypeEnabled() {
  return static_cast<
      TipsSafeBrowsingExperimentType>(base::GetFieldTrialParamByFeatureAsInt(
      segmentation_platform::features::kSegmentationPlatformTipsEphemeralCard,
      kTipsSafeBrowsingExperimentType,
      /*default_value=*/
      (int)TipsSafeBrowsingExperimentType::kShowEnhancedSafeBrowsingPromo));
}

BASE_FEATURE(kIOSChooseFromDrive,
             "IOSChooseFromDrive",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kIOSChooseFromDriveSimulatedClick,
             "IOSChooseFromDriveSimulatedClick",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kIOSDownloadNoUIUpdateInBackground,
             "IOSDownloadNoUIUpdateInBackground",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kIOSManageAccountStorage,
             "IOSManageAccountStorage",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDeprecateFeedHeader,
             "DeprecateFeedHeader",
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

BASE_FEATURE(kFullscreenImprovement,
             "FullscreenImprovement",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsTabGroupInGridEnabled() {
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    if (@available(iOS 17, *)) {
      return true;
    }
    return false;
  }
  return true;
}

BASE_FEATURE(kTabGroupSync, "TabGroupSync", base::FEATURE_DISABLED_BY_DEFAULT);

bool IsTabGroupSyncEnabled() {
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    return IsTabGroupInGridEnabled() &&
           base::FeatureList::IsEnabled(kTabGroupSync);
  }
  return true;
}

BASE_FEATURE(kTabGroupIndicator,
             "TabGroupIndicator",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsTabGroupIndicatorEnabled() {
  if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_TABLET) {
    return true;
  }
  return IsTabGroupInGridEnabled() &&
         base::FeatureList::IsEnabled(kTabGroupIndicator);
}

bool IsTabGroupSendFeedbackAvailable() {
  return base::GetFieldTrialParamByFeatureAsBool(
      data_sharing::features::kDataSharingFeature, "show_send_feedback",
      /*default=*/false);
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

BASE_FEATURE(kDownloadAutoDeletionFeatureEnabled,
             "DownloadAutoDeletionFeatureEnabled",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsDownloadAutoDeletionFeatureEnabled() {
  return base::FeatureList::IsEnabled(kDownloadAutoDeletionFeatureEnabled);
}

BASE_FEATURE(kDownloadedPDFOpening,
             "DownloadedPDFOpening",
             base::FEATURE_ENABLED_BY_DEFAULT);

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
  return false;
}

bool IsDiscoverFeedServiceCreatedEarly() {
  return base::FeatureList::IsEnabled(kCreateDiscoverFeedServiceEarly);
}

bool IsFeedBackgroundRefreshEnabled() {
  return base::FeatureList::IsEnabled(kEnableFeedBackgroundRefresh);
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
  return false;
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

bool IsKeyboardAccessoryUpgradeEnabled() {
  return (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_TABLET) ||
         base::FeatureList::IsEnabled(kIOSKeyboardAccessoryUpgradeForIPad);
}

bool IsKeyboardAccessoryUpgradeWithShortManualFillMenuEnabled() {
  return (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_TABLET) &&
         base::FeatureList::IsEnabled(
             kIOSKeyboardAccessoryUpgradeShortManualFillMenu);
}

// Feature disabled by default.
BASE_FEATURE(kMagicStack, "MagicStack", base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTabResumption, "TabResumption", base::FEATURE_ENABLED_BY_DEFAULT);

// A parameter to indicate whether the native UI is enabled for the discover
// feed.
const char kDiscoverFeedIsNativeUIEnabled[] = "DiscoverFeedIsNativeUIEnabled";

const char kTabResumptionThresholdParameterName[] =
    "tab-resumption-sync-threshold";

bool IsTabResumptionEnabled() {
  return base::FeatureList::IsEnabled(kTabResumption);
}

const base::TimeDelta TabResumptionForXDevicesTimeThreshold() {
  // Default to 12 hours.
  int threshold = base::GetFieldTrialParamByFeatureAsInt(
      kTabResumption, kTabResumptionThresholdParameterName,
      /*default_value*/ 12 * 3600);
  return base::Seconds(threshold);
}

BASE_FEATURE(kTabResumptionImages,
             "TabResumptionImages",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kTabResumptionImagesTypes[] = "tr-images-type";
const char kTabResumptionImagesTypesSalient[] = "salient";
const char kTabResumptionImagesTypesThumbnails[] = "thumbnails";

bool IsTabResumptionImagesSalientEnabled() {
  if (!base::FeatureList::IsEnabled(kTabResumptionImages)) {
    return false;
  }
  std::string image_type = base::GetFieldTrialParamByFeatureAsString(
      kTabResumptionImages, kTabResumptionImagesTypes, "");

  return image_type == kTabResumptionImagesTypesSalient || image_type == "";
}

bool IsTabResumptionImagesThumbnailsEnabled() {
  if (!base::FeatureList::IsEnabled(kTabResumptionImages)) {
    return false;
  }
  std::string image_type = base::GetFieldTrialParamByFeatureAsString(
      kTabResumptionImages, kTabResumptionImagesTypes, "");

  return image_type == kTabResumptionImagesTypesThumbnails || image_type == "";
}

BASE_FEATURE(kInactiveNavigationAfterAppLaunchKillSwitch,
             "kInactiveNavigationAfterAppLaunchKillSwitch",
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

BASE_FEATURE(kSpotlightNeverRetainIndex,
             "SpotlightNeverRetainIndex",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kIOSSaveToPhotosImprovements,
             "SaveToPhotosImprovements",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

bool ShouldDeprecateFeedHeader() {
  return base::FeatureList::IsEnabled(kDeprecateFeedHeader);
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
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsBlueDotOnToolsMenuButtoneEnabled() {
  return base::FeatureList::IsEnabled(kBlueDotOnToolsMenuButton);
}

BASE_FEATURE(kSeparateProfilesForManagedAccounts,
             "SeparateProfilesForManagedAccounts",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSeparateProfilesForManagedAccountsKillSwitch,
             "SeparateProfilesForManagedAccountsKillSwitch",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kOmahaResyncTimerOnForeground,
             "OmahaResyncTimerOnForeground",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kChromeStartupParametersAsync,
             "ChromeStartupParametersAsync",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kYoutubeIncognito,
             "YoutubeIncognito",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kYoutubeIncognitoTargetApps[] = "youtube-incognito-target-apps";

const char kYoutubeIncognitoTargetAppsAllowlisted[] = "allow-listed";
const char kYoutubeIncognitoTargetAppsFirstParty[] = "first-party";
const char kYoutubeIncognitoTargetAppsAll[] = "all";

const char kYoutubeIncognitoErrorHandlingWithoutIncognitoInterstitialParam[] =
    "youtube-incognito-error-handling-without-incognito-interstitial";

bool IsYoutubeIncognitoTargetAllowListedEnabled() {
  std::string target_apps = base::GetFieldTrialParamByFeatureAsString(
      kYoutubeIncognito, kYoutubeIncognitoTargetApps, "");
  return target_apps == kYoutubeIncognitoTargetAppsAllowlisted ||
         target_apps == "";
}

bool IsYoutubeIncognitoTargetFirstPartyEnabled() {
  std::string target_apps = base::GetFieldTrialParamByFeatureAsString(
      kYoutubeIncognito, kYoutubeIncognitoTargetApps, "");
  return target_apps == kYoutubeIncognitoTargetAppsFirstParty;
}

bool IsYoutubeIncognitoTargetAllEnabled() {
  std::string target_apps = base::GetFieldTrialParamByFeatureAsString(
      kYoutubeIncognito, kYoutubeIncognitoTargetApps, "");
  return target_apps == kYoutubeIncognitoTargetAppsAll;
}

bool IsYoutubeIncognitoErrorHandlingWithoutIncognitoInterstitialEnabled() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kYoutubeIncognito,
      kYoutubeIncognitoErrorHandlingWithoutIncognitoInterstitialParam, false);
}

BASE_FEATURE(kIOSReactivationNotifications,
             "IOSReactivationNotifications",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kIOSReactivationNotificationsTriggerTimeParam[] =
    "reactivation_trigger_time";
const char kIOSReactivationNotificationsOrderParam[] = "reactivation_order";

bool IsIOSReactivationNotificationsEnabled() {
  return base::FeatureList::IsEnabled(kIOSReactivationNotifications);
}

BASE_FEATURE(kIOSExpandedTips,
             "IOSExpandedTips",
             base::FEATURE_DISABLED_BY_DEFAULT);
const char kIOSExpandedTipsOrderParam[] = "expanded_tips_order";

bool IsIOSExpandedTipsEnabled() {
  return base::FeatureList::IsEnabled(kIOSExpandedTips);
}

BASE_FEATURE(kProvisionalNotificationAlert,
             "ProvisionalNotificationAlert",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsProvisionalNotificationAlertEnabled() {
  return base::FeatureList::IsEnabled(kProvisionalNotificationAlert);
}

BASE_FEATURE(kDefaultBrowserBannerPromo,
             "DefaultBrowserBannerPromo",
             base::FEATURE_DISABLED_BY_DEFAULT);

constexpr base::FeatureParam<int> kDefaultBrowserBannerPromoImpressionLimit{
    &kDefaultBrowserBannerPromo, "DefaultBrowserBannerPromoImpressionLimit",
    10};

bool IsDefaultBrowserBannerPromoEnabled() {
  return base::FeatureList::IsEnabled(kDefaultBrowserBannerPromo);
}

constexpr base::FeatureParam<std::string>
    kFRESignInSecondaryActionLabelUpdateParam{
        &kFRESignInSecondaryActionLabelUpdate,
        "FRESignInSecondaryActionLabelUpdateParam", "StaySignedOut"};

const std::string_view kFRESignInSecondaryActionLabelUpdateParamStaySignedOut =
    "StaySignedOut";

BASE_FEATURE(kFRESignInSecondaryActionLabelUpdate,
             "FRESignInSecondaryActionLabelUpdate",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool FRESignInSecondaryActionLabelUpdate() {
  return base::FeatureList::IsEnabled(kFRESignInSecondaryActionLabelUpdate);
}

BASE_FEATURE(kIOSPasskeysM2, "IOSPasskeysM2", base::FEATURE_ENABLED_BY_DEFAULT);

bool IOSPasskeysM2Enabled() {
  return base::FeatureList::IsEnabled(kIOSPasskeysM2);
}

BASE_FEATURE(kIOSPushNotificationMultiProfile,
             "IOSPushNotificationMultiProfile",
             base::FEATURE_ENABLED_BY_DEFAULT);

const char kFullscreenTransitionSlower[] = "SlowFullscreenTransitionSpeed";
const char kFullscreenTransitionDefaultSpeed[] =
    "MediumFullscreenTransitionSpeed";
const char kFullscreenTransitionFaster[] = "FastFullscreenTransitionSpeed";
const char kFullscreenTransitionSpeedParam[] = "FullscreenTransitionSpeed";
const char kMediumFullscreenTransitionOffsetParam[] =
    "MediumFullscreenTransitionOffset";

bool IsFullscreenTransitionSet() {
  return base::FeatureList::IsEnabled(kFullscreenTransition);
}

FullscreenTransitionSpeed FullscreenTransitionSpeedParam() {
  return static_cast<FullscreenTransitionSpeed>(
      base::GetFieldTrialParamByFeatureAsInt(
          kFullscreenTransition, kFullscreenTransitionSpeedParam, 1));
}

bool IsFullscreenTransitionOffsetSet() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kFullscreenTransition, kMediumFullscreenTransitionOffsetParam, false);
}

BASE_FEATURE(kFullscreenTransition,
             "FullscreenTransition",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kRefactorToolbarsSize,
             "RefactorToolbarsSize",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsRefactorToolbarsSize() {
  return base::FeatureList::IsEnabled(kRefactorToolbarsSize);
}

BASE_FEATURE(kNewShareExtension,
             "NewShareExtension",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kIPHAblation, "IPHAblation", base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensOverlayDisableIPHPanGesture,
             "LensOverlayDisableIPHPanGesture",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsIPHAblationEnabled() {
  return base::FeatureList::IsEnabled(kIPHAblation);
}

BASE_FEATURE(kIPHGestureRecognitionAblation,
             "IPHGestureRecognitionAblation",
             base::FEATURE_ENABLED_BY_DEFAULT);

const char kIPHGestureRecognitionInsideTapAblation[] =
    "IPHGestureRecognitionInsideTapAblation";
const char kIPHGestureRecognitionOutsideTapAblation[] =
    "IPHGestureRecognitionOutsideTapAblation";
const char kIPHGestureRecognitionPanAblation[] =
    "IPHGestureRecognitionPanAblation";
const char kIPHGestureRecognitionSwipeAblation[] =
    "IPHGestureRecognitionSwipeAblation";
const char kCancelTouchesInViewForIPH[] = "CancelTouchesInViewForIPH";
const char kIPHGestureRecognitionImprovement[] =
    "IPHGestureRecognitionImprovement";

bool IsIPHGestureRecognitionInsideTapAblationEnabled() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kIPHGestureRecognitionAblation, kIPHGestureRecognitionInsideTapAblation,
      false);
}

bool IsIPHGestureRecognitionOutsideTapAblationEnabled() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kIPHGestureRecognitionAblation, kIPHGestureRecognitionOutsideTapAblation,
      false);
}

bool IsIPHGestureRecognitionPanAblationEnabled() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kIPHGestureRecognitionAblation, kIPHGestureRecognitionPanAblation, false);
}

bool IsIPHGestureRecognitionSwipeAblationEnabled() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kIPHGestureRecognitionAblation, kIPHGestureRecognitionSwipeAblation,
      false);
}

bool ShouldCancelTouchesInViewForIPH() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kIPHGestureRecognitionAblation, kCancelTouchesInViewForIPH, false);
}

bool IsIPHGestureRecognitionImprovementEnabled() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kIPHGestureRecognitionAblation, kIPHGestureRecognitionImprovement, false);
}

BASE_FEATURE(kNonModalSignInPromo,
             "NonModalSignInPromo",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsNonModalSignInPromoEnabled() {
  return base::FeatureList::IsEnabled(kNonModalSignInPromo);
}

BASE_FEATURE(kIOSOneTapMiniMapRestrictions,
             "IOSOneTapMiniMapRestrictions",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kIOSOneTapMiniMapRestrictionCrossValidateParamName[] =
    "ios-one-tap-minimap-cross-validate";
constexpr base::FeatureParam<bool>
    kIOSOneTapMiniMapRestrictionCrossValidateParam{
        &kIOSOneTapMiniMapRestrictions,
        /*name=*/kIOSOneTapMiniMapRestrictionCrossValidateParamName,
        /*default_value=*/false};
const char kIOSOneTapMiniMapRestrictionThreshholdParamName[] =
    "ios-one-tap-minimap-threshhold";
constexpr base::FeatureParam<double>
    kIOSOneTapMiniMapRestrictionThreshholdParam{
        &kIOSOneTapMiniMapRestrictions,
        /*name=*/kIOSOneTapMiniMapRestrictionThreshholdParamName,
        /*default_value=*/0};
const char kIOSOneTapMiniMapRestrictionMinCharsParamName[] =
    "ios-one-tap-minimap-min-chars";
constexpr base::FeatureParam<int> kIOSOneTapMiniMapRestrictionMinCharsParam{
    &kIOSOneTapMiniMapRestrictions,
    /*name=*/kIOSOneTapMiniMapRestrictionMinCharsParamName,
    /*default_value=*/0};
const char kIOSOneTapMiniMapRestrictionMaxSectionsParamName[] =
    "ios-one-tap-minimap-max-section";
constexpr base::FeatureParam<int> kIOSOneTapMiniMapRestrictionMaxSectionsParam{
    &kIOSOneTapMiniMapRestrictions,
    /*name=*/kIOSOneTapMiniMapRestrictionMaxSectionsParamName,
    /*default_value=*/0};
const char kIOSOneTapMiniMapRestrictionLongestWordMinCharsParamName[] =
    "ios-one-tap-minimap-longest-word-min-chars";
constexpr base::FeatureParam<int>
    kIOSOneTapMiniMapRestrictionLongestWordMinCharsParam{
        &kIOSOneTapMiniMapRestrictions,
        /*name=*/kIOSOneTapMiniMapRestrictionLongestWordMinCharsParamName,
        /*default_value=*/0};
const char kIOSOneTapMiniMapRestrictionMinAlphanumProportionParamName[] =
    "ios-one-tap-minimap-min-alphanum-proportion";
constexpr base::FeatureParam<double>
    kIOSOneTapMiniMapRestrictionMinAlphanumProportionParam{
        &kIOSOneTapMiniMapRestrictions,
        /*name=*/kIOSOneTapMiniMapRestrictionMinAlphanumProportionParamName,
        /*default_value=*/0};

BASE_FEATURE(kIOSOneTapMiniMapRemoveSectionsBreaks,
             "IOSOneTapMiniMapRemoveSectionsBreaks",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsNotificationCollisionManagementEnabled() {
  return base::FeatureList::IsEnabled(kNotificationCollisionManagement);
}

BASE_FEATURE(kNotificationCollisionManagement,
             "NotificationCollisionManagement",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kIOSProvidesAppNotificationSettings,
             "IOSProvidesAppNotificationSettings",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSignInButtonNoAvatar,
             "SignInButtonNoAvatar",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsSignInButtonNoAvatarEnabled() {
  return base::FeatureList::IsEnabled(kSignInButtonNoAvatar);
}

BASE_FEATURE(kNTPBackgroundCustomization,
             "NTPBackgroundCustomization",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsNTPBackgroundCustomizationEnabled() {
  return base::FeatureList::IsEnabled(kNTPBackgroundCustomization);
}

BASE_FEATURE(kRunDefaultStatusCheck,
             "RunDefaultStatusCheck",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsRunDefaultStatusCheckEnabled() {
  return base::FeatureList::IsEnabled(kRunDefaultStatusCheck);
}

BASE_FEATURE(kContainedTabGroup,
             "ContainedTabGroup",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsContainedTabGroupEnabled() {
  return base::FeatureList::IsEnabled(kContainedTabGroup);
}

BASE_FEATURE(kColorfulTabGroup,
             "ColorfulTabGroup",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsColorfulTabGroupEnabled() {
  return base::FeatureList::IsEnabled(kColorfulTabGroup);
}

BASE_FEATURE(kBestOfAppFRE, "BestOfAppFRE", base::FEATURE_DISABLED_BY_DEFAULT);

bool IsBestOfAppFREEnabled() {
  return base::FeatureList::IsEnabled(kBestOfAppFRE);
}

bool IsBestOfAppGuidedTourEnabled() {
  return base::GetFieldTrialParamValueByFeature(kBestOfAppFRE, "variant") ==
         "4";
}

bool IsBestOfAppLensInteractivePromoEnabled() {
  return (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_PHONE) &&
         IsBestOfAppFREEnabled() &&
         (base::GetFieldTrialParamValueByFeature(kBestOfAppFRE, "variant") ==
          "1");
}

BASE_FEATURE(kFeedbackIncludeGWSVariations,
             "FeedbackIncludeGWSVariations",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsFeedbackIncludeGWSVariationsEnabled() {
  return base::FeatureList::IsEnabled(kFeedbackIncludeGWSVariations);
}
