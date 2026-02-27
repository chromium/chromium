// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of about_flags for iOS that sets flags based on experimental
// settings.

#import "ios/chrome/browser/flags/about_flags.h"

#import <UIKit/UIKit.h>
#import <stddef.h>
#import <stdint.h>

#import "base/apple/foundation_util.h"
#import "base/base_switches.h"
#import "base/check_op.h"
#import "base/debug/debugging_buildflags.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/no_destructor.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/system/sys_info.h"
#import "components/autofill/core/browser/manual_testing_import.h"
#import "components/autofill/core/common/autofill_debug_features.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/core/common/autofill_payments_features.h"
#import "components/autofill/core/common/autofill_switches.h"
#import "components/autofill/ios/common/features.h"
#import "components/bookmarks/common/bookmark_features.h"
#import "components/browser_sync/browser_sync_switches.h"
#import "components/collaboration/public/features.h"
#import "components/commerce/core/commerce_feature_list.h"
#import "components/commerce/core/flag_descriptions.h"
#import "components/content_settings/core/common/features.h"
#import "components/data_sharing/public/features.h"
#import "components/data_sharing/public/switches.h"
#import "components/desktop_to_mobile_promos/features.h"
#import "components/dom_distiller/core/dom_distiller_features.h"
#import "components/dom_distiller/core/dom_distiller_switches.h"
#import "components/download/public/background_service/features.h"
#import "components/enterprise/browser/enterprise_switches.h"
#import "components/enterprise/buildflags/buildflags.h"
#import "components/enterprise/connectors/core/features.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/feature_list.h"
#import "components/feed/feed_feature_list.h"
#import "components/history/core/browser/features.h"
#import "components/ntp_tiles/features.h"
#import "components/ntp_tiles/switches.h"
#import "components/omnibox/browser/aim_eligibility_service_features.h"
#import "components/omnibox/browser/omnibox_field_trial.h"
#import "components/omnibox/common/omnibox_feature_configs.h"
#import "components/omnibox/common/omnibox_features.h"
#import "components/optimization_guide/core/optimization_guide_features.h"
#import "components/optimization_guide/core/optimization_guide_switches.h"
#import "components/page_content_annotations/core/page_content_annotations_features.h"
#import "components/password_manager/core/browser/features/password_features.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/payments/core/features.h"
#import "components/policy/core/common/policy_loader_ios_constants.h"
#import "components/policy/policy_constants.h"
#import "components/regional_capabilities/regional_capabilities_switches.h"
#import "components/safe_browsing/core/common/features.h"
#import "components/safe_browsing/ios/browser/web_ui/features.h"
#import "components/search/ntp_features.cc"
#import "components/search_engines/search_engines_switches.h"
#import "components/segmentation_platform/embedder/home_modules/constants.h"
#import "components/segmentation_platform/public/constants.h"
#import "components/segmentation_platform/public/features.h"
#import "components/send_tab_to_self/features.h"
#import "components/shared_highlighting/core/common/shared_highlighting_features.h"
#import "components/sharing_message/features.h"
#import "components/signin/core/browser/account_reconcilor.h"
#import "components/signin/ios/browser/features.h"
#import "components/signin/public/base/signin_switches.h"
#import "components/strike_database/strike_database_features.h"
#import "components/strings/grit/components_strings.h"
#import "components/supervised_user/core/common/features.h"
#import "components/sync/base/command_line_switches.h"
#import "components/sync/base/features.h"
#import "components/sync/base/pref_names.h"
#import "components/sync_preferences/features.h"
#import "components/translate/core/browser/translate_prefs.h"
#import "components/translate/core/common/translate_util.h"
#import "components/variations/net/variations_command_line.h"
#import "components/variations/variations_switches.h"
#import "components/webui/flags/feature_entry.h"
#import "components/webui/flags/feature_entry_macros.h"
#import "components/webui/flags/flags_storage.h"
#import "components/webui/flags/flags_ui_switches.h"
#import "crypto/features.h"
#import "ios/chrome/app/background_mode_buildflags.h"
#import "ios/chrome/browser/autofill/model/features.h"
#import "ios/chrome/browser/badges/model/features.h"
#import "ios/chrome/browser/composebox/public/features.h"
#import "ios/chrome/browser/crash_report/model/features.h"
#import "ios/chrome/browser/credential_provider/model/features.h"
#import "ios/chrome/browser/default_browser/model/features.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/default_browser/promo/public/features.h"
#import "ios/chrome/browser/download/ui/features.h"
#import "ios/chrome/browser/first_run/public/features.h"
#import "ios/chrome/browser/flags/chrome_switches.h"
#import "ios/chrome/browser/flags/ios_chrome_flag_descriptions.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/features.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/lens/ui_bundled/features.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/omnibox/public/omnibox_ui_features.h"
#import "ios/chrome/browser/page_info/certificate/features/features.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/policy/model/reporting/features.h"
#import "ios/chrome/browser/reader_mode/model/features.h"
#import "ios/chrome/browser/settings/ui_bundled/clear_browsing_data/public/features.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_manager_ui_features.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_features.h"
#import "ios/chrome/browser/text_selection/model/text_selection_util.h"
#import "ios/chrome/browser/variations/model/ios_chrome_variations_seed_fetcher.h"
#import "ios/chrome/browser/web/model/features.h"
#import "ios/chrome/browser/welcome_back/model/features.h"
#import "ios/chrome/browser/whats_new/coordinator/whats_new_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/components/enterprise/analysis/features.h"
#import "ios/components/security_interstitials/https_only_mode/feature.h"
#import "ios/public/provider/chrome/browser/app_utils/app_utils_api.h"
#import "ios/web/common/features.h"
#import "ios/web/common/user_agent.h"
#import "ios/web/common/web_view_creation_util.h"

using flags_ui::FeatureEntry;

namespace {

const FeatureEntry::Choice
    kWaitThresholdMillisecondsForCapabilitiesApiChoices[] = {
        {flags_ui::kGenericExperimentChoiceDefault, "", ""},
        {"200", signin::kWaitThresholdMillisecondsForCapabilitiesApi, "200"},
        {"500", signin::kWaitThresholdMillisecondsForCapabilitiesApi, "500"},
        {"5000", signin::kWaitThresholdMillisecondsForCapabilitiesApi, "5000"},
};

const FeatureEntry::FeatureParam
    kNTPMIAEntrypointOmniboxContainedSingleButton[] = {
        {kNTPMIAEntrypointParam,
         kNTPMIAEntrypointParamOmniboxContainedSingleButton}};
const FeatureEntry::FeatureParam kNTPMIAEntrypointOmniboxContainedInline[] = {
    {kNTPMIAEntrypointParam, kNTPMIAEntrypointParamOmniboxContainedInline}};
const FeatureEntry::FeatureParam
    kNTPMIAEntrypointOmniboxContainedEnlargedFakebox[] = {
        {kNTPMIAEntrypointParam,
         kNTPMIAEntrypointParamOmniboxContainedEnlargedFakebox}};
const FeatureEntry::FeatureParam kNTPMIAEntrypointEnlargedFakeboxNoIncognito[] =
    {{kNTPMIAEntrypointParam,
      kNTPMIAEntrypointParamEnlargedFakeboxNoIncognito}};
const FeatureEntry::FeatureParam kNTPMIAEntrypointAIMInQuickActions[] = {
    {kNTPMIAEntrypointParam, kNTPMIAEntrypointParamAIMInQuickActions}};

const FeatureEntry::FeatureVariation kNTPMIAEntrypointVariations[] = {
    {"A: Contained in Omnibox, single button",
     kNTPMIAEntrypointOmniboxContainedSingleButton, nullptr},
    {"B: Contained in Omnibox, inline with Voice and Lens",
     kNTPMIAEntrypointOmniboxContainedInline, nullptr},
    {"C: Contained in Omnibox, enlarged fakebox",
     kNTPMIAEntrypointOmniboxContainedEnlargedFakebox, nullptr},
    {"D: Contained in enlarged fakebox, without incognito shortcut",
     kNTPMIAEntrypointEnlargedFakeboxNoIncognito, nullptr},
    {"E: AIM entry point in quick actions, enlarged fakebox",
     kNTPMIAEntrypointAIMInQuickActions, nullptr},
};

const FeatureEntry::FeatureParam kComposeboxTabPickerVariationCachedAPC[] = {
    {kComposeboxTabPickerVariationParam,
     kComposeboxTabPickerVariationParamCachedAPC}};

const FeatureEntry::FeatureParam kComposeboxTabPickerVariationOnFlightAPC[] = {
    {kComposeboxTabPickerVariationParam,
     kComposeboxTabPickerVariationParamOnFlightAPC}};

const FeatureEntry::FeatureVariation kComposeboxTabPickerVariationVariations[] =
    {
        {"A) Use Cached APC", kComposeboxTabPickerVariationCachedAPC, nullptr},
        {"B) Use On flight APC", kComposeboxTabPickerVariationOnFlightAPC,
         nullptr},
};

const FeatureEntry::FeatureParam kDisableKeyboardAccessoryOnlySymbolsParam[] = {
    {kDisableKeyboardAccessoryParam, kDisableKeyboardAccessoryOnlySymbols}};

const FeatureEntry::FeatureParam kDisableKeyboardAccessoryOnlyFeaturesParam[] =
    {{kDisableKeyboardAccessoryParam, kDisableKeyboardAccessoryOnlyFeatures}};

const FeatureEntry::FeatureParam kDisableKeyboardAccessoryCompletelyParam[] = {
    {kDisableKeyboardAccessoryParam, kDisableKeyboardAccessoryCompletely}};

const FeatureEntry::FeatureVariation kDisableKeyboardAccessoryVariations[] = {
    {"A) only show symbols", kDisableKeyboardAccessoryOnlySymbolsParam,
     nullptr},
    {"B) only show lens and voice search",
     kDisableKeyboardAccessoryOnlyFeaturesParam, nullptr},
    {"C) disable completely", kDisableKeyboardAccessoryCompletelyParam,
     nullptr}};

const FeatureEntry::FeatureParam
    kEnableFuseboxKeyboardAccessoryOnlySymbolsParam[] = {
        {kEnableFuseboxKeyboardAccessoryParam,
         kEnableFuseboxKeyboardAccessoryOnlySymbols}};

const FeatureEntry::FeatureParam
    kEnableFuseboxKeyboardAccessoryOnlyFeaturesParam[] = {
        {kEnableFuseboxKeyboardAccessoryParam,
         kEnableFuseboxKeyboardAccessoryOnlyFeatures}};

const FeatureEntry::FeatureParam kEnableFuseboxKeyboardAccessoryBothParam[] = {
    {kEnableFuseboxKeyboardAccessoryParam,
     kEnableFuseboxKeyboardAccessoryBoth}};

const FeatureEntry::FeatureVariation
    kEnableFuseboxKeyboardAccessoryVariations[] = {
        {"A) only show symbols",
         kEnableFuseboxKeyboardAccessoryOnlySymbolsParam, nullptr},
        {"B) only show lens and voice search",
         kEnableFuseboxKeyboardAccessoryOnlyFeaturesParam, nullptr},
        {"C) enable both", kEnableFuseboxKeyboardAccessoryBothParam, nullptr}};

const FeatureEntry::FeatureParam
    kIOSKeyboardAccessoryTwoBubbleWithKeyboardIcon[] = {
        {kIOSKeyboardAccessoryTwoBubbleKeyboardIconParamName, "true"}};

const FeatureEntry::FeatureParam
    kIOSKeyboardAccessoryTwoBubbleWithCheckmarkIcon[] = {
        {kIOSKeyboardAccessoryTwoBubbleKeyboardIconParamName, "false"}};

const FeatureEntry::FeatureVariation
    kIOSKeyboardAccessoryTwoBubbleVariations[] = {
        {"With Keyboard Icon", kIOSKeyboardAccessoryTwoBubbleWithKeyboardIcon,
         nullptr},
        {"With Checkmark Icon", kIOSKeyboardAccessoryTwoBubbleWithCheckmarkIcon,
         nullptr}};

const FeatureEntry::FeatureParam kOmniboxUIMaxAutocompleteMatches3[] = {
    {OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "3"}};
const FeatureEntry::FeatureParam kOmniboxUIMaxAutocompleteMatches4[] = {
    {OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "4"}};
const FeatureEntry::FeatureParam kOmniboxUIMaxAutocompleteMatches5[] = {
    {OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "5"}};
const FeatureEntry::FeatureParam kOmniboxUIMaxAutocompleteMatches6[] = {
    {OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "6"}};
const FeatureEntry::FeatureParam kOmniboxUIMaxAutocompleteMatches8[] = {
    {OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "8"}};
const FeatureEntry::FeatureParam kOmniboxUIMaxAutocompleteMatches10[] = {
    {OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "10"}};
const FeatureEntry::FeatureParam kOmniboxUIMaxAutocompleteMatches12[] = {
    {OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "12"}};

const FeatureEntry::FeatureVariation
    kOmniboxUIMaxAutocompleteMatchesVariations[] = {
        {"3 matches", kOmniboxUIMaxAutocompleteMatches3, nullptr},
        {"4 matches", kOmniboxUIMaxAutocompleteMatches4, nullptr},
        {"5 matches", kOmniboxUIMaxAutocompleteMatches5, nullptr},
        {"6 matches", kOmniboxUIMaxAutocompleteMatches6, nullptr},
        {"8 matches", kOmniboxUIMaxAutocompleteMatches8, nullptr},
        {"10 matches", kOmniboxUIMaxAutocompleteMatches10, nullptr},
        {"12 matches", kOmniboxUIMaxAutocompleteMatches12, nullptr}};

const FeatureEntry::FeatureParam kContentPushNotificationsEnabledPromo[] = {
    {kContentPushNotificationsExperimentType, "1"}};
const FeatureEntry::FeatureParam kContentPushNotificationsEnabledSetupLists[] =
    {{kContentPushNotificationsExperimentType, "2"}};
const FeatureEntry::FeatureParam kContentPushNotificationsEnabledProvisional[] =
    {{kContentPushNotificationsExperimentType, "3"}};
const FeatureEntry::FeatureParam
    kContentPushNotificationsPromoRegistrationOnly[] = {
        {kContentPushNotificationsExperimentType, "5"}};
const FeatureEntry::FeatureParam
    kContentPushNotificationsProvisionalRegistrationOnly[] = {
        {kContentPushNotificationsExperimentType, "6"}};
const FeatureEntry::FeatureParam
    kContentPushNotificationsSetUpListRegistrationOnly[] = {
        {kContentPushNotificationsExperimentType, "7"}};

const FeatureEntry::FeatureVariation kContentPushNotificationsVariations[] = {
    {"Promo", kContentPushNotificationsEnabledPromo, nullptr},
    {"Set up list", kContentPushNotificationsEnabledSetupLists, nullptr},
    {"Provisional Notification", kContentPushNotificationsEnabledProvisional,
     nullptr},
    {"Promo Registeration Only", kContentPushNotificationsPromoRegistrationOnly,
     nullptr},
    {"Provisional Notification Registeration Only",
     kContentPushNotificationsProvisionalRegistrationOnly, nullptr},
    {"Set up list Registeration Only",
     kContentPushNotificationsSetUpListRegistrationOnly, nullptr}};

const FeatureEntry::FeatureParam kEnableDefaultModel[] = {
    {segmentation_platform::kDefaultModelEnabledParam, "true"}};

const FeatureEntry::FeatureVariation
    kSegmentationPlatformIosModuleRankerVariations[]{
        {"Enabled With Default Model Parameter (Must Set this!)",
         kEnableDefaultModel, nullptr},
    };

const FeatureEntry::FeatureParam
    kIOSReactivationNotifications10SecondTrigger[] = {
        {kIOSReactivationNotificationsTriggerTimeParam, "10s"},
};
const FeatureEntry::FeatureParam
    kIOSReactivationNotifications30SecondTrigger[] = {
        {kIOSReactivationNotificationsTriggerTimeParam, "30s"},
};
const FeatureEntry::FeatureVariation kIOSReactivationNotificationsVariations[] =
    {
        {"(10s trigger)", kIOSReactivationNotifications10SecondTrigger,
         nullptr},
        {"(30s trigger)", kIOSReactivationNotifications30SecondTrigger,
         nullptr},
};

#if BUILDFLAG(IOS_BACKGROUND_MODE_ENABLED)
// Feed Background Refresh Feature Params.
const FeatureEntry::FeatureParam kOneHourIntervalOneHourMaxAgeOnce[] = {
    {kEnableServerDrivenBackgroundRefreshSchedule, "false"},
    {kEnableRecurringBackgroundRefreshSchedule, "false"},
    {kMaxCacheAgeInSeconds, /*60*60*/ "3600"},
    {kBackgroundRefreshIntervalInSeconds, /* 60*60= */ "3600"}};
const FeatureEntry::FeatureParam kFourHourIntervalSixHourMaxAgeOnce[] = {
    {kEnableServerDrivenBackgroundRefreshSchedule, "false"},
    {kEnableRecurringBackgroundRefreshSchedule, "false"},
    {kMaxCacheAgeInSeconds, /*6*60*60*/ "21600"},
    {kBackgroundRefreshIntervalInSeconds, /* 4*60*60= */ "14400"}};
const FeatureEntry::FeatureParam kOneHourIntervalOneHourMaxAgeRecurring[] = {
    {kEnableServerDrivenBackgroundRefreshSchedule, "false"},
    {kEnableRecurringBackgroundRefreshSchedule, "true"},
    {kMaxCacheAgeInSeconds, /*60*60*/ "3600"},
    {kBackgroundRefreshIntervalInSeconds, /* 60*60= */ "3600"}};
const FeatureEntry::FeatureParam kFourHourIntervalSixHourMaxAgeRecurring[] = {
    {kEnableServerDrivenBackgroundRefreshSchedule, "false"},
    {kEnableRecurringBackgroundRefreshSchedule, "true"},
    {kMaxCacheAgeInSeconds, /*6*60*60*/ "21600"},
    {kBackgroundRefreshIntervalInSeconds, /* 4*60*60= */ "14400"}};
const FeatureEntry::FeatureParam kServerDrivenOneHourMaxAgeOnce[] = {
    {kEnableServerDrivenBackgroundRefreshSchedule, "true"},
    {kEnableRecurringBackgroundRefreshSchedule, "false"},
    {kMaxCacheAgeInSeconds, /*60*60*/ "3600"},
    {kBackgroundRefreshIntervalInSeconds, "0"}};
const FeatureEntry::FeatureParam kServerDrivenOneHourMaxAgeRecurring[] = {
    {kEnableServerDrivenBackgroundRefreshSchedule, "true"},
    {kEnableRecurringBackgroundRefreshSchedule, "true"},
    {kMaxCacheAgeInSeconds, /*60*60*/ "3600"},
    {kBackgroundRefreshIntervalInSeconds, "0"}};
const FeatureEntry::FeatureParam kServerDrivenSixHourMaxAgeOnce[] = {
    {kEnableServerDrivenBackgroundRefreshSchedule, "true"},
    {kEnableRecurringBackgroundRefreshSchedule, "false"},
    {kMaxCacheAgeInSeconds, /*6*60*60*/ "21600"},
    {kBackgroundRefreshIntervalInSeconds, "0"}};
const FeatureEntry::FeatureParam kServerDrivenSixHourMaxAgeRecurring[] = {
    {kEnableServerDrivenBackgroundRefreshSchedule, "true"},
    {kEnableRecurringBackgroundRefreshSchedule, "true"},
    {kMaxCacheAgeInSeconds, /*6*60*60*/ "21600"},
    {kBackgroundRefreshIntervalInSeconds, "0"}};

// Feed Background Refresh Feature Variations.
const FeatureEntry::FeatureVariation kFeedBackgroundRefreshVariations[] = {
    {"1hr Interval 1hr Max Age Once", kOneHourIntervalOneHourMaxAgeOnce,
     nullptr},
    {"4hr Interval 6hr Max Age Once", kFourHourIntervalSixHourMaxAgeOnce,
     nullptr},
    {"1hr Interval 1hr Max Age Recurring",
     kOneHourIntervalOneHourMaxAgeRecurring, nullptr},
    {"4hr Interval 6hr Max Age Recurring",
     kFourHourIntervalSixHourMaxAgeRecurring, nullptr},
    {"Server Driven 1hr Max Age Once", kServerDrivenOneHourMaxAgeOnce, nullptr},
    {"Server Driven 1hr Max Age Recurring", kServerDrivenOneHourMaxAgeRecurring,
     nullptr},
    {"Server Driven 6hr Max Age Once", kServerDrivenSixHourMaxAgeOnce, nullptr},
    {"Server Driven 6hr Max Age Recurring", kServerDrivenSixHourMaxAgeRecurring,
     nullptr},
};
#endif  // BUILDFLAG(IOS_BACKGROUND_MODE_ENABLED)

// Download List UI feature flag parameters.
// IMPORTANT: These values must match DownloadListUIType enum in features.h
const FeatureEntry::FeatureParam kDownloadListDefaultUIParam[] = {
    {kDownloadListUITypeParam, "0"}};
const FeatureEntry::FeatureParam kDownloadListCustomUIParam[] = {
    {kDownloadListUITypeParam, "1"}};
const FeatureEntry::FeatureVariation kDownloadListVariations[] = {
    {"Default UI", kDownloadListDefaultUIParam, nullptr},
    {"Custom UI", kDownloadListCustomUIParam, nullptr},
};

// Default browser promo refresh feature flag parameters.
const FeatureEntry::FeatureParam kDefaultBrowserPictureInPictureArm1[] = {
    {kDefaultBrowserPictureInPictureParam,
     kDefaultBrowserPictureInPictureParamEnabled}};
const FeatureEntry::FeatureParam kDefaultBrowserPictureInPictureArm2[] = {
    {kDefaultBrowserPictureInPictureParam,
     kDefaultBrowserPictureInPictureParamDisabledDefaultApps}};
const FeatureEntry::FeatureParam kDefaultBrowserPictureInPictureArm3[] = {
    {kDefaultBrowserPictureInPictureParam,
     kDefaultBrowserPictureInPictureParamEnabledDefaultApps}};
const FeatureEntry::FeatureVariation
    kDefaultBrowserPictureInPictureVariations[] = {
        {"Picture-in-picture instructions.",
         kDefaultBrowserPictureInPictureArm1, nullptr},
        {"No picture in picture, default apps destination.",
         kDefaultBrowserPictureInPictureArm2, nullptr},
        {"Picture-in-picture instructions, default apps destination.",
         kDefaultBrowserPictureInPictureArm3, nullptr},
};

const FeatureEntry::FeatureParam kIOSDockingPromoV2Header1[] = {
    {kIOSDockingPromoV2VariationParam, kIOSDockingPromoV2VariationHeader1}};

const FeatureEntry::FeatureParam kIOSDockingPromoV2Header2[] = {
    {kIOSDockingPromoV2VariationParam, kIOSDockingPromoV2VariationHeader2}};

const FeatureEntry::FeatureParam kIOSDockingPromoV2Header3[] = {
    {kIOSDockingPromoV2VariationParam, kIOSDockingPromoV2VariationHeader3}};

const FeatureEntry::FeatureVariation kIOSDockingPromoV2Variations[] = {
    {"Display Header #1", kIOSDockingPromoV2Header1, nullptr},
    {"Display Header #2", kIOSDockingPromoV2Header2, nullptr},
    {"Display Header #3 without Subheader", kIOSDockingPromoV2Header3,
     nullptr}};

const FeatureEntry::FeatureParam kIOSDockingPromoDisplayedAfterFRE[] = {
    {kIOSDockingPromoExperimentType, "0"}};
const FeatureEntry::FeatureParam kIOSDockingPromoDisplayedAtAppLaunch[] = {
    {kIOSDockingPromoExperimentType, "1"}};
const FeatureEntry::FeatureParam kIOSDockingPromoDisplayedDuringFRE[] = {
    {kIOSDockingPromoExperimentType, "2"}};

const FeatureEntry::FeatureVariation kIOSDockingPromoVariations[] = {
    {"Display promo after FRE", kIOSDockingPromoDisplayedAfterFRE, nullptr},
    {"Display promo at app launch", kIOSDockingPromoDisplayedAtAppLaunch,
     nullptr},
    {"Display promo during FRE", kIOSDockingPromoDisplayedDuringFRE, nullptr}};

// Uses int values from Lens filters ablation mode enum.
const FeatureEntry::FeatureParam kLensFiltersAblationModeDisabled[] = {
    {kLensFiltersAblationMode, "0"}};
const FeatureEntry::FeatureParam kLensFiltersAblationModePostCapture[] = {
    {kLensFiltersAblationMode, "1"}};
const FeatureEntry::FeatureParam kLensFiltersAblationModeLVF[] = {
    {kLensFiltersAblationMode, "2"}};
const FeatureEntry::FeatureParam kLensFiltersAblationModeAlways[] = {
    {kLensFiltersAblationMode, "3"}};

const FeatureEntry::FeatureVariation kLensFiltersAblationModeVariations[] = {
    {"(Disabled)", kLensFiltersAblationModeDisabled, nullptr},
    {"(Post Capture)", kLensFiltersAblationModePostCapture, nullptr},
    {"(LVF)", kLensFiltersAblationModeLVF, nullptr},
    {"(Always)", kLensFiltersAblationModeAlways, nullptr}};

// Uses int values from Lens translate toggle mode enum.
const FeatureEntry::FeatureParam kLensTranslateToggleModeDisabled[] = {
    {kLensTranslateToggleMode, "0"}};
const FeatureEntry::FeatureParam kLensTranslateToggleModePostCapture[] = {
    {kLensTranslateToggleMode, "1"}};
const FeatureEntry::FeatureParam kLensTranslateToggleModeLVF[] = {
    {kLensTranslateToggleMode, "2"}};
const FeatureEntry::FeatureParam kLensTranslateToggleModeAlways[] = {
    {kLensTranslateToggleMode, "3"}};

const FeatureEntry::FeatureVariation kLensTranslateToggleModeVariations[] = {
    {"(Disabled)", kLensTranslateToggleModeDisabled, nullptr},
    {"(Post Capture)", kLensTranslateToggleModePostCapture, nullptr},
    {"(LVF)", kLensTranslateToggleModeLVF, nullptr},
    {"(Always)", kLensTranslateToggleModeAlways, nullptr}};

const FeatureEntry::FeatureParam
    kMlUrlPiecewiseMappedSearchBlendingAdjustedBy0[] = {
        {"MlUrlPiecewiseMappedSearchBlending", "true"},
        {"MlUrlPiecewiseMappedSearchBlending_BreakPoints",
         "0,550;0.018,1300;0.14,1398;1,1422"},
        {"MlUrlPiecewiseMappedSearchBlending_GroupingThreshold", "1400"},
        {"MlUrlPiecewiseMappedSearchBlending_RelevanceBias", "0"}};
const FeatureEntry::FeatureParam
    kMlUrlPiecewiseMappedSearchBlendingDemotedBy50[] = {
        {"MlUrlPiecewiseMappedSearchBlending", "true"},
        {"MlUrlPiecewiseMappedSearchBlending_BreakPoints",
         "0,550;0.018,1250;0.14,1348;1,1422"},
        {"MlUrlPiecewiseMappedSearchBlending_GroupingThreshold", "1350"},
        {"MlUrlPiecewiseMappedSearchBlending_RelevanceBias", "0"}};
const FeatureEntry::FeatureParam
    kMlUrlPiecewiseMappedSearchBlendingPromotedBy50[] = {
        {"MlUrlPiecewiseMappedSearchBlending", "true"},
        {"MlUrlPiecewiseMappedSearchBlending_BreakPoints",
         "0,550;0.018,1350;0.14,1448;1,1472"},
        {"MlUrlPiecewiseMappedSearchBlending_GroupingThreshold", "1450"},
        {"MlUrlPiecewiseMappedSearchBlending_RelevanceBias", "0"}};
const FeatureEntry::FeatureParam
    kMlUrlPiecewiseMappedSearchBlendingPromotedBy100[] = {
        {"MlUrlPiecewiseMappedSearchBlending", "true"},
        {"MlUrlPiecewiseMappedSearchBlending_BreakPoints",
         "0,550;0.018,1400;0.14,1498;1,1522"},
        {"MlUrlPiecewiseMappedSearchBlending_GroupingThreshold", "1500"},
        {"MlUrlPiecewiseMappedSearchBlending_RelevanceBias", "0"}};
const FeatureEntry::FeatureParam
    kMlUrlPiecewiseMappedSearchBlendingMobileMapping[] = {
        {"MlUrlPiecewiseMappedSearchBlending", "true"},
        {"MlUrlPiecewiseMappedSearchBlending_BreakPoints",
         "0,590;0.006,790;0.082,1290;0.443,1360;0.464,1400;0.987,1425;1,1530"},
        {"MlUrlPiecewiseMappedSearchBlending_GroupingThreshold", "1340"},
        {"MlUrlPiecewiseMappedSearchBlending_RelevanceBias", "0"}};

const FeatureEntry::FeatureVariation
    kMlUrlPiecewiseMappedSearchBlendingVariations[] = {
        {"adjusted by 0", kMlUrlPiecewiseMappedSearchBlendingAdjustedBy0,
         nullptr},
        {"demoted by 50", kMlUrlPiecewiseMappedSearchBlendingDemotedBy50,
         nullptr},
        {"promoted by 50", kMlUrlPiecewiseMappedSearchBlendingPromotedBy50,
         nullptr},
        {"promoted by 100", kMlUrlPiecewiseMappedSearchBlendingPromotedBy100,
         nullptr},
        {"mobile mapping", kMlUrlPiecewiseMappedSearchBlendingMobileMapping,
         nullptr},
};

const FeatureEntry::FeatureParam kOmniboxMiaZpsEnabledWithHistoryAblation[] = {
    {OmniboxFieldTrial::kSuppressPsuggestBackfillWithMIAParam, "true"}};
const FeatureEntry::FeatureVariation kOmniboxMiaZpsVariations[] = {
    {"with History Ablation", kOmniboxMiaZpsEnabledWithHistoryAblation,
     nullptr}};

const FeatureEntry::FeatureParam kOmniboxMlUrlScoringEnabledWithFixes[] = {
    {"enable_scoring_signals_annotators_for_ml_scoring", "true"},
    {"MlUrlScoringShortcutDocumentSignals", "true"},
};
const FeatureEntry::FeatureParam kOmniboxMlUrlScoringUnlimitedNumCandidates[] =
    {
        {"MlUrlScoringUnlimitedNumCandidates", "true"},
        {"enable_scoring_signals_annotators_for_ml_scoring", "true"},
        {"MlUrlScoringShortcutDocumentSignals", "true"},
};
// Sets Bookmark(1), History Quick(4), History URL(8), Shortcuts(64),
// Document(512), and History Fuzzy(65536) providers max matches to 10.
const FeatureEntry::FeatureParam kOmniboxMlUrlScoringMaxMatchesByProvider10[] =
    {
        {"MlUrlScoringMaxMatchesByProvider",
         "1:10,4:10,8:10,64:10,512:10,65536:10"},
        {"enable_scoring_signals_annotators_for_ml_scoring", "true"},
        {"MlUrlScoringShortcutDocumentSignals", "true"},
};

const FeatureEntry::FeatureVariation kOmniboxMlUrlScoringVariations[] = {
    {"Enabled with fixes", kOmniboxMlUrlScoringEnabledWithFixes, nullptr},
    {"unlimited suggestion candidates",
     kOmniboxMlUrlScoringUnlimitedNumCandidates, nullptr},
    {"Increase provider max limit to 10",
     kOmniboxMlUrlScoringMaxMatchesByProvider10, nullptr},
};

const FeatureEntry::FeatureParam kMlUrlSearchBlendingStable[] = {
    {"MlUrlSearchBlending_StableSearchBlending", "true"},
    {"MlUrlSearchBlending_MappedSearchBlending", "false"},
};
const FeatureEntry::FeatureParam kMlUrlSearchBlendingMappedConservativeUrls[] =
    {
        {"MlUrlSearchBlending_StableSearchBlending", "false"},
        {"MlUrlSearchBlending_MappedSearchBlending", "true"},
        {"MlUrlSearchBlending_MappedSearchBlendingMin", "0"},
        {"MlUrlSearchBlending_MappedSearchBlendingMax", "2000"},
        {"MlUrlSearchBlending_MappedSearchBlendingGroupingThreshold", "1000"},
};
const FeatureEntry::FeatureParam kMlUrlSearchBlendingMappedModerateUrls[] = {
    {"MlUrlSearchBlending_StableSearchBlending", "false"},
    {"MlUrlSearchBlending_MappedSearchBlending", "true"},
};
const FeatureEntry::FeatureParam kMlUrlSearchBlendingMappedAggressiveUrls[] = {
    {"MlUrlSearchBlending_StableSearchBlending", "false"},
    {"MlUrlSearchBlending_MappedSearchBlending", "true"},
    {"MlUrlSearchBlending_MappedSearchBlendingMin", "1000"},
    {"MlUrlSearchBlending_MappedSearchBlendingMax", "4000"},
    {"MlUrlSearchBlending_MappedSearchBlendingGroupingThreshold", "1500"},
};

const FeatureEntry::FeatureVariation kMlUrlSearchBlendingVariations[] = {
    {"Stable", kMlUrlSearchBlendingStable, nullptr},
    {"Mapped conservative urls", kMlUrlSearchBlendingMappedConservativeUrls,
     nullptr},
    {"Mapped moderate urls", kMlUrlSearchBlendingMappedModerateUrls, nullptr},
    {"Mapped aggressive urls", kMlUrlSearchBlendingMappedAggressiveUrls,
     nullptr},
};

const FeatureEntry::FeatureVariation kUrlScoringModelVariations[] = {
    {"Small model", {}, "3379590"},
    {"Full model", {}, "3380197"},
};

const FeatureEntry::FeatureParam kPriceTrackingPromoForceShowArm[] = {
    {segmentation_platform::features::kEphemeralCardRankerForceShowCardParam,
     segmentation_platform::kPriceTrackingNotificationPromo},
};
const FeatureEntry::FeatureParam kPriceTrackingPromoForceHideArm[] = {
    {segmentation_platform::features::kEphemeralCardRankerForceHideCardParam,
     segmentation_platform::kPriceTrackingNotificationPromo},
};

// ShopCard variants
const FeatureEntry::FeatureParam kPriceDropOnTabArm[] = {
    {"ShopCardVariant", "arm_3"},
};
const FeatureEntry::FeatureParam kPriceTrackableProductOnTabArm[] = {
    {"ShopCardVariant", "arm_4"},
};
const FeatureEntry::FeatureParam kTabResumptionWithImpressionLimitsArm[] = {
    {"ShopCardVariant", "arm_5"},
};
const FeatureEntry::FeatureParam kPriceDropOnTabFront[] = {
    {"ShopCardVariant", "arm_3"},
    {"ShopCardPosition", "shop_card_front"},
};
const FeatureEntry::FeatureParam kPriceTrackableProductOnTabFront[] = {
    {"ShopCardVariant", "arm_4"},
    {"ShopCardPosition", "shop_card_front"},
};
const FeatureEntry::FeatureParam kTabResumptionWithImpressionLimitsFront[] = {
    {"ShopCardVariant", "arm_5"},
    {"ShopCardPosition", "shop_card_front"},
};
const FeatureEntry::FeatureParam kPriceDropOnTabDelayedDataAcquisition[] = {
    {"ShopCardVariant", "arm_6"},
};
const FeatureEntry::FeatureParam kPriceDropOnTabDelayedDataAcquisitionFront[] =
    {
        {"ShopCardVariant", "arm_6"},
        {"ShopCardPosition", "shop_card_front"},
};

// Address Bar Position
const FeatureEntry::FeatureParam kTipsAddressBarPositionForceShowArm[] = {
    {segmentation_platform::features::kEphemeralCardRankerForceShowCardParam,
     segmentation_platform::kAddressBarPositionEphemeralModule},
};
const FeatureEntry::FeatureParam kTipsAddressBarPositionForceHideArm[] = {
    {segmentation_platform::features::kEphemeralCardRankerForceHideCardParam,
     segmentation_platform::kAddressBarPositionEphemeralModule},
};

// Autofill Passwords
const FeatureEntry::FeatureParam kTipsAutofillPasswordsForceShowArm[] = {
    {segmentation_platform::features::kEphemeralCardRankerForceShowCardParam,
     segmentation_platform::kAutofillPasswordsEphemeralModule},
};
const FeatureEntry::FeatureParam kTipsAutofillPasswordsForceHideArm[] = {
    {segmentation_platform::features::kEphemeralCardRankerForceHideCardParam,
     segmentation_platform::kAutofillPasswordsEphemeralModule},
};

// Enhanced Safe Browsing
const FeatureEntry::FeatureParam kTipsEnhancedSafeBrowsingForceShowArm[] = {
    {segmentation_platform::features::kEphemeralCardRankerForceShowCardParam,
     segmentation_platform::kEnhancedSafeBrowsingEphemeralModule},
};
const FeatureEntry::FeatureParam kTipsEnhancedSafeBrowsingForceHideArm[] = {
    {segmentation_platform::features::kEphemeralCardRankerForceHideCardParam,
     segmentation_platform::kEnhancedSafeBrowsingEphemeralModule},
};

// Lens Search
const FeatureEntry::FeatureParam kTipsLensSearchForceShowArm[] = {
    {segmentation_platform::features::kEphemeralCardRankerForceShowCardParam,
     segmentation_platform::kLensEphemeralModuleSearchVariation},
};
const FeatureEntry::FeatureParam kTipsLensSearchForceHideArm[] = {
    {segmentation_platform::features::kEphemeralCardRankerForceHideCardParam,
     segmentation_platform::kLensEphemeralModuleSearchVariation},
};

// Lens Shop
const FeatureEntry::FeatureParam kTipsLensShopForceShowArm[] = {
    {segmentation_platform::features::kEphemeralCardRankerForceShowCardParam,
     segmentation_platform::kLensEphemeralModuleShopVariation},
};
const FeatureEntry::FeatureParam kTipsLensShopForceHideArm[] = {
    {segmentation_platform::features::kEphemeralCardRankerForceHideCardParam,
     segmentation_platform::kLensEphemeralModuleShopVariation},
};

// Lens Translate
const FeatureEntry::FeatureParam kTipsLensTranslateForceShowArm[] = {
    {segmentation_platform::features::kEphemeralCardRankerForceShowCardParam,
     segmentation_platform::kLensEphemeralModuleTranslateVariation},
};
const FeatureEntry::FeatureParam kTipsLensTranslateForceHideArm[] = {
    {segmentation_platform::features::kEphemeralCardRankerForceHideCardParam,
     segmentation_platform::kLensEphemeralModuleTranslateVariation},
};

// Save Passwords
const FeatureEntry::FeatureParam kTipsSavePasswordsForceShowArm[] = {
    {segmentation_platform::features::kEphemeralCardRankerForceShowCardParam,
     segmentation_platform::kSavePasswordsEphemeralModule},
};
const FeatureEntry::FeatureParam kTipsSavePasswordsForceHideArm[] = {
    {segmentation_platform::features::kEphemeralCardRankerForceHideCardParam,
     segmentation_platform::kSavePasswordsEphemeralModule},
};

// Send Tab Promo
const FeatureEntry::FeatureParam kSendTabPromoForceShowArm[] = {
    {segmentation_platform::features::kEphemeralCardRankerForceShowCardParam,
     segmentation_platform::kSendTabNotificationPromo},
};
const FeatureEntry::FeatureParam kSendTabPromoForceHideArm[] = {
    {segmentation_platform::features::kEphemeralCardRankerForceHideCardParam,
     segmentation_platform::kSendTabNotificationPromo},
};

// App Bundle Promo
const FeatureEntry::FeatureParam kAppBundlePromoForceShowArm[] = {
    {segmentation_platform::features::kEphemeralCardRankerForceShowCardParam,
     segmentation_platform::kAppBundlePromoEphemeralModule},
};
const FeatureEntry::FeatureParam kAppBundlePromoForceHideArm[] = {
    {segmentation_platform::features::kEphemeralCardRankerForceHideCardParam,
     segmentation_platform::kAppBundlePromoEphemeralModule},
};

// Default Browser Promo
const FeatureEntry::FeatureParam kDefaultBrowserPromoForceShowArm[] = {
    {segmentation_platform::features::kEphemeralCardRankerForceShowCardParam,
     segmentation_platform::kDefaultBrowserPromoEphemeralModule},
};
const FeatureEntry::FeatureParam kDefaultBrowserPromoForceHideArm[] = {
    {segmentation_platform::features::kEphemeralCardRankerForceHideCardParam,
     segmentation_platform::kDefaultBrowserPromoEphemeralModule},
};

// ShopCard experiment arms
const FeatureEntry::FeatureVariation kShopCardOverrideOptions[] = {
    {"Card 3 Price Drop on Tab Resumption", kPriceDropOnTabArm, nullptr},
    {"Card 4 Price Trackable on Tab Resumption", kPriceTrackableProductOnTabArm,
     nullptr},
    {"Card 5 Tab Resumption with Impression Limits",
     kTabResumptionWithImpressionLimitsArm, nullptr},
    {"Card 3 Price Drop on Tab Resumption at front of magic stack",
     kPriceDropOnTabFront, nullptr},
    {"Card 4 Price Trackable on Tab Resumption at front of magic stack",
     kPriceTrackableProductOnTabFront, nullptr},
    {"Card 5 Tab Resumption with Impression Limits at front of magic stack",
     kTabResumptionWithImpressionLimitsFront, nullptr},
    {"Card 6 Price Drop on Tab Resumption with delayed data acquisition",
     kPriceDropOnTabDelayedDataAcquisition, nullptr},
    {"Card 6 Price Drop on Tab Resumption with delayed data acquisition at "
     "front of magic stack",
     kPriceDropOnTabDelayedDataAcquisitionFront, nullptr},
};

const FeatureEntry::FeatureVariation kEphemeralCardRankerCardOverrideOptions[] =
    {
        {"- Force Show Price Tracking Notification",
         kPriceTrackingPromoForceShowArm, nullptr},
        {"- Force Hide Price Tracking Notification",
         kPriceTrackingPromoForceHideArm, nullptr},

        // Address Bar Position
        {"- Force Show Address Bar Position Tip",
         kTipsAddressBarPositionForceShowArm, nullptr},
        {"- Force Hide Address Bar Position Tip",
         kTipsAddressBarPositionForceHideArm, nullptr},

        // Autofill Passwords
        {"- Force Show Autofill Passwords Tip",
         kTipsAutofillPasswordsForceShowArm, nullptr},
        {"- Force Hide Autofill Passwords Tip",
         kTipsAutofillPasswordsForceHideArm, nullptr},

        // Enhanced Safe Browsing
        {"- Force Show Enhanced Safe Browsing Tip",
         kTipsEnhancedSafeBrowsingForceShowArm, nullptr},
        {"- Force Hide Enhanced Safe Browsing Tip",
         kTipsEnhancedSafeBrowsingForceHideArm, nullptr},

        // Lens Search
        {"- Force Show Lens Search Tip", kTipsLensSearchForceShowArm, nullptr},
        {"- Force Hide Lens Search Tip", kTipsLensSearchForceHideArm, nullptr},

        // Lens Shop
        {"- Force Show Lens Shop Tip", kTipsLensShopForceShowArm, nullptr},
        {"- Force Hide Lens Shop Tip", kTipsLensShopForceHideArm, nullptr},

        // Lens Translate
        {"- Force Show Lens Translate Tip", kTipsLensTranslateForceShowArm,
         nullptr},
        {"- Force Hide Lens Translate Tip", kTipsLensTranslateForceHideArm,
         nullptr},

        // Save Passwords
        {"- Force Show Save Passwords Tip", kTipsSavePasswordsForceShowArm,
         nullptr},
        {"- Force Hide Save Passwords Tip", kTipsSavePasswordsForceHideArm,
         nullptr},

        // Send Tab Promo.
        {"- Force Show Send Tab Promo", kSendTabPromoForceShowArm, nullptr},
        {"- Force Hide Send Tab Promo", kSendTabPromoForceHideArm, nullptr},

        // App Bundle Promo.
        {"- Force Show App Bundle Promo", kAppBundlePromoForceShowArm, nullptr},
        {"- Force Hide App Bundle Promo", kAppBundlePromoForceHideArm, nullptr},

        // Default Browser Promo.
        {"- Force Show Default Browser Promo", kDefaultBrowserPromoForceShowArm,
         nullptr},
        {"- Force Hide Default Browser Promo", kDefaultBrowserPromoForceHideArm,
         nullptr},
};

const FeatureEntry::FeatureParam
    kSendTabIOSPushNotificationsWithMagicStackCard[] = {
        {send_tab_to_self::kSendTabIOSPushNotificationsWithMagicStackCardParam,
         "true"}};
const FeatureEntry::FeatureParam kSendTabIOSPushNotificationsWithURLImage[] = {
    {send_tab_to_self::kSendTabIOSPushNotificationsURLImageParam, "true"}};
const FeatureEntry::FeatureParam
    kSendTabIOSPushNotificationsWithTabReminders[] = {
        {send_tab_to_self::kSendTabIOSPushNotificationsWithTabRemindersParam,
         "true"}};

const FeatureEntry::FeatureVariation kSendTabIOSPushNotificationsVariations[] =
    {
        {"With Magic Stack Card",
         kSendTabIOSPushNotificationsWithMagicStackCard, nullptr},
        {"With URL Image", kSendTabIOSPushNotificationsWithURLImage, nullptr},
        {"With Tab Reminders", kSendTabIOSPushNotificationsWithTabReminders,
         nullptr},
};

// Soft Lock
const FeatureEntry::FeatureParam kIOSSoftLockNoDelay[] = {
    {kIOSSoftLockBackgroundThresholdParam, "0m"},
};

const FeatureEntry::FeatureVariation kIOSSoftLockVariations[] = {
    {" - No delay", kIOSSoftLockNoDelay, nullptr}};

constexpr flags_ui::FeatureEntry::FeatureParam
    kAutofillThrottleDocFormScanShortPeriodParam[] = {{"period-ms", "250"}};
constexpr flags_ui::FeatureEntry::FeatureParam
    kAutofillThrottleDocFormScanMediumPeriodParam[] = {{"period-ms", "500"}};
constexpr flags_ui::FeatureEntry::FeatureParam
    kAutofillThrottleDocFormScanLongPeriodParam[] = {{"period-ms", "1000"}};
constexpr flags_ui::FeatureEntry::FeatureVariation
    kAutofillThrottleDocFormScanVariations[] = {
        {"Short period", kAutofillThrottleDocFormScanShortPeriodParam, nullptr},
        {"Medium period", kAutofillThrottleDocFormScanMediumPeriodParam,
         nullptr},
        {"Long period", kAutofillThrottleDocFormScanLongPeriodParam, nullptr}};

constexpr flags_ui::FeatureEntry::FeatureParam
    kAutofillThrottleFilteredDocFormScanShortPeriodParam[] = {
        {"period-ms", "100"}};
constexpr flags_ui::FeatureEntry::FeatureParam
    kAutofillThrottleFilteredDocFormScanMediumPeriodParam[] = {
        {"period-ms", "250"}};
constexpr flags_ui::FeatureEntry::FeatureParam
    kAutofillThrottleFilteredDocFormScanLongPeriodParam[] = {
        {"period-ms", "500"}};
constexpr flags_ui::FeatureEntry::FeatureVariation
    kAutofillThrottleFilteredDocFormScanVariations[] = {
        {"Short period", kAutofillThrottleFilteredDocFormScanShortPeriodParam,
         nullptr},
        {"Medium period", kAutofillThrottleFilteredDocFormScanMediumPeriodParam,
         nullptr},
        {"Long period", kAutofillThrottleFilteredDocFormScanLongPeriodParam,
         nullptr}};

const FeatureEntry::FeatureParam kUpdatedFirstRunSequenceArm1[] = {
    {first_run::kUpdatedFirstRunSequenceParam, "1"}};
const FeatureEntry::FeatureParam kUpdatedFirstRunSequenceArm2[] = {
    {first_run::kUpdatedFirstRunSequenceParam, "2"}};
const FeatureEntry::FeatureParam kUpdatedFirstRunSequenceArm3[] = {
    {first_run::kUpdatedFirstRunSequenceParam, "3"}};

const FeatureEntry::FeatureVariation kUpdatedFirstRunSequenceVariations[] = {
    {" - Default browser promo first", kUpdatedFirstRunSequenceArm1, nullptr},
    {" - Remove sign in & sync conditionally", kUpdatedFirstRunSequenceArm2,
     nullptr},
    {" - DB promo first and remove sign in & sync",
     kUpdatedFirstRunSequenceArm3, nullptr}};

const FeatureEntry::FeatureParam kSlowFullscreenTransitionSpeed[] = {
    {kFullscreenTransitionSpeedParam, "0"}};
const FeatureEntry::FeatureParam kDefaultFullscreenTransitionSpeed[] = {
    {kFullscreenTransitionSpeedParam, "1"}};
const FeatureEntry::FeatureParam kFastFullscreenTransitionSpeed[] = {
    {kFullscreenTransitionSpeedParam, "2"}};

const FeatureEntry::FeatureVariation kFullscreenTransitionVariations[] = {
    {"Slow speed", kSlowFullscreenTransitionSpeed, nullptr},
    {"Default speed", kDefaultFullscreenTransitionSpeed, nullptr},
    {"Fast speed", kFastFullscreenTransitionSpeed, nullptr}};

const FeatureEntry::FeatureParam kFullscreenScrollThreshold1[] = {
    {web::features::kFullscreenScrollThresholdAmount, "1"}};
const FeatureEntry::FeatureParam kFullscreenScrollThreshold5[] = {
    {web::features::kFullscreenScrollThresholdAmount, "5"}};
const FeatureEntry::FeatureParam kFullscreenScrollThreshold10[] = {
    {web::features::kFullscreenScrollThresholdAmount, "10"}};
const FeatureEntry::FeatureParam kFullscreenScrollThreshold20[] = {
    {web::features::kFullscreenScrollThresholdAmount, "20"}};
const FeatureEntry::FeatureVariation kFullscreenScrollThresholdVariations[] = {
    {"1px", kFullscreenScrollThreshold1, nullptr},
    {"5px", kFullscreenScrollThreshold5, nullptr},
    {"10px", kFullscreenScrollThreshold10, nullptr},
    {"20px", kFullscreenScrollThreshold20, nullptr}};

const FeatureEntry::FeatureParam kAnimatedDBPInFREWithActionButtons[] = {
    {first_run::kAnimatedDefaultBrowserPromoInFREExperimentType, "0"}};
const FeatureEntry::FeatureParam kAnimatedDBPInFREWithShowMeHow[] = {
    {first_run::kAnimatedDefaultBrowserPromoInFREExperimentType, "1"}};
const FeatureEntry::FeatureParam kAnimatedDBPInFREWithInstructions[] = {
    {first_run::kAnimatedDefaultBrowserPromoInFREExperimentType, "2"}};

const FeatureEntry::FeatureVariation
    kAnimatedDefaultBrowserPromoInFREVariations[] = {
        {" - with Action Buttons", kAnimatedDBPInFREWithActionButtons, nullptr},
        {" - with Show Me How", kAnimatedDBPInFREWithShowMeHow, nullptr},
        {" - with Instructions", kAnimatedDBPInFREWithInstructions, nullptr}};

const FeatureEntry::FeatureParam kBestFeaturesScreenInFirstRunArm2[] = {
    {first_run::kBestFeaturesScreenInFirstRunParam, "2"}};
const FeatureEntry::FeatureParam kBestFeaturesScreenInFirstRunArm3[] = {
    {first_run::kBestFeaturesScreenInFirstRunParam, "3"}};
const FeatureEntry::FeatureParam kBestFeaturesScreenInFirstRunArm4[] = {
    {first_run::kBestFeaturesScreenInFirstRunParam, "4"}};
const FeatureEntry::FeatureParam kBestFeaturesScreenInFirstRunArm5[] = {
    {first_run::kBestFeaturesScreenInFirstRunParam, "5"}};
const FeatureEntry::FeatureParam kBestFeaturesScreenInFirstRunArm6[] = {
    {first_run::kBestFeaturesScreenInFirstRunParam, "6"}};

const FeatureEntry::FeatureVariation kBestFeaturesScreenInFirstRunVariations[] =
    {{" - Variant A: General screen, after DB promo",
      kUpdatedFirstRunSequenceArm1, nullptr},
     {" - Variant B: General screen, before DB promo",
      kBestFeaturesScreenInFirstRunArm2, nullptr},
     {" - Variant C: General screen with passwords item",
      kBestFeaturesScreenInFirstRunArm3, nullptr},
     {" - Variant D: Shopping users screen, variant C as fallback",
      kBestFeaturesScreenInFirstRunArm4, nullptr},
     {" - Variant E: Show screen to signed-in users only",
      kBestFeaturesScreenInFirstRunArm5, nullptr},
     {" - Variant F: Show address bar promo", kBestFeaturesScreenInFirstRunArm6,
      nullptr}};

const FeatureEntry::FeatureParam kIOSOneTapMiniMapRestrictionCrossValidate[] = {
    {kIOSOneTapMiniMapRestrictionCrossValidateParamName, "true"}};
const FeatureEntry::FeatureParam kIOSOneTapMiniMapRestrictionThreshold999[] = {
    {kIOSOneTapMiniMapRestrictionThreshholdParamName, "0.999"}};
const FeatureEntry::FeatureParam kIOSOneTapMiniMapRestrictionMinLength20[] = {
    {kIOSOneTapMiniMapRestrictionMinCharsParamName, "20"}};
const FeatureEntry::FeatureParam kIOSOneTapMiniMapRestrictionMaxSections6[] = {
    {kIOSOneTapMiniMapRestrictionMaxSectionsParamName, "6"}};
const FeatureEntry::FeatureParam kIOSOneTapMiniMapRestrictionLongWords4[] = {
    {kIOSOneTapMiniMapRestrictionLongestWordMinCharsParamName, "4"}};
const FeatureEntry::FeatureParam kIOSOneTapMiniMapRestrictionMinAlphaNum60[] = {
    {kIOSOneTapMiniMapRestrictionMinAlphanumProportionParamName, "0.6"}};

const FeatureEntry::FeatureVariation kIOSOneTapMiniMapRestrictionsVariations[] =
    {{"Revalidate with NSDataDetector",
      kIOSOneTapMiniMapRestrictionCrossValidate, nullptr},
     {"Confidence Level (0.999)", kIOSOneTapMiniMapRestrictionThreshold999,
      nullptr},
     {"Minimum address length (20 chars)",
      kIOSOneTapMiniMapRestrictionMinLength20, nullptr},
     {"Maximum sections (6)", kIOSOneTapMiniMapRestrictionMaxSections6,
      nullptr},
     {"Longest word length (4)", kIOSOneTapMiniMapRestrictionLongWords4,
      nullptr},
     {"Proportion of alnum chars (60%)",
      kIOSOneTapMiniMapRestrictionMinAlphaNum60, nullptr}};

const FeatureEntry::FeatureParam kFeedSwipeInProductHelpStaticInFirstRun[] = {
    {kFeedSwipeInProductHelpArmParam, "1"}};
const FeatureEntry::FeatureParam kFeedSwipeInProductHelpStaticInSecondRun[] = {
    {kFeedSwipeInProductHelpArmParam, "2"}};
const FeatureEntry::FeatureParam kFeedSwipeInProductHelpAnimated[] = {
    {kFeedSwipeInProductHelpArmParam, "3"}};

const FeatureEntry::FeatureVariation kFeedSwipeInProductHelpVariations[] = {
    {" - Static IPH after the FRE", kFeedSwipeInProductHelpStaticInFirstRun,
     nullptr},
    {"- Static IPH after the second run",
     kFeedSwipeInProductHelpStaticInSecondRun, nullptr},
    {"- Animated IPH", kFeedSwipeInProductHelpAnimated, nullptr}};

// LINT.IfChange(AutofillVcnEnrollStrikeExpiryTime)
const FeatureEntry::FeatureParam kAutofillVcnEnrollStrikeExpiryTime_120Days[] =
    {{"autofill_vcn_strike_expiry_time_days", "120"}};

const FeatureEntry::FeatureParam kAutofillVcnEnrollStrikeExpiryTime_60Days[] = {
    {"autofill_vcn_strike_expiry_time_days", "60"}};

const FeatureEntry::FeatureParam kAutofillVcnEnrollStrikeExpiryTime_30Days[] = {
    {"autofill_vcn_strike_expiry_time_days", "30"}};

const FeatureEntry::FeatureVariation
    kAutofillVcnEnrollStrikeExpiryTimeOptions[] = {
        {"120 days", kAutofillVcnEnrollStrikeExpiryTime_120Days, nullptr},
        {"60 days", kAutofillVcnEnrollStrikeExpiryTime_60Days, nullptr},
        {"30 days", kAutofillVcnEnrollStrikeExpiryTime_30Days, nullptr}};
// LINT.ThenChange(/chrome/browser/about_flags.cc:AutofillVcnEnrollStrikeExpiryTime)

const FeatureEntry::FeatureParam kWelcomeBackArm1[] = {
    {kWelcomeBackParam, "1"}};
const FeatureEntry::FeatureParam kWelcomeBackArm2[] = {
    {kWelcomeBackParam, "2"}};
const FeatureEntry::FeatureParam kWelcomeBackArm3[] = {
    {kWelcomeBackParam, "3"}};
const FeatureEntry::FeatureParam kWelcomeBackArm4[] = {
    {kWelcomeBackParam, "4"}};

const FeatureEntry::FeatureVariation kWelcomeBackVariations[] = {
    {" - Variant A: Basics with Locked Incognito", kWelcomeBackArm1, nullptr},
    {" - Variant B: Basics with Save & Autofill Passwords", kWelcomeBackArm2,
     nullptr},
    {" - Variant C: Productivity & Shopping", kWelcomeBackArm3, nullptr},
    {" - Variant D: Sign-in Benefits", kWelcomeBackArm4, nullptr},
};

const FeatureEntry::FeatureParam kBestOfAppFREArm1[] = {{"variant", "1"}};
const FeatureEntry::FeatureParam kBestOfAppFREArm2[] = {{"variant", "2"}};
const FeatureEntry::FeatureParam kBestOfAppFREArm4[] = {{"variant", "4"}};
const FeatureEntry::FeatureParam kBestOfAppFREArm4Upload[] = {
    {"variant", "4"},
    {"manual_upload_uma", "true"}};

const FeatureEntry::FeatureVariation kBestOfAppFREVariations[] = {
    {" - Variant A: Lens Interactive Promo", kBestOfAppFREArm1, nullptr},
    {" - Variant A: Lens Animated Promo", kBestOfAppFREArm2, nullptr},
    {" - Variant D: Guided Tour", kBestOfAppFREArm4, nullptr},
    {" - Variant D: Guided Tour with manual metric upload",
     kBestOfAppFREArm4Upload, nullptr},
};

const FeatureEntry::FeatureParam
    kInvalidateChoiceOnRestoreIsRetroactiveOption[] = {
        {"is_retroactive", "true"}};
const FeatureEntry::FeatureVariation
    kInvalidateSearchEngineChoiceOnRestoreVariations[] = {
        {"(retroactive)", kInvalidateChoiceOnRestoreIsRetroactiveOption,
         nullptr}};

const FeatureEntry::FeatureParam kSingleScreenForBWGPromoConsent[] = {
    {kBWGPromoConsentParams, "1"}};
const FeatureEntry::FeatureParam kDoubleScreenForBWGPromoConsent[] = {
    {kBWGPromoConsentParams, "2"}};
const FeatureEntry::FeatureParam kSkipBWGPromoConsent[] = {
    {kBWGPromoConsentParams, "3"}};
const FeatureEntry::FeatureParam kForceBWGFirstTimeRun[] = {
    {kBWGPromoConsentParams, "4"}};
const FeatureEntry::FeatureParam kSkipNewUserDelay[] = {
    {kBWGPromoConsentParams, "5"}};

const FeatureEntry::FeatureVariation kBWGPromoConsentVariations[] = {
    {"Single screen for BWG Promo Consent Flow",
     kSingleScreenForBWGPromoConsent, nullptr},
    {"Double screen for BWG Promo Consent Flow",
     kDoubleScreenForBWGPromoConsent, nullptr},
    {"Skip FRE", kSkipBWGPromoConsent, nullptr},
    {"Force FRE", kForceBWGFirstTimeRun, nullptr},
    {"Skip new user delay", kSkipNewUserDelay, nullptr}};

const FeatureEntry::FeatureParam kOmniboxMobileParityEnableFeedForGoogleOnly[] =
    {{OmniboxFieldTrial::kMobileParityEnableFeedForGoogleOnly.name, "true"}};
const FeatureEntry::FeatureVariation kOmniboxMobileParityVariations[] = {
    {"- feed only when searching with Google",
     kOmniboxMobileParityEnableFeedForGoogleOnly, nullptr}};

const FeatureEntry::FeatureParam kPageActionMenuDirectEntryPoint[] = {
    {kPageActionMenuDirectEntryPointParam, "true"},
};
const FeatureEntry::FeatureParam kPageActionMenuBWGSessionValidityDuration[] = {
    {kBWGSessionValidityDurationParam, "1"}};
const FeatureEntry::FeatureVariation kPageActionMenuVariations[] = {
    {"Direct Entry Point", kPageActionMenuDirectEntryPoint, nullptr},
    {"1 min session validity duration",
     kPageActionMenuBWGSessionValidityDuration, nullptr},
};

const FeatureEntry::FeatureParam
    kProactiveSuggestionsFrameworkPopupBlockerParam[] = {
        {kProactiveSuggestionsFrameworkPopupBlocker, "true"}};
const FeatureEntry::FeatureVariation
    kProactiveSuggestionsFrameworkVariations[] = {
        {"Popup Blocker", kProactiveSuggestionsFrameworkPopupBlockerParam,
         nullptr}};

const FeatureEntry::FeatureParam kAskGeminiChipIgnoreCriteriaVariation[] = {
    {kAskGeminiChipIgnoreCriteria, "true"},
};
const FeatureEntry::FeatureParam kAskGeminiChipPrepopulateFloatyVariation[] = {
    {kAskGeminiChipPrepopulateFloaty, "true"},
};
const FeatureEntry::FeatureParam
    kAskGeminiChipPrepopulateAndIgnoreCriteriaVariation[] = {
        {kAskGeminiChipPrepopulateAndIgnoreCriteria, "true"},
};
const FeatureEntry::FeatureParam
    kAskGeminiChipAllowNonconsentedUsersVariation[] = {
        {kAskGeminiChipAllowNonconsentedUsers, "true"},
};
const FeatureEntry::FeatureVariation kAskGeminiChipVariations[] = {
    {"Ignore FET and Time Criteria", kAskGeminiChipIgnoreCriteriaVariation,
     nullptr},
    {"Prepopulate Floaty", kAskGeminiChipPrepopulateFloatyVariation, nullptr},
    {"Prepopulate Floaty and Ignore Criteria",
     kAskGeminiChipPrepopulateAndIgnoreCriteriaVariation, nullptr},
    {"Allow non-consented users", kAskGeminiChipAllowNonconsentedUsersVariation,
     nullptr},
};

// LINT.IfChange(DataSharingVersioningChoices)
const FeatureEntry::Choice kDataSharingVersioningStateChoices[] = {
    {"Default", "", ""},
    {flag_descriptions::kDataSharingSharedDataTypesEnabled,
     switches::kEnableFeatures, "SharedDataTypesKillSwitch"},
    {flag_descriptions::kDataSharingSharedDataTypesEnabledWithUi,
     switches::kEnableFeatures,
     "SharedDataTypesKillSwitch,DataSharingEnableUpdateChromeUI"},
    {"Disabled", switches::kDisableFeatures,
     "SharedDataTypesKillSwitch, DataSharingEnableUpdateChromeUI"},
};
// LINT.ThenChange(//chrome/browser/about_flags.cc:DataSharingVersioningChoices)

const FeatureEntry::FeatureParam kComposeboxDevToolsForceFailure[] = {
    {kForceUploadFailureParam, "true"}};
const FeatureEntry::FeatureParam kComposeboxDevToolsSlowLoad[] = {
    {kImageLoadDelayMsParam, "1000"}};
const FeatureEntry::FeatureParam kComposeboxDevToolsSlowUpload[] = {
    {kUploadDelayMsParam, "3000"}};

const FeatureEntry::FeatureVariation kComposeboxDevToolsVariations[] = {
    {"Force Failure", kComposeboxDevToolsForceFailure, nullptr},
    {"Slow Load (1s)", kComposeboxDevToolsSlowLoad, nullptr},
    {"Slow Upload (3s)", kComposeboxDevToolsSlowUpload, nullptr}};

const FeatureEntry::FeatureParam kMobilePromoOnDesktopLens[] = {
    {kMobilePromoOnDesktopPromoTypeParam, "1"},
    {kMobilePromoOnDesktopNotificationParam, "false"}};
const FeatureEntry::FeatureParam kMobilePromoOnDesktopLensNotification[] = {
    {kMobilePromoOnDesktopPromoTypeParam, "1"},
    {kMobilePromoOnDesktopNotificationParam, "true"}};
const FeatureEntry::FeatureParam kMobilePromoOnDesktopESB[] = {
    {kMobilePromoOnDesktopPromoTypeParam, "2"},
    {kMobilePromoOnDesktopNotificationParam, "false"}};
const FeatureEntry::FeatureParam kMobilePromoOnDesktopESBNotification[] = {
    {kMobilePromoOnDesktopPromoTypeParam, "2"},
    {kMobilePromoOnDesktopNotificationParam, "true"}};
const FeatureEntry::FeatureParam kMobilePromoOnDesktopAutofill[] = {
    {kMobilePromoOnDesktopPromoTypeParam, "3"},
    {kMobilePromoOnDesktopNotificationParam, "false"}};
const FeatureEntry::FeatureParam kMobilePromoOnDesktopAutofillNotification[] = {
    {kMobilePromoOnDesktopPromoTypeParam, "3"},
    {kMobilePromoOnDesktopNotificationParam, "true"}};
const FeatureEntry::FeatureParam kMobilePromoOnDesktopTabGroups[] = {
    {kMobilePromoOnDesktopPromoTypeParam, "4"},
    {kMobilePromoOnDesktopNotificationParam, "false"}};
const FeatureEntry::FeatureParam kMobilePromoOnDesktopTabGroupsNotification[] =
    {{kMobilePromoOnDesktopPromoTypeParam, "4"},
     {kMobilePromoOnDesktopNotificationParam, "true"}};
const FeatureEntry::FeatureParam kMobilePromoOnDesktopPriceTracking[] = {
    {kMobilePromoOnDesktopPromoTypeParam, "5"},
    {kMobilePromoOnDesktopNotificationParam, "false"}};
const FeatureEntry::FeatureParam
    kMobilePromoOnDesktopPriceTrackingNotification[] = {
        {kMobilePromoOnDesktopPromoTypeParam, "5"},
        {kMobilePromoOnDesktopNotificationParam, "true"}};

const FeatureEntry::FeatureVariation kMobilePromoOnDesktopVariations[] = {
    {" - Lens Promo", kMobilePromoOnDesktopLens, nullptr},
    {" - Lens Promo with push notification",
     kMobilePromoOnDesktopLensNotification, nullptr},
    {" - ESB", kMobilePromoOnDesktopESB, nullptr},
    {" - ESB with push notification", kMobilePromoOnDesktopESBNotification,
     nullptr},
    {" - PW Autofill", kMobilePromoOnDesktopAutofill, nullptr},
    {" - PW Autofill with push notification",
     kMobilePromoOnDesktopAutofillNotification, nullptr},
    {" - Tab Groups", kMobilePromoOnDesktopTabGroups, nullptr},
    {" - Tab Groups with push notification",
     kMobilePromoOnDesktopTabGroupsNotification, nullptr},
    {" - Price Tracking", kMobilePromoOnDesktopPriceTracking, nullptr},
    {" - Price Tracking with push notification",
     kMobilePromoOnDesktopPriceTrackingNotification, nullptr},
};

const FeatureEntry::FeatureParam kTaiyakiChoiceScreenSurfaceParamAll[] = {
    {"choice_screen_surface", "all"}};
const FeatureEntry::FeatureParam kTaiyakiChoiceScreenSurfaceParamFREOnly[] = {
    {"choice_screen_surface", "fre_only"}};

const FeatureEntry::FeatureVariation kTaiyakiChoiceScreenSurfaceVariations[] = {
    {"all", kTaiyakiChoiceScreenSurfaceParamAll, nullptr},
    {"FRE only", kTaiyakiChoiceScreenSurfaceParamFREOnly, nullptr},
};

// Tips Notifications alternative strings.
const FeatureEntry::FeatureParam kTipsNotificationsAlternative1[] = {
    {kTipsNotificationsAlternativeStringVersion, "1"}};
const FeatureEntry::FeatureParam kTipsNotificationsAlternative2[] = {
    {kTipsNotificationsAlternativeStringVersion, "2"}};
const FeatureEntry::FeatureParam kTipsNotificationsAlternative3[] = {
    {kTipsNotificationsAlternativeStringVersion, "3"}};

const FeatureEntry::FeatureVariation
    kTipsNotificationsAlternativeStringVariation[] = {
        {" - 1", kTipsNotificationsAlternative1, nullptr},
        {" - 2", kTipsNotificationsAlternative2, nullptr},
        {" - 3", kTipsNotificationsAlternative3, nullptr}};

const FeatureEntry::FeatureParam kZeroStateSuggestionsPlacementAIHubParam[] = {
    {kZeroStateSuggestionsPlacementAIHub, "true"}};
const FeatureEntry::FeatureParam
    kZeroStateSuggestionsPlacementAskGeminiParam[] = {
        {kZeroStateSuggestionsPlacementAskGemini, "true"}};

const FeatureEntry::FeatureParam kGeminiImageRemixToolShowFRERowParam[] = {
    {kGeminiImageRemixToolShowFRERow, "true"}};
const FeatureEntry::FeatureParam
    kGeminiImageRemixToolShowAboveSearchImageParam[] = {
        {kGeminiImageRemixToolShowAboveSearchImage, "true"}};
const FeatureEntry::FeatureParam
    kGeminiImageRemixToolShowBelowSearchImageParam[] = {
        {kGeminiImageRemixToolShowBelowSearchImage, "true"}};

const FeatureEntry::FeatureVariation kGeminiImageRemixToolVariations[] = {
    {"(Show FRE Row)", kGeminiImageRemixToolShowFRERowParam, nullptr},
    {"(Show Above Search Image)",
     kGeminiImageRemixToolShowAboveSearchImageParam, nullptr},
    {"(Show Below Search Image)",
     kGeminiImageRemixToolShowBelowSearchImageParam, nullptr}};

const FeatureEntry::FeatureVariation kZeroStateSuggestionsVariations[] = {
    {"AI Hub", kZeroStateSuggestionsPlacementAIHubParam, nullptr},
    {"Ask Gemini Overlay", kZeroStateSuggestionsPlacementAskGeminiParam,
     nullptr},
};

const FeatureEntry::FeatureParam kGeminiCopresenceResponseReadyIntervalParam[] =
    {{kGeminiCopresenceResponseReadyInterval, "7.0"}};
const FeatureEntry::FeatureParam
    kGeminiCopresenceZeroStateWithChatHistoryParam[] = {
        {kGeminiCopresenceZeroStateWithChatHistory, "true"}};

const FeatureEntry::FeatureVariation kGeminiCopresenceVariations[] = {
    {"Response Ready Interval", kGeminiCopresenceResponseReadyIntervalParam,
     nullptr},
    {"Zero State with Chat History",
     kGeminiCopresenceZeroStateWithChatHistoryParam, nullptr},
};

const char kFRESignInHeaderTextUpdateParamName[] =
    "FRESignInHeaderTextUpdateParam";
const FeatureEntry::FeatureParam kFRESignInHeaderTextUpdateArm0[] = {
    {kFRESignInHeaderTextUpdateParamName, "Arm0"}};
const FeatureEntry::FeatureParam kFRESignInHeaderTextUpdateArm1[] = {
    {kFRESignInHeaderTextUpdateParamName, "Arm1"}};

const FeatureEntry::FeatureVariation kFRESignInHeaderTextUpdateVariations[] = {
    {"Header variation #1", kFRESignInHeaderTextUpdateArm0, nullptr},
    {"Header variation #2", kFRESignInHeaderTextUpdateArm1, nullptr},
};

const FeatureEntry::FeatureParam
    kPersistTabContextFileSystem_WasHidden_FullContext[] = {
        {kPersistTabContextStorageParam, "0"},
        {kPersistTabContextExtractionTimingParam, "0"},
        {kPersistTabContextDataParam, "0"}};
const FeatureEntry::FeatureParam
    kPersistTabContextSqlite_WasHidden_FullContext[] = {
        {kPersistTabContextStorageParam, "1"},
        {kPersistTabContextExtractionTimingParam, "0"},
        {kPersistTabContextDataParam, "0"}};
const FeatureEntry::FeatureParam
    kPersistTabContextSqlite_WasHiddenPageLoad_FullContext[] = {
        {kPersistTabContextStorageParam, "1"},
        {kPersistTabContextExtractionTimingParam, "1"},
        {kPersistTabContextDataParam, "0"}};
const FeatureEntry::FeatureParam
    kPersistTabContextSqlite_WasHidden_InnerTextOnly[] = {
        {kPersistTabContextStorageParam, "1"},
        {kPersistTabContextExtractionTimingParam, "0"},
        {kPersistTabContextDataParam, "1"}};

const FeatureEntry::FeatureVariation kPersistTabContextVariations[] = {
    {"FileSystem, On Tab Hide, APC + Inner Text",
     kPersistTabContextFileSystem_WasHidden_FullContext, nullptr},
    {"SQLite, On Tab Hide, APC + Inner Text",
     kPersistTabContextSqlite_WasHidden_FullContext, nullptr},
    {"SQLite, On Tab Hide & Page Load, APC + Inner Text",
     kPersistTabContextSqlite_WasHiddenPageLoad_FullContext, nullptr},
    {"SQLite, On Tab Hide, Inner Text",
     kPersistTabContextSqlite_WasHidden_InnerTextOnly, nullptr}};

const FeatureEntry::FeatureParam kIOSExpandedSetupListSafariImport[] = {
    {kIOSExpandedSetupListVariationParam,
     kIOSExpandedSetupListVariationParamSafariImport}};

const FeatureEntry::FeatureParam
    kIOSExpandedSetupListBackgroundCustomization[] = {
        {kIOSExpandedSetupListVariationParam,
         kIOSExpandedSetupListVariationParamBackgroundCustomization}};

const FeatureEntry::FeatureParam kIOSExpandedSetupListAll[] = {
    {kIOSExpandedSetupListVariationParam,
     kIOSExpandedSetupListVariationParamAll}};

const FeatureEntry::FeatureVariation kIOSExpandedSetupListVariations[] = {
    {"Safari Data Import", kIOSExpandedSetupListSafariImport, nullptr},
    {"Home Background Customization",
     kIOSExpandedSetupListBackgroundCustomization, nullptr},
    {"Safari Data Import & Home Background Customization (without CPE)",
     kIOSExpandedSetupListAll, nullptr},
    {"Safari Data Import, Home Background Customization, CPE",
     kIOSExpandedSetupListAll, nullptr}};

const FeatureEntry::FeatureParam kModelBasedPageClassificationParam1[] = {
    {kModelBasedPageClassificationExecutionRateParam, "1"}};
const FeatureEntry::FeatureParam kModelBasedPageClassificationParam10[] = {
    {kModelBasedPageClassificationExecutionRateParam, "10"}};
const FeatureEntry::FeatureParam kModelBasedPageClassificationParam25[] = {
    {kModelBasedPageClassificationExecutionRateParam, "25"}};
const FeatureEntry::FeatureParam kModelBasedPageClassificationParam50[] = {
    {kModelBasedPageClassificationExecutionRateParam, "50"}};
const FeatureEntry::FeatureParam kModelBasedPageClassificationParam75[] = {
    {kModelBasedPageClassificationExecutionRateParam, "75"}};
const FeatureEntry::FeatureParam kModelBasedPageClassificationParam100[] = {
    {kModelBasedPageClassificationExecutionRateParam, "100"}};

const FeatureEntry::FeatureVariation kModelBasedPageClassificationVariations[] =
    {
        {"(1%)", kModelBasedPageClassificationParam1, nullptr},
        {"(10%)", kModelBasedPageClassificationParam10, nullptr},
        {"(25%)", kModelBasedPageClassificationParam25, nullptr},
        {"(50%)", kModelBasedPageClassificationParam50, nullptr},
        {"(75%)", kModelBasedPageClassificationParam75, nullptr},
        {"(100%)", kModelBasedPageClassificationParam100, nullptr},
};

const FeatureEntry::FeatureParam kAfterEditForExplainGeminiEditMenu[] = {
    {kExplainGeminiEditMenuParams, "1"}};
const FeatureEntry::FeatureParam kAfterSearchForExplainGeminiEditMenu[] = {
    {kExplainGeminiEditMenuParams, "2"}};

const FeatureEntry::FeatureVariation kPositionForExplainGeminiEditMenu[] = {
    {"Explain Gemini shows up after Edit", kAfterEditForExplainGeminiEditMenu,
     nullptr},
    {"Explain Gemini shows up after Search with Google",
     kAfterSearchForExplainGeminiEditMenu, nullptr}};

const FeatureEntry::FeatureParam kPageActionMenuIconSparkles1[] = {
    {kPageActionMenuIconParams, "1"}};
const FeatureEntry::FeatureParam kPageActionMenuIconSparkles2[] = {
    {kPageActionMenuIconParams, "2"}};

const FeatureEntry::FeatureVariation kPageActionMenuIconVariations[] = {
    {"Sparkles 1", kPageActionMenuIconSparkles1, nullptr},
    {"Sparkles 2", kPageActionMenuIconSparkles2, nullptr}};

// To add a new entry, add to the end of kFeatureEntries. There are four
// distinct types of entries:
// . ENABLE_DISABLE_VALUE: entry is either enabled, disabled, or uses the
//   default value for this feature. Use the ENABLE_DISABLE_VALUE_TYPE
//   macro for this type supplying the command line to the macro.
// . MULTI_VALUE: a list of choices, the first of which should correspond to a
//   deactivated state for this lab (i.e. no command line option).  To specify
//   this type of entry use the macro MULTI_VALUE_TYPE supplying it the
//   array of choices.
// . FEATURE_VALUE: entry is associated with a base::Feature instance. Entry is
//   either enabled, disabled, or uses the default value of the associated
//   base::Feature instance. To specify this type of entry use the macro
//   FEATURE_VALUE_TYPE supplying it the base::Feature instance.
// . FEATURE_WITH_PARAM_VALUES: a list of choices associated with a
//   base::Feature instance. Choices corresponding to the default state, a
//   universally enabled state, and a universally disabled state are
//   automatically included. To specify this type of entry use the macro
//   FEATURE_WITH_PARAMS_VALUE_TYPE supplying it the base::Feature instance and
//   the array of choices.
//
// See the documentation of FeatureEntry for details on the fields.
//
// When adding a new choice, add it to the end of the list.
constexpr auto kFeatureEntries = std::to_array<flags_ui::FeatureEntry>({
    {"in-product-help-demo-mode-choice",
     flag_descriptions::kInProductHelpDemoModeName,
     flag_descriptions::kInProductHelpDemoModeDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         feature_engagement::kIPHDemoMode,
         feature_engagement::kIPHDemoModeChoiceVariations,
         "IPH_DemoMode")},
    {"enable-autofill-credit-card-upload",
     flag_descriptions::kAutofillCreditCardUploadName,
     flag_descriptions::kAutofillCreditCardUploadDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillUpstream)},
    {"begin-cursor-at-point-tentative-fix",
     flag_descriptions::kBeginCursorAtPointTentativeFixName,
     flag_descriptions::kBeginCursorAtPointTentativeFixDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kBeginCursorAtPointTentativeFix)},
    {"use-sync-sandbox", flag_descriptions::kSyncSandboxName,
     flag_descriptions::kSyncSandboxDescription, flags_ui::kOsIos,
     SINGLE_VALUE_TYPE_AND_VALUE(
         syncer::kSyncServiceURL,
         "https://chrome-sync.sandbox.google.com/chrome-sync/alpha")},
    {"wallet-service-use-sandbox",
     flag_descriptions::kWalletServiceUseSandboxName,
     flag_descriptions::kWalletServiceUseSandboxDescription, flags_ui::kOsIos,
     ENABLE_DISABLE_VALUE_TYPE_AND_VALUE(
         autofill::switches::kWalletServiceUseSandbox,
         "1",
         autofill::switches::kWalletServiceUseSandbox,
         "0")},
    {"show-autofill-type-predictions",
     flag_descriptions::kShowAutofillTypePredictionsName,
     flag_descriptions::kShowAutofillTypePredictionsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::debug::kAutofillShowTypePredictions)},
    {"ntp-background-customization",
     flag_descriptions::kNTPBackgroundCustomizationName,
     flag_descriptions::kNTPBackgroundCustomizationDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kNTPBackgroundCustomization)},
    {"ntp-background-color-slider",
     flag_descriptions::kNTPBackgroundColorSliderName,
     flag_descriptions::kNTPBackgroundColorSliderDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kNTPBackgroundColorSlider)},
    {"ntp-background-image-cache",
     flag_descriptions::kEnableNTPBackgroundImageCacheName,
     flag_descriptions::kEnableNTPBackgroundImageCacheDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kEnableNTPBackgroundImageCache)},
    {"ntp-alpha-background-collections",
     flag_descriptions::kNtpAlphaBackgroundCollectionsName,
     flag_descriptions::kNtpAlphaBackgroundCollectionsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(ntp_features::kNtpAlphaBackgroundCollections)},
    {"confirmation-button-swap-order",
     flag_descriptions::kConfirmationButtonSwapOrderName,
     flag_descriptions::kConfirmationButtonSwapOrderDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kConfirmationButtonSwapOrder)},
    {"fullscreen-viewport-adjustment-experiment",
     flag_descriptions::kFullscreenSmoothScrollingName,
     flag_descriptions::kFullscreenSmoothScrollingDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::features::kSmoothScrollingDefault)},
    {"fullscreen-scroll-threshold",
     flag_descriptions::kFullscreenScrollThresholdName,
     flag_descriptions::kFullscreenScrollThresholdDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(web::features::kFullscreenScrollThreshold,
                                    kFullscreenScrollThresholdVariations,
                                    "FullscreenScrollThreshold")},
    {"webpage-default-zoom-from-dynamic-type",
     flag_descriptions::kWebPageDefaultZoomFromDynamicTypeName,
     flag_descriptions::kWebPageDefaultZoomFromDynamicTypeDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::kWebPageDefaultZoomFromDynamicType)},
    {"webpage-alternative-text-zoom",
     flag_descriptions::kWebPageAlternativeTextZoomName,
     flag_descriptions::kWebPageAlternativeTextZoomDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(web::kWebPageAlternativeTextZoom)},
    {"webpage-text-zoom-ipad", flag_descriptions::kWebPageTextZoomIPadName,
     flag_descriptions::kWebPageTextZoomIPadDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::kWebPageTextZoomIPad)},
    {"aim-server-eligibility",
     flag_descriptions::kIOSOmniboxAimServerEligibilityName,
     flag_descriptions::kIOSOmniboxAimServerEligibilityDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(omnibox::kAimServerEligibilityEnabled)},
    {"omnibox-ui-max-autocomplete-matches",
     flag_descriptions::kOmniboxUIMaxAutocompleteMatchesName,
     flag_descriptions::kOmniboxUIMaxAutocompleteMatchesDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         omnibox::kUIExperimentMaxAutocompleteMatches,
         kOmniboxUIMaxAutocompleteMatchesVariations,
         "OmniboxUIMaxAutocompleteVariations")},
    {"omnibox-local-history-zero-suggest-beyond-ntp",
     flag_descriptions::kOmniboxLocalHistoryZeroSuggestBeyondNTPName,
     flag_descriptions::kOmniboxLocalHistoryZeroSuggestBeyondNTPDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(omnibox::kLocalHistoryZeroSuggestBeyondNTP)},
    {"omnibox-mobile-parity-update",
     flag_descriptions::kOmniboxMobileParityUpdateName,
     flag_descriptions::kOmniboxMobileParityUpdateDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(omnibox::kOmniboxMobileParityUpdate,
                                    kOmniboxMobileParityVariations,
                                    "OmniboxMobileParityUpdate")},
    {"omnibox-mobile-parity-update-v2",
     flag_descriptions::kOmniboxMobileParityUpdateV2Name,
     flag_descriptions::kOmniboxMobileParityUpdateV2Description,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxMobileParityUpdateV2)},
    {"force-startup-signin-promo",
     flag_descriptions::kForceStartupSigninPromoName,
     flag_descriptions::kForceStartupSigninPromoDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(switches::kForceStartupSigninPromo)},
    {"shared-highlighting-ios", flag_descriptions::kSharedHighlightingIOSName,
     flag_descriptions::kSharedHighlightingIOSDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kSharedHighlightingIOS)},
    {"ios-reactivation-notifications",
     flag_descriptions::kIOSReactivationNotificationsName,
     flag_descriptions::kIOSReactivationNotificationsDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kIOSReactivationNotifications,
                                    kIOSReactivationNotificationsVariations,
                                    "IOSReactivationNotifications")},
    {"ios-expanded-setup-list", flag_descriptions::kIOSExpandedSetupListName,
     flag_descriptions::kIOSExpandedSetupListDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kIOSExpandedSetupList,
                                    kIOSExpandedSetupListVariations,
                                    "IOSExpandedSetupList")},
    {"ios-expanded-tips", flag_descriptions::kIOSExpandedTipsName,
     flag_descriptions::kIOSExpandedTipsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIOSExpandedTips)},
    {"one-time-default-browser-notification",
     flag_descriptions::kIOSOneTimeDefaultBrowserNotificationName,
     flag_descriptions::kIOSOneTimeDefaultBrowserNotificationDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIOSOneTimeDefaultBrowserNotification)},
    {"invalidate-search-engine-choice-on-device-restore-detection",
     flag_descriptions::
         kInvalidateSearchEngineChoiceOnDeviceRestoreDetectionName,
     flag_descriptions::
         kInvalidateSearchEngineChoiceOnDeviceRestoreDetectionDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         switches::kInvalidateSearchEngineChoiceOnDeviceRestoreDetection,
         kInvalidateSearchEngineChoiceOnRestoreVariations,
         "InvalidateSearchEngineChoiceOnDeviceRestoreDetection")},
    {"ios-provides-app-notification-settings",
     flag_descriptions::kIOSProvidesAppNotificationSettingsName,
     flag_descriptions::kIOSProvidesAppNotificationSettingsDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kIOSProvidesAppNotificationSettings)},
    {"wait-threshold-seconds-for-capabilities-api",
     flag_descriptions::kWaitThresholdMillisecondsForCapabilitiesApiName,
     flag_descriptions::kWaitThresholdMillisecondsForCapabilitiesApiDescription,
     flags_ui::kOsIos,
     MULTI_VALUE_TYPE(kWaitThresholdMillisecondsForCapabilitiesApiChoices)},
    {"consistent-logo-doodle-height",
     flag_descriptions::kConsistentLogoDoodleHeightName,
     flag_descriptions::kConsistentLogoDoodleHeightDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kConsistentLogoDoodleHeight)},
    {"content-notification-provisional-ignore-conditions",
     flag_descriptions::kContentNotificationProvisionalIgnoreConditionsName,
     flag_descriptions::
         kContentNotificationProvisionalIgnoreConditionsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kContentNotificationProvisionalIgnoreConditions)},
    {"content-push-notifications",
     flag_descriptions::kContentPushNotificationsName,
     flag_descriptions::kContentPushNotificationsDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kContentPushNotifications,
                                    kContentPushNotificationsVariations,
                                    "ContentPushNotifications")},
    {"enable-lens-in-omnibox-copied-image",
     flag_descriptions::kEnableLensInOmniboxCopiedImageName,
     flag_descriptions::kEnableLensInOmniboxCopiedImageDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kEnableLensInOmniboxCopiedImage)},
    {"ntp-view-hierarchy-repair",
     flag_descriptions::kNTPViewHierarchyRepairName,
     flag_descriptions::kNTPViewHierarchyRepairDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableNTPViewHierarchyRepair)},
    {"price-insights", commerce::flag_descriptions::kPriceInsightsName,
     commerce::flag_descriptions::kPriceInsightsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(commerce::kPriceInsights)},
    {"optimization-guide-debug-logs",
     flag_descriptions::kOptimizationGuideDebugLogsName,
     flag_descriptions::kOptimizationGuideDebugLogsDescription,
     flags_ui::kOsIos,
     SINGLE_VALUE_TYPE(optimization_guide::switches::kDebugLoggingEnabled)},
    {"intents-on-measurements", flag_descriptions::kMeasurementsName,
     flag_descriptions::kMeasurementsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::features::kEnableMeasurements)},
    {"omnibox-https-upgrades", flag_descriptions::kOmniboxHttpsUpgradesName,
     flag_descriptions::kOmniboxHttpsUpgradesDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(omnibox::kDefaultTypedNavigationsToHttps)},
    {"enable-feed-ablation", flag_descriptions::kEnableFeedAblationName,
     flag_descriptions::kEnableFeedAblationDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableFeedAblation)},
    {"ios-keyboard-accessory-default-view",
     flag_descriptions::kIOSKeyboardAccessoryDefaultViewName,
     flag_descriptions::kIOSKeyboardAccessoryDefaultViewDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kIOSKeyboardAccessoryDefaultView)},
    {"ios-keyboard-accessory-two-bubble",
     flag_descriptions::kIOSKeyboardAccessoryTwoBubbleName,
     flag_descriptions::kIOSKeyboardAccessoryTwoBubbleDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kIOSKeyboardAccessoryTwoBubble,
                                    kIOSKeyboardAccessoryTwoBubbleVariations,
                                    "IOSKeyboardAccessoryTwoBubble")},
    {"ios-magic-stack-segmentation-ranking",
     flag_descriptions::kSegmentationPlatformIosModuleRankerName,
     flag_descriptions::kSegmentationPlatformIosModuleRankerDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         segmentation_platform::features::kSegmentationPlatformIosModuleRanker,
         kSegmentationPlatformIosModuleRankerVariations,
         flag_descriptions::kSegmentationPlatformIosModuleRankerName)},
    {"ios-magic-stack-segmentation-ranking-caching",
     flag_descriptions::kSegmentationPlatformIosModuleRankerCachingName,
     flag_descriptions::kSegmentationPlatformIosModuleRankerCachingDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kSegmentationPlatformIosModuleRankerCaching)},
    {"ios-magic-stack-segmentation-ranking-split-by-surface",
     flag_descriptions::kSegmentationPlatformIosModuleRankerSplitBySurfaceName,
     flag_descriptions::
         kSegmentationPlatformIosModuleRankerSplitBySurfaceDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         segmentation_platform::features::
             kSegmentationPlatformIosModuleRankerSplitBySurface)},
    {"ios-proactive-password-generation-bottom-sheet",
     flag_descriptions::kIOSProactivePasswordGenerationBottomSheetName,
     flag_descriptions::kIOSProactivePasswordGenerationBottomSheetDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(password_manager::features::
                            kIOSProactivePasswordGenerationBottomSheet)},
    {"omnibox-zero-suggest-prefetching-on-srp",
     flag_descriptions::kOmniboxZeroSuggestPrefetchingOnSRPName,
     flag_descriptions::kOmniboxZeroSuggestPrefetchingOnSRPDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(omnibox::kZeroSuggestPrefetchingOnSRP)},
    {"omnibox-zero-suggest-prefetching-on-web",
     flag_descriptions::kOmniboxZeroSuggestPrefetchingOnWebName,
     flag_descriptions::kOmniboxZeroSuggestPrefetchingOnWebDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(omnibox::kZeroSuggestPrefetchingOnWeb)},
    {"omnibox-on-device-tail-suggestions",
     flag_descriptions::kOmniboxOnDeviceTailSuggestionsName,
     flag_descriptions::kOmniboxOnDeviceTailSuggestionsDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(omnibox::kOnDeviceTailModel)},
    {"omnibox-most-visited-tiles-horizontal-render-group",
     flag_descriptions::kMostVisitedTilesHorizontalRenderGroupName,
     flag_descriptions::kMostVisitedTilesHorizontalRenderGroupDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(omnibox::kMostVisitedTilesHorizontalRenderGroup)},
    {"metrickit-non-crash-reports",
     flag_descriptions::kMetrickitNonCrashReportName,
     flag_descriptions::kMetrickitNonCrashReportDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kMetrickitNonCrashReport)},
    {"default-browser-off-cycle-promo",
     flag_descriptions::kDefaultBrowserOffCyclePromoName,
     flag_descriptions::kDefaultBrowserOffCyclePromoDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kIOSDefaultBrowserOffCyclePromo)},
    {"use-default-apps-page-for-promos",
     flag_descriptions::kUseDefaultAppsDestinationForPromosName,
     flag_descriptions::kUseDefaultAppsDestinationForPromosDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIOSUseDefaultAppsDestinationForPromos)},
    {"use-scene-view-controller",
     flag_descriptions::kUseSceneViewControllerName,
     flag_descriptions::kUseSceneViewControllerDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kUseSceneViewController)},
    {"persistent-default-browser-promo",
     flag_descriptions::kPersistentDefaultBrowserPromoName,
     flag_descriptions::kPersistentDefaultBrowserPromoDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kPersistentDefaultBrowserPromo)},
    {"default-browser-promo-ipad-instructions",
     flag_descriptions::kDefaultBrowserPromoIpadInstructionsName,
     flag_descriptions::kDefaultBrowserPromoIpadInstructionsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kDefaultBrowserPromoIpadInstructions)},
    {"default-browser-picture-in-picture",
     flag_descriptions::kDefaultBrowserPictureInPictureName,
     flag_descriptions::kDefaultBrowserPictureInPictureDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kDefaultBrowserPictureInPicture,
                                    kDefaultBrowserPictureInPictureVariations,
                                    "DefaultBrowserPictureInPicture")},
#if BUILDFLAG(IOS_BACKGROUND_MODE_ENABLED)
    {"feed-background-refresh-ios",
     flag_descriptions::kFeedBackgroundRefreshName,
     flag_descriptions::kFeedBackgroundRefreshDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kEnableFeedBackgroundRefresh,
                                    kFeedBackgroundRefreshVariations,
                                    "FeedBackgroundRefresh")},
#endif  // BUILDFLAG(IOS_BACKGROUND_MODE_ENABLED)
    {"tab-grid-drag-and-drop", flag_descriptions::kTabGridDragAndDropName,
     flag_descriptions::kTabGridDragAndDropDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kTabGridDragAndDrop)},
    {"tab-grid-new-transitions", flag_descriptions::kTabGridNewTransitionsName,
     flag_descriptions::kTabGridNewTransitionsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kTabGridNewTransitions)},
    {"shopping-list", commerce::flag_descriptions::kShoppingListName,
     commerce::flag_descriptions::kShoppingListDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(commerce::kShoppingList)},
    {"ios-browser-edit-menu-metrics",
     flag_descriptions::kIOSBrowserEditMenuMetricsName,
     flag_descriptions::kIOSBrowserEditMenuMetricsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIOSBrowserEditMenuMetrics)},
    {"ios-docking-promo", flag_descriptions::kIOSDockingPromoName,
     flag_descriptions::kIOSDockingPromoDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kIOSDockingPromo,
                                    kIOSDockingPromoVariations,
                                    "IOSDockingPromo")},
    {"ios-docking-promo-v2", flag_descriptions::kIOSDockingPromoV2Name,
     flag_descriptions::kIOSDockingPromoV2Description, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kIOSDockingPromoV2,
                                    kIOSDockingPromoV2Variations,
                                    "IOSDockingPromoV2")},
    {"omnibox-grouping-framework-non-zps",
     flag_descriptions::kOmniboxGroupingFrameworkForTypedSuggestionsName,
     flag_descriptions::kOmniboxGroupingFrameworkForTypedSuggestionsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(omnibox::kGroupingFrameworkForNonZPS)},
    {"bwg-precise-location", flag_descriptions::kBWGPreciseLocationName,
     flag_descriptions::kBWGPreciseLocationDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kBWGPreciseLocation)},
    {"ai-hub-new-badge", flag_descriptions::kAIHubNewBadgeName,
     flag_descriptions::kAIHubNewBadgeDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kAIHubNewBadge)},
    {"autofill-payments-field-swapping",
     flag_descriptions::kAutofillPaymentsFieldSwappingName,
     flag_descriptions::kAutofillPaymentsFieldSwappingDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillPaymentsFieldSwapping)},
    {"https-upgrades-ios", flag_descriptions::kHttpsUpgradesName,
     flag_descriptions::kHttpsUpgradesDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(security_interstitials::features::kHttpsUpgrades)},
    {"reader-mode-omnibox-entrypoint-in-us",
     flag_descriptions::kReaderModeOmniboxEntrypointInUSName,
     flag_descriptions::kReaderModeOmniboxEntrypointInUSDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableReaderModeOmniboxEntryPointInUS)},
    {"disable-lens-camera", flag_descriptions::kDisableLensCameraName,
     flag_descriptions::kDisableLensCameraDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kDisableLensCamera)},
    {"autofill-enable-card-benefits-for-american-express",
     flag_descriptions::kAutofillEnableCardBenefitsForAmericanExpressName,
     flag_descriptions::
         kAutofillEnableCardBenefitsForAmericanExpressDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableCardBenefitsForAmericanExpress)},
    {"autofill-enable-card-benefits-sync",
     flag_descriptions::kAutofillEnableCardBenefitsSyncName,
     flag_descriptions::kAutofillEnableCardBenefitsSyncDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillEnableCardBenefitsSync)},
    {"autofill-enable-prefetching-risk-data-for-retrieval",
     flag_descriptions::kAutofillEnablePrefetchingRiskDataForRetrievalName,
     flag_descriptions::
         kAutofillEnablePrefetchingRiskDataForRetrievalDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnablePrefetchingRiskDataForRetrieval)},

    {"page-content-annotations", flag_descriptions::kPageContentAnnotationsName,
     flag_descriptions::kPageContentAnnotationsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         page_content_annotations::features::kPageContentAnnotations)},
    {"page-content-annotations-remote-page-metadata",
     flag_descriptions::kPageContentAnnotationsRemotePageMetadataName,
     flag_descriptions::kPageContentAnnotationsRemotePageMetadataDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         page_content_annotations::features::kRemotePageMetadata)},
    {"page-info-certificate-information",
     flag_descriptions::kViewCertificateInformationName,
     flag_descriptions::kViewCertificateInformationDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(page_info_certificate::kViewCertificateInformation)},
    {"page-visibility-page-content-annotations",
     flag_descriptions::kPageVisibilityPageContentAnnotationsName,
     flag_descriptions::kPageVisibilityPageContentAnnotationsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(page_content_annotations::features::
                            kPageVisibilityPageContentAnnotations)},
    {"cpe-passkey-prf-support",
     flag_descriptions::kCredentialProviderPasskeyPRFName,
     flag_descriptions::kCredentialProviderPasskeyPRFDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kCredentialProviderPasskeyPRF)},
    {"migrate-ios-keychain-accessibility",
     flag_descriptions::kMigrateIOSKeychainAccessibilityName,
     flag_descriptions::kMigrateIOSKeychainAccessibilityDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(crypto::features::kMigrateIOSKeychainAccessibility)},
    {"password-form-clientside-classifier",
     flag_descriptions::kPasswordFormClientsideClassifierName,
     flag_descriptions::kPasswordFormClientsideClassifierDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         password_manager::features::kPasswordFormClientsideClassifier)},
    {"send-tab-to-self-enhanced-handoff",
     flag_descriptions::kSendTabToSelfEnhancedHandoffName,
     flag_descriptions::kSendTabToSelfEnhancedHandoffDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(send_tab_to_self::kSendTabToSelfPropagateFormFields)},
    {"send-tab-ios-push-notifications",
     flag_descriptions::kSendTabToSelfIOSPushNotificationsName,
     flag_descriptions::kSendTabToSelfIOSPushNotificationsDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         send_tab_to_self::kSendTabToSelfIOSPushNotifications,
         kSendTabIOSPushNotificationsVariations,
         "SendTabToSelfIOSPushNotifications")},
    {"lens-filters-ablation-mode-enabled",
     flag_descriptions::kLensFiltersAblationModeEnabledName,
     flag_descriptions::kLensFiltersAblationModeEnabledDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kLensFiltersAblationModeEnabled,
                                    kLensFiltersAblationModeVariations,
                                    "LensFiltersAblationMode")},
    {"lens-translate-toogle-mode-enabled",
     flag_descriptions::kLensTranslateToggleModeEnabledName,
     flag_descriptions::kLensTranslateToggleModeEnabledDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kLensTranslateToggleModeEnabled,
                                    kLensTranslateToggleModeVariations,
                                    "LensTranslateToggleMode")},
    {"omnibox-mia-zps", flag_descriptions::kOmniboxMiaZpsName,
     flag_descriptions::kOmniboxMiaZpsDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         omnibox_feature_configs::MiaZPS::kOmniboxMiaZPS,
         kOmniboxMiaZpsVariations,
         "OmniboxMiaZpsVariations")},
    {"omnibox-ml-log-url-scoring-signals",
     flag_descriptions::kOmniboxMlLogUrlScoringSignalsName,
     flag_descriptions::kOmniboxMlLogUrlScoringSignalsDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(omnibox::kLogUrlScoringSignals)},
    {"omnibox-ml-url-piecewise-mapped-search-blending",
     flag_descriptions::kOmniboxMlUrlPiecewiseMappedSearchBlendingName,
     flag_descriptions::kOmniboxMlUrlPiecewiseMappedSearchBlendingDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         omnibox::kMlUrlPiecewiseMappedSearchBlending,
         kMlUrlPiecewiseMappedSearchBlendingVariations,
         "MlUrlPiecewiseMappedSearchBlending")},
    {"omnibox-ml-url-score-caching",
     flag_descriptions::kOmniboxMlUrlScoreCachingName,
     flag_descriptions::kOmniboxMlUrlScoreCachingDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(omnibox::kMlUrlScoreCaching)},
    {"omnibox-ml-url-scoring", flag_descriptions::kOmniboxMlUrlScoringName,
     flag_descriptions::kOmniboxMlUrlScoringDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(omnibox::kMlUrlScoring,
                                    kOmniboxMlUrlScoringVariations,
                                    "MlUrlScoring")},
    {"omnibox-ml-url-search-blending",
     flag_descriptions::kOmniboxMlUrlSearchBlendingName,
     flag_descriptions::kOmniboxMlUrlSearchBlendingDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(omnibox::kMlUrlSearchBlending,
                                    kMlUrlSearchBlendingVariations,
                                    "MlUrlScoring")},
    {"omnibox-ml-url-scoring-model",
     flag_descriptions::kOmniboxMlUrlScoringModelName,
     flag_descriptions::kOmniboxMlUrlScoringModelDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(omnibox::kUrlScoringModel,
                                    kUrlScoringModelVariations,
                                    "MlUrlScoring")},
    {"autofill-show-manual-fill-for-virtual-cards",
     flag_descriptions::kAutofillShowManualFillForVirtualCardsName,
     flag_descriptions::kAutofillShowManualFillForVirtualCardsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillShowManualFillForVirtualCards)},
    {"app-background-refresh-ios", flag_descriptions::kAppBackgroundRefreshName,
     flag_descriptions::kAppBackgroundRefreshDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableAppBackgroundRefresh)},
    {"lens-web-page-load-optimization-enabled",
     flag_descriptions::kLensWebPageLoadOptimizationEnabledName,
     flag_descriptions::kLensWebPageLoadOptimizationEnabledDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kLensWebPageLoadOptimizationEnabled)},
    {"autofill-unmask-card-request-timeout",
     flag_descriptions::kAutofillUnmaskCardRequestTimeoutName,
     flag_descriptions::kAutofillUnmaskCardRequestTimeoutDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillUnmaskCardRequestTimeout)},
    {"autofill-across-iframes", flag_descriptions::kAutofillAcrossIframesName,
     flag_descriptions::kAutofillAcrossIframesDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillAcrossIframesIos)},
    {"enable-trait-collection-registration",
     flag_descriptions::kEnableTraitCollectionRegistrationName,
     flag_descriptions::kEnableTraitCollectionRegistrationDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kEnableTraitCollectionRegistration)},
    {"safe-browsing-local-lists-use-sbv5",
     flag_descriptions::kSafeBrowsingLocalListsUseSBv5Name,
     flag_descriptions::kSafeBrowsingLocalListsUseSBv5Description,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(safe_browsing::kLocalListsUseSBv5)},
    {"ios-shop-card", flag_descriptions::kShopCardName,
     flag_descriptions::kShopCardDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(commerce::kTabResumptionShopCard,
                                    kShopCardOverrideOptions,
                                    "TabResumptionShopCard")},
    {"ios-shop-card-impression-limits",
     flag_descriptions::kShopCardImpressionLimitsName,
     flag_descriptions::kShopCardImpressionLimitsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(commerce::kShopCardImpressionLimits)},
    {"ios-segmentation-ephemeral-card-ranker",
     flag_descriptions::kSegmentationPlatformEphemeralCardRankerName,
     flag_descriptions::kSegmentationPlatformEphemeralCardRankerDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         segmentation_platform::features::
             kSegmentationPlatformEphemeralCardRanker,
         kEphemeralCardRankerCardOverrideOptions,
         "SegmentationPlatformEphemeralCardRanker")},
    {"lens-overlay-enable-landscape-compatibility",
     flag_descriptions::kLensOverlayEnableLandscapeCompatibilityName,
     flag_descriptions::kLensOverlayEnableLandscapeCompatibilityDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kLensOverlayEnableLandscapeCompatibility)},
    {"data-sharing", flag_descriptions::kDataSharingName,
     flag_descriptions::kDataSharingDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(data_sharing::features::kDataSharingFeature)},
    {"data-sharing-join-only", flag_descriptions::kDataSharingJoinOnlyName,
     flag_descriptions::kDataSharingJoinOnlyDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(data_sharing::features::kDataSharingJoinOnly)},
    {"ios-soft-lock", flag_descriptions::kIOSSoftLockName,
     flag_descriptions::kIOSSoftLockDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kIOSSoftLock,
                                    kIOSSoftLockVariations,
                                    "IOSSoftLock")},
    {"lens-unary-apis-with-http-transport-enabled",
     flag_descriptions::kLensUnaryApisWithHttpTransportEnabledName,
     flag_descriptions::kLensUnaryApisWithHttpTransportEnabledDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kLensUnaryApisWithHttpTransportEnabled)},
    {"ios-provisional-notification-alert",
     flag_descriptions::kProvisionalNotificationAlertName,
     flag_descriptions::kProvisionalNotificationAlertDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kProvisionalNotificationAlert)},
    {"autofill-throttle-doc-form-scans",
     flag_descriptions::kAutofillThrottleDocumentFormScanName,
     flag_descriptions::kAutofillThrottleDocumentFormScanDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kAutofillThrottleDocumentFormScanIos,
                                    kAutofillThrottleDocFormScanVariations,
                                    "AutofillThrottleDocumentFormScan")},
    {"autofill-throttle-filtered-doc-form-scan",
     flag_descriptions::kAutofillThrottleFilteredDocumentFormScanName,
     flag_descriptions::kAutofillThrottleFilteredDocumentFormScanDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         kAutofillThrottleFilteredDocumentFormScanIos,
         kAutofillThrottleFilteredDocFormScanVariations,
         "AutofillThrottleFilteredDocumentFormScan")},
    {"autofill-payments-sheet-v2",
     flag_descriptions::kAutofillPaymentsSheetV2Name,
     flag_descriptions::kAutofillPaymentsSheetV2Description, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kAutofillPaymentsSheetV2Ios)},
    {"lens-unary-http-transport-enabled",
     flag_descriptions::kLensUnaryHttpTransportEnabledName,
     flag_descriptions::kLensUnaryHttpTransportEnabledDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kLensUnaryHttpTransportEnabled)},
    {"collaboration-messaging", flag_descriptions::kCollaborationMessagingName,
     flag_descriptions::kCollaborationMessagingDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(collaboration::features::kCollaborationMessaging)},
    {"lens-single-tap-text-selection-disabled",
     flag_descriptions::kLensSingleTapTextSelectionDisabledName,
     flag_descriptions::kLensSingleTapTextSelectionDisabledDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kLensSingleTapTextSelectionDisabled)},
    {"updated-fre-screens-sequence", flag_descriptions::kUpdatedFRESequenceName,
     flag_descriptions::kUpdatedFRESequenceDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(first_run::kUpdatedFirstRunSequence,
                                    kUpdatedFirstRunSequenceVariations,
                                    "UpdatedFirstRunSequence")},
    {"autofill-enable-card-benefits-for-bmo",
     flag_descriptions::kAutofillEnableCardBenefitsForBmoName,
     flag_descriptions::kAutofillEnableCardBenefitsForBmoDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillEnableCardBenefitsForBmo)},
    {"download-auto-deletion", flag_descriptions::kDownloadAutoDeletionName,
     flag_descriptions::kDownloadAutoDeletionDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kDownloadAutoDeletionFeatureEnabled)},
    {"download-auto-deletion-clear-files-on-every-startup",
     flag_descriptions::kDownloadAutoDeletionClearFilesOnEveryStartupName,
     flag_descriptions::
         kDownloadAutoDeletionClearFilesOnEveryStartupDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kDownloadAutoDeletionClearFilesOnEveryStartup)},
    {"download-list-ios", flag_descriptions::kDownloadListName,
     flag_descriptions::kDownloadListDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kDownloadList,
                                    kDownloadListVariations,
                                    "IOSDownloadList")},
    {"animated-default-browser-promo-in-fre",
     flag_descriptions::kAnimatedDefaultBrowserPromoInFREName,
     flag_descriptions::kAnimatedDefaultBrowserPromoInFREDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         first_run::kAnimatedDefaultBrowserPromoInFRE,
         kAnimatedDefaultBrowserPromoInFREVariations,
         "AnimatedDBPInFRE-Layout")},
    {"fullscreen-transition-speed",
     flag_descriptions::kFullscreenTransitionSpeedName,
     flag_descriptions::kFullscreenTransitionSpeedDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kFullscreenTransitionSpeed,
                                    kFullscreenTransitionVariations,
                                    "IOSFullscreenTransitionSpeed")},
    {"refactor-toolbars-size", flag_descriptions::kRefactorToolbarsSizeName,
     flag_descriptions::kRefactorToolbarsSizeDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kRefactorToolbarsSize)},
    {"lens-gesture-text-selection-disabled",
     flag_descriptions::kLensGestureTextSelectionDisabledName,
     flag_descriptions::kLensGestureTextSelectionDisabledDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kLensGestureTextSelectionDisabled)},
    {"ios-best-features-screen",
     flag_descriptions::kBestFeaturesScreenInFirstRunName,
     flag_descriptions::kBestFeaturesScreenInFirstRunDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(first_run::kBestFeaturesScreenInFirstRun,
                                    kBestFeaturesScreenInFirstRunVariations,
                                    "BestFeaturesScreenInFirstRun")},
    {"manual-log-uploads-in-the-fre",
     flag_descriptions::kManualLogUploadsInFREName,
     flag_descriptions::kManualLogUploadsInFREDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(first_run::kManualLogUploadsInTheFRE)},
    {"lens-unary-api-salient-text-enabled",
     flag_descriptions::kLensUnaryApiSalientTextEnabledName,
     flag_descriptions::kLensUnaryApiSalientTextEnabledDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kLensUnaryApiSalientTextEnabled)},
    {"ios-one-tap-mini-map-restrictions",
     flag_descriptions::kIOSOneTapMiniMapRestrictionsName,
     flag_descriptions::kIOSOneTapMiniMapRestrictionsDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kIOSOneTapMiniMapRestrictions,
                                    kIOSOneTapMiniMapRestrictionsVariations,
                                    "IOSOneTapMiniMapRestrictions")},
    {"lens-block-fetch-objects-interaction-rpcs-on-separate-handshake",
     flag_descriptions::
         kLensBlockFetchObjectsInteractionRPCsOnSeparateHandshakeName,
     flag_descriptions::
         kLensBlockFetchObjectsInteractionRPCsOnSeparateHandshakeDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         kLensBlockFetchObjectsInteractionRPCsOnSeparateHandshake)},
    {"lens-prewarm-hard-stickiness-in-input-selection",
     flag_descriptions::kLensPrewarmHardStickinessInInputSelectionName,
     flag_descriptions::kLensPrewarmHardStickinessInInputSelectionDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kLensPrewarmHardStickinessInInputSelection)},
    {"lens-prewarm-hard-stickiness-in-query-formulation",
     flag_descriptions::kLensPrewarmHardStickinessInQueryFormulationName,
     flag_descriptions::kLensPrewarmHardStickinessInQueryFormulationDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kLensPrewarmHardStickinessInQueryFormulation)},
    {"enhanced-calendar", flag_descriptions::kEnhancedCalendarName,
     flag_descriptions::kEnhancedCalendarDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnhancedCalendar)},
    {"explain-gemini-edit-menu", flag_descriptions::kExplainGeminiEditMenuName,
     flag_descriptions::kExplainGeminiEditMenuDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kExplainGeminiEditMenu,
                                    kPositionForExplainGeminiEditMenu,
                                    "ExplainGeminiEditMenu")},
    {"data-sharing-debug-logs", flag_descriptions::kDataSharingDebugLogsName,
     flag_descriptions::kDataSharingDebugLogsDescription, flags_ui::kOsIos,
     SINGLE_VALUE_TYPE(data_sharing::kDataSharingDebugLoggingEnabled)},
    {"supervised-user-block-interstitial-v3",
     flag_descriptions::kSupervisedUserBlockInterstitialV3Name,
     flag_descriptions::kSupervisedUserBlockInterstitialV3Description,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(supervised_user::kSupervisedUserBlockInterstitialV3)},
    {"supervised-user-emit-log-record-separately",
     flag_descriptions::kSupervisedUserEmitLogRecordSeparatelyName,
     flag_descriptions::kSupervisedUserEmitLogRecordSeparatelyDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         supervised_user::kSupervisedUserEmitLogRecordSeparately)},
    {"supervised-user-merge-device-parental-controls-and-family-link-prefs",
     flag_descriptions::
         kSupervisedUserMergeDeviceParentalControlsAndFamilyLinkPrefsName,
     flag_descriptions::
         kSupervisedUserMergeDeviceParentalControlsAndFamilyLinkPrefsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         supervised_user::
             kSupervisedUserMergeDeviceParentalControlsAndFamilyLinkPrefs)},
    {"supervised-user-use-url-filtering-service",
     flag_descriptions::kSupervisedUserUseUrlFilteringServiceName,
     flag_descriptions::kSupervisedUserUseUrlFilteringServiceDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         supervised_user::kSupervisedUserUseUrlFilteringService)},
    {"lens-fetch-srp-api-enabled",
     flag_descriptions::kLensFetchSrpApiEnabledName,
     flag_descriptions::kLensFetchSrpApiEnabledDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kLensFetchSrpApiEnabled)},
    {"autofill-enable-card-info-runtime-retrieval",
     flag_descriptions::kAutofillEnableCardInfoRuntimeRetrievalName,
     flag_descriptions::kAutofillEnableCardInfoRuntimeRetrievalDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableCardInfoRuntimeRetrieval)},
    {"feed-swipe-iph", flag_descriptions::kFeedSwipeInProductHelpName,
     flag_descriptions::kFeedSwipeInProductHelpDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kFeedSwipeInProductHelp,
                                    kFeedSwipeInProductHelpVariations,
                                    "FeedSwipeInProductHelp")},
    {"notification-collision-management",
     flag_descriptions::kNotificationCollisionManagementName,
     flag_descriptions::kNotificationCollisionManagementDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kNotificationCollisionManagement)},
    {"ntp-mia-entrypoint", flag_descriptions::kNTPMIAEntrypointName,
     flag_descriptions::kNTPMIAEntrypointDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kNTPMIAEntrypoint,
                                    kNTPMIAEntrypointVariations,
                                    "kNTPMIAEntrypoint")},

    {"autofill-enable-support-for-home-and-work",
     flag_descriptions::kAutofillEnableSupportForHomeAndWorkName,
     flag_descriptions::kAutofillEnableSupportForHomeAndWorkDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableSupportForHomeAndWork)},
    {"lens-overlay-navigation-history",
     flag_descriptions::kLensOverlayNavigationHistoryName,
     flag_descriptions::kLensOverlayNavigationHistoryDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kLensOverlayNavigationHistory)},
    {"lens-overlay-custom-bottom-sheet",
     flag_descriptions::kLensOverlayCustomBottomSheetName,
     flag_descriptions::kLensOverlayCustomBottomSheetDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kLensOverlayCustomBottomSheet)},
    {"page-action-menu", flag_descriptions::kPageActionMenuName,
     flag_descriptions::kPageActionMenuDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kPageActionMenu,
                                    kPageActionMenuVariations,
                                    "IOSPageActionMenu")},
    {"proactive-suggestions-framework",
     flag_descriptions::kProactiveSuggestionsFrameworkName,
     flag_descriptions::kProactiveSuggestionsFrameworkDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kProactiveSuggestionsFramework,
                                    kProactiveSuggestionsFrameworkVariations,
                                    "ProactiveSuggestionsFramework")},
    {"ask-gemini-chip", flag_descriptions::kAskGeminiChipName,
     flag_descriptions::kAskGeminiChipDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kAskGeminiChip,
                                    kAskGeminiChipVariations,
                                    "IOSAskGeminiChip")},
    {"gemini-copresence", flag_descriptions::kGeminiCopresenceName,
     flag_descriptions::kGeminiCopresenceDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kGeminiCopresence,
                                    kGeminiCopresenceVariations,
                                    "GeminiCopresence")},
    {"bwg-promo-consent", flag_descriptions::kBWGPromoConsentName,
     flag_descriptions::kBWGPromoConsentDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kBWGPromoConsent,
                                    kBWGPromoConsentVariations,
                                    "IOSBWGPromoConsent")},
    {"safe-browsing-trusted-url",
     flag_descriptions::kSafeBrowsingTrustedURLName,
     flag_descriptions::kSafeBrowsingTrustedURLDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kSafeBrowsingTrustedURL)},
    {"sync-trusted-vault-infobar-message-improvements",
     flag_descriptions::kSyncTrustedVaultInfobarMessageImprovementsName,
     flag_descriptions::kSyncTrustedVaultInfobarMessageImprovementsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(syncer::kSyncTrustedVaultInfobarMessageImprovements)},
    {"autofill-vcn-enroll-strike-expiry-time",
     flag_descriptions::kAutofillVcnEnrollStrikeExpiryTimeName,
     flag_descriptions::kAutofillVcnEnrollStrikeExpiryTimeDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         autofill::features::kAutofillVcnEnrollStrikeExpiryTime,
         kAutofillVcnEnrollStrikeExpiryTimeOptions,
         "AutofillVcnEnrollStrikeExpiryTime")},
    {"ios-welcome-back-screen", flag_descriptions::kWelcomeBackName,
     flag_descriptions::kWelcomeBackDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kWelcomeBack,
                                    kWelcomeBackVariations,
                                    "WelcomeBack")},
    {"autofill-enable-flat-rate-card-benefits-from-curinos",
     flag_descriptions::kAutofillEnableFlatRateCardBenefitsFromCurinosName,
     flag_descriptions::
         kAutofillEnableFlatRateCardBenefitsFromCurinosDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableFlatRateCardBenefitsFromCurinos)},
    {"reader-mode-translation-with-infobar-enabled",
     flag_descriptions::kReaderModeTranslationWithInfobarName,
     flag_descriptions::kReaderModeTranslationWithInfobarDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableReaderModeTranslationWithInfobar)},
    {"reader-mode-optimization-guide-eligibility",
     flag_descriptions::kReaderModeOptimizationGuideEligibilityName,
     flag_descriptions::kReaderModeOptimizationGuideEligibilityDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableReaderModeOptimizationGuideEligibility)},
    {"reader-mode-readability-heuristic-enabled",
     flag_descriptions::kReaderModeReadabilityHeuristicName,
     flag_descriptions::kReaderModeReadabilityHeuristicDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kEnableReadabilityHeuristic)},
    {"reader-mode-support-new-fonts",
     flag_descriptions::kReaderModeSupportNewFontsName,
     flag_descriptions::kReaderModeSupportNewFontsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(dom_distiller::kReaderModeSupportNewFonts)},
    {"reader-mode-us-enabled", flag_descriptions::kReaderModeUSEnabledName,
     flag_descriptions::kReaderModeUSEnabledDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableReaderModeInUS)},
    {"reader-mode-readability-distiller-enabled",
     flag_descriptions::kReaderModeReadabilityDistillerName,
     flag_descriptions::kReaderModeReadabilityDistillerDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(dom_distiller::kReaderModeUseReadability)},
    {"reader-mode-content-settings-for-links",
     flag_descriptions::kReaderModeContentSettingsForLinksName,
     flag_descriptions::kReaderModeContentSettingsForLinksDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableContentSettingsOptionForLinks)},
    {"best-of-app-fre", flag_descriptions::kBestOfAppFREName,
     flag_descriptions::kBestOfAppFREDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kBestOfAppFRE,
                                    kBestOfAppFREVariations,
                                    "BestOfAppFRE")},
    {"use-new-feed-eligibility-service",
     flag_descriptions::kUseFeedEligibilityServiceName,
     flag_descriptions::kUseFeedEligibilityServiceDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kUseFeedEligibilityService)},
    {"enable-profile-reporting",
     flag_descriptions::kIOSEnableCloudProfileReportingName,
     flag_descriptions::kIOSEnableCloudProfileReportingDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(enterprise_reporting::kCloudProfileReporting)},
    {"browser-report-include-all-profiles",
     flag_descriptions::kIOSBrowserReportIncludeAllProfilesName,
     flag_descriptions::kIOSBrowserReportIncludeAllProfilesDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         enterprise_reporting::kBrowserReportIncludeAllProfiles)},
    {"lens-load-aim-in-lens-result-page",
     flag_descriptions::kLensLoadAIMInLensResultPageName,
     flag_descriptions::kLensLoadAIMInLensResultPageDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kLensLoadAIMInLensResultPage)},
    {"lens-exact-matches-enabled",
     flag_descriptions::kLensExactMatchesEnabledName,
     flag_descriptions::kLensExactMatchesEnabledDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kLensExactMatchesEnabled)},
    {"disable-autofill-strike-system",
     flag_descriptions::kDisableAutofillStrikeSystemName,
     flag_descriptions::kDisableAutofillStrikeSystemDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(strike_database::features::kDisableStrikeSystem)},
    {"ios-default-browser-promo-propensity-model",
     flag_descriptions::kDefaultBrowserPromoPropensityModelName,
     flag_descriptions::kDefaultBrowserPromoPropensityModelDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         segmentation_platform::features::kDefaultBrowserPromoPropensityModel)},
    {"shopping-alternate-server",
     commerce::flag_descriptions::kShoppingAlternateServerName,
     commerce::flag_descriptions::kShoppingAlternateServerDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(commerce::kShoppingAlternateServer)},
    {"taiyaki", flag_descriptions::kTaiyakiName,
     flag_descriptions::kTaiyakiDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(switches::kTaiyaki,
                                    kTaiyakiChoiceScreenSurfaceVariations,
                                    "Taiyaki")},
    {"lens-camera-no-still-output-required",
     flag_descriptions::kLensCameraNoStillOutputRequiredName,
     flag_descriptions::kLensCameraNoStillOutputRequiredDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kLensCameraNoStillOutputRequired)},
    {"lens-camera-unbinned-capture-formats-preferred",
     flag_descriptions::kLensCameraUnbinnedCaptureFormatsPreferredName,
     flag_descriptions::kLensCameraUnbinnedCaptureFormatsPreferredDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kLensCameraUnbinnedCaptureFormatsPreferred)},
    {"lens-continuous-zoom-enabled",
     flag_descriptions::kLensContinuousZoomEnabledName,
     flag_descriptions::kLensContinuousZoomEnabledDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kLensContinuousZoomEnabled)},
    {"lens-initial-lvf-zoom-level-90-percent",
     flag_descriptions::kLensInitialLvfZoomLevel90PercentName,
     flag_descriptions::kLensInitialLvfZoomLevel90PercentDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kLensInitialLvfZoomLevel90Percent)},
    {"lens-triple-camera-enabled",
     flag_descriptions::kLensTripleCameraEnabledName,
     flag_descriptions::kLensTripleCameraEnabledDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kLensTripleCameraEnabled)},
    // LINT.IfChange(DataSharingVersioning)
    {"shared-data-types-kill-switch",
     flag_descriptions::kDataSharingVersioningStatesName,
     flag_descriptions::kDataSharingVersioningStatesDescription,
     flags_ui::kOsIos, MULTI_VALUE_TYPE(kDataSharingVersioningStateChoices)},
    // LINT.ThenChange(//chrome/browser/about_flags.cc:DataSharingVersioning)
    {"share-ablation-hide-share-in-toolbar",
     flag_descriptions::kDisableShareButtonName,
     flag_descriptions::kDisableShareButtonDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kDisableShareButton)},
    {"share-ablation-omnibox-long-press",
     flag_descriptions::kShareInOmniboxLongPressName,
     flag_descriptions::kShareInOmniboxLongPressDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kShareInOmniboxLongPress)},
    {"share-ablation-omnibox-overflow-menu",
     flag_descriptions::kShareInOverflowMenuName,
     flag_descriptions::kShareInOverflowMenuDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kShareInOverflowMenu)},
    {"share-ablation-verbatim-match",
     flag_descriptions::kShareInVerbatimMatchName,
     flag_descriptions::kShareInVerbatimMatchDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kShareInVerbatimMatch)},
    {"ios-trusted-vault-notification",
     flag_descriptions::kIOSTrustedVaultNotificationName,
     flag_descriptions::kIOSTrustedVaultNotificationDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kIOSTrustedVaultNotification)},
    {"omnibox-drs-prototype", flag_descriptions::kOmniboxDRSPrototypeName,
     flag_descriptions::kOmniboxDRSPrototypeDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kOmniboxDRSPrototype)},
    {"sync-autofill-wallet-credential-data",
     flag_descriptions::kSyncAutofillWalletCredentialDataName,
     flag_descriptions::kSyncAutofillWalletCredentialDataDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(syncer::kSyncAutofillWalletCredentialData)},
    {"autofill-enable-cvc-storage-and-filling",
     flag_descriptions::kAutofillEnableCvcStorageAndFillingName,
     flag_descriptions::kAutofillEnableCvcStorageAndFillingDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableCvcStorageAndFilling)},
    {"autofill-enable-cvc-storage-and-filling-enhancement",
     flag_descriptions::kAutofillEnableCvcStorageAndFillingEnhancementName,
     flag_descriptions::
         kAutofillEnableCvcStorageAndFillingEnhancementDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableCvcStorageAndFillingEnhancement)},
    {"autofill-enable-cvc-storage-and-filling-standalone-form-enhancement",
     flag_descriptions::
         kAutofillEnableCvcStorageAndFillingStandaloneFormEnhancementName,
     flag_descriptions::
         kAutofillEnableCvcStorageAndFillingStandaloneFormEnhancementDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::
             kAutofillEnableCvcStorageAndFillingStandaloneFormEnhancement)},
    {"rcaps-dynamic-profile-country",
     flag_descriptions::kRcapsDynamicProfileCountryName,
     flag_descriptions::kRcapsDynamicProfileCountryDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(switches::kDynamicProfileCountry)},
    {"ios-skip-fre-default-browser-promo-in-eea",
     flag_descriptions::kSkipDefaultBrowserPromoInFirstRunName,
     flag_descriptions::kSkipDefaultBrowserPromoInFirstRunDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(first_run::kSkipDefaultBrowserPromoInFirstRun)},
    {"apply-clientside-model-predictions-for-password-types",
     flag_descriptions::kApplyClientsideModelPredictionsForPasswordTypesName,
     flag_descriptions::
         kApplyClientsideModelPredictionsForPasswordTypesDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(password_manager::features::
                            kApplyClientsideModelPredictionsForPasswordTypes)},
    {"cpe-passkey-largeblob-support",
     flag_descriptions::kCredentialProviderPasskeyLargeBlobName,
     flag_descriptions::kCredentialProviderPasskeyLargeBlobDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kCredentialProviderPasskeyLargeBlob)},
    {"autofill-credit-card-scanner-ios",
     flag_descriptions::kAutofillCreditCardScannerIosName,
     flag_descriptions::kAutofillCreditCardScannerIosDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillCreditCardScannerIos)},
    {"lens-strokes-api-enabled", flag_descriptions::kStrokesAPIEnabledName,
     flag_descriptions::kStrokesAPIEnabledDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kLensStrokesAPIEnabled)},
    {"composebox-deep-search", flag_descriptions::kComposeboxDeepSearchName,
     flag_descriptions::kComposeboxDeepSearchDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kComposeboxDeepSearch)},
    {"composebox-server-side-state",
     flag_descriptions::kComposeboxServerSideStateName,
     flag_descriptions::kComposeboxServerSideStateDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kComposeboxServerSideState)},
    {"composebox-devtools", flag_descriptions::kComposeboxDevToolsName,
     flag_descriptions::kComposeboxDevToolsDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kComposeboxDevTools,
                                    kComposeboxDevToolsVariations,
                                    "ComposeboxDevTools")},
    {"autofill-manual-testing-data",
     flag_descriptions::kAutofillManualTestingDataName,
     flag_descriptions::kAutofillManualTestingDataDescription, flags_ui::kOsIos,
     STRING_VALUE_TYPE(autofill::kManualContentImportForTestingFlag, "")},
    {"autofill-enable-support-for-name-and-email-profile",
     flag_descriptions::kAutofillEnableSupportForNameAndEmailName,
     flag_descriptions::kAutofillEnableSupportForNameAndEmailDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableSupportForNameAndEmail)},
    {"mobile-promo-on-desktop-data-collection",
     flag_descriptions::kMobilePromoOnDesktopRecordActiveDaysName,
     flag_descriptions::kMobilePromoOnDesktopRecordActiveDaysDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kMobilePromoOnDesktopRecordActiveDays)},
    {"mobile-promo-on-desktop-with-reminder",
     flag_descriptions::kMobilePromoOnDesktopName,
     flag_descriptions::kMobilePromoOnDesktopDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kMobilePromoOnDesktopWithReminder,
                                    kMobilePromoOnDesktopVariations,
                                    "MobilePromoOnDesktopWithReminder")},
    {"lens-search-headers-check-enabled",
     flag_descriptions::kLensSearchHeadersCheckEnabledName,
     flag_descriptions::kLensSearchHeadersCheckEnabledDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kLensSearchHeadersCheckEnabled)},
    {"autofill-bottom-sheet-new-blur",
     flag_descriptions::kAutofillBottomSheetNewBlurName,
     flag_descriptions::kAutofillBottomSheetNewBlurDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kAutofillBottomSheetNewBlur)},
    {"ios-app-bundle-promo-magic-stack",
     flag_descriptions::kIOSAppBundlePromoEphemeralCardName,
     flag_descriptions::kIOSAppBundlePromoEphemeralCardDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         segmentation_platform::features::kAppBundlePromoEphemeralCard)},
    {"hide-fusebox-voice-lens-actions",
     flag_descriptions::kHideFuseboxVoiceLensActionsName,
     flag_descriptions::kHideFuseboxVoiceLensActionsDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kHideFuseboxVoiceLensActions)},
    {"hide-toolbars-in-overflow-menu",
     flag_descriptions::kHideToolbarsInOverflowMenuName,
     flag_descriptions::kHideToolbarsInOverflowMenuDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kHideToolbarsInOverflowMenu)},
    {"smart-tab-grouping", flag_descriptions::kSmartTabGroupingName,
     flag_descriptions::kSmartTabGroupingDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kSmartTabGrouping)},
    {"persist-tab-context", flag_descriptions::kPersistTabContextName,
     flag_descriptions::kPersistTabContextDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kPersistTabContext,
                                    kPersistTabContextVariations,
                                    "PersistTabContext")},
    {"composebox-immersive-srp", flag_descriptions::kComposeboxImmersiveSRPName,
     flag_descriptions::kComposeboxImmersiveSRPDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kComposeboxImmersiveSRP)},
    {"composebox-tab-picker-variation",
     flag_descriptions::kComposeboxTabPickerVariationName,
     flag_descriptions::kComposeboxTabPickerVariationDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kComposeboxTabPickerVariation,
                                    kComposeboxTabPickerVariationVariations,
                                    "ComposeboxTabPickerVariation")},
    {"composebox-uses-chrome-compose-client",
     flag_descriptions::kNtpComposeboxUsesChromeComposeClientName,
     flag_descriptions::kNtpComposeboxUsesChromeComposeClientDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(omnibox::kComposeboxUsesChromeComposeClient)},
    {"ios-custom-file-upload-menu",
     flag_descriptions::kIOSCustomFileUploadMenuName,
     flag_descriptions::kIOSCustomFileUploadMenuDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIOSCustomFileUploadMenu)},
    {"ios-tab-group-entry-point-overflow-menu",
     flag_descriptions::kTabGroupInOverflowMenuName,
     flag_descriptions::kTabGroupInOverflowMenuDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kTabGroupInOverflowMenu)},
    {"ios-tab-group-entry-point-tab-icon",
     flag_descriptions::kTabGroupInTabIconContextMenuName,
     flag_descriptions::kTabGroupInTabIconContextMenuDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kTabGroupInTabIconContextMenu)},
    {"ios-tab-group-entry-point-tab-recall",
     flag_descriptions::kTabRecallNewTabGroupButtonName,
     flag_descriptions::kTabRecallNewTabGroupButtonDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kTabRecallNewTabGroupButton)},
    {"ios-tab-group-entry-point-tab-switcher",
     flag_descriptions::kTabSwitcherOverflowMenuName,
     flag_descriptions::kTabSwitcherOverflowMenuDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kTabSwitcherOverflowMenu)},
    {"cache-identity-list-in-chrome",
     flag_descriptions::kCacheIdentityListInChromeName,
     flag_descriptions::kCacheIdentityListInChromeDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(switches::kCacheIdentityListInChrome)},
    {"gemini-dynamic-settings", flag_descriptions::kGeminiDynamicSettingsName,
     flag_descriptions::kGeminiDynamicSettingsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kGeminiDynamicSettings)},
    {"enable-ac-prefetch", flag_descriptions::kEnableACPrefetchName,
     flag_descriptions::kEnableACPrefetchDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(switches::kEnableACPrefetch)},
    {"show-tab-group-in-grid-on-start",
     flag_descriptions::kShowTabGroupInGridOnStartName,
     flag_descriptions::kShowTabGroupInGridOnStartDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kShowTabGroupInGridOnStart)},
    {"ios-tips-notifications-string-alternatives",
     flag_descriptions::kIOSTipsNotificationsStringAlternativesName,
     flag_descriptions::kIOSTipsNotificationsStringAlternativesDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         kIOSTipsNotificationsAlternativeStrings,
         kTipsNotificationsAlternativeStringVariation,
         "IOSTipsNotificationsAlternativeStrings")},
    {"variations-experimental-corpus",
     flag_descriptions::kVariationsExperimentalCorpusName,
     flag_descriptions::kVariationsExperimentalCorpusDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kVariationsExperimentalCorpus)},
    {"variations-restrict-dogfood",
     flag_descriptions::kVariationsRestrictDogfoodName,
     flag_descriptions::kVariationsRestrictDogfoodDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kVariationsRestrictDogfood)},
    {"zero-state-suggestions", flag_descriptions::kZeroStateSuggestionsName,
     flag_descriptions::kZeroStateSuggestionsDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kZeroStateSuggestions,
                                    kZeroStateSuggestionsVariations,
                                    "ZeroStateSuggestions")},
    {"ios-synced-set-up", flag_descriptions::kIOSSyncedSetUpName,
     flag_descriptions::kIOSSyncedSetUpDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIOSSyncedSetUp)},
    {"gemini-floaty-all-pages", flag_descriptions::kGeminiFloatyAllPagesName,
     flag_descriptions::kGeminiFloatyAllPagesDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kGeminiFloatyAllPages)},
    {"gemini-full-chat-history", flag_descriptions::kGeminiFullChatHistoryName,
     flag_descriptions::kGeminiFullChatHistoryDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kGeminiFullChatHistory)},
    {"gemini-loading-state-redesign",
     flag_descriptions::kGeminiLoadingStateRedesignName,
     flag_descriptions::kGeminiLoadingStateRedesignDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kGeminiLoadingStateRedesign)},
    {"gemini-latency-improvement",
     flag_descriptions::kGeminiLatencyImprovementName,
     flag_descriptions::kGeminiLatencyImprovementDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kGeminiLatencyImprovement)},
    {"ios-save-to-drive-client-folder",
     flag_descriptions::kIOSSaveToDriveClientFolderName,
     flag_descriptions::kIOSSaveToDriveClientFolderDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kIOSSaveToDriveClientFolder)},
    {"disable-keyboard-accessory",
     flag_descriptions::kDisableKeyboardAccessoryName,
     flag_descriptions::kDisableKeyboardAccessoryDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kDisableKeyboardAccessory,
                                    kDisableKeyboardAccessoryVariations,
                                    "DisableKeyboardAccessoryVariations")},
    {"enable-fusebox-keyboard-accessory",
     flag_descriptions::kEnableFuseboxKeyboardAccessoryName,
     flag_descriptions::kEnableFuseboxKeyboardAccessoryDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         kEnableFuseboxKeyboardAccessory,
         kEnableFuseboxKeyboardAccessoryVariations,
         "EnableFuseboxKeyboardAccessoryVariations")},
    {"mdm-errors-for-dasher-accounts-handling",
     flag_descriptions::kHandleMdmErrorsForDasherAccountsName,
     flag_descriptions::kHandleMdmErrorsForDasherAccountsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(switches::kHandleMdmErrorsForDasherAccounts)},
    {"location-bar-badge-migration",
     flag_descriptions::kLocationBarBadgeMigrationName,
     flag_descriptions::kLocationBarBadgeMigrationDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kLocationBarBadgeMigration)},
    {"composebox-additional-advanced-tools",
     flag_descriptions::kComposeboxAdditionalAdvancedToolsName,
     flag_descriptions::kComposeboxAdditionalAdvancedToolsDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kComposeboxAdditionalAdvancedTools)},
    {"composebox-close-button-top-align",
     flag_descriptions::kComposeboxCloseButtonTopAlignName,
     flag_descriptions::kComposeboxCloseButtonTopAlignDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kComposeboxCloseButtonTopAlign)},
    {"composebox-compact-mode", flag_descriptions::kComposeboxCompactModeName,
     flag_descriptions::kComposeboxCompactModeDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kComposeboxCompactMode)},
    {"composebox-force-top", flag_descriptions::kComposeboxForceTopName,
     flag_descriptions::kComposeboxForceTopDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kComposeboxForceTop)},
    {"composebox-aim-nudge", flag_descriptions::kComposeboxAIMNudgeName,
     flag_descriptions::kComposeboxAIMNudgeDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kComposeboxAIMNudge)},
    {"composebox-ios", flag_descriptions::kComposeboxIOSName,
     flag_descriptions::kComposeboxIOSDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kComposeboxIOS)},
    {"gemini-navigation-promo", flag_descriptions::kGeminiNavigationPromoName,
     flag_descriptions::kGeminiNavigationPromoDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kGeminiNavigationPromo)},
    {"fre-sign-in-header-text-update",
     flag_descriptions::kFRESignInHeaderTextUpdateName,
     flag_descriptions::kFRESignInHeaderTextUpdateDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kFRESignInHeaderTextUpdate,
                                    kFRESignInHeaderTextUpdateVariations,
                                    "FRESignInHeaderTextUpdate")},
    {"most-visited-tiles-customization-ios",
     flag_descriptions::kMostVisitedTilesCustomizationName,
     flag_descriptions::kMostVisitedTilesCustomizationDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kMostVisitedTilesCustomizationIOS)},
    {"tab-group-color-on-surface",
     flag_descriptions::kTabGroupColorOnSurfaceName,
     flag_descriptions::kTabGroupColorOnSurfaceDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kTabGroupColorOnSurface)},
    {"gemini-live", flag_descriptions::kGeminiLiveName,
     flag_descriptions::kGeminiLiveDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kGeminiLive)},
    {"omnibox-crash-fix-kill-switch",
     flag_descriptions::kOmniboxCrashFixKillSwitchName,
     flag_descriptions::kOmniboxCrashFixKillSwitchDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kOmniboxCrashFixKillSwitch)},
    {"aim-eligibility-service-start-with-profile",
     flag_descriptions::kAIMEligibilityServiceStartWithProfileName,
     flag_descriptions::kAIMEligibilityServiceStartWithProfileDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kAIMEligibilityServiceStartWithProfile)},
    {"lens-omnient-shader-v2-enabled",
     flag_descriptions::kLensOmnientShaderV2EnabledName,
     flag_descriptions::kLensOmnientShaderV2EnabledDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kLensOmnientShaderV2Enabled)},
    {"lens-stream-service-web-channel-transport-enabled",
     flag_descriptions::kLensStreamServiceWebChannelTransportEnabledName,
     flag_descriptions::kLensStreamServiceWebChannelTransportEnabledDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kLensStreamServiceWebChannelTransportEnabled)},
    {"aimntp-entrypoint-tablet", flag_descriptions::kAIMNTPEntrypointTabletName,
     flag_descriptions::kAIMNTPEntrypointTabletDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kAIMNTPEntrypointTablet)},
    {"aim-eligibility-refresh-ntp-modules",
     flag_descriptions::kAIMEligibilityRefreshNTPModulesName,
     flag_descriptions::kAIMEligibilityRefreshNTPModulesDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kAIMEligibilityRefreshNTPModules)},
    {"ios-web-context-menu-new-title",
     flag_descriptions::kIOSWebContextMenuNewTitleName,
     flag_descriptions::kIOSWebContextMenuNewTitleDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIOSWebContextMenuNewTitle)},
    {"composebox-menu-title", flag_descriptions::kComposeboxMenuTitleName,
     flag_descriptions::kComposeboxMenuTitleDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kComposeboxMenuTitle)},
    {"gemini-personalization", flag_descriptions::kGeminiPersonalizationName,
     flag_descriptions::kGeminiPersonalizationDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kGeminiPersonalization)},
    {"gemini-refactored-fre", flag_descriptions::kGeminiRefactoredFREName,
     flag_descriptions::kGeminiRefactoredFREDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kGeminiRefactoredFRE)},
    {"composebox-fetch-contextual-suggestions-for-multiple-attachments",
     flag_descriptions::
         kComposeboxFetchContextualSuggestionsForMultipleAttachmentsName,
     flag_descriptions::
         kComposeboxFetchContextualSuggestionsForMultipleAttachmentsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         kComposeboxFetchContextualSuggestionsForMultipleAttachments)},
    {"composebox-attachments-typed-state",
     flag_descriptions::kComposeboxAttachmentsTypedStateName,
     flag_descriptions::kComposeboxAttachmentsTypedStateDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(omnibox::kComposeboxAttachmentsTypedState)},
    {"password-removal-from-delete-browsing-data",
     flag_descriptions::kPasswordRemovalFromDeleteBrowsingDataName,
     flag_descriptions::kPasswordRemovalFromDeleteBrowsingDataDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kPasswordRemovalFromDeleteBrowsingData)},
    {"close-other-tabs", flag_descriptions::kCloseOtherTabsName,
     flag_descriptions::kCloseOtherTabsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kCloseOtherTabs)},
    {"autofill-enable-wallet-branding",
     flag_descriptions::kAutofillEnableWalletBrandingName,
     flag_descriptions::kAutofillEnableWalletBrandingDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillEnableWalletBranding)},
    {"assistant-container", flag_descriptions::kAssistantContainerName,
     flag_descriptions::kAssistantContainerDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kAssistantContainer)},
    {"composebox-ipad", flag_descriptions::kComposeboxIpadName,
     flag_descriptions::kComposeboxIpadDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kComposeboxIpad)},
    {"chrome-next-ia", flag_descriptions::kChromeNextIaName,
     flag_descriptions::kChromeNextIaDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kChromeNextIa)},
    {"gemini-image-remix-tool", flag_descriptions::kGeminiImageRemixToolName,
     flag_descriptions::kGeminiImageRemixToolDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kGeminiImageRemixTool,
                                    kGeminiImageRemixToolVariations,
                                    "GeminiImageRemixTool")},
    {"composebox-aim-disabled", flag_descriptions::kComposeboxAIMDisabledName,
     flag_descriptions::kComposeboxAIMDisabledDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kComposeboxAIMDisabled)},
    {"enable-new-startup-flow", flag_descriptions::kEnableNewStartupFlowName,
     flag_descriptions::kEnableNewStartupFlowDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableNewStartupFlow)},
    {"gemini-updated-eligibility",
     flag_descriptions::kGeminiUpdatedEligibilityName,
     flag_descriptions::kGeminiUpdatedEligibilityDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kGeminiUpdatedEligibility)},
    {"enable-file-download-connector-ios",
     flag_descriptions::kEnableFileDownloadConnectorIOSName,
     flag_descriptions::kEnableFileDownloadConnectorIOSDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         enterprise_connectors::kEnableFileDownloadConnectorIOS)},
    {"disable-composebox-from-aimntp",
     flag_descriptions::kDisableComposeboxFromAIMNTPName,
     flag_descriptions::kDisableComposeboxFromAIMNTPDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kDisableComposeboxFromAIMNTP)},
    {"ios-save-to-drive-signed-out",
     flag_descriptions::kIOSSaveToDriveSignedOutName,
     flag_descriptions::kIOSSaveToDriveSignedOutDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIOSSaveToDriveSignedOut)},
    {"autofill-enable-bottom-sheet-scan-card-and-fill",
     flag_descriptions::kAutofillEnableBottomSheetScanCardAndFillName,
     flag_descriptions::kAutofillEnableBottomSheetScanCardAndFillDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableBottomSheetScanCardAndFill)},
    {"gemini-response-view-dynamic-resizing",
     flag_descriptions::kGeminiResponseViewDynamicResizingName,
     flag_descriptions::kGeminiResponseViewDynamicResizingDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kGeminiResponseViewDynamicResizing)},
    {"fs-no-broadcast-experiment",
     flag_descriptions::kSmoothScrollingUseDelegateName,
     flag_descriptions::kSmoothScrollingUseDelegateDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::features::kSmoothScrollingUseDelegate)},
    {"model-based-page-classification",
     flag_descriptions::kModelBasedPageClassificationName,
     flag_descriptions::kModelBasedPageClassificationDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kModelBasedPageClassification,
                                    kModelBasedPageClassificationVariations,
                                    "ModelBasedPageClassification")},
    {"page-action-menu-icon", flag_descriptions::kPageActionMenuIconName,
     flag_descriptions::kPageActionMenuIconDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kPageActionMenuIcon,
                                    kPageActionMenuIconVariations,
                                    "PageActionMenuIcon")},
    {"ios-choose-from-drive-signed-out",
     flag_descriptions::kIOSChooseFromDriveSignedOutName,
     flag_descriptions::kIOSChooseFromDriveSignedOutDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kIOSChooseFromDriveSignedOut)},
    {"ios-save-to-photos-signed-out",
     flag_descriptions::kIOSSaveToPhotosSignedOutName,
     flag_descriptions::kIOSSaveToPhotosSignedOutDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIOSSaveToPhotosSignedOut)},
    {"aim-cobrowse", flag_descriptions::kAimCobrowseName,
     flag_descriptions::kAimCobrowseDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kAimCobrowse)},
    {"aim-cobrowse-debug-entrypoint",
     flag_descriptions::kAIMCobrowseDebugEntrypointName,
     flag_descriptions::kAIMCobrowseDebugEntrypointDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kAIMCobrowseDebugEntrypoint)},
    {"ios-date-to-calendar-signed-out",
     flag_descriptions::kIOSDateToCalendarSignedOutName,
     flag_descriptions::kIOSDateToCalendarSignedOutDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kIOSDateToCalendarSignedOut)},
    {"gemini-backend-migration", flag_descriptions::kGeminiBackendMigrationName,
     flag_descriptions::kGeminiBackendMigrationDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kGeminiBackendMigration)},
    {"gemini-actor", flag_descriptions::kGeminiActorName,
     flag_descriptions::kGeminiActorDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kGeminiActor)},
    {"gemini-rich-apc-extraction",
     flag_descriptions::kGeminiRichAPCExtractionName,
     flag_descriptions::kGeminiRichAPCExtractionDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kGeminiRichAPCExtraction)},
    {"in-flow-trusted-vault-key-retrieval-ios",
     flag_descriptions::kInFlowTrustedVaultKeyRetrievalIosName,
     flag_descriptions::kInFlowTrustedVaultKeyRetrievalIosDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         password_manager::features::kInFlowTrustedVaultKeyRetrievalIos)},
    {"sync-themes-ios", flag_descriptions::kSyncThemesIosName,
     flag_descriptions::kSyncThemesIosDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(syncer::kSyncThemesIos)},
    {"disable-u18-feedback-ios", flag_descriptions::kDisableU18FeedbackIosName,
     flag_descriptions::kDisableU18FeedbackIosDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kDisableU18FeedbackIos)},
});

bool SkipConditionalFeatureEntry(const flags_ui::FeatureEntry& entry) {
  return false;
}

flags_ui::FlagsState& GetGlobalFlagsState() {
  static base::NoDestructor<flags_ui::FlagsState> flags_state(kFeatureEntries,
                                                              nullptr);
  return *flags_state;
}
// Creates the experimental test policies map, used by AsyncPolicyLoader and
// PolicyLoaderIOS to locally enable policies.
NSMutableDictionary* CreateExperimentalTestingPolicies() {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];

  // Shared variables for all enterprise experimental flags.
  NSMutableDictionary* testing_policies = [[NSMutableDictionary alloc] init];
  NSMutableArray* allowed_experimental_policies = [[NSMutableArray alloc] init];

  // Set some sample policy values for testing if EnableSamplePolicies is set to
  // true.
  if ([defaults boolForKey:@"EnableSamplePolicies"]) {
    [testing_policies addEntriesFromDictionary:@{
      base::SysUTF8ToNSString(policy::key::kAutofillAddressEnabled) : @NO,

      base::SysUTF8ToNSString(policy::key::kAutofillCreditCardEnabled) : @NO,

      // 2 = Disable all variations
      base::SysUTF8ToNSString(policy::key::kChromeVariations) : @2,

      // 2 = Do not allow any site to show popups
      base::SysUTF8ToNSString(policy::key::kDefaultPopupsSetting) : @2,

      // Set default search engine.
      base::SysUTF8ToNSString(policy::key::kDefaultSearchProviderEnabled) :
          @YES,
      base::SysUTF8ToNSString(policy::key::kDefaultSearchProviderSearchURL) :
          @"http://www.google.com/search?q={searchTerms}",
      base::SysUTF8ToNSString(policy::key::kDefaultSearchProviderName) :
          @"TestEngine",

      base::SysUTF8ToNSString(policy::key::kEditBookmarksEnabled) : @NO,

      base::SysUTF8ToNSString(policy::key::kNTPContentSuggestionsEnabled) : @NO,

      base::SysUTF8ToNSString(policy::key::kTranslateEnabled) : @NO,

      // 2 = Enhanced safe browsing protection
      base::SysUTF8ToNSString(policy::key::kSafeBrowsingProtectionLevel) : @2,

      base::SysUTF8ToNSString(policy::key::kSearchSuggestEnabled) : @YES,

      base::SysUTF8ToNSString(policy::key::kAppStoreRatingEnabled) : @NO,
    }];
  }

  if ([defaults boolForKey:@"EnableSyncDisabledPolicy"]) {
    NSString* sync_policy_key =
        base::SysUTF8ToNSString(policy::key::kSyncDisabled);
    [testing_policies addEntriesFromDictionary:@{sync_policy_key : @YES}];
  }

  // SyncTypesListDisabled policy.
  NSString* Sync_types_list_disabled_key =
      base::SysUTF8ToNSString(policy::key::kSyncTypesListDisabled);
  NSMutableArray* Sync_types_list_disabled_values =
      [[NSMutableArray alloc] init];
  if ([defaults boolForKey:@"SyncTypesListBookmarks"]) {
    [Sync_types_list_disabled_values addObject:@"bookmarks"];
  }
  if ([defaults boolForKey:@"SyncTypesListReadingList"]) {
    [Sync_types_list_disabled_values addObject:@"readingList"];
  }
  if ([defaults boolForKey:@"SyncTypesListPreferences"]) {
    [Sync_types_list_disabled_values addObject:@"preferences"];
  }
  if ([defaults boolForKey:@"SyncTypesListPasswords"]) {
    [Sync_types_list_disabled_values addObject:@"passwords"];
  }
  if ([defaults boolForKey:@"SyncTypesListAutofill"]) {
    [Sync_types_list_disabled_values addObject:@"autofill"];
  }
  if ([defaults boolForKey:@"SyncTypesListTypedUrls"]) {
    [Sync_types_list_disabled_values addObject:@"typedUrls"];
  }
  if ([defaults boolForKey:@"SyncTypesListTabs"]) {
    [Sync_types_list_disabled_values addObject:@"tabs"];
  }
  if ([Sync_types_list_disabled_values count]) {
    [testing_policies addEntriesFromDictionary:@{
      Sync_types_list_disabled_key : Sync_types_list_disabled_values
    }];
  }

  // If an incognito mode availability is set, set the value.
  NSString* incognito_policy_key =
      base::SysUTF8ToNSString(policy::key::kIncognitoModeAvailability);
  NSInteger incognito_mode_availability =
      [defaults integerForKey:incognito_policy_key];
  if (incognito_mode_availability) {
    [testing_policies addEntriesFromDictionary:@{
      incognito_policy_key : @(incognito_mode_availability),
    }];
  }

  NSString* restriction_pattern =
      [defaults stringForKey:@"RestrictAccountsToPatterns"];
  if ([restriction_pattern length] > 0) {
    NSString* restrict_key =
        base::SysUTF8ToNSString(policy::key::kRestrictAccountsToPatterns);
    [testing_policies addEntriesFromDictionary:@{
      restrict_key : @[ restriction_pattern ]
    }];
  }

  // If the sign-in policy is set (not "None"), add the policy key to the list
  // of enabled experimental policies, and set the value.
  NSString* const kSigninPolicyKey = @"BrowserSignin";
  NSInteger signin_policy_mode = [defaults integerForKey:kSigninPolicyKey];
  if (signin_policy_mode) {
    // Remove the mode offset that was used to represent the unset policy.
    --signin_policy_mode;
    DCHECK(signin_policy_mode >= 0);

    [testing_policies addEntriesFromDictionary:@{
      kSigninPolicyKey : @(signin_policy_mode),
    }];
  }

  // If the New Tab Page URL is set (not empty) add the value to the list of
  // test policies.
  NSString* ntp_location = [defaults stringForKey:@"NTPLocation"];
  if ([ntp_location length] > 0) {
    NSString* ntp_location_key =
        base::SysUTF8ToNSString(policy::key::kNewTabPageLocation);
    [testing_policies
        addEntriesFromDictionary:@{ntp_location_key : ntp_location}];
    [allowed_experimental_policies addObject:ntp_location_key];
  }

  if ([defaults boolForKey:@"DisallowChromeDataInBackups"]) {
    NSString* allow_backups_key =
        base::SysUTF8ToNSString(policy::key::kAllowChromeDataInBackups);
    [testing_policies addEntriesFromDictionary:@{allow_backups_key : @NO}];
    [allowed_experimental_policies addObject:allow_backups_key];
  }

  if ([defaults boolForKey:@"DisablePasswordManagerPolicy"]) {
    NSString* password_manager_key =
        base::SysUTF8ToNSString(policy::key::kPasswordManagerEnabled);
    [testing_policies addEntriesFromDictionary:@{password_manager_key : @NO}];
    [allowed_experimental_policies addObject:password_manager_key];
  }

  if ([defaults boolForKey:@"EnableUserPolicyMerge"]) {
    NSString* user_policy_merge_key =
        base::SysUTF8ToNSString(policy::key::kCloudUserPolicyMerge);
    [testing_policies addEntriesFromDictionary:@{user_policy_merge_key : @YES}];
    [allowed_experimental_policies addObject:user_policy_merge_key];
  }

  if ([defaults boolForKey:@"AddManagedBookmarks"]) {
    NSString* managed_bookmarks_key =
        base::SysUTF8ToNSString(policy::key::kManagedBookmarks);
    NSString* managed_bookmarks_value =
        @"["
         // The following gets filtered out from
         // the JSON string when parsed.
         "  {"
         "    \"toplevel_name\": \"Managed Bookmarks\""
         "  },"
         "  {"
         "    \"name\": \"Google\","
         "    \"url\": \"google.com\""
         "  },"
         "  {"
         "    \"name\": \"Empty Folder\","
         "    \"children\": []"
         "  },"
         "  {"
         "    \"name\": \"Big Folder\","
         "    \"children\": ["
         "      {"
         "        \"name\": \"Youtube\","
         "        \"url\": \"youtube.com\""
         "      },"
         "      {"
         "        \"name\": \"Chromium\","
         "        \"url\": \"chromium.org\""
         "      },"
         "      {"
         "        \"name\": \"More Stuff\","
         "        \"children\": ["
         "          {"
         "            \"name\": \"Bugs\","
         "            \"url\": \"crbug.com\""
         "          }"
         "        ]"
         "      }"
         "    ]"
         "  }"
         "]";
    [testing_policies addEntriesFromDictionary:@{
      managed_bookmarks_key : managed_bookmarks_value
    }];
    [allowed_experimental_policies addObject:managed_bookmarks_key];
  }

  // If any experimental policy was allowed, set the EnableExperimentalPolicies
  // policy.
  if ([allowed_experimental_policies count] > 0) {
    [testing_policies setValue:allowed_experimental_policies
                        forKey:base::SysUTF8ToNSString(
                                   policy::key::kEnableExperimentalPolicies)];
  }

  NSString* metrics_reporting_key = @"MetricsReportingEnabled";
  switch ([defaults integerForKey:metrics_reporting_key]) {
    case 1:
      // Metrics reporting forced.
      [testing_policies setValue:@YES forKey:metrics_reporting_key];
      break;
    case 2:
      // Metrics reporting disabled.
      [testing_policies setValue:@NO forKey:metrics_reporting_key];
      break;
    default:
      // Metrics reporting not managed.
      break;
  }

  // Warning: Add new flags to TestingPoliciesHash() below.

  return testing_policies;
}

}  // namespace

// Add all switches from experimental flags to `command_line`.
void AppendSwitchesFromExperimentalSettings(base::CommandLine* command_line) {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];

  // Set the UA flag if UseMobileSafariUA is enabled.
  if ([defaults boolForKey:@"UseMobileSafariUA"]) {
    // Safari uses "Vesion/", followed by the OS version excluding bugfix, where
    // Chrome puts its product token.
    int32_t major = 0;
    int32_t minor = 0;
    int32_t bugfix = 0;
    base::SysInfo::OperatingSystemVersionNumbers(&major, &minor, &bugfix);
    std::string product = base::StringPrintf("Version/%d.%d", major, minor);

    command_line->AppendSwitchASCII(switches::kUserAgent,
                                    web::BuildMobileUserAgent(product));
  }

  // Shared variables for all enterprise experimental flags.
  NSMutableDictionary* testing_policies = CreateExperimentalTestingPolicies();

  // If a CBCM enrollment token is provided, force Chrome Browser Cloud
  // Management to enabled and add the token to the list of policies.
  NSString* token_key =
      base::SysUTF8ToNSString(policy::key::kCloudManagementEnrollmentToken);
  NSString* token = [defaults stringForKey:token_key];

  if ([token length] > 0) {
    command_line->AppendSwitch(switches::kEnableChromeBrowserCloudManagement);
    [testing_policies setValue:token forKey:token_key];
  }

  // If some policies were set, commit them to the app's registration defaults.
  if ([testing_policies count] > 0) {
    NSDictionary* registration_defaults =
        @{kPolicyLoaderIOSConfigurationKey : testing_policies};
    [defaults registerDefaults:registration_defaults];
  }

  // Freeform commandline flags.  These are added last, so that any flags added
  // earlier in this function take precedence.
  if ([defaults boolForKey:@"EnableFreeformCommandLineFlags"]) {
    base::CommandLine::StringVector flags;
    // Append an empty "program" argument.
    flags.push_back("");

    // The number of flags corresponds to the number of text fields in
    // Experimental.plist.
    const int kNumFreeformFlags = 5;
    for (int i = 1; i <= kNumFreeformFlags; ++i) {
      NSString* key =
          [NSString stringWithFormat:@"FreeformCommandLineFlag%d", i];
      NSString* flag = [defaults stringForKey:key];
      if ([flag length]) {
        // iOS keyboard replaces -- with —, so undo that.
        flag = [flag stringByReplacingOccurrencesOfString:@"—"
                                               withString:@"--"
                                                  options:0
                                                    range:NSMakeRange(0, 1)];
        // To make things easier, allow flags with no dashes by prepending them
        // here. This also allows for flags that just have one dash if they
        // exist.
        if (![flag hasPrefix:@"-"]) {
          flag = [@"--" stringByAppendingString:flag];
        }
        flags.push_back(base::SysNSStringToUTF8(flag));
      }
    }

    base::CommandLine temp_command_line(flags);
    command_line->AppendArguments(temp_command_line, false);
  }

  // Populate command line flag for 3rd party keyboard omnibox workaround.
  NSString* enableThirdPartyKeyboardWorkaround =
      [defaults stringForKey:@"EnableThirdPartyKeyboardWorkaround"];
  if ([enableThirdPartyKeyboardWorkaround isEqualToString:@"Enabled"]) {
    command_line->AppendSwitch(switches::kEnableThirdPartyKeyboardWorkaround);
  } else if ([enableThirdPartyKeyboardWorkaround isEqualToString:@"Disabled"]) {
    command_line->AppendSwitch(switches::kDisableThirdPartyKeyboardWorkaround);
  }

  ios::provider::AppendSwitchesFromExperimentalSettings(defaults, command_line);
}

void ConvertFlagsToSwitches(flags_ui::FlagsStorage* flags_storage,
                            base::CommandLine* command_line) {
  GetGlobalFlagsState().ConvertFlagsToSwitches(
      flags_storage, command_line, flags_ui::kAddSentinels,
      switches::kEnableFeatures, switches::kDisableFeatures);
}

std::vector<std::string> RegisterAllFeatureVariationParameters(
    flags_ui::FlagsStorage* flags_storage,
    base::FeatureList* feature_list) {
  return GetGlobalFlagsState().RegisterAllFeatureVariationParameters(
      flags_storage, feature_list);
}

void GetFlagFeatureEntries(flags_ui::FlagsStorage* flags_storage,
                           flags_ui::FlagAccess access,
                           base::ListValue& supported_entries,
                           base::ListValue& unsupported_entries) {
  GetGlobalFlagsState().GetFlagFeatureEntries(
      flags_storage, access, supported_entries, unsupported_entries,
      base::BindRepeating(&SkipConditionalFeatureEntry));
}

void SetFeatureEntryEnabled(flags_ui::FlagsStorage* flags_storage,
                            const std::string& internal_name,
                            bool enable) {
  GetGlobalFlagsState().SetFeatureEntryEnabled(flags_storage, internal_name,
                                               enable);
}

void ResetAllFlags(flags_ui::FlagsStorage* flags_storage) {
  GetGlobalFlagsState().ResetAllFlags(flags_storage);
}

bool IsRestartNeededToCommitChanges() {
  return GetGlobalFlagsState().IsRestartNeededToCommitChanges();
}

namespace testing {

base::span<const flags_ui::FeatureEntry> GetFeatureEntries() {
  return kFeatureEntries;
}

}  // namespace testing
