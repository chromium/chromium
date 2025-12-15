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
#import "ios/chrome/browser/content_suggestions/ui_bundled/default_browser/public/features.h"
#import "ios/chrome/browser/crash_report/model/features.h"
#import "ios/chrome/browser/credential_provider/model/features.h"
#import "ios/chrome/browser/default_browser/model/features.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/default_promo/ui_bundled/features.h"
#import "ios/chrome/browser/download/ui/features.h"
#import "ios/chrome/browser/first_run/ui_bundled/features.h"
#import "ios/chrome/browser/flags/chrome_switches.h"
#import "ios/chrome/browser/flags/ios_chrome_flag_descriptions.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/features.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/lens/ui_bundled/features.h"
#import "ios/chrome/browser/ntp/model/features.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/omnibox/public/omnibox_ui_features.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/policy/model/reporting/features.h"
#import "ios/chrome/browser/popup_menu/ui_bundled/overflow_menu/feature_flags.h"
#import "ios/chrome/browser/promos_manager/model/features.h"
#import "ios/chrome/browser/reader_mode/model/features.h"
#import "ios/chrome/browser/screen_time/model/screen_time_buildflags.h"
#import "ios/chrome/browser/settings/ui_bundled/google_services/features.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_manager_ui_features.h"
#import "ios/chrome/browser/settings/ui_bundled/privacy/privacy_guide/features.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_features.h"
#import "ios/chrome/browser/text_selection/model/text_selection_util.h"
#import "ios/chrome/browser/web/model/features.h"
#import "ios/chrome/browser/welcome_back/model/features.h"
#import "ios/chrome/browser/whats_new/coordinator/whats_new_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/components/enterprise/data_controls/features.h"
#import "ios/components/security_interstitials/https_only_mode/feature.h"
#import "ios/public/provider/chrome/browser/app_utils/app_utils_api.h"
#import "ios/web/common/features.h"
#import "ios/web/common/user_agent.h"
#import "ios/web/common/web_view_creation_util.h"

#if BUILDFLAG(IOS_SCREEN_TIME_ENABLED)
#import "ios/chrome/browser/screen_time/model/features.h"
#endif

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
     kNTPMIAEntrypointOmniboxContainedSingleButton,
     std::size(kNTPMIAEntrypointOmniboxContainedSingleButton), nullptr},
    {"B: Contained in Omnibox, inline with Voice and Lens",
     kNTPMIAEntrypointOmniboxContainedInline,
     std::size(kNTPMIAEntrypointOmniboxContainedInline), nullptr},
    {"C: Contained in Omnibox, enlarged fakebox",
     kNTPMIAEntrypointOmniboxContainedEnlargedFakebox,
     std::size(kNTPMIAEntrypointOmniboxContainedEnlargedFakebox), nullptr},
    {"D: Contained in enlarged fakebox, without incognito shortcut",
     kNTPMIAEntrypointEnlargedFakeboxNoIncognito,
     std::size(kNTPMIAEntrypointEnlargedFakeboxNoIncognito), nullptr},
    {"E: AIM entry point in quick actions, enlarged fakebox",
     kNTPMIAEntrypointAIMInQuickActions,
     std::size(kNTPMIAEntrypointAIMInQuickActions), nullptr},
};

const FeatureEntry::FeatureParam
    kBottomOmniboxEvolutionEditStateFollowSteadyState[] = {
        {kBottomOmniboxEvolutionParam,
         kBottomOmniboxEvolutionParamEditStateFollowSteadyState}};
const FeatureEntry::FeatureParam
    kBottomOmniboxEvolutionForceBottomOmniboxEditState[] = {
        {kBottomOmniboxEvolutionParam,
         kBottomOmniboxEvolutionParamForceBottomOmniboxEditState}};

const FeatureEntry::FeatureVariation kBottomOmniboxEvolutionVariations[] = {
    {"A: Follows the same position in edit state as the steady state.",
     kBottomOmniboxEvolutionEditStateFollowSteadyState,
     std::size(kBottomOmniboxEvolutionEditStateFollowSteadyState), nullptr},
    {"B: Forces the bottom omnibox position for the edit state",
     kBottomOmniboxEvolutionForceBottomOmniboxEditState,
     std::size(kBottomOmniboxEvolutionForceBottomOmniboxEditState), nullptr},
};

const FeatureEntry::FeatureParam kComposeboxTabPickerVariationCachedAPC[] = {
    {kComposeboxTabPickerVariationParam,
     kComposeboxTabPickerVariationParamCachedAPC}};

const FeatureEntry::FeatureParam kComposeboxTabPickerVariationOnFlightAPC[] = {
    {kComposeboxTabPickerVariationParam,
     kComposeboxTabPickerVariationParamOnFlightAPC}};

const FeatureEntry::FeatureVariation kComposeboxTabPickerVariationVariations[] =
    {
        {"A) Use Cached APC", kComposeboxTabPickerVariationCachedAPC,
         std::size(kComposeboxTabPickerVariationCachedAPC), nullptr},
        {"B) Use On flight APC", kComposeboxTabPickerVariationOnFlightAPC,
         std::size(kComposeboxTabPickerVariationOnFlightAPC), nullptr},
};

const FeatureEntry::FeatureParam kDisableKeyboardAccessoryOnlySymbolsParam[] = {
    {kDisableKeyboardAccessoryParam, kDisableKeyboardAccessoryOnlySymbols}};

const FeatureEntry::FeatureParam kDisableKeyboardAccessoryOnlyFeaturesParam[] =
    {{kDisableKeyboardAccessoryParam, kDisableKeyboardAccessoryOnlyFeatures}};

const FeatureEntry::FeatureParam kDisableKeyboardAccessoryCompletelyParam[] = {
    {kDisableKeyboardAccessoryParam, kDisableKeyboardAccessoryCompletely}};

const FeatureEntry::FeatureVariation kDisableKeyboardAccessoryVariations[] = {
    {"A) only show symbols", kDisableKeyboardAccessoryOnlySymbolsParam,
     std::size(kDisableKeyboardAccessoryOnlySymbolsParam), nullptr},
    {"B) only show lens and voice search",
     kDisableKeyboardAccessoryOnlyFeaturesParam,
     std::size(kDisableKeyboardAccessoryOnlyFeaturesParam), nullptr},
    {"C) disable completely", kDisableKeyboardAccessoryCompletelyParam,
     std::size(kDisableKeyboardAccessoryCompletelyParam), nullptr}};

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
        {"3 matches", kOmniboxUIMaxAutocompleteMatches3,
         std::size(kOmniboxUIMaxAutocompleteMatches3), nullptr},
        {"4 matches", kOmniboxUIMaxAutocompleteMatches4,
         std::size(kOmniboxUIMaxAutocompleteMatches4), nullptr},
        {"5 matches", kOmniboxUIMaxAutocompleteMatches5,
         std::size(kOmniboxUIMaxAutocompleteMatches5), nullptr},
        {"6 matches", kOmniboxUIMaxAutocompleteMatches6,
         std::size(kOmniboxUIMaxAutocompleteMatches6), nullptr},
        {"8 matches", kOmniboxUIMaxAutocompleteMatches8,
         std::size(kOmniboxUIMaxAutocompleteMatches8), nullptr},
        {"10 matches", kOmniboxUIMaxAutocompleteMatches10,
         std::size(kOmniboxUIMaxAutocompleteMatches10), nullptr},
        {"12 matches", kOmniboxUIMaxAutocompleteMatches12,
         std::size(kOmniboxUIMaxAutocompleteMatches12), nullptr}};

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
    {"Promo", kContentPushNotificationsEnabledPromo,
     std::size(kContentPushNotificationsEnabledPromo), nullptr},
    {"Set up list", kContentPushNotificationsEnabledSetupLists,
     std::size(kContentPushNotificationsEnabledSetupLists), nullptr},
    {"Provisional Notification", kContentPushNotificationsEnabledProvisional,
     std::size(kContentPushNotificationsEnabledProvisional), nullptr},
    {"Promo Registeration Only", kContentPushNotificationsPromoRegistrationOnly,
     std::size(kContentPushNotificationsPromoRegistrationOnly), nullptr},
    {"Provisional Notification Registeration Only",
     kContentPushNotificationsProvisionalRegistrationOnly,
     std::size(kContentPushNotificationsProvisionalRegistrationOnly), nullptr},
    {"Set up list Registeration Only",
     kContentPushNotificationsSetUpListRegistrationOnly,
     std::size(kContentPushNotificationsSetUpListRegistrationOnly), nullptr}};

const FeatureEntry::FeatureParam kStartSurfaceTenSeconds[] = {
    {kReturnToStartSurfaceInactiveDurationInSeconds, "10"}};
const FeatureEntry::FeatureParam kStartSurfaceOneHour[] = {
    {kReturnToStartSurfaceInactiveDurationInSeconds, "3600"}};

const FeatureEntry::FeatureVariation kStartSurfaceVariations[] = {
    {"10s:Show Home Surface", kStartSurfaceTenSeconds,
     std::size(kStartSurfaceTenSeconds), nullptr},
    {"1h:Show Home Surface", kStartSurfaceOneHour,
     std::size(kStartSurfaceOneHour), nullptr},
};

const FeatureEntry::FeatureParam kEnableDefaultModel[] = {
    {segmentation_platform::kDefaultModelEnabledParam, "true"}};

const FeatureEntry::FeatureVariation
    kSegmentationPlatformIosModuleRankerVariations[]{
        {"Enabled With Default Model Parameter (Must Set this!)",
         kEnableDefaultModel, std::size(kEnableDefaultModel), nullptr},
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
         std::size(kIOSReactivationNotifications10SecondTrigger), nullptr},
        {"(30s trigger)", kIOSReactivationNotifications30SecondTrigger,
         std::size(kIOSReactivationNotifications30SecondTrigger), nullptr},
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
     std::size(kOneHourIntervalOneHourMaxAgeOnce), nullptr},
    {"4hr Interval 6hr Max Age Once", kFourHourIntervalSixHourMaxAgeOnce,
     std::size(kFourHourIntervalSixHourMaxAgeOnce), nullptr},
    {"1hr Interval 1hr Max Age Recurring",
     kOneHourIntervalOneHourMaxAgeRecurring,
     std::size(kOneHourIntervalOneHourMaxAgeRecurring), nullptr},
    {"4hr Interval 6hr Max Age Recurring",
     kFourHourIntervalSixHourMaxAgeRecurring,
     std::size(kFourHourIntervalSixHourMaxAgeRecurring), nullptr},
    {"Server Driven 1hr Max Age Once", kServerDrivenOneHourMaxAgeOnce,
     std::size(kServerDrivenOneHourMaxAgeOnce), nullptr},
    {"Server Driven 1hr Max Age Recurring", kServerDrivenOneHourMaxAgeRecurring,
     std::size(kServerDrivenOneHourMaxAgeRecurring), nullptr},
    {"Server Driven 6hr Max Age Once", kServerDrivenSixHourMaxAgeOnce,
     std::size(kServerDrivenSixHourMaxAgeOnce), nullptr},
    {"Server Driven 6hr Max Age Recurring", kServerDrivenSixHourMaxAgeRecurring,
     std::size(kServerDrivenSixHourMaxAgeRecurring), nullptr},
};
#endif  // BUILDFLAG(IOS_BACKGROUND_MODE_ENABLED)

// Download List UI feature flag parameters.
// IMPORTANT: These values must match DownloadListUIType enum in features.h
const FeatureEntry::FeatureParam kDownloadListDefaultUIParam[] = {
    {kDownloadListUITypeParam, "0"}};
const FeatureEntry::FeatureParam kDownloadListCustomUIParam[] = {
    {kDownloadListUITypeParam, "1"}};
const FeatureEntry::FeatureVariation kDownloadListVariations[] = {
    {"Default UI", kDownloadListDefaultUIParam,
     std::size(kDownloadListDefaultUIParam), nullptr},
    {"Custom UI", kDownloadListCustomUIParam,
     std::size(kDownloadListCustomUIParam), nullptr},
};

const FeatureEntry::FeatureParam kIOSDockingPromoDisplayedAfterFRE[] = {
    {kIOSDockingPromoExperimentType, "0"}};
const FeatureEntry::FeatureParam kIOSDockingPromoDisplayedAtAppLaunch[] = {
    {kIOSDockingPromoExperimentType, "1"}};
const FeatureEntry::FeatureParam kIOSDockingPromoDisplayedDuringFRE[] = {
    {kIOSDockingPromoExperimentType, "2"}};

const FeatureEntry::FeatureVariation kIOSDockingPromoVariations[] = {
    {"Display promo after FRE", kIOSDockingPromoDisplayedAfterFRE,
     std::size(kIOSDockingPromoDisplayedAfterFRE), nullptr},
    {"Display promo at app launch", kIOSDockingPromoDisplayedAtAppLaunch,
     std::size(kIOSDockingPromoDisplayedAtAppLaunch), nullptr},
    {"Display promo during FRE", kIOSDockingPromoDisplayedDuringFRE,
     std::size(kIOSDockingPromoDisplayedDuringFRE), nullptr}};

const FeatureEntry::FeatureParam kTabResumptionImagesOnlyThumbnail[] = {
    {kTabResumptionImagesTypes, kTabResumptionImagesTypesThumbnails}};
const FeatureEntry::FeatureParam kTabResumptionImagesOnlySalient[] = {
    {kTabResumptionImagesTypes, kTabResumptionImagesTypesSalient}};

const FeatureEntry::FeatureVariation kTabResumptionImagesVariations[] = {
    {"Only thumbnails", kTabResumptionImagesOnlyThumbnail,
     std::size(kTabResumptionImagesOnlyThumbnail), nullptr},
    {"Only salient", kTabResumptionImagesOnlySalient,
     std::size(kTabResumptionImagesOnlySalient), nullptr},
};

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
    {"(Disabled)", kLensFiltersAblationModeDisabled,
     std::size(kLensFiltersAblationModeDisabled), nullptr},
    {"(Post Capture)", kLensFiltersAblationModePostCapture,
     std::size(kLensFiltersAblationModePostCapture), nullptr},
    {"(LVF)", kLensFiltersAblationModeLVF,
     std::size(kLensFiltersAblationModeLVF), nullptr},
    {"(Always)", kLensFiltersAblationModeAlways,
     std::size(kLensFiltersAblationModeAlways), nullptr}};

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
    {"(Disabled)", kLensTranslateToggleModeDisabled,
     std::size(kLensTranslateToggleModeDisabled), nullptr},
    {"(Post Capture)", kLensTranslateToggleModePostCapture,
     std::size(kLensTranslateToggleModePostCapture), nullptr},
    {"(LVF)", kLensTranslateToggleModeLVF,
     std::size(kLensTranslateToggleModeLVF), nullptr},
    {"(Always)", kLensTranslateToggleModeAlways,
     std::size(kLensTranslateToggleModeAlways), nullptr}};

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
         std::size(kMlUrlPiecewiseMappedSearchBlendingAdjustedBy0), nullptr},
        {"demoted by 50", kMlUrlPiecewiseMappedSearchBlendingDemotedBy50,
         std::size(kMlUrlPiecewiseMappedSearchBlendingDemotedBy50), nullptr},
        {"promoted by 50", kMlUrlPiecewiseMappedSearchBlendingPromotedBy50,
         std::size(kMlUrlPiecewiseMappedSearchBlendingPromotedBy50), nullptr},
        {"promoted by 100", kMlUrlPiecewiseMappedSearchBlendingPromotedBy100,
         std::size(kMlUrlPiecewiseMappedSearchBlendingPromotedBy100), nullptr},
        {"mobile mapping", kMlUrlPiecewiseMappedSearchBlendingMobileMapping,
         std::size(kMlUrlPiecewiseMappedSearchBlendingMobileMapping), nullptr},
};

const FeatureEntry::FeatureParam kOmniboxMiaZpsEnabledWithHistoryAblation[] = {
    {OmniboxFieldTrial::kSuppressPsuggestBackfillWithMIAParam, "true"}};
const FeatureEntry::FeatureVariation kOmniboxMiaZpsVariations[] = {
    {"with History Ablation", kOmniboxMiaZpsEnabledWithHistoryAblation,
     std::size(kOmniboxMiaZpsEnabledWithHistoryAblation), nullptr}};

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
    {"Enabled with fixes", kOmniboxMlUrlScoringEnabledWithFixes,
     std::size(kOmniboxMlUrlScoringEnabledWithFixes), nullptr},
    {"unlimited suggestion candidates",
     kOmniboxMlUrlScoringUnlimitedNumCandidates,
     std::size(kOmniboxMlUrlScoringUnlimitedNumCandidates), nullptr},
    {"Increase provider max limit to 10",
     kOmniboxMlUrlScoringMaxMatchesByProvider10,
     std::size(kOmniboxMlUrlScoringMaxMatchesByProvider10), nullptr},
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
    {"Stable", kMlUrlSearchBlendingStable,
     std::size(kMlUrlSearchBlendingStable), nullptr},
    {"Mapped conservative urls", kMlUrlSearchBlendingMappedConservativeUrls,
     std::size(kMlUrlSearchBlendingMappedConservativeUrls), nullptr},
    {"Mapped moderate urls", kMlUrlSearchBlendingMappedModerateUrls,
     std::size(kMlUrlSearchBlendingMappedModerateUrls), nullptr},
    {"Mapped aggressive urls", kMlUrlSearchBlendingMappedAggressiveUrls,
     std::size(kMlUrlSearchBlendingMappedAggressiveUrls), nullptr},
};

const FeatureEntry::FeatureVariation kUrlScoringModelVariations[] = {
    {"Small model", nullptr, 0, "3379590"},
    {"Full model", nullptr, 0, "3380197"},
};

const FeatureEntry::FeatureParam kSafetyCheckNotificationsVerbose[] = {
    {kSafetyCheckNotificationsExperimentType, "0"}};
const FeatureEntry::FeatureParam kSafetyCheckNotificationsSuccinct[] = {
    {kSafetyCheckNotificationsExperimentType, "1"}};

const FeatureEntry::FeatureVariation kSafetyCheckNotificationsVariations[] = {
    {"Display multiple notifications at once", kSafetyCheckNotificationsVerbose,
     std::size(kSafetyCheckNotificationsVerbose), nullptr},
    {"Display one notification at a time", kSafetyCheckNotificationsSuccinct,
     std::size(kSafetyCheckNotificationsSuccinct), nullptr}};

// Contextual Panel flag variations.
const FeatureEntry::FeatureParam kContextualPanelRichIPHArms[] = {
    {"entrypoint-highlight-iph", "true"},
    {"entrypoint-rich-iph", "true"},
};
const FeatureEntry::FeatureParam kContextualPanelSmallIPHArm[] = {
    {"entrypoint-highlight-iph", "false"},
    {"entrypoint-rich-iph", "false"},
};
const FeatureEntry::FeatureParam
    kContextualPanelSmallIPHWithBlueHighlightArm[] = {
        {"entrypoint-highlight-iph", "true"},
        {"entrypoint-rich-iph", "false"},
};

const FeatureEntry::FeatureVariation kContextualPanelEntrypointArmVariations[] =
    {
        {"- Rich IPH", kContextualPanelRichIPHArms,
         std::size(kContextualPanelRichIPHArms), nullptr},
        {"- Small IPH, no blue highlight", kContextualPanelSmallIPHArm,
         std::size(kContextualPanelSmallIPHArm), nullptr},
        {"- Small IPH with blue highlight",
         kContextualPanelSmallIPHWithBlueHighlightArm,
         std::size(kContextualPanelSmallIPHWithBlueHighlightArm), nullptr},
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
    {"Card 3 Price Drop on Tab Resumption", kPriceDropOnTabArm,
     std::size(kPriceDropOnTabArm), nullptr},
    {"Card 4 Price Trackable on Tab Resumption", kPriceTrackableProductOnTabArm,
     std::size(kPriceTrackableProductOnTabArm), nullptr},
    {"Card 5 Tab Resumption with Impression Limits",
     kTabResumptionWithImpressionLimitsArm,
     std::size(kTabResumptionWithImpressionLimitsArm), nullptr},
    {"Card 3 Price Drop on Tab Resumption at front of magic stack",
     kPriceDropOnTabFront, std::size(kPriceDropOnTabFront), nullptr},
    {"Card 4 Price Trackable on Tab Resumption at front of magic stack",
     kPriceTrackableProductOnTabFront,
     std::size(kPriceTrackableProductOnTabFront), nullptr},
    {"Card 5 Tab Resumption with Impression Limits at front of magic stack",
     kTabResumptionWithImpressionLimitsFront,
     std::size(kTabResumptionWithImpressionLimitsFront), nullptr},
    {"Card 6 Price Drop on Tab Resumption with delayed data acquisition",
     kPriceDropOnTabDelayedDataAcquisition,
     std::size(kPriceDropOnTabDelayedDataAcquisition), nullptr},
    {"Card 6 Price Drop on Tab Resumption with delayed data acquisition at "
     "front of magic stack",
     kPriceDropOnTabDelayedDataAcquisitionFront,
     std::size(kPriceDropOnTabDelayedDataAcquisitionFront), nullptr},
};

const FeatureEntry::FeatureVariation kEphemeralCardRankerCardOverrideOptions[] =
    {
        {"- Force Show Price Tracking Notification",
         kPriceTrackingPromoForceShowArm,
         std::size(kPriceTrackingPromoForceShowArm), nullptr},
        {"- Force Hide Price Tracking Notification",
         kPriceTrackingPromoForceHideArm,
         std::size(kPriceTrackingPromoForceHideArm), nullptr},

        // Address Bar Position
        {"- Force Show Address Bar Position Tip",
         kTipsAddressBarPositionForceShowArm,
         std::size(kTipsAddressBarPositionForceShowArm), nullptr},
        {"- Force Hide Address Bar Position Tip",
         kTipsAddressBarPositionForceHideArm,
         std::size(kTipsAddressBarPositionForceHideArm), nullptr},

        // Autofill Passwords
        {"- Force Show Autofill Passwords Tip",
         kTipsAutofillPasswordsForceShowArm,
         std::size(kTipsAutofillPasswordsForceShowArm), nullptr},
        {"- Force Hide Autofill Passwords Tip",
         kTipsAutofillPasswordsForceHideArm,
         std::size(kTipsAutofillPasswordsForceHideArm), nullptr},

        // Enhanced Safe Browsing
        {"- Force Show Enhanced Safe Browsing Tip",
         kTipsEnhancedSafeBrowsingForceShowArm,
         std::size(kTipsEnhancedSafeBrowsingForceShowArm), nullptr},
        {"- Force Hide Enhanced Safe Browsing Tip",
         kTipsEnhancedSafeBrowsingForceHideArm,
         std::size(kTipsEnhancedSafeBrowsingForceHideArm), nullptr},

        // Lens Search
        {"- Force Show Lens Search Tip", kTipsLensSearchForceShowArm,
         std::size(kTipsLensSearchForceShowArm), nullptr},
        {"- Force Hide Lens Search Tip", kTipsLensSearchForceHideArm,
         std::size(kTipsLensSearchForceHideArm), nullptr},

        // Lens Shop
        {"- Force Show Lens Shop Tip", kTipsLensShopForceShowArm,
         std::size(kTipsLensShopForceShowArm), nullptr},
        {"- Force Hide Lens Shop Tip", kTipsLensShopForceHideArm,
         std::size(kTipsLensShopForceHideArm), nullptr},

        // Lens Translate
        {"- Force Show Lens Translate Tip", kTipsLensTranslateForceShowArm,
         std::size(kTipsLensTranslateForceShowArm), nullptr},
        {"- Force Hide Lens Translate Tip", kTipsLensTranslateForceHideArm,
         std::size(kTipsLensTranslateForceHideArm), nullptr},

        // Save Passwords
        {"- Force Show Save Passwords Tip", kTipsSavePasswordsForceShowArm,
         std::size(kTipsSavePasswordsForceShowArm), nullptr},
        {"- Force Hide Save Passwords Tip", kTipsSavePasswordsForceHideArm,
         std::size(kTipsSavePasswordsForceHideArm), nullptr},

        // Send Tab Promo.
        {"- Force Show Send Tab Promo", kSendTabPromoForceShowArm,
         std::size(kSendTabPromoForceShowArm), nullptr},
        {"- Force Hide Send Tab Promo", kSendTabPromoForceHideArm,
         std::size(kSendTabPromoForceHideArm), nullptr},

        // App Bundle Promo.
        {"- Force Show App Bundle Promo", kAppBundlePromoForceShowArm,
         std::size(kAppBundlePromoForceShowArm), nullptr},
        {"- Force Hide App Bundle Promo", kAppBundlePromoForceHideArm,
         std::size(kAppBundlePromoForceHideArm), nullptr},

        // Default Browser Promo.
        {"- Force Show Default Browser Promo", kDefaultBrowserPromoForceShowArm,
         std::size(kDefaultBrowserPromoForceShowArm), nullptr},
        {"- Force Hide Default Browser Promo", kDefaultBrowserPromoForceHideArm,
         std::size(kDefaultBrowserPromoForceHideArm), nullptr},
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
         kSendTabIOSPushNotificationsWithMagicStackCard,
         std::size(kSendTabIOSPushNotificationsWithMagicStackCard), nullptr},
        {"With URL Image", kSendTabIOSPushNotificationsWithURLImage,
         std::size(kSendTabIOSPushNotificationsWithURLImage), nullptr},
        {"With Tab Reminders", kSendTabIOSPushNotificationsWithTabReminders,
         std::size(kSendTabIOSPushNotificationsWithTabReminders), nullptr},
};

// Soft Lock
const FeatureEntry::FeatureParam kIOSSoftLockNoDelay[] = {
    {kIOSSoftLockBackgroundThresholdParam, "0m"},
};

const FeatureEntry::FeatureVariation kIOSSoftLockVariations[] = {
    {" - No delay", kIOSSoftLockNoDelay, std::size(kIOSSoftLockNoDelay),
     nullptr}};

const FeatureEntry::FeatureParam
    kIOSStartTimeStartupRemediationsSaveNTPWebStateArm[] = {
        {kIOSStartTimeStartupRemediationsSaveNTPWebState, "true"},
};
const FeatureEntry::FeatureVariation
    kIOSStartTimeStartupRemediationsVariations[] = {
        {" - Save NTP Web State",
         kIOSStartTimeStartupRemediationsSaveNTPWebStateArm,
         std::size(kIOSStartTimeStartupRemediationsSaveNTPWebStateArm),
         nullptr}};

constexpr flags_ui::FeatureEntry::FeatureParam
    kAutofillThrottleDocFormScanShortPeriodParam[] = {{"period-ms", "250"}};
constexpr flags_ui::FeatureEntry::FeatureParam
    kAutofillThrottleDocFormScanMediumPeriodParam[] = {{"period-ms", "500"}};
constexpr flags_ui::FeatureEntry::FeatureParam
    kAutofillThrottleDocFormScanLongPeriodParam[] = {{"period-ms", "1000"}};
constexpr flags_ui::FeatureEntry::FeatureVariation
    kAutofillThrottleDocFormScanVariations[] = {
        {"Short period", kAutofillThrottleDocFormScanShortPeriodParam,
         std::size(kAutofillThrottleDocFormScanShortPeriodParam), nullptr},
        {"Medium period", kAutofillThrottleDocFormScanMediumPeriodParam,
         std::size(kAutofillThrottleDocFormScanMediumPeriodParam), nullptr},
        {"Long period", kAutofillThrottleDocFormScanLongPeriodParam,
         std::size(kAutofillThrottleDocFormScanLongPeriodParam), nullptr}};

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
         std::size(kAutofillThrottleFilteredDocFormScanShortPeriodParam),
         nullptr},
        {"Medium period", kAutofillThrottleFilteredDocFormScanMediumPeriodParam,
         std::size(kAutofillThrottleFilteredDocFormScanMediumPeriodParam),
         nullptr},
        {"Long period", kAutofillThrottleFilteredDocFormScanLongPeriodParam,
         std::size(kAutofillThrottleFilteredDocFormScanLongPeriodParam),
         nullptr}};

const FeatureEntry::FeatureParam
    kIOSStartTimeBackgroundRemediationsAvoidNTPCleanupArm[] = {
        {kIOSStartTimeBackgroundRemediationsAvoidNTPCleanup, "true"},
        {kIOSStartTimeBrowserBackgroundRemediationsUpdateFeedRefresh, "false"}};
const FeatureEntry::FeatureParam
    kIOSStartTimeBrowserBackgroundRemediationsUpdateFeedRefreshArm[] = {
        {kIOSStartTimeBrowserBackgroundRemediationsUpdateFeedRefresh, "true"},
        {kIOSStartTimeBackgroundRemediationsAvoidNTPCleanup, "false"}};
const FeatureEntry::FeatureVariation
    kIOSStartTimeBrowserBackgroundRemediationsVariations[] = {
        {" - Avoid NTP Cleanup",
         kIOSStartTimeBackgroundRemediationsAvoidNTPCleanupArm,
         std::size(kIOSStartTimeBackgroundRemediationsAvoidNTPCleanupArm),
         nullptr},
        {" - Update Feed Refresh",
         kIOSStartTimeBrowserBackgroundRemediationsUpdateFeedRefreshArm,
         std::size(
             kIOSStartTimeBrowserBackgroundRemediationsUpdateFeedRefreshArm),
         nullptr}};

const FeatureEntry::FeatureParam kSetUpListDuration3Days[] = {
    {set_up_list::kSetUpListDurationParam, "2"}};
const FeatureEntry::FeatureParam kSetUpListDuration5Days[] = {
    {set_up_list::kSetUpListDurationParam, "4"}};
const FeatureEntry::FeatureParam kSetUpListDuration7Days[] = {
    {set_up_list::kSetUpListDurationParam, "6"}};

const FeatureEntry::FeatureVariation kSetUpListDurationVariations[] = {
    {" - 3 Days", kSetUpListDuration3Days, std::size(kSetUpListDuration3Days),
     nullptr},
    {" - 5 Days", kSetUpListDuration5Days, std::size(kSetUpListDuration5Days),
     nullptr},
    {" - 7 Days", kSetUpListDuration7Days, std::size(kSetUpListDuration7Days),
     nullptr}};

const FeatureEntry::FeatureParam kUpdatedFirstRunSequenceArm1[] = {
    {first_run::kUpdatedFirstRunSequenceParam, "1"}};
const FeatureEntry::FeatureParam kUpdatedFirstRunSequenceArm2[] = {
    {first_run::kUpdatedFirstRunSequenceParam, "2"}};
const FeatureEntry::FeatureParam kUpdatedFirstRunSequenceArm3[] = {
    {first_run::kUpdatedFirstRunSequenceParam, "3"}};

const FeatureEntry::FeatureVariation kUpdatedFirstRunSequenceVariations[] = {
    {" - Default browser promo first", kUpdatedFirstRunSequenceArm1,
     std::size(kUpdatedFirstRunSequenceArm1), nullptr},
    {" - Remove sign in & sync conditionally", kUpdatedFirstRunSequenceArm2,
     std::size(kUpdatedFirstRunSequenceArm2), nullptr},
    {" - DB promo first and remove sign in & sync",
     kUpdatedFirstRunSequenceArm3, std::size(kUpdatedFirstRunSequenceArm3),
     nullptr}};

const FeatureEntry::FeatureParam
    kYoutubeIncognitoErrorHandlingWithoutIncognitoInterstitial[] = {
        {kYoutubeIncognitoErrorHandlingWithoutIncognitoInterstitialParam,
         "true"},
};

const FeatureEntry::FeatureParam kYoutubeIncognitoTargetAllowListed[] = {
    {kYoutubeIncognitoTargetApps, kYoutubeIncognitoTargetAppsAllowlisted},
};
const FeatureEntry::FeatureParam kYoutubeIncognitoTargetFirstParty[] = {
    {kYoutubeIncognitoTargetApps, kYoutubeIncognitoTargetAppsFirstParty},
};
const FeatureEntry::FeatureParam kYoutubeIncognitoTargetAll[] = {
    {kYoutubeIncognitoTargetApps, kYoutubeIncognitoTargetAppsAll},
};

const FeatureEntry::FeatureVariation kYoutubeIncognitoVariations[] = {
    {"Error handling without Incognito Interstitial",
     kYoutubeIncognitoErrorHandlingWithoutIncognitoInterstitial,
     std::size(kYoutubeIncognitoErrorHandlingWithoutIncognitoInterstitial),
     nullptr},
    {"Enable for listed apps", kYoutubeIncognitoTargetAllowListed,
     std::size(kYoutubeIncognitoTargetAllowListed), nullptr},
    {"Enable for first party apps", kYoutubeIncognitoTargetFirstParty,
     std::size(kYoutubeIncognitoTargetFirstParty), nullptr},
    {"Enable for all apps", kYoutubeIncognitoTargetAll,
     std::size(kYoutubeIncognitoTargetAll), nullptr},
};

const FeatureEntry::FeatureParam kSlowFullscreenTransitionSpeed[] = {
    {kFullscreenTransitionSpeedParam, "0"}};
const FeatureEntry::FeatureParam kDefaultFullscreenTransitionSpeed[] = {
    {kFullscreenTransitionSpeedParam, "1"}};
const FeatureEntry::FeatureParam kFastFullscreenTransitionSpeed[] = {
    {kFullscreenTransitionSpeedParam, "2"}};

const FeatureEntry::FeatureVariation kFullscreenTransitionVariations[] = {
    {"Slow speed", kSlowFullscreenTransitionSpeed,
     std::size(kSlowFullscreenTransitionSpeed), nullptr},
    {"Default speed", kDefaultFullscreenTransitionSpeed,
     std::size(kDefaultFullscreenTransitionSpeed), nullptr},
    {"Fast speed", kFastFullscreenTransitionSpeed,
     std::size(kFastFullscreenTransitionSpeed), nullptr}};

const FeatureEntry::FeatureParam kFullscreenScrollThreshold1[] = {
    {web::features::kFullscreenScrollThresholdAmount, "1"}};
const FeatureEntry::FeatureParam kFullscreenScrollThreshold5[] = {
    {web::features::kFullscreenScrollThresholdAmount, "5"}};
const FeatureEntry::FeatureParam kFullscreenScrollThreshold10[] = {
    {web::features::kFullscreenScrollThresholdAmount, "10"}};
const FeatureEntry::FeatureParam kFullscreenScrollThreshold20[] = {
    {web::features::kFullscreenScrollThresholdAmount, "20"}};
const FeatureEntry::FeatureVariation kFullscreenScrollThresholdVariations[] = {
    {"1px", kFullscreenScrollThreshold1, std::size(kFullscreenScrollThreshold1),
     nullptr},
    {"5px", kFullscreenScrollThreshold5, std::size(kFullscreenScrollThreshold5),
     nullptr},
    {"10px", kFullscreenScrollThreshold10,
     std::size(kFullscreenScrollThreshold10), nullptr},
    {"20px", kFullscreenScrollThreshold20,
     std::size(kFullscreenScrollThreshold20), nullptr}};

const FeatureEntry::FeatureParam kAnimatedDBPInFREWithActionButtons[] = {
    {first_run::kAnimatedDefaultBrowserPromoInFREExperimentType, "0"}};
const FeatureEntry::FeatureParam kAnimatedDBPInFREWithShowMeHow[] = {
    {first_run::kAnimatedDefaultBrowserPromoInFREExperimentType, "1"}};
const FeatureEntry::FeatureParam kAnimatedDBPInFREWithInstructions[] = {
    {first_run::kAnimatedDefaultBrowserPromoInFREExperimentType, "2"}};

const FeatureEntry::FeatureVariation
    kAnimatedDefaultBrowserPromoInFREVariations[] = {
        {" - with Action Buttons", kAnimatedDBPInFREWithActionButtons,
         std::size(kAnimatedDBPInFREWithActionButtons), nullptr},
        {" - with Show Me How", kAnimatedDBPInFREWithShowMeHow,
         std::size(kAnimatedDBPInFREWithShowMeHow), nullptr},
        {" - with Instructions", kAnimatedDBPInFREWithInstructions,
         std::size(kAnimatedDBPInFREWithInstructions), nullptr}};

const FeatureEntry::FeatureParam kBestFeaturesScreenInFirstRunArm1[] = {
    {first_run::kBestFeaturesScreenInFirstRunParam, "1"}};
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
      kUpdatedFirstRunSequenceArm1,
      std::size(kBestFeaturesScreenInFirstRunArm1), nullptr},
     {" - Variant B: General screen, before DB promo",
      kBestFeaturesScreenInFirstRunArm2,
      std::size(kBestFeaturesScreenInFirstRunArm2), nullptr},
     {" - Variant C: General screen with passwords item",
      kBestFeaturesScreenInFirstRunArm3,
      std::size(kBestFeaturesScreenInFirstRunArm3), nullptr},
     {" - Variant D: Shopping users screen, variant C as fallback",
      kBestFeaturesScreenInFirstRunArm4,
      std::size(kBestFeaturesScreenInFirstRunArm4), nullptr},
     {" - Variant E: Show screen to signed-in users only",
      kBestFeaturesScreenInFirstRunArm5,
      std::size(kBestFeaturesScreenInFirstRunArm5), nullptr},
     {" - Variant F: Show address bar promo", kBestFeaturesScreenInFirstRunArm6,
      std::size(kBestFeaturesScreenInFirstRunArm6), nullptr}};

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
      kIOSOneTapMiniMapRestrictionCrossValidate,
      std::size(kIOSOneTapMiniMapRestrictionCrossValidate), nullptr},
     {"Confidence Level (0.999)", kIOSOneTapMiniMapRestrictionThreshold999,
      std::size(kIOSOneTapMiniMapRestrictionThreshold999), nullptr},
     {"Minimum address length (20 chars)",
      kIOSOneTapMiniMapRestrictionMinLength20,
      std::size(kIOSOneTapMiniMapRestrictionMinLength20), nullptr},
     {"Maximum sections (6)", kIOSOneTapMiniMapRestrictionMaxSections6,
      std::size(kIOSOneTapMiniMapRestrictionMaxSections6), nullptr},
     {"Longest word length (4)", kIOSOneTapMiniMapRestrictionLongWords4,
      std::size(kIOSOneTapMiniMapRestrictionLongWords4), nullptr},
     {"Proportion of alnum chars (60%)",
      kIOSOneTapMiniMapRestrictionMinAlphaNum60,
      std::size(kIOSOneTapMiniMapRestrictionMinAlphaNum60), nullptr}};

const FeatureEntry::FeatureParam kFeedSwipeInProductHelpStaticInFirstRun[] = {
    {kFeedSwipeInProductHelpArmParam, "1"}};
const FeatureEntry::FeatureParam kFeedSwipeInProductHelpStaticInSecondRun[] = {
    {kFeedSwipeInProductHelpArmParam, "2"}};
const FeatureEntry::FeatureParam kFeedSwipeInProductHelpAnimated[] = {
    {kFeedSwipeInProductHelpArmParam, "3"}};

const FeatureEntry::FeatureVariation kFeedSwipeInProductHelpVariations[] = {
    {" - Static IPH after the FRE", kFeedSwipeInProductHelpStaticInFirstRun,
     std::size(kFeedSwipeInProductHelpStaticInFirstRun), nullptr},
    {"- Static IPH after the second run",
     kFeedSwipeInProductHelpStaticInSecondRun,
     std::size(kFeedSwipeInProductHelpStaticInSecondRun), nullptr},
    {"- Animated IPH", kFeedSwipeInProductHelpAnimated,
     std::size(kFeedSwipeInProductHelpAnimated), nullptr}};

// LINT.IfChange(AutofillVcnEnrollStrikeExpiryTime)
const FeatureEntry::FeatureParam kAutofillVcnEnrollStrikeExpiryTime_120Days[] =
    {{"autofill_vcn_strike_expiry_time_days", "120"}};

const FeatureEntry::FeatureParam kAutofillVcnEnrollStrikeExpiryTime_60Days[] = {
    {"autofill_vcn_strike_expiry_time_days", "60"}};

const FeatureEntry::FeatureParam kAutofillVcnEnrollStrikeExpiryTime_30Days[] = {
    {"autofill_vcn_strike_expiry_time_days", "30"}};

const FeatureEntry::FeatureVariation
    kAutofillVcnEnrollStrikeExpiryTimeOptions[] = {
        {"120 days", kAutofillVcnEnrollStrikeExpiryTime_120Days,
         std::size(kAutofillVcnEnrollStrikeExpiryTime_120Days), nullptr},
        {"60 days", kAutofillVcnEnrollStrikeExpiryTime_60Days,
         std::size(kAutofillVcnEnrollStrikeExpiryTime_60Days), nullptr},
        {"30 days", kAutofillVcnEnrollStrikeExpiryTime_30Days,
         std::size(kAutofillVcnEnrollStrikeExpiryTime_30Days), nullptr}};
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
    {" - Variant A: Basics with Locked Incognito", kWelcomeBackArm1,
     std::size(kWelcomeBackArm1), nullptr},
    {" - Variant B: Basics with Save & Autofill Passwords", kWelcomeBackArm2,
     std::size(kWelcomeBackArm2), nullptr},
    {" - Variant C: Productivity & Shopping", kWelcomeBackArm3,
     std::size(kWelcomeBackArm3), nullptr},
    {" - Variant D: Sign-in Benefits", kWelcomeBackArm4,
     std::size(kWelcomeBackArm4), nullptr},
};

const FeatureEntry::FeatureParam kBestOfAppFREArm1[] = {{"variant", "1"}};
const FeatureEntry::FeatureParam kBestOfAppFREArm2[] = {{"variant", "2"}};
const FeatureEntry::FeatureParam kBestOfAppFREArm4[] = {{"variant", "4"}};
const FeatureEntry::FeatureParam kBestOfAppFREArm4Upload[] = {
    {"variant", "4"},
    {"manual_upload_uma", "true"}};

const FeatureEntry::FeatureVariation kBestOfAppFREVariations[] = {
    {" - Variant A: Lens Interactive Promo", kBestOfAppFREArm1,
     std::size(kBestOfAppFREArm1), nullptr},
    {" - Variant A: Lens Animated Promo", kBestOfAppFREArm2,
     std::size(kBestOfAppFREArm2), nullptr},
    {" - Variant D: Guided Tour", kBestOfAppFREArm4,
     std::size(kBestOfAppFREArm4), nullptr},
    {" - Variant D: Guided Tour with manual metric upload",
     kBestOfAppFREArm4Upload, std::size(kBestOfAppFREArm4Upload), nullptr},
};

const FeatureEntry::FeatureParam
    kInvalidateChoiceOnRestoreIsRetroactiveOption[] = {
        {"is_retroactive", "true"}};
const FeatureEntry::FeatureVariation
    kInvalidateSearchEngineChoiceOnRestoreVariations[] = {
        {"(retroactive)", kInvalidateChoiceOnRestoreIsRetroactiveOption,
         std::size(kInvalidateChoiceOnRestoreIsRetroactiveOption), nullptr}};

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
     kSingleScreenForBWGPromoConsent,
     std::size(kSingleScreenForBWGPromoConsent), nullptr},
    {"Double screen for BWG Promo Consent Flow",
     kDoubleScreenForBWGPromoConsent,
     std::size(kDoubleScreenForBWGPromoConsent), nullptr},
    {"Skip FRE", kSkipBWGPromoConsent, std::size(kSkipBWGPromoConsent),
     nullptr},
    {"Force FRE", kForceBWGFirstTimeRun, std::size(kForceBWGFirstTimeRun),
     nullptr},
    {"Skip new user delay", kSkipNewUserDelay, std::size(kSkipNewUserDelay)}};

const FeatureEntry::FeatureParam kOmniboxMobileParityEnableFeedForGoogleOnly[] =
    {{OmniboxFieldTrial::kMobileParityEnableFeedForGoogleOnly.name, "true"}};
const FeatureEntry::FeatureVariation kOmniboxMobileParityVariations[] = {
    {"- feed only when searching with Google",
     kOmniboxMobileParityEnableFeedForGoogleOnly,
     std::size(kOmniboxMobileParityEnableFeedForGoogleOnly), nullptr}};

const FeatureEntry::FeatureParam kPageActionMenuDirectEntryPoint[] = {
    {kPageActionMenuDirectEntryPointParam, "true"},
};
const FeatureEntry::FeatureParam kPageActionMenuBWGSessionValidityDuration[] = {
    {kBWGSessionValidityDurationParam, "1"}};
const FeatureEntry::FeatureVariation kPageActionMenuVariations[] = {
    {"Direct Entry Point", kPageActionMenuDirectEntryPoint,
     std::size(kPageActionMenuDirectEntryPoint), nullptr},
    {"1 min session validity duration",
     kPageActionMenuBWGSessionValidityDuration,
     std::size(kPageActionMenuBWGSessionValidityDuration), nullptr},
};

const FeatureEntry::FeatureParam kAskGeminiChipUseSnackbarVariation[] = {
    {kAskGeminiChipUseSnackbar, "true"},
};
const FeatureEntry::FeatureParam kAskGeminiChipIgnoreCriteriaVariation[] = {
    {kAskGeminiChipIgnoreCriteria, "true"},
};
const FeatureEntry::FeatureParam kAskGeminiChipPrepopulateFloatyVariation[] = {
    {kAskGeminiChipPrepopulateFloaty, "true"},
};
const FeatureEntry::FeatureVariation kAskGeminiChipVariations[] = {
    {"Use Snackbar", kAskGeminiChipUseSnackbarVariation,
     std::size(kAskGeminiChipUseSnackbarVariation), nullptr},
    {"Ignore FET and Time Criteria", kAskGeminiChipIgnoreCriteriaVariation,
     std::size(kAskGeminiChipIgnoreCriteriaVariation), nullptr},
    {"Prepopulate Floaty", kAskGeminiChipPrepopulateFloatyVariation,
     std::size(kAskGeminiChipPrepopulateFloatyVariation), nullptr},
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

const FeatureEntry::FeatureParam
    kOmniboxAimShortcutTypedStateEnabledForTypedLength15[] = {
        {OmniboxFieldTrial::kMinimumTypedCharactersToInvokeAimShortcut.name,
         "15"}};
const FeatureEntry::FeatureVariation kOmniboxAimShortcutTypedStateVariations[] =
    {{"for 15+ chars", kOmniboxAimShortcutTypedStateEnabledForTypedLength15,
      std::size(kOmniboxAimShortcutTypedStateEnabledForTypedLength15),
      nullptr}};

const FeatureEntry::FeatureParam kComposeboxDevToolsForceFailure[] = {
    {kForceUploadFailureParam, "true"}};
const FeatureEntry::FeatureParam kComposeboxDevToolsSlowLoad[] = {
    {kImageLoadDelayMsParam, "1000"}};
const FeatureEntry::FeatureParam kComposeboxDevToolsSlowUpload[] = {
    {kUploadDelayMsParam, "3000"}};

const FeatureEntry::FeatureVariation kComposeboxDevToolsVariations[] = {
    {"Force Failure", kComposeboxDevToolsForceFailure,
     std::size(kComposeboxDevToolsForceFailure), nullptr},
    {"Slow Load (1s)", kComposeboxDevToolsSlowLoad,
     std::size(kComposeboxDevToolsSlowLoad), nullptr},
    {"Slow Upload (3s)", kComposeboxDevToolsSlowUpload,
     std::size(kComposeboxDevToolsSlowUpload), nullptr}};

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

const FeatureEntry::FeatureVariation kMobilePromoOnDesktopVariations[] = {
    {" - Lens Promo", kMobilePromoOnDesktopLens,
     std::size(kMobilePromoOnDesktopLens), nullptr},
    {" - Lens Promo with push notification",
     kMobilePromoOnDesktopLensNotification,
     std::size(kMobilePromoOnDesktopLensNotification), nullptr},
    {" - ESB", kMobilePromoOnDesktopESB, std::size(kMobilePromoOnDesktopESB),
     nullptr},
    {" - ESB with push notification", kMobilePromoOnDesktopESBNotification,
     std::size(kMobilePromoOnDesktopESBNotification), nullptr},
    {" - PW Autofill", kMobilePromoOnDesktopAutofill,
     std::size(kMobilePromoOnDesktopAutofill), nullptr},
    {" - PW Autofill with push notification",
     kMobilePromoOnDesktopAutofillNotification,
     std::size(kMobilePromoOnDesktopAutofillNotification), nullptr},
};

const FeatureEntry::FeatureParam kDefaultBrowserMagicStackIosDeviceSettings[] =
    {{kDefaultBrowserMagicStackIosVariation, "1"}};
const FeatureEntry::FeatureParam kDefaultBrowserMagicStackIosInAppSettings[] = {
    {kDefaultBrowserMagicStackIosVariation, "2"}};

const FeatureEntry::FeatureVariation kDefaultBrowserMagicStackIosVariations[] =
    {{" - Tap to Device Settings", kDefaultBrowserMagicStackIosDeviceSettings,
      std::size(kDefaultBrowserMagicStackIosDeviceSettings), nullptr},
     {" - Tap to In App Settings", kDefaultBrowserMagicStackIosInAppSettings,
      std::size(kDefaultBrowserMagicStackIosInAppSettings), nullptr}};

const FeatureEntry::FeatureParam kTaiyakiChoiceScreenSurfaceParamAll[] = {
    {"choice_screen_surface", "all"}};
const FeatureEntry::FeatureParam kTaiyakiChoiceScreenSurfaceParamFREOnly[] = {
    {"choice_screen_surface", "fre_only"}};

const FeatureEntry::FeatureVariation kTaiyakiChoiceScreenSurfaceVariations[] = {
    {"all", kTaiyakiChoiceScreenSurfaceParamAll,
     std::size(kTaiyakiChoiceScreenSurfaceParamAll), nullptr},
    {"FRE only", kTaiyakiChoiceScreenSurfaceParamFREOnly,
     std::size(kTaiyakiChoiceScreenSurfaceParamFREOnly), nullptr},
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
        {" - 1", kTipsNotificationsAlternative1,
         std::size(kTipsNotificationsAlternative1), nullptr},
        {" - 2", kTipsNotificationsAlternative2,
         std::size(kTipsNotificationsAlternative2), nullptr},
        {" - 3", kTipsNotificationsAlternative3,
         std::size(kTipsNotificationsAlternative3), nullptr}};

const FeatureEntry::FeatureParam kZeroStateSuggestionsPlacementAIHubParam[] = {
    {kZeroStateSuggestionsPlacementAIHub, "true"}};
const FeatureEntry::FeatureParam
    kZeroStateSuggestionsPlacementAskGeminiParam[] = {
        {kZeroStateSuggestionsPlacementAskGemini, "true"}};

const FeatureEntry::FeatureVariation kZeroStateSuggestionsVariations[] = {
    {"AI Hub", kZeroStateSuggestionsPlacementAIHubParam,
     std::size(kZeroStateSuggestionsPlacementAIHubParam), nullptr},
    {"Ask Gemini Overlay", kZeroStateSuggestionsPlacementAskGeminiParam,
     std::size(kZeroStateSuggestionsPlacementAskGeminiParam), nullptr},
};

const char kFRESignInHeaderTextUpdateParamName[] =
    "FRESignInHeaderTextUpdateParam";
const FeatureEntry::FeatureParam kFRESignInHeaderTextUpdateArm0[] = {
    {kFRESignInHeaderTextUpdateParamName, "Arm0"}};
const FeatureEntry::FeatureParam kFRESignInHeaderTextUpdateArm1[] = {
    {kFRESignInHeaderTextUpdateParamName, "Arm1"}};

const FeatureEntry::FeatureVariation kFRESignInHeaderTextUpdateVariations[] = {
    {"Header variation #1", kFRESignInHeaderTextUpdateArm0,
     std::size(kFRESignInHeaderTextUpdateArm0), nullptr},
    {"Header variation #2", kFRESignInHeaderTextUpdateArm1,
     std::size(kFRESignInHeaderTextUpdateArm1), nullptr},
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
     kPersistTabContextFileSystem_WasHidden_FullContext,
     std::size(kPersistTabContextFileSystem_WasHidden_FullContext), nullptr},
    {"SQLite, On Tab Hide, APC + Inner Text",
     kPersistTabContextSqlite_WasHidden_FullContext,
     std::size(kPersistTabContextSqlite_WasHidden_FullContext), nullptr},
    {"SQLite, On Tab Hide & Page Load, APC + Inner Text",
     kPersistTabContextSqlite_WasHiddenPageLoad_FullContext,
     std::size(kPersistTabContextSqlite_WasHiddenPageLoad_FullContext),
     nullptr},
    {"SQLite, On Tab Hide, Inner Text",
     kPersistTabContextSqlite_WasHidden_InnerTextOnly,
     std::size(kPersistTabContextSqlite_WasHidden_InnerTextOnly), nullptr}};
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
    {"sign-in-button-no-avatar", flag_descriptions::kSignInButtonNoAvatarName,
     flag_descriptions::kSignInButtonNoAvatarDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kSignInButtonNoAvatar)},
    {"ntp-background-customization",
     flag_descriptions::kNTPBackgroundCustomizationName,
     flag_descriptions::kNTPBackgroundCustomizationDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kNTPBackgroundCustomization)},
    {"ntp-background-color-slider",
     flag_descriptions::kNTPBackgroundColorSliderName,
     flag_descriptions::kNTPBackgroundColorSliderDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kNTPBackgroundColorSlider)},
    {"ntp-alpha-background-collections",
     flag_descriptions::kNtpAlphaBackgroundCollectionsName,
     flag_descriptions::kNtpAlphaBackgroundCollectionsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(ntp_features::kNtpAlphaBackgroundCollections)},
    {"confirmation-button-swap-order",
     flag_descriptions::kConfirmationButtonSwapOrderName,
     flag_descriptions::kConfirmationButtonSwapOrderDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kConfirmationButtonSwapOrder)},
    {"fullscreen-promos-manager-skip-internal-limits",
     flag_descriptions::kFullscreenPromosManagerSkipInternalLimitsName,
     flag_descriptions::kFullscreenPromosManagerSkipInternalLimitsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kFullscreenPromosManagerSkipInternalLimits)},
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
    {"omnibox-aim-shortcut-typed-state",
     flag_descriptions::kIOSOmniboxAimShortcutName,
     flag_descriptions::kIOSOmniboxAimShortcutDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(omnibox::kOmniboxAimShortcutTypedState,
                                    kOmniboxAimShortcutTypedStateVariations,
                                    "OmniboxAimShortcutTypedState")},
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
    {"omnibox-mobile-parity-update-v3",
     flag_descriptions::kOmniboxMobileParityUpdateV3Name,
     flag_descriptions::kOmniboxMobileParityUpdateV3Description,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxMobileParityUpdateV3)},
    {"force-startup-signin-promo",
     flag_descriptions::kForceStartupSigninPromoName,
     flag_descriptions::kForceStartupSigninPromoDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(switches::kForceStartupSigninPromo)},
    {"shared-highlighting-ios", flag_descriptions::kSharedHighlightingIOSName,
     flag_descriptions::kSharedHighlightingIOSDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kSharedHighlightingIOS)},
#if BUILDFLAG(IOS_SCREEN_TIME_ENABLED)
    {"screen-time-integration-ios",
     flag_descriptions::kScreenTimeIntegrationName,
     flag_descriptions::kScreenTimeIntegrationDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kScreenTimeIntegration)},
#endif
    {"ios-reactivation-notifications",
     flag_descriptions::kIOSReactivationNotificationsName,
     flag_descriptions::kIOSReactivationNotificationsDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kIOSReactivationNotifications,
                                    kIOSReactivationNotificationsVariations,
                                    "IOSReactivationNotifications")},
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
    {"start-surface", flag_descriptions::kStartSurfaceName,
     flag_descriptions::kStartSurfaceDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kStartSurface,
                                    kStartSurfaceVariations,
                                    "StartSurface")},
    {"wait-threshold-seconds-for-capabilities-api",
     flag_descriptions::kWaitThresholdMillisecondsForCapabilitiesApiName,
     flag_descriptions::kWaitThresholdMillisecondsForCapabilitiesApiDescription,
     flags_ui::kOsIos,
     MULTI_VALUE_TYPE(kWaitThresholdMillisecondsForCapabilitiesApiChoices)},
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
    {"enable-lens-overlay", flag_descriptions::kEnableLensOverlayName,
     flag_descriptions::kEnableLensOverlayDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableLensOverlay)},
    {"enable-lens-view-finder-unified-experience",
     flag_descriptions::kEnableLensViewFinderUnifiedExperienceName,
     flag_descriptions::kEnableLensViewFinderUnifiedExperienceDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableLensViewFinderUnifiedExperience)},
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
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kIOSKeyboardAccessoryTwoBubble)},
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
    {"omnibox-zero-suggest-prefetching",
     flag_descriptions::kOmniboxZeroSuggestPrefetchingName,
     flag_descriptions::kOmniboxZeroSuggestPrefetchingDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(omnibox::kZeroSuggestPrefetching)},
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
    {"omnibox-zero-suggest-in-memory-caching",
     flag_descriptions::kOmniboxZeroSuggestInMemoryCachingName,
     flag_descriptions::kOmniboxZeroSuggestInMemoryCachingDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(omnibox::kZeroSuggestInMemoryCaching)},
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
    {"default-browser-banner-promo",
     flag_descriptions::kDefaultBrowserBannerPromoName,
     flag_descriptions::kDefaultBrowserBannerPromoDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kDefaultBrowserBannerPromo)},
    {"default-browser-off-cycle-promo",
     flag_descriptions::kDefaultBrowserOffCyclePromoName,
     flag_descriptions::kDefaultBrowserOffCyclePromoDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kIOSDefaultBrowserOffCyclePromo)},
    {"use-default-apps-page-for-promos",
     flag_descriptions::kUseDefaultAppsDestinationForPromosName,
     flag_descriptions::kUseDefaultAppsDestinationForPromosDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIOSUseDefaultAppsDestinationForPromos)},
    {"persistent-default-browser-promo",
     flag_descriptions::kPersistentDefaultBrowserPromoName,
     flag_descriptions::kPersistentDefaultBrowserPromoDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kPersistentDefaultBrowserPromo)},
    {"default-browser-promo-ipad_instructions",
     flag_descriptions::kDefaultBrowserPromoIpadInstructionsName,
     flag_descriptions::kDefaultBrowserPromoIpadInstructionsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kDefaultBrowserPromoIpadInstructions)},
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
    {"iph-price-notifications-while-browsing",
     flag_descriptions::kIPHPriceNotificationsWhileBrowsingName,
     flag_descriptions::kIPHPriceNotificationsWhileBrowsingDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         feature_engagement::kIPHPriceNotificationsWhileBrowsingFeature)},
    {"shopping-list", commerce::flag_descriptions::kShoppingListName,
     commerce::flag_descriptions::kShoppingListDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(commerce::kShoppingList)},
    {"notification-settings-menu-item",
     flag_descriptions::kNotificationSettingsMenuItemName,
     flag_descriptions::kNotificationSettingsMenuItemDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kNotificationSettingsMenuItem)},
    {"web-feed-feedback-reroute",
     flag_descriptions::kWebFeedFeedbackRerouteName,
     flag_descriptions::kWebFeedFeedbackRerouteDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kWebFeedFeedbackReroute)},
    {"ios-browser-edit-menu-metrics",
     flag_descriptions::kIOSBrowserEditMenuMetricsName,
     flag_descriptions::kIOSBrowserEditMenuMetricsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIOSBrowserEditMenuMetrics)},
    {"ios-docking-promo", flag_descriptions::kIOSDockingPromoName,
     flag_descriptions::kIOSDockingPromoDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kIOSDockingPromo,
                                    kIOSDockingPromoVariations,
                                    "IOSDockingPromo")},
    {"omnibox-grouping-framework-non-zps",
     flag_descriptions::kOmniboxGroupingFrameworkForTypedSuggestionsName,
     flag_descriptions::kOmniboxGroupingFrameworkForTypedSuggestionsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(omnibox::kGroupingFrameworkForNonZPS)},
    {"only-access-clipboard-async",
     flag_descriptions::kOnlyAccessClipboardAsyncName,
     flag_descriptions::kOnlyAccessClipboardAsyncDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kOnlyAccessClipboardAsync)},
    {"spotlight-never-retain-index",
     flag_descriptions::kSpotlightNeverRetainIndexName,
     flag_descriptions::kSpotlightNeverRetainIndexDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kSpotlightNeverRetainIndex)},
    {"tab-resumption", flag_descriptions::kTabResumptionName,
     flag_descriptions::kTabResumptionDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kTabResumption)},
    {"bottom-omnibox-evolution", flag_descriptions::kBottomOmniboxEvolutionName,
     flag_descriptions::kBottomOmniboxEvolutionDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kBottomOmniboxEvolution,
                                    kBottomOmniboxEvolutionVariations,
                                    "BottomOmniboxEvolution")},
    {"bwg-precise-location", flag_descriptions::kBWGPreciseLocationName,
     flag_descriptions::kBWGPreciseLocationDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kBWGPreciseLocation)},
    {"ai-hub-new-badge", flag_descriptions::kAIHubNewBadgeName,
     flag_descriptions::kAIHubNewBadgeDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kAIHubNewBadge)},
    {"enable-identity-in-auth-error",
     flag_descriptions::kEnableIdentityInAuthErrorName,
     flag_descriptions::kEnableIdentityInAuthErrorDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(switches::kEnableIdentityInAuthError)},
    {"enable-asweb-authentication-session",
     flag_descriptions::kEnableASWebAuthenticationSessionName,
     flag_descriptions::kEnableASWebAuthenticationSessionDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(switches::kEnableASWebAuthenticationSession)},
    {"privacy-guide-ios", flag_descriptions::kPrivacyGuideIosName,
     flag_descriptions::kPrivacyGuideIosDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kPrivacyGuideIos)},
    {"autofill-payments-field-swapping",
     flag_descriptions::kAutofillPaymentsFieldSwappingName,
     flag_descriptions::kAutofillPaymentsFieldSwappingDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillPaymentsFieldSwapping)},
    {"https-upgrades-ios", flag_descriptions::kHttpsUpgradesName,
     flag_descriptions::kHttpsUpgradesDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(security_interstitials::features::kHttpsUpgrades)},
    {"contextual-panel", flag_descriptions::kContextualPanelName,
     flag_descriptions::kContextualPanelDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kContextualPanel,
                                    kContextualPanelEntrypointArmVariations,
                                    "ContextualPanel")},
    {"enable-ipad-feed-ghost-cards",
     flag_descriptions::kEnableiPadFeedGhostCardsName,
     flag_descriptions::kEnableiPadFeedGhostCardsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableiPadFeedGhostCards)},
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
    {"linked-services-setting-ios",
     flag_descriptions::kLinkedServicesSettingIosName,
     flag_descriptions::kLinkedServicesSettingIosDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kLinkedServicesSettingIos)},
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
    {"page-visibility-page-content-annotations",
     flag_descriptions::kPageVisibilityPageContentAnnotationsName,
     flag_descriptions::kPageVisibilityPageContentAnnotationsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(page_content_annotations::features::
                            kPageVisibilityPageContentAnnotations)},
    {"enhanced-safe-browsing-promo",
     flag_descriptions::kEnhancedSafeBrowsingPromoName,
     flag_descriptions::kEnhancedSafeBrowsingPromoDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(safe_browsing::kEnhancedSafeBrowsingPromo)},
    {"cpe-passkey-prf-support",
     flag_descriptions::kCredentialProviderPasskeyPRFName,
     flag_descriptions::kCredentialProviderPasskeyPRFDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kCredentialProviderPasskeyPRF)},
    {"cpe-performance-improvements",
     flag_descriptions::kCredentialProviderPerformanceImprovementsName,
     flag_descriptions::kCredentialProviderPerformanceImprovementsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kCredentialProviderPerformanceImprovements)},
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
    {"omnibox-suggestion-answer-migration",
     flag_descriptions::kOmniboxSuggestionAnswerMigrationName,
     flag_descriptions::kOmniboxSuggestionAnswerMigrationDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(omnibox_feature_configs::SuggestionAnswerMigration::
                            kOmniboxSuggestionAnswerMigration)},
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
    {"ios-choose-from-drive", flag_descriptions::kIOSChooseFromDriveName,
     flag_descriptions::kIOSChooseFromDriveDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIOSChooseFromDrive)},
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
    {"safety-check-notifications",
     flag_descriptions::kSafetyCheckNotificationsName,
     flag_descriptions::kSafetyCheckNotificationsDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kSafetyCheckNotifications,
                                    kSafetyCheckNotificationsVariations,
                                    "SafetyCheckNotifications")},
    {"app-background-refresh-ios", flag_descriptions::kAppBackgroundRefreshName,
     flag_descriptions::kAppBackgroundRefreshDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableAppBackgroundRefresh)},
    {"home-memory-improvements", flag_descriptions::kHomeMemoryImprovementsName,
     flag_descriptions::kHomeMemoryImprovementsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kHomeMemoryImprovements)},
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
    {"autofill-isolated-world-ios",
     flag_descriptions::kAutofillIsolatedWorldForJavascriptIOSName,
     flag_descriptions::kAutofillIsolatedWorldForJavascriptIOSDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kAutofillIsolatedWorldForJavascriptIos)},
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
    {"lens-overlay-enable-ipad-compatibility",
     flag_descriptions::kLensOverlayEnableIPadCompatibilityName,
     flag_descriptions::kLensOverlayEnableIPadCompatibilityDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kLensOverlayEnableIPadCompatibility)},
    {"lens-overlay-enable-landscape-compatibility",
     flag_descriptions::kLensOverlayEnableLandscapeCompatibilityName,
     flag_descriptions::kLensOverlayEnableLandscapeCompatibilityDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kLensOverlayEnableLandscapeCompatibility)},
    {"lens-overlay-force-show-onboarding-screen",
     flag_descriptions::kLensOverlayForceShowOnboardingScreenName,
     flag_descriptions::kLensOverlayForceShowOnboardingScreenDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kLensOverlayForceShowOnboardingScreen)},
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
    {"tab-resumption-images", flag_descriptions::kTabResumptionImagesName,
     flag_descriptions::kTabResumptionImagesDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kTabResumptionImages,
                                    kTabResumptionImagesVariations,
                                    "TabResumption1_5")},
    {"segmentation-platform-tips-ephemeral-card",
     flag_descriptions::kSegmentationPlatformTipsEphemeralCardName,
     flag_descriptions::kSegmentationPlatformTipsEphemeralCardDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(segmentation_platform::features::
                            kSegmentationPlatformTipsEphemeralCard)},

    {"lens-unary-apis-with-http-transport-enabled",
     flag_descriptions::kLensUnaryApisWithHttpTransportEnabledName,
     flag_descriptions::kLensUnaryApisWithHttpTransportEnabledDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kLensUnaryApisWithHttpTransportEnabled)},
    {"ios-chrome-startup-parameters-async",
     flag_descriptions::kChromeStartupParametersAsyncName,
     flag_descriptions::kChromeStartupParametersAsyncDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kChromeStartupParametersAsync)},
    {"ios-youtube-incognito", flag_descriptions::kYoutubeIncognitoName,
     flag_descriptions::kYoutubeIncognitoDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kYoutubeIncognito,
                                    kYoutubeIncognitoVariations,
                                    "YoutubeIncognito")},
    {"lens-overlay-disable-iph-pan-gesture",
     flag_descriptions::kLensOverlayDisableIPHPanGestureName,
     flag_descriptions::kLensOverlayDisableIPHPanGestureDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kLensOverlayDisableIPHPanGesture)},
    {"ios-provisional-notification-alert",
     flag_descriptions::kProvisionalNotificationAlertName,
     flag_descriptions::kProvisionalNotificationAlertDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kProvisionalNotificationAlert)},
    {"ios-start-time-startup-remediations",
     flag_descriptions::kIOSStartTimeStartupRemediationsName,
     flag_descriptions::kIOSStartTimeStartupRemediationsDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kIOSStartTimeStartupRemediations,
                                    kIOSStartTimeStartupRemediationsVariations,
                                    "IOSStartTimeStartupRemediations")},
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
    {"ios-start-time-browser-background-remediations",
     flag_descriptions::kIOSStartTimeBrowserBackgroundRemediationsName,
     flag_descriptions::kIOSStartTimeBrowserBackgroundRemediationsDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         kIOSStartTimeBrowserBackgroundRemediations,
         kIOSStartTimeBrowserBackgroundRemediationsVariations,
         "IOSStartTimeStartupRemediations")},
    {"lens-unary-http-transport-enabled",
     flag_descriptions::kLensUnaryHttpTransportEnabledName,
     flag_descriptions::kLensUnaryHttpTransportEnabledDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kLensUnaryHttpTransportEnabled)},
    {"set-up-list-shortened-duration",
     flag_descriptions::kSetUpListShortenedDurationName,
     flag_descriptions::kSetUpListShortenedDurationDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(set_up_list::kSetUpListShortenedDuration,
                                    kSetUpListDurationVariations,
                                    "SetUpListShortenedDuration")},
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
    {"ios-manage-account-storage",
     flag_descriptions::kIOSManageAccountStorageName,
     flag_descriptions::kIOSManageAccountStorageDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIOSManageAccountStorage)},
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
    {"autofill-enable-allowlist-for-bmo-card-category-benefits",
     flag_descriptions::kAutofillEnableAllowlistForBmoCardCategoryBenefitsName,
     flag_descriptions::
         kAutofillEnableAllowlistForBmoCardCategoryBenefitsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::
             kAutofillEnableAllowlistForBmoCardCategoryBenefits)},
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
    {"ios-new-share-extension", flag_descriptions::kNewShareExtensionName,
     flag_descriptions::kNewShareExtensionDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kNewShareExtension)},
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
    {"non-modal-sign-in-promo", flag_descriptions::kNonModalSignInPromoName,
     flag_descriptions::kNonModalSignInPromoDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kNonModalSignInPromo)},
    {"autofill-save-card-bottomsheet",
     flag_descriptions::kAutofillSaveCardBottomSheetName,
     flag_descriptions::kAutofillSaveCardBottomSheetDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillSaveCardBottomSheet)},
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
    {"data-sharing-debug-logs", flag_descriptions::kDataSharingDebugLogsName,
     flag_descriptions::kDataSharingDebugLogsDescription, flags_ui::kOsIos,
     SINGLE_VALUE_TYPE(data_sharing::kDataSharingDebugLoggingEnabled)},
    {"supervised-user-block-interstitial-v3",
     flag_descriptions::kSupervisedUserBlockInterstitialV3Name,
     flag_descriptions::kSupervisedUserBlockInterstitialV3Description,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(supervised_user::kSupervisedUserBlockInterstitialV3)},
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
    {"ios-one-tap-mini-map-remove-section-breaks",
     flag_descriptions::kIOSOneTapMiniMapRemoveSectionBreaksName,
     flag_descriptions::kIOSOneTapMiniMapRemoveSectionBreaksDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIOSOneTapMiniMapRemoveSectionsBreaks)},
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
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kProactiveSuggestionsFramework)},
    {"ask-gemini-chip", flag_descriptions::kAskGeminiChipName,
     flag_descriptions::kAskGeminiChipDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kAskGeminiChip,
                                    kAskGeminiChipVariations,
                                    "IOSAskGeminiChip")},
    {"gemini-cross-tab", flag_descriptions::kGeminiCrossTabName,
     flag_descriptions::kGeminiCrossTabDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kGeminiCrossTab)},
    {"bwg-promo-consent", flag_descriptions::kBWGPromoConsentName,
     flag_descriptions::kBWGPromoConsentDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kBWGPromoConsent,
                                    kBWGPromoConsentVariations,
                                    "IOSBWGPromoConsent")},
    {"feedback-include-variations",
     flag_descriptions::kFeedbackIncludeVariationsName,
     flag_descriptions::kFeedbackIncludeVariationsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(variations::kFeedbackIncludeVariations)},
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
    {"reader-mode-enabled", flag_descriptions::kReaderModeName,
     flag_descriptions::kReaderModeDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableReaderMode)},
    {"reader-mode-translation-enabled",
     flag_descriptions::kReaderModeTranslationName,
     flag_descriptions::kReaderModeTranslationDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableReaderModeTranslation)},
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
    {"reader-mode-page-eligibility-enabled",
     flag_descriptions::kReaderModePageEligibilityHeuristicName,
     flag_descriptions::kReaderModePageEligibilityHeuristicDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableReaderModePageEligibilityForToolsMenu)},
    {"reader-mode-readability-heuristic-enabled",
     flag_descriptions::kReaderModeReadabilityHeuristicName,
     flag_descriptions::kReaderModeReadabilityHeuristicDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kEnableReadabilityHeuristic)},
    {"reader-mode-us-enabled", flag_descriptions::kReaderModeUSEnabledName,
     flag_descriptions::kReaderModeUSEnabledDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableReaderModeInUS)},
    {"reader-mode-readability-distiller-enabled",
     flag_descriptions::kReaderModeReadabilityDistillerName,
     flag_descriptions::kReaderModeReadabilityDistillerDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(dom_distiller::kReaderModeUseReadability)},
    {"best-of-app-fre", flag_descriptions::kBestOfAppFREName,
     flag_descriptions::kBestOfAppFREDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kBestOfAppFRE,
                                    kBestOfAppFREVariations,
                                    "BestOfAppFRE")},
    {"use-new-feed-eligibility-service",
     flag_descriptions::kUseFeedEligibilityServiceName,
     flag_descriptions::kUseFeedEligibilityServiceDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kUseFeedEligibilityService)},
    {"import-passwords-from-safari",
     flag_descriptions::kImportPasswordsFromSafariName,
     flag_descriptions::kImportPasswordsFromSafariDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kImportPasswordsFromSafari)},
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
    {"autofill-local-save-card-bottomsheet",
     flag_descriptions::kAutofillLocalSaveCardBottomSheetName,
     flag_descriptions::kAutofillLocalSaveCardBottomSheetDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillLocalSaveCardBottomSheet)},
    {"autofill-enable-multiple-request-in-virtual-card-downstream-enrollment",
     flag_descriptions::
         kAutofillEnableMultipleRequestInVirtualCardDownstreamEnrollmentName,
     flag_descriptions::
         kAutofillEnableMultipleRequestInVirtualCardDownstreamEnrollmentDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::
             kAutofillEnableMultipleRequestInVirtualCardDownstreamEnrollment)},
    {"ios-mini-map-universal-links",
     flag_descriptions::kIOSMiniMapUniversalLinkName,
     flag_descriptions::kIOSMiniMapUniversalLinkDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIOSMiniMapUniversalLink)},
    {"ios-fill-recovery-password",
     flag_descriptions::kIOSFillRecoveryPasswordName,
     flag_descriptions::kIOSFillRecoveryPasswordDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(password_manager::features::kIOSFillRecoveryPassword)},
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
    {"ios-trusted-vault-notification",
     flag_descriptions::kIOSTrustedVaultNotificationName,
     flag_descriptions::kIOSTrustedVaultNotificationDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kIOSTrustedVaultNotification)},
    {"diamond-prototype", flag_descriptions::kDiamondPrototypeName,
     flag_descriptions::kDiamondPrototypeDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kDiamondPrototype)},
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
    {"cpe-signal-api", flag_descriptions::kCredentialProviderSignalAPIName,
     flag_descriptions::kCredentialProviderSignalAPIDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kCredentialProviderSignalAPI)},
    {"migrate-account-prefs-on-mobile",
     flag_descriptions::kMigrateAccountPrefsOnMobileName,
     flag_descriptions::kMigrateAccountPrefsOnMobileDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(syncer::kMigrateAccountPrefs)},
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
    {"page-context-anchor-tags", flag_descriptions::kPageContextAnchorTagsName,
     flag_descriptions::kPageContextAnchorTagsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kPageContextAnchorTags)},
    {"cpe-passkey-largeblob-support",
     flag_descriptions::kCredentialProviderPasskeyLargeBlobName,
     flag_descriptions::kCredentialProviderPasskeyLargeBlobDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kCredentialProviderPasskeyLargeBlob)},
#if BUILDFLAG(ENTERPRISE_DATA_CONTROLS)
    {"enable-clipboard-data-controls-ios",
     flag_descriptions::kEnableClipboardDataControlsIOSName,
     flag_descriptions::kEnableClipboardDataControlsIOSDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(data_controls::kEnableClipboardDataControlsIOS)},
#endif  // BUILDFLAG(ENTERPRISE_DATA_CONTROLS)
    {"autofill-credit-card-scanner-ios",
     flag_descriptions::kAutofillCreditCardScannerIosName,
     flag_descriptions::kAutofillCreditCardScannerIosDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillCreditCardScannerIos)},
    {"lens-strokes-api-enabled", flag_descriptions::kStrokesAPIEnabledName,
     flag_descriptions::kStrokesAPIEnabledDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kLensStrokesAPIEnabled)},
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
    {"mobile-promo-on-desktop", flag_descriptions::kMobilePromoOnDesktopName,
     flag_descriptions::kMobilePromoOnDesktopDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kMobilePromoOnDesktop,
                                    kMobilePromoOnDesktopVariations,
                                    "MobilePromoOnDesktop")},
    {"ios-default-browser-magic-stack",
     flag_descriptions::kDefaultBrowserMagicStackIosName,
     flag_descriptions::kDefaultBrowserMagicStackIosDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         segmentation_platform::features::kDefaultBrowserMagicStackIos,
         kDefaultBrowserMagicStackIosVariations,
         "DefaultBrowserMagicStackIos")},
    {"lens-search-headers-check-enabled",
     flag_descriptions::kLensSearchHeadersCheckEnabledName,
     flag_descriptions::kLensSearchHeadersCheckEnabledDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kLensSearchHeadersCheckEnabled)},
    {"autofill-bottom-sheet-new-blur",
     flag_descriptions::kAutofillBottomSheetNewBlurName,
     flag_descriptions::kAutofillBottomSheetNewBlurDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kAutofillBottomSheetNewBlur)},
    {"enable-cross-device-pref-tracker",
     flag_descriptions::kEnableCrossDevicePrefTrackerName,
     flag_descriptions::kEnableCrossDevicePrefTrackerDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         sync_preferences::features::kEnableCrossDevicePrefTracker)},
    {"reader-mode-new-css-enabled", flag_descriptions::kReaderModeNewCssName,
     flag_descriptions::kReaderModeNewCssDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(dom_distiller::kEnableReaderModeNewCss)},
    {"tab-grid-empty-thumbnail", flag_descriptions::kTabGridEmptyThumbnailName,
     flag_descriptions::kTabGridEmptyThumbnailDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kTabGridEmptyThumbnail)},
    {"ios-app-bundle-promo-magic-stack",
     flag_descriptions::kIOSAppBundlePromoEphemeralCardName,
     flag_descriptions::kIOSAppBundlePromoEphemeralCardDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         segmentation_platform::features::kAppBundlePromoEphemeralCard)},
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
    {"composebox-autoattach-tab",
     flag_descriptions::kComposeboxAutoattachTabName,
     flag_descriptions::kComposeboxAutoattachTabDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kComposeboxAutoattachTab)},
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
    {"variations-seed-corpus", flag_descriptions::kVariationsSeedCorpusName,
     flag_descriptions::kVariationsSeedCorpusDescription, flags_ui::kOsIos,
     STRING_VALUE_TYPE(variations::switches::kVariationsSeedCorpus, "")},
    {"zero-state-suggestions", flag_descriptions::kZeroStateSuggestionsName,
     flag_descriptions::kZeroStateSuggestionsDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kZeroStateSuggestions,
                                    kZeroStateSuggestionsVariations,
                                    "ZeroStateSuggestions")},
    {"ios-synced-set-up", flag_descriptions::kIOSSyncedSetUpName,
     flag_descriptions::kIOSSyncedSetUpDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIOSSyncedSetUp)},
    {"multiline-browser-omnibox",
     flag_descriptions::kMultilineBrowserOmniboxName,
     flag_descriptions::kMultilineBrowserOmniboxDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kMultilineBrowserOmnibox)},
    {"ios-auto-open-remote-tab-groups-settings",
     flag_descriptions::kIOSAutoOpenRemoteTabGroupsSettingsName,
     flag_descriptions::kIOSAutoOpenRemoteTabGroupsSettingsDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kIOSAutoOpenRemoteTabGroupsSettings)},
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
    {"gemini-onboarding-cards", flag_descriptions::kGeminiOnboardingCardsName,
     flag_descriptions::kGeminiOnboardingCardsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kGeminiOnboardingCards)},
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
    {"mdm-errors-for-dasher-accounts-handling",
     flag_descriptions::kHandleMdmErrorsForDasherAccountsName,
     flag_descriptions::kHandleMdmErrorsForDasherAccountsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(switches::kHandleMdmErrorsForDasherAccounts)},
    {"location-bar-badge-migration",
     flag_descriptions::kLocationBarBadgeMigrationName,
     flag_descriptions::kLocationBarBadgeMigrationDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kLocationBarBadgeMigration)},
    {"webpage-reported-images-sheet",
     flag_descriptions::kWebPageReportedImagesSheetName,
     flag_descriptions::kWebPageReportedImagesSheetDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kWebPageReportedImagesSheet)},
    {"image-context-menu-gemini-entry-point",
     flag_descriptions::kImageContextMenuGeminiEntryPointName,
     flag_descriptions::kImageContextMenuGeminiEntryPointDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kImageContextMenuGeminiEntryPoint)},
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
    {"gemini-immediate-overlay", flag_descriptions::kGeminiImmediateOverlayName,
     flag_descriptions::kGeminiImmediateOverlayDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kGeminiImmediateOverlay)},
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
    {"composebox-fetch-contextual-suggestions-for-image",
     flag_descriptions::kComposeboxFetchContextualSuggestionsForImageName,
     flag_descriptions::
         kComposeboxFetchContextualSuggestionsForImageDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kComposeboxFetchContextualSuggestionsForImage)},
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
                           base::Value::List& supported_entries,
                           base::Value::List& unsupported_entries) {
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
