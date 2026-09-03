// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/public/features/features.h"

#import <algorithm>
#import <string>
#import <vector>

#import "base/containers/fixed_flat_set.h"
#import "base/metrics/field_trial_params.h"
#import "base/strings/string_split.h"
#import "base/strings/string_util.h"
#import "components/segmentation_platform/public/features.h"
#import "components/sync/base/features.h"
#import "components/sync_preferences/features.h"
#import "components/tab_groups/features.h"
#import "components/variations/service/variations_service.h"
#import "components/variations/service/variations_service_utils.h"
#import "components/version_info/channel.h"
#import "crypto/features.h"
#import "ios/chrome/app/background_mode_buildflags.h"
#import "ios/chrome/browser/ntp/shared/metrics/feed_metrics_constants.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/common/channel_info.h"
#import "ui/base/device_form_factor.h"

BASE_FEATURE(kTestFeature, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSafetyCheckAutorunByManagerKillswitch,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSafetyCheckModuleHiddenIfNoIssuesKillswitch,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kHideToolbar, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsHideToolbarEnabled() {
  return base::FeatureList::IsEnabled(kHideToolbar);
}
BASE_FEATURE(kHideFuseboxVoiceLensActions, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSharedHighlightingIOS, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kIOSBrowserEditMenuMetrics, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kIOSLevelUp, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsLevelUpEnabled() {
  return base::FeatureList::IsEnabled(kIOSLevelUp);
}

BASE_FEATURE(kEnableLensInOmniboxCopiedImage,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensLoadAIMInLensResultPage, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensOverlayEnableLandscapeCompatibility,
             "EnableLensOverlayLandscapeSupport",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensOverlayNavigationHistory, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensOverlayCustomBottomSheet, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensSearchHeadersCheckEnabled, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableTraitCollectionWorkAround,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kRemoveExcessNTPs, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTCRexKillSwitch,
             "kTCRexKillSwitch",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kContextualPanelForceShowEntrypoint,
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsContextualPanelForceShowEntrypointEnabled() {
  return base::FeatureList::IsEnabled(kContextualPanelForceShowEntrypoint);
}

BASE_FEATURE(kNonModalDefaultBrowserPromoImpressionLimit,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(int,
                   kNonModalDefaultBrowserPromoImpressionLimitParam,
                   &kNonModalDefaultBrowserPromoImpressionLimit,
                   "impression-limit",
                   3);

bool IsSafetyCheckAutorunByManagerEnabled() {
  return base::FeatureList::IsEnabled(kSafetyCheckAutorunByManagerKillswitch);
}

bool ShouldHideSafetyCheckModuleIfNoIssues() {
  return base::FeatureList::IsEnabled(
      kSafetyCheckModuleHiddenIfNoIssuesKillswitch);
}

BASE_FEATURE(kIOSChooseFromDriveSignedOut, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kIOSDateToCalendarSignedOut, base::FEATURE_DISABLED_BY_DEFAULT);

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

BASE_FEATURE_PARAM(int,
                   kDownloadListUITypeFeatureParam,
                   &kDownloadList,
                   kDownloadListUITypeParam,
                   static_cast<int>(DownloadListUIType::kDefaultUI));

bool IsDownloadListEnabled() {
  return base::FeatureList::IsEnabled(kDownloadList);
}

DownloadListUIType CurrentDownloadListUIType() {
  CHECK(IsDownloadListEnabled());
  return static_cast<DownloadListUIType>(kDownloadListUITypeFeatureParam.Get());
}

BASE_FEATURE(kDownloadList, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDownloadListPagination, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsDownloadListPaginationEnabled() {
  return base::FeatureList::IsEnabled(kDownloadListPagination);
}

const char kEnableServerDrivenBackgroundRefreshSchedule[] =
    "EnableServerDrivenBackgroundRefreshSchedule";
const char kEnableRecurringBackgroundRefreshSchedule[] =
    "EnableRecurringBackgroundRefreshSchedule";
const char kMaxCacheAgeInSeconds[] = "MaxCacheAgeInSeconds";
const char kBackgroundRefreshIntervalInSeconds[] =
    "BackgroundRefreshIntervalInSeconds";
const char kBackgroundRefreshMaxAgeInSeconds[] =
    "BackgroundRefreshMaxAgeInSeconds";

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

BASE_FEATURE_PARAM(int,
                   kContentPushNotificationsExperimentTypeFeatureParam,
                   &kContentPushNotifications,
                   kContentPushNotificationsExperimentType,
                   0);

NotificationsExperimentType ContentNotificationsExperimentTypeEnabled() {
  // This translates to the `NotificationsExperimentType` enum.
  // Value 0 corresponds to `Enabled` on the feature flag. Only activates the
  // Settings tab for content notifications.
  return static_cast<NotificationsExperimentType>(
      kContentPushNotificationsExperimentTypeFeatureParam.Get());
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

BASE_FEATURE(kIOSKeyboardAccessoryDefaultView,
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsIOSKeyboardAccessoryDefaultViewEnabled() {
  return base::FeatureList::IsEnabled(kIOSKeyboardAccessoryDefaultView);
}

BASE_FEATURE(kInactiveNavigationAfterAppLaunchKillSwitch,
             "kInactiveNavigationAfterAppLaunchKillSwitch",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsPinnedTabsEnabled() {
  return ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_TABLET;
}

BASE_FEATURE(kEnableAppBackgroundRefresh, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsAppBackgroundRefreshEnabled() {
  if (!base::FeatureList::IsEnabled(
          crypto::features::kMigrateIOSKeychainAccessibility)) {
    return false;
  }
  return base::FeatureList::IsEnabled(kEnableAppBackgroundRefresh);
}

BASE_FEATURE(kEnableTraitCollectionRegistration,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSeparateProfilesForManagedAccounts,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAuthenticationFlowReauthFirstKillswitch,
             base::FEATURE_DISABLED_BY_DEFAULT);

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

BASE_FEATURE(kIOSOneTimeDefaultBrowserNotification,
             base::FEATURE_DISABLED_BY_DEFAULT);

constexpr base::FeatureParam<std::string> kFRESignInHeaderTextUpdateParam{
    &kFRESignInHeaderTextUpdate,
    /*name=*/"FRESignInHeaderTextUpdateParam",
    /*default_value=*/"Arm1"};

const std::string_view kFRESignInHeaderTextUpdateParamArm0 = "Arm0";
const std::string_view kFRESignInHeaderTextUpdateParamArm1 = "Arm1";

BASE_FEATURE(kFRESignInHeaderTextUpdate, base::FEATURE_ENABLED_BY_DEFAULT);

bool FRESignInHeaderTextUpdate() {
  return base::FeatureList::IsEnabled(kFRESignInHeaderTextUpdate);
}

BASE_FEATURE(kIOSPushNotificationMultiProfile,
             base::FEATURE_ENABLED_BY_DEFAULT);

const char kFullscreenTransitionSlower[] = "SlowFullscreenTransitionSpeed";
const char kFullscreenTransitionDefaultSpeed[] =
    "MediumFullscreenTransitionSpeed";
const char kFullscreenTransitionFaster[] = "FastFullscreenTransitionSpeed";
const char kFullscreenTransitionSpeedParam[] = "FullscreenTransitionSpeed";

BASE_FEATURE_PARAM(int,
                   kFullscreenTransitionSpeedFeatureParam,
                   &kFullscreenTransitionSpeed,
                   kFullscreenTransitionSpeedParam,
                   1);

bool IsFullscreenTransitionSpeedSet() {
  return base::FeatureList::IsEnabled(kFullscreenTransitionSpeed);
}

FullscreenTransitionSpeed FullscreenTransitionSpeedParam() {
  return static_cast<FullscreenTransitionSpeed>(
      kFullscreenTransitionSpeedFeatureParam.Get());
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

BASE_FEATURE(kIOSOneTapMiniMapRestrictions, base::FEATURE_ENABLED_BY_DEFAULT);

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
BASE_FEATURE_PARAM(int,
                   kIOSOneTapMiniMapRestrictionMinCharsParam,
                   &kIOSOneTapMiniMapRestrictions,
                   kIOSOneTapMiniMapRestrictionMinCharsParamName,
                   14);
const char kIOSOneTapMiniMapRestrictionMaxSectionsParamName[] =
    "ios-one-tap-minimap-max-section";
BASE_FEATURE_PARAM(int,
                   kIOSOneTapMiniMapRestrictionMaxSectionsParam,
                   &kIOSOneTapMiniMapRestrictions,
                   kIOSOneTapMiniMapRestrictionMaxSectionsParamName,
                   0);
const char kIOSOneTapMiniMapRestrictionLongestWordMinCharsParamName[] =
    "ios-one-tap-minimap-longest-word-min-chars";
BASE_FEATURE_PARAM(int,
                   kIOSOneTapMiniMapRestrictionLongestWordMinCharsParam,
                   &kIOSOneTapMiniMapRestrictions,
                   kIOSOneTapMiniMapRestrictionLongestWordMinCharsParamName,
                   0);
const char kIOSOneTapMiniMapRestrictionMinAlphanumProportionParamName[] =
    "ios-one-tap-minimap-min-alphanum-proportion";
constexpr base::FeatureParam<double>
    kIOSOneTapMiniMapRestrictionMinAlphanumProportionParam{
        &kIOSOneTapMiniMapRestrictions,
        /*name=*/kIOSOneTapMiniMapRestrictionMinAlphanumProportionParamName,
        /*default_value=*/0.8};

namespace {

// Country codes where Mini Map Universal Links are excluded.
constexpr auto kExcludedCountryCodes =
    base::MakeFixedFlatSet<std::string_view>({
        "at", "be", "bg", "cy", "cz", "de", "dk", "ee", "es", "fi", "fr",
        "gr", "hr", "hu", "ie", "is", "it", "li", "lt", "lu", "lv", "mt",
        "ng", "nl", "no", "pl", "pt", "ro", "se", "si", "sk", "tr",
    });

// Checks whether the user is located in an excluded country.
bool IsInExcludedCountry() {
  ApplicationContext* application_context = GetApplicationContext();
  variations::VariationsService* variations_service = nullptr;
  if (application_context) {
    variations_service = application_context->GetVariationsService();
  }
  std::string country_code =
      base::ToLowerASCII(variations::GetCurrentCountryCode(variations_service));
  return kExcludedCountryCodes.contains(country_code);
}

}  // namespace

bool IsMiniMapUniversalLinkEnabled() {
  return !IsInExcludedCountry();
}

BASE_FEATURE(kIOSMiniMapLinkifiedAddress, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsNotificationCollisionManagementEnabled() {
  return base::FeatureList::IsEnabled(kNotificationCollisionManagement);
}

BASE_FEATURE(kNotificationCollisionManagement,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kIOSProvidesAppNotificationSettings,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kNTPBackgroundColorSlider, base::FEATURE_ENABLED_BY_DEFAULT);

bool IsNTPBackgroundColorSliderEnabled() {
  return base::FeatureList::IsEnabled(kNTPBackgroundColorSlider);
}

BASE_FEATURE(kNTPBackgroundDownsampleImage, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kRunDefaultStatusCheck, base::FEATURE_ENABLED_BY_DEFAULT);

bool IsRunDefaultStatusCheckEnabled() {
  return base::FeatureList::IsEnabled(kRunDefaultStatusCheck);
}

BASE_FEATURE(kBestOfAppFRE, base::FEATURE_ENABLED_BY_DEFAULT);

bool IsBestOfAppFREEnabled() {
  return base::FeatureList::IsEnabled(kBestOfAppFRE);
}

// Enable Lens Promo arm by default.
constexpr base::FeatureParam<std::string> kBestOfAppFREVariantParam{
    &kBestOfAppFRE,
    /*name=*/"variant",
    /*default_value=*/"1"};

std::vector<std::string> GetBestOfAppFREActiveVariants() {
  return SplitString(kBestOfAppFREVariantParam.Get(), ",",
                     base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
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

bool IsBestOfAppBestFeaturesEnabled() {
  return IsBestOfAppFREEnabled() &&
         std::ranges::contains(GetBestOfAppFREActiveVariants(), "3");
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

BASE_FEATURE(kDisableKeyboardAccessory, base::FEATURE_DISABLED_BY_DEFAULT);

const char kDisableKeyboardAccessoryParam[] = "kDisableKeyboardAccessoryParam";
const char kDisableKeyboardAccessoryOnlySymbols[] =
    "kDisableKeyboardAccessoryOnlySymbols";
const char kDisableKeyboardAccessoryOnlyFeatures[] =
    "kDisableKeyboardAccessoryOnlyFeatures";
const char kDisableKeyboardAccessoryCompletely[] =
    "kDisableKeyboardAccessoryCompletely";

BASE_FEATURE(kEnableFuseboxKeyboardAccessory,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAIOmniboxAskPlaceholder, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsAIOmniboxAskPlaceholderEnabled() {
  return base::FeatureList::IsEnabled(kAIOmniboxAskPlaceholder);
}

const char kEnableFuseboxKeyboardAccessoryParam[] =
    "kEnableFuseboxKeyboardAccessoryParam";
const char kEnableFuseboxKeyboardAccessoryOnlySymbols[] =
    "kEnableFuseboxKeyboardAccessoryOnlySymbols";
const char kEnableFuseboxKeyboardAccessoryOnlyFeatures[] =
    "kEnableFuseboxKeyboardAccessoryOnlyFeatures";
const char kEnableFuseboxKeyboardAccessoryBoth[] =
    "kEnableFuseboxKeyboardAccessoryBoth";

bool ShouldShowKeyboardAccessory() {
  if (!IsComposeboxIOSEnabled()) {
    // Keyboard accessory is enabled by default.
    if (!base::FeatureList::IsEnabled(kDisableKeyboardAccessory)) {
      return true;
    }
    std::string feature_param = base::GetFieldTrialParamValueByFeature(
        kDisableKeyboardAccessory, kDisableKeyboardAccessoryParam);
    return feature_param != kDisableKeyboardAccessoryCompletely;
  }

  // Fusebox:
  // Keyboard accessory is disabled by default but can be forced with a flag.
  return base::FeatureList::IsEnabled(kEnableFuseboxKeyboardAccessory);
}

bool ShouldShowKeyboardAccessorySymbols() {
  if (IsComposeboxIOSEnabled()) {
    if (base::FeatureList::IsEnabled(kEnableFuseboxKeyboardAccessory)) {
      std::string feature_param = base::GetFieldTrialParamValueByFeature(
          kEnableFuseboxKeyboardAccessory,
          kEnableFuseboxKeyboardAccessoryParam);
      return feature_param != kEnableFuseboxKeyboardAccessoryOnlyFeatures;
    }
    return false;
  }

  if (!base::FeatureList::IsEnabled(kDisableKeyboardAccessory)) {
    return true;
  }
  std::string feature_param = base::GetFieldTrialParamValueByFeature(
      kDisableKeyboardAccessory, kDisableKeyboardAccessoryParam);
  return feature_param == kDisableKeyboardAccessoryOnlySymbols;
}

bool ShouldShowKeyboardAccessoryFeatures() {
  if (IsComposeboxIOSEnabled()) {
    if (base::FeatureList::IsEnabled(kEnableFuseboxKeyboardAccessory)) {
      std::string feature_param = base::GetFieldTrialParamValueByFeature(
          kEnableFuseboxKeyboardAccessory,
          kEnableFuseboxKeyboardAccessoryParam);
      return feature_param != kEnableFuseboxKeyboardAccessoryOnlySymbols;
    }
    return false;
  }

  if (!base::FeatureList::IsEnabled(kDisableKeyboardAccessory)) {
    return true;
  }
  std::string feature_param = base::GetFieldTrialParamValueByFeature(
      kDisableKeyboardAccessory, kDisableKeyboardAccessoryParam);
  return feature_param == kDisableKeyboardAccessoryOnlyFeatures;
}

BASE_FEATURE(kLocationBarBadgeMigration, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsLocationBarBadgeMigrationEnabled() {
  return base::FeatureList::IsEnabled(kLocationBarBadgeMigration);
}

bool IsComposeboxIOSEnabled() {
  return true;
}

BASE_FEATURE(kContextMenuPreviewDownsampleImage,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kOmniboxCrashFixKillSwitch, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsOmniboxCrashFixKillSwitchEnabled() {
  return base::FeatureList::IsEnabled(kOmniboxCrashFixKillSwitch);
}

BASE_FEATURE(kAIMEligibilityServiceStartWithProfile,
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsAIMEligibilityServiceStartWithProfileEnabled() {
  return base::FeatureList::IsEnabled(kAIMEligibilityServiceStartWithProfile);
}

BASE_FEATURE(kAIMNTPEntrypointTablet, base::FEATURE_ENABLED_BY_DEFAULT);

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

BASE_FEATURE(kAtMemoryContextMenuEntryPoint, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsAtMemoryContextMenuEntryPointEnabled() {
  return base::FeatureList::IsEnabled(kAtMemoryContextMenuEntryPoint);
}

BASE_FEATURE(kAssistantContainer, base::FEATURE_DISABLED_BY_DEFAULT);

const char kAssistantContainerParam[] = "kAssistantContainerParam";
const char kAssistantContainerParamDebug[] = "kAssistantContainerParamDebug";
const char kAssistantContainerMediumDetentPercentParam[] =
    "AssistantMediumDetentPercent";

bool IsAssistantContainerEnabled() {
  if (IsAimCobrowseEnabled()) {
    return true;
  }

  if (IsAssistantSidePanelEnabled()) {
    return true;
  }
  return base::FeatureList::IsEnabled(kAssistantContainer);
}

bool IsAssistantContainerDebugEnabled() {
  if (!base::FeatureList::IsEnabled(kAssistantContainer)) {
    return false;
  }
  std::string feature_param = base::GetFieldTrialParamValueByFeature(
      kAssistantContainer, kAssistantContainerParam);
  return !feature_param.empty();
}

NSInteger GetAssistantMediumDetentPercentage() {
  if (!base::FeatureList::IsEnabled(kAssistantContainer)) {
    return 0;
  }
  return base::GetFieldTrialParamByFeatureAsInt(
      kAssistantContainer, kAssistantContainerMediumDetentPercentParam, 0);
}

BASE_FEATURE(kComposeboxPlusButtonBottomSheet,
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsComposeboxPlusButtonBottomSheet() {
  return base::FeatureList::IsEnabled(kComposeboxPlusButtonBottomSheet);
}

BASE_FEATURE(kChromeNextIa, base::FEATURE_DISABLED_BY_DEFAULT);

constexpr base::FeatureParam<bool> kChromeNextIaLensIconVisible{
    &kChromeNextIa, "chrome_next_ia_lens_icon_visible", false};

constexpr base::FeatureParam<bool> kChromeNextIaShareIconVisible{
    &kChromeNextIa, "chrome_next_ia_share_icon_visible", false};

bool IsChromeNextIaEnabled() {
  if (!IsComposeboxIOSEnabled()) {
    return false;
  }
  return base::FeatureList::IsEnabled(kChromeNextIa);
}

bool IsChromeNextIaLensIconVisible() {
  return IsChromeNextIaEnabled() && kChromeNextIaLensIconVisible.Get();
}

bool IsChromeNextIaShareIconVisible() {
  return IsChromeNextIaEnabled() && kChromeNextIaShareIconVisible.Get();
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

BASE_FEATURE(kUseSceneViewController, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsUseSceneViewControllerEnabled() {
  if (IsChromeNextIaEnabled() || IsAssistantSidePanelEnabled()) {
    return true;
  }
  return base::FeatureList::IsEnabled(kUseSceneViewController);
}

BASE_FEATURE(kDisplayTracing, base::FEATURE_ENABLED_BY_DEFAULT);

bool IsDisplayTracingEnabled() {
  return base::FeatureList::IsEnabled(kDisplayTracing);
}

BASE_FEATURE(kDisableComposeboxFromAIMNTP, base::FEATURE_ENABLED_BY_DEFAULT);

bool IsDisableComposeboxFromAIMNTPEnabled() {
  return base::FeatureList::IsEnabled(kDisableComposeboxFromAIMNTP);
}

BASE_FEATURE(kRecordRecentActiveDays, base::FEATURE_ENABLED_BY_DEFAULT);

bool IsRecordRecentActiveDaysEnabled() {
  return base::FeatureList::IsEnabled(kRecordRecentActiveDays);
}

BASE_FEATURE(kIOSSoftLock, base::FEATURE_ENABLED_BY_DEFAULT);

bool IsIOSSoftLockEnabled() {
  return base::FeatureList::IsEnabled(kIOSSoftLock);
}

const char kIOSSoftLockBackgroundThresholdParam[] =
    "soft-lock-background-threshold-minutes";

const base::FeatureParam<base::TimeDelta> kIOSSoftLockBackgroundThreshold{
    &kIOSSoftLock, kIOSSoftLockBackgroundThresholdParam, base::Minutes(10)};

BASE_FEATURE(kAimCobrowse, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsAimCobrowseEnabled() {
  return base::FeatureList::IsEnabled(kAimCobrowse);
}

BASE_FEATURE(kPreventCobrowseOnAimSrpTap, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFeedbackEntryPointsRequireCanSubmitFeedbackCapability,
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsFeedbackEntryPointsRequireCanSubmitFeedbackCapabilityEnabled() {
  return base::FeatureList::IsEnabled(
      kFeedbackEntryPointsRequireCanSubmitFeedbackCapability);
}

BASE_FEATURE(kDisableFeedbackForIneligibleUsers,
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsDisableFeedbackForIneligibleUsersEnabled() {
  return base::FeatureList::IsEnabled(kDisableFeedbackForIneligibleUsers);
}

BASE_FEATURE(kIncludeSystemLogInFeedback, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsIncludeSystemLogInFeedbackEnabled() {
  return base::FeatureList::IsEnabled(kIncludeSystemLogInFeedback);
}

BASE_FEATURE(kFullscreenRefactoring, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsFullscreenRefactoringEnabled() {
  return IsChromeNextIaEnabled() ||
         base::FeatureList::IsEnabled(kFullscreenRefactoring);
}

BASE_FEATURE(kFullscreenEasedTransitions, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsFullscreenEasedTransitionsEnabled() {
  return IsFullscreenRefactoringEnabled() &&
         base::FeatureList::IsEnabled(kFullscreenEasedTransitions);
}

BASE_FEATURE(kPageToolsFeatureUnavailability, base::FEATURE_ENABLED_BY_DEFAULT);

bool IsPageToolsFeatureUnavailabilityEnabled() {
  return base::FeatureList::IsEnabled(kPageToolsFeatureUnavailability);
}

BASE_FEATURE(kAskAboutThisPage, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsAskAboutThisPageEnabled() {
  return IsAimCobrowseEnabled() &&
         base::FeatureList::IsEnabled(kAskAboutThisPage);
}

BASE_FEATURE(kGridMediatorSnapshotUpdateBatchGuard,
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsGridMediatorSnapshotUpdateBatchGuardEnabled() {
  return base::FeatureList::IsEnabled(kGridMediatorSnapshotUpdateBatchGuard);
}

BASE_FEATURE(kAssistantSidePanel, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsAssistantSidePanelEnabled() {
  return base::FeatureList::IsEnabled(kAssistantSidePanel) &&
         IsChromeNextIaEnabled() && IsFullscreenRefactoringEnabled();
}

BASE_FEATURE(kYourSavedInfoSettingsPageIos, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsYourSavedInfoSettingsPageIosEnabled() {
  return base::FeatureList::IsEnabled(kYourSavedInfoSettingsPageIos);
}

BASE_FEATURE(kBackgroundRefreshRegressionTest,
             base::FEATURE_DISABLED_BY_DEFAULT);

namespace {
const base::FeatureParam<BackgroundRefreshRegressionTestArm>::Option
    kBackgroundRefreshRegressionTestArmOptions[] = {
        {BackgroundRefreshRegressionTestArm::kControl, "control"},
        {BackgroundRefreshRegressionTestArm::kBaseline, "baseline"},
        {BackgroundRefreshRegressionTestArm::kShortPersistenceDelay,
         "short-persistence-delay"},
        {BackgroundRefreshRegressionTestArm::kLongRefreshInterval,
         "long-refresh-interval"},
        {BackgroundRefreshRegressionTestArm::kNoBeacon, "no-beacon"}};
}  // namespace

const base::FeatureParam<BackgroundRefreshRegressionTestArm>
    kBackgroundRefreshRegressionTestArmParam{
        &kBackgroundRefreshRegressionTest, "regression_test_arm",
        BackgroundRefreshRegressionTestArm::kControl,
        &kBackgroundRefreshRegressionTestArmOptions};

BackgroundRefreshRegressionTestArm GetBackgroundRefreshRegressionTestArm() {
  return kBackgroundRefreshRegressionTestArmParam.Get();
}

BASE_FEATURE(kOpenEditGroupViewByTappingTitle,
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsOpenEditGroupViewByTappingTitleEnabled() {
  return base::FeatureList::IsEnabled(kOpenEditGroupViewByTappingTitle);
}

bool IsUpdateTabGroupColorsEnabled() {
  return base::FeatureList::IsEnabled(tab_groups::kUpdateTabGroupColors);
}

// Enables the plus button in NTP fakebox.
BASE_FEATURE(kPlusButtonInFakebox, base::FEATURE_DISABLED_BY_DEFAULT);

// Returns true if the plus button in NTP fakebox is enabled.
bool IsPlusButtonInFakeboxEnabled() {
  if (!IsComposeboxIOSEnabled()) {
    return false;
  }

  return base::FeatureList::IsEnabled(kPlusButtonInFakebox);
}

BASE_FEATURE(kAssistantAimMinimizedState, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsAssistantAimMinimizedStateEnabled() {
  return base::FeatureList::IsEnabled(kAssistantAimMinimizedState);
}

BASE_FEATURE(kIOSBackendPromoServiceIntegration,
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsIOSBackendPromoServiceIntegrationEnabled() {
  return base::FeatureList::IsEnabled(kIOSBackendPromoServiceIntegration);
}

BASE_FEATURE(kIOSBackendPromoCustomUI, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsIOSBackendPromoCustomUIEnabled() {
  return base::FeatureList::IsEnabled(kIOSBackendPromoCustomUI);
}

BASE_FEATURE(kUseUIGraphicsImageRendererForFallbackIcons,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kIOSDarkModeDetection, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsIOSDarkModeDetectionEnabled() {
  return base::FeatureList::IsEnabled(kIOSDarkModeDetection);
}

BASE_FEATURE(kIdentityAwareness, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsIdentityAwarenessEnabled() {
  return base::FeatureList::IsEnabled(kIdentityAwareness);
}

BASE_FEATURE(kAiSubscriptionAvatarRingIOS, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsAiSubscriptionAvatarRingIOSEnabled() {
  return base::FeatureList::IsEnabled(kAiSubscriptionAvatarRingIOS);
}

BASE_FEATURE(kInfobarBannerRevamp, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsInfobarBannerRevampEnabled() {
  return base::FeatureList::IsEnabled(kInfobarBannerRevamp);
}

BASE_FEATURE(kIOSPhishGuardPasteShortcutDetection,
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsIOSPhishGuardPasteShortcutDetectionEnabled() {
  return base::FeatureList::IsEnabled(kIOSPhishGuardPasteShortcutDetection);
}

BASE_FEATURE(kAppBarHideLabels, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsAppBarLabelsHidden() {
  return base::FeatureList::IsEnabled(kAppBarHideLabels);
}

BASE_FEATURE(kSupportGoogleOneDeepLink, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsGoogleOneDeepLinkEnabled() {
  return base::FeatureList::IsEnabled(kSupportGoogleOneDeepLink);
}

BASE_FEATURE(kEnableDiscoverBackgroundRefresh,
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsDiscoverBackgroundRefreshEnabled() {
  return base::FeatureList::IsEnabled(kEnableDiscoverBackgroundRefresh);
}

const base::FeatureParam<base::TimeDelta>
    kDiscoverFeedBackgroundRefreshNoServiceInterval{
        &kEnableDiscoverBackgroundRefresh,
        "discover_refresh_no_service_interval", base::Minutes(30)};

const base::FeatureParam<base::TimeDelta>
    kDiscoverFeedBackgroundRefreshNoDateInterval{
        &kEnableDiscoverBackgroundRefresh, "discover_refresh_no_date_interval",
        base::Hours(24)};  // 1 day

const base::FeatureParam<base::TimeDelta>
    kDiscoverFeedBackgroundRefreshMinBuffer{&kEnableDiscoverBackgroundRefresh,
                                            "discover_refresh_min_buffer",
                                            base::Minutes(15)};

BASE_FEATURE(kAppBarHideInFullscreen, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsAppBarHiddenInFullscreen() {
  return base::FeatureList::IsEnabled(kAppBarHideInFullscreen);
}

BASE_FEATURE(kDefaultBottomOmniboxOnIOS, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsDefaultBottomOmniboxOnIOSEnabled() {
  return base::FeatureList::IsEnabled(kDefaultBottomOmniboxOnIOS);
}

BASE_FEATURE(kGlassToolbar, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsGlassToolbarEnabled() {
  if (@available(iOS 26, *)) {
    return base::FeatureList::IsEnabled(kGlassToolbar);
  }
  return false;
}

BASE_FEATURE(kNextOldDesign, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsNextOldDesignEnabled() {
  if (!IsChromeNextIaEnabled()) {
    return false;
  }
  return base::FeatureList::IsEnabled(kNextOldDesign);
}

BASE_FEATURE(kRecoverTabsOfLastClosedWindow, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDomainLevelSitePermissions, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsDomainLevelSitePermissionsEnabled() {
  return base::FeatureList::IsEnabled(kDomainLevelSitePermissions);
}

BASE_FEATURE(kAimHistoryThreadsManagement, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsAimHistoryThreadsManagementEnabled() {
  return base::FeatureList::IsEnabled(kAimHistoryThreadsManagement);
}
