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
#import "components/enterprise/client_certificates/core/features.h"
#import "components/enterprise/connectors/core/features.h"
#import "components/enterprise/data_controls/core/browser/features.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/feature_list.h"
#import "components/feed/feed_feature_list.h"
#import "components/history/core/browser/features.h"
#import "components/lens/lens_features.h"
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
#import "components/password_manager/ios/features.h"
#import "components/payments/core/features.h"
#import "components/policy/core/common/policy_loader_ios_constants.h"
#import "components/policy/policy_constants.h"
#import "components/regional_capabilities/regional_capabilities_switches.h"
#import "components/safe_browsing/core/common/features.h"
#import "components/safe_browsing/ios/browser/web_ui/features.h"
#import "components/search/ntp_features.h"
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
#import "components/tab_groups/features.h"
#import "components/translate/core/browser/translate_prefs.h"
#import "components/translate/core/common/translate_util.h"
#import "components/variations/net/variations_command_line.h"
#import "components/variations/variations_switches.h"
#import "components/wallet/core/common/wallet_features.h"
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
#import "ios/chrome/browser/enterprise/data_protection/public/features.h"
#import "ios/chrome/browser/first_run/public/features.h"
#import "ios/chrome/browser/flags/chrome_switches.h"
#import "ios/chrome/browser/flags/ios_chrome_flag_descriptions.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/lens/ui_bundled/features.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/omnibox/public/omnibox_ui_features.h"
#import "ios/chrome/browser/page_info/certificate/features/features.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/policy/model/reporting/features.h"
#import "ios/chrome/browser/reader_mode/model/features.h"
#import "ios/chrome/browser/settings/clear_browsing_data/public/features.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_manager_ui_features.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/snapshots/model/features.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_features.h"
#import "ios/chrome/browser/text_selection/model/text_selection_util.h"
#import "ios/chrome/browser/variations/model/ios_chrome_variations_seed_fetcher.h"
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

const FeatureEntry::Choice kSendTabToSelfEnhancedHandoffChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flags_ui::kGenericExperimentChoiceEnabled, switches::kEnableFeatures,
     "SendTabToSelfAutoOpen,"
     "SendTabToSelfImprovedLastActiveLabels,"
     "SendTabToSelfPropagateFormFields,"
     "SendTabToSelfPropagateNavigationHistory,"
     "SendTabToSelfPropagateScrollPosition,"
     "SendTabToSelfPostSendToast"},
    {flags_ui::kGenericExperimentChoiceDisabled, switches::kDisableFeatures,
     "SendTabToSelfAutoOpen,"
     "SendTabToSelfImprovedLastActiveLabels,"
     "SendTabToSelfPropagateFormFields,"
     "SendTabToSelfPropagateNavigationHistory,"
     "SendTabToSelfPropagateScrollPosition,"
     "SendTabToSelfPostSendToast"},
};

const FeatureEntry::Choice
    kWaitThresholdMillisecondsForCapabilitiesApiChoices[] = {
        {flags_ui::kGenericExperimentChoiceDefault, "", ""},
        {"200", signin::kWaitThresholdMillisecondsForCapabilitiesApi, "200"},
        {"500", signin::kWaitThresholdMillisecondsForCapabilitiesApi, "500"},
        {"5000", signin::kWaitThresholdMillisecondsForCapabilitiesApi, "5000"},
};

const FeatureEntry::FeatureParam kActorToolsPageStabilityEnabled[] = {
    {kActorToolsPageStabilityParam, "true"},
};
const FeatureEntry::FeatureVariation kActorToolsPageStabilityVariations[] = {
    {"PageStabilityEnabled", kActorToolsPageStabilityEnabled, nullptr},
};

const FeatureEntry::FeatureParam kPageStabilityMetricsDefault[] = {
    {"PageStabilityIntervalDuration", "4000ms"},
};
const FeatureEntry::FeatureParam kPageStabilityMetricsShorterInterval[] = {
    {"PageStabilityIntervalDuration", "1000ms"},
};

const FeatureEntry::FeatureVariation kPageStabilityMetricsVariations[] = {
    {"Default (4s)", kPageStabilityMetricsDefault, nullptr},
    {"Shorter Interval (1s)", kPageStabilityMetricsShorterInterval, nullptr},
};

const FeatureEntry::FeatureParam kAIMCobrowseHeaderOptionA[] = {
    {kAIMCobrowseHeaderParam, kAIMCobrowseHeaderParamOptionA}};
const FeatureEntry::FeatureParam kAIMCobrowseHeaderOptionB[] = {
    {kAIMCobrowseHeaderParam, kAIMCobrowseHeaderParamOptionB}};
const FeatureEntry::FeatureParam kAIMCobrowseHeaderOptionC[] = {
    {kAIMCobrowseHeaderParam, kAIMCobrowseHeaderParamOptionC}};

const FeatureEntry::FeatureVariation kAIMCobrowseHeaderVariations[] = {
    {"A: Center logo, overflow menu leading", kAIMCobrowseHeaderOptionA,
     nullptr},
    {"B: Left logo with histroy button", kAIMCobrowseHeaderOptionB, nullptr},
    {"C: Left logo with overflow button", kAIMCobrowseHeaderOptionC, nullptr},
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

const FeatureEntry::FeatureParam kBackgroundRefreshRegressionTestControl[] = {
    {"regression_test_arm", "control"}};
const FeatureEntry::FeatureParam kBackgroundRefreshRegressionTestBaseline[] = {
    {"regression_test_arm", "baseline"}};
const FeatureEntry::FeatureParam
    kBackgroundRefreshRegressionTestShortPersistenceDelay[] = {
        {"regression_test_arm", "short-persistence-delay"}};
const FeatureEntry::FeatureParam
    kBackgroundRefreshRegressionTestLongRefreshInterval[] = {
        {"regression_test_arm", "long-refresh-interval"}};
const FeatureEntry::FeatureParam kBackgroundRefreshRegressionTestNoBeacon[] = {
    {"regression_test_arm", "no-beacon"}};

const FeatureEntry::FeatureVariation
    kBackgroundRefreshRegressionTestVariations[] = {
        {"Control", kBackgroundRefreshRegressionTestControl, nullptr},
        {"Baseline", kBackgroundRefreshRegressionTestBaseline, nullptr},
        {"Short Persistence Delay",
         kBackgroundRefreshRegressionTestShortPersistenceDelay, nullptr},
        {"Long Refresh Interval",
         kBackgroundRefreshRegressionTestLongRefreshInterval, nullptr},
        {"No Beacon", kBackgroundRefreshRegressionTestNoBeacon, nullptr}};

const FeatureEntry::FeatureParam kBestOfAppFREArm1[] = {{"variant", "1"}};
const FeatureEntry::FeatureParam kBestOfAppFREArm2[] = {{"variant", "2"}};
const FeatureEntry::FeatureParam kBestOfAppFREArm3[] = {{"variant", "3"}};
const FeatureEntry::FeatureParam kBestOfAppFREArm4[] = {{"variant", "4"}};
const FeatureEntry::FeatureParam kBestOfAppFREArm4Upload[] = {
    {"variant", "4"},
    {"manual_upload_uma", "true"}};

const FeatureEntry::FeatureVariation kBestOfAppFREVariations[] = {
    {" - Variant A: Lens Interactive Promo", kBestOfAppFREArm1, nullptr},
    {" - Variant B: Lens Animated Promo", kBestOfAppFREArm2, nullptr},
    {" - Variant C: Best Features", kBestOfAppFREArm3, nullptr},
    {" - Variant D: Guided Tour", kBestOfAppFREArm4, nullptr},
    {" - Variant D: Guided Tour with manual metric upload",
     kBestOfAppFREArm4Upload, nullptr},
};

const FeatureEntry::FeatureParam kChromeNextIaLensHiddenShareHidden[] = {
    {"chrome_next_ia_lens_icon_visible", "false"},
    {"chrome_next_ia_share_icon_visible", "false"}};

const FeatureEntry::FeatureParam kChromeNextIaLensVisibleShareHidden[] = {
    {"chrome_next_ia_lens_icon_visible", "true"},
    {"chrome_next_ia_share_icon_visible", "false"}};

const FeatureEntry::FeatureParam kChromeNextIaLensHiddenShareVisible[] = {
    {"chrome_next_ia_lens_icon_visible", "false"},
    {"chrome_next_ia_share_icon_visible", "true"}};

const FeatureEntry::FeatureParam kChromeNextIaLensVisibleShareVisible[] = {
    {"chrome_next_ia_lens_icon_visible", "true"},
    {"chrome_next_ia_share_icon_visible", "true"}};

const FeatureEntry::FeatureVariation kChromeNextIaVariations[] = {
    {"Lens Hidden, Share Hidden", kChromeNextIaLensHiddenShareHidden, nullptr},
    {"Lens Visible, Share Hidden", kChromeNextIaLensVisibleShareHidden,
     nullptr},
    {"Lens Hidden, Share Visible", kChromeNextIaLensHiddenShareVisible,
     nullptr},
    {"Lens Visible, Share Visible", kChromeNextIaLensVisibleShareVisible,
     nullptr},
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

const FeatureEntry::FeatureParam kComposeboxConditionalPlusButtonHidePreEdit[] =
    {{kComposeboxConditionalPlusButtonParam, "1"}};

const FeatureEntry::FeatureVariation
    kComposeboxConditionalPlusButtonVariations[] = {
        {"(Hide Plus button in pre-edit)",
         kComposeboxConditionalPlusButtonHidePreEdit, nullptr}};

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
    {" - ESB", kMobilePromoOnDesktopESB, nullptr},
    {" - ESB with push notification", kMobilePromoOnDesktopESBNotification,
     nullptr},
    {" - PW Autofill", kMobilePromoOnDesktopAutofill, nullptr},
    {" - PW Autofill with push notification",
     kMobilePromoOnDesktopAutofillNotification, nullptr},
};

const FeatureEntry::FeatureVariation kMobilePromoOnDesktopWave1Variations[] = {
    {" - Lens Promo", kMobilePromoOnDesktopLens, nullptr},
    {" - Lens Promo with push notification",
     kMobilePromoOnDesktopLensNotification, nullptr},
    {" - Tab Groups", kMobilePromoOnDesktopTabGroups, nullptr},
    {" - Tab Groups with push notification",
     kMobilePromoOnDesktopTabGroupsNotification, nullptr},
    {" - Price Tracking", kMobilePromoOnDesktopPriceTracking, nullptr},
    {" - Price Tracking with push notification",
     kMobilePromoOnDesktopPriceTrackingNotification, nullptr},
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

const FeatureEntry::FeatureParam kGeminiImageRemixToolShowFRERowParam[] = {
    {kGeminiImageRemixToolShowFRERow, "true"}};
const FeatureEntry::FeatureParam
    kGeminiImageRemixToolShowAboveSearchImageParam[] = {
        {kGeminiImageRemixToolShowAboveSearchImage, "true"}};
const FeatureEntry::FeatureParam
    kGeminiImageRemixToolShowBelowSearchImageParam[] = {
        {kGeminiImageRemixToolShowBelowSearchImage, "true"}};
const FeatureEntry::FeatureParam kGeminiImageRemixToolRemovePageContextParam[] =
    {{kGeminiImageRemixToolRemovePageContext, "true"}};

const FeatureEntry::FeatureVariation kGeminiImageRemixToolVariations[] = {
    {"(Show FRE Row)", kGeminiImageRemixToolShowFRERowParam, nullptr},
    {"(Show Above Search Image)",
     kGeminiImageRemixToolShowAboveSearchImageParam, nullptr},
    {"(Show Below Search Image)",
     kGeminiImageRemixToolShowBelowSearchImageParam, nullptr},
    {"(Disable Page Context)", kGeminiImageRemixToolRemovePageContextParam,
     nullptr},
};

const FeatureEntry::FeatureParam kWalletApiPrivatePassesUrl[] = {
    {"wallet_pass_save_url", "https://wallet1ppasses.pa.googleapis.com"}};

const FeatureEntry::FeatureVariation
    kWalletApiPrivatePassesEnabledVariations[] = {
        {"1P URL", kWalletApiPrivatePassesUrl, nullptr}};

const FeatureEntry::FeatureParam kGeminiCopresenceResponseReadyIntervalParam[] =
    {{kGeminiCopresenceResponseReadyInterval, "7.0"}};
const FeatureEntry::FeatureParam
    kGeminiCopresenceWithFullscreenDisablerParam[] = {
        {kGeminiCopresenceWithFullscreenDisabler, "true"}};
const FeatureEntry::FeatureParam kGeminiCopresenceTrackSourcesParam[] = {
    {kGeminiCopresenceTrackSources, "true"}};

const FeatureEntry::FeatureVariation kGeminiCopresenceVariations[] = {
    {"Response Ready Interval", kGeminiCopresenceResponseReadyIntervalParam,
     nullptr},
    {"With Fullscreen Disabler", kGeminiCopresenceWithFullscreenDisablerParam,
     nullptr},
    {"Track Sources", kGeminiCopresenceTrackSourcesParam, nullptr},
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

const FeatureEntry::FeatureParam
    IOSExpandedSetupListVariationParamAllExceptCPE[] = {
        {kIOSExpandedSetupListVariationParam,
         kIOSExpandedSetupListVariationParamAllExceptCPE}};

const FeatureEntry::FeatureParam kIOSExpandedSetupListAll[] = {
    {kIOSExpandedSetupListVariationParam,
     kIOSExpandedSetupListVariationParamAll}};

const FeatureEntry::FeatureVariation kIOSExpandedSetupListVariations[] = {
    {"Safari Data Import", kIOSExpandedSetupListSafariImport, nullptr},
    {"Home Background Customization",
     kIOSExpandedSetupListBackgroundCustomization, nullptr},
    {"Safari Data Import & Home Background Customization (without CPE)",
     IOSExpandedSetupListVariationParamAllExceptCPE, nullptr},
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
const FeatureEntry::FeatureParam kAdjacentForExplainGeminiEditMenu[] = {
    {kExplainGeminiEditMenuParams, "3"}};

const FeatureEntry::FeatureVariation kPositionForExplainGeminiEditMenu[] = {
    {"Explain Gemini shows up after Edit", kAfterEditForExplainGeminiEditMenu,
     nullptr},
    {"Explain Gemini shows up after Search with Google",
     kAfterSearchForExplainGeminiEditMenu, nullptr},
    {"Explain Gemini shows up adjacent to Search with Google",
     kAdjacentForExplainGeminiEditMenu, nullptr}};

const FeatureEntry::FeatureParam kPageActionMenuIconSparkles1[] = {
    {kPageActionMenuIconParams, "1"}};
const FeatureEntry::FeatureParam kPageActionMenuIconSparkles2[] = {
    {kPageActionMenuIconParams, "2"}};

const FeatureEntry::FeatureVariation kPageActionMenuIconVariations[] = {
    {"Sparkles 1", kPageActionMenuIconSparkles1, nullptr},
    {"Sparkles 2", kPageActionMenuIconSparkles2, nullptr}};

const FeatureEntry::FeatureParam kAssistantContainerParamDebugMode[] = {
    {kAssistantContainerParam, kAssistantContainerParamDebug}};

const FeatureEntry::FeatureParam kAssistantContainer30[] = {
    {kAssistantContainerMediumDetentPercentParam, "30"}};

const FeatureEntry::FeatureParam kAssistantContainer60[] = {
    {kAssistantContainerMediumDetentPercentParam, "60"}};

const FeatureEntry::FeatureVariation kAssistantContainerVariations[] = {
    {"with debug enabled", kAssistantContainerParamDebugMode, nullptr},
    {"30% medium detent", kAssistantContainer30, nullptr},
    {"60% medium detent", kAssistantContainer60, nullptr}};

const FeatureEntry::FeatureParam kAutoSubmissionDismissThenSubmit[] = {
    {"auto-submission-type", "DismissThenSubmit"},
};
const FeatureEntry::FeatureParam kAutoSubmissionSubmitThenDismiss[] = {
    {"auto-submission-type", "SubmitThenDismiss"},
};
const FeatureEntry::FeatureParam kAutoSubmissionDismissThenBlockThenSubmit[] = {
    {"auto-submission-type", "DismissThenBlockThenSubmit"},
};
const FeatureEntry::FeatureParam
    kAutoSubmissionDismissThenBlockThenSubmitWithWait[] = {
        {"auto-submission-type", "DismissThenBlockThenSubmit"},
        {"auto-submission-use-wait-period", "100"},
};
const FeatureEntry::FeatureParam kAutoSubmissionDismissThenSubmitWithWait[] = {
    {"auto-submission-type", "DismissThenSubmit"},
    {"auto-submission-use-wait-period", "100"},
};
const FeatureEntry::FeatureParam kAutoSubmissionScriptSubmit[] = {
    {"auto-submission-type", "ScriptSubmit"},
};

const FeatureEntry::FeatureVariation kAutoSubmissionVariations[] = {
    {"Dismiss then Submit", kAutoSubmissionDismissThenSubmit, nullptr},
    {"Submit then Dismiss", kAutoSubmissionSubmitThenDismiss, nullptr},
    {"Dismiss then Block then Submit",
     kAutoSubmissionDismissThenBlockThenSubmit, nullptr},
    {"Dismiss then Block then Submit (Wait)",
     kAutoSubmissionDismissThenBlockThenSubmitWithWait, nullptr},
    {"Dismiss then Submit (Wait)", kAutoSubmissionDismissThenSubmitWithWait,
     nullptr},
    {"Script Submit", kAutoSubmissionScriptSubmit, nullptr},
};

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
    {"build-external-privacy-context",
     flag_descriptions::kBuildExternalPrivacyContextName,
     flag_descriptions::kBuildExternalPrivacyContextDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(switches::kBuildExternalPrivacyContext)},
    {"enforce-can-sign-in-to-chrome-capability",
     flag_descriptions::kEnforceCanSignInToChromeCapabilityName,
     flag_descriptions::kEnforceCanSignInToChromeCapabilityDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(switches::kEnforceCanSignInToChromeCapability)},
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
    {"use-sync-sandbox", flag_descriptions::kSyncSandboxName,
     flag_descriptions::kSyncSandboxDescription, flags_ui::kOsIos,
     SINGLE_VALUE_TYPE_AND_VALUE(
         syncer::kSyncServiceURL,
         "https://chrome-sync.sandbox.google.com/chrome-sync/alpha")},
    {"wallet-api-private-passes-enabled",
     flag_descriptions::kWalletApiPrivatePassesEnabledName,
     flag_descriptions::kWalletApiPrivatePassesEnabledDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         wallet::features::kWalletApiPrivatePassesEnabled,
         kWalletApiPrivatePassesEnabledVariations,
         "WalletApiPrivatePassesEnabled")},
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
    {"ntp-header-use-transforms-for-animations",
     flag_descriptions::kNTPHeaderUseTransformsForAnimationsName,
     flag_descriptions::kNTPHeaderUseTransformsForAnimationsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kNTPHeaderUseTransformsForAnimations)},
    {"ntp-background-color-slider",
     flag_descriptions::kNTPBackgroundColorSliderName,
     flag_descriptions::kNTPBackgroundColorSliderDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kNTPBackgroundColorSlider)},
    {"ntp-background-downsample-image",
     flag_descriptions::kNTPBackgroundDownsampleImageName,
     flag_descriptions::kNTPBackgroundDownsampleImageDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kNTPBackgroundDownsampleImage)},
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
    {"plus-button-in-fakebox", flag_descriptions::kPlusButtonInFakeboxName,
     flag_descriptions::kPlusButtonInFakeboxDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kPlusButtonInFakebox)},
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
    {"force-startup-signin-promo",
     flag_descriptions::kForceStartupSigninPromoName,
     flag_descriptions::kForceStartupSigninPromoDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(switches::kForceStartupSigninPromo)},
    {"shared-highlighting-ios", flag_descriptions::kSharedHighlightingIOSName,
     flag_descriptions::kSharedHighlightingIOSDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kSharedHighlightingIOS)},
    {"snapshot-compressed-jpeg-quality",
     flag_descriptions::kSnapshotCompressedJPEGQualityName,
     flag_descriptions::kSnapshotCompressedJPEGQualityDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kSnapshotCompressedJPEGQuality)},
    {"snapshot-downsample-image",
     flag_descriptions::kSnapshotDownsampleImageName,
     flag_descriptions::kSnapshotDownsampleImageDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kSnapshotDownsampleImage)},
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
    {"tab-grid-new-transitions", flag_descriptions::kTabGridNewTransitionsName,
     flag_descriptions::kTabGridNewTransitionsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kTabGridNewTransitions)},
    {"shopping-list", commerce::flag_descriptions::kShoppingListName,
     commerce::flag_descriptions::kShoppingListDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(commerce::kShoppingList)},
    {"ios-bottom-sheet-migration",
     flag_descriptions::kIOSBottomSheetMigrationName,
     flag_descriptions::kIOSBottomSheetMigrationDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIOSBottomSheetMigration)},
    {"ios-browser-edit-menu-metrics",
     flag_descriptions::kIOSBrowserEditMenuMetricsName,
     flag_descriptions::kIOSBrowserEditMenuMetricsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIOSBrowserEditMenuMetrics)},
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
    {"bwg-precise-location", flag_descriptions::kGeminiPreciseLocationName,
     flag_descriptions::kGeminiPreciseLocationDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kGeminiPreciseLocation)},
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
    {"migrate-ios-keychain-accessibility",
     flag_descriptions::kMigrateIOSKeychainAccessibilityName,
     flag_descriptions::kMigrateIOSKeychainAccessibilityDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(crypto::features::kMigrateIOSKeychainAccessibility)},
    {"send-tab-to-self-enhanced-handoff",
     flag_descriptions::kSendTabToSelfEnhancedHandoffName,
     flag_descriptions::kSendTabToSelfEnhancedHandoffDescription,
     flags_ui::kOsIos, MULTI_VALUE_TYPE(kSendTabToSelfEnhancedHandoffChoices)},
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
    {"app-background-refresh-ios", flag_descriptions::kAppBackgroundRefreshName,
     flag_descriptions::kAppBackgroundRefreshDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableAppBackgroundRefresh)},
    {"background-refresh-regression-test",
     flag_descriptions::kBackgroundRefreshRegressionTestName,
     flag_descriptions::kBackgroundRefreshRegressionTestDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kBackgroundRefreshRegressionTest,
                                    kBackgroundRefreshRegressionTestVariations,
                                    "BackgroundRefreshRegressionTest")},
    {"autofill-support-date-input",
     flag_descriptions::kAutofillSupportDateInputName,
     flag_descriptions::kAutofillSupportDateInputDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kAutofillSupportDateInput)},
    {"autofill-across-iframes", flag_descriptions::kAutofillAcrossIframesName,
     flag_descriptions::kAutofillAcrossIframesDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillAcrossIframesIos)},
    {"enable-trait-collection-registration",
     flag_descriptions::kEnableTraitCollectionRegistrationName,
     flag_descriptions::kEnableTraitCollectionRegistrationDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kEnableTraitCollectionRegistration)},
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
    {"update-tab-group-colors", flag_descriptions::kUpdateTabGroupColorsName,
     flag_descriptions::kUpdateTabGroupColorsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(tab_groups::kUpdateTabGroupColors)},
    {"updated-fre-screens-sequence", flag_descriptions::kUpdatedFRESequenceName,
     flag_descriptions::kUpdatedFRESequenceDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(first_run::kUpdatedFirstRunSequence,
                                    kUpdatedFirstRunSequenceVariations,
                                    "UpdatedFirstRunSequence")},
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
    {"feed-swipe-iph", flag_descriptions::kFeedSwipeInProductHelpName,
     flag_descriptions::kFeedSwipeInProductHelpDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kFeedSwipeInProductHelp,
                                    kFeedSwipeInProductHelpVariations,
                                    "FeedSwipeInProductHelp")},
    {"notification-collision-management",
     flag_descriptions::kNotificationCollisionManagementName,
     flag_descriptions::kNotificationCollisionManagementDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kNotificationCollisionManagement)},
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
    {"page-action-menu-auth-flow",
     flag_descriptions::kPageActionMenuAuthFlowName,
     flag_descriptions::kPageActionMenuAuthFlowDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kPageActionMenuAuthFlow)},
    {"generalized-gemini-entry-flow",
     flag_descriptions::kGeneralizedGeminiEntryFlowName,
     flag_descriptions::kGeneralizedGeminiEntryFlowDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kGeneralizedGeminiEntryFlow)},
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
    {"ios-welcome-back-screen", flag_descriptions::kWelcomeBackName,
     flag_descriptions::kWelcomeBackDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kWelcomeBack,
                                    kWelcomeBackVariations,
                                    "WelcomeBack")},
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
    {"taiyaki-all-surfaces", flag_descriptions::kTaiyakiAllSurfacesName,
     flag_descriptions::kTaiyakiAllSurfacesDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(switches::kTaiyakiAllSurfaces)},
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
    {"lens-enable-urls-in-composeboxes",
     flag_descriptions::kLensEnableSendUrlsInComposeboxesName,
     flag_descriptions::kLensEnableSendUrlsInComposeboxesDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(lens::features::kLensSendUrlsInComposeboxes)},
    {"lens-enable-raw-file-media-types",
     flag_descriptions::kLensEnableSendRawFileMediaTypesName,
     flag_descriptions::kLensEnableSendRawFileMediaTypesDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(lens::features::kLensSendRawFileMediaTypes)},
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
    {"composebox-uses-chrome-compose-client",
     flag_descriptions::kNtpComposeboxUsesChromeComposeClientName,
     flag_descriptions::kNtpComposeboxUsesChromeComposeClientDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(omnibox::kComposeboxUsesChromeComposeClient)},
    {"ios-custom-file-upload-menu",
     flag_descriptions::kIOSCustomFileUploadMenuName,
     flag_descriptions::kIOSCustomFileUploadMenuDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIOSCustomFileUploadMenu)},
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
    {"start-surface-user-setting",
     flag_descriptions::kStartSurfaceUserSettingName,
     flag_descriptions::kStartSurfaceUserSettingDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kStartSurfaceUserSetting)},
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
     FEATURE_VALUE_TYPE(kZeroStateSuggestions)},
    {"zero-state-suggestions-centralization",
     flag_descriptions::kZeroStateSuggestionsCentralizationName,
     flag_descriptions::kZeroStateSuggestionsCentralizationDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kZeroStateSuggestionsCentralization)},
    {"ios-synced-set-up", flag_descriptions::kIOSSyncedSetUpName,
     flag_descriptions::kIOSSyncedSetUpDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIOSSyncedSetUp)},
    {"gemini-floaty-all-pages", flag_descriptions::kGeminiFloatyAllPagesName,
     flag_descriptions::kGeminiFloatyAllPagesDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kGeminiFloatyAllPages)},
    {"disable-keyboard-accessory",
     flag_descriptions::kDisableKeyboardAccessoryName,
     flag_descriptions::kDisableKeyboardAccessoryDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kDisableKeyboardAccessory,
                                    kDisableKeyboardAccessoryVariations,
                                    "DisableKeyboardAccessoryVariations")},
    {"ai-omnibox-ask-placeholder",
     flag_descriptions::kAIOmniboxAskPlaceholderName,
     flag_descriptions::kAIOmniboxAskPlaceholderDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kAIOmniboxAskPlaceholder)},
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
    {"composebox-conditional-plus-button",
     flag_descriptions::kComposeboxConditionalPlusButtonName,
     flag_descriptions::kComposeboxConditionalPlusButtonDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kComposeboxConditionalPlusButton,
                                    kComposeboxConditionalPlusButtonVariations,
                                    "ComposeboxConditionalPlusButton")},
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
    {"gemini-live-dormant-reasons",
     flag_descriptions::kGeminiLiveDormantReasonsName,
     flag_descriptions::kGeminiLiveDormantReasonsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kGeminiLiveDormantReasons)},

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
    {"autofill-enable-wallet-branding",
     flag_descriptions::kAutofillEnableWalletBrandingName,
     flag_descriptions::kAutofillEnableWalletBrandingDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillEnableWalletBranding)},
    {"assistant-container", flag_descriptions::kAssistantContainerName,
     flag_descriptions::kAssistantContainerDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kAssistantContainer,
                                    kAssistantContainerVariations,
                                    "AssistantContainer")},
    {"composebox-ipad", flag_descriptions::kComposeboxIpadName,
     flag_descriptions::kComposeboxIpadDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kComposeboxIpad)},
    {"composebox-plus-button-bottom-sheet",
     flag_descriptions::kComposeboxPlusButtonBottomSheetName,
     flag_descriptions::kComposeboxPlusButtonBottomSheetDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kComposeboxPlusButtonBottomSheet)},
    {"chrome-next-ia", flag_descriptions::kChromeNextIaName,
     flag_descriptions::kChromeNextIaDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kChromeNextIa,
                                    kChromeNextIaVariations,
                                    "ChromeNextIa")},
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
    {"gemini-updated-consent", flag_descriptions::kGeminiUpdatedConsentName,
     flag_descriptions::kGeminiUpdatedConsentDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kGeminiUpdatedConsent)},
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
    {"aim-cobrowse-header", flag_descriptions::kAimCobrowseHeaderName,
     flag_descriptions::kAimCobrowseHeaderDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kAIMCobrowseHeader,
                                    kAIMCobrowseHeaderVariations,
                                    "kAIMCobrowseHeader")},
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
    {"gemini-multi-tab-context", flag_descriptions::kGeminiMultiTabContextName,
     flag_descriptions::kGeminiMultiTabContextDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kGeminiMultiTabContext)},
    {"in-flow-trusted-vault-key-retrieval-ios",
     flag_descriptions::kInFlowTrustedVaultKeyRetrievalIosName,
     flag_descriptions::kInFlowTrustedVaultKeyRetrievalIosDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         password_manager::features::kInFlowTrustedVaultKeyRetrievalIos)},
    {"sync-themes-ios", flag_descriptions::kSyncThemesIosName,
     flag_descriptions::kSyncThemesIosDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(syncer::kSyncThemesIos)},
    {"sync-account-settings", flag_descriptions::kSyncAccountSettingsName,
     flag_descriptions::kSyncAccountSettingsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(syncer::kSyncAccountSettings)},
    {"sync-ai-threads", flag_descriptions::kSyncAIThreadsName,
     flag_descriptions::kSyncAIThreadsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(syncer::kSyncAIThread)},
    {"sync-contextual-task", flag_descriptions::kSyncContextualTaskName,
     flag_descriptions::kSyncContextualTaskDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(syncer::kSyncContextualTask)},
    {"sync-autofill-valuable", flag_descriptions::kSyncAutofillValuableName,
     flag_descriptions::kSyncAutofillValuableDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(syncer::kSyncAutofillValuable)},
    {"sync-autofill-valuable-metadata",
     flag_descriptions::kSyncAutofillValuableMetadataName,
     flag_descriptions::kSyncAutofillValuableMetadataDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(syncer::kSyncAutofillValuableMetadata)},
    {"sync-wallet-flight-reservations",
     flag_descriptions::kSyncWalletFlightReservationsName,
     flag_descriptions::kSyncWalletFlightReservationsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(syncer::kSyncWalletFlightReservations)},
    {"sync-wallet-vehicle-registrations",
     flag_descriptions::kSyncWalletVehicleRegistrationsName,
     flag_descriptions::kSyncWalletVehicleRegistrationsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(syncer::kSyncWalletVehicleRegistrations)},
    {"feedback-entry-points-require-can-submit-feedback-capability",
     flag_descriptions::
         kFeedbackEntryPointsRequireCanSubmitFeedbackCapabilityName,
     flag_descriptions::
         kFeedbackEntryPointsRequireCanSubmitFeedbackCapabilityDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         kFeedbackEntryPointsRequireCanSubmitFeedbackCapability)},
    {"disable-feedback-for-ineligible-users",
     flag_descriptions::kDisableFeedbackForIneligibleUsersName,
     flag_descriptions::kDisableFeedbackForIneligibleUsersDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kDisableFeedbackForIneligibleUsers)},
    {"fullscreen-refactoring", flag_descriptions::kFullscreenRefactoringName,
     flag_descriptions::kFullscreenRefactoringDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kFullscreenRefactoring)},
    {"autofill-ai-available-by-default",
     flag_descriptions::kAutofillAiAvailableByDefaultName,
     flag_descriptions::kAutofillAiAvailableByDefaultDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillAiAvailableByDefault)},
    {"autofill-ai-create-entity-data-manager",
     flag_descriptions::kAutofillAiCreateEntityDataManagerName,
     flag_descriptions::kAutofillAiCreateEntityDataManagerDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillAiCreateEntityDataManager)},
    {"autofill-ai-dedupe-entities",
     flag_descriptions::kAutofillAiDedupeEntitiesName,
     flag_descriptions::kAutofillAiDedupeEntitiesDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillAiDedupeEntities)},
    {"autofill-ai-wallet-flight-reservation",
     flag_descriptions::kAutofillAiWalletFlightReservationName,
     flag_descriptions::kAutofillAiWalletFlightReservationDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillAiWalletFlightReservation)},
    {"autofill-ai-wallet-vehicle-registration",
     flag_descriptions::kAutofillAiWalletVehicleRegistrationName,
     flag_descriptions::kAutofillAiWalletVehicleRegistrationDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillAiWalletVehicleRegistration)},
    {"autofill-ai-wallet-private-passes",
     flag_descriptions::kAutofillAiWalletPrivatePassesName,
     flag_descriptions::kAutofillAiWalletPrivatePassesDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillAiWalletPrivatePasses)},
    {"autofill-ai-wallet-private-passes-deep-link",
     flag_descriptions::kAutofillAiWalletPrivatePassesDeepLinkName,
     flag_descriptions::kAutofillAiWalletPrivatePassesDeepLinkDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillAiWalletPrivatePassesDeepLink)},
    {"autofill-ai-reauth-required",
     flag_descriptions::kAutofillAiReauthRequiredName,
     flag_descriptions::kAutofillAiReauthRequiredDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillAiReauthRequired)},
    {"autofill-ai-no-filling-icons-experiment",
     flag_descriptions::kAutofillAiNoFillingIconsExperimentName,
     flag_descriptions::kAutofillAiNoFillingIconsExperimentDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillAiNoFillingIconsExperiment)},
    {"autofill-ai-order", flag_descriptions::kAutofillAiOrderName,
     flag_descriptions::kAutofillAiOrderDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillAiOrder)},
    {"autofill-ai-shipment", flag_descriptions::kAutofillAiShipmentName,
     flag_descriptions::kAutofillAiShipmentDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillAiShipment)},
    {"autofill-ai-valuables-iph",
     flag_descriptions::kAutofillAiValuablesIPHName,
     flag_descriptions::kAutofillAiValuablesIPHDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(feature_engagement::kIPHAutofillAiValuablesFeature)},
    {"autofill-ai-with-data-schema",
     flag_descriptions::kAutofillAiWithDataSchemaName,
     flag_descriptions::kAutofillAiWithDataSchemaDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillAiWithDataSchema)},
    {"autofill-ambient-autofill",
     flag_descriptions::kAutofillAmbientAutofillName,
     flag_descriptions::kAutofillAmbientAutofillDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillAmbientAutofill)},
    {"gemini-chat-persistence", flag_descriptions::kGeminiChatPersistenceName,
     flag_descriptions::kGeminiChatPersistenceDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kGeminiChatPersistence)},
    {"ios-tab-reminders", flag_descriptions::kIOSTabRemindersName,
     flag_descriptions::kIOSTabRemindersDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(send_tab_to_self::kIOSTabReminders)},
    {"gemini-maps-rich-ui", flag_descriptions::kGeminiMapsRichUIName,
     flag_descriptions::kGeminiMapsRichUIDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kGeminiMapsRichUI)},
    {"gemini-unary-migration", flag_descriptions::kGeminiUnaryMigrationName,
     flag_descriptions::kGeminiUnaryMigrationDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kGeminiUnaryMigration)},
    {"gemini-binary-migration", flag_descriptions::kGeminiBinaryMigrationName,
     flag_descriptions::kGeminiBinaryMigrationDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kGeminiBinaryMigration)},
    {"ios-cobalt", flag_descriptions::kIOSCobaltName,
     flag_descriptions::kIOSCobaltDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::features::kIOSCobalt)},
    {"enable-client-certificate-provisioning-on-ios",
     flag_descriptions::kEnableClientCertificateProvisioningOnIOSName,
     flag_descriptions::kEnableClientCertificateProvisioningOnIOSDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(client_certificates::features::
                            kEnableClientCertificateProvisioningOnIOS)},
    {"ask-about-this-page", flag_descriptions::kAskAboutThisPageName,
     flag_descriptions::kAskAboutThisPageDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kAskAboutThisPage)},
    {"mobile-promo-on-desktop-with-reminder-wave-1",
     flag_descriptions::kMobilePromoOnDesktopWave1Name,
     flag_descriptions::kMobilePromoOnDesktopWave1Description, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kMobilePromoOnDesktopWithReminderWave1,
                                    kMobilePromoOnDesktopWave1Variations,
                                    "MobilePromoOnDesktopWithReminderWave1")},
    {"enable-screenshot-protection-ios",
     flag_descriptions::kEnableScreenshotProtectionIOSName,
     flag_descriptions::kEnableScreenshotProtectionIOSDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kEnableScreenshotProtectionIOS)},
    {"aim-url-navigation-fetch-enabled",
     flag_descriptions::kAimUrlNavigationFetchEnabledName,
     flag_descriptions::kAimUrlNavigationFetchEnabledDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(omnibox::kAimUrlNavigationFetchEnabled)},
    {"reader-mode-ignore-badge-threshold",
     flag_descriptions::kReaderModeIgnoreBadgeThresholdName,
     flag_descriptions::kReaderModeIgnoreBadgeThresholdDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kReaderModeIgnoreBadgeThreshold)},
    {"page-tools-feature-unavailability",
     flag_descriptions::kPageToolsFeatureUnavailabilityName,
     flag_descriptions::kPageToolsFeatureUnavailabilityDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kPageToolsFeatureUnavailability)},
    {"persist-tab-context-rich-extraction",
     flag_descriptions::kPersistTabContextRichExtractionName,
     flag_descriptions::kPersistTabContextRichExtractionDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kPersistTabContextRichExtraction)},
    {"page-context-ipc-optimization",
     flag_descriptions::kPageContextIPCOptimizationName,
     flag_descriptions::kPageContextIPCOptimizationDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kPageContextIPCOptimization)},
    {"autofill-enable-wallet-branding-v2",
     flag_descriptions::kAutofillEnableWalletBrandingV2Name,
     flag_descriptions::kAutofillEnableWalletBrandingV2Description,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillEnableWalletBrandingV2)},
    {"assistant-side-panel", flag_descriptions::kAssistantSidePanelName,
     flag_descriptions::kAssistantSidePanelDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kAssistantSidePanel)},
    {"your-saved-info-settings-page-ios",
     flag_descriptions::kYourSavedInfoSettingsPageIosName,
     flag_descriptions::kYourSavedInfoSettingsPageIosDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kYourSavedInfoSettingsPageIos)},
    {"open-edit-group-view-by-tapping-title",
     flag_descriptions::kOpenEditGroupViewByTappingTitleName,
     flag_descriptions::kOpenEditGroupViewByTappingTitleDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kOpenEditGroupViewByTappingTitle)},
    {"ios-actor-tools", flag_descriptions::kIOSActorToolsName,
     flag_descriptions::kIOSActorToolsDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kActorTools,
                                    kActorToolsPageStabilityVariations,
                                    "ActorTools")},
    {"gemini-client-migration", flag_descriptions::kGeminiClientMigrationName,
     flag_descriptions::kGeminiClientMigrationDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kGeminiClientMigration)},
    {"gemini-screen-context-migration",
     flag_descriptions::kGeminiScreenContextMigrationName,
     flag_descriptions::kGeminiScreenContextMigrationDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kGeminiScreenContextMigration)},
    {"ios-cobalt-developer-mode",
     flag_descriptions::kIOSCobaltDeveloperModeName,
     flag_descriptions::kIOSCobaltDeveloperModeDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::features::kIOSCobaltDeveloperMode)},
    {"cobrowse-aim-history", flag_descriptions::kCobrowseAimHistoryName,
     flag_descriptions::kCobrowseAimHistoryDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kCobrowseAimHistory)},
    {"autofill-upstream-enforce-strike-delay",
     flag_descriptions::kAutofillUpstreamEnforceStrikeDelayName,
     flag_descriptions::kAutofillUpstreamEnforceStrikeDelayDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillUpstreamEnforceStrikeDelay)},
    {"no-accounts-web-signin", flag_descriptions::kNoAccountWebSigninName,
     flag_descriptions::kNoAccountWebSigninDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(switches::kNoAccountWebSignin)},
    {"assistant-aim-minimized-state",
     flag_descriptions::kAssistantAimMinimizedStateName,
     flag_descriptions::kAssistantAimMinimizedStateDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kAssistantAimMinimizedState)},
    {"lens-filter-toggle-enabled",
     flag_descriptions::kLensFilterToggleEnabledName,
     flag_descriptions::kLensFilterToggleEnabledDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kLensFilterToggleEnabled)},
    {"ios-mini-map-universal-links",
     flag_descriptions::kIOSMiniMapUniversalLinkName,
     flag_descriptions::kIOSMiniMapUniversalLinkDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIOSMiniMapUniversalLink)},
    {"cross-device-signin", flag_descriptions::kCrossDeviceSigninName,
     flag_descriptions::kCrossDeviceSigninDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(switches::kCrossDeviceSignin)},
    {"use-ui-graphics-image-renderer-for-fallback-icons",
     flag_descriptions::kUseUIGraphicsImageRendererForFallbackIconsName,
     flag_descriptions::kUseUIGraphicsImageRendererForFallbackIconsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kUseUIGraphicsImageRendererForFallbackIcons)},
    {"password-auto-submission-ios",
     flag_descriptions::kIOSPasswordAutoSubmissionName,
     flag_descriptions::kIOSPasswordAutoSubmissionDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         password_manager::features::kIOSPasswordAutoSubmission,
         kAutoSubmissionVariations,
         "PasswordAutofillAutoSubmission")},
    {"ios-mini-map-universal-links-counterfactual",
     flag_descriptions::kIOSMiniMapUniversalLinkCounterfactualName,
     flag_descriptions::kIOSMiniMapUniversalLinkCounterfactualDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIOSMiniMapUniversalLinkCounterfactual)},
    {"ios-level-up", flag_descriptions::kIOSLevelUpName,
     flag_descriptions::kIOSLevelUpDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIOSLevelUp)},
    {"ios-backend-promo-service-integration",
     flag_descriptions::kIOSBackendPromoServiceIntegrationName,
     flag_descriptions::kIOSBackendPromoServiceIntegrationDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kIOSBackendPromoServiceIntegration)},
    {"data-controls-search-with",
     flag_descriptions::kDataControlsSearchWithName,
     flag_descriptions::kDataControlsSearchWithDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(data_controls::kDataControlsSearchWith)},
    {"page-stability-metrics", flag_descriptions::kPageStabilityMetricsName,
     flag_descriptions::kPageStabilityMetricsDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kPageStabilityMetrics,
                                    kPageStabilityMetricsVariations,
                                    "PageStabilityMetrics")},
    {"actor-service-logging", flag_descriptions::kActorServiceLoggingName,
     flag_descriptions::kActorServiceLoggingDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kActorServiceLogging)},
    {"identity-awareness", flag_descriptions::kIdentityAwarenessName,
     flag_descriptions::kIdentityAwarenessDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIdentityAwareness)},
    {"autofill-enable-gradient-google-logos",
     flag_descriptions::kAutofillEnableGradientGoogleLogosName,
     flag_descriptions::kAutofillEnableGradientGoogleLogosDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableGradientGoogleLogos)},
    {"autofill-ai-wallet-pass-branding-2026",
     flag_descriptions::kAutofillAiWalletPassBranding2026Name,
     flag_descriptions::kAutofillAiWalletPassBranding2026Description,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillAiWalletPassBranding2026)},
    {"ios-mini-map-linkified-address",
     flag_descriptions::kIOSMiniMapLinkifiedAddressName,
     flag_descriptions::kIOSMiniMapLinkifiedAddressDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kIOSMiniMapLinkifiedAddress)},
    {"composebox-drive-context-menu-option",
     flag_descriptions::kComposeboxDriveContextMenuOptionName,
     flag_descriptions::kComposeboxDriveContextMenuOptionDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(omnibox::kComposeboxDriveContextMenuOption)},
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
