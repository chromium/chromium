// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/public/features/features.h"

#import <algorithm>
#import <string>
#import <vector>

#import "base/metrics/field_trial_params.h"
#import "base/strings/string_split.h"
#import "components/country_codes/country_codes.h"
#import "components/segmentation_platform/public/features.h"
#import "components/sync/base/features.h"
#import "components/sync_preferences/features.h"
#import "components/version_info/channel.h"
#import "crypto/features.h"
#import "ios/chrome/app/background_mode_buildflags.h"
#import "ios/chrome/browser/ntp/shared/metrics/feed_metrics_constants.h"
#import "ios/chrome/common/channel_info.h"
#import "ui/base/device_form_factor.h"

BASE_FEATURE(kTestFeature, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSafetyCheckAutorunByManagerKillswitch,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSafetyCheckModuleHiddenIfNoIssuesKillswitch,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kOmahaServiceRefactor, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kHideToolbarsInOverflowMenu, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSharedHighlightingIOS, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kIOSBrowserEditMenuMetrics, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kIOSCustomFileUploadMenu, base::FEATURE_DISABLED_BY_DEFAULT);

const char kIOSDockingPromoV2VariationParam[] =
    "IOSDockingPromoV2VariationParam";
const char kIOSDockingPromoV2VariationHeader1[] =
    "IOSDockingPromoV2VariationHeader1";
const char kIOSDockingPromoV2VariationHeader2[] =
    "IOSDockingPromoV2VariationHeader2";
const char kIOSDockingPromoV2VariationHeader3[] =
    "IOSDockingPromoV2VariationHeader3";

BASE_FEATURE(kIOSDockingPromoV2, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsDockingPromoV2Enabled() {
  return base::FeatureList::IsEnabled(kIOSDockingPromoV2);
}

const char kIOSDockingPromoExperimentType[] = "IOSDockingPromoExperimentType";
const char kIOSDockingPromoNewUserInactiveThresholdHours[] =
    "IOSDockingPromoNewUserInactiveThresholdHours";
const char kIOSDockingPromoOldUserInactiveThresholdHours[] =
    "IOSDockingPromoOldUserInactiveThresholdHours";
const char kIOSDockingPromoNewUserInactiveThreshold[] =
    "IOSDockingPromoNewUserInactiveThreshold";
const char kIOSDockingPromoOldUserInactiveThreshold[] =
    "IOSDockingPromoOldUserInactiveThreshold";

BASE_FEATURE(kIOSDockingPromo, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kIOSDockingPromoForEligibleUsersOnly,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kIOSDockingPromoFixedTriggerLogicKillswitch,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kIOSDockingPromoPreventDeregistrationKillswitch,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableLensInOmniboxCopiedImage,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensLoadAIMInLensResultPage, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensOverlayEnableLandscapeCompatibility,
             "EnableLensOverlayLandscapeSupport",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensOverlayForceShowOnboardingScreen,
             "EnableLensOverlayForceShowOnboardingScreen",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensOverlayNavigationHistory, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensOverlayCustomBottomSheet, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensSearchHeadersCheckEnabled, base::FEATURE_ENABLED_BY_DEFAULT);

// Variations of MIA NTP entrypoint.
const char kNTPMIAEntrypointParam[] = "kNTPMIAEntrypointParam";
const char kNTPMIAEntrypointParamOmniboxContainedSingleButton[] =
    "kNTPMIAEntrypointParamOmniboxContainedSingleButton";
const char kNTPMIAEntrypointParamOmniboxContainedInline[] =
    "kNTPMIAEntrypointParamOmniboxContainedInline";
const char kNTPMIAEntrypointParamOmniboxContainedEnlargedFakebox[] =
    "kNTPMIAEntrypointParamOmniboxContainedEnlargedFakebox";
const char kNTPMIAEntrypointParamEnlargedFakeboxNoIncognito[] =
    "kNTPMIAEntrypointParamEnlargedFakeboxNoIncognito";
const char kNTPMIAEntrypointParamAIMInQuickActions[] =
    "kNTPMIAEntrypointParamAIMInQuickActions";

// Feature flag to change the MIA entrypoint in NTP.
BASE_FEATURE(kNTPMIAEntrypoint,
             "kNTPMIAEntrypoint",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kNTPMIAEntrypointAllLocales,
             "kNTPMIAEntrypointAllLocales",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Used to gate the immersive SRP in the Composebox.
BASE_FEATURE(kComposeboxImmersiveSRP, base::FEATURE_DISABLED_BY_DEFAULT);

const char kComposeboxTabPickerVariationParam[] =
    "kComposeboxTabPickerVariationParam";
const char kComposeboxTabPickerVariationParamCachedAPC[] =
    "kComposeboxTabPickerVariationParamCachedAPC";
const char kComposeboxTabPickerVariationParamOnFlightAPC[] =
    "kComposeboxTabPickerVariationParamOnFlightAPC";

// Feature flag for the tab picker in the Composebox.
BASE_FEATURE(kComposeboxTabPickerVariation, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsComposeboxTabPickerCachedAPCEnabled() {
  std::string param = base::GetFieldTrialParamValueByFeature(
      kComposeboxTabPickerVariation, kComposeboxTabPickerVariationParam);
  return param == kComposeboxTabPickerVariationParamCachedAPC;
}

BASE_FEATURE(kOmniboxDRSPrototype, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableTraitCollectionWorkAround,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kRemoveExcessNTPs, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTCRexKillSwitch,
             "kTCRexKillSwitch",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabGridDragAndDrop, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsTabGridDragAndDropEnabled() {
  return base::FeatureList::IsEnabled(kTabGridDragAndDrop);
}

BASE_FEATURE(kTabGridNewTransitions, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsNewTabGridTransitionsEnabled() {
  if (IsChromeNextIaEnabled()) {
    return true;
  }
  return base::FeatureList::IsEnabled(kTabGridNewTransitions);
}

BASE_FEATURE(kTabGroupInOverflowMenu, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabGroupInTabIconContextMenu, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabRecallNewTabGroupButton, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabSwitcherOverflowMenu, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kContextualPanelForceShowEntrypoint,
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsContextualPanelForceShowEntrypointEnabled() {
  return base::FeatureList::IsEnabled(kContextualPanelForceShowEntrypoint);
}

BASE_FEATURE(kNonModalDefaultBrowserPromoImpressionLimit,
             base::FEATURE_ENABLED_BY_DEFAULT);

constexpr base::FeatureParam<int>
    kNonModalDefaultBrowserPromoImpressionLimitParam{
        &kNonModalDefaultBrowserPromoImpressionLimit,
        /*name=*/"impression-limit", /*default_value=*/3};

const char kBottomOmniboxEvolutionParam[] = "kBottomOmniboxEvolutionParam";
const char kBottomOmniboxEvolutionParamEditStateFollowSteadyState[] =
    "kBottomOmniboxEvolutionParamEditStateFollowSteadyState";
const char kBottomOmniboxEvolutionParamForceBottomOmniboxEditState[] =
    "kBottomOmniboxEvolutionParamForceBottomOmniboxEditState";

BASE_FEATURE(kBottomOmniboxEvolution, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsSafetyCheckAutorunByManagerEnabled() {
  return base::FeatureList::IsEnabled(kSafetyCheckAutorunByManagerKillswitch);
}

bool ShouldHideSafetyCheckModuleIfNoIssues() {
  return base::FeatureList::IsEnabled(
      kSafetyCheckModuleHiddenIfNoIssuesKillswitch);
}

bool IsOmahaServiceRefactorEnabled() {
  return base::FeatureList::IsEnabled(kOmahaServiceRefactor);
}

// TODO(crbug.com/473788390): Clean-up feature once file upload menu is ready.
BASE_FEATURE(kIOSChooseFromDrive, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kIOSChooseFromDriveSignedOut, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kIOSDownloadNoUIUpdateInBackground,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kIOSSaveToDriveClientFolder, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kIOSSaveToDriveSignedOut, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kIOSSaveToPhotosSignedOut, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableFeedBackgroundRefresh, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableFeedAblation, base::FEATURE_DISABLED_BY_DEFAULT);

const char kContentPushNotificationsExperimentType[] =
    "ContentPushNotificationsExperimentType";

BASE_FEATURE(kContentPushNotifications, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kContentNotificationProvisionalIgnoreConditions,
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsContentNotificationProvisionalIgnoreConditions() {
  return base::FeatureList::IsEnabled(
      kContentNotificationProvisionalIgnoreConditions);
}

BASE_FEATURE(kContentNotificationDeliveredNAU,
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kDeliveredNAUMaxPerSession[] = "DeliveredNAUMaxPerSession";

BASE_FEATURE(kNewSyncOptInIllustration, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsNewSyncOptInIllustration() {
  return base::FeatureList::IsEnabled(kNewSyncOptInIllustration);
}

BASE_FEATURE(kDisableLensCamera, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDownloadAutoDeletionClearFilesOnEveryStartup,
             base::FEATURE_DISABLED_BY_DEFAULT);

bool isDownloadAutoDeletionTestingFeatureEnabled() {
  return base::FeatureList::IsEnabled(
      kDownloadAutoDeletionClearFilesOnEveryStartup);
}

BASE_FEATURE(kDownloadAutoDeletionFeatureEnabled,
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsDownloadAutoDeletionFeatureEnabled() {
  return base::FeatureList::IsEnabled(kDownloadAutoDeletionFeatureEnabled);
}

const char kDownloadListUITypeParam[] = "DownloadListUIType";

bool IsDownloadListEnabled() {
  return base::FeatureList::IsEnabled(kDownloadList);
}

DownloadListUIType CurrentDownloadListUIType() {
  CHECK(IsDownloadListEnabled());
  return static_cast<DownloadListUIType>(base::GetFieldTrialParamByFeatureAsInt(
      kDownloadList, kDownloadListUITypeParam, /*default_value=*/
      (int)DownloadListUIType::kDefaultUI));
}

BASE_FEATURE(kDownloadList, base::FEATURE_DISABLED_BY_DEFAULT);

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

bool IsLiquidGlassEffectEnabled() {
  if (@available(iOS 26, *)) {
    return true;
  }

  return false;
}

BASE_FEATURE(kIOSEnhancedAutofill, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kIOSKeyboardAccessoryDefaultView,
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsIOSKeyboardAccessoryDefaultViewEnabled() {
  return base::FeatureList::IsEnabled(kIOSKeyboardAccessoryDefaultView);
}

BASE_FEATURE(kIOSKeyboardAccessoryTwoBubble, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsIOSKeyboardAccessoryTwoBubbleEnabled() {
  return base::FeatureList::IsEnabled(kIOSKeyboardAccessoryTwoBubble);
}

// Feature parameter for kIOSKeyboardAccessoryTwoBubble.
BASE_FEATURE_PARAM(bool,
                   kIOSKeyboardAccessoryTwoBubbleKeyboardIconParam,
                   &kIOSKeyboardAccessoryTwoBubble,
                   kIOSKeyboardAccessoryTwoBubbleKeyboardIconParamName,
                   false);

BASE_FEATURE(kInactiveNavigationAfterAppLaunchKillSwitch,
             "kInactiveNavigationAfterAppLaunchKillSwitch",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsPinnedTabsEnabled() {
  return ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_TABLET;
}

BASE_FEATURE(kSegmentationPlatformIosModuleRankerCaching,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableAppBackgroundRefresh, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsAppBackgroundRefreshEnabled() {
  return base::FeatureList::IsEnabled(kEnableAppBackgroundRefresh);
}

BASE_FEATURE(kEnableTraitCollectionRegistration,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSeparateProfilesForManagedAccounts,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Feature parameter for kSeparateProfilesForManagedAccountsForceMigration.
constexpr base::FeatureParam<base::TimeDelta> kMultiProfileMigrationGracePeriod{
    &kSeparateProfilesForManagedAccountsForceMigration,
    /*name=*/"MultiProfileMigrationGracePeriod",
    /*default_value=*/base::Days(90)};

BASE_FEATURE(kSeparateProfilesForManagedAccountsForceMigration,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kOmahaResyncTimerOnForeground, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kIOSReactivationNotifications, base::FEATURE_DISABLED_BY_DEFAULT);

const char kIOSReactivationNotificationsTriggerTimeParam[] =
    "reactivation_trigger_time";
const char kIOSReactivationNotificationsOrderParam[] = "reactivation_order";

bool IsIOSReactivationNotificationsEnabled() {
  return base::FeatureList::IsEnabled(kIOSReactivationNotifications);
}

const char kIOSExpandedSetupListVariationParam[] =
    "kIOSExpandedSetupListVariationParam";
const char kIOSExpandedSetupListVariationParamSafariImport[] =
    "kIOSExpandedSetupListVariationParamSafariImport";
const char kIOSExpandedSetupListVariationParamBackgroundCustomization[] =
    "kIOSExpandedSetupListVariationParamBackgroundCustomization";
extern const char kIOSExpandedSetupListVariationParamAllExceptCPE[] =
    "kIOSExpandedSetupListVariationParamAllExceptCPE";
const char kIOSExpandedSetupListVariationParamAll[] =
    "kIOSExpandedSetupListVariationParamAll";

BASE_FEATURE(kIOSExpandedSetupList, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsIOSExpandedSetupListEnabled() {
  return base::FeatureList::IsEnabled(kIOSExpandedSetupList);
}

BASE_FEATURE(kIOSExpandedTips,
             "kIOSExpandedTips",
             base::FEATURE_DISABLED_BY_DEFAULT);
const char kIOSExpandedTipsOrderParam[] = "expanded_tips_order";

bool IsIOSExpandedTipsEnabled() {
  return base::FeatureList::IsEnabled(kIOSExpandedTips);
}

BASE_FEATURE(kProvisionalNotificationAlert, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsProvisionalNotificationAlertEnabled() {
  return base::FeatureList::IsEnabled(kProvisionalNotificationAlert);
}

BASE_FEATURE(kIOSOneTimeDefaultBrowserNotification,
             base::FEATURE_DISABLED_BY_DEFAULT);

constexpr base::FeatureParam<std::string> kFRESignInHeaderTextUpdateParam{
    &kFRESignInHeaderTextUpdate,
    /*name=*/"FRESignInHeaderTextUpdateParam",
    /*default_value=*/""};

const std::string_view kFRESignInHeaderTextUpdateParamArm0 = "Arm0";
const std::string_view kFRESignInHeaderTextUpdateParamArm1 = "Arm1";

BASE_FEATURE(kFRESignInHeaderTextUpdate, base::FEATURE_DISABLED_BY_DEFAULT);

bool FRESignInHeaderTextUpdate() {
  return base::FeatureList::IsEnabled(kFRESignInHeaderTextUpdate);
}

constexpr base::FeatureParam<std::string>
    kFRESignInSecondaryActionLabelUpdateParam{
        &kFRESignInSecondaryActionLabelUpdate,
        "FRESignInSecondaryActionLabelUpdateParam", "StaySignedOut"};

const std::string_view kFRESignInSecondaryActionLabelUpdateParamStaySignedOut =
    "StaySignedOut";

BASE_FEATURE(kFRESignInSecondaryActionLabelUpdate,
             base::FEATURE_ENABLED_BY_DEFAULT);

bool FRESignInSecondaryActionLabelUpdate() {
  return base::FeatureList::IsEnabled(kFRESignInSecondaryActionLabelUpdate);
}

BASE_FEATURE(kConfirmationButtonSwapOrder, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsConfirmationButtonSwapOrderEnabled() {
  return base::FeatureList::IsEnabled(kConfirmationButtonSwapOrder);
}

BASE_FEATURE(kIOSPushNotificationMultiProfile,
             base::FEATURE_ENABLED_BY_DEFAULT);

const char kFullscreenTransitionSlower[] = "SlowFullscreenTransitionSpeed";
const char kFullscreenTransitionDefaultSpeed[] =
    "MediumFullscreenTransitionSpeed";
const char kFullscreenTransitionFaster[] = "FastFullscreenTransitionSpeed";
const char kFullscreenTransitionSpeedParam[] = "FullscreenTransitionSpeed";

bool IsFullscreenTransitionSpeedSet() {
  return base::FeatureList::IsEnabled(kFullscreenTransitionSpeed);
}

FullscreenTransitionSpeed FullscreenTransitionSpeedParam() {
  return static_cast<FullscreenTransitionSpeed>(
      base::GetFieldTrialParamByFeatureAsInt(
          kFullscreenTransitionSpeed, kFullscreenTransitionSpeedParam, 1));
}

BASE_FEATURE(kFullscreenTransitionSpeed, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kRefactorToolbarsSize, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsRefactorToolbarsSize() {
  return base::FeatureList::IsEnabled(kRefactorToolbarsSize);
}

BASE_FEATURE(kIPHAblation, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsIPHAblationEnabled() {
  return base::FeatureList::IsEnabled(kIPHAblation);
}

BASE_FEATURE(kIPHGestureRecognitionAblation, base::FEATURE_ENABLED_BY_DEFAULT);

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

BASE_FEATURE(kIOSOneTapMiniMapRestrictions, base::FEATURE_DISABLED_BY_DEFAULT);

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

bool IsNotificationCollisionManagementEnabled() {
  return base::FeatureList::IsEnabled(kNotificationCollisionManagement);
}

BASE_FEATURE(kNotificationCollisionManagement,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kIOSProvidesAppNotificationSettings,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kNTPBackgroundCustomization, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsNTPBackgroundCustomizationEnabled() {
  return base::FeatureList::IsEnabled(kNTPBackgroundCustomization);
}

constexpr base::FeatureParam<int> kMaxRecentlyUsedBackgrounds{
    &kNTPBackgroundCustomization,
    /*name=*/"max-recently-used-backgrounds", /*default_value=*/7};

int MaxRecentlyUsedBackgrounds() {
  return kMaxRecentlyUsedBackgrounds.Get();
}

BASE_FEATURE(kNTPBackgroundColorSlider, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsNTPBackgroundColorSliderEnabled() {
  return base::FeatureList::IsEnabled(kNTPBackgroundColorSlider);
}

BASE_FEATURE(kRunDefaultStatusCheck, base::FEATURE_ENABLED_BY_DEFAULT);

bool IsRunDefaultStatusCheckEnabled() {
  return base::FeatureList::IsEnabled(kRunDefaultStatusCheck);
}

BASE_FEATURE(kBestOfAppFRE, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsBestOfAppFREEnabled() {
  return base::FeatureList::IsEnabled(kBestOfAppFRE);
}

std::vector<std::string> GetBestOfAppFREActiveVariants() {
  std::string variants_string =
      base::GetFieldTrialParamValueByFeature(kBestOfAppFRE, "variant");
  return SplitString(variants_string, ",", base::TRIM_WHITESPACE,
                     base::SPLIT_WANT_NONEMPTY);
}

bool IsBestOfAppGuidedTourEnabled() {
  return std::ranges::contains(GetBestOfAppFREActiveVariants(), "4");
}

bool IsManualUploadForBestOfAppEnabled() {
  return base::GetFieldTrialParamByFeatureAsBool(kBestOfAppFRE,
                                                 "manual_upload_uma", false);
}

bool IsBestOfAppLensInteractivePromoEnabled() {
  return (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_PHONE) &&
         IsBestOfAppFREEnabled() &&
         std::ranges::contains(GetBestOfAppFREActiveVariants(), "1");
}

bool IsBestOfAppLensAnimatedPromoEnabled() {
  return IsBestOfAppFREEnabled() &&
         std::ranges::contains(GetBestOfAppFREActiveVariants(), "2");
}

bool IsDefaultBrowserPromoPropensityModelEnabled() {
  return base::FeatureList::IsEnabled(
      segmentation_platform::features::kDefaultBrowserPromoPropensityModel);
}

BASE_FEATURE(kIOSTrustedVaultNotification, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsIOSTrustedVaultNotificationEnabled() {
  return base::FeatureList::IsEnabled(kIOSTrustedVaultNotification);
}

BASE_FEATURE(kIOSDefaultBrowserOffCyclePromo,
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsDefaultBrowserOffCyclePromoEnabled() {
  if (@available(iOS 18.3, *)) {
    return base::FeatureList::IsEnabled(kIOSDefaultBrowserOffCyclePromo);
  }
  return false;
}

BASE_FEATURE(kIOSLogInstallAttribution, base::FEATURE_ENABLED_BY_DEFAULT);

bool IsInstallAttributionLoggingEnabled() {
  return base::FeatureList::IsEnabled(kIOSLogInstallAttribution);
}

BASE_FEATURE(kIOSLogAppPreviewInstallAttribution,
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsAppPreviewInstallAttributionLoggingEnabled() {
  return base::FeatureList::IsEnabled(kIOSLogAppPreviewInstallAttribution);
}

BASE_FEATURE(kIOSUseDefaultAppsDestinationForPromos,
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsDefaultAppsDestinationAvailable() {
  if (@available(iOS 18.3, *)) {
    return true;
  }
  return false;
}

bool IsUseDefaultAppsDestinationForPromosEnabled() {
  return base::FeatureList::IsEnabled(kIOSUseDefaultAppsDestinationForPromos);
}

BASE_FEATURE(kSynchronousEditMenuItems, base::FEATURE_ENABLED_BY_DEFAULT);

bool ShouldShowEditMenuItemsSynchronously() {
  if (@available(iOS 26, *)) {
    return base::FeatureList::IsEnabled(kSynchronousEditMenuItems);
  }
  return false;
}

BASE_FEATURE(kIOSTipsNotificationsAlternativeStrings,
             base::FEATURE_DISABLED_BY_DEFAULT);
bool IsTipsNotificationsAlternativeStringsEnabled() {
  return base::FeatureList::IsEnabled(kIOSTipsNotificationsAlternativeStrings);
}

const char kTipsNotificationsAlternativeStringVersion[] =
    "TipsNotificationsAlternativeStringVersion";

TipsNotificationsAlternativeStringVersion
GetTipsNotificationsAlternativeStringVersion() {
  return static_cast<TipsNotificationsAlternativeStringVersion>(
      base::GetFieldTrialParamByFeatureAsInt(
          kIOSTipsNotificationsAlternativeStrings,
          kTipsNotificationsAlternativeStringVersion,
          /*default_value=*/
          static_cast<int>(
              TipsNotificationsAlternativeStringVersion::kDefault)));
}

BASE_FEATURE(kIOSSyncedSetUp, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsSyncedSetUpEnabled() {
  return base::FeatureList::IsEnabled(
             sync_preferences::features::kEnableCrossDevicePrefTracker) &&
         base::FeatureList::IsEnabled(kIOSSyncedSetUp);
}

const char kSyncedSetUpImpressionLimit[] = "SyncedSetUpImpressionLimit";

int GetSyncedSetUpImpressionLimit() {
  return base::GetFieldTrialParamByFeatureAsInt(
      kIOSSyncedSetUp, kSyncedSetUpImpressionLimit, /*default_value=*/1);
}

BASE_FEATURE(kDisableKeyboardAccessory, base::FEATURE_DISABLED_BY_DEFAULT);

const char kDisableKeyboardAccessoryParam[] = "kDisableKeyboardAccessoryParam";
const char kDisableKeyboardAccessoryOnlySymbols[] =
    "kDisableKeyboardAccessoryOnlySymbols";
const char kDisableKeyboardAccessoryOnlyFeatures[] =
    "kDisableKeyboardAccessoryOnlyFeatures";
const char kDisableKeyboardAccessoryCompletely[] =
    "kDisableKeyboardAccessoryCompletely";

bool ShouldShowKeyboardAccessory() {
  if (!base::FeatureList::IsEnabled(kDisableKeyboardAccessory)) {
    return true;
  }
  std::string feature_param = base::GetFieldTrialParamValueByFeature(
      kDisableKeyboardAccessory, kDisableKeyboardAccessoryParam);
  return feature_param != kDisableKeyboardAccessoryCompletely;
}

bool ShouldShowKeyboardAccessorySymbols() {
  if (!base::FeatureList::IsEnabled(kDisableKeyboardAccessory)) {
    return true;
  }
  std::string feature_param = base::GetFieldTrialParamValueByFeature(
      kDisableKeyboardAccessory, kDisableKeyboardAccessoryParam);
  return feature_param == kDisableKeyboardAccessoryOnlySymbols;
}

bool ShouldShowKeyboardAccessoryFeatures() {
  if (!base::FeatureList::IsEnabled(kDisableKeyboardAccessory)) {
    return true;
  }
  std::string feature_param = base::GetFieldTrialParamValueByFeature(
      kDisableKeyboardAccessory, kDisableKeyboardAccessoryParam);
  return feature_param == kDisableKeyboardAccessoryOnlyFeatures;
}

BASE_FEATURE(kLocationBarBadgeMigration, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsLocationBarBadgeMigrationEnabled() {
  if (IsChromeNextIaEnabled()) {
    return true;
  }
  return base::FeatureList::IsEnabled(kLocationBarBadgeMigration);
}

BASE_FEATURE(kComposeboxIOS, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsComposeboxIOSEnabled() {
  if (!base::FeatureList::IsEnabled(kComposeboxIOS)) {
    return false;
  }
  if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_PHONE) {
    return IsComposeboxIpadEnabled();
  }
  return true;
}

BASE_FEATURE(kTabGroupColorOnSurface, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsTabGroupColorOnSurfaceEnabled() {
  return base::FeatureList::IsEnabled(kTabGroupColorOnSurface);
}

BASE_FEATURE(kOmniboxCrashFixKillSwitch, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsOmniboxCrashFixKillSwitchEnabled() {
  return base::FeatureList::IsEnabled(kOmniboxCrashFixKillSwitch);
}

BASE_FEATURE(kAIMEligibilityServiceStartWithProfile,
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsAIMEligibilityServiceStartWithProfileEnabled() {
  return base::FeatureList::IsEnabled(kAIMEligibilityServiceStartWithProfile);
}

BASE_FEATURE(kAIMNTPEntrypointTablet, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsAIMNTPEntrypointTabletEnabled() {
  return base::FeatureList::IsEnabled(kAIMNTPEntrypointTablet);
}

BASE_FEATURE(kAIMEligibilityRefreshNTPModules,
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsAIMEligibilityRefreshNTPModulesEnabled() {
  return base::FeatureList::IsEnabled(kAIMEligibilityRefreshNTPModules);
}

BASE_FEATURE(kIOSWebContextMenuNewTitle, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsIOSWebContextMenuNewTitleEnabled() {
  return base::FeatureList::IsEnabled(kIOSWebContextMenuNewTitle);
}

BASE_FEATURE(kCloseOtherTabs, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsCloseOtherTabsEnabled() {
  return base::FeatureList::IsEnabled(kCloseOtherTabs);
}

BASE_FEATURE(kAssistantSheet, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsAssistantSheetEnabled() {
  return base::FeatureList::IsEnabled(kAssistantSheet);
}

BASE_FEATURE(kComposeboxIpad, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsComposeboxIpadEnabled() {
  return base::FeatureList::IsEnabled(kComposeboxIpad);
}

BASE_FEATURE(kChromeNextIa, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsChromeNextIaEnabled() {
  return base::FeatureList::IsEnabled(kChromeNextIa);
}

BASE_FEATURE(kComposeboxAIMDisabled, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsComposeboxAIMDisabled() {
  return base::FeatureList::IsEnabled(kComposeboxAIMDisabled);
}

NSString* const kNewStartupFlowKey = @"IsEnableNewStartupFlowEnabled";

BASE_FEATURE(kEnableNewStartupFlow, base::FEATURE_DISABLED_BY_DEFAULT);

namespace {

enum class NewStartupFlowStatus {
  kUnspecified,
  kEnabled,
  kDisabled,
};

// Tracks the cached state for the current session.
NewStartupFlowStatus startup_flow_status = NewStartupFlowStatus::kUnspecified;

}  // namespace

bool IsEnableNewStartupFlowEnabled() {
  // If we haven't checked the defaults yet this session, do it now.
  if (startup_flow_status == NewStartupFlowStatus::kUnspecified) {
    const bool is_enabled =
        [[NSUserDefaults standardUserDefaults] boolForKey:kNewStartupFlowKey];
    startup_flow_status = is_enabled ? NewStartupFlowStatus::kEnabled
                                     : NewStartupFlowStatus::kDisabled;
  }
  return startup_flow_status == NewStartupFlowStatus::kEnabled;
}

void SaveEnableNewStartupFlowForNextStart() {
  const bool enabled = base::FeatureList::IsEnabled(kEnableNewStartupFlow);
  [[NSUserDefaults standardUserDefaults] setBool:enabled
                                          forKey:kNewStartupFlowKey];
}

void ResetEnableNewStartupFlowEnabledForTesting() {
  startup_flow_status = NewStartupFlowStatus::kUnspecified;
}

// Flags for Share Ablation study.
BASE_FEATURE(kDisableShareButton, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kShareInOmniboxLongPress, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kShareInOverflowMenu, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kShareInVerbatimMatch, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kUseSceneViewController, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsUseSceneViewControllerEnabled() {
  if (IsChromeNextIaEnabled()) {
    return true;
  }
  return base::FeatureList::IsEnabled(kUseSceneViewController);
}

BASE_FEATURE(kDisableComposeboxFromAIMNTP, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsDisableComposeboxFromAIMNTPEnabled() {
  return base::FeatureList::IsEnabled(kDisableComposeboxFromAIMNTP);
}
